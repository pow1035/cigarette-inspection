#!/usr/bin/env python3
"""P5 dataset audit, preannotation validation, and evaluation tools.

The file intentionally uses only the Python standard library so the evidence
workflow can run in a plain Windows Python installation.
"""

from __future__ import annotations

import argparse
import copy
import csv
import datetime as dt
import hashlib
import json
import math
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable


CLASS_NAMES = (
    "dakoucuoya",
    "feiyan",
    "jiamo",
    "lvzuizhezhou",
    "quezui",
    "yanbangposun",
    "yanbangzangwu",
    "wuzi",
    "jietou",
)
VALID_DECISIONS = {"OK", "NG", "REVIEW"}
VALID_STATUSES = {"unlabeled", "preannotated", "annotated", "reviewed"}
VALID_SPLITS = {"pilot", "train", "validation", "test", "unassigned"}
VALID_AUTHORIZATION_STATUSES = {"unverified", "approved", "restricted"}
IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png"}
TIMESTAMP_RE = re.compile(r"(?<!\d)(\d{17})(?!\d)")
SOURCE_RE = re.compile(r"^(?:(\d+)_)?\d{17}$")
DEFAULT_CLASS_CATALOG = Path(__file__).resolve().parents[1] / "config" / "p5-class-catalog.json"
CLASS_DEFINITIONS: tuple[dict[str, Any], ...] = tuple(
    {"id": index, "name": name, "displayNameZh": name,
     "mappingStatus": "source-backed-needs-business-approval"}
    for index, name in enumerate(CLASS_NAMES)
)
CLASS_CATALOG_SHA256 = ""


class DatasetError(ValueError):
    """A user-correctable dataset or annotation error."""


def read_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8-sig") as handle:
            return json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise DatasetError(f"cannot read JSON {path}: {exc}") from exc


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as handle:
            json.dump(value, handle, ensure_ascii=False, indent=2, sort_keys=False)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def configure_classes(path: Path) -> None:
    global CLASS_NAMES, CLASS_DEFINITIONS, CLASS_CATALOG_SHA256
    value = read_json(path)
    classes = value.get("classes") if isinstance(value, dict) else None
    if not isinstance(classes, list) or not classes:
        raise DatasetError("class catalog must contain a non-empty classes list")
    for item in classes:
        if (not isinstance(item, dict) or not isinstance(item.get("id"), int)
                or isinstance(item.get("id"), bool)):
            raise DatasetError("class catalog ids must be strict integers, not booleans")
    ordered = sorted(classes, key=lambda item: item.get("id", -1) if isinstance(item, dict) else -1)
    expected_ids = list(range(len(ordered)))
    actual_ids = [item.get("id") if isinstance(item, dict) else None for item in ordered]
    if actual_ids != expected_ids:
        raise DatasetError(f"class catalog ids must be contiguous from zero: {actual_ids}")
    names = [item.get("name") for item in ordered]
    if any(not isinstance(name, str) or not name for name in names) or len(set(names)) != len(names):
        raise DatasetError("class catalog names must be unique non-empty strings")
    for item in ordered:
        if not isinstance(item.get("mappingStatus"), str):
            raise DatasetError(f"class catalog mappingStatus is missing for id {item['id']}")
    CLASS_NAMES = tuple(names)
    CLASS_DEFINITIONS = tuple(dict(item) for item in ordered)
    CLASS_CATALOG_SHA256 = sha256_file(path)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_size(path: Path) -> tuple[int, int, str]:
    """Return width, height, and detected format for PNG/JPEG."""
    with path.open("rb") as handle:
        signature = handle.read(24)
        if signature.startswith(b"\x89PNG\r\n\x1a\n"):
            if len(signature) < 24 or signature[12:16] != b"IHDR":
                raise DatasetError(f"invalid PNG header: {path}")
            width, height = struct.unpack(">II", signature[16:24])
            return width, height, "PNG"
        if not signature.startswith(b"\xff\xd8"):
            raise DatasetError(f"unsupported image format: {path}")
        handle.seek(2)
        while True:
            marker_start = handle.read(1)
            if not marker_start:
                break
            if marker_start != b"\xff":
                continue
            marker = handle.read(1)
            while marker == b"\xff":
                marker = handle.read(1)
            if not marker:
                break
            code = marker[0]
            if code in {0xD8, 0xD9} or 0xD0 <= code <= 0xD7:
                continue
            length_data = handle.read(2)
            if len(length_data) != 2:
                break
            segment_length = struct.unpack(">H", length_data)[0]
            if segment_length < 2:
                break
            if code in {
                0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
                0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF,
            }:
                payload = handle.read(5)
                if len(payload) != 5:
                    break
                height, width = struct.unpack(">HH", payload[1:5])
                return width, height, "JPEG"
            handle.seek(segment_length - 2, 1)
    raise DatasetError(f"cannot determine image dimensions: {path}")


def filename_metadata(path: Path) -> tuple[str, str | None]:
    stem = path.stem
    match = SOURCE_RE.fullmatch(stem)
    timestamp_match = TIMESTAMP_RE.search(stem)
    source_group = match.group(1) if match and match.group(1) else "unprefixed"
    captured_at = None
    if timestamp_match:
        raw = timestamp_match.group(1)
        try:
            captured_at = dt.datetime.strptime(raw, "%Y%m%d%H%M%S%f").isoformat(
                timespec="milliseconds"
            )
        except ValueError:
            captured_at = None
    return source_group, captured_at


def coco_categories() -> list[dict[str, Any]]:
    return [
        {
            "id": item["id"],
            "name": item["name"],
            "display_name_zh": item.get("displayNameZh", item["name"]),
            "mapping_status": item["mappingStatus"],
            "supercategory": "cigarette_defect",
        }
        for item in CLASS_DEFINITIONS
    ]


def audit_dataset(image_dir: Path, legacy_dir: Path | None = None) -> tuple[dict, dict, dict]:
    if not image_dir.is_dir():
        raise DatasetError(f"image directory does not exist: {image_dir}")
    images = sorted(
        (path for path in image_dir.iterdir() if path.is_file() and path.suffix.lower() in IMAGE_SUFFIXES),
        key=lambda path: path.name.lower(),
    )
    if not images:
        raise DatasetError(f"no JPEG/PNG images found: {image_dir}")
    legacy_names = set()
    if legacy_dir is not None:
        if not legacy_dir.is_dir():
            raise DatasetError(f"legacy prediction directory does not exist: {legacy_dir}")
        legacy_names = {path.name.lower() for path in legacy_dir.iterdir() if path.is_file()}

    records: list[dict[str, Any]] = []
    by_hash: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for path in images:
        width, height, image_format = image_size(path)
        digest = sha256_file(path)
        source_group, captured_at = filename_metadata(path)
        record = {
            "file_name": path.name,
            "relative_path": path.name,
            "sha256": digest,
            "width": width,
            "height": height,
            "format": image_format,
            "source_group": source_group,
            "captured_at": captured_at,
            "legacy_prediction_paired": path.name.lower() in legacy_names,
            "canonical": False,
            "canonical_file_name": None,
            "duplicate_group": None,
            "split": "unassigned",
            "authorization_status": "unverified",
        }
        records.append(record)
        by_hash[digest].append(record)

    duplicate_groups = []
    for digest in sorted(by_hash):
        group = sorted(by_hash[digest], key=lambda item: item["file_name"].lower())
        canonical = group[0]["file_name"]
        duplicate_group = f"sha256:{digest}" if len(group) > 1 else None
        for record in group:
            record["canonical"] = record["file_name"] == canonical
            record["canonical_file_name"] = canonical
            record["duplicate_group"] = duplicate_group
        if len(group) > 1:
            duplicate_groups.append(
                {"sha256": digest, "canonical_file_name": canonical,
                 "file_names": [item["file_name"] for item in group]}
            )

    canonical_records = [record for record in records if record["canonical"]]
    manifest = {
        "schema_version": "p5-dataset-manifest-v1",
        "image_root": str(image_dir.resolve()),
        "legacy_prediction_root": str(legacy_dir.resolve()) if legacy_dir else None,
        "split_policy": "assign canonical hashes only; all duplicate aliases inherit the canonical split",
        "images": records,
        "duplicate_groups": duplicate_groups,
    }
    summary = {
        "image_count": len(records),
        "unique_sha256_count": len(canonical_records),
        "duplicate_group_count": len(duplicate_groups),
        "duplicate_file_count": len(records) - len(canonical_records),
        "legacy_prediction_pair_count": sum(bool(item["legacy_prediction_paired"]) for item in records),
        "source_group_counts": dict(sorted(Counter(item["source_group"] for item in records).items())),
        "dimension_counts": dict(sorted(Counter(f'{item["width"]}x{item["height"]}' for item in records).items())),
        "annotation_ground_truth_available": False,
        "accuracy_metrics_claimed": False,
    }
    template_images = []
    for image_id, record in enumerate(canonical_records, 1):
        template_images.append({
            "id": image_id,
            "file_name": record["file_name"],
            "width": record["width"],
            "height": record["height"],
            "sha256": record["sha256"],
            "source_group": record["source_group"],
            "captured_at": record["captured_at"],
            "split": "unassigned",
            "annotation_status": "unlabeled",
            "cigarette_decision": None,
            "is_ground_truth": False,
            "authorization_status": "unverified",
        })
    template = {
        "info": {
            "description": "P5 canonical annotation template",
            "schema_version": "p5-coco-v1",
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
        },
        "images": template_images,
        "annotations": [],
        "categories": coco_categories(),
    }
    return manifest, summary, template


