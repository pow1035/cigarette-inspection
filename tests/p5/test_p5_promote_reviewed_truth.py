import datetime as dt
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))
import p5_promote_reviewed_truth as PROMOTION


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")


def categories(catalog):
    return [{
        "id": item["id"], "name": item["name"],
        "display_name_zh": item["displayNameZh"],
        "mapping_status": item["mappingStatus"],
        "supercategory": "cigarette_defect",
    } for item in catalog["classes"]]


def make_fixture(root):
    catalog = json.loads((ROOT / "config" / "p5-class-catalog.json").read_text(encoding="utf-8"))
    catalog_path = root / "catalog.json"
    write_json(catalog_path, catalog)
    catalog_hash = PROMOTION.sha256_file(catalog_path)
    records, images, prediction_images = [], [], []
    for image_id, decision in enumerate(["OK", "NG", "REVIEW"], 1):
        name = f"image-{image_id}.jpg"
        common = {
            "file_name": name, "sha256": f"{image_id:064x}", "width": 100,
            "height": 50, "source_group": "fixture", "split": "pilot",
            "authorization_status": "unverified",
        }
        records.append({**common, "relative_path": name, "canonical": True, "canonical_file_name": name})
        images.append({
            **common, "id": image_id, "annotation_status": "annotated",
            "is_ground_truth": False, "source": "human", "annotated_by": "标注员A",
            "cigarette_decision": decision,
            "notes": "待确认" if decision == "REVIEW" else "",
        })
        prediction_images.append({
            **common, "id": image_id, "annotation_status": "preannotated",
            "is_ground_truth": False, "cigarette_decision": "REVIEW",
            "predicted_decision": "NG", "p4_result_present": True,
        })
    records.append({
        "file_name": "unassigned.jpg", "relative_path": "unassigned.jpg",
        "sha256": "f" * 64, "width": 100, "height": 50,
        "source_group": "fixture", "split": "unassigned",
        "authorization_status": "unverified", "canonical": True, "canonical_file_name": "unassigned.jpg",
    })
    manifest = {"schema_version": "p5-dataset-manifest-v1", "images": records}
    pass1 = {
        "info": {
            "schema_version": "p5-coco-v1", "annotation_stage": "pass1",
            "annotation_status": "annotated", "ground_truth_complete": False,
            "accuracy_metrics_claimed": False, "source": "human",
            "annotators": ["标注员A"], "class_catalog_sha256": catalog_hash,
        },
        "images": images,
        "annotations": [
            {"id": 1, "image_id": 2, "category_id": 1, "bbox": [1, 2, 10, 5],
             "area": 50, "iscrowd": 0, "annotation_status": "annotated",
             "is_ground_truth": False, "source": "human", "annotated_by": "标注员A"},
            {"id": 2, "image_id": 3, "category_id": 7, "bbox": [2, 3, 8, 4],
             "area": 32, "iscrowd": 0, "annotation_status": "annotated",
             "is_ground_truth": False, "source": "human", "annotated_by": "标注员A"},
        ],
        "categories": categories(catalog),
    }
    predictions = {
        "info": {"schema_version": "p5-coco-v1", "ground_truth_complete": False,
                 "accuracy_metrics_claimed": False, "class_catalog_sha256": catalog_hash},
        "images": prediction_images,
        "annotations": [
            {"id": 10, "image_id": 2, "category_id": 1, "bbox": [1, 2, 10, 5],
             "area": 50, "iscrowd": 0, "score": 0.8,
             "detector_version": "fixture", "annotation_status": "preannotated",
             "is_ground_truth": False},
        ],
        "categories": categories(catalog),
    }
    paths = {"pass1": root / "pass1.json", "manifest": root / "manifest.json",
             "predictions": root / "predictions.json", "catalog": catalog_path}
    for name, value in (("pass1", pass1), ("manifest", manifest), ("predictions", predictions)):
        write_json(paths[name], value)
    hashes = {name: PROMOTION.sha256_file(path) for name, path in paths.items()}
    return paths, hashes, pass1, manifest, predictions


def promote(paths, hashes, output, **overrides):
    arguments = {
        "pass1_path": paths["pass1"], "manifest_path": paths["manifest"],
        "predictions_path": paths["predictions"], "class_catalog_path": paths["catalog"],
        "output_dir": output, "annotated_by": "标注员A",
        "annotator_id": "annotator-01", "reviewed_by": "复核员B",
        "reviewer_id": "reviewer-01", "approved_by": "项目批准人",
        "approver_id": "approver-01", "approval_basis": "fixture-owner-approval",
        "reviewed_at": dt.datetime.fromtimestamp(
            paths["pass1"].stat().st_mtime_ns / 1_000_000_000,
            tz=dt.timezone.utc).isoformat(),
        "expected_pass1_sha256": hashes["pass1"],
        "expected_manifest_sha256": hashes["manifest"],
        "expected_predictions_sha256": hashes["predictions"],
        "expected_class_catalog_sha256": hashes["catalog"],
        "attested_at": "2026-07-19T12:00:00+08:00",
    }
    arguments.update(overrides)
    return PROMOTION.promote_reviewed_truth(**arguments)

