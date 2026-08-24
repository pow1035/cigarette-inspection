#!/usr/bin/env python3
"""Build an event-aware, predictions-only P5 development review package.

This tool never changes the frozen pilot assignment. It groups canonical images
into provisional capture events, excludes unassigned images whose event also
contains pilot images, and assigns only the remaining whole events to train or
validation.
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
import shutil
import struct
import sys
import tempfile
from collections import Counter
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Any, Iterable


SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
TIMESTAMP_RE = re.compile(r"(?<!\d)(\d{17})(?!\d)")
VALID_SPLITS = {"pilot", "unassigned"}
VALID_DECISIONS = {"OK", "NG", "REVIEW"}
DEFAULT_EVENT_GAP_MS = 1000
DEFAULT_VALIDATION_FRACTION = 0.20
EXPECTED_CLASS_COUNT = 9
DEFAULT_CLASS_CATALOG = Path(__file__).resolve().parents[1] / "config" / "p5-class-catalog.json"


class DevelopmentSplitError(ValueError):
    """A user-correctable package input or destination error."""


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON number {value}")


def read_json_snapshot(path: Path) -> tuple[Any, dict[str, Any]]:
    try:
        payload = path.read_bytes()
        value = json.loads(
            payload.decode("utf-8-sig"), parse_constant=_reject_json_constant)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        raise DevelopmentSplitError(f"cannot read JSON {path}: {exc}") from exc
    return value, {
        "name": path.name,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "size": len(payload),
    }


def read_json(path: Path) -> Any:
    return read_json_snapshot(path)[0]


def require_unchanged_snapshot(path: Path, snapshot: dict[str, Any], label: str) -> None:
    try:
        payload = path.read_bytes()
    except OSError as exc:
        raise DevelopmentSplitError(f"{label} changed or became unreadable: {path}") from exc
    if (
        len(payload) != snapshot["size"]
        or hashlib.sha256(payload).hexdigest() != snapshot["sha256"]
    ):
        raise DevelopmentSplitError(f"{label} changed during package generation")


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, ensure_ascii=False, indent=2, sort_keys=False)
        handle.write("\n")
        handle.flush()
        os.fsync(handle.fileno())


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_size(path: Path) -> tuple[int, int, str]:
    with path.open("rb") as handle:
        signature = handle.read(24)
        if signature.startswith(b"\x89PNG\r\n\x1a\n"):
            if len(signature) < 24 or signature[12:16] != b"IHDR":
                raise DevelopmentSplitError(f"invalid PNG header: {path}")
            width, height = struct.unpack(">II", signature[16:24])
            return width, height, "PNG"
        if not signature.startswith(b"\xff\xd8"):
            raise DevelopmentSplitError(f"unsupported image format: {path}")
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
    raise DevelopmentSplitError(f"cannot determine image dimensions: {path}")


def _strict_positive_int(value: Any, field: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value <= 0:
        raise DevelopmentSplitError(f"{field} must be a positive integer")
    return value


def _safe_relative_parts(value: Any, field: str) -> tuple[str, ...]:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise DevelopmentSplitError(f"{field} must be a non-empty relative path")
    windows = PureWindowsPath(value)
    posix = PurePosixPath(value.replace("\\", "/"))
    if windows.is_absolute() or windows.drive or posix.is_absolute():
        raise DevelopmentSplitError(f"{field} must not be absolute: {value!r}")
    parts = tuple(value.replace("\\", "/").split("/"))
    if any(part in {"", ".", ".."} for part in parts):
        raise DevelopmentSplitError(
            f"{field} must be normalized and must not contain dot segments: {value!r}")
    return parts


def resolve_source_path(source_root: Path, relative_path: Any) -> Path:
    parts = _safe_relative_parts(relative_path, "manifest relative_path")
    try:
        candidate = source_root.joinpath(*parts).resolve(strict=True)
    except OSError as exc:
        raise DevelopmentSplitError(
            f"source image does not exist or cannot be resolved: {relative_path!r}: {exc}") from exc
    try:
        candidate.relative_to(source_root)
    except ValueError as exc:
        raise DevelopmentSplitError(
            f"source image escapes source root through symlink/reparse point: "
            f"{relative_path!r}") from exc
    if not candidate.is_file():
        raise DevelopmentSplitError(f"source image is not a regular file: {relative_path!r}")
    return candidate


def validate_manifest(manifest: Any) -> tuple[dict[str, dict], dict[str, dict]]:
    if not isinstance(manifest, dict) or not isinstance(manifest.get("images"), list):
        raise DevelopmentSplitError("pilot manifest must contain an images list")
    if manifest.get("schema_version") not in {None, "p5-dataset-manifest-v1"}:
        raise DevelopmentSplitError("unsupported pilot manifest schema_version")

    all_records: dict[str, dict] = {}
    canonical: dict[str, dict] = {}
    canonical_hashes: set[str] = set()
    relative_paths: set[str] = set()
    for index, record in enumerate(manifest["images"]):
        if not isinstance(record, dict):
            raise DevelopmentSplitError(f"manifest image {index} is not an object")
        name = record.get("file_name")
        if not isinstance(name, str) or not name or name in all_records:
            raise DevelopmentSplitError("manifest file_name values must be unique non-empty strings")
        _safe_relative_parts(record.get("relative_path"), f"relative_path for {name}")
        relative_key = record["relative_path"].replace("\\", "/").casefold()
        if relative_key in relative_paths:
            raise DevelopmentSplitError(
                f"manifest relative_path values must be case-insensitively unique: "
                f"{record['relative_path']}")
        relative_paths.add(relative_key)
        digest = record.get("sha256")
        if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
            raise DevelopmentSplitError(f"invalid lowercase SHA-256 for manifest image {name}")
        _strict_positive_int(record.get("width"), f"width for {name}")
        _strict_positive_int(record.get("height"), f"height for {name}")
        source_group = record.get("source_group")
        if not isinstance(source_group, str) or not source_group:
            raise DevelopmentSplitError(f"invalid source_group for manifest image {name}")
        if record.get("split") not in VALID_SPLITS:
            raise DevelopmentSplitError(
                f"manifest image {name} must have frozen pilot or unassigned split")
        if type(record.get("canonical")) is not bool:
            raise DevelopmentSplitError(f"canonical flag must be boolean for {name}")
        canonical_name = record.get("canonical_file_name")
        if not isinstance(canonical_name, str) or not canonical_name:
            raise DevelopmentSplitError(f"invalid canonical_file_name for {name}")
        all_records[name] = record
        if record["canonical"]:
            if canonical_name != name:
                raise DevelopmentSplitError(f"canonical image must reference itself: {name}")
            if digest in canonical_hashes:
                raise DevelopmentSplitError(
                    f"canonical SHA-256 values must be unique: {digest}")
            canonical_hashes.add(digest)
            canonical[name] = record

    if len(canonical) < 2:
        raise DevelopmentSplitError("manifest must contain at least two canonical images")
    for name, record in all_records.items():
        canonical_name = record["canonical_file_name"]
        if canonical_name not in canonical:
            raise DevelopmentSplitError(
                f"manifest image references unknown canonical image: {name}")
        owner = canonical[canonical_name]
        for field in ("sha256", "width", "height", "source_group", "split"):
            if record.get(field) != owner.get(field):
                raise DevelopmentSplitError(
                    f"duplicate alias {name} does not inherit canonical {field}")
        if record["canonical"] is False and name == canonical_name:
            raise DevelopmentSplitError(f"alias incorrectly references itself: {name}")

    pilot_count = sum(record["split"] == "pilot" for record in canonical.values())
    unassigned_count = sum(record["split"] == "unassigned" for record in canonical.values())
    if pilot_count == 0 or unassigned_count == 0:
        raise DevelopmentSplitError(
            "manifest must contain both frozen pilot and unassigned canonical images")
    pilot_selection = manifest.get("pilot_selection")
    if isinstance(pilot_selection, dict) and "size" in pilot_selection:
        if pilot_selection["size"] != pilot_count:
            raise DevelopmentSplitError(
                "pilot_selection.size does not match frozen canonical pilot count")
    return all_records, canonical


def load_class_catalog(
        class_catalog_path: Path,
) -> tuple[dict[int, dict[str, Any]], dict[str, Any]]:
    catalog, binding = read_json_snapshot(class_catalog_path)
    classes = catalog.get("classes") if isinstance(catalog, dict) else None
    if not isinstance(classes, list) or not classes:
        raise DevelopmentSplitError("class catalog must contain a non-empty classes list")
    definitions: dict[int, dict[str, Any]] = {}
    for item in classes:
        if not isinstance(item, dict):
            raise DevelopmentSplitError("every class catalog entry must be an object")
        class_id = item.get("id")
        if (
            not isinstance(class_id, int)
            or isinstance(class_id, bool)
            or class_id in definitions
        ):
            raise DevelopmentSplitError(
                "class catalog ids must be unique strict integers")
        for field in ("name", "displayNameZh", "mappingStatus"):
            if not isinstance(item.get(field), str) or not item[field]:
                raise DevelopmentSplitError(
                    f"class catalog {field} is invalid for id {class_id}")
        definitions[class_id] = item
    if len(definitions) != EXPECTED_CLASS_COUNT:
        raise DevelopmentSplitError(
            f"class catalog must contain exactly {EXPECTED_CLASS_COUNT} classes")
    if sorted(definitions) != list(range(EXPECTED_CLASS_COUNT)):
        raise DevelopmentSplitError(
            "class catalog ids must be contiguous from zero")
    return definitions, binding


def validate_preannotations(
        preannotations: Any, canonical: dict[str, dict],
        class_definitions: dict[int, dict[str, Any]],
        class_catalog_hash: str,
) -> tuple[dict[str, dict], dict[int, list[dict]], dict[int, str]]:
    if not isinstance(preannotations, dict):
        raise DevelopmentSplitError("preannotations must be a JSON object")
    info = preannotations.get("info")
    if not isinstance(info, dict):
        raise DevelopmentSplitError("preannotations.info must be an object")
    if info.get("ground_truth_complete") is not False:
        raise DevelopmentSplitError(
            "preannotations must explicitly set ground_truth_complete=false")
    if info.get("accuracy_metrics_claimed") is not False:
        raise DevelopmentSplitError(
            "preannotations must explicitly set accuracy_metrics_claimed=false")
    if info.get("class_catalog_sha256") != class_catalog_hash:
        raise DevelopmentSplitError(
            "preannotations.info.class_catalog_sha256 does not match the class catalog")

    categories = preannotations.get("categories")
    if not isinstance(categories, list):
        raise DevelopmentSplitError("preannotations.categories must be a list")
    category_names: dict[int, str] = {}
    for category in categories:
        if not isinstance(category, dict):
            raise DevelopmentSplitError("every preannotation category must be an object")
        category_id = category.get("id")
        name = category.get("name")
        if (not isinstance(category_id, int) or isinstance(category_id, bool)
                or category_id < 0 or category_id in category_names):
            raise DevelopmentSplitError("category ids must be unique non-negative integers")
        definition = class_definitions.get(category_id)
        if definition is None:
            raise DevelopmentSplitError(
                f"category id {category_id} is not in the class catalog")
        expected = {
            "name": definition["name"],
            "display_name_zh": definition["displayNameZh"],
            "mapping_status": definition["mappingStatus"],
        }
        for field, expected_value in expected.items():
            if category.get(field) != expected_value:
                raise DevelopmentSplitError(
                    f"category {category_id} {field} does not match the class catalog")
        category_names[category_id] = name
    if set(category_names) != set(class_definitions):
        raise DevelopmentSplitError(
            "preannotation categories must exactly match the full class catalog")

    images = preannotations.get("images")
    if not isinstance(images, list):
        raise DevelopmentSplitError("preannotations.images must be a list")
    by_name: dict[str, dict] = {}
    ids: set[int] = set()
    for image in images:
        if not isinstance(image, dict):
            raise DevelopmentSplitError("every preannotation image must be an object")
        image_id = image.get("id")
        if (not isinstance(image_id, int) or isinstance(image_id, bool)
                or image_id <= 0 or image_id in ids):
            raise DevelopmentSplitError("preannotation image ids must be unique positive integers")
        ids.add(image_id)
        name = image.get("file_name")
        if not isinstance(name, str) or not name or name in by_name:
            raise DevelopmentSplitError(
                "preannotation file_name values must be unique non-empty strings")
        if name not in canonical:
            raise DevelopmentSplitError(f"preannotation references non-canonical image: {name}")
        record = canonical[name]
        for field in ("sha256", "width", "height", "source_group",
                      "authorization_status"):
            if image.get(field) != record.get(field):
                raise DevelopmentSplitError(
                    f"preannotation {field} does not match manifest for {name}")
        source_split = image.get("split")
        allowed_source_splits = (
            {"unassigned", "pilot"} if record["split"] == "pilot" else {"unassigned"}
        )
        if source_split not in allowed_source_splits:
            raise DevelopmentSplitError(
                f"preannotation split is incompatible with frozen manifest for {name}")
        if image.get("annotation_status") != "preannotated":
            raise DevelopmentSplitError(
                f"preannotation image is not predictions-only for {name}")
        if image.get("is_ground_truth") is not False:
            raise DevelopmentSplitError(
                f"preannotation image claims ground truth for {name}")
        if image.get("p4_result_present") is not True:
            raise DevelopmentSplitError(
                f"preannotation image lacks P4 prediction provenance for {name}")
        if image.get("cigarette_decision") not in VALID_DECISIONS:
            raise DevelopmentSplitError(
                f"invalid predicted decision for {name}: {image.get('cigarette_decision')!r}")
        by_name[name] = image
    missing = sorted(set(canonical) - set(by_name), key=str.casefold)
    extra = sorted(set(by_name) - set(canonical), key=str.casefold)
    if missing or extra:
        raise DevelopmentSplitError(
            "preannotations must cover canonical manifest exactly; "
            f"missing={missing}, extra={extra}")

    annotations = preannotations.get("annotations")
    if not isinstance(annotations, list):
        raise DevelopmentSplitError("preannotations.annotations must be a list")
    by_image_id: dict[int, list[dict]] = {image["id"]: [] for image in images}
    annotation_ids: set[int] = set()
    image_by_id = {image["id"]: image for image in images}
    for annotation in annotations:
        if not isinstance(annotation, dict):
            raise DevelopmentSplitError("every preannotation annotation must be an object")
        annotation_id = annotation.get("id")
        if (not isinstance(annotation_id, int) or isinstance(annotation_id, bool)
                or annotation_id <= 0 or annotation_id in annotation_ids):
            raise DevelopmentSplitError(
                "preannotation annotation ids must be unique positive integers")
        annotation_ids.add(annotation_id)
        image_id = annotation.get("image_id")
        if (not isinstance(image_id, int) or isinstance(image_id, bool)
                or image_id not in image_by_id):
            raise DevelopmentSplitError(
                f"annotation {annotation_id} references an unknown image")
        category_id = annotation.get("category_id")
        if (not isinstance(category_id, int) or isinstance(category_id, bool)
                or category_id not in category_names):
            raise DevelopmentSplitError(
                f"annotation {annotation_id} has invalid category_id")
        bbox = annotation.get("bbox")
        if (not isinstance(bbox, list) or len(bbox) != 4
                or any(not isinstance(value, (int, float)) or isinstance(value, bool)
                       or not math.isfinite(value) for value in bbox)):
            raise DevelopmentSplitError(f"annotation {annotation_id} has invalid bbox")
        x, y, width, height = (float(value) for value in bbox)
        image = image_by_id[image_id]
        if (x < 0 or y < 0 or width <= 0 or height <= 0
                or x + width > image["width"] + 1e-6
                or y + height > image["height"] + 1e-6):
            raise DevelopmentSplitError(
                f"annotation {annotation_id} bbox is outside image bounds")
        area = annotation.get("area")
        if (not isinstance(area, (int, float)) or isinstance(area, bool)
                or not math.isfinite(area)
                or not math.isclose(float(area), width * height,
                                    rel_tol=1e-6, abs_tol=1e-6)):
            raise DevelopmentSplitError(f"annotation {annotation_id} area does not match bbox")
        score = annotation.get("score")
        if (not isinstance(score, (int, float)) or isinstance(score, bool)
                or not math.isfinite(score) or not 0 <= float(score) <= 1):
            raise DevelopmentSplitError(
                f"annotation {annotation_id} has invalid prediction score")
        if annotation.get("annotation_status") != "preannotated":
            raise DevelopmentSplitError(
                f"annotation {annotation_id} is not predictions-only")
        if annotation.get("is_ground_truth") is not False:
            raise DevelopmentSplitError(
                f"annotation {annotation_id} claims ground truth")
        if annotation.get("source") not in {None, "prediction"}:
            raise DevelopmentSplitError(
                f"annotation {annotation_id} has non-prediction source")
        if not isinstance(annotation.get("detector_version"), str) or not annotation["detector_version"]:
            raise DevelopmentSplitError(
                f"annotation {annotation_id} lacks detector_version")
        by_image_id[image_id].append(annotation)
    return by_name, by_image_id, category_names


def verify_source_images(
        source_root: Path, all_records: dict[str, dict],
) -> dict[str, dict[str, Any]]:
    try:
        resolved_root = source_root.resolve(strict=True)
    except OSError as exc:
        raise DevelopmentSplitError(f"source image root cannot be resolved: {source_root}") from exc
    if not resolved_root.is_dir():
        raise DevelopmentSplitError(f"source image root is not a directory: {source_root}")
    verified: dict[str, dict[str, Any]] = {}
    for name in sorted(all_records, key=str.casefold):
        record = all_records[name]
        source = resolve_source_path(resolved_root, record["relative_path"])
        digest = sha256_file(source)
        if digest != record["sha256"]:
            raise DevelopmentSplitError(f"source image SHA-256 mismatch for {name}")
        width, height, image_format = image_size(source)
        if (width, height) != (record["width"], record["height"]):
            raise DevelopmentSplitError(f"source image dimensions mismatch for {name}")
        expected_format = record.get("format")
        if expected_format is not None and str(expected_format).upper() != image_format:
            raise DevelopmentSplitError(f"source image format mismatch for {name}")
        verified[name] = {
            "path": source,
            "relative_path": record["relative_path"].replace("\\", "/"),
            "sha256": digest,
            "size": source.stat().st_size,
            "width": width,
            "height": height,
        }
    return verified


def image_features(
        image: dict, record: dict, annotations: list[dict], category_names: dict[int, str],
) -> tuple[str, ...]:
    features = {
        f"source_group={record['source_group']}",
        f"dimensions={record['width']}x{record['height']}",
        f"decision={image['cigarette_decision']}",
    }
    category_ids = sorted({annotation["category_id"] for annotation in annotations})
    if category_ids:
        features.update(
            f"class_id={category_id}:{category_names[category_id]}"
            for category_id in category_ids
        )
    else:
        features.add("class_id=none")
    return tuple(sorted(features))


def extract_timestamp(file_name: str) -> tuple[str | None, dt.datetime | None, str]:
    matches = TIMESTAMP_RE.findall(file_name)
    if len(matches) != 1:
        reason = "missing" if not matches else "ambiguous"
        return None, None, reason
    raw = matches[0]
    try:
        return raw, dt.datetime.strptime(raw, "%Y%m%d%H%M%S%f"), "parsed"
    except ValueError:
        return raw, None, "invalid"


def _event_id(source_group: str, records: list[dict], timestamp_status: str) -> str:
    first = records[0]
    last = records[-1]
    digest = hashlib.sha256(
        "\n".join(record["sha256"] for record in records).encode("ascii")
    ).hexdigest()[:16]
    if timestamp_status == "parsed":
        first_raw = first["_event_timestamp_raw"]
        last_raw = last["_event_timestamp_raw"]
        return f"event:{source_group}:{first_raw}-{last_raw}:{digest}"
    return f"event:{source_group}:unparsed:{first['sha256'][:16]}"


def build_event_groups(
        canonical: dict[str, dict], event_gap_ms: int,
) -> tuple[list[dict[str, Any]], dict[str, str], list[dict[str, str]]]:
    _strict_positive_int(event_gap_ms, "event_gap_ms")
    parsed_by_source: dict[str, list[dict]] = {}
    unparsed: list[dict] = []
    timestamp_notes: list[dict[str, str]] = []
    working: dict[str, dict] = {}
    for name, source in canonical.items():
        record = copy.deepcopy(source)
        raw, parsed, status = extract_timestamp(name)
        record["_event_timestamp_raw"] = raw
        record["_event_timestamp"] = parsed
        record["_event_timestamp_status"] = status
        working[name] = record
        if parsed is None:
            unparsed.append(record)
            timestamp_notes.append({"file_name": name, "strategy": "independent_event",
                                    "reason": status})
        else:
            parsed_by_source.setdefault(record["source_group"], []).append(record)

    groups: list[dict[str, Any]] = []
    for source_group in sorted(parsed_by_source, key=str.casefold):
        records = sorted(
            parsed_by_source[source_group],
            key=lambda item: (item["_event_timestamp"], item["sha256"], item["file_name"]),
        )
        current: list[dict] = []
        for record in records:
            if current:
                delta_ms = (
                    record["_event_timestamp"] - current[-1]["_event_timestamp"]
                ).total_seconds() * 1000
                if delta_ms > event_gap_ms:
                    groups.append({
                        "source_group": source_group,
                        "timestamp_status": "parsed",
                        "records": current,
                    })
                    current = []
            current.append(record)
        if current:
            groups.append({
                "source_group": source_group,
                "timestamp_status": "parsed",
                "records": current,
            })
    for record in sorted(unparsed, key=lambda item: (item["sha256"], item["file_name"])):
        groups.append({
            "source_group": record["source_group"],
            "timestamp_status": record["_event_timestamp_status"],
            "records": [record],
        })

    by_name: dict[str, str] = {}
    result = []
    for group in groups:
        records = group["records"]
        event_id = _event_id(group["source_group"], records, group["timestamp_status"])
        member_names = [record["file_name"] for record in records]
        for name in member_names:
            by_name[name] = event_id
        result.append({
            "event_id": event_id,
            "source_group": group["source_group"],
            "timestamp_status": group["timestamp_status"],
            "first_timestamp": records[0]["_event_timestamp_raw"],
            "last_timestamp": records[-1]["_event_timestamp_raw"],
            "members": member_names,
            "tie_break": [
                {"sha256": record["sha256"], "file_name": record["file_name"]}
                for record in sorted(
                    records, key=lambda item: (item["sha256"], item["file_name"]))
            ],
            "pilot_members": [
                record["file_name"] for record in records if record["split"] == "pilot"
            ],
            "unassigned_members": [
                record["file_name"] for record in records if record["split"] == "unassigned"
            ],
        })
    return sorted(result, key=lambda item: item["event_id"]), by_name, timestamp_notes


def _reachable_sums(sizes: Iterable[int]) -> set[int]:
    reachable = {0}
    for size in sizes:
        reachable |= {value + size for value in tuple(reachable)}
    return reachable


def _event_tie_key(event: dict[str, Any]) -> tuple[tuple[str, str], ...]:
    return tuple(
        (item["sha256"], item["file_name"]) for item in event["tie_break"]
    )


def choose_validation_events(
        events: list[dict[str, Any]], image_feature_map: dict[str, tuple[str, ...]],
        target_size: int,
) -> tuple[list[dict[str, Any]], int, dict[str, dict[str, int]]]:
    eligible_count = sum(len(event["members"]) for event in events)
    if not 0 < target_size < eligible_count:
        raise DevelopmentSplitError(
            f"validation_size must be between 1 and {eligible_count - 1}")
    reachable = sorted(
        value for value in _reachable_sums(len(event["members"]) for event in events)
        if 0 < value < eligible_count
    )
    if not reachable:
        raise DevelopmentSplitError(
            "event grouping leaves no valid non-empty train/validation partition")
    actual_size = min(reachable, key=lambda value: (abs(value - target_size), value))

    totals: Counter[str] = Counter()
    event_feature_counts: dict[str, Counter[str]] = {}
    for event in events:
        counts: Counter[str] = Counter()
        for name in event["members"]:
            counts.update(image_feature_map[name])
        event_feature_counts[event["event_id"]] = counts
        totals.update(counts)
    desired = {
        feature: count * actual_size / eligible_count for feature, count in totals.items()
    }
    selected: list[dict[str, Any]] = []
    selected_counts: Counter[str] = Counter()
    selected_size = 0
    remaining = sorted(events, key=_event_tie_key)
    while selected_size < actual_size:
        needed = actual_size - selected_size
        candidates = []
        for event in remaining:
            size = len(event["members"])
            if size > needed:
                continue
            other_sizes = [
                len(other["members"]) for other in remaining if other is not event
            ]
            if needed - size not in _reachable_sums(other_sizes):
                continue
            counts = event_feature_counts[event["event_id"]]
            newly_covered = sum(
                selected_counts[feature] == 0 for feature in counts
            )
            deficit_reduction = sum(
                min(count, max(desired[feature] - selected_counts[feature], 0.0))
                / max(desired[feature], 1e-12)
                for feature, count in counts.items()
            )
            rarity = sum(count / totals[feature] for feature, count in counts.items())
            size_closeness = -abs(needed - size)
            score = (
                newly_covered,
                round(deficit_reduction, 12),
                round(rarity, 12),
                size_closeness,
            )
            candidates.append((score, event))
        if not candidates:
            raise DevelopmentSplitError(
                "internal event selection error: no exact completion for reachable target")
        best_score = max(score for score, _ in candidates)
        chosen = next(
            event for score, event in candidates if score == best_score
        )
        chosen = copy.deepcopy(chosen)
        chosen["selection_rank"] = len(selected) + 1
        chosen["selection_reasons"] = {
            "new_feature_count": best_score[0],
            "normalized_feature_deficit_reduction": best_score[1],
            "rarity_score": best_score[2],
        }
        selected.append(chosen)
        selected_size += len(chosen["members"])
        selected_counts.update(event_feature_counts[chosen["event_id"]])
        remaining = [event for event in remaining if event["event_id"] != chosen["event_id"]]

    feature_balance = {
        feature: {
            "eligible": totals[feature],
            "validation": selected_counts[feature],
            "train": totals[feature] - selected_counts[feature],
        }
        for feature in sorted(totals)
    }
    return selected, actual_size, feature_balance


def copy_verified_source(source: Path, destination: Path, expected_sha256: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as source_handle, destination.open("xb") as destination_handle:
        shutil.copyfileobj(source_handle, destination_handle, length=1024 * 1024)
        destination_handle.flush()
        os.fsync(destination_handle.fileno())
    if sha256_file(destination) != expected_sha256:
        raise DevelopmentSplitError(
            f"source image changed while copying: {source.name}")


def _subset_coco(
        preannotations: dict, selected_names: set[str], split: str,
) -> dict[str, Any]:
    value = copy.deepcopy(preannotations)
    value["info"].update({
        "description": f"P5 {split} predictions-only human review candidates",
        "ground_truth_complete": False,
        "accuracy_metrics_claimed": False,
        "human_review_status": "pending",
        "training_complete": False,
        "split": split,
    })
    value["images"] = [
        image for image in value["images"] if image["file_name"] in selected_names
    ]
    selected_ids = {image["id"] for image in value["images"]}
    for image in value["images"]:
        image["split"] = split
    value["annotations"] = [
        annotation for annotation in value["annotations"]
        if annotation["image_id"] in selected_ids
    ]
    return value


def _write_review_csv(
        path: Path, rows: list[dict[str, Any]],
) -> None:
    fieldnames = [
        "split", "excluded_reason", "event_group_id", "file_name",
        "source_relative_path", "sha256", "source_group", "dimensions",
        "timestamp_status", "predicted_decision", "predicted_classes",
        "prediction_count", "annotation_status", "ground_truth",
        "human_review_status", "reviewed_decision", "reviewer_notes",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
        handle.flush()
        os.fsync(handle.fileno())


def _file_binding(path: Path, root: Path) -> dict[str, Any]:
    return {
        "path": path.relative_to(root).as_posix(),
        "sha256": sha256_file(path),
        "size": path.stat().st_size,
    }


def build_package(
        pilot_manifest_path: Path,
        preannotations_path: Path,
        source_images: Path,
        output_dir: Path,
        validation_size: int | None = None,
        event_gap_ms: int = DEFAULT_EVENT_GAP_MS,
        class_catalog_path: Path = DEFAULT_CLASS_CATALOG,
) -> dict[str, Any]:
    event_gap_ms = _strict_positive_int(event_gap_ms, "event_gap_ms")
    if validation_size is not None:
        validation_size = _strict_positive_int(validation_size, "validation_size")
    if os.path.lexists(output_dir):
        raise DevelopmentSplitError(f"output directory already exists: {output_dir}")

    manifest, manifest_snapshot = read_json_snapshot(pilot_manifest_path)
    preannotations, preannotations_snapshot = read_json_snapshot(preannotations_path)
    class_definitions, class_catalog_snapshot = load_class_catalog(class_catalog_path)
    all_records, canonical = validate_manifest(manifest)
    pre_by_name, annotations_by_image_id, category_names = validate_preannotations(
        preannotations,
        canonical,
        class_definitions,
        class_catalog_snapshot["sha256"],
    )
    verified_sources = verify_source_images(source_images, all_records)

    event_groups, event_by_name, timestamp_notes = build_event_groups(
        canonical, event_gap_ms)
    mixed_events = [
        event for event in event_groups
        if event["pilot_members"] and event["unassigned_members"]
    ]
    excluded_names = {
        name for event in mixed_events for name in event["unassigned_members"]
    }
    eligible_names = {
        name for name, record in canonical.items()
        if record["split"] == "unassigned" and name not in excluded_names
    }
    if len(eligible_names) < 2:
        raise DevelopmentSplitError(
            "near-pilot exclusion leaves fewer than two development candidates")

    image_features_by_name = {}
    for name, image in pre_by_name.items():
        image_features_by_name[name] = image_features(
            image, canonical[name], annotations_by_image_id[image["id"]], category_names)

    eligible_events = []
    for event in event_groups:
        members = set(event["members"])
        if members and members <= eligible_names:
            eligible_events.append(event)
    if set().union(*(set(event["members"]) for event in eligible_events)) != eligible_names:
        raise DevelopmentSplitError("internal event coverage error for eligible candidates")

    eligible_count = len(eligible_names)
    target_size = validation_size
    if target_size is None:
        target_size = max(
            1, min(eligible_count - 1,
                   math.floor(eligible_count * DEFAULT_VALIDATION_FRACTION + 0.5)))
    if not 0 < target_size < eligible_count:
        raise DevelopmentSplitError(
            f"validation_size must be between 1 and {eligible_count - 1}")
    selected_events, actual_validation_size, feature_balance = choose_validation_events(
        eligible_events, image_features_by_name, target_size)
    validation_names = {
        name for event in selected_events for name in event["members"]
    }
    train_names = eligible_names - validation_names
    if not train_names or not validation_names:
        raise DevelopmentSplitError("train and validation must both be non-empty")
    if train_names & validation_names:
        raise DevelopmentSplitError("internal train/validation overlap")
    pilot_names = {
        name for name, record in canonical.items() if record["split"] == "pilot"
    }
    if (train_names | validation_names) & pilot_names:
        raise DevelopmentSplitError("internal pilot leakage detected")

    pilot_features = Counter(
        feature for name in pilot_names for feature in image_features_by_name[name]
    )
    development_features = Counter(
        feature for name in eligible_names for feature in image_features_by_name[name]
    )
    pilot_only_features = [
        {
            "feature": feature,
            "pilot_count": pilot_features[feature],
            "eligible_development_count": 0,
        }
        for feature in sorted(pilot_features)
        if development_features[feature] == 0
    ]
    limitations = [
        "Predictions only: ground_truth=false and human review is pending.",
        "No accuracy metric is claimed and model training is not complete.",
    ]
    if mixed_events:
        limitations.append(
            f"{len(excluded_names)} unassigned canonical image(s) in "
            f"{len(mixed_events)} pilot-adjacent event group(s) remain unassigned.")
    if pilot_only_features:
        limitations.append(
            "Some pilot features are unavailable in eligible development data; "
            "coverage was not fabricated and frozen pilot membership was not changed.")
    if timestamp_notes:
        limitations.append(
            f"{len(timestamp_notes)} canonical image(s) had an unparseable timestamp "
            "and were conservatively assigned independent provisional events.")
    if actual_validation_size != target_size:
        limitations.append(
            f"Requested validation target {target_size} was not event-group reachable; "
            f"the closest deterministic size {actual_validation_size} was used.")

    split_by_canonical = {
        **{name: "pilot" for name in pilot_names},
        **{name: "unassigned" for name in excluded_names},
        **{name: "train" for name in train_names},
        **{name: "validation" for name in validation_names},
    }
    development_manifest = copy.deepcopy(manifest)
    for record in development_manifest["images"]:
        canonical_name = record["canonical_file_name"]
        record["split"] = split_by_canonical[canonical_name]
        record["provisional_event_group_id"] = event_by_name[canonical_name]
    development_manifest["development_split"] = {
        "schema_version": "p5-development-split-v1",
        "algorithm": "source-group-adjacent-timestamp-event-cover-v1",
        "event_gap_ms": event_gap_ms,
        "timestamp_pattern": "17-digit YYYYMMDDhhmmssSSS",
        "unparseable_timestamp_strategy": "conservative-independent-event",
        "validation_target_size": target_size,
        "validation_actual_size": actual_validation_size,
        "canonical_counts": {
            "pilot": len(pilot_names),
            "train": len(train_names),
            "validation": len(validation_names),
            "excluded_near_pilot_unassigned": len(excluded_names),
        },
        "excluded_near_pilot": mixed_events,
        "unparseable_timestamps": timestamp_notes,
        "pilot_only_features": pilot_only_features,
        "unavailable_in_development": pilot_only_features,
        "limitations": limitations,
        "ground_truth": False,
        "accuracy_metrics_claimed": False,
        "human_review_status": "pending",
        "training_complete": False,
    }

    train_coco = _subset_coco(preannotations, train_names, "train")
    validation_coco = _subset_coco(preannotations, validation_names, "validation")
    selected_event_ids = {event["event_id"] for event in selected_events}
    train_events = [
        event for event in eligible_events if event["event_id"] not in selected_event_ids
    ]
    selection = {
        "schema_version": "p5-development-selection-v1",
        "algorithm": "source-group-adjacent-timestamp-event-cover-v1",
        "event_gap_ms": event_gap_ms,
        "timestamp_pattern": "17-digit YYYYMMDDhhmmssSSS",
        "unparseable_timestamp_strategy": "conservative-independent-event",
        "validation_target_size": target_size,
        "validation_actual_size": actual_validation_size,
        "eligible_development_count": eligible_count,
        "train_count": len(train_names),
        "validation_count": len(validation_names),
        "validation_events": selected_events,
        "train_events": train_events,
        "excluded_near_pilot": mixed_events,
        "excluded_near_pilot_count": len(excluded_names),
        "unparseable_timestamps": timestamp_notes,
        "feature_balance": feature_balance,
        "pilot_only_features": pilot_only_features,
        "unavailable_in_development": pilot_only_features,
        "tie_break": "event members ordered by sha256 then file_name",
        "limitations": limitations,
        "ground_truth": False,
        "accuracy_metrics_claimed": False,
        "human_review_status": "pending",
        "training_complete": False,
    }

    review_rows = []
    for name in sorted(train_names | validation_names | excluded_names, key=str.casefold):
        image = pre_by_name[name]
        annotations = annotations_by_image_id[image["id"]]
        split = split_by_canonical[name]
        review_rows.append({
            "split": split,
            "excluded_reason": (
                "pilot-adjacent-event; remains unassigned"
                if name in excluded_names else ""
            ),
            "event_group_id": event_by_name[name],
            "file_name": name,
            "source_relative_path": canonical[name]["relative_path"].replace("\\", "/"),
            "sha256": canonical[name]["sha256"],
            "source_group": canonical[name]["source_group"],
            "dimensions": f"{canonical[name]['width']}x{canonical[name]['height']}",
            "timestamp_status": extract_timestamp(name)[2],
            "predicted_decision": image["cigarette_decision"],
            "predicted_classes": "|".join(
                category_names[category_id]
                for category_id in sorted({ann["category_id"] for ann in annotations})
            ),
            "prediction_count": len(annotations),
            "annotation_status": "preannotated",
            "ground_truth": "false",
            "human_review_status": "pending",
            "reviewed_decision": "",
            "reviewer_notes": "",
        })

    output_parent = output_dir.parent
    output_parent.mkdir(parents=True, exist_ok=True)
    resolved_parent = output_parent.resolve(strict=True)
    final_output = resolved_parent / output_dir.name
    if os.path.lexists(final_output):
        raise DevelopmentSplitError(f"output directory already exists: {output_dir}")
    staging = Path(tempfile.mkdtemp(
        prefix=f".{output_dir.name}.staging-", dir=resolved_parent))
    try:
        write_json(staging / "development-manifest.json", development_manifest)
        write_json(staging / "train-preannotations.coco.json", train_coco)
        write_json(staging / "validation-preannotations.coco.json", validation_coco)
        write_json(staging / "selection.json", selection)
        _write_review_csv(staging / "review.csv", review_rows)

        for split, names in (("train", train_names), ("validation", validation_names)):
            for name in sorted(names, key=str.casefold):
                source = verified_sources[name]
                destination = staging / split / "images"
                destination = destination.joinpath(
                    *_safe_relative_parts(source["relative_path"], "source relative_path"))
                copy_verified_source(source["path"], destination, source["sha256"])

        output_bindings = [
            _file_binding(path, staging)
            for path in sorted(
                (path for path in staging.rglob("*") if path.is_file()),
                key=lambda path: path.relative_to(staging).as_posix(),
            )
        ]
        require_unchanged_snapshot(
            pilot_manifest_path, manifest_snapshot, "pilot manifest")
        require_unchanged_snapshot(
            preannotations_path, preannotations_snapshot, "preannotations")
        require_unchanged_snapshot(
            class_catalog_path, class_catalog_snapshot, "class catalog")
        tool_path = Path(__file__).resolve()
        evidence = {
            "schema_version": "p5-development-package-evidence-v1",
            "status": "PASS",
            "package_claim": "event-aware predictions-only review package generated",
            "inputs": {
                "pilot_manifest": {
                    **manifest_snapshot,
                },
                "preannotations": {
                    **preannotations_snapshot,
                },
                "class_catalog": {
                    **class_catalog_snapshot,
                },
                "source_images": [
                    {
                        "file_name": name,
                        "relative_path": source["relative_path"],
                        "sha256": source["sha256"],
                        "size": source["size"],
                        "width": source["width"],
                        "height": source["height"],
                    }
                    for name, source in sorted(
                        verified_sources.items(), key=lambda item: item[0].casefold())
                ],
            },
            "tool": {
                "name": tool_path.name,
                "sha256": sha256_file(tool_path),
                "size": tool_path.stat().st_size,
            },
            "outputs": output_bindings,
            "event_grouping": {
                "algorithm": selection["algorithm"],
                "event_gap_ms": event_gap_ms,
                "timestamp_pattern": selection["timestamp_pattern"],
                "unparseable_timestamp_strategy":
                    selection["unparseable_timestamp_strategy"],
            },
            "counts": development_manifest["development_split"]["canonical_counts"],
            "excluded_near_pilot": mixed_events,
            "pilot_only_features": pilot_only_features,
            "unavailable_in_development": pilot_only_features,
            "limitations": limitations,
            "ground_truth": False,
            "accuracy_metrics_claimed": False,
            "human_review_status": "pending",
            "training_complete": False,
            "atomic_directory_promotion": True,
        }
        write_json(staging / "manifest.json", evidence)
        if os.path.lexists(final_output):
            raise DevelopmentSplitError(f"output directory already exists: {output_dir}")
        os.replace(staging, final_output)
    except Exception:
        if staging.exists():
            resolved_staging = staging.resolve()
            try:
                resolved_staging.relative_to(resolved_parent)
            except ValueError as exc:
                raise RuntimeError("refusing to clean staging outside output parent") from exc
            shutil.rmtree(resolved_staging)
        raise

    return {
        "output": str(final_output),
        "pilot_count": len(pilot_names),
        "eligible_development_count": eligible_count,
        "train_count": len(train_names),
        "validation_target_size": target_size,
        "validation_actual_size": actual_validation_size,
        "excluded_near_pilot_count": len(excluded_names),
        "pilot_only_features": pilot_only_features,
        "ground_truth": False,
        "accuracy_metrics_claimed": False,
        "human_review_status": "pending",
        "training_complete": False,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot-manifest", type=Path, required=True)
    parser.add_argument("--preannotations", type=Path, required=True)
    parser.add_argument("--source-images", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--class-catalog", type=Path, default=DEFAULT_CLASS_CATALOG,
        help="authoritative class catalog (default: config/p5-class-catalog.json)")
    parser.add_argument(
        "--validation-size", type=int,
        help="target canonical image count; closest event-reachable count is used")
    parser.add_argument(
        "--event-gap-ms", type=int, default=DEFAULT_EVENT_GAP_MS,
        help="positive adjacent capture gap used to form provisional events (default: 1000)")
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        report = build_package(
            args.pilot_manifest,
            args.preannotations,
            args.source_images,
            args.output,
            args.validation_size,
            args.event_gap_ms,
            args.class_catalog,
        )
        print(json.dumps(report, ensure_ascii=False, sort_keys=True))
        return 0
    except DevelopmentSplitError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