def canonical_map(manifest: dict) -> dict[str, dict]:
    images = manifest.get("images")
    if not isinstance(images, list):
        raise DatasetError("manifest.images must be a list")
    result = {}
    all_names = set()
    canonical_hashes = set()
    for item in images:
        if not isinstance(item, dict):
            raise DatasetError("every manifest image must be an object")
        name = item.get("file_name")
        if not isinstance(name, str) or not name or name in all_names:
            raise DatasetError("manifest file names must be unique non-empty strings")
        all_names.add(name)
        if item.get("canonical") is True:
            digest = item.get("sha256")
            if not isinstance(digest, str) or not digest or digest in canonical_hashes:
                raise DatasetError("manifest canonical SHA-256 values must be unique non-empty strings")
            if item.get("canonical_file_name") != name:
                raise DatasetError(f"canonical image must reference itself: {name}")
            canonical_hashes.add(digest)
            result[name] = item
    if not result:
        raise DatasetError("manifest contains no canonical images")
    for item in images:
        canonical_name = item.get("canonical_file_name")
        if canonical_name not in result:
            raise DatasetError(
                f"manifest image references unknown canonical file: {item.get('file_name')}")
        if item.get("sha256") != result[canonical_name].get("sha256"):
            raise DatasetError(
                f"manifest alias SHA-256 differs from canonical: {item.get('file_name')}")
    return result


def normalize_preannotation_bbox(
        bbox: list[Any], image_width: int, image_height: int,
        epsilon: float = 1e-3) -> tuple[list[Any], bool]:
    if (len(bbox) != 4 or
            any(not isinstance(value, (int, float)) or isinstance(value, bool)
                or not math.isfinite(value) for value in bbox)):
        return bbox, False
    x, y, width, height = (float(value) for value in bbox)
    original = [x, y, width, height]
    if -epsilon <= x < 0:
        width += x
        x = 0.0
    if -epsilon <= y < 0:
        height += y
        y = 0.0
    right = x + width
    bottom = y + height
    if image_width < right <= image_width + epsilon:
        width = float(image_width) - x
    if image_height < bottom <= image_height + epsilon:
        height = float(image_height) - y
    normalized = [x, y, width, height]
    return normalized, normalized != original


def preannotate(manifest: dict, p4_results: Path) -> dict:
    canonical = canonical_map(manifest)
    aliases = {
        item["file_name"]: item.get("canonical_file_name")
        for item in manifest["images"]
        if isinstance(item, dict) and isinstance(item.get("file_name"), str)
    }
    if not p4_results.is_dir():
        raise DatasetError(f"P4 result directory does not exist: {p4_results}")
    result_files = sorted(p4_results.glob("frame-*.json"))
    if not result_files:
        raise DatasetError(f"no P4 frame JSON found: {p4_results}")
    by_source = {}
    for path in result_files:
        value = read_json(path)
        source = value.get("sourceFile")
        if not isinstance(source, str):
            raise DatasetError(f"P4 result has no sourceFile: {path}")
        canonical_name = aliases.get(source)
        if canonical_name is None:
            raise DatasetError(f"P4 source is absent from audit manifest: {source}")
        if canonical_name == source or canonical_name not in by_source:
            by_source[canonical_name] = value

    images = []
    annotations = []
    annotation_id = 1
    for image_id, name in enumerate(sorted(canonical, key=str.lower), 1):
        record = canonical[name]
        result = by_source.get(name)
        images.append({
            "id": image_id,
            "file_name": name,
            "width": record["width"],
            "height": record["height"],
            "sha256": record["sha256"],
            "source_group": record.get("source_group"),
            "split": record.get("split", "unassigned"),
            "annotation_status": "preannotated",
            "cigarette_decision": result.get("decision") if result else "OK",
            "is_ground_truth": False,
            "authorization_status": record.get("authorization_status", "unverified"),
            "p4_result_present": result is not None,
        })
        for defect in (result.get("defects", []) if result else []):
            box = defect.get("box", {})
            source_bbox = [box.get("x"), box.get("y"), box.get("width"), box.get("height")]
            bbox, boundary_clamped = normalize_preannotation_bbox(
                source_bbox, record["width"], record["height"])
            category_id = defect.get("classId")
            annotation = {
                "id": annotation_id,
                "image_id": image_id,
                "category_id": category_id,
                "bbox": bbox,
                "area": bbox[2] * bbox[3] if all(isinstance(v, (int, float)) for v in bbox) else None,
                "iscrowd": 0,
                "score": defect.get("confidence"),
                "annotation_status": "preannotated",
                "is_ground_truth": False,
                "detector_version": defect.get("detectorVersion"),
            }
            if boundary_clamped:
                annotation["source_bbox"] = source_bbox
                annotation["boundary_clamped"] = True
            annotations.append(annotation)
            annotation_id += 1
    return {
        "info": {
            "description": "P4 predictions converted to P5 COCO preannotations",
            "schema_version": "p5-coco-v1",
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
        },
        "images": images,
        "annotations": annotations,
        "categories": coco_categories(),
    }


def _pilot_features(image: dict, manifest_record: dict, category_ids: set[int]) -> set[str]:
    return {
        f"source:{manifest_record.get('source_group')}",
        f"dimension:{manifest_record.get('width')}x{manifest_record.get('height')}",
        f"decision:{image.get('cigarette_decision')}",
        *(f"class:{category_id}" for category_id in sorted(category_ids)),
    }