class PromotionTests(unittest.TestCase):
    def test_promotes_without_mutating_sources_or_prediction_boxes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, pass1, manifest, predictions = make_fixture(root)
            output = root / "output"
            summary = promote(paths, hashes, output)
            truth = json.loads((output / "reviewed-ground-truth.coco.json").read_text(encoding="utf-8"))
            approved = json.loads((output / "approved-pilot-manifest.json").read_text(encoding="utf-8"))
            evaluation = json.loads((output / "evaluation-predictions.coco.json").read_text(encoding="utf-8"))
            attestation = json.loads((output / "ground-truth-attestation.json").read_text(encoding="utf-8"))
            self.assertEqual(3, summary["reviewed_image_count"])
            self.assertEqual(2, summary["comparable_image_count"])
            self.assertEqual(1, summary["formal_ground_truth_box_count"])
            self.assertEqual([1], [item["id"] for item in truth["annotations"]])
            self.assertTrue(all(item["is_ground_truth"] for item in truth["images"]))
            self.assertTrue(all(item["reviewed_by"] == "复核员B" for item in truth["images"]))
            self.assertTrue(all(item["annotator_id"] == "annotator-01"
                                for item in truth["images"]))
            self.assertEqual("approver-01", attestation["approver_id"])
            self.assertEqual("approved", approved["images"][0]["authorization_status"])
            self.assertEqual("unverified", approved["images"][-1]["authorization_status"])
            self.assertEqual(predictions["annotations"], evaluation["annotations"])
            self.assertEqual(PROMOTION.sha256_file(output / "reviewed-ground-truth.coco.json"),
                             attestation["ground_truth_sha256"])
            self.assertEqual(pass1, json.loads(paths["pass1"].read_text(encoding="utf-8")))
            self.assertEqual(manifest, json.loads(paths["manifest"].read_text(encoding="utf-8")))
            self.assertEqual(hashes["predictions"], PROMOTION.sha256_file(paths["predictions"]))
            evidence = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(PROMOTION.sha256_file(Path(PROMOTION.__file__)),
                             evidence["implementation"]["promotion_tool"]["sha256"])
            self.assertEqual({
                "approved-pilot-manifest.json", "evaluation-predictions.coco.json",
                "ground-truth-attestation.json", "manifest.json",
                "promotion-summary.json", "reviewed-ground-truth.coco.json",
            }, {item.name for item in output.iterdir()})

    def test_rejects_same_people_without_partial_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            output = root / "output"
            with self.assertRaisesRegex(PROMOTION.PromotionError, "distinct"):
                promote(paths, hashes, output, reviewed_by=" 标注员Ａ ")
            self.assertFalse(output.exists())
            self.assertFalse(any(path.name.endswith(".staging") for path in root.iterdir()))

    def test_rejects_equivalent_stable_ids(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            output = root / "output"
            with self.assertRaisesRegex(PROMOTION.PromotionError, "stable ids"):
                promote(paths, hashes, output, reviewer_id=" ANNOTATOR-01 ")
            self.assertFalse(output.exists())

    def test_rejects_missing_explicit_approval(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            output = root / "output"
            with self.assertRaisesRegex(PROMOTION.PromotionError, "approval requires"):
                promote(paths, hashes, output, approval_basis=" ")
            self.assertFalse(output.exists())

    def test_rejects_input_hash_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            with self.assertRaisesRegex(PROMOTION.PromotionError, "SHA-256 drift"):
                promote(paths, hashes, root / "output", expected_pass1_sha256="0" * 64)

    def test_rejects_reviewed_time_not_equal_to_source_export_time(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            with self.assertRaisesRegex(PROMOTION.PromotionError, "final-export"):
                promote(paths, hashes, root / "output",
                        reviewed_at="2026-01-01T00:00:00+08:00")

    def test_rejects_subsecond_reviewed_time_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            exact = dt.datetime.fromtimestamp(
                paths["pass1"].stat().st_mtime_ns / 1_000_000_000,
                tz=dt.timezone.utc)
            drifted = (exact + dt.timedelta(milliseconds=250)).isoformat()
            with self.assertRaisesRegex(PROMOTION.PromotionError, "final-export"):
                promote(paths, hashes, root / "output", reviewed_at=drifted)

    def test_cleans_staging_after_post_write_validation_failure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, _, _, _ = make_fixture(root)
            output = root / "output"
            with patch.object(
                    PROMOTION.dataset_tools, "validate_annotations",
                    side_effect=[[], [], ["forced post-write failure"]]):
                with self.assertRaisesRegex(PROMOTION.PromotionError, "forced post-write"):
                    promote(paths, hashes, output)
            self.assertFalse(output.exists())
            self.assertFalse(any(path.name.endswith(".staging") for path in root.iterdir()))
    def test_rejects_malformed_review_reference_box(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, pass1, _, _ = make_fixture(root)
            pass1["annotations"][1]["bbox"] = [2, 3, 1000, 4]
            pass1["annotations"][1]["area"] = 4000
            write_json(paths["pass1"], pass1)
            hashes["pass1"] = PROMOTION.sha256_file(paths["pass1"])
            with self.assertRaisesRegex(PROMOTION.PromotionError, "source pass1 validation failed"):
                promote(paths, hashes, root / "output")
    def test_rejects_unconfirmed_class_outside_review(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths, hashes, pass1, _, _ = make_fixture(root)
            pass1["annotations"][0]["category_id"] = 7
            write_json(paths["pass1"], pass1)
            hashes["pass1"] = PROMOTION.sha256_file(paths["pass1"])
            with self.assertRaisesRegex(PROMOTION.PromotionError, "outside a REVIEW"):
                promote(paths, hashes, root / "output")


if __name__ == "__main__":
    unittest.main()
