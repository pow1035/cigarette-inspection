#!/usr/bin/env python3
"""Verify or atomically import a P5 reviewed-truth evidence package."""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import hashlib
import json
import os
import shutil
import stat
import sys
import uuid
from pathlib import Path
from typing import Any, Iterable

import p5_dataset_tools as dataset_tools


EVIDENCE_SCHEMA = "p5-reviewed-truth-evidence-manifest-v1"
TRANSFER_SCHEMA = "p5-reviewed-truth-transfer-manifest-v1"
RECEIPT_SCHEMA = "p5-reviewed-truth-transfer-receipt-v1"
REQUIRED_OUTPUTS = {
    "reviewed-ground-truth.coco.json",
    "approved-pilot-manifest.json",
    "evaluation-predictions.coco.json",
    "ground-truth-attestation.json",
    "promotion-summary.json",
}
SOURCE_FILES = REQUIRED_OUTPUTS | {"manifest.json"}
SHA256_HEX = frozenset("0123456789abcdef")
REPARSE_POINT = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)


class TransferError(ValueError):
    """The reviewed-truth package cannot be trusted or imported."""


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise TransferError(f"JSON contains duplicate key: {key!r}")
        value[key] = item
    return value


def read_json(path: Path) -> dict[str, Any]:
    return _read_json_payload(path.read_bytes(), path.name)


def _read_json_payload(payload: bytes, label: str) -> dict[str, Any]:
    try:
        value = json.loads(
            payload.decode("utf-8-sig"),
            object_pairs_hook=_reject_duplicate_keys,
        )
    except TransferError:
        raise
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise TransferError(f"cannot read JSON {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise TransferError(f"JSON root must be an object: {label}")
    return value


def write_json(path: Path, value: Any) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _is_reparse(path: Path) -> bool:
    try:
        return bool(getattr(path.lstat(), "st_file_attributes", 0) & REPARSE_POINT)
    except OSError as exc:
        raise TransferError(f"cannot inspect filesystem entry: {path}") from exc


def _require_plain_file(path: Path, label: str) -> None:
    try:
        mode = path.lstat().st_mode
    except OSError as exc:
        raise TransferError(f"{label} does not exist or cannot be inspected: {path}") from exc
    if path.is_symlink() or _is_reparse(path):
        raise TransferError(f"{label} must not be a symlink or reparse point: {path.name}")
    if not stat.S_ISREG(mode):
        raise TransferError(f"{label} must be a regular file: {path.name}")


def _require_plain_directory(path: Path, label: str) -> None:
    try:
        mode = path.lstat().st_mode
    except OSError as exc:
        raise TransferError(f"{label} does not exist or cannot be inspected: {path}") from exc
    if path.is_symlink() or _is_reparse(path):
        raise TransferError(f"{label} must not be a symlink or reparse point: {path}")
    if not stat.S_ISDIR(mode):
        raise TransferError(f"{label} must be a directory: {path}")


def _valid_sha256(value: Any) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and set(value.lower()) <= SHA256_HEX
    )


