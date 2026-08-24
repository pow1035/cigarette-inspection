#!/usr/bin/env python3
"""Local-only P5 annotation workbench server."""

from __future__ import annotations

import argparse
import copy
import csv
import datetime as dt
import hashlib
import json
import math
import mimetypes
import os
import struct
import threading
import time
import unicodedata
from contextlib import contextmanager
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit


PILOT_IMAGE_COUNT = 30
DEVELOPMENT_SPLITS = {"train", "validation"}
MAX_JSON_BYTES = 1_000_000
MAX_REJECT_DRAIN_BYTES = 2_000_000
MAX_NOTES_LENGTH = 10_000
WORKSPACE_LOCK_TIMEOUT_SECONDS = 5.0
ALLOWED_DECISIONS = {"OK", "NG", "REVIEW"}
ALLOWED_REVIEW_STATES = {"pending", "in-progress", "annotation-complete"}
ALLOWED_MODES = {"annotator", "reviewer"}
ALLOWED_REVIEW_OUTCOMES = {"", "accepted", "corrected", "unresolved"}
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


def _normalized_identity(value) -> str:
    if not isinstance(value, str):
        return ""
    normalized = unicodedata.normalize("NFKC", value)
    normalized = "".join(
        character for character in normalized
        if not _is_default_ignorable(character))
    return normalized.strip().casefold()


_DEFAULT_IGNORABLE_UNICODE_VERSION = "17.0.0"
_DEFAULT_IGNORABLE_RANGES = (
    (0x00AD, 0x00AD),
    (0x034F, 0x034F),
    (0x061C, 0x061C),
    (0x115F, 0x1160),
    (0x17B4, 0x17B5),
    (0x180B, 0x180F),
    (0x200B, 0x200F),
    (0x202A, 0x202E),
    (0x2060, 0x206F),
    (0x3164, 0x3164),
    (0xFE00, 0xFE0F),
    (0xFEFF, 0xFEFF),
    (0xFFA0, 0xFFA0),
    (0xFFF0, 0xFFF8),
    (0x1BCA0, 0x1BCA3),
    (0x1D173, 0x1D17A),
    (0xE0000, 0xE0FFF),
)


def _is_default_ignorable(character: str) -> bool:
    # Unicode 17.0.0 DerivedCoreProperties.txt, Default_Ignorable_Code_Point.
    codepoint = ord(character)
    return any(
        start <= codepoint <= end
        for start, end in _DEFAULT_IGNORABLE_RANGES)


def _canonical_sha256(value) -> str:
    payload = json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
        allow_nan=False)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def _paths_overlap(left: Path, right: Path) -> bool:
    left = left.resolve()
    right = right.resolve()
    return left == right or left in right.parents or right in left.parents