def select_pilot(
        manifest: dict, preannotations: dict, size: int = 30
        ) -> tuple[dict, dict, dict, list[dict[str, Any]]]:
    """Build a deterministic, canonical-only pilot review package."""
    canonical = canonical_map(manifest)
    if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
        raise DatasetError("pilot size must be a positive integer")
    if size > len(canonical):
        raise DatasetError(
            f"pilot size {size} exceeds {len(canonical)} canonical images")
    assigned = sorted(
        item["file_name"] for item in manifest["images"]
        if item.get("split") != "unassigned")
    if assigned:
        raise DatasetError(
            "pilot selection refuses to overwrite existing split assignments: "
            + ", ".join(assigned))

    validation_errors = validate_annotations(preannotations, manifest)
    if validation_errors:
        raise DatasetError(
            "preannotations are invalid:\n- " + "\n- ".join(validation_errors))
    if (preannotations.get("info", {}).get("ground_truth_complete") is not False
            or preannotations.get("info", {}).get("accuracy_metrics_claimed") is not False):
        raise DatasetError("pilot input must explicitly be predictions-only preannotations")
    invalid_prediction_images = [
        item.get("file_name") for item in preannotations.get("images", [])
        if item.get("annotation_status") != "preannotated"
        or item.get("is_ground_truth") is not False
        or item.get("p4_result_present") is not True
    ]
    invalid_prediction_annotations = [
        item.get("id") for item in preannotations.get("annotations", [])
        if item.get("annotation_status") != "preannotated"
        or item.get("is_ground_truth") is not False
        or not isinstance(item.get("score"), (int, float))
        or isinstance(item.get("score"), bool)
        or not math.isfinite(float(item.get("score")))
        or not isinstance(item.get("detector_version"), str)
        or not item.get("detector_version")
    ]
    if invalid_prediction_images or invalid_prediction_annotations:
        raise DatasetError(
            "pilot input contains non-prediction records; "
            f"images={invalid_prediction_images}, annotations={invalid_prediction_annotations}")
    images = preannotations.get("images", [])
    by_name = {item.get("file_name"): item for item in images if isinstance(item, dict)}
    if set(by_name) != set(canonical):
        missing = sorted(set(canonical) - set(by_name))
        extra = sorted(set(by_name) - set(canonical))
        raise DatasetError(
            f"preannotations must cover canonical manifest exactly; missing={missing}, extra={extra}")

    annotations_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for annotation in preannotations.get("annotations", []):
        annotations_by_image[annotation["image_id"]].append(annotation)

    candidates = []
    for name in sorted(canonical, key=str.lower):
        image = by_name[name]
        image_annotations = annotations_by_image[image["id"]]
        category_ids = {item["category_id"] for item in image_annotations}
        candidates.append({
            "name": name,
            "sha256": canonical[name]["sha256"],
            "image": image,
            "annotations": image_annotations,
            "category_ids": category_ids,
            "features": _pilot_features(image, canonical[name], category_ids),
        })

    feature_frequency = Counter(
        feature for candidate in candidates for feature in candidate["features"])
    required_features = set(feature_frequency)
    selected = []
    selected_names = set()
    uncovered = set(required_features)

    while uncovered and len(selected) < size:
        ranked = []
        for candidate in candidates:
            if candidate["name"] in selected_names:
                continue
            new_features = candidate["features"] & uncovered
            if not new_features:
                continue
            rarity = sum(1.0 / feature_frequency[item] for item in new_features)
            ranked.append((-rarity, -len(new_features), candidate["sha256"], candidate["name"],
                           candidate, sorted(new_features)))
        if not ranked:
            break
        _, _, _, _, chosen, reasons = min(ranked)
        selected.append((chosen, reasons))
        selected_names.add(chosen["name"])
        uncovered -= chosen["features"]

    if uncovered:
        raise DatasetError(
            f"pilot size {size} cannot cover observed features: {sorted(uncovered)}")

    selected_feature_counts = Counter(
        feature for candidate, _ in selected for feature in candidate["features"])
    while len(selected) < size:
        ranked = []
        for candidate in candidates:
            if candidate["name"] in selected_names:
                continue
            balance = sum(
                1.0 / (1 + selected_feature_counts[feature])
                for feature in candidate["features"]
                if not feature.startswith("class:"))
            class_rarity = sum(
                1.0 / (feature_frequency[feature] * (1 + selected_feature_counts[feature]))
                for feature in candidate["features"] if feature.startswith("class:"))
            ranked.append((-(balance + class_rarity), candidate["sha256"], candidate["name"], candidate))
        _, _, _, chosen = min(ranked)
        selected.append((chosen, ["stratified-fill"]))
        selected_names.add(chosen["name"])
        selected_feature_counts.update(chosen["features"])

    selected_candidates = [candidate for candidate, _ in selected]
    selected_image_ids = {item["image"]["id"] for item in selected_candidates}
    selected_names_for_split = {item["name"] for item in selected_candidates}

    pilot_manifest = copy.deepcopy(manifest)
    canonical_split = {
        item["file_name"]: ("pilot" if item["file_name"] in selected_names_for_split else "unassigned")
        for item in pilot_manifest["images"] if item.get("canonical") is True
    }
    for item in pilot_manifest["images"]:
        item["split"] = canonical_split[item["canonical_file_name"]]
    pilot_manifest["split_policy"] = (
        "P5-02 deterministic pilot selection; duplicate aliases inherit canonical split")
    pilot_manifest["pilot_selection"] = {
        "algorithm": "p5-pilot-greedy-cover-v1",
        "size": size,
        "source_manifest_sha256": None,
        "source_preannotations_sha256": None,
    }

    pilot_images = []
    review_rows = []
    unconfirmed_ids = {
        item["id"] for item in CLASS_DEFINITIONS
        if item["mappingStatus"].startswith("unconfirmed")
    }
    reasons_by_name = {candidate["name"]: reasons for candidate, reasons in selected}
    for candidate in sorted(selected_candidates, key=lambda item: item["name"].lower()):
        source_image = candidate["image"]
        manifest_record = canonical[candidate["name"]]
        output_image = copy.deepcopy(source_image)
        output_image["split"] = "pilot"
        output_image["predicted_decision"] = source_image["cigarette_decision"]
        output_image["review_required"] = True
        has_unconfirmed = bool(candidate["category_ids"] & unconfirmed_ids)
        if has_unconfirmed:
            output_image["cigarette_decision"] = "REVIEW"
            output_image["review_reason"] = "contains-unconfirmed-class"
        pilot_images.append(output_image)
        review_rows.append({
            "file_name": candidate["name"],
            "sha256": candidate["sha256"],
            "source_group": manifest_record.get("source_group"),
            "dimensions": f'{manifest_record.get("width")}x{manifest_record.get("height")}',
            "predicted_decision": source_image.get("cigarette_decision"),
            "predicted_class_ids": ";".join(str(item) for item in sorted(candidate["category_ids"])),
            "predicted_box_count": len(candidate["annotations"]),
            "requires_business_mapping_review": str(has_unconfirmed).lower(),
            "selection_reasons": ";".join(reasons_by_name[candidate["name"]]),
            "human_decision": "",
            "human_class_ids": "",
            "annotator": "",
            "reviewer": "",
            "review_status": "pending",
            "notes": "",
        })

    pilot_coco = {
        "info": copy.deepcopy(preannotations["info"]),
        "images": pilot_images,
        "annotations": [
            copy.deepcopy(item) for item in preannotations["annotations"]
            if item["image_id"] in selected_image_ids
        ],
        "categories": copy.deepcopy(preannotations["categories"]),
    }
    pilot_coco["info"].update({
        "description": "P5-02 pilot review package; predictions only, not ground truth",
        "pilot_selection_algorithm": "p5-pilot-greedy-cover-v1",
        "ground_truth_complete": False,
        "accuracy_metrics_claimed": False,
    })

    selected_counts = Counter(
        feature for candidate in selected_candidates for feature in candidate["features"])
    selection = {
        "schema_version": "p5-pilot-selection-v1",
        "algorithm": "p5-pilot-greedy-cover-v1",
        "deterministic_tie_break": "sha256-then-file-name",
        "requested_size": size,
        "selected_count": len(selected_candidates),
        "canonical_only": True,
        "ground_truth_available": False,
        "accuracy_metrics_claimed": False,
        "required_features": sorted(required_features),
        "uncovered_features": sorted(required_features - set(selected_counts)),
        "available_feature_counts": dict(sorted(feature_frequency.items())),
        "selected_feature_counts": dict(sorted(selected_counts.items())),
        "selected": [
            {
                "file_name": candidate["name"],
                "sha256": candidate["sha256"],
                "features": sorted(candidate["features"]),
                "selection_reasons": reasons,
            }
            for candidate, reasons in selected
        ],
    }
    return selection, pilot_manifest, pilot_coco, review_rows


