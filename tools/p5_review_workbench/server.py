#!/usr/bin/env python3
"""Local-only P5 pilot annotation workbench server."""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import math
import mimetypes
import os
import struct
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit


IMAGE_COUNT = 30
MAX_JSON_BYTES = 1_000_000
MAX_REJECT_DRAIN_BYTES = 2_000_000
MAX_NOTES_LENGTH = 10_000
ALLOWED_DECISIONS = {"OK", "NG", "REVIEW"}
ALLOWED_REVIEW_STATES = {"pending", "in-progress", "annotation-complete"}
PREDICTION_ANNOTATION_FIELDS = {
    "score", "detector_version", "source_bbox", "prediction_id", "p4_result_present"
}


class WorkbenchError(Exception):
    """An expected request or package validation failure."""

    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message


def _strict_int(value, label: str, minimum: int | None = None) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"{label} must be an integer")
    if minimum is not None and value < minimum:
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"{label} must be at least {minimum}")
    return value


def _finite_number(value, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"{label} must be a finite number")
    return float(value)


def _read_json(path: Path):
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            return json.load(stream)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"cannot read valid JSON from {path.name}: {exc}") from exc


def _atomic_json(path: Path, payload) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.{threading.get_ident()}.tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as stream:
            json.dump(payload, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _image_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        signature = stream.read(24)
        if signature.startswith(b"\x89PNG\r\n\x1a\n") and len(signature) == 24:
            return struct.unpack(">II", signature[16:24])
        if signature[:2] != b"\xff\xd8":
            raise ValueError("unsupported image format")
        stream.seek(2)
        while True:
            marker_start = stream.read(1)
            if not marker_start:
                break
            if marker_start != b"\xff":
                continue
            marker = stream.read(1)
            while marker == b"\xff":
                marker = stream.read(1)
            if marker in {b"\xd8", b"\xd9"}:
                continue
            length_bytes = stream.read(2)
            if len(length_bytes) != 2:
                break
            length = struct.unpack(">H", length_bytes)[0]
            if length < 2:
                break
            if marker and marker[0] in {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
                                        0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}:
                dimensions = stream.read(5)
                if len(dimensions) != 5:
                    break
                height, width = struct.unpack(">HH", dimensions[1:5])
                return width, height
            stream.seek(length - 2, os.SEEK_CUR)
    raise ValueError("image dimensions not found")


def _contained_file(root: Path, relative: str) -> Path:
    relative = unquote(relative).replace("\\", "/")
    parts = relative.split("/")
    if not relative or any(part in {"", ".", ".."} for part in parts):
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "invalid file path")
    candidate = root.joinpath(*parts).resolve()
    try:
        candidate.relative_to(root.resolve())
    except ValueError as exc:
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "file path escapes its allowed root") from exc
    if not candidate.is_file():
        raise WorkbenchError(HTTPStatus.NOT_FOUND, "file not found")
    return candidate


