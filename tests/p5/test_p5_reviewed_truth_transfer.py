import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))
import p5_reviewed_truth_transfer as TRANSFER


def write_json(path, value):
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


def sha256_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def coco_categories(catalog):
    return [
        {
            "id": item["id"],
            "name": item["name"],
            "display_name_zh": item["displayNameZh"],
            "mapping_status": item["mappingStatus"],
            "supercategory": "cigarette_defect",
        }
        for item in catalog["classes"]
    ]


def file_record(path):
    return {"sha256": sha256_file(path), "size_bytes": path.stat().st_size}


def make_package(root):
    source = root / "source"
    source.mkdir()
    source_catalog_path = ROOT / "config" / "p5-class-catalog.json"
    catalog_path = root / "p5-class-catalog.json"
    catalog_path.write_bytes(source_catalog_path.read_bytes())
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    catalog_hash = sha256_file(catalog_path)
    reviewed_at = "2026-07-19T11:30:00+08:00"
    attested_at = "2026-07-19T12:00:00+08:00"
    common_identity = {
        "annotated_by": "标注员A",
        "annotator_id": "annotator-01",
        "reviewed_by": "复核员B",
        "reviewer_id": "reviewer-01",
        "reviewed_at": reviewed_at,
    }
    manifest_images = []
    truth_images = []
    prediction_images = []
    for image_id, decision in enumerate(("OK", "NG", "REVIEW"), 1):
        name = f"image-{image_id}.jpg"
        common = {
            "file_name": name,
            "sha256": f"{image_id:064x}",
            "width": 100,
            "height": 50,
            "source_group": "fixture",
            "split": "pilot",
            "authorization_status": "approved",
        }
        manifest_images.append(
            {
                **common,
                "relative_path": name,
                "canonical": True,
                "canonical_file_name": name,
            }
        )
        truth_images.append(
            {
                **common,
                **common_identity,
                "id": image_id,
                "annotation_status": "reviewed",
                "is_ground_truth": True,
                "source": "human",
                "cigarette_decision": decision,
            }
        )
        prediction_images.append(
            {
                **common,
                "id": image_id,
                "annotation_status": "preannotated",
                "is_ground_truth": False,
                "cigarette_decision": "REVIEW",
                "predicted_decision": "NG",
                "p4_result_present": True,
            }
        )
    approved_manifest = {
        "schema_version": "p5-dataset-manifest-v1",
        "images": manifest_images,
    }
    truth = {
        "info": {
            "schema_version": "p5-coco-v1",
            "annotation_stage": "reviewed-ground-truth",
            "annotation_status": "reviewed",
            "ground_truth_complete": True,
            "accuracy_metrics_claimed": False,
            "authorization_status": "approved",
            "evaluation_split": "pilot",
            "source": "human",
            "annotators": ["标注员A"],
            "annotator_ids": ["annotator-01"],
            "reviewers": ["复核员B"],
            "reviewer_ids": ["reviewer-01"],
            "reviewed_at": reviewed_at,
            "review_method": "human-double-review",
            "class_catalog_sha256": catalog_hash,
            "source_pass1_sha256": "a" * 64,
        },
        "images": truth_images,
        "annotations": [
            {
                "id": 1,
                "image_id": 2,
                "category_id": 1,
                "bbox": [1, 2, 10, 5],
                "area": 50,
                "iscrowd": 0,
                "annotation_status": "reviewed",
                "is_ground_truth": True,
                "source": "human",
                **common_identity,
            }
        ],
        "categories": coco_categories(catalog),
    }
    predictions = {
        "info": {
            "schema_version": "p5-coco-v1",
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
            "class_catalog_sha256": catalog_hash,
        },
        "images": prediction_images,
        "annotations": [
            {
                "id": 10,
                "image_id": 2,
                "category_id": 1,
                "bbox": [1, 2, 10, 5],
                "area": 50,
                "iscrowd": 0,
                "score": 0.8,
                "detector_version": "fixture",
                "annotation_status": "preannotated",
                "is_ground_truth": False,
                "source": "prediction",
            }
        ],
        "categories": coco_categories(catalog),
    }
    paths = {
        "reviewed-ground-truth.coco.json": source
        / "reviewed-ground-truth.coco.json",
        "approved-pilot-manifest.json": source / "approved-pilot-manifest.json",
        "evaluation-predictions.coco.json": source
        / "evaluation-predictions.coco.json",
        "ground-truth-attestation.json": source / "ground-truth-attestation.json",
        "promotion-summary.json": source / "promotion-summary.json",
    }
    write_json(paths["reviewed-ground-truth.coco.json"], truth)
    write_json(paths["approved-pilot-manifest.json"], approved_manifest)
    write_json(paths["evaluation-predictions.coco.json"], predictions)
    attestation = {
        "schema_version": "p5-ground-truth-attestation-v1",
        "review_method": "human-double-review",
        "authorization_status": "approved",
        "evaluation_split": "pilot",
        **common_identity,
        "ground_truth_sha256": sha256_file(
            paths["reviewed-ground-truth.coco.json"]
        ),
        "manifest_sha256": sha256_file(paths["approved-pilot-manifest.json"]),
        "class_catalog_sha256": catalog_hash,
        "attested_at": attested_at,
        "authorization_approved_by": "项目批准人",
        "approver_id": "approver-01",
        "approval_basis": "fixture-owner-approval",
        "source_pass1_sha256": "a" * 64,
    }
    write_json(paths["ground-truth-attestation.json"], attestation)
    summary = {
        "schema_version": "p5-reviewed-truth-promotion-summary-v1",
        "status": "PASS",
        "evaluation_split": "pilot",
        "review_method": "human-double-review",
        "authorization_status": "approved",
        **common_identity,
        "authorization_approved_by": "项目批准人",
        "approver_id": "approver-01",
        "approval_basis": "fixture-owner-approval",
        "attested_at": attested_at,
        "reviewed_image_count": 3,
        "comparable_image_count": 2,
        "review_excluded_image_count": 1,
        "decision_counts": {"NG": 1, "OK": 1, "REVIEW": 1},
        "formal_ground_truth_box_count": 1,
        "review_reference_box_count_excluded": 1,
        "review_reference_boxes_preserved_in": "C:/historical/pass1.json",
        "accuracy_metrics_claimed": False,
        "limitations": ["fixture"],
    }
    write_json(paths["promotion-summary.json"], summary)
    evidence = {
        "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
        "created_at": attested_at,
        "status": "PASS",
        "implementation": {
            "promotion_tool": {
                "path": "C:/historical/p5_promote_reviewed_truth.py",
                "sha256": "b" * 64,
                "size_bytes": 1,
            },
            "validator": {
                "path": "C:/historical/p5_dataset_tools.py",
                "sha256": "c" * 64,
                "size_bytes": 1,
            },
        },
        "inputs": {
            "source_pass1": {
                "path": "C:/historical/pass1.json",
                "sha256": "a" * 64,
                "size_bytes": 1,
            },
            "source_manifest": {
                "path": "C:/historical/pilot-manifest.json",
                "sha256": "d" * 64,
                "size_bytes": 1,
            },
            "source_predictions": {
                "path": "C:/historical/predictions.json",
                "sha256": "e" * 64,
                "size_bytes": 1,
            },
            "class_catalog": {
                "path": "C:/historical/p5-class-catalog.json",
                "sha256": catalog_hash,
                "size_bytes": catalog_path.stat().st_size,
            },
        },
        "outputs": {name: file_record(path) for name, path in paths.items()},
        "validation": {
            "reviewed_truth": "PASS",
            "evaluation_predictions": "PASS",
            "atomic_directory_promotion": True,
        },
    }
    write_json(source / "manifest.json", evidence)
    return source, catalog_path