def verify_pilot_previews(pilot: dict, p4_results: Path) -> dict:
    """Bind P4 JSON/preview files to the exact predictions in a pilot package."""
    if not p4_results.is_dir():
        raise DatasetError(f"P4 result directory does not exist: {p4_results}")
    p4_by_source: dict[str, tuple[Path, dict, Path]] = {}
    for result_path in sorted(p4_results.glob("frame-*.json")):
        result = read_json(result_path)
        source = result.get("sourceFile")
        if not isinstance(source, str) or not source:
            raise DatasetError(f"P4 result has no sourceFile: {result_path}")
        if source in p4_by_source:
            raise DatasetError(f"duplicate P4 result sourceFile: {source}")
        preview_path = result_path.with_name(result_path.stem + "-annotated.png")
        if not preview_path.is_file():
            raise DatasetError(f"P4 annotated preview is missing: {preview_path}")
        p4_by_source[source] = (result_path, result, preview_path)

    pilot_images = pilot.get("images")
    pilot_annotations = pilot.get("annotations")
    if not isinstance(pilot_images, list) or not isinstance(pilot_annotations, list):
        raise DatasetError("pilot images and annotations must be lists")
    annotations_by_image: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for annotation in pilot_annotations:
        if not isinstance(annotation, dict):
            raise DatasetError("every pilot annotation must be an object")
        annotations_by_image[annotation.get("image_id")].append(annotation)
    expected_detector_versions = {
        annotation.get("detector_version") for annotation in pilot_annotations
        if isinstance(annotation.get("detector_version"), str)
        and annotation.get("detector_version")
    }
    if len(expected_detector_versions) != 1:
        raise DatasetError(
            "pilot annotations must bind exactly one non-empty detector_version")
    expected_detector_version = next(iter(expected_detector_versions))
    records = []
    errors = []
    for image in pilot_images:
        if not isinstance(image, dict):
            errors.append("every pilot image must be an object")
            continue
        name = image.get("file_name")
        source = p4_by_source.get(name)
        if source is None:
            errors.append(f"pilot image has no P4 result: {name}")
            continue
        result_path, result, preview_path = source
        width, height = image.get("width"), image.get("height")
        if (not isinstance(width, int) or isinstance(width, bool) or width <= 0
                or not isinstance(height, int) or isinstance(height, bool) or height <= 0):
            errors.append(f"pilot dimensions are invalid: {name}")
            continue
        if result.get("width") != width or result.get("height") != height:
            errors.append(f"P4 dimensions differ from pilot: {name}")
        if result.get("decision") != image.get("predicted_decision"):
            errors.append(f"P4 decision differs from pilot prediction: {name}")
        if result.get("parameterVersion") != expected_detector_version:
            errors.append(f"P4 parameterVersion differs from pilot provenance: {name}")
        defects = result.get("defects")
        candidate_annotations = sorted(
            annotations_by_image[image.get("id")], key=lambda item: item.get("id", -1))
        if not isinstance(defects, list) or len(defects) != len(candidate_annotations):
            errors.append(f"P4 defect count differs from pilot: {name}")
            continue
        for index, (defect, annotation) in enumerate(zip(defects, candidate_annotations), 1):
            if not isinstance(defect, dict):
                errors.append(f"P4 defect {index} is not an object: {name}")
                continue
            box = defect.get("box", {})
            if not isinstance(box, dict):
                errors.append(f"P4 defect {index} box is not an object: {name}")
                continue
            p4_bbox, _ = normalize_preannotation_bbox(
                [box.get("x"), box.get("y"), box.get("width"), box.get("height")],
                width, height)
            pilot_bbox = annotation.get("bbox")
            bbox_equal = (
                isinstance(p4_bbox, list) and len(p4_bbox) == 4
                and all(isinstance(value, (int, float)) and not isinstance(value, bool)
                        and math.isfinite(value) for value in p4_bbox)
                and isinstance(pilot_bbox, list) and len(pilot_bbox) == 4
                and all(isinstance(value, (int, float)) and not isinstance(value, bool)
                        and math.isfinite(value) for value in pilot_bbox)
                and all(math.isclose(float(left), float(right), rel_tol=1e-12, abs_tol=1e-9)
                        for left, right in zip(p4_bbox, pilot_bbox)))
            score = annotation.get("score")
            confidence = defect.get("confidence") if isinstance(defect, dict) else None
            score_equal = (
                isinstance(score, (int, float)) and not isinstance(score, bool)
                and math.isfinite(score)
                and isinstance(confidence, (int, float)) and not isinstance(confidence, bool)
                and math.isfinite(confidence)
                and math.isclose(float(score), float(confidence), rel_tol=1e-12, abs_tol=1e-12))
            class_id = defect.get("classId")
            pilot_category_id = annotation.get("category_id")
            class_equal = (
                isinstance(class_id, int) and not isinstance(class_id, bool)
                and isinstance(pilot_category_id, int) and not isinstance(pilot_category_id, bool)
                and class_id == pilot_category_id)
            detector_version = defect.get("detectorVersion")
            detector_equal = (
                isinstance(detector_version, str) and bool(detector_version)
                and detector_version == annotation.get("detector_version"))
            if (not class_equal or not detector_equal
                    or not bbox_equal or not score_equal):
                errors.append(f"P4 defect {index} differs from pilot annotation: {name}")
        records.append({
            "file_name": name,
            "source_image_sha256": image.get("sha256"),
            "frame_json_path": str(result_path.resolve()),
            "frame_json_sha256": sha256_file(result_path),
            "preview_path": str(preview_path.resolve()),
            "preview_sha256": sha256_file(preview_path),
            "detector_version": result.get("parameterVersion"),
            "defect_count": len(defects),
        })
    return {
        "schema_version": "p5-pilot-preview-provenance-v1",
        "valid": not errors,
        "pilot_image_count": len(pilot_images),
        "verified_record_count": len(records),
        "expected_detector_version": expected_detector_version,
        "errors": errors,
        "records": records,
    }


