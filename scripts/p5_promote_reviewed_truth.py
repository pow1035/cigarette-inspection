#!/usr/bin/env python3
"""Promote an immutable P5 pass1 export to approved reviewed pilot truth."""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import hashlib
import json
import os
import shutil
import sys
import uuid
from collections import Counter
from pathlib import Path
from typing import Any

import p5_dataset_tools as dataset_tools

PREDICTION_ANNOTATION_FIELDS = {"score", "detector_version", "source_bbox", "boundary_clamped"}
PREDICTION_IMAGE_FIELDS = {"p4_result_present", "predicted_decision", "review_required", "review_reason"}
VALID_DECISIONS = {"OK", "NG", "REVIEW"}


class PromotionError(ValueError):
    """Pass1 evidence cannot be safely promoted."""


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            value = json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise PromotionError(f"cannot read JSON {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise PromotionError(f"JSON root must be an object: {path}")
    return value


def write_json(path: Path, value: Any) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as stream:
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


def require_sha256(path: Path, expected: str, label: str) -> str:
    expected = expected.lower()
    if len(expected) != 64 or any(char not in "0123456789abcdef" for char in expected):
        raise PromotionError(f"expected {label} SHA-256 is invalid")
    actual = sha256_file(path)
    if actual != expected:
        raise PromotionError(f"{label} SHA-256 drift: expected {expected}, got {actual}")
    return actual


def require_timestamp(value: str, label: str) -> str:
    try:
        parsed = dt.datetime.fromisoformat(value)
    except ValueError as exc:
        raise PromotionError(f"{label} must be an ISO-8601 timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise PromotionError(f"{label} must include a timezone offset")
    return value


def require_list(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise PromotionError(f"{label} must be a list")
    return value


def catalog_categories(catalog: dict[str, Any]) -> list[dict[str, Any]]:
    classes = require_list(catalog.get("classes"), "class catalog classes")
    ordered = sorted(classes, key=lambda item: item.get("id", -1) if isinstance(item, dict) else -1)
    if [item.get("id") for item in ordered if isinstance(item, dict)] != list(range(len(ordered))):
        raise PromotionError("class catalog ids must be contiguous from zero")
    return [{
        "id": item["id"], "name": item.get("name"),
        "display_name_zh": item.get("displayNameZh", item.get("name")),
        "mapping_status": item.get("mappingStatus"), "supercategory": "cigarette_defect",
    } for item in ordered]


def validate_inputs(pass1: dict[str, Any], manifest: dict[str, Any],
                    predictions: dict[str, Any], catalog: dict[str, Any],
                    catalog_sha256: str, annotated_by: str,
                    annotator_id: str, reviewed_by: str,
                    reviewer_id: str, approved_by: str,
                    approver_id: str, approval_basis: str
                    ) -> tuple[set[int], Counter[str]]:
    annotated_identity = dataset_tools.normalized_identity(annotated_by)
    reviewed_identity = dataset_tools.normalized_identity(reviewed_by)
    annotator_identity = dataset_tools.normalized_identity(annotator_id)
    reviewer_identity = dataset_tools.normalized_identity(reviewer_id)
    if (not annotated_identity or not reviewed_identity
            or annotated_identity == reviewed_identity
            or not annotator_identity or not reviewer_identity
            or annotator_identity == reviewer_identity):
        raise PromotionError(
            "annotator and reviewer must have distinct non-empty names and stable ids")
    if (not dataset_tools.normalized_identity(approved_by)
            or not dataset_tools.normalized_identity(approver_id)
            or not isinstance(approval_basis, str) or not approval_basis.strip()):
        raise PromotionError("approval requires an explicit approver, stable id, and basis")
    info = pass1.get("info")
    if not isinstance(info, dict):
        raise PromotionError("pass1 info must be an object")
    expected_info = {
        "annotation_stage": "pass1", "annotation_status": "annotated",
        "ground_truth_complete": False, "accuracy_metrics_claimed": False, "source": "human",
    }
    for field, expected in expected_info.items():
        if info.get(field) != expected:
            raise PromotionError(f"pass1 info.{field} must be {expected!r}")
    if str(info.get("class_catalog_sha256", "")).lower() != catalog_sha256:
        raise PromotionError("pass1 class catalog binding does not match supplied catalog")
    pass1_annotators = info.get("annotators")
    if (not isinstance(pass1_annotators, list) or len(pass1_annotators) != 1
            or dataset_tools.normalized_identity(pass1_annotators[0])
            != annotated_identity):
        raise PromotionError("pass1 annotator binding does not match approved annotator")
    expected_categories = catalog_categories(catalog)
    if pass1.get("categories") != expected_categories:
        raise PromotionError("pass1 categories do not exactly match class catalog")

    manifest_images = require_list(manifest.get("images"), "manifest images")
    canonical = {item.get("file_name"): item for item in manifest_images
                 if isinstance(item, dict) and item.get("canonical") is True}
    pilot = {name: item for name, item in canonical.items() if item.get("split") == "pilot"}
    if not pilot:
        raise PromotionError("manifest contains no canonical pilot images")
    if any(item.get("authorization_status") != "unverified" for item in pilot.values()):
        raise PromotionError("source pilot manifest must still be unverified")

    images = require_list(pass1.get("images"), "pass1 images")
    names = [item.get("file_name") for item in images if isinstance(item, dict)]
    if len(names) != len(images) or len(set(names)) != len(names) or set(names) != set(pilot):
        raise PromotionError("pass1 images do not exactly match frozen pilot split")
    image_ids: dict[int, dict[str, Any]] = {}
    decisions: Counter[str] = Counter()
    for image in images:
        image_id, name = image.get("id"), image.get("file_name")
        if not isinstance(image_id, int) or isinstance(image_id, bool) or image_id in image_ids:
            raise PromotionError(f"pass1 image id is not a unique integer: {image_id!r}")
        for field in ("sha256", "width", "height", "source_group", "split", "authorization_status"):
            if image.get(field) != pilot[name].get(field):
                raise PromotionError(f"pass1 image {name} disagrees with manifest field {field}")
        if (image.get("annotation_status") != "annotated"
                or image.get("is_ground_truth") is not False
                or image.get("source") != "human"
                or dataset_tools.normalized_identity(image.get("annotated_by"))
                != annotated_identity):
            raise PromotionError(f"pass1 image is not clean human evidence: {name}")
        if PREDICTION_IMAGE_FIELDS.intersection(image):
            raise PromotionError(f"pass1 image retains prediction provenance: {name}")
        if image.get("cigarette_decision") not in VALID_DECISIONS:
            raise PromotionError(f"pass1 image has invalid decision: {name}")
        decisions[image["cigarette_decision"]] += 1
        image_ids[image_id] = image

    review_ids = {image_id for image_id, image in image_ids.items()
                  if image["cigarette_decision"] == "REVIEW"}
    unconfirmed = {item["id"] for item in catalog["classes"] if
                   str(item.get("mappingStatus", "")).startswith("unconfirmed")}
    annotation_ids: set[int] = set()
    by_image: Counter[int] = Counter()
    for annotation in require_list(pass1.get("annotations"), "pass1 annotations"):
        if not isinstance(annotation, dict):
            raise PromotionError("every pass1 annotation must be an object")
        annotation_id, image_id = annotation.get("id"), annotation.get("image_id")
        if not isinstance(annotation_id, int) or isinstance(annotation_id, bool) or annotation_id in annotation_ids:
            raise PromotionError(f"pass1 annotation id is not a unique integer: {annotation_id!r}")
        annotation_ids.add(annotation_id)
        if image_id not in image_ids:
            raise PromotionError(f"pass1 annotation references unknown image id: {image_id!r}")
        if (annotation.get("annotation_status") != "annotated"
                or annotation.get("is_ground_truth") is not False
                or annotation.get("source") != "human"
                or dataset_tools.normalized_identity(annotation.get("annotated_by"))
                != annotated_identity
                or PREDICTION_ANNOTATION_FIELDS.intersection(annotation)):
            raise PromotionError(f"pass1 annotation is not clean human evidence: {annotation_id!r}")
        if annotation.get("category_id") in unconfirmed and image_id not in review_ids:
            raise PromotionError(f"unconfirmed class occurs outside a REVIEW image: {annotation_id!r}")
        by_image[image_id] += 1
    for image_id, image in image_ids.items():
        if image["cigarette_decision"] == "OK" and by_image[image_id]:
            raise PromotionError(f"OK image contains defect boxes: {image['file_name']}")
        if image["cigarette_decision"] == "NG" and not by_image[image_id]:
            raise PromotionError(f"NG image contains no defect boxes: {image['file_name']}")

    prediction_info = predictions.get("info")
    if (not isinstance(prediction_info, dict) or prediction_info.get("ground_truth_complete") is not False
            or prediction_info.get("accuracy_metrics_claimed") is not False
            or str(prediction_info.get("class_catalog_sha256", "")).lower() != catalog_sha256):
        raise PromotionError("prediction package provenance or class binding is invalid")
    prediction_images = require_list(predictions.get("images"), "prediction images")
    prediction_names = {item.get("file_name") for item in prediction_images if isinstance(item, dict)}
    if len(prediction_names) != len(prediction_images) or prediction_names != set(pilot):
        raise PromotionError("prediction images do not exactly match frozen pilot split")
    for image in prediction_images:
        record = pilot[image["file_name"]]
        for field in ("sha256", "width", "height", "source_group", "split", "authorization_status"):
            if image.get(field) != record.get(field):
                raise PromotionError(f"prediction image disagrees with manifest: {image['file_name']}/{field}")
        if image.get("is_ground_truth") is not False or image.get("annotation_status") != "preannotated":
            raise PromotionError(f"prediction image has invalid truth markers: {image['file_name']}")
    if predictions.get("categories") != expected_categories:
        raise PromotionError("prediction categories do not exactly match class catalog")
    for annotation in require_list(predictions.get("annotations"), "prediction annotations"):
        if (not isinstance(annotation, dict) or annotation.get("annotation_status") != "preannotated"
                or annotation.get("is_ground_truth") is not False or "score" not in annotation
                or "detector_version" not in annotation):
            raise PromotionError("prediction annotation provenance is incomplete")
    return review_ids, decisions


def promote_reviewed_truth(pass1_path: Path, manifest_path: Path, predictions_path: Path,
                           class_catalog_path: Path, output_dir: Path, annotated_by: str,
                           annotator_id: str, reviewed_by: str, reviewer_id: str,
                           approved_by: str, approver_id: str, approval_basis: str,
                           reviewed_at: str,
                           expected_pass1_sha256: str, expected_manifest_sha256: str,
                           expected_predictions_sha256: str,
                           expected_class_catalog_sha256: str,
                           attested_at: str | None = None) -> dict[str, Any]:
    reviewed_at = require_timestamp(reviewed_at, "reviewed_at")
    reviewed_instant = dt.datetime.fromisoformat(reviewed_at)
    source_export_instant = dt.datetime.fromtimestamp(
        pass1_path.stat().st_mtime_ns / 1_000_000_000,
        tz=reviewed_instant.tzinfo)
    if reviewed_instant != source_export_instant:
        raise PromotionError(
            "reviewed_at must equal the source pass1 final-export filesystem timestamp")
    attested_at = require_timestamp(
        attested_at or dt.datetime.now().astimezone().isoformat(timespec="seconds"), "attested_at")
    input_paths = {
        "source_pass1": pass1_path, "source_manifest": manifest_path,
        "source_predictions": predictions_path, "class_catalog": class_catalog_path,
    }
    expected = {
        "source_pass1": expected_pass1_sha256,
        "source_manifest": expected_manifest_sha256,
        "source_predictions": expected_predictions_sha256,
        "class_catalog": expected_class_catalog_sha256,
    }
    input_hashes = {name: require_sha256(path, expected[name], name)
                    for name, path in input_paths.items()}
    pass1 = read_json(pass1_path)
    source_manifest = read_json(manifest_path)
    source_predictions = read_json(predictions_path)
    catalog = read_json(class_catalog_path)
    dataset_tools.configure_classes(class_catalog_path)
    try:
        source_checks = {
            "pass1": dataset_tools.validate_annotations(
                pass1, source_manifest, require_reviewed=False, required_split="pilot"),
            "predictions": dataset_tools.validate_annotations(
                source_predictions, source_manifest, require_reviewed=False, required_split="pilot"),
        }
    except dataset_tools.DatasetError as exc:
        raise PromotionError(f"source dataset validation failed: {exc}") from exc
    for label, errors in source_checks.items():
        if errors:
            raise PromotionError(f"source {label} validation failed:\n- " + "\n- ".join(errors))
    review_ids, decisions = validate_inputs(
        pass1, source_manifest, source_predictions, catalog,
        input_hashes["class_catalog"], annotated_by, annotator_id,
        reviewed_by, reviewer_id, approved_by, approver_id, approval_basis)

    approved_manifest = copy.deepcopy(source_manifest)
    for record in approved_manifest["images"]:
        if record.get("canonical") is True and record.get("split") == "pilot":
            record["authorization_status"] = "approved"

    truth = copy.deepcopy(pass1)
    truth["info"].update({
        "description": "P5 approved human-double-reviewed pilot ground truth",
        "ground_truth_complete": True, "accuracy_metrics_claimed": False,
        "annotation_stage": "reviewed-ground-truth", "annotation_status": "reviewed",
        "authorization_status": "approved", "evaluation_split": "pilot", "source": "human",
        "annotators": [annotated_by], "annotator_ids": [annotator_id],
        "reviewers": [reviewed_by], "reviewer_ids": [reviewer_id],
        "reviewed_at": reviewed_at, "review_method": "human-double-review",
        "source_pass1_sha256": input_hashes["source_pass1"],
        "review_exclusion_policy": "REVIEW images remain reviewed members but have no formal GT boxes",
    })
    for image in truth["images"]:
        image.update({
            "annotation_status": "reviewed", "authorization_status": "approved",
            "is_ground_truth": True, "source": "human", "annotated_by": annotated_by,
            "annotator_id": annotator_id, "reviewed_by": reviewed_by,
            "reviewer_id": reviewer_id, "reviewed_at": reviewed_at,
        })
    retained = []
    for annotation in truth["annotations"]:
        if annotation["image_id"] in review_ids:
            continue
        annotation.update({
            "annotation_status": "reviewed", "is_ground_truth": True, "source": "human",
            "annotated_by": annotated_by, "annotator_id": annotator_id,
            "reviewed_by": reviewed_by, "reviewer_id": reviewer_id,
            "reviewed_at": reviewed_at,
        })
        retained.append(annotation)
    truth["annotations"] = retained

    evaluation_predictions = copy.deepcopy(source_predictions)
    for image in evaluation_predictions["images"]:
        image["authorization_status"] = "approved"
    evaluation_predictions["info"] = copy.deepcopy(evaluation_predictions["info"])
    evaluation_predictions["info"]["evaluation_authorization_note"] = (
        "pilot authorization synchronized to approved manifest; predictions unchanged")

    if output_dir.exists():
        raise PromotionError(f"refusing to overwrite existing output directory: {output_dir}")
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    staging = output_dir.parent / f".{output_dir.name}.{os.getpid()}.{uuid.uuid4().hex}.staging"
    staging.mkdir()
    try:
        paths = {
            "ground_truth": staging / "reviewed-ground-truth.coco.json",
            "manifest": staging / "approved-pilot-manifest.json",
            "predictions": staging / "evaluation-predictions.coco.json",
        }
        write_json(paths["ground_truth"], truth)
        write_json(paths["manifest"], approved_manifest)
        write_json(paths["predictions"], evaluation_predictions)

        dataset_tools.configure_classes(class_catalog_path)
        truth_errors = dataset_tools.validate_annotations(
            truth, approved_manifest, require_reviewed=True, required_split="pilot")
        if truth_errors:
            raise PromotionError("promoted truth validation failed:\n- " + "\n- ".join(truth_errors))
        prediction_errors = dataset_tools.validate_annotations(
            evaluation_predictions, approved_manifest, require_reviewed=False,
            required_split="pilot")
        if prediction_errors:
            raise PromotionError("evaluation prediction validation failed:\n- " +
                                 "\n- ".join(prediction_errors))

        hashes = {name: sha256_file(path) for name, path in paths.items()}
        attestation = {
            "schema_version": "p5-ground-truth-attestation-v1",
            "review_method": "human-double-review", "authorization_status": "approved",
            "evaluation_split": "pilot", "annotated_by": annotated_by,
            "annotator_id": annotator_id, "reviewed_by": reviewed_by,
            "reviewer_id": reviewer_id, "reviewed_at": reviewed_at,
            "ground_truth_sha256": hashes["ground_truth"],
            "manifest_sha256": hashes["manifest"],
            "class_catalog_sha256": input_hashes["class_catalog"],
            "attested_at": attested_at, "authorization_approved_by": approved_by,
            "approver_id": approver_id, "approval_basis": approval_basis,
            "review_process_note": (
                "annotator completed each page and a distinct reviewer checked each page; "
                "the legacy workbench retained only the annotator name"),
            "reviewed_at_basis": (
                "derived from the final pass1 export filesystem timestamp; "
                "review process and dataset approval attested by project owner"),
            "source_pass1_sha256": input_hashes["source_pass1"],
        }
        attestation_path = staging / "ground-truth-attestation.json"
        write_json(attestation_path, attestation)

        review_box_count = len(pass1["annotations"]) - len(retained)
        summary = {
            "schema_version": "p5-reviewed-truth-promotion-summary-v1", "status": "PASS",
            "evaluation_split": "pilot", "review_method": "human-double-review",
            "authorization_status": "approved", "annotated_by": annotated_by,
            "annotator_id": annotator_id, "reviewed_by": reviewed_by,
            "reviewer_id": reviewer_id, "reviewed_at": reviewed_at,
            "authorization_approved_by": approved_by, "approver_id": approver_id,
            "approval_basis": approval_basis, "attested_at": attested_at,
            "reviewed_image_count": len(truth["images"]),
            "comparable_image_count": len(truth["images"]) - len(review_ids),
            "review_excluded_image_count": len(review_ids),
            "decision_counts": dict(sorted(decisions.items())),
            "formal_ground_truth_box_count": len(retained),
            "review_reference_box_count_excluded": review_box_count,
            "review_reference_boxes_preserved_in": str(pass1_path.resolve()),
            "accuracy_metrics_claimed": False,
            "limitations": [
                "REVIEW images are excluded from metric denominators",
                "classes with zero comparable ground-truth support are not representative",
                "this frozen pilot must not be used for training or threshold tuning",
            ],
        }
        summary_path = staging / "promotion-summary.json"
        write_json(summary_path, summary)
        output_files = {
            "reviewed-ground-truth.coco.json": paths["ground_truth"],
            "approved-pilot-manifest.json": paths["manifest"],
            "evaluation-predictions.coco.json": paths["predictions"],
            "ground-truth-attestation.json": attestation_path,
            "promotion-summary.json": summary_path,
        }
        evidence = {
            "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
            "created_at": attested_at, "status": "PASS",
            "implementation": {
                "promotion_tool": {
                    "path": str(Path(__file__).resolve()),
                    "sha256": sha256_file(Path(__file__).resolve()),
                    "size_bytes": Path(__file__).resolve().stat().st_size,
                },
                "validator": {
                    "path": str(Path(dataset_tools.__file__).resolve()),
                    "sha256": sha256_file(Path(dataset_tools.__file__).resolve()),
                    "size_bytes": Path(dataset_tools.__file__).resolve().stat().st_size,
                },
            },
            "inputs": {name: {
                "path": str(path.resolve()), "sha256": input_hashes[name],
                "size_bytes": path.stat().st_size,
            } for name, path in input_paths.items()},
            "outputs": {name: {
                "sha256": sha256_file(path), "size_bytes": path.stat().st_size,
            } for name, path in output_files.items()},
            "validation": {"reviewed_truth": "PASS", "evaluation_predictions": "PASS",
                           "atomic_directory_promotion": True},
        }
        write_json(staging / "manifest.json", evidence)
        os.replace(staging, output_dir)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pass1", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--predictions", type=Path, required=True)
    parser.add_argument("--class-catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--annotated-by", required=True)
    parser.add_argument("--annotator-id", required=True)
    parser.add_argument("--reviewed-by", required=True)
    parser.add_argument("--reviewer-id", required=True)
    parser.add_argument("--approved-by", required=True)
    parser.add_argument("--approver-id", required=True)
    parser.add_argument("--approval-basis", required=True)
    parser.add_argument("--reviewed-at", required=True)
    parser.add_argument("--attested-at")
    parser.add_argument("--expected-pass1-sha256", required=True)
    parser.add_argument("--expected-manifest-sha256", required=True)
    parser.add_argument("--expected-predictions-sha256", required=True)
    parser.add_argument("--expected-class-catalog-sha256", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        summary = promote_reviewed_truth(
            args.pass1, args.manifest, args.predictions, args.class_catalog,
            args.output, args.annotated_by, args.annotator_id,
            args.reviewed_by, args.reviewer_id, args.approved_by,
            args.approver_id, args.approval_basis, args.reviewed_at,
            args.expected_pass1_sha256, args.expected_manifest_sha256,
            args.expected_predictions_sha256, args.expected_class_catalog_sha256,
            args.attested_at)
    except (PromotionError, dataset_tools.DatasetError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