class ReviewWorkbench:
    """Validated package state and mutation/export operations."""

    def __init__(self, package: Path, workspace: Path, static_dir: Path | None = None):
        self.package = Path(package).resolve()
        self.workspace = Path(workspace).resolve()
        self.static_dir = (Path(static_dir) if static_dir else Path(__file__).with_name("static")).resolve()
        self.images_dir = (self.package / "images").resolve()
        self.previews_dir = (self.package / "previews").resolve()
        self.state_path = self.workspace / "review-state.json"
        self.export_path = self.workspace / "pass1-annotations.coco.json"
        self._lock = threading.RLock()
        self._bound_file_hashes: dict[Path, str] = {}
        catalog_path = Path(__file__).resolve().parents[2] / "config" / "p5-class-catalog.json"
        catalog = _read_json(catalog_path)
        catalog_classes = catalog.get("classes")
        if not isinstance(catalog_classes, list) or len(catalog_classes) != 9:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "class catalog must contain exactly nine classes")
        self.class_catalog_sha256 = _sha256(catalog_path)
        self.categories = [{
            "id": _strict_int(item.get("id"), "catalog class id", 0),
            "name": item.get("name"),
            "display_name_zh": item.get("displayNameZh"),
            "mapping_status": item.get("mappingStatus"),
            "supercategory": "cigarette_defect",
        } for item in catalog_classes]
        if [item["id"] for item in self.categories] != list(range(9)):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "class catalog IDs must be unique and ordered 0 through 8")
        self.state_categories = [{
            **item,
            "display_name": item["display_name_zh"],
            "status": item["mapping_status"],
            "labelable": True,
        } for item in self.categories]
        required_package_files = [
            "pilot-preannotations.coco.json", "pilot-review.csv", "pilot-manifest.json",
            "pilot-preview-provenance.json", "pilot-selection.json",
        ]
        self.package_bindings = {}
        for name in required_package_files:
            path = self.package / name
            if not path.is_file():
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"package is missing {name}")
            self.package_bindings[name] = _sha256(path)
            self._bound_file_hashes[path.resolve()] = self.package_bindings[name]
        self.package_fingerprint = hashlib.sha256(json.dumps(
            self.package_bindings, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")).hexdigest()
        self.source_coco, initial_state = self._load_package()
        self._identity = {
            item["id"]: (item["file_name"], item["sha256"], item["width"], item["height"])
            for item in initial_state["images"]
        }
        self.category_ids = {item["id"] for item in self.categories}
        if self.state_path.exists():
            state = _read_json(self.state_path)
            for category in state.get("categories", []):
                if category.get("id") in {7, 8}:
                    category["labelable"] = True
            self._validate_state(state, expected_revision=None)
            self.state = state
        else:
            self._validate_state(initial_state, expected_revision=None)
            self.state = initial_state
            _atomic_json(self.state_path, self.state)

    def _load_package(self):
        coco_path = self.package / "pilot-preannotations.coco.json"
        csv_path = self.package / "pilot-review.csv"
        if not self.package.is_dir() or not coco_path.is_file() or not csv_path.is_file():
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package must contain pilot-preannotations.coco.json and pilot-review.csv")
        coco = _read_json(coco_path)
        preview_provenance = _read_json(self.package / "pilot-preview-provenance.json")
        images = coco.get("images")
        annotations = coco.get("annotations")
        categories = coco.get("categories")
        if not isinstance(images, list) or len(images) != IMAGE_COUNT:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"package must contain exactly {IMAGE_COUNT} COCO images")
        if not isinstance(annotations, list) or not isinstance(categories, list):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package has invalid COCO collections")
        if coco.get("info", {}).get("class_catalog_sha256") != self.class_catalog_sha256:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package class catalog hash does not match the local catalog")
        preview_records = preview_provenance.get("records")
        if (preview_provenance.get("valid") is not True or not isinstance(preview_records, list)
                or len(preview_records) != IMAGE_COUNT):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package preview provenance is incomplete")
        previews_by_name = {item.get("file_name"): item for item in preview_records if isinstance(item, dict)}
        if len(previews_by_name) != IMAGE_COUNT or None in previews_by_name:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package preview provenance names must be unique")

        try:
            with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.DictReader(stream))
        except (OSError, UnicodeError, csv.Error) as exc:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"cannot read pilot-review.csv: {exc}") from exc
        if len(rows) != IMAGE_COUNT or any(not row.get("file_name") for row in rows):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"pilot-review.csv must contain exactly {IMAGE_COUNT} named rows")
        rows_by_name = {row["file_name"]: row for row in rows}
        if len(rows_by_name) != IMAGE_COUNT:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "pilot-review.csv file names must be unique")

        if categories != self.categories:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package categories do not match the local class catalog")
        category_ids = set(range(9))

        annotations_by_image = {}
        annotation_ids = set()
        for annotation in annotations:
            annotation_id = _strict_int(annotation.get("id"), "annotation id", 1)
            image_id = _strict_int(annotation.get("image_id"), "annotation image_id", 1)
            category_id = _strict_int(annotation.get("category_id"), "annotation category_id", 0)
            if annotation_id in annotation_ids or category_id not in category_ids:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package contains duplicate annotation IDs or unknown classes")
            annotation_ids.add(annotation_id)
            annotations_by_image.setdefault(image_id, []).append({
                "id": annotation_id,
                "category_id": category_id,
                "bbox": annotation.get("bbox"),
                "source": annotation.get("source", "prediction"),
                **({"score": annotation["score"]} if "score" in annotation else {}),
            })

        state_images = []
        image_ids = set()
        image_names = set()
        annotators = set()
        for image in images:
            image_id = _strict_int(image.get("id"), "image id", 1)
            width = _strict_int(image.get("width"), "image width", 1)
            height = _strict_int(image.get("height"), "image height", 1)
            name = image.get("file_name")
            sha = image.get("sha256")
            if image_id in image_ids or not isinstance(name, str) or name in image_names:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package image IDs and names must be unique")
            if not isinstance(sha, str) or len(sha) != 64 or any(char not in "0123456789abcdefABCDEF" for char in sha):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"invalid SHA-256 for {name}")
            if name not in rows_by_name:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"pilot-review.csv is missing {name}")
            row = rows_by_name[name]
            if row.get("sha256", "").lower() != sha.lower() or row.get("dimensions") != f"{width}x{height}":
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"CSV identity mismatch for {name}")
            source_path = _contained_file(self.images_dir, name)
            preview_path = _contained_file(self.previews_dir, f"{Path(name).stem}.annotated.png")
            preview_record = previews_by_name.get(name)
            if (not isinstance(preview_record, dict)
                    or preview_record.get("source_image_sha256", "").lower() != sha.lower()
                    or preview_record.get("preview_sha256", "").lower() != _sha256(preview_path)):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"preview provenance mismatch for {name}")
            self._bound_file_hashes[source_path] = sha.lower()
            self._bound_file_hashes[preview_path] = preview_record["preview_sha256"].lower()
            try:
                actual_dimensions = _image_dimensions(source_path)
                preview_dimensions = _image_dimensions(preview_path)
            except (OSError, ValueError, struct.error) as exc:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"cannot verify image dimensions for {name}: {exc}") from exc
            if (_sha256(source_path) != sha.lower() or actual_dimensions != (width, height)
                    or preview_dimensions != (width, height)):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"image hash or dimensions mismatch for {name}")
            decision = row.get("human_decision") or image.get("cigarette_decision")
            review_state = row.get("review_status") or "pending"
            annotator = (row.get("annotator") or "").strip()
            if annotator:
                annotators.add(annotator)
            state_images.append({
                "id": image_id,
                "file_name": name,
                "sha256": sha.lower(),
                "width": width,
                "height": height,
                "image_url": f"/images/{name}",
                "preview_url": f"/previews/{Path(name).stem}.annotated.png",
                "decision": decision,
                "predicted_decision": image.get("predicted_decision", ""),
                "review_state": review_state,
                "notes": row.get("notes") or "",
                "boxes": annotations_by_image.get(image_id, []),
                "annotated_by": "",
                "completed_revision": None,
            })
            image_ids.add(image_id)
            image_names.add(name)
        if set(rows_by_name) != image_names or set(annotations_by_image) - image_ids:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "COCO and CSV image identities do not match exactly")
        operator_id = next(iter(annotators)) if len(annotators) == 1 else ""
        state = {
            "revision": 0,
            "mode": "annotator",
            "operator_id": operator_id,
            "package_fingerprint": self.package_fingerprint,
            "categories": copy.deepcopy(self.state_categories),
            "images": state_images,
        }
        return coco, state

    def _validate_state(self, state, expected_revision: int | None) -> None:
        if not isinstance(state, dict):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state must be a JSON object")
        revision = _strict_int(state.get("revision"), "revision", 0)
        if expected_revision is not None and revision != expected_revision:
            raise WorkbenchError(HTTPStatus.CONFLICT, "stale revision")
        if state.get("mode") != "annotator":
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "mode must be annotator")
        if state.get("package_fingerprint") != self.package_fingerprint:
            raise WorkbenchError(HTTPStatus.CONFLICT, "workspace state does not match the selected package")
        operator_id = state.get("operator_id")
        if not isinstance(operator_id, str) or len(operator_id) > 200:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "operator_id must be a short string")
        if state.get("categories") != self.state_categories:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state categories do not match the local class catalog")
        images = state.get("images")
        if not isinstance(images, list) or len(images) != IMAGE_COUNT:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"state must contain exactly {IMAGE_COUNT} images")
        seen_images = set()
        seen_boxes = set()
        max_completed_revision = revision + (1 if expected_revision is not None else 0)
        for image in images:
            if not isinstance(image, dict):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, "each image state must be an object")
            image_id = _strict_int(image.get("id"), "image id", 1)
            if image_id in seen_images or image_id not in self._identity:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state contains a duplicate or unknown image ID")
            seen_images.add(image_id)
            expected = self._identity[image_id]
            _strict_int(image.get("width"), "image width", 1)
            _strict_int(image.get("height"), "image height", 1)
            actual = (image.get("file_name"), image.get("sha256"), image.get("width"), image.get("height"))
            if actual != expected:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"image identity mismatch for ID {image_id}")
            if image.get("decision") not in ALLOWED_DECISIONS:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"invalid decision for image {image_id}")
            if image.get("review_state") not in ALLOWED_REVIEW_STATES:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"invalid review_state for image {image_id}")
            notes = image.get("notes")
            if not isinstance(notes, str) or len(notes) > MAX_NOTES_LENGTH:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"invalid notes for image {image_id}")
            boxes = image.get("boxes")
            if not isinstance(boxes, list):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"boxes must be an array for image {image_id}")
            for box in boxes:
                if not isinstance(box, dict):
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, "each box must be an object")
                box_id = _strict_int(box.get("id"), "box id", 1)
                category_id = _strict_int(box.get("category_id"), "box category_id", 0)
                if box_id in seen_boxes or category_id not in self.category_ids:
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, "box IDs must be globally unique and classes must be allowed")
                seen_boxes.add(box_id)
                bbox = box.get("bbox")
                if not isinstance(bbox, list) or len(bbox) != 4:
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"box {box_id} must have four bbox values")
                x, y, width, height = [_finite_number(value, f"box {box_id} bbox") for value in bbox]
                if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > expected[2] or y + height > expected[3]:
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"box {box_id} is outside image bounds")
            if image.get("review_state") == "annotation-complete":
                decision = image.get("decision")
                annotated_by = image.get("annotated_by")
                completed_revision = image.get("completed_revision")
                if (not isinstance(annotated_by, str) or not annotated_by.strip()
                        or isinstance(completed_revision, bool) or not isinstance(completed_revision, int)
                        or completed_revision < 1 or completed_revision > max_completed_revision):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed image {image_id} requires server-stamped provenance")
                if decision == "OK" and boxes:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed OK image {image_id} must not contain boxes")
                if decision == "NG" and not boxes:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed NG image {image_id} requires at least one box")
                if decision == "NG" and any(box["category_id"] in {7, 8} for box in boxes):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed NG image {image_id} cannot use unconfirmed classes")
                if decision == "REVIEW" and not notes.strip():
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed REVIEW image {image_id} requires notes")
            elif image.get("annotated_by") != "" or image.get("completed_revision") is not None:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pending image {image_id} cannot retain completion provenance")
        if seen_images != set(self._identity):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state image identity set is incomplete")

    def get_state(self):
        with self._lock:
            return self._public_state(self.state)

    def read_served_file(self, path: Path) -> bytes:
        with self._lock:
            content = path.read_bytes()
            expected = self._bound_file_hashes.get(path.resolve())
            if expected is not None and hashlib.sha256(content).hexdigest() != expected:
                raise WorkbenchError(
                    HTTPStatus.CONFLICT, f"bound file changed after startup: {path.name}")
            return content

    def _validate_bound_files(self) -> None:
        for path, expected in self._bound_file_hashes.items():
            try:
                actual = _sha256(path)
            except OSError as exc:
                raise WorkbenchError(
                    HTTPStatus.CONFLICT, f"bound file is unavailable: {path.name}") from exc
            if actual != expected:
                raise WorkbenchError(
                    HTTPStatus.CONFLICT, f"bound file changed after startup: {path.name}")

    def _public_state(self, state):
        public = copy.deepcopy(state)
        public["session"] = {
            "revision": public["revision"],
            "mode": public["mode"],
            "operator_id": public["operator_id"],
        }
        for category in public["categories"]:
            category.setdefault("display_name", category.get("display_name_zh", category.get("name", "")))
            category.setdefault("status", category.get("mapping_status", ""))
            category["labelable"] = True
        for image in public["images"]:
            if image["review_state"] == "annotation-complete":
                image["review_state"] = "complete"
        return public

    def _normalize_save_payload(self, payload):
        if not isinstance(payload, dict):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state must be a JSON object")
        images = payload.get("images")
        is_full_state = (payload.get("mode") == "annotator" and isinstance(images, list) and
                         bool(images) and "file_name" in images[0])
        if is_full_state:
            normalized = copy.deepcopy(payload)
        else:
            revision = _strict_int(payload.get("revision"), "revision", 0)
            if not isinstance(images, list) or len(images) != IMAGE_COUNT:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"state must contain exactly {IMAGE_COUNT} images")
            incoming_by_id = {}
            for image in images:
                if not isinstance(image, dict):
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, "each image state must be an object")
                image_id = _strict_int(image.get("id"), "image id", 1)
                if image_id in incoming_by_id:
                    raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state contains duplicate image IDs")
                incoming_by_id[image_id] = image
            if set(incoming_by_id) != set(self._identity):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state image identity set is incomplete")
            normalized = copy.deepcopy(self.state)
            normalized["revision"] = revision
            normalized["operator_id"] = payload.get("operator_id")
            for image in normalized["images"]:
                incoming = incoming_by_id[image["id"]]
                for field in ("decision", "review_state", "notes", "boxes"):
                    if field not in incoming:
                        raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"image {image['id']} is missing {field}")
                    image[field] = copy.deepcopy(incoming[field])

        client_box_ids = set()
        next_box_id = 1
        numeric_ids = [
            box.get("id") for image in normalized.get("images", []) for box in image.get("boxes", [])
            if isinstance(box, dict) and isinstance(box.get("id"), int) and not isinstance(box.get("id"), bool)
        ]
        if numeric_ids:
            next_box_id = max(numeric_ids) + 1
        for image in normalized.get("images", []):
            if image.get("review_state") == "complete":
                image["review_state"] = "annotation-complete"
            for box in image.get("boxes", []):
                box_id = box.get("id")
                if isinstance(box_id, str):
                    if not box_id or box_id in client_box_ids:
                        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "temporary box IDs must be nonempty and unique")
                    client_box_ids.add(box_id)
                    box["id"] = next_box_id
                    next_box_id += 1
            if (image.get("decision") == "NG"
                    and any(box.get("category_id") in {7, 8} for box in image.get("boxes", []))):
                image["decision"] = "REVIEW"
        return normalized

    def _stamp_completion_provenance(self, normalized) -> None:
        current_by_id = {item["id"]: item for item in self.state["images"]}
        operator_id = normalized.get("operator_id")
        operator_id = operator_id.strip() if isinstance(operator_id, str) else ""
        next_revision = self.state["revision"] + 1
        for image in normalized["images"]:
            current = current_by_id[image["id"]]
            completed = image.get("review_state") == "annotation-complete"
            unchanged_completion = (
                completed and current.get("review_state") == "annotation-complete"
                and all(image.get(field) == current.get(field)
                        for field in ("decision", "notes", "boxes"))
            )
            if unchanged_completion:
                image["annotated_by"] = current["annotated_by"]
                image["completed_revision"] = current["completed_revision"]
            elif completed:
                if not operator_id:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST, "operator_id is required to complete an image")
                image["annotated_by"] = operator_id
                image["completed_revision"] = next_revision
            else:
                image["annotated_by"] = ""
                image["completed_revision"] = None

    def save(self, state):
        with self._lock:
            self._validate_bound_files()
            normalized = self._normalize_save_payload(state)
            self._stamp_completion_provenance(normalized)
            self._validate_state(normalized, expected_revision=self.state["revision"])
            saved = copy.deepcopy(normalized)
            saved["revision"] += 1
            _atomic_json(self.state_path, saved)
            self.state = saved
            return self._public_state(saved)

    def export_pass1(self):
        with self._lock:
            self._validate_bound_files()
            state = copy.deepcopy(self.state)
            self._validate_state(state, expected_revision=None)
            operator_id = state["operator_id"].strip()
            if not operator_id:
                raise WorkbenchError(HTTPStatus.CONFLICT, "nonempty operator_id required for pass1 export")
            if any(item["review_state"] != "annotation-complete" for item in state["images"]):
                raise WorkbenchError(HTTPStatus.CONFLICT, "all images must be annotation-complete")
            if any(not item["annotated_by"].strip() for item in state["images"]):
                raise WorkbenchError(HTTPStatus.CONFLICT, "every completed image requires annotator provenance")
            source_images = {item["id"]: item for item in self.source_coco["images"]}
            state_sha256 = _sha256(self.state_path)
            annotators = sorted({item["annotated_by"] for item in state["images"]})
            exported = {
                "info": {
                "description": "P5 pass1 human annotations; not reviewed ground truth",
                "schema_version": "p5-coco-v1",
                "class_catalog_sha256": self.class_catalog_sha256,
                "ground_truth_complete": False,
                "accuracy_metrics_claimed": False,
                "annotation_stage": "pass1",
                "annotation_status": "annotated",
                "source": "human",
                "annotators": annotators,
                "state_revision": state["revision"],
                "review_state_sha256": state_sha256,
                "package_fingerprint": self.package_fingerprint,
                "package_bindings": copy.deepcopy(self.package_bindings),
                },
                "images": [],
                "annotations": [],
                "categories": copy.deepcopy(self.categories),
            }
            output_annotations = []
            for current in state["images"]:
                source_image = source_images[current["id"]]
                image = {
                    "id": current["id"],
                    "file_name": current["file_name"],
                    "width": current["width"],
                    "height": current["height"],
                    "sha256": current["sha256"],
                    "source_group": source_image.get("source_group"),
                    "split": source_image.get("split"),
                    "authorization_status": "unverified",
                    "annotation_status": "annotated",
                    "is_ground_truth": False,
                    "source": "human",
                    "annotated_by": current["annotated_by"],
                    "completed_revision": current["completed_revision"],
                    "cigarette_decision": current["decision"],
                    "notes": current["notes"],
                }
                exported["images"].append(image)
                boxes = current["boxes"]
                if current["decision"] == "OK" and boxes:
                    raise WorkbenchError(HTTPStatus.CONFLICT, "OK images must have no annotations")
                if current["decision"] == "NG" and not boxes:
                    raise WorkbenchError(HTTPStatus.CONFLICT, "NG images must have at least one annotation")
                if current["decision"] == "NG" and any(box["category_id"] in {7, 8} for box in boxes):
                    raise WorkbenchError(HTTPStatus.CONFLICT, "classes 7 and 8 cannot be exported as NG")
                if current["decision"] not in {"NG", "REVIEW"}:
                    continue
                for box in boxes:
                    annotation = {
                        "id": box["id"],
                        "image_id": current["id"],
                        "category_id": box["category_id"],
                        "bbox": [float(value) for value in box["bbox"]],
                        "area": float(box["bbox"][2]) * float(box["bbox"][3]),
                        "iscrowd": 0,
                        "annotation_status": "annotated",
                        "is_ground_truth": False,
                        "source": "human",
                        "annotated_by": current["annotated_by"],
                        "completed_revision": current["completed_revision"],
                    }
                    for field in PREDICTION_ANNOTATION_FIELDS:
                        annotation.pop(field, None)
                    output_annotations.append(annotation)
            exported["annotations"] = output_annotations
            _atomic_json(self.export_path, exported)
            return {
                "path": str(self.export_path),
                "sha256": _sha256(self.export_path),
                "image_count": len(exported["images"]),
                "annotation_count": len(output_annotations),
                "status": "annotated",
                "is_ground_truth": False,
                "state_revision": state["revision"],
                "review_state_sha256": state_sha256,
                "package_fingerprint": self.package_fingerprint,
            }


class WorkbenchHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, workbench: ReviewWorkbench):
        self.workbench = workbench
        super().__init__(address, WorkbenchRequestHandler)


class WorkbenchRequestHandler(BaseHTTPRequestHandler):
    server_version = "P5ReviewWorkbench/1"

    @property
    def workbench(self) -> ReviewWorkbench:
        return self.server.workbench

    def log_message(self, format, *args):
        print(f"{self.address_string()} - {format % args}")

    def _json_response(self, status: int, payload) -> None:
        body = json.dumps(payload, ensure_ascii=False, allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _error(self, error: WorkbenchError) -> None:
        self._json_response(error.status, {"error": error.message})

    def _validate_host(self) -> None:
        host_header = self.headers.get("Host")
        try:
            hostname = urlsplit(f"//{host_header}").hostname if host_header else None
        except ValueError as exc:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "Host header is malformed") from exc
        if hostname not in {"127.0.0.1", "localhost", "::1"}:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "Host must resolve to localhost")

    def _request_json(self):
        content_type = self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if content_type != "application/json":
            raise WorkbenchError(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, "Content-Type must be application/json")
        raw_length = self.headers.get("Content-Length")
        try:
            length = int(raw_length) if raw_length is not None else -1
        except ValueError as exc:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "invalid Content-Length") from exc
        if length < 0:
            raise WorkbenchError(HTTPStatus.LENGTH_REQUIRED, "Content-Length required")
        if length > MAX_JSON_BYTES:
            if length <= MAX_REJECT_DRAIN_BYTES:
                self.rfile.read(length)
            else:
                self.close_connection = True
            raise WorkbenchError(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "JSON request is too large")
        body = self.rfile.read(length)
        try:
            return json.loads(body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "malformed JSON request") from exc

    def _optional_request_json(self):
        raw_length = self.headers.get("Content-Length")
        if raw_length in {None, "0"}:
            return {}
        return self._request_json()

    def _send_file(self, path: Path) -> None:
        content = self.workbench.read_served_file(path)
        content_type = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(content)

    def do_GET(self):
        path = urlsplit(self.path).path
        try:
            self._validate_host()
            if path == "/api/health":
                self._json_response(HTTPStatus.OK, {"status": "ok", "mode": "annotator"})
            elif path == "/api/state":
                self._json_response(HTTPStatus.OK, self.workbench.get_state())
            elif path in {"/", "/index.html"}:
                self._send_file(_contained_file(self.workbench.static_dir, "index.html"))
            elif path.startswith("/static/"):
                self._send_file(_contained_file(self.workbench.static_dir, path[len("/static/"):]))
            elif path.startswith("/images/"):
                self._send_file(_contained_file(self.workbench.images_dir, path[len("/images/"):]))
            elif path.startswith("/previews/"):
                self._send_file(_contained_file(self.workbench.previews_dir, path[len("/previews/"):]))
            else:
                raise WorkbenchError(HTTPStatus.NOT_FOUND, "route not found")
        except WorkbenchError as exc:
            self._error(exc)
        except OSError:
            self._error(WorkbenchError(HTTPStatus.NOT_FOUND, "file not found"))

    def do_POST(self):
        path = urlsplit(self.path).path
        try:
            self._validate_host()
            if path == "/api/save":
                self._json_response(HTTPStatus.OK, self.workbench.save(self._request_json()))
            elif path == "/api/export-pass1":
                self._optional_request_json()
                self._json_response(HTTPStatus.OK, self.workbench.export_pass1())
            elif path == "/api/export-reviewed":
                self._optional_request_json()
                raise WorkbenchError(HTTPStatus.CONFLICT, "authorization and independent reviewer required")
            else:
                raise WorkbenchError(HTTPStatus.NOT_FOUND, "route not found")
        except WorkbenchError as exc:
            self._error(exc)


def create_server(package: Path, workspace: Path, host: str = "127.0.0.1", port: int = 8765,
                  static_dir: Path | None = None) -> WorkbenchHTTPServer:
    if host not in {"127.0.0.1", "localhost", "::1"}:
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "review workbench may bind only to loopback")
    return WorkbenchHTTPServer((host, port), ReviewWorkbench(package, workspace, static_dir))


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path, help="P5 pilot review-package directory")
    parser.add_argument("--workspace", required=True, type=Path, help="draft and export output directory")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    if not 0 <= args.port <= 65535:
        raise SystemExit("--port must be between 0 and 65535")
    server = create_server(args.package, args.workspace, args.host, args.port)
    host, port = server.server_address[:2]
    print(f"P5 review workbench: http://{host}:{port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