def write_review_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0]) if rows else []
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        with temporary.open("w", encoding="utf-8-sig", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        try:
            temporary.unlink()
        except FileNotFoundError:
            pass


def _validate_categories(data: dict) -> list[str]:
    errors = []
    categories = data.get("categories")
    expected = {index: name for index, name in enumerate(CLASS_NAMES)}
    if not isinstance(categories, list):
        return ["categories must be a list"]
    actual = {}
    for category in categories:
        if (not isinstance(category, dict) or not isinstance(category.get("id"), int)
                or isinstance(category.get("id"), bool)):
            errors.append("every category must have an integer id")
            continue
        actual[category["id"]] = category.get("name")
        class_id = category["id"]
        if 0 <= class_id < len(CLASS_DEFINITIONS):
            definition = CLASS_DEFINITIONS[class_id]
            if category.get("mapping_status") != definition["mappingStatus"]:
                errors.append(f"category {class_id} mapping_status does not match class catalog")
            if category.get("display_name_zh") != definition.get("displayNameZh", definition["name"]):
                errors.append(f"category {class_id} display_name_zh does not match class catalog")
    if actual != expected:
        errors.append(f"categories must exactly match {expected}")
    if data.get("info", {}).get("class_catalog_sha256") != CLASS_CATALOG_SHA256:
        errors.append("info.class_catalog_sha256 does not match the configured class catalog")
    return errors


def validate_annotations(
        data: dict, manifest: dict, require_reviewed: bool = False,
        required_split: str | None = None) -> list[str]:
    errors = _validate_categories(data)
    canonical = canonical_map(manifest)
    if required_split is not None and required_split not in VALID_SPLITS - {"unassigned"}:
        errors.append(f"invalid required split: {required_split!r}")
    expected_files = {
        name for name, record in canonical.items()
        if required_split is None or record.get("split") == required_split
    }
    if require_reviewed and required_split is not None and not expected_files:
        errors.append(f"manifest contains no canonical images in split {required_split!r}")
    images = data.get("images")
    annotations = data.get("annotations")
    if not isinstance(images, list):
        errors.append("images must be a list")
        images = []
    if not isinstance(annotations, list):
        errors.append("annotations must be a list")
        annotations = []

    image_ids = {}
    seen_files = set()
    for image in images:
        if not isinstance(image, dict):
            errors.append("every image must be an object")
            continue
        image_id = image.get("id")
        name = image.get("file_name")
        if (not isinstance(image_id, int) or isinstance(image_id, bool)
                or image_id in image_ids):
            errors.append(f"image id must be a unique integer: {image_id!r}")
            continue
        if name not in canonical:
            errors.append(f"image is not a canonical manifest file: {name!r}")
        elif name in seen_files:
            errors.append(f"canonical image appears more than once: {name}")
        else:
            record = canonical[name]
            if image.get("width") != record.get("width") or image.get("height") != record.get("height"):
                errors.append(f"image dimensions do not match manifest: {name}")
            if image.get("sha256") != record.get("sha256"):
                errors.append(f"image SHA-256 does not match manifest: {name}")
            if image.get("source_group") != record.get("source_group"):
                errors.append(f"image source_group does not match manifest: {name}")
            split = image.get("split")
            if split not in VALID_SPLITS:
                errors.append(f"invalid image split for {name}: {split!r}")
            if split != record.get("split"):
                errors.append(f"image split does not match manifest: {name}")
            if image.get("authorization_status") != record.get("authorization_status"):
                errors.append(f"image authorization_status does not match manifest: {name}")
            if required_split is not None and split != required_split:
                errors.append(f"image is outside required split {required_split!r}: {name}")
            seen_files.add(name)
        status = image.get("annotation_status")
        if status not in VALID_STATUSES:
            errors.append(f"invalid image annotation_status for {name}: {status!r}")
        decision = image.get("cigarette_decision")
        if decision not in VALID_DECISIONS and not (status == "unlabeled" and decision is None):
            errors.append(f"invalid image decision for {name}: {decision!r}")
        is_gt = image.get("is_ground_truth")
        if not isinstance(is_gt, bool):
            errors.append(f"image is_ground_truth must be boolean: {name}")
        elif is_gt != (status == "reviewed"):
            errors.append(f"image ground-truth flag does not match annotation_status: {name}")
        if require_reviewed and (status != "reviewed" or is_gt is not True or decision not in VALID_DECISIONS):
            errors.append(f"ground truth image is not fully reviewed: {name}")
        if image.get("authorization_status") not in VALID_AUTHORIZATION_STATUSES:
            errors.append(f"invalid image authorization_status for {name}: {image.get('authorization_status')!r}")
        if require_reviewed and image.get("authorization_status") != "approved":
            errors.append(f"ground truth image is not authorized for evaluation: {name}")
        if require_reviewed:
            annotated_by = image.get("annotated_by")
            reviewed_by = image.get("reviewed_by")
            if (not isinstance(annotated_by, str) or not annotated_by.strip()
                    or not isinstance(reviewed_by, str) or not reviewed_by.strip()
                    or annotated_by == reviewed_by):
                errors.append(f"ground truth image requires distinct annotator and reviewer: {name}")
            if "p4_result_present" in image or image.get("source") == "prediction":
                errors.append(f"ground truth image retains prediction provenance: {name}")
        image_ids[image_id] = image

    if require_reviewed and seen_files != expected_files:
        missing = sorted(expected_files - seen_files)
        extra = sorted(seen_files - expected_files)
        if missing:
            errors.append(f"ground truth is incomplete; missing canonical images: {missing}")
        if extra:
            errors.append(f"ground truth contains images outside the evaluation split: {extra}")

    annotation_ids = set()
    annotations_by_image = Counter()
    for annotation in annotations:
        if not isinstance(annotation, dict):
            errors.append("every annotation must be an object")
            continue
        annotation_id = annotation.get("id")
        image_id = annotation.get("image_id")
        category_id = annotation.get("category_id")
        if (not isinstance(annotation_id, int) or isinstance(annotation_id, bool)
                or annotation_id in annotation_ids):
            errors.append(f"annotation id must be a unique integer: {annotation_id!r}")
        else:
            annotation_ids.add(annotation_id)
        if not isinstance(image_id, int) or isinstance(image_id, bool):
            errors.append(f"annotation image_id must be an integer: {image_id!r}")
            continue
        if image_id not in image_ids:
            errors.append(f"annotation references unknown image_id: {image_id!r}")
            continue
        if (not isinstance(category_id, int) or isinstance(category_id, bool)
                or not 0 <= category_id < len(CLASS_NAMES)):
            errors.append(f"annotation has invalid category_id: {category_id!r}")
        bbox = annotation.get("bbox")
        if (not isinstance(bbox, list) or len(bbox) != 4 or
                any(not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value) for value in bbox)):
            errors.append(f"annotation {annotation_id!r} has invalid bbox")
        else:
            x, y, width, height = bbox
            image = image_ids[image_id]
            if x < 0 or y < 0 or width <= 0 or height <= 0 or x + width > image.get("width", 0) or y + height > image.get("height", 0):
                errors.append(f"annotation {annotation_id!r} bbox is outside image bounds")
            area = annotation.get("area")
            if (not isinstance(area, (int, float)) or isinstance(area, bool)
                    or not math.isfinite(area) or not math.isclose(
                        float(area), float(width * height), rel_tol=1e-9, abs_tol=1e-6)):
                errors.append(f"annotation {annotation_id!r} area does not match bbox")
        status = annotation.get("annotation_status")
        if status not in VALID_STATUSES:
            errors.append(f"annotation {annotation_id!r} has invalid annotation_status")
        annotation_is_gt = annotation.get("is_ground_truth")
        if not isinstance(annotation_is_gt, bool):
            errors.append(f"annotation {annotation_id!r} is_ground_truth must be boolean")
        elif annotation_is_gt != (status == "reviewed"):
            errors.append(
                f"annotation {annotation_id!r} ground-truth flag does not match annotation_status")
        if require_reviewed and (status != "reviewed" or annotation.get("is_ground_truth") is not True):
            errors.append(f"ground truth annotation is not reviewed: {annotation_id!r}")
        if require_reviewed:
            prediction_fields = {
                "score", "detector_version", "source_bbox", "boundary_clamped"
            }
            if annotation.get("source") != "human" or prediction_fields.intersection(annotation):
                errors.append(f"ground truth annotation retains prediction provenance: {annotation_id!r}")
        if (require_reviewed and isinstance(category_id, int) and not isinstance(category_id, bool)
                and 0 <= category_id < len(CLASS_DEFINITIONS)
                and CLASS_DEFINITIONS[category_id]["mappingStatus"].startswith("unconfirmed")):
            errors.append(
                f"ground truth annotation uses an unconfirmed class: {annotation_id!r}"
            )
        annotations_by_image[image_id] += 1

    for image_id, image in image_ids.items():
        if image.get("cigarette_decision") == "OK" and annotations_by_image[image_id]:
            errors.append(f"OK image must not contain defect annotations: {image.get('file_name')}")
        if (image.get("cigarette_decision") == "NG"
                and image.get("annotation_status") != "unlabeled"
                and not annotations_by_image[image_id]):
            errors.append(f"NG image must contain at least one defect annotation: {image.get('file_name')}")
        if (require_reviewed and image.get("cigarette_decision") == "REVIEW"
                and annotations_by_image[image_id]):
            errors.append(f"REVIEW image must not contain ground-truth annotations: {image.get('file_name')}")
    return errors


def bbox_iou(left: list[float], right: list[float]) -> float:
    lx, ly, lw, lh = left
    rx, ry, rw, rh = right
    x1, y1 = max(lx, rx), max(ly, ry)
    x2, y2 = min(lx + lw, rx + rw), min(ly + lh, ry + rh)
    intersection = max(0.0, x2 - x1) * max(0.0, y2 - y1)
    union = lw * lh + rw * rh - intersection
    return intersection / union if union > 0 else 0.0


def _ratio(numerator: int, denominator: int) -> float:
    return numerator / denominator if denominator else 0.0


REQUIRED_EVALUATION_BINDINGS = {
    "ground_truth", "predictions", "manifest", "class_catalog",
    "model", "engine", "detector_config", "attestation",
}
FALLBACK_EVALUATION_BINDINGS = (REQUIRED_EVALUATION_BINDINGS - {"engine"}) | {"runtime_contract"}