def refresh_output_binding(source, name):
    manifest_path = source / "manifest.json"
    evidence = json.loads(manifest_path.read_text(encoding="utf-8"))
    evidence["outputs"][name] = file_record(source / name)
    write_json(manifest_path, evidence)


class ReviewedTruthTransferTests(unittest.TestCase):
    def test_verify_and_import_success_with_portable_complete_manifest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            verified = TRANSFER.verify_package(source, catalog)
            self.assertEqual("PASS", verified["status"])
            self.assertFalse(verified["accuracy_metrics_claimed"])
            self.assertEqual(3, verified["reviewed_image_count"])

            destination = root / "imported"
            result = TRANSFER.import_package(source, destination, catalog)
            self.assertEqual("PASS", result["status"])
            self.assertTrue(destination.is_dir())
            self.assertEqual(
                {
                    *TRANSFER.REQUIRED_OUTPUTS,
                    "source-manifest.json",
                    "transfer-receipt.json",
                    "manifest.json",
                },
                {item.name for item in destination.iterdir()},
            )
            imported = json.loads(
                (destination / "manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(TRANSFER.TRANSFER_SCHEMA, imported["schema_version"])
            self.assertFalse(imported["accuracy_metrics_claimed"])
            for record in imported["files"].values():
                self.assertFalse(Path(record["path"]).is_absolute())
                self.assertNotIn("..", Path(record["path"]).parts)
                path = destination / record["path"]
                self.assertEqual(record["sha256"], sha256_file(path))
                self.assertEqual(record["size_bytes"], path.stat().st_size)
            self.assertEqual(
                verified["source_manifest_sha256"],
                sha256_file(destination / "source-manifest.json"),
            )

    def test_cli_verify_and_import(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            destination = root / "cli-import"
            script = SCRIPTS / "p5_reviewed_truth_transfer.py"
            verify = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "verify",
                    "--source",
                    str(source),
                    "--class-catalog",
                    str(catalog),
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
                check=False,
            )
            self.assertEqual(0, verify.returncode, verify.stderr)
            self.assertEqual("PASS", json.loads(verify.stdout)["status"])
            imported = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "import",
                    "--source",
                    str(source),
                    "--destination",
                    str(destination),
                    "--class-catalog",
                    str(catalog),
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
                check=False,
            )
            self.assertEqual(0, imported.returncode, imported.stderr)
            self.assertEqual("PASS", json.loads(imported.stdout)["status"])
            self.assertTrue((destination / "transfer-receipt.json").is_file())

    def test_rejects_tampered_hash_and_size(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            truth = source / "reviewed-ground-truth.coco.json"
            truth.write_bytes(truth.read_bytes() + b" ")
            with self.assertRaisesRegex(TRANSFER.TransferError, "size mismatch"):
                TRANSFER.verify_package(source, catalog)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            evidence_path = source / "manifest.json"
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            record = evidence["outputs"]["reviewed-ground-truth.coco.json"]
            record["sha256"] = "0" * 64
            write_json(evidence_path, evidence)
            with self.assertRaisesRegex(TRANSFER.TransferError, "SHA-256 mismatch"):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_missing_and_extra_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            (source / "promotion-summary.json").unlink()
            with self.assertRaisesRegex(TRANSFER.TransferError, "missing required"):
                TRANSFER.verify_package(source, catalog)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            (source / "unbound.txt").write_text("unexpected", encoding="utf-8")
            with self.assertRaisesRegex(TRANSFER.TransferError, "unbound files"):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_manifest_path_escape(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            evidence_path = source / "manifest.json"
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
            record = evidence["outputs"].pop("promotion-summary.json")
            evidence["outputs"]["../promotion-summary.json"] = record
            write_json(evidence_path, evidence)
            with self.assertRaisesRegex(TRANSFER.TransferError, "exactly the five"):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_symlink_output_when_platform_supports_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            name = "promotion-summary.json"
            original = source / name
            outside = root / "outside.json"
            outside.write_bytes(original.read_bytes())
            original.unlink()
            try:
                original.symlink_to(outside)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable in this environment")
            with self.assertRaisesRegex(
                TRANSFER.TransferError, "symlink or reparse point"
            ):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_semantically_invalid_truth_with_fresh_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            truth_path = source / "reviewed-ground-truth.coco.json"
            truth = json.loads(truth_path.read_text(encoding="utf-8"))
            truth["images"][1]["cigarette_decision"] = "OK"
            write_json(truth_path, truth)
            attestation_path = source / "ground-truth-attestation.json"
            attestation = json.loads(attestation_path.read_text(encoding="utf-8"))
            attestation["ground_truth_sha256"] = sha256_file(truth_path)
            write_json(attestation_path, attestation)
            refresh_output_binding(source, "reviewed-ground-truth.coco.json")
            refresh_output_binding(source, "ground-truth-attestation.json")
            with self.assertRaisesRegex(
                TRANSFER.TransferError, "reviewed truth validation failed"
            ):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_prediction_promoted_to_ground_truth_with_fresh_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            predictions_path = source / "evaluation-predictions.coco.json"
            predictions = json.loads(predictions_path.read_text(encoding="utf-8"))
            predictions["images"][0]["annotation_status"] = "reviewed"
            predictions["images"][0]["is_ground_truth"] = True
            write_json(predictions_path, predictions)
            refresh_output_binding(source, "evaluation-predictions.coco.json")
            with self.assertRaisesRegex(
                TRANSFER.TransferError,
                "prediction images must be approved pilot preannotations",
            ):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_missing_or_invalid_implementation_binding(self):
        mutations = [
            (
                "missing",
                lambda evidence: evidence.pop("implementation"),
                "implementation",
            ),
            (
                "path",
                lambda evidence: evidence["implementation"]["promotion_tool"].update(
                    path="C:/historical/other.py"),
                "path",
            ),
            (
                "hash",
                lambda evidence: evidence["implementation"]["validator"].update(
                    sha256="not-a-hash"),
                "SHA-256",
            ),
            (
                "size",
                lambda evidence: evidence["implementation"]["validator"].update(
                    size_bytes=0),
                "size_bytes",
            ),
        ]
        for label, mutate, pattern in mutations:
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source, catalog = make_package(root)
                evidence_path = source / "manifest.json"
                evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
                mutate(evidence)
                write_json(evidence_path, evidence)
                with self.assertRaisesRegex(TRANSFER.TransferError, pattern):
                    TRANSFER.verify_package(source, catalog)

    def test_rejects_prediction_without_completed_inference_marker(self):
        for value in (None, False):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                source, catalog = make_package(root)
                predictions_path = source / "evaluation-predictions.coco.json"
                predictions = json.loads(predictions_path.read_text(encoding="utf-8"))
                if value is None:
                    predictions["images"][0].pop("p4_result_present")
                else:
                    predictions["images"][0]["p4_result_present"] = value
                write_json(predictions_path, predictions)
                refresh_output_binding(source, "evaluation-predictions.coco.json")
                with self.assertRaisesRegex(
                        TRANSFER.TransferError,
                        "approved pilot preannotations"):
                    TRANSFER.verify_package(source, catalog)

    def test_verify_rejects_source_drift_before_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            original_cross_bindings = TRANSFER._require_cross_bindings

            def mutate_after_semantic_validation(*args, **kwargs):
                result = original_cross_bindings(*args, **kwargs)
                predictions = source / "evaluation-predictions.coco.json"
                predictions.write_bytes(predictions.read_bytes() + b" ")
                return result

            with mock.patch.object(
                    TRANSFER,
                    "_require_cross_bindings",
                    side_effect=mutate_after_semantic_validation):
                with self.assertRaisesRegex(
                        TRANSFER.TransferError, "changed during verification"):
                    TRANSFER.verify_package(source, catalog)

    def test_verify_rejects_class_catalog_drift_before_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            original_cross_bindings = TRANSFER._require_cross_bindings

            def mutate_catalog_after_semantic_validation(*args, **kwargs):
                result = original_cross_bindings(*args, **kwargs)
                catalog.write_bytes(catalog.read_bytes() + b" ")
                return result

            with mock.patch.object(
                    TRANSFER,
                    "_require_cross_bindings",
                    side_effect=mutate_catalog_after_semantic_validation):
                with self.assertRaisesRegex(
                        TRANSFER.TransferError, "class catalog changed"):
                    TRANSFER.verify_package(source, catalog)

    def test_rejects_invalid_attestation_and_summary_identities(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            attestation_path = source / "ground-truth-attestation.json"
            attestation = json.loads(attestation_path.read_text(encoding="utf-8"))
            attestation["reviewer_id"] = " ANNOTATOR-01 "
            write_json(attestation_path, attestation)
            summary_path = source / "promotion-summary.json"
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            summary["reviewer_id"] = " ANNOTATOR-01 "
            write_json(summary_path, summary)
            refresh_output_binding(source, "ground-truth-attestation.json")
            refresh_output_binding(source, "promotion-summary.json")
            with self.assertRaisesRegex(
                TRANSFER.TransferError, "distinct annotator and reviewer"
            ):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_missing_explicit_approval_with_fresh_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            attestation_path = source / "ground-truth-attestation.json"
            attestation = json.loads(attestation_path.read_text(encoding="utf-8"))
            attestation["approval_basis"] = " "
            write_json(attestation_path, attestation)
            summary_path = source / "promotion-summary.json"
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            summary["approval_basis"] = " "
            write_json(summary_path, summary)
            refresh_output_binding(source, "ground-truth-attestation.json")
            refresh_output_binding(source, "promotion-summary.json")
            with self.assertRaisesRegex(TRANSFER.TransferError, "explicit approver"):
                TRANSFER.verify_package(source, catalog)

    def test_rejects_existing_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            destination = root / "existing"
            destination.mkdir()
            marker = destination / "keep.txt"
            marker.write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(TRANSFER.TransferError, "overwrite"):
                TRANSFER.import_package(source, destination, catalog)
            self.assertEqual("keep", marker.read_text(encoding="utf-8"))

    def test_copy_failure_leaves_no_destination_or_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, catalog = make_package(root)
            destination = root / "failed-import"
            original_copy = TRANSFER._copy_verified_file
            calls = 0

            def fail_after_one(source_path, destination_path):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("forced copy failure")
                original_copy(source_path, destination_path)

            with mock.patch.object(
                TRANSFER, "_copy_verified_file", side_effect=fail_after_one
            ):
                with self.assertRaisesRegex(OSError, "forced copy failure"):
                    TRANSFER.import_package(source, destination, catalog)
            self.assertFalse(destination.exists())
            self.assertFalse(
                any(
                    path.name.startswith(f".{destination.name}.")
                    and path.name.endswith(".staging")
                    for path in root.iterdir()
                )
            )


if __name__ == "__main__":
    unittest.main()