def _require_timestamp(value: Any, label: str) -> str:
    if not isinstance(value, str):
        raise TransferError(f"{label} must be an ISO-8601 timestamp")
    try:
        parsed = dt.datetime.fromisoformat(value)
    except ValueError as exc:
        raise TransferError(f"{label} must be an ISO-8601 timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise TransferError(f"{label} must include a timezone offset")
    return value


def _normalized(value: Any) -> str:
    return dataset_tools.normalized_identity(value)


def _require_source_layout(source: Path) -> dict[str, Path]:
    _require_plain_directory(source, "source")
    source_resolved = source.resolve(strict=True)
    entries: dict[str, Path] = {}
    try:
        children = list(source.iterdir())
    except OSError as exc:
        raise TransferError(f"cannot enumerate source directory: {source}") from exc
    for child in children:
        if child.name in {".", ".."} or Path(child.name).is_absolute():
            raise TransferError(f"source entry has an unsafe name: {child.name!r}")
        _require_plain_file(child, "source entry")
        resolved = child.resolve(strict=True)
        if resolved.parent != source_resolved:
            raise TransferError(f"source entry escapes the package directory: {child.name}")
        entries[child.name] = child
    actual = set(entries)
    missing = SOURCE_FILES - actual
    extra = actual - SOURCE_FILES
    if missing:
        raise TransferError(f"source package is missing required files: {sorted(missing)}")
    if extra:
        raise TransferError(f"source package contains unbound files: {sorted(extra)}")
    return entries


def _require_file_record(record: Any, label: str) -> tuple[str, int]:
    if not isinstance(record, dict) or set(record) != {"sha256", "size_bytes"}:
        raise TransferError(f"{label} must contain only sha256 and size_bytes")
    digest = record.get("sha256")
    size = record.get("size_bytes")
    if not _valid_sha256(digest):
        raise TransferError(f"{label} has an invalid SHA-256")
    if not isinstance(size, int) or isinstance(size, bool) or size < 0:
        raise TransferError(f"{label} has an invalid size_bytes")
    return digest.lower(), size


def _snapshot_plain_file(path: Path, label: str) -> tuple[bytes, str, int]:
    _require_plain_file(path, label)
    try:
        payload = path.read_bytes()
    except OSError as exc:
        raise TransferError(f"cannot read {label}: {path}") from exc
    return payload, hashlib.sha256(payload).hexdigest(), len(payload)


def _require_snapshots_unchanged(
        entries: dict[str, Path], snapshots: dict[str, tuple[str, int]]
) -> None:
    if set(_require_source_layout(next(iter(entries.values())).parent)) != set(entries):
        raise TransferError("source package layout changed during verification")
    for name, (expected_hash, expected_size) in snapshots.items():
        _, actual_hash, actual_size = _snapshot_plain_file(
            entries[name], f"source entry {name!r}")
        if actual_hash != expected_hash or actual_size != expected_size:
            raise TransferError(f"source package changed during verification: {name}")


def _verify_output_files(
        entries: dict[str, Path], evidence: dict[str, Any]
) -> tuple[
    dict[str, dict[str, Any]],
    dict[str, dict[str, Any]],
    dict[str, tuple[str, int]],
]:
    outputs = evidence.get("outputs")
    if not isinstance(outputs, dict) or set(outputs) != REQUIRED_OUTPUTS:
        missing = sorted(REQUIRED_OUTPUTS - set(outputs or {})) if isinstance(outputs, dict) else []
        extra = sorted(set(outputs or {}) - REQUIRED_OUTPUTS) if isinstance(outputs, dict) else []
        raise TransferError(
            f"manifest outputs must be exactly the five reviewed-truth files; "
            f"missing={missing}, extra={extra}"
        )
    verified: dict[str, dict[str, Any]] = {}
    parsed: dict[str, dict[str, Any]] = {}
    snapshots: dict[str, tuple[str, int]] = {}
    for name in sorted(REQUIRED_OUTPUTS):
        if Path(name).is_absolute() or len(Path(name).parts) != 1 or ".." in Path(name).parts:
            raise TransferError(f"manifest output path is unsafe: {name!r}")
        expected_hash, expected_size = _require_file_record(
            outputs[name], f"manifest output {name!r}"
        )
        path = entries[name]
        payload, actual_hash, actual_size = _snapshot_plain_file(
            path, f"output {name!r}")
        if actual_size != expected_size:
            raise TransferError(
                f"output size mismatch for {name}: expected {expected_size}, got {actual_size}"
            )
        if actual_hash != expected_hash:
            raise TransferError(
                f"output SHA-256 mismatch for {name}: expected {expected_hash}, got {actual_hash}"
            )
        verified[name] = {
            "path": name,
            "sha256": actual_hash,
            "size_bytes": actual_size,
        }
        parsed[name] = _read_json_payload(payload, name)
        snapshots[name] = (actual_hash, actual_size)
    return verified, parsed, snapshots


def _require_manifest_contract(
        evidence: dict[str, Any], class_catalog_hash: str
) -> None:
    if evidence.get("schema_version") != EVIDENCE_SCHEMA:
        raise TransferError(f"manifest schema_version must be {EVIDENCE_SCHEMA!r}")
    if evidence.get("status") != "PASS":
        raise TransferError("manifest status must be PASS")
    _require_timestamp(evidence.get("created_at"), "manifest created_at")
    validation = evidence.get("validation")
    expected_validation = {
        "reviewed_truth": "PASS",
        "evaluation_predictions": "PASS",
        "atomic_directory_promotion": True,
    }
    if validation != expected_validation:
        raise TransferError("manifest validation record is incomplete or not PASS")
    inputs = evidence.get("inputs")
    required_inputs = {
        "source_pass1", "source_manifest", "source_predictions", "class_catalog"
    }
    if not isinstance(inputs, dict) or set(inputs) != required_inputs:
        raise TransferError("manifest inputs must exactly bind promotion inputs")
    for name, record in inputs.items():
        if not isinstance(record, dict):
            raise TransferError(f"manifest input {name!r} must be an object")
        digest = record.get("sha256")
        size = record.get("size_bytes")
        if not _valid_sha256(digest):
            raise TransferError(f"manifest input {name!r} has an invalid SHA-256")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise TransferError(f"manifest input {name!r} has an invalid size_bytes")
    if inputs["class_catalog"]["sha256"].lower() != class_catalog_hash:
        raise TransferError("manifest class catalog binding does not match the supplied catalog")
    implementation = evidence.get("implementation")
    expected_tools = {
        "promotion_tool": "p5_promote_reviewed_truth.py",
        "validator": "p5_dataset_tools.py",
    }
    if not isinstance(implementation, dict) or set(implementation) != set(expected_tools):
        raise TransferError(
            "manifest implementation must exactly bind promotion_tool and validator")
    for name, expected_file_name in expected_tools.items():
        record = implementation[name]
        if not isinstance(record, dict) or set(record) != {
            "path", "sha256", "size_bytes"
        }:
            raise TransferError(
                f"manifest implementation {name!r} must contain path, sha256, and size_bytes")
        path = record.get("path")
        if (
            not isinstance(path, str)
            or not path.strip()
            or "\x00" in path
            or Path(path.replace("\\", "/")).name.casefold()
            != expected_file_name.casefold()
        ):
            raise TransferError(
                f"manifest implementation {name!r} path must identify {expected_file_name}")
        if not _valid_sha256(record.get("sha256")):
            raise TransferError(
                f"manifest implementation {name!r} has an invalid SHA-256")
        size = record.get("size_bytes")
        if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
            raise TransferError(
                f"manifest implementation {name!r} has an invalid size_bytes")


def _require_identity_contract(
        truth: dict[str, Any], attestation: dict[str, Any], summary: dict[str, Any]
) -> None:
    identity_fields = (
        "annotated_by", "annotator_id", "reviewed_by", "reviewer_id",
        "reviewed_at", "authorization_approved_by", "approver_id",
        "approval_basis", "attested_at",
    )
    for field in identity_fields:
        if summary.get(field) != attestation.get(field):
            raise TransferError(f"promotion summary and attestation disagree on {field}")
    annotator_name = _normalized(attestation.get("annotated_by"))
    reviewer_name = _normalized(attestation.get("reviewed_by"))
    annotator_id = _normalized(attestation.get("annotator_id"))
    reviewer_id = _normalized(attestation.get("reviewer_id"))
    if (
        not annotator_name
        or not reviewer_name
        or annotator_name == reviewer_name
        or not annotator_id
        or not reviewer_id
        or annotator_id == reviewer_id
    ):
        raise TransferError("attestation requires distinct annotator and reviewer stable identities")
    if (
        not _normalized(attestation.get("authorization_approved_by"))
        or not _normalized(attestation.get("approver_id"))
        or not isinstance(attestation.get("approval_basis"), str)
        or not attestation["approval_basis"].strip()
    ):
        raise TransferError("attestation requires an explicit approver, stable id, and basis")
    _require_timestamp(attestation.get("reviewed_at"), "attestation reviewed_at")
    _require_timestamp(attestation.get("attested_at"), "attestation attested_at")

    info = truth.get("info")
    if not isinstance(info, dict):
        raise TransferError("reviewed truth info must be an object")
    expected_info = {
        "annotation_stage": "reviewed-ground-truth",
        "annotation_status": "reviewed",
        "ground_truth_complete": True,
        "accuracy_metrics_claimed": False,
        "authorization_status": "approved",
        "evaluation_split": "pilot",
        "review_method": "human-double-review",
    }
    for field, expected in expected_info.items():
        if info.get(field) != expected:
            raise TransferError(f"reviewed truth info.{field} must be {expected!r}")
    if [_normalized(item) for item in info.get("annotators", [])] != [annotator_name]:
        raise TransferError("reviewed truth annotator name binding is invalid")
    if [_normalized(item) for item in info.get("annotator_ids", [])] != [annotator_id]:
        raise TransferError("reviewed truth annotator stable-id binding is invalid")
    if [_normalized(item) for item in info.get("reviewers", [])] != [reviewer_name]:
        raise TransferError("reviewed truth reviewer name binding is invalid")
    if [_normalized(item) for item in info.get("reviewer_ids", [])] != [reviewer_id]:
        raise TransferError("reviewed truth reviewer stable-id binding is invalid")
    if info.get("reviewed_at") != attestation.get("reviewed_at"):
        raise TransferError("reviewed truth and attestation disagree on reviewed_at")

    for section in ("images", "annotations"):
        records = truth.get(section)
        if not isinstance(records, list):
            raise TransferError(f"reviewed truth {section} must be a list")
        for record in records:
            if not isinstance(record, dict):
                raise TransferError(f"reviewed truth {section} contains a non-object")
            expected = {
                "annotated_by": annotator_name,
                "annotator_id": annotator_id,
                "reviewed_by": reviewer_name,
                "reviewer_id": reviewer_id,
            }
            for field, normalized_expected in expected.items():
                if _normalized(record.get(field)) != normalized_expected:
                    raise TransferError(
                        f"reviewed truth {section} record disagrees on {field}"
                    )
            if record.get("reviewed_at") != attestation.get("reviewed_at"):
                raise TransferError(
                    f"reviewed truth {section} record disagrees on reviewed_at"
                )


def _require_cross_bindings(
        files: dict[str, dict[str, Any]],
        evidence: dict[str, Any],
        truth: dict[str, Any],
        approved_manifest: dict[str, Any],
        predictions: dict[str, Any],
        attestation: dict[str, Any],
        summary: dict[str, Any],
        class_catalog_hash: str,
) -> None:
    expected_attestation = {
        "schema_version": "p5-ground-truth-attestation-v1",
        "review_method": "human-double-review",
        "authorization_status": "approved",
        "evaluation_split": "pilot",
    }
    for field, expected in expected_attestation.items():
        if attestation.get(field) != expected:
            raise TransferError(f"attestation {field} must be {expected!r}")
    if attestation.get("ground_truth_sha256") != files[
            "reviewed-ground-truth.coco.json"]["sha256"]:
        raise TransferError("attestation ground_truth_sha256 does not match the package")
    if attestation.get("manifest_sha256") != files[
            "approved-pilot-manifest.json"]["sha256"]:
        raise TransferError("attestation manifest_sha256 does not match the package")
    if attestation.get("class_catalog_sha256") != class_catalog_hash:
        raise TransferError("attestation class_catalog_sha256 does not match the supplied catalog")
    source_pass1 = evidence["inputs"]["source_pass1"]["sha256"].lower()
    if attestation.get("source_pass1_sha256") != source_pass1:
        raise TransferError("attestation source_pass1_sha256 does not match evidence inputs")
    if truth.get("info", {}).get("source_pass1_sha256") != source_pass1:
        raise TransferError("reviewed truth source_pass1_sha256 does not match evidence inputs")

    expected_summary = {
        "schema_version": "p5-reviewed-truth-promotion-summary-v1",
        "status": "PASS",
        "evaluation_split": "pilot",
        "review_method": "human-double-review",
        "authorization_status": "approved",
        "accuracy_metrics_claimed": False,
    }
    for field, expected in expected_summary.items():
        if summary.get(field) != expected:
            raise TransferError(f"promotion summary {field} must be {expected!r}")
    _require_identity_contract(truth, attestation, summary)

    truth_images = truth.get("images", [])
    decisions = collections.Counter(item.get("cigarette_decision") for item in truth_images)
    review_count = decisions.get("REVIEW", 0)
    expected_counts = {
        "reviewed_image_count": len(truth_images),
        "comparable_image_count": len(truth_images) - review_count,
        "review_excluded_image_count": review_count,
        "formal_ground_truth_box_count": len(truth.get("annotations", [])),
    }
    for field, expected in expected_counts.items():
        if summary.get(field) != expected:
            raise TransferError(f"promotion summary {field} must be {expected}")
    if summary.get("decision_counts") != dict(sorted(decisions.items())):
        raise TransferError("promotion summary decision_counts does not match reviewed truth")
    reference_count = summary.get("review_reference_box_count_excluded")
    if not isinstance(reference_count, int) or isinstance(reference_count, bool) or reference_count < 0:
        raise TransferError("promotion summary review reference box count is invalid")
    if review_count == 0 and reference_count != 0:
        raise TransferError("promotion summary excludes reference boxes without REVIEW images")

    prediction_info = predictions.get("info")
    if (
        not isinstance(prediction_info, dict)
        or prediction_info.get("ground_truth_complete") is not False
        or prediction_info.get("accuracy_metrics_claimed") is not False
    ):
        raise TransferError("evaluation predictions must explicitly remain non-ground-truth")
    prediction_images = predictions.get("images")
    prediction_annotations = predictions.get("annotations")
    if not isinstance(prediction_images, list) or not isinstance(
        prediction_annotations, list
    ):
        raise TransferError("evaluation predictions must contain image and annotation lists")
    for image in prediction_images:
        if (
            not isinstance(image, dict)
            or image.get("annotation_status") != "preannotated"
            or image.get("is_ground_truth") is not False
            or image.get("split") != "pilot"
            or image.get("authorization_status") != "approved"
            or image.get("p4_result_present") is not True
        ):
            raise TransferError(
                "evaluation prediction images must be approved pilot preannotations"
            )
    for annotation in prediction_annotations:
        if (
            not isinstance(annotation, dict)
            or annotation.get("annotation_status") != "preannotated"
            or annotation.get("is_ground_truth") is not False
            or not isinstance(annotation.get("score"), (int, float))
            or isinstance(annotation.get("score"), bool)
            or not isinstance(annotation.get("detector_version"), str)
            or not annotation["detector_version"]
        ):
            raise TransferError(
                "evaluation prediction annotations must retain model provenance"
            )
    if any(
        item.get("split") == "pilot"
        and item.get("canonical") is True
        and item.get("authorization_status") != "approved"
        for item in approved_manifest.get("images", [])
        if isinstance(item, dict)
    ):
        raise TransferError("approved manifest contains an unapproved canonical pilot image")


def verify_package(
        source: Path,
        class_catalog: Path = dataset_tools.DEFAULT_CLASS_CATALOG,
) -> dict[str, Any]:
    source = Path(source)
    class_catalog = Path(class_catalog)
    entries = _require_source_layout(source)
    catalog_payload, class_catalog_hash, _ = _snapshot_plain_file(
        class_catalog, "class catalog")
    manifest_payload, manifest_hash, manifest_size = _snapshot_plain_file(
        entries["manifest.json"], "source manifest")
    evidence = _read_json_payload(manifest_payload, "manifest.json")
    _require_manifest_contract(evidence, class_catalog_hash)
    verified_files, parsed, snapshots = _verify_output_files(entries, evidence)

    approved_manifest = parsed["approved-pilot-manifest.json"]
    truth = parsed["reviewed-ground-truth.coco.json"]
    predictions = parsed["evaluation-predictions.coco.json"]
    attestation = parsed["ground-truth-attestation.json"]
    summary = parsed["promotion-summary.json"]

    dataset_tools.configure_classes(class_catalog)
    if class_catalog.read_bytes() != catalog_payload:
        raise TransferError("class catalog changed during verification")
    truth_errors = dataset_tools.validate_annotations(
        truth, approved_manifest, require_reviewed=True, required_split="pilot"
    )
    if truth_errors:
        raise TransferError(
            "reviewed truth validation failed:\n- " + "\n- ".join(truth_errors)
        )
    prediction_errors = dataset_tools.validate_annotations(
        predictions, approved_manifest, require_reviewed=False, required_split="pilot"
    )
    if prediction_errors:
        raise TransferError(
            "evaluation prediction validation failed:\n- "
            + "\n- ".join(prediction_errors)
        )
    _require_cross_bindings(
        verified_files,
        evidence,
        truth,
        approved_manifest,
        predictions,
        attestation,
        summary,
        class_catalog_hash,
    )
    _require_snapshots_unchanged(
        entries,
        {
            "manifest.json": (manifest_hash, manifest_size),
            **snapshots,
        },
    )
    try:
        current_catalog_payload = class_catalog.read_bytes()
    except OSError as exc:
        raise TransferError("class catalog changed or became unreadable") from exc
    if current_catalog_payload != catalog_payload:
        raise TransferError("class catalog changed during verification")
    return {
        "schema_version": "p5-reviewed-truth-transfer-verification-v1",
        "status": "PASS",
        "accuracy_metrics_claimed": False,
        "source_manifest_sha256": manifest_hash,
        "class_catalog_sha256": class_catalog_hash,
        "source_implementation": evidence["implementation"],
        "reviewed_image_count": summary["reviewed_image_count"],
        "comparable_image_count": summary["comparable_image_count"],
        "files": verified_files,
    }


def _copy_verified_file(source: Path, destination: Path) -> None:
    _require_plain_file(source, "source file")
    with source.open("rb") as input_stream, destination.open("xb") as output_stream:
        shutil.copyfileobj(input_stream, output_stream, length=1024 * 1024)
        output_stream.flush()
        os.fsync(output_stream.fileno())


def _relative_file_record(path: Path, root: Path) -> dict[str, Any]:
    relative = path.relative_to(root).as_posix()
    if Path(relative).is_absolute() or ".." in Path(relative).parts:
        raise TransferError(f"generated import binding is not portable: {relative!r}")
    return {
        "path": relative,
        "sha256": sha256_file(path),
        "size_bytes": path.stat().st_size,
    }


def import_package(
        source: Path,
        destination: Path,
        class_catalog: Path = dataset_tools.DEFAULT_CLASS_CATALOG,
) -> dict[str, Any]:
    source = Path(source)
    destination = Path(destination)
    class_catalog = Path(class_catalog)
    verification = verify_package(source, class_catalog)
    if destination.exists() or destination.is_symlink():
        raise TransferError(f"refusing to overwrite existing destination: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    _require_plain_directory(destination.parent, "destination parent")
    staging = destination.parent / (
        f".{destination.name}.{os.getpid()}.{uuid.uuid4().hex}.staging"
    )
    if staging.exists():
        raise TransferError(f"staging path unexpectedly exists: {staging}")
    staging.mkdir()
    try:
        for name in sorted(SOURCE_FILES):
            _copy_verified_file(source / name, staging / name)
        staged_verification = verify_package(staging, class_catalog)
        if staged_verification["source_manifest_sha256"] != verification["source_manifest_sha256"]:
            raise TransferError("source package changed during import")

        os.replace(staging / "manifest.json", staging / "source-manifest.json")
        copied_records = {
            name: _relative_file_record(staging / name, staging)
            for name in sorted(REQUIRED_OUTPUTS | {"source-manifest.json"})
        }
        receipt = {
            "schema_version": RECEIPT_SCHEMA,
            "status": "PASS",
            "transferred_at": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
            "accuracy_metrics_claimed": False,
            "source_evidence_schema": EVIDENCE_SCHEMA,
            "source_manifest": copied_records["source-manifest.json"],
            "class_catalog_sha256": verification["class_catalog_sha256"],
            "reviewed_image_count": verification["reviewed_image_count"],
            "comparable_image_count": verification["comparable_image_count"],
            "files": {
                name: record
                for name, record in copied_records.items()
                if name != "source-manifest.json"
            },
        }
        receipt_path = staging / "transfer-receipt.json"
        write_json(receipt_path, receipt)
        complete_files = {
            **copied_records,
            "transfer-receipt.json": _relative_file_record(receipt_path, staging),
        }
        imported_manifest = {
            "schema_version": TRANSFER_SCHEMA,
            "status": "PASS",
            "created_at": receipt["transferred_at"],
            "accuracy_metrics_claimed": False,
            "portable_bindings": True,
            "source_evidence_schema": EVIDENCE_SCHEMA,
            "source_manifest_sha256": verification["source_manifest_sha256"],
            "class_catalog_sha256": verification["class_catalog_sha256"],
            "files": complete_files,
            "validation": {
                "source_verified": "PASS",
                "copied_package_verified": "PASS",
                "atomic_directory_import": True,
            },
        }
        write_json(staging / "manifest.json", imported_manifest)
        expected_names = (
            REQUIRED_OUTPUTS
            | {"source-manifest.json", "transfer-receipt.json", "manifest.json"}
        )
        if {item.name for item in staging.iterdir()} != expected_names:
            raise TransferError("staged import contains an unexpected file set")
        if destination.exists():
            raise TransferError(f"destination appeared during import: {destination}")
        os.rename(staging, destination)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return {
        "schema_version": RECEIPT_SCHEMA,
        "status": "PASS",
        "accuracy_metrics_claimed": False,
        "destination_manifest": "manifest.json",
        "source_manifest": "source-manifest.json",
        "transfer_receipt": "transfer-receipt.json",
        "source_manifest_sha256": verification["source_manifest_sha256"],
        "reviewed_image_count": verification["reviewed_image_count"],
        "comparable_image_count": verification["comparable_image_count"],
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    verify = subparsers.add_parser("verify", help="verify a reviewed-truth evidence package")
    verify.add_argument("--source", type=Path, required=True)
    verify.add_argument(
        "--class-catalog", type=Path, default=dataset_tools.DEFAULT_CLASS_CATALOG
    )
    verify.set_defaults(function=lambda args: verify_package(args.source, args.class_catalog))

    import_command = subparsers.add_parser(
        "import", help="verify and atomically import into a fresh directory"
    )
    import_command.add_argument("--source", type=Path, required=True)
    import_command.add_argument("--destination", type=Path, required=True)
    import_command.add_argument(
        "--class-catalog", type=Path, default=dataset_tools.DEFAULT_CLASS_CATALOG
    )
    import_command.set_defaults(
        function=lambda args: import_package(
            args.source, args.destination, args.class_catalog
        )
    )
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        result = args.function(args)
    except (TransferError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