def validate_evaluation_provenance(
        attestation: dict, bindings: dict[str, dict[str, str]], split: str) -> list[str]:
    errors = []
    binding_names = set(bindings)
    if binding_names not in {frozenset(REQUIRED_EVALUATION_BINDINGS),
                              frozenset(FALLBACK_EVALUATION_BINDINGS)}:
        errors.append(
            "evaluation bindings must exactly describe either a TensorRT engine or "
            "a fallback runtime contract"
        )
    for name, binding in bindings.items():
        if not isinstance(binding, dict) or not isinstance(binding.get("path"), str):
            errors.append(f"evaluation binding {name!r} must contain a path")
            continue
        digest = binding.get("sha256")
        if (not isinstance(digest, str) or len(digest) != 64
                or any(character not in "0123456789abcdefABCDEF" for character in digest)):
            errors.append(f"evaluation binding {name!r} has an invalid SHA-256")
    if not isinstance(attestation, dict):
        return errors + ["ground-truth attestation must be an object"]
    if attestation.get("schema_version") != "p5-ground-truth-attestation-v1":
        errors.append("ground-truth attestation schema_version is invalid")
    if attestation.get("review_method") != "human-double-review":
        errors.append("ground-truth attestation must record human-double-review")
    if attestation.get("authorization_status") != "approved":
        errors.append("ground-truth attestation authorization_status must be approved")
    if attestation.get("evaluation_split") != split:
        errors.append(f"ground-truth attestation evaluation_split must be {split!r}")
    annotated_by = attestation.get("annotated_by")
    reviewed_by = attestation.get("reviewed_by")
    if (not isinstance(annotated_by, str) or not annotated_by.strip()
            or not isinstance(reviewed_by, str) or not reviewed_by.strip()
            or annotated_by == reviewed_by):
        errors.append("ground-truth attestation requires distinct annotator and reviewer")
    try:
        dt.datetime.fromisoformat(str(attestation.get("reviewed_at")))
    except ValueError:
        errors.append("ground-truth attestation reviewed_at must be an ISO timestamp")
    expected_attested_hashes = {
        "ground_truth_sha256": "ground_truth",
        "manifest_sha256": "manifest",
        "class_catalog_sha256": "class_catalog",
    }
    for field, binding_name in expected_attested_hashes.items():
        if (binding_name not in bindings
                or attestation.get(field) != bindings[binding_name].get("sha256")):
            errors.append(f"ground-truth attestation {field} does not match evidence binding")
    return errors


def validate_fallback_runtime_contract(
        contract: dict, detector_config: dict,
        bindings: dict[str, dict[str, str]]) -> list[str]:
    errors = []
    if not isinstance(contract, dict):
        return ["fallback runtime contract must be an object"]
    if contract.get("schema_version") != "p5-fallback-runtime-contract-v1":
        errors.append("fallback runtime contract schema_version is invalid")
    if contract.get("formal_p4_tensorrt_evidence") is not False:
        errors.append("fallback runtime contract must set formal_p4_tensorrt_evidence to false")
    if contract.get("backend") != "ONNX Runtime":
        errors.append("fallback runtime contract backend must be ONNX Runtime")
    if contract.get("provider") != "CPUExecutionProvider":
        errors.append("fallback runtime contract provider must be CPUExecutionProvider")
    if not isinstance(contract.get("runtime_version"), str) or not contract["runtime_version"].strip():
        errors.append("fallback runtime contract runtime_version must be non-empty")
    scope = contract.get("accuracy_scope")
    if (not isinstance(scope, str) or "provisional" not in scope.lower()
            or "fallback" not in scope.lower()):
        errors.append("fallback runtime contract accuracy_scope must state provisional fallback")
    for field, binding_name in (("model_sha256", "model"),
                                ("predictions_sha256", "predictions")):
        if contract.get(field) != bindings.get(binding_name, {}).get("sha256"):
            errors.append(f"fallback runtime contract {field} does not match evidence binding")
    source_path = contract.get("source_runtime_manifest_path")
    source_hash = contract.get("source_runtime_manifest_sha256")
    if not isinstance(source_path, str) or not Path(source_path).is_file():
        errors.append("fallback source runtime manifest does not exist")
    elif source_hash != sha256_file(Path(source_path)):
        errors.append("fallback source runtime manifest SHA-256 does not match")
    if not isinstance(detector_config, dict):
        errors.append("fallback detector config must be an object")
    else:
        if detector_config.get("schema_version") != "p5-detector-config-v1":
            errors.append("fallback detector config schema_version is invalid")
        if detector_config.get("formal_p4_tensorrt_evidence") is not False:
            errors.append("fallback detector config must set formal_p4_tensorrt_evidence to false")
        backend = detector_config.get("inference_backend")
        if (not isinstance(backend, str) or contract.get("backend") not in backend
                or "fallback" not in backend.lower()):
            errors.append("fallback detector config backend does not match runtime contract")
    return errors


def greedy_matches(
        ground_truth: list[dict], predictions: list[dict],
        iou_threshold: float) -> tuple[list[tuple[float, int, int]], set[int], set[int]]:
    candidates = []
    for gt_index, gt_item in enumerate(ground_truth):
        for pred_index, pred_item in enumerate(predictions):
            overlap = bbox_iou(gt_item["bbox"], pred_item["bbox"])
            if overlap >= iou_threshold:
                candidates.append((overlap, gt_index, pred_index))
    matched_gt: set[int] = set()
    matched_pred: set[int] = set()
    matches = []
    for overlap, gt_index, pred_index in sorted(
            candidates, key=lambda item: (-item[0], item[1], item[2])):
        if gt_index not in matched_gt and pred_index not in matched_pred:
            matched_gt.add(gt_index)
            matched_pred.add(pred_index)
            matches.append((overlap, gt_index, pred_index))
    return matches, matched_gt, matched_pred