def _safe_package_path(package: Path, relative: str) -> Path:
    if not isinstance(relative, str):
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package output path must be a string")
    normalized = relative.replace("\\", "/")
    parts = normalized.split("/")
    if (not normalized or normalized.startswith("/") or ":" in parts[0]
            or any(part in {"", ".", ".."} for part in parts)):
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package output path is unsafe")
    candidate = package.joinpath(*parts).resolve()
    try:
        candidate.relative_to(package.resolve())
    except ValueError as exc:
        raise WorkbenchError(
            HTTPStatus.BAD_REQUEST, "package output path escapes the package") from exc
    return candidate


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

    def __init__(
            self, package: Path, workspace: Path, static_dir: Path | None = None,
            split: str | None = None, mode: str = "annotator",
            pass1: Path | None = None):
        self.package = Path(package).resolve()
        self.workspace = Path(workspace).resolve()
        self.static_dir = (Path(static_dir) if static_dir else Path(__file__).with_name("static")).resolve()
        if mode not in ALLOWED_MODES:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST, "mode must be annotator or reviewer")
        if (mode == "reviewer") != (pass1 is not None):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "reviewer mode requires --pass1 and annotator mode forbids it")
        self.mode = mode
        self.pass1_path = Path(pass1).resolve() if pass1 is not None else None
        if (self.pass1_path is not None
                and (self.pass1_path == self.workspace
                     or self.workspace in self.pass1_path.parents)):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "reviewer workspace must not contain the immutable pass1 export")
        for protected, label in (
                (self.package, "package"), (self.static_dir, "static assets")):
            if _paths_overlap(self.workspace, protected):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"workspace must not overlap the {label} directory")
        if split is not None and split not in DEVELOPMENT_SPLITS:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST, "development split must be train or validation")
        self.package_kind = "development" if split else "pilot"
        self.dataset_split = split or "pilot"
        if self.package_kind == "development":
            self.images_dir = (self.package / self.dataset_split / "images").resolve()
            self.previews_dir = None
            self.coco_name = f"{self.dataset_split}-preannotations.coco.json"
            self.csv_name = "review.csv"
            required_package_files = [
                self.coco_name, self.csv_name, "development-manifest.json",
                "manifest.json", "selection.json",
            ]
        else:
            self.images_dir = (self.package / "images").resolve()
            self.previews_dir = (self.package / "previews").resolve()
            self.coco_name = "pilot-preannotations.coco.json"
            self.csv_name = "pilot-review.csv"
            required_package_files = [
                self.coco_name, self.csv_name, "pilot-manifest.json",
                "pilot-preview-provenance.json", "pilot-selection.json",
            ]
        self.state_path = self.workspace / (
            "reviewer-state.json" if self.mode == "reviewer" else "review-state.json")
        self.workspace_lock_path = self.workspace / ".review-workbench.lock"
        if self.mode == "reviewer":
            export_name = (
                f"review-candidate-{self.dataset_split}.coco.json"
                if self.package_kind == "development"
                else "review-candidate-pilot.coco.json"
            )
        else:
            export_name = (
                f"pass1-{self.dataset_split}-annotations.coco.json"
                if self.package_kind == "development"
                else "pass1-annotations.coco.json"
            )
        self.export_path = self.workspace / export_name
        self._lock = threading.RLock()
        self._bound_file_hashes: dict[Path, str] = {}
        catalog_path = Path(__file__).resolve().parents[2] / "config" / "p5-class-catalog.json"
        catalog = _read_json(catalog_path)
        catalog_classes = catalog.get("classes")
        if not isinstance(catalog_classes, list) or len(catalog_classes) != 9:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "class catalog must contain exactly nine classes")
        self.class_catalog_sha256 = _sha256(catalog_path)
        self._bound_file_hashes[catalog_path.resolve()] = self.class_catalog_sha256
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
        self.package_bindings = {}
        for name in required_package_files:
            path = self.package / name
            if not path.is_file():
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"package is missing {name}")
            self.package_bindings[name] = _sha256(path)
            self._bound_file_hashes[path.resolve()] = self.package_bindings[name]
        if self.package_kind == "development":
            self._validate_development_evidence_manifest()
        fingerprint_payload = {
            "package_kind": self.package_kind,
            "dataset_split": self.dataset_split,
            "bindings": self.package_bindings,
        }
        self.legacy_pilot_package_fingerprint = hashlib.sha256(json.dumps(
            self.package_bindings, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")).hexdigest()
        self.package_fingerprint = hashlib.sha256(json.dumps(
            fingerprint_payload, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")).hexdigest()
        self.source_coco, initial_state = self._load_package()
        self.source_pass1 = None
        self.source_pass1_sha256 = None
        self._source_content_hashes = {}
        self._source_annotator_identities = {}
        if self.mode == "reviewer":
            self.source_pass1_sha256 = _sha256(self.pass1_path)
            self._bound_file_hashes[self.pass1_path] = self.source_pass1_sha256
            self.source_pass1 = _read_json(self.pass1_path)
            initial_state = self._reviewer_initial_state(initial_state)
        self._validate_bound_files()
        self.image_count = len(initial_state["images"])
        self._identity = {
            item["id"]: (item["file_name"], item["sha256"], item["width"], item["height"])
            for item in initial_state["images"]
        }
        self.category_ids = {item["id"] for item in self.categories}
        self._state_sha256 = None
        with self._workspace_lock():
            if self.state_path.exists():
                state, state_sha256 = self._read_workspace_state()
                migrated_legacy_pilot = False
                if state.get("state_schema_version") != "p5-workbench-state-v2":
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "legacy workspace state requires explicit audited migration")
                if self.package_kind == "pilot":
                    if (state.get("package_fingerprint")
                            == self.legacy_pilot_package_fingerprint):
                        state["package_fingerprint"] = self.package_fingerprint
                        migrated_legacy_pilot = True
                    state.setdefault("package_kind", "pilot")
                    state.setdefault("dataset_split", "pilot")
                state.setdefault("operator_name", state.get("operator_id", ""))
                for image in state.get("images", []):
                    if any(field not in image for field in (
                            "annotator_id", "reviewed_by", "reviewer_id",
                            "review_outcome", "reviewed_at",
                            "source_content_sha256")):
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            "workspace state lacks v2 audit fields; explicit migration required")
                for category in state.get("categories", []):
                    if category.get("id") in {7, 8}:
                        category["labelable"] = True
                self._validate_state(state, expected_revision=None)
                self.state = state
                if migrated_legacy_pilot:
                    _atomic_json(self.state_path, self.state)
                    state_sha256 = _sha256(self.state_path)
                self._state_sha256 = state_sha256
            else:
                self._validate_state(initial_state, expected_revision=None)
                self.state = initial_state
                _atomic_json(self.state_path, self.state)
                self._state_sha256 = _sha256(self.state_path)

    def _validate_development_evidence_manifest(self) -> None:
        evidence = _read_json(self.package / "manifest.json")
        if (evidence.get("schema_version") != "p5-development-package-evidence-v1"
                or evidence.get("status") != "PASS"):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development package evidence manifest must have the expected schema and PASS status")
        if (evidence.get("ground_truth") is not False
                or evidence.get("accuracy_metrics_claimed") is not False
                or evidence.get("human_review_status") != "pending"
                or evidence.get("training_complete") is not False):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development package evidence manifest violates the non-truth review boundary")
        outputs = evidence.get("outputs")
        if not isinstance(outputs, list) or not outputs:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development package evidence manifest must contain output bindings")
        bindings = {}
        for record in outputs:
            if not isinstance(record, dict):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "development package output binding must be an object")
            relative = record.get("path")
            path = _safe_package_path(self.package, relative)
            normalized = path.relative_to(self.package).as_posix()
            if normalized == "manifest.json" or normalized in bindings:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    "development package output bindings contain a duplicate or manifest.json")
            expected_hash = record.get("sha256")
            expected_size = record.get("size")
            if (not isinstance(expected_hash, str) or len(expected_hash) != 64
                    or isinstance(expected_size, bool) or not isinstance(expected_size, int)
                    or expected_size < 0):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"development package output binding is invalid: {normalized}")
            if not path.is_file():
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"development package output is missing: {normalized}")
            if path.stat().st_size != expected_size or _sha256(path) != expected_hash:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"development package output binding mismatch: {normalized}")
            bindings[normalized] = (path, expected_hash)
        actual_files = {
            path.relative_to(self.package).as_posix()
            for path in self.package.rglob("*")
            if path.is_file() and path.resolve() != (self.package / "manifest.json").resolve()
        }
        if set(bindings) != actual_files:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development package outputs do not exactly cover the package files")
        for path, expected_hash in bindings.values():
            self._bound_file_hashes[path.resolve()] = expected_hash

    def _load_package(self):
        coco_path = self.package / self.coco_name
        csv_path = self.package / self.csv_name
        if not self.package.is_dir() or not coco_path.is_file() or not csv_path.is_file():
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"package must contain {self.coco_name} and {self.csv_name}")
        coco = _read_json(coco_path)
        images = coco.get("images")
        annotations = coco.get("annotations")
        categories = coco.get("categories")
        if not isinstance(images, list) or not images:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package must contain COCO images")
        if not isinstance(annotations, list) or not isinstance(categories, list):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package has invalid COCO collections")
        if coco.get("info", {}).get("class_catalog_sha256") != self.class_catalog_sha256:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "package class catalog hash does not match the local catalog")
        if self.package_kind == "development":
            expected_count = self._validate_development_package(images)
            preview_records = None
            previews_by_name = {}
        else:
            expected_count = PILOT_IMAGE_COUNT
            preview_provenance = _read_json(
                self.package / "pilot-preview-provenance.json")
            preview_records = preview_provenance.get("records")
            if (preview_provenance.get("valid") is not True
                    or not isinstance(preview_records, list)
                    or len(preview_records) != expected_count):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "package preview provenance is incomplete")
            previews_by_name = {
                item.get("file_name"): item
                for item in preview_records if isinstance(item, dict)
            }
            if len(previews_by_name) != expected_count or None in previews_by_name:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    "package preview provenance names must be unique")
        if len(images) != expected_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"{self.dataset_split} package must contain exactly "
                f"{expected_count} COCO images")

        try:
            with csv_path.open("r", encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.DictReader(stream))
        except (OSError, UnicodeError, csv.Error) as exc:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST, f"cannot read {self.csv_name}: {exc}") from exc
        if self.package_kind == "development":
            rows = [row for row in rows if row.get("split") == self.dataset_split]
        if len(rows) != expected_count or any(not row.get("file_name") for row in rows):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"{self.csv_name} must contain exactly {expected_count} named "
                f"{self.dataset_split} rows")
        rows_by_name = {row["file_name"]: row for row in rows}
        if len(rows_by_name) != expected_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"{self.csv_name} {self.dataset_split} file names must be unique")

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
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, f"{self.csv_name} is missing {name}")
            row = rows_by_name[name]
            if row.get("sha256", "").lower() != sha.lower() or row.get("dimensions") != f"{width}x{height}":
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"CSV identity mismatch for {name}")
            source_path = _contained_file(self.images_dir, name)
            self._bound_file_hashes[source_path] = sha.lower()
            try:
                actual_dimensions = _image_dimensions(source_path)
            except (OSError, ValueError, struct.error) as exc:
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"cannot verify image dimensions for {name}: {exc}") from exc
            if _sha256(source_path) != sha.lower() or actual_dimensions != (width, height):
                raise WorkbenchError(HTTPStatus.BAD_REQUEST, f"image hash or dimensions mismatch for {name}")
            preview_url = None
            if preview_records is not None:
                preview_path = _contained_file(
                    self.previews_dir, f"{Path(name).stem}.annotated.png")
                preview_record = previews_by_name.get(name)
                if (not isinstance(preview_record, dict)
                        or preview_record.get("source_image_sha256", "").lower() != sha.lower()
                        or preview_record.get("preview_sha256", "").lower()
                        != _sha256(preview_path)):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST, f"preview provenance mismatch for {name}")
                self._bound_file_hashes[preview_path] = (
                    preview_record["preview_sha256"].lower())
                try:
                    preview_dimensions = _image_dimensions(preview_path)
                except (OSError, ValueError, struct.error) as exc:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"cannot verify preview dimensions for {name}: {exc}") from exc
                if preview_dimensions != (width, height):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST, f"preview dimensions mismatch for {name}")
                preview_url = f"/previews/{Path(name).stem}.annotated.png"
            decision = (
                row.get("human_decision")
                or row.get("reviewed_decision")
                or image.get("cigarette_decision")
            )
            review_state = (
                row.get("review_status")
                or ("pending" if row.get("human_review_status") in {None, "", "pending"}
                    else row.get("human_review_status"))
            )
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
                "preview_url": preview_url,
                "decision": decision,
                "predicted_decision": (
                    image.get("predicted_decision")
                    or row.get("predicted_decision", "")),
                "review_state": review_state,
                "notes": row.get("notes") or row.get("reviewer_notes") or "",
                "boxes": annotations_by_image.get(image_id, []),
                "annotated_by": "",
                "annotator_id": "",
                "reviewed_by": "",
                "reviewer_id": "",
                "review_outcome": "",
                "reviewed_at": "",
                "source_content_sha256": "",
                "completed_revision": None,
            })
            image_ids.add(image_id)
            image_names.add(name)
        if set(rows_by_name) != image_names or set(annotations_by_image) - image_ids:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "COCO and CSV image identities do not match exactly")
        operator_id = next(iter(annotators)) if len(annotators) == 1 else ""
        state = {
            "revision": 0,
            "state_schema_version": "p5-workbench-state-v2",
            "mode": "annotator",
            "operator_name": operator_id,
            "operator_id": operator_id,
            "package_fingerprint": self.package_fingerprint,
            "package_kind": self.package_kind,
            "dataset_split": self.dataset_split,
            "categories": copy.deepcopy(self.state_categories),
            "images": state_images,
        }
        return coco, state

    def _reviewer_initial_state(self, package_state):
        pass1 = self.source_pass1
        info = pass1.get("info") if isinstance(pass1, dict) else None
        expected_info = {
            "schema_version": "p5-coco-v1",
            "annotation_stage": "pass1",
            "annotation_status": "annotated",
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
            "source": "human",
            "dataset_split": self.dataset_split,
            "package_kind": self.package_kind,
            "package_fingerprint": self.package_fingerprint,
            "class_catalog_sha256": self.class_catalog_sha256,
        }
        if not isinstance(info, dict):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST, "pass1 info must be an object")
        for field, expected in expected_info.items():
            if info.get(field) != expected:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 info.{field} must be {expected!r}")
        annotators = info.get("annotators")
        annotator_ids = info.get("annotator_ids")
        if (not isinstance(annotators, list) or not annotators
                or not isinstance(annotator_ids, list) or not annotator_ids
                or len(annotators) != len(annotator_ids)
                or any(not isinstance(value, str) or not value.strip()
                       for value in annotators + annotator_ids)):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "pass1 must bind nonempty annotator names and stable ids")
        if pass1.get("categories") != self.categories:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "pass1 categories do not match the local class catalog")

        package_images = {item["id"]: item for item in package_state["images"]}
        images = pass1.get("images")
        annotations = pass1.get("annotations")
        if (not isinstance(images, list) or len(images) != len(package_images)
                or not isinstance(annotations, list)):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "pass1 image or annotation collections are incomplete")
        pass1_images = {}
        for image in images:
            if not isinstance(image, dict):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "every pass1 image must be an object")
            image_id = _strict_int(image.get("id"), "pass1 image id", 1)
            if image_id in pass1_images or image_id not in package_images:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    "pass1 contains a duplicate or unknown image id")
            package_image = package_images[image_id]
            identity = (
                image.get("file_name"), image.get("sha256"),
                image.get("width"), image.get("height"))
            expected_identity = (
                package_image["file_name"], package_image["sha256"],
                package_image["width"], package_image["height"])
            if identity != expected_identity or image.get("split") != self.dataset_split:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 image identity does not match the frozen package: {image_id}")
            if (image.get("authorization_status") != "unverified"
                    or image.get("annotation_status") != "annotated"
                    or image.get("is_ground_truth") is not False
                    or image.get("source") != "human"):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 image is not clean non-truth human evidence: {image_id}")
            annotated_by = image.get("annotated_by")
            annotator_id = image.get("annotator_id")
            if (not isinstance(annotated_by, str) or not annotated_by.strip()
                    or not isinstance(annotator_id, str) or not annotator_id.strip()
                    or annotated_by not in annotators or annotator_id not in annotator_ids):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 image lacks bound annotator provenance: {image_id}")
            decision = image.get("cigarette_decision")
            notes = image.get("notes")
            if decision not in ALLOWED_DECISIONS or not isinstance(notes, str):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 image decision or notes are invalid: {image_id}")
            pass1_images[image_id] = image

        boxes_by_image = {}
        annotation_ids = set()
        for annotation in annotations:
            if not isinstance(annotation, dict):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    "every pass1 annotation must be an object")
            annotation_id = _strict_int(
                annotation.get("id"), "pass1 annotation id", 1)
            image_id = _strict_int(
                annotation.get("image_id"), "pass1 annotation image id", 1)
            category_id = _strict_int(
                annotation.get("category_id"), "pass1 annotation category id", 0)
            if (annotation_id in annotation_ids or image_id not in pass1_images
                    or category_id not in {item["id"] for item in self.categories}
                    or annotation.get("annotation_status") != "annotated"
                    or annotation.get("is_ground_truth") is not False
                    or annotation.get("source") != "human"
                    or PREDICTION_ANNOTATION_FIELDS.intersection(annotation)):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 annotation provenance is invalid: {annotation_id}")
            image = pass1_images[image_id]
            if (annotation.get("annotated_by") != image["annotated_by"]
                    or annotation.get("annotator_id") != image["annotator_id"]):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 annotation annotator binding is invalid: {annotation_id}")
            bbox = annotation.get("bbox")
            if not isinstance(bbox, list) or len(bbox) != 4:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 annotation bbox is invalid: {annotation_id}")
            x, y, width, height = [
                _finite_number(value, f"pass1 annotation {annotation_id} bbox")
                for value in bbox
            ]
            if (x < 0 or y < 0 or width <= 0 or height <= 0
                    or x + width > image["width"] or y + height > image["height"]):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"pass1 annotation is outside image bounds: {annotation_id}")
            annotation_ids.add(annotation_id)
            boxes_by_image.setdefault(image_id, []).append({
                "id": annotation_id,
                "category_id": category_id,
                "bbox": [x, y, width, height],
                "source": "human",
            })

        state = copy.deepcopy(package_state)
        state.update({
            "state_schema_version": "p5-workbench-state-v2",
            "mode": "reviewer",
            "operator_name": "",
            "operator_id": "",
            "source_pass1_sha256": self.source_pass1_sha256,
        })
        for state_image in state["images"]:
            source = pass1_images[state_image["id"]]
            boxes = boxes_by_image.get(state_image["id"], [])
            decision = source["cigarette_decision"]
            if decision == "OK" and boxes:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "pass1 OK image contains boxes")
            if decision == "NG" and not boxes:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "pass1 NG image contains no boxes")
            if decision == "NG" and any(box["category_id"] in {7, 8} for box in boxes):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    "pass1 NG image uses an unconfirmed class")
            if decision == "REVIEW" and not source["notes"].strip():
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST, "pass1 REVIEW image requires notes")
            state_image.update({
                "decision": decision,
                "review_state": "pending",
                "notes": source["notes"],
                "boxes": boxes,
                "annotated_by": source["annotated_by"],
                "annotator_id": source["annotator_id"],
                "reviewed_by": "",
                "reviewer_id": "",
                "review_outcome": "",
                "reviewed_at": "",
                "source_content_sha256": _canonical_sha256({
                    "decision": decision,
                    "notes": source["notes"],
                    "boxes": self._box_signature(boxes),
                }),
                "completed_revision": None,
            })
            self._source_content_hashes[state_image["id"]] = state_image[
                "source_content_sha256"]
            self._source_annotator_identities[state_image["id"]] = (
                state_image["annotated_by"], state_image["annotator_id"])
        return state

    def _validate_development_package(self, images) -> int:
        manifest = _read_json(self.package / "development-manifest.json")
        selection = _read_json(self.package / "selection.json")
        split_info = manifest.get("development_split")
        if not isinstance(split_info, dict):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development-manifest.json is missing development_split")
        counts = split_info.get("canonical_counts")
        if not isinstance(counts, dict):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development manifest is missing canonical split counts")
        expected_count = _strict_int(
            counts.get(self.dataset_split),
            f"development {self.dataset_split} count", 1)
        selection_count = selection.get(f"{self.dataset_split}_count")
        if selection_count != expected_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"selection {self.dataset_split} count does not match manifest")
        manifest_images = manifest.get("images")
        if not isinstance(manifest_images, list):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development manifest must contain an images list")
        split_records = {
            item.get("file_name"): item for item in manifest_images
            if isinstance(item, dict) and item.get("canonical") is True
            and item.get("split") == self.dataset_split
        }
        other_split = "validation" if self.dataset_split == "train" else "train"
        other_records = {
            item.get("file_name"): item for item in manifest_images
            if isinstance(item, dict) and item.get("canonical") is True
            and item.get("split") == other_split
        }
        pilot_names = {
            item.get("file_name") for item in manifest_images
            if isinstance(item, dict) and item.get("canonical") is True
            and item.get("split") == "pilot"
        }
        pilot_hashes = {
            item.get("sha256") for item in manifest_images
            if isinstance(item, dict) and item.get("canonical") is True
            and item.get("split") == "pilot"
        }
        if len(split_records) != expected_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"development manifest {self.dataset_split} membership is incomplete")
        other_expected_count = _strict_int(
            counts.get(other_split), f"development {other_split} count", 1)
        if len(other_records) != other_expected_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"development manifest {other_split} membership is incomplete")
        split_hashes = {item.get("sha256") for item in split_records.values()}
        other_hashes = {item.get("sha256") for item in other_records.values()}
        if set(split_records) & set(other_records) or split_hashes & other_hashes:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "development train and validation membership overlaps")
        coco_names = {item.get("file_name") for item in images if isinstance(item, dict)}
        coco_hashes = {item.get("sha256") for item in images if isinstance(item, dict)}
        if (len(coco_names) != len(images) or set(split_records) != coco_names
                or coco_names & pilot_names or coco_hashes & pilot_hashes):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"{self.dataset_split} COCO membership overlaps pilot or differs "
                "from the frozen development manifest")
        for image in images:
            record = split_records[image["file_name"]]
            if image.get("split") != self.dataset_split:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"image {image['file_name']} has the wrong development split")
            for field in ("sha256", "width", "height", "source_group"):
                if image.get(field) != record.get(field):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"development manifest identity mismatch for "
                        f"{image['file_name']}")
            if (image.get("annotation_status") != "preannotated"
                    or image.get("is_ground_truth") is not False
                    or image.get("authorization_status") != "unverified"
                    or image.get("p4_result_present") is not True):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"development image {image['file_name']} is not an unverified "
                    "P4 preannotation")
        return expected_count

    def _validate_state(self, state, expected_revision: int | None) -> None:
        if not isinstance(state, dict):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state must be a JSON object")
        revision = _strict_int(state.get("revision"), "revision", 0)
        if state.get("state_schema_version") != "p5-workbench-state-v2":
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "state schema version must be p5-workbench-state-v2")
        if expected_revision is not None and revision != expected_revision:
            raise WorkbenchError(HTTPStatus.CONFLICT, "stale revision")
        if state.get("mode") != self.mode:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST, f"mode must be {self.mode}")
        if state.get("package_fingerprint") != self.package_fingerprint:
            raise WorkbenchError(HTTPStatus.CONFLICT, "workspace state does not match the selected package")
        if (state.get("package_kind") != self.package_kind
                or state.get("dataset_split") != self.dataset_split):
            raise WorkbenchError(
                HTTPStatus.CONFLICT,
                "workspace state does not match the selected dataset split")
        operator_name = state.get("operator_name")
        operator_id = state.get("operator_id")
        if (not isinstance(operator_name, str) or len(operator_name) > 200
                or not isinstance(operator_id, str) or len(operator_id) > 200):
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                "operator_name and operator_id must be short strings")
        if self.mode == "reviewer":
            if state.get("source_pass1_sha256") != self.source_pass1_sha256:
                raise WorkbenchError(
                    HTTPStatus.CONFLICT,
                    "workspace state does not match the selected pass1 export")
        if state.get("categories") != self.state_categories:
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state categories do not match the local class catalog")
        images = state.get("images")
        if not isinstance(images, list) or len(images) != self.image_count:
            raise WorkbenchError(
                HTTPStatus.BAD_REQUEST,
                f"state must contain exactly {self.image_count} images")
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
            annotated_by = image.get("annotated_by")
            annotator_id = image.get("annotator_id")
            reviewed_by = image.get("reviewed_by")
            reviewer_id = image.get("reviewer_id")
            review_outcome = image.get("review_outcome")
            reviewed_at = image.get("reviewed_at")
            source_content_sha256 = image.get("source_content_sha256")
            if review_outcome not in ALLOWED_REVIEW_OUTCOMES:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"invalid review outcome for image {image_id}")
            if not isinstance(reviewed_at, str):
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"invalid reviewed_at for image {image_id}")
            if self.mode == "reviewer":
                if (not isinstance(annotated_by, str) or not annotated_by.strip()
                        or not isinstance(annotator_id, str) or not annotator_id.strip()):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"review image {image_id} requires source annotator provenance")
                if (annotated_by, annotator_id) != (
                        self._source_annotator_identities.get(image_id)):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        f"review image {image_id} source annotator binding does not match pass1")
                if source_content_sha256 != self._source_content_hashes.get(
                        image_id):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"review image {image_id} source content binding does not match pass1")
            elif source_content_sha256 != "" or review_outcome != "" or reviewed_at != "":
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"annotator image {image_id} cannot retain reviewer state")
            if image.get("review_state") == "annotation-complete":
                decision = image.get("decision")
                completed_revision = image.get("completed_revision")
                actor_name = reviewed_by if self.mode == "reviewer" else annotated_by
                actor_id = reviewer_id if self.mode == "reviewer" else annotator_id
                if (not isinstance(actor_name, str) or not actor_name.strip()
                        or not isinstance(actor_id, str) or not actor_id.strip()
                        or isinstance(completed_revision, bool) or not isinstance(completed_revision, int)
                        or completed_revision < 1 or completed_revision > max_completed_revision):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed image {image_id} requires server-stamped provenance")
                if self.mode == "reviewer":
                    if (_normalized_identity(reviewed_by)
                            == _normalized_identity(annotated_by)
                            or _normalized_identity(reviewer_id)
                            == _normalized_identity(annotator_id)):
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            f"review image {image_id} reviewer must differ from annotator")
                    try:
                        parsed_reviewed_at = dt.datetime.fromisoformat(reviewed_at)
                    except ValueError as exc:
                        raise WorkbenchError(
                            HTTPStatus.BAD_REQUEST,
                            f"review image {image_id} has invalid reviewed_at") from exc
                    if (review_outcome not in {
                            "accepted", "corrected", "unresolved"}
                            or parsed_reviewed_at.tzinfo is None
                            or parsed_reviewed_at.utcoffset() is None):
                        raise WorkbenchError(
                            HTTPStatus.BAD_REQUEST,
                            f"completed review image {image_id} requires outcome and timezone")
                    current_content_sha256 = _canonical_sha256({
                        "decision": decision,
                        "notes": notes,
                        "boxes": self._box_signature(boxes),
                    })
                    expected_outcome = (
                        "unresolved" if decision == "REVIEW"
                        else ("accepted"
                              if current_content_sha256
                              == source_content_sha256 else "corrected"))
                    if review_outcome != expected_outcome:
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            f"review image {image_id} outcome does not match its content")
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
            else:
                if image.get("completed_revision") is not None:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"pending image {image_id} cannot retain completion revision")
                if self.mode == "reviewer":
                    if (reviewed_by != "" or reviewer_id != ""
                            or review_outcome != "" or reviewed_at != ""):
                        raise WorkbenchError(
                            HTTPStatus.BAD_REQUEST,
                            f"pending review image {image_id} cannot retain reviewer provenance")
                elif (annotated_by != "" or annotator_id != ""
                      or reviewed_by != "" or reviewer_id != ""):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"pending image {image_id} cannot retain completion provenance")
        if seen_images != set(self._identity):
            raise WorkbenchError(HTTPStatus.BAD_REQUEST, "state image identity set is incomplete")

    def get_state(self):
        with self._lock:
            with self._workspace_lock():
                self._validate_bound_files()
                state, _ = self._read_workspace_state()
                self._validate_state(state, expected_revision=None)
                self.state = state
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

    @contextmanager
    def _workspace_lock(self):
        self.workspace.mkdir(parents=True, exist_ok=True)
        deadline = time.monotonic() + WORKSPACE_LOCK_TIMEOUT_SECONDS
        lock_stream = self.workspace_lock_path.open("a+b")
        lock_stream.seek(0, os.SEEK_END)
        if lock_stream.tell() == 0:
            lock_stream.write(b"\0")
            lock_stream.flush()
            os.fsync(lock_stream.fileno())
        acquired = False
        if os.name == "nt":
            import msvcrt
        else:
            import fcntl
        while not acquired:
            try:
                lock_stream.seek(0)
                if os.name == "nt":
                    msvcrt.locking(lock_stream.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    fcntl.flock(lock_stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                acquired = True
            except OSError:
                if time.monotonic() >= deadline:
                    lock_stream.close()
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "workspace is busy; retry after the other operation finishes")
                time.sleep(0.02)
        try:
            yield
        finally:
            lock_stream.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock_stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock_stream.fileno(), fcntl.LOCK_UN)
            lock_stream.close()

    def _read_workspace_state(self):
        try:
            raw = self.state_path.read_bytes()
            state = json.loads(raw.decode("utf-8-sig"))
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            raise WorkbenchError(
                HTTPStatus.CONFLICT,
                f"workspace state is unavailable or invalid: {exc}") from exc
        actual_sha256 = hashlib.sha256(raw).hexdigest()
        expected_sha256 = getattr(self, "_state_sha256", None)
        if expected_sha256 is not None and actual_sha256 != expected_sha256:
            raise WorkbenchError(
                HTTPStatus.CONFLICT,
                "stale revision or workspace state changed outside this process")
        return state, actual_sha256

    def _public_state(self, state):
        public = copy.deepcopy(state)
        public["session"] = {
            "revision": public["revision"],
            "mode": public["mode"],
            "operator_name": public["operator_name"],
            "operator_id": public["operator_id"],
            "package_kind": public["package_kind"],
            "dataset_split": public["dataset_split"],
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
        is_full_state = (payload.get("mode") == self.mode and isinstance(images, list) and
                         bool(images) and "file_name" in images[0])
        if is_full_state:
            normalized = copy.deepcopy(payload)
            if not normalized.get("operator_name"):
                normalized["operator_name"] = normalized.get("operator_id", "")
        else:
            revision = _strict_int(payload.get("revision"), "revision", 0)
            if not isinstance(images, list) or len(images) != self.image_count:
                raise WorkbenchError(
                    HTTPStatus.BAD_REQUEST,
                    f"state must contain exactly {self.image_count} images")
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
            normalized["operator_name"] = payload.get(
                "operator_name", payload.get("operator_id"))
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
        operator_name = normalized.get("operator_name")
        operator_name = operator_name.strip() if isinstance(operator_name, str) else ""
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
                image["annotator_id"] = current["annotator_id"]
                image["reviewed_by"] = current["reviewed_by"]
                image["reviewer_id"] = current["reviewer_id"]
                image["review_outcome"] = current["review_outcome"]
                image["reviewed_at"] = current["reviewed_at"]
                image["source_content_sha256"] = current["source_content_sha256"]
                image["completed_revision"] = current["completed_revision"]
            elif completed:
                if image.get("decision") == "OK" and image.get("boxes"):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed OK image {image['id']} must not contain boxes")
                if image.get("decision") == "NG" and not image.get("boxes"):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed NG image {image['id']} requires at least one box")
                if (image.get("decision") == "REVIEW"
                        and not str(image.get("notes", "")).strip()):
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        f"completed REVIEW image {image['id']} requires notes")
                if not operator_name or not operator_id:
                    raise WorkbenchError(
                        HTTPStatus.BAD_REQUEST,
                        "operator_name and operator_id are required to complete an image")
                if self.mode == "reviewer":
                    if (_normalized_identity(operator_name)
                            == _normalized_identity(current["annotated_by"])
                            or _normalized_identity(operator_id)
                            == _normalized_identity(current["annotator_id"])):
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            "reviewer name and stable id must differ from the annotator")
                    image["annotated_by"] = current["annotated_by"]
                    image["annotator_id"] = current["annotator_id"]
                    image["reviewed_by"] = operator_name
                    image["reviewer_id"] = operator_id
                    source_signature = current["source_content_sha256"]
                    current_signature = _canonical_sha256({
                        "decision": image["decision"],
                        "notes": image["notes"],
                        "boxes": self._box_signature(image["boxes"]),
                    })
                    image["review_outcome"] = (
                        "unresolved" if image["decision"] == "REVIEW"
                        else ("accepted" if current_signature == source_signature
                              else "corrected"))
                    image["reviewed_at"] = dt.datetime.now(
                        dt.timezone.utc).astimezone().isoformat(
                            timespec="microseconds")
                else:
                    image["annotated_by"] = operator_name
                    image["annotator_id"] = operator_id
                    image["reviewed_by"] = ""
                    image["reviewer_id"] = ""
                    image["review_outcome"] = ""
                    image["reviewed_at"] = ""
                    image["source_content_sha256"] = ""
                image["completed_revision"] = next_revision
            else:
                if self.mode == "reviewer":
                    image["annotated_by"] = current["annotated_by"]
                    image["annotator_id"] = current["annotator_id"]
                else:
                    image["annotated_by"] = ""
                    image["annotator_id"] = ""
                image["reviewed_by"] = ""
                image["reviewer_id"] = ""
                image["review_outcome"] = ""
                image["reviewed_at"] = ""
                image["completed_revision"] = None

    def save(self, state):
        with self._lock:
            with self._workspace_lock():
                self._validate_bound_files()
                disk_state, _ = self._read_workspace_state()
                self._validate_state(disk_state, expected_revision=None)
                self.state = disk_state
                normalized = self._normalize_save_payload(state)
                self._stamp_completion_provenance(normalized)
                self._validate_state(
                    normalized, expected_revision=disk_state["revision"])
                saved = copy.deepcopy(normalized)
                saved["revision"] += 1
                _atomic_json(self.state_path, saved)
                self._state_sha256 = _sha256(self.state_path)
                self.state = saved
                return self._public_state(saved)

    def export_pass1(self):
        if self.mode != "annotator":
            raise WorkbenchError(
                HTTPStatus.CONFLICT,
                "pass1 export is available only in annotator mode")
        with self._lock:
            with self._workspace_lock():
                self._validate_bound_files()
                disk_state, state_sha256 = self._read_workspace_state()
                self._validate_state(disk_state, expected_revision=None)
                self.state = disk_state
                state = copy.deepcopy(disk_state)
                if any(item["review_state"] != "annotation-complete"
                       for item in state["images"]):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "all images must be annotation-complete")
                operator_name = state["operator_name"].strip()
                operator_id = state["operator_id"].strip()
                if not operator_name or not operator_id:
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "nonempty operator name and stable id required for pass1 export")
                if any(not item["annotated_by"].strip() for item in state["images"]):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "every completed image requires annotator provenance")
                if any(not item["annotator_id"].strip() for item in state["images"]):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "every completed image requires stable annotator id provenance")
                source_images = {item["id"]: item for item in self.source_coco["images"]}
                annotators = sorted({item["annotated_by"] for item in state["images"]})
                annotator_ids = sorted(
                    {item["annotator_id"] for item in state["images"]})
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
                        "dataset_split": self.dataset_split,
                        "package_kind": self.package_kind,
                        "annotators": annotators,
                        "annotator_ids": annotator_ids,
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
                        "annotator_id": current["annotator_id"],
                        "completed_revision": current["completed_revision"],
                        "cigarette_decision": current["decision"],
                        "notes": current["notes"],
                    }
                    exported["images"].append(image)
                    boxes = current["boxes"]
                    if current["decision"] == "OK" and boxes:
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT, "OK images must have no annotations")
                    if current["decision"] == "NG" and not boxes:
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            "NG images must have at least one annotation")
                    if (current["decision"] == "NG"
                            and any(box["category_id"] in {7, 8} for box in boxes)):
                        raise WorkbenchError(
                            HTTPStatus.CONFLICT,
                            "classes 7 and 8 cannot be exported as NG")
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
                            "annotator_id": current["annotator_id"],
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
                    "dataset_split": self.dataset_split,
                }

    @staticmethod
    def _box_signature(boxes):
        return sorted(
            (
                int(box["category_id"]),
                tuple(round(float(value), 9) for value in box["bbox"]),
            )
            for box in boxes
        )

    def export_review_candidate(self):
        if self.mode != "reviewer":
            raise WorkbenchError(
                HTTPStatus.CONFLICT,
                "authorization and independent reviewer required")
        with self._lock:
            with self._workspace_lock():
                self._validate_bound_files()
                disk_state, state_sha256 = self._read_workspace_state()
                self._validate_state(disk_state, expected_revision=None)
                self.state = disk_state
                state = copy.deepcopy(disk_state)
                if any(item["review_state"] != "annotation-complete"
                       for item in state["images"]):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "all images must be independently reviewed")
                reviewers = sorted(
                    {item["reviewed_by"] for item in state["images"]})
                reviewer_ids = sorted(
                    {item["reviewer_id"] for item in state["images"]})
                if (not reviewers or not reviewer_ids
                        or any(not value.strip()
                               for value in reviewers + reviewer_ids)):
                    raise WorkbenchError(
                        HTTPStatus.CONFLICT,
                        "every image requires reviewer name and stable id provenance")

                source_info = self.source_pass1["info"]
                source_images = {
                    item["id"]: item for item in self.source_pass1["images"]}
                source_boxes = {}
                for annotation in self.source_pass1["annotations"]:
                    source_boxes.setdefault(annotation["image_id"], []).append(annotation)
                exported = {
                    "info": {
                        "description": (
                            "P5 independently reviewed candidate; explicit approval "
                            "is still required before ground-truth promotion"),
                        "schema_version": "p5-coco-v1",
                        "class_catalog_sha256": self.class_catalog_sha256,
                        "ground_truth_complete": False,
                        "accuracy_metrics_claimed": False,
                        "annotation_stage": "independent-review-candidate",
                        "annotation_status": "independently-reviewed",
                        "authorization_status": "unverified",
                        "source": "human",
                        "dataset_split": self.dataset_split,
                        "package_kind": self.package_kind,
                        "annotators": copy.deepcopy(source_info["annotators"]),
                        "annotator_ids": copy.deepcopy(
                            source_info["annotator_ids"]),
                        "reviewers": reviewers,
                        "reviewer_ids": reviewer_ids,
                        "review_method": "human-double-review-pending-approval",
                        "source_pass1_sha256": self.source_pass1_sha256,
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
                    current_signature = self._box_signature(current["boxes"])
                    source_signature = self._box_signature(
                        source_boxes.get(current["id"], []))
                    review_changed = (
                        current["decision"]
                        != source_image["cigarette_decision"]
                        or current["notes"] != source_image.get("notes", "")
                        or current_signature != source_signature
                    )
                    exported["images"].append({
                        "id": current["id"],
                        "file_name": current["file_name"],
                        "width": current["width"],
                        "height": current["height"],
                        "sha256": current["sha256"],
                        "source_group": source_image.get("source_group"),
                        "split": source_image.get("split"),
                        "authorization_status": "unverified",
                        "annotation_status": "independently-reviewed",
                        "is_ground_truth": False,
                        "source": "human",
                        "annotated_by": current["annotated_by"],
                        "annotator_id": current["annotator_id"],
                        "reviewed_by": current["reviewed_by"],
                        "reviewer_id": current["reviewer_id"],
                        "reviewed_revision": current["completed_revision"],
                        "reviewed_at": current["reviewed_at"],
                        "review_outcome": current["review_outcome"],
                        "source_content_sha256": current["source_content_sha256"],
                        "review_changed": review_changed,
                        "cigarette_decision": current["decision"],
                        "notes": current["notes"],
                    })
                    if current["decision"] not in {"NG", "REVIEW"}:
                        continue
                    for box in current["boxes"]:
                        output_annotations.append({
                            "id": box["id"],
                            "image_id": current["id"],
                            "category_id": box["category_id"],
                            "bbox": [float(value) for value in box["bbox"]],
                            "area": (
                                float(box["bbox"][2])
                                * float(box["bbox"][3])),
                            "iscrowd": 0,
                            "annotation_status": "independently-reviewed",
                            "is_ground_truth": False,
                            "source": "human",
                            "annotated_by": current["annotated_by"],
                            "annotator_id": current["annotator_id"],
                            "reviewed_by": current["reviewed_by"],
                            "reviewer_id": current["reviewer_id"],
                            "reviewed_revision": current["completed_revision"],
                            "reviewed_at": current["reviewed_at"],
                            "review_outcome": current["review_outcome"],
                            "source_content_sha256": current[
                                "source_content_sha256"],
                        })
                exported["annotations"] = output_annotations
                _atomic_json(self.export_path, exported)
                return {
                    "path": str(self.export_path),
                    "sha256": _sha256(self.export_path),
                    "image_count": len(exported["images"]),
                    "annotation_count": len(output_annotations),
                    "changed_image_count": sum(
                        item["review_changed"] for item in exported["images"]),
                    "status": "independently-reviewed",
                    "authorization_status": "unverified",
                    "is_ground_truth": False,
                    "state_revision": state["revision"],
                    "review_state_sha256": state_sha256,
                    "source_pass1_sha256": self.source_pass1_sha256,
                    "package_fingerprint": self.package_fingerprint,
                    "dataset_split": self.dataset_split,
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

    def _empty_response(self, status: int) -> None:
        self.send_response(status)
        self.send_header("Content-Length", "0")
        self.send_header("Cache-Control", "public, max-age=86400")
        self.end_headers()

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
                self._json_response(HTTPStatus.OK, {
                    "status": "ok",
                    "mode": self.workbench.mode,
                    "package_kind": self.workbench.package_kind,
                    "dataset_split": self.workbench.dataset_split,
                    "image_count": self.workbench.image_count,
                })
            elif path == "/favicon.ico":
                self._empty_response(HTTPStatus.NO_CONTENT)
            elif path == "/api/state":
                self._json_response(HTTPStatus.OK, self.workbench.get_state())
            elif path in {"/", "/index.html"}:
                self._send_file(_contained_file(self.workbench.static_dir, "index.html"))
            elif path.startswith("/static/"):
                self._send_file(_contained_file(self.workbench.static_dir, path[len("/static/"):]))
            elif path.startswith("/images/"):
                self._send_file(_contained_file(self.workbench.images_dir, path[len("/images/"):]))
            elif path.startswith("/previews/"):
                if self.workbench.previews_dir is None:
                    raise WorkbenchError(
                        HTTPStatus.NOT_FOUND,
                        "this package does not include rendered model previews")
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
                self._json_response(
                    HTTPStatus.OK, self.workbench.export_review_candidate())
            else:
                raise WorkbenchError(HTTPStatus.NOT_FOUND, "route not found")
        except WorkbenchError as exc:
            self._error(exc)


def create_server(
        package: Path, workspace: Path, host: str = "127.0.0.1",
        port: int = 8765, static_dir: Path | None = None,
        split: str | None = None, mode: str = "annotator",
        pass1: Path | None = None) -> WorkbenchHTTPServer:
    if host not in {"127.0.0.1", "localhost", "::1"}:
        raise WorkbenchError(HTTPStatus.BAD_REQUEST, "review workbench may bind only to loopback")
    return WorkbenchHTTPServer(
        (host, port), ReviewWorkbench(
            package, workspace, static_dir, split, mode, pass1))


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--package", required=True, type=Path,
        help="P5 pilot review package or development package directory")
    parser.add_argument("--workspace", required=True, type=Path, help="draft and export output directory")
    parser.add_argument(
        "--split", choices=sorted(DEVELOPMENT_SPLITS),
        help="read the frozen train or validation split from a development package")
    parser.add_argument(
        "--mode", choices=sorted(ALLOWED_MODES), default="annotator",
        help="run first-pass annotation or independent second-person review")
    parser.add_argument(
        "--pass1", type=Path,
        help="immutable pass1 COCO export required by reviewer mode")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    if not 0 <= args.port <= 65535:
        raise SystemExit("--port must be between 0 and 65535")
    server = create_server(
        args.package, args.workspace, args.host, args.port, split=args.split,
        mode=args.mode, pass1=args.pass1)
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