def evaluate(
        ground_truth: dict, predictions: dict, manifest: dict,
        iou_threshold: float, attestation: dict,
        bindings: dict[str, dict[str, str]], split: str = "test",
        runtime_contract: dict | None = None,
        detector_config: dict | None = None) -> dict:
    if not 0 < iou_threshold <= 1:
        raise DatasetError("IoU threshold must be in (0, 1]")
    provenance_errors = validate_evaluation_provenance(attestation, bindings, split)
    if provenance_errors:
        raise DatasetError(
            "accuracy evaluation refused because provenance is invalid:\n- "
            + "\n- ".join(provenance_errors)
        )
    if "runtime_contract" in bindings:
        fallback_errors = validate_fallback_runtime_contract(
            runtime_contract, detector_config, bindings)
        if fallback_errors:
            raise DatasetError(
                "accuracy evaluation refused because fallback identity is invalid:\n- "
                + "\n- ".join(fallback_errors)
            )
    gt_errors = validate_annotations(
        ground_truth, manifest, require_reviewed=True, required_split=split)
    if ground_truth.get("info", {}).get("ground_truth_complete") is not True:
        gt_errors.append("ground truth info.ground_truth_complete must be true")
    if ground_truth.get("info", {}).get("evaluation_split") != split:
        gt_errors.append(f"ground truth info.evaluation_split must be {split!r}")
    if gt_errors:
        raise DatasetError("accuracy evaluation refused because ground truth is incomplete or invalid:\n- " + "\n- ".join(gt_errors))
    prediction_errors = validate_annotations(predictions, manifest, require_reviewed=False)
    if prediction_errors:
        raise DatasetError("predictions are invalid:\n- " + "\n- ".join(prediction_errors))

    review_excluded = [
        image["file_name"] for image in ground_truth["images"]
        if image["cigarette_decision"] == "REVIEW"
    ]
    gt_images = {
        image["file_name"]: image for image in ground_truth["images"]
        if image["cigarette_decision"] in {"OK", "NG"}
    }
    if not gt_images:
        raise DatasetError("accuracy evaluation refused because the split has no OK/NG ground truth")
    pred_images = {image["file_name"]: image for image in predictions["images"]}
    gt_by_image = defaultdict(list)
    pred_by_image = defaultdict(list)
    gt_id_to_name = {image["id"]: image["file_name"] for image in ground_truth["images"]}
    pred_id_to_name = {image["id"]: image["file_name"] for image in predictions["images"]}
    for annotation in ground_truth["annotations"]:
        gt_by_image[gt_id_to_name[annotation["image_id"]]].append(annotation)
    for annotation in predictions["annotations"]:
        pred_by_image[pred_id_to_name[annotation["image_id"]]].append(annotation)

    per_class = {
        index: {"tp": 0, "fp": 0, "fn": 0, "support": 0}
        for index in range(len(CLASS_NAMES))
    }
    background_id = len(CLASS_NAMES)
    confusion_names = list(CLASS_NAMES) + ["__background__"]
    confusion = [[0 for _ in confusion_names] for _ in confusion_names]
    errors = []
    missed_ng = false_ng = 0
    for name in sorted(gt_images, key=str.lower):
        gt_items = gt_by_image[name]
        pred_items = pred_by_image[name]

        class_matched_gt: set[int] = set()
        class_matched_pred: set[int] = set()
        for class_id in range(len(CLASS_NAMES)):
            class_gt_indices = [
                index for index, item in enumerate(gt_items)
                if item["category_id"] == class_id
            ]
            class_pred_indices = [
                index for index, item in enumerate(pred_items)
                if item["category_id"] == class_id
            ]
            class_gt = [gt_items[index] for index in class_gt_indices]
            class_pred = [pred_items[index] for index in class_pred_indices]
            matches, matched_gt_local, matched_pred_local = greedy_matches(
                class_gt, class_pred, iou_threshold)
            per_class[class_id]["tp"] += len(matches)
            per_class[class_id]["fn"] += len(class_gt) - len(matched_gt_local)
            per_class[class_id]["fp"] += len(class_pred) - len(matched_pred_local)
            per_class[class_id]["support"] += len(class_gt)
            for _, gt_local, pred_local in matches:
                gt_index = class_gt_indices[gt_local]
                pred_index = class_pred_indices[pred_local]
                class_matched_gt.add(gt_index)
                class_matched_pred.add(pred_index)
                confusion[class_id][class_id] += 1

        image_errors = []
        remaining_gt = [index for index in range(len(gt_items)) if index not in class_matched_gt]
        remaining_pred = [index for index in range(len(pred_items)) if index not in class_matched_pred]
        wrong_class_candidates = []
        for gt_local, gt_index in enumerate(remaining_gt):
            for pred_local, pred_index in enumerate(remaining_pred):
                if gt_items[gt_index]["category_id"] == pred_items[pred_index]["category_id"]:
                    continue
                overlap = bbox_iou(gt_items[gt_index]["bbox"], pred_items[pred_index]["bbox"])
                if overlap >= iou_threshold:
                    wrong_class_candidates.append((overlap, gt_local, pred_local))
        wrong_matched_gt: set[int] = set()
        wrong_matched_pred: set[int] = set()
        wrong_matches = []
        for overlap, gt_local, pred_local in sorted(
                wrong_class_candidates, key=lambda item: (-item[0], item[1], item[2])):
            if gt_local not in wrong_matched_gt and pred_local not in wrong_matched_pred:
                wrong_matched_gt.add(gt_local)
                wrong_matched_pred.add(pred_local)
                wrong_matches.append((overlap, remaining_gt[gt_local], remaining_pred[pred_local]))

        for overlap, gt_index, pred_index in wrong_matches:
            gt_item = gt_items[gt_index]
            pred_item = pred_items[pred_index]
            gt_class = gt_item["category_id"]
            pred_class = pred_item["category_id"]
            confusion[gt_class][pred_class] += 1
            image_errors.append({"type": "wrong_class", "iou": overlap,
                                 "ground_truth_class_id": gt_class, "predicted_class_id": pred_class})
        matched_gt = class_matched_gt.union(gt_index for _, gt_index, _ in wrong_matches)
        matched_pred = class_matched_pred.union(pred_index for _, _, pred_index in wrong_matches)
        for index, item in enumerate(gt_items):
            if index not in matched_gt:
                confusion[item["category_id"]][background_id] += 1
                image_errors.append({"type": "false_negative", "ground_truth_class_id": item["category_id"], "bbox": item["bbox"]})
        for index, item in enumerate(pred_items):
            if index not in matched_pred:
                confusion[background_id][item["category_id"]] += 1
                image_errors.append({"type": "false_positive", "predicted_class_id": item["category_id"], "bbox": item["bbox"], "score": item.get("score")})

        gt_decision = gt_images[name]["cigarette_decision"]
        predicted_decision = "NG" if pred_items else "OK"
        if gt_decision == "NG" and predicted_decision == "OK":
            missed_ng += 1
            image_errors.append({"type": "missed_ng"})
        if gt_decision == "OK" and predicted_decision == "NG":
            false_ng += 1
            image_errors.append({"type": "false_ng"})
        if image_errors:
            errors.append({"file_name": name, "ground_truth_decision": gt_decision,
                           "predicted_decision": predicted_decision, "errors": image_errors})

    class_rows = []
    for class_id, name in enumerate(CLASS_NAMES):
        counts = per_class[class_id]
        precision = _ratio(counts["tp"], counts["tp"] + counts["fp"])
        recall = _ratio(counts["tp"], counts["tp"] + counts["fn"])
        f1 = _ratio(2 * precision * recall, precision + recall)
        class_rows.append({"class_id": class_id, "class_name": name, **counts,
                           "precision": precision, "recall": recall, "f1": f1})
    total_tp = sum(row["tp"] for row in class_rows)
    total_fp = sum(row["fp"] for row in class_rows)
    total_fn = sum(row["fn"] for row in class_rows)
    micro_precision = _ratio(total_tp, total_tp + total_fp)
    micro_recall = _ratio(total_tp, total_tp + total_fn)
    micro_f1 = _ratio(2 * micro_precision * micro_recall, micro_precision + micro_recall)
    present_class_rows = [row for row in class_rows if row["support"] > 0]
    active_union_rows = [
        row for row in class_rows if row["support"] > 0 or row["fp"] > 0
    ]
    macro_present = {
        metric: (_ratio(sum(row[metric] for row in present_class_rows), len(present_class_rows)))
        for metric in ("precision", "recall", "f1")
    }
    macro_all = {
        metric: _ratio(sum(row[metric] for row in class_rows), len(class_rows))
        for metric in ("precision", "recall", "f1")
    }
    macro_active_union = {
        metric: _ratio(sum(row[metric] for row in active_union_rows), len(active_union_rows))
        for metric in ("precision", "recall", "f1")
    }
    gt_ng = sum(image["cigarette_decision"] == "NG" for image in gt_images.values())
    gt_ok = len(gt_images) - gt_ng
    return {
        "accuracy_metrics_claimed": True,
        "baseline_classification": (
            "provisional ONNX Runtime CPU fallback; not formal P4 TensorRT evidence"
            if "runtime_contract" in bindings else "formal TensorRT evaluation"),
        "formal_p4_tensorrt_evidence": "engine" in bindings,
        "runtime_identity": ({
            "backend": runtime_contract["backend"],
            "provider": runtime_contract["provider"],
            "runtime_version": runtime_contract["runtime_version"],
        } if "runtime_contract" in bindings else {"backend": "TensorRT"}),
        "ground_truth_complete": True,
        "evaluation_split": split,
        "review_excluded_images": review_excluded,
        "evidence_bindings": bindings,
        "ground_truth_attestation": attestation,
        "iou_threshold": iou_threshold,
        "image_count": len(gt_images),
        "per_class": class_rows,
        "micro": {"tp": total_tp, "fp": total_fp, "fn": total_fn,
                  "precision": micro_precision, "recall": micro_recall, "f1": micro_f1},
        "macro_ground_truth_present_classes": {
            "class_count": len(present_class_rows), **macro_present,
        },
        "macro_active_union_classes": {
            "class_count": len(active_union_rows), **macro_active_union,
        },
        "macro_all_classes": macro_all,
        "cigarette_level": {
            "ground_truth_ng": gt_ng,
            "ground_truth_ok": gt_ok,
            "missed_ng": missed_ng,
            "false_ng": false_ng,
            "missed_ng_rate": _ratio(missed_ng, gt_ng),
            "false_ng_rate": _ratio(false_ng, gt_ok),
        },
        "confusion_matrix": {
            "class_names": confusion_names,
            "rows_true_columns_predicted": confusion,
        },
        "error_images": errors,
    }


def command_audit(args: argparse.Namespace) -> None:
    configure_classes(args.class_catalog)
    manifest, summary, template = audit_dataset(args.images, args.legacy_predictions)
    catalog_hash = sha256_file(args.class_catalog)
    manifest["class_catalog"] = {
        "path": str(args.class_catalog.resolve()), "sha256": catalog_hash,
    }
    summary["class_catalog_sha256"] = catalog_hash
    template["info"]["class_catalog_sha256"] = catalog_hash
    write_json(args.output / "dataset-manifest.json", manifest)
    write_json(args.output / "dataset-summary.json", summary)
    write_json(args.output / "annotations-empty.coco.json", template)
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))


def command_preannotate(args: argparse.Namespace) -> None:
    configure_classes(args.class_catalog)
    value = preannotate(read_json(args.manifest), args.p4_results)
    value["info"]["class_catalog_sha256"] = sha256_file(args.class_catalog)
    errors = validate_annotations(value, read_json(args.manifest))
    if errors:
        raise DatasetError("generated preannotations are invalid:\n- " + "\n- ".join(errors))
    write_json(args.output, value)
    print(json.dumps({"images": len(value["images"]), "annotations": len(value["annotations"]),
                      "is_ground_truth": False}, sort_keys=True))


def command_select_pilot(args: argparse.Namespace) -> None:
    configure_classes(args.class_catalog)
    manifest = read_json(args.manifest)
    preannotations = read_json(args.preannotations)
    selection, pilot_manifest, pilot_coco, review_rows = select_pilot(
        manifest, preannotations, args.size)
    source_manifest_hash = sha256_file(args.manifest)
    source_preannotations_hash = sha256_file(args.preannotations)
    bindings = {
        "source_manifest": {"path": str(args.manifest.resolve()), "sha256": source_manifest_hash},
        "source_preannotations": {
            "path": str(args.preannotations.resolve()), "sha256": source_preannotations_hash,
        },
        "class_catalog": {
            "path": str(args.class_catalog.resolve()), "sha256": sha256_file(args.class_catalog),
        },
    }
    selection["evidence_bindings"] = bindings
    pilot_manifest["pilot_selection"].update({
        "source_manifest_sha256": source_manifest_hash,
        "source_preannotations_sha256": source_preannotations_hash,
    })
    pilot_coco["info"]["evidence_bindings"] = bindings
    errors = validate_annotations(
        pilot_coco, pilot_manifest, require_reviewed=False, required_split="pilot")
    if errors:
        raise DatasetError("generated pilot package is invalid:\n- " + "\n- ".join(errors))
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / "pilot-selection.json", selection)
    write_json(args.output / "pilot-manifest.json", pilot_manifest)
    write_json(args.output / "pilot-preannotations.coco.json", pilot_coco)
    write_review_csv(args.output / "pilot-review.csv", review_rows)
    print(json.dumps({
        "selected_count": selection["selected_count"],
        "uncovered_features": selection["uncovered_features"],
        "review_image_count": sum(
            item["cigarette_decision"] == "REVIEW" for item in pilot_coco["images"]),
        "is_ground_truth": False,
    }, sort_keys=True))


def command_verify_pilot_previews(args: argparse.Namespace) -> None:
    report = verify_pilot_previews(read_json(args.pilot), args.p4_results)
    write_json(args.output, report)
    print(json.dumps({
        "valid": report["valid"],
        "pilot_image_count": report["pilot_image_count"],
        "verified_record_count": report["verified_record_count"],
        "error_count": len(report["errors"]),
    }, sort_keys=True))
    if not report["valid"]:
        raise DatasetError("pilot preview provenance is invalid:\n- " + "\n- ".join(report["errors"]))


def command_validate(args: argparse.Namespace) -> None:
    configure_classes(args.class_catalog)
    errors = validate_annotations(
        read_json(args.annotations), read_json(args.manifest),
        args.require_reviewed, args.split)
    report = {"valid": not errors, "require_reviewed": args.require_reviewed, "errors": errors}
    if args.output:
        write_json(args.output, report)
    console_report = {
        "valid": not errors,
        "require_reviewed": args.require_reviewed,
        "error_count": len(errors),
        "first_errors": errors[:5],
        "errors_truncated": len(errors) > 5,
    }
    print(json.dumps(console_report, ensure_ascii=False, sort_keys=True))
    if errors:
        raise DatasetError(f"annotation validation failed with {len(errors)} error(s)")


def command_evaluate(args: argparse.Namespace) -> None:
    configure_classes(args.class_catalog)
    binding_paths = {
        "ground_truth": args.ground_truth,
        "predictions": args.predictions,
        "manifest": args.manifest,
        "class_catalog": args.class_catalog,
        "model": args.model,
        "detector_config": args.detector_config,
        "attestation": args.attestation,
    }
    if args.engine is not None:
        binding_paths["engine"] = args.engine
    else:
        binding_paths["runtime_contract"] = args.runtime_contract
    bindings = {}
    for name, path in binding_paths.items():
        if not path.is_file():
            raise DatasetError(f"evaluation binding file does not exist: {path}")
        bindings[name] = {"path": str(path.resolve()), "sha256": sha256_file(path)}
    report = evaluate(
        read_json(args.ground_truth), read_json(args.predictions),
        read_json(args.manifest), args.iou, read_json(args.attestation),
        bindings, args.split,
        read_json(args.runtime_contract) if args.runtime_contract else None,
        read_json(args.detector_config))
    write_json(args.output, report)
    print(json.dumps({"image_count": report["image_count"], "micro": report["micro"],
                      "cigarette_level": report["cigarette_level"]}, sort_keys=True))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    audit = subparsers.add_parser("audit", help="audit source images without modifying them")
    audit.add_argument("--images", type=Path, required=True)
    audit.add_argument("--legacy-predictions", type=Path)
    audit.add_argument("--output", type=Path, required=True)
    audit.add_argument("--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG)
    audit.set_defaults(function=command_audit)

    pre = subparsers.add_parser("preannotate", help="convert P4 frame JSON to non-GT COCO candidates")
    pre.add_argument("--manifest", type=Path, required=True)
    pre.add_argument("--p4-results", type=Path, required=True)
    pre.add_argument("--output", type=Path, required=True)
    pre.add_argument("--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG)
    pre.set_defaults(function=command_preannotate)

    pilot = subparsers.add_parser(
        "select-pilot", help="build a deterministic canonical-only pilot review package")
    pilot.add_argument("--manifest", type=Path, required=True)
    pilot.add_argument("--preannotations", type=Path, required=True)
    pilot.add_argument("--size", type=int, default=30)
    pilot.add_argument("--output", type=Path, required=True)
    pilot.add_argument("--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG)
    pilot.set_defaults(function=command_select_pilot)

    preview = subparsers.add_parser(
        "verify-pilot-previews", help="bind P4 JSON and previews to pilot predictions")
    preview.add_argument("--pilot", type=Path, required=True)
    preview.add_argument("--p4-results", type=Path, required=True)
    preview.add_argument("--output", type=Path, required=True)
    preview.set_defaults(function=command_verify_pilot_previews)

    validate = subparsers.add_parser("validate-annotations", help="validate P5 COCO annotations")
    validate.add_argument("--manifest", type=Path, required=True)
    validate.add_argument("--annotations", type=Path, required=True)
    validate.add_argument("--require-reviewed", action="store_true")
    validate.add_argument("--split", choices=sorted(VALID_SPLITS - {"unassigned"}))
    validate.add_argument("--output", type=Path)
    validate.add_argument("--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG)
    validate.set_defaults(function=command_validate)

    evaluation = subparsers.add_parser("evaluate", help="evaluate reviewed GT against predictions")
    evaluation.add_argument("--manifest", type=Path, required=True)
    evaluation.add_argument("--ground-truth", type=Path, required=True)
    evaluation.add_argument("--predictions", type=Path, required=True)
    evaluation.add_argument("--attestation", type=Path, required=True)
    evaluation.add_argument("--model", type=Path, required=True)
    runtime = evaluation.add_mutually_exclusive_group(required=True)
    runtime.add_argument("--engine", type=Path)
    runtime.add_argument("--runtime-contract", type=Path)
    evaluation.add_argument("--detector-config", type=Path, required=True)
    evaluation.add_argument("--iou", type=float, default=0.5)
    evaluation.add_argument("--split", choices=sorted(VALID_SPLITS - {"unassigned"}), default="test")
    evaluation.add_argument("--output", type=Path, required=True)
    evaluation.add_argument("--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG)
    evaluation.set_defaults(function=command_evaluate)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.function(args)
        return 0
    except DatasetError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
