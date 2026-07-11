import importlib.util
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("p5_dataset_tools", ROOT / "scripts" / "p5_dataset_tools.py")
TOOLS = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TOOLS)
TOOLS.configure_classes(ROOT / "config" / "p5-class-catalog.json")


def png_bytes(width=20, height=10, value=0):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\x00" + bytes([value]) * width for _ in range(height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def categories():
    return TOOLS.coco_categories()


def fixture_manifest(names=("a.png",)):
    return {"images": [
        {"file_name": name, "canonical": True, "canonical_file_name": name,
         "sha256": f"hash-{index}", "width": 100, "height": 50, "split": "test",
         "authorization_status": "approved"}
        for index, name in enumerate(names)
    ]}


def image(image_id, name, decision, status="reviewed", is_gt=True):
    value = {"id": image_id, "file_name": name, "width": 100, "height": 50,
             "sha256": f"hash-{image_id - 1}", "split": "test",
             "annotation_status": status, "cigarette_decision": decision,
             "is_ground_truth": is_gt,
             "authorization_status": "approved"}
    if status == "reviewed":
        value.update({"annotated_by": "annotator-a", "reviewed_by": "reviewer-b"})
    return value


def ann(annotation_id, image_id, category_id, bbox, status="reviewed", is_gt=True):
    value = {"id": annotation_id, "image_id": image_id, "category_id": category_id,
             "bbox": bbox, "area": bbox[2] * bbox[3], "iscrowd": 0,
             "annotation_status": status, "is_ground_truth": is_gt,
             "source": "human" if is_gt else "prediction"}
    if not is_gt:
        value.update({"score": 0.9, "detector_version": "synthetic"})
    return value


def dataset(images, annotations, complete=True):
    return {"info": {"ground_truth_complete": complete, "evaluation_split": "test",
                     "class_catalog_sha256": TOOLS.CLASS_CATALOG_SHA256}, "images": images,
            "annotations": annotations, "categories": categories()}


def evaluation_bindings():
    return {
        name: {"path": f"C:/evidence/{name}", "sha256": f"{index + 1:064x}"}
        for index, name in enumerate(sorted(TOOLS.REQUIRED_EVALUATION_BINDINGS))
    }


def ground_truth_attestation(bindings=None):
    bindings = bindings or evaluation_bindings()
    return {
        "schema_version": "p5-ground-truth-attestation-v1",
        "review_method": "human-double-review",
        "authorization_status": "approved",
        "evaluation_split": "test",
        "annotated_by": "annotator-a",
        "reviewed_by": "reviewer-b",
        "reviewed_at": "2026-07-11T16:00:00+08:00",
        "ground_truth_sha256": bindings["ground_truth"]["sha256"],
        "manifest_sha256": bindings["manifest"]["sha256"],
        "class_catalog_sha256": bindings["class_catalog"]["sha256"],
    }


def run_evaluate(truth, predictions, manifest, iou=0.5, split="test",
                 attestation=None, bindings=None):
    bindings = bindings or evaluation_bindings()
    attestation = attestation or ground_truth_attestation(bindings)
    return TOOLS.evaluate(truth, predictions, manifest, iou, attestation, bindings, split)


class AuditTests(unittest.TestCase):
    def test_duplicate_images_have_one_canonical_and_one_coco_image(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            payload = png_bytes()
            (root / "1_20240102030405006.png").write_bytes(payload)
            (root / "2_20240102030405007.png").write_bytes(payload)
            manifest, summary, template = TOOLS.audit_dataset(root)
            self.assertEqual(summary["image_count"], 2)
            self.assertEqual(summary["unique_sha256_count"], 1)
            self.assertEqual(summary["duplicate_group_count"], 1)
            self.assertEqual(sum(item["canonical"] for item in manifest["images"]), 1)
            self.assertEqual(len(template["images"]), 1)
            self.assertEqual(manifest["images"][0]["canonical_file_name"], manifest["images"][1]["canonical_file_name"])


class PilotSelectionTests(unittest.TestCase):
    @staticmethod
    def pilot_fixture(count=12):
        manifest_images = []
        images = []
        annotations = []
        annotation_id = 1
        for index in range(count):
            name = f"sample-{index:02d}.png"
            width, height = ((100, 50) if index % 2 == 0 else (120, 60))
            source_group = ("1", "21", "22", "unprefixed")[index % 4]
            decision = "OK" if index % 3 == 0 else "NG"
            manifest_images.append({
                "file_name": name, "canonical": True, "canonical_file_name": name,
                "sha256": f"{index:064x}", "width": width, "height": height,
                "source_group": source_group, "split": "unassigned",
                "authorization_status": "unverified",
            })
            images.append({
                "id": index + 1, "file_name": name, "sha256": f"{index:064x}",
                "width": width, "height": height, "source_group": source_group,
                "split": "unassigned", "annotation_status": "preannotated",
                "cigarette_decision": decision, "is_ground_truth": False,
                "authorization_status": "unverified", "p4_result_present": True,
            })
            if decision == "NG":
                category_id = (0, 1, 2, 3, 4, 5, 7)[(annotation_id - 1) % 7]
                annotations.append({
                    "id": annotation_id, "image_id": index + 1, "category_id": category_id,
                    "bbox": [1, 1, 10, 10], "area": 100, "iscrowd": 0,
                    "score": 0.8, "annotation_status": "preannotated",
                    "is_ground_truth": False, "detector_version": "fixture",
                })
                annotation_id += 1
        manifest = {"images": manifest_images}
        preannotations = dataset(images, annotations, complete=False)
        preannotations["info"]["evaluation_split"] = "unassigned"
        preannotations["info"]["accuracy_metrics_claimed"] = False
        return manifest, preannotations

    def test_selection_is_deterministic_and_canonical_only(self):
        manifest, preannotations = self.pilot_fixture()
        first = TOOLS.select_pilot(manifest, preannotations, size=10)
        second = TOOLS.select_pilot(manifest, preannotations, size=10)
        self.assertEqual(first[0], second[0])
        self.assertEqual(first[0]["selected_count"], 10)
        self.assertEqual(first[0]["uncovered_features"], [])
        self.assertEqual(len({item["sha256"] for item in first[0]["selected"]}), 10)

    def test_pilot_outputs_preserve_prediction_provenance_and_mark_unconfirmed_review(self):
        manifest, preannotations = self.pilot_fixture()
        selection, pilot_manifest, pilot_coco, rows = TOOLS.select_pilot(
            manifest, preannotations, size=10)
        self.assertFalse(pilot_coco["info"]["ground_truth_complete"])
        self.assertFalse(pilot_coco["info"]["accuracy_metrics_claimed"])
        self.assertTrue(all(image["is_ground_truth"] is False for image in pilot_coco["images"]))
        self.assertTrue(all(ann["is_ground_truth"] is False for ann in pilot_coco["annotations"]))
        selected_wuzi_ids = {
            ann["image_id"] for ann in pilot_coco["annotations"] if ann["category_id"] == 7
        }
        self.assertTrue(selected_wuzi_ids)
        self.assertTrue(all(
            image["cigarette_decision"] == "REVIEW"
            for image in pilot_coco["images"] if image["id"] in selected_wuzi_ids))
        self.assertTrue(all(image["split"] == "pilot" for image in pilot_coco["images"]))
        self.assertEqual(sum(item["split"] == "pilot" for item in pilot_manifest["images"]), 10)
        self.assertEqual(len(rows), selection["selected_count"])

    def test_selection_refuses_invalid_sizes_and_insufficient_coverage(self):
        manifest, preannotations = self.pilot_fixture()
        with self.assertRaisesRegex(TOOLS.DatasetError, "positive integer"):
            TOOLS.select_pilot(manifest, preannotations, size=0)
        with self.assertRaisesRegex(TOOLS.DatasetError, "exceeds"):
            TOOLS.select_pilot(manifest, preannotations, size=99)
        with self.assertRaisesRegex(TOOLS.DatasetError, "cannot cover"):
            TOOLS.select_pilot(manifest, preannotations, size=1)

    def test_selection_refuses_noncanonical_or_incomplete_preannotations(self):
        manifest, preannotations = self.pilot_fixture()
        preannotations["images"].pop()
        with self.assertRaisesRegex(TOOLS.DatasetError, "invalid|cover canonical manifest exactly"):
            TOOLS.select_pilot(manifest, preannotations, size=5)

    def test_selection_refuses_existing_split_and_forged_source_group(self):
        manifest, preannotations = self.pilot_fixture()
        manifest["images"][0]["split"] = "test"
        preannotations["images"][0]["split"] = "test"
        with self.assertRaisesRegex(TOOLS.DatasetError, "refuses to overwrite"):
            TOOLS.select_pilot(manifest, preannotations, size=10)
        manifest["images"][0]["split"] = "unassigned"
        preannotations["images"][0]["split"] = "unassigned"
        preannotations["images"][0]["source_group"] = "forged-source"
        with self.assertRaisesRegex(TOOLS.DatasetError, "source_group"):
            TOOLS.select_pilot(manifest, preannotations, size=10)

    def test_selection_refuses_duplicate_canonical_hash_and_dangling_alias(self):
        manifest, preannotations = self.pilot_fixture()
        manifest["images"][1]["sha256"] = manifest["images"][0]["sha256"]
        preannotations["images"][1]["sha256"] = manifest["images"][0]["sha256"]
        with self.assertRaisesRegex(TOOLS.DatasetError, "canonical SHA-256"):
            TOOLS.select_pilot(manifest, preannotations, size=10)
        manifest, preannotations = self.pilot_fixture()
        manifest["images"].append({
            "file_name": "alias.png", "canonical": False,
            "canonical_file_name": "missing.png", "sha256": "alias-hash",
            "width": 100, "height": 50, "source_group": "1",
            "split": "unassigned", "authorization_status": "unverified",
        })
        with self.assertRaisesRegex(TOOLS.DatasetError, "unknown canonical"):
            TOOLS.select_pilot(manifest, preannotations, size=10)

    def test_selection_refuses_truth_marked_input(self):
        manifest, preannotations = self.pilot_fixture()
        preannotations["images"][0]["annotation_status"] = "reviewed"
        preannotations["images"][0]["is_ground_truth"] = True
        with self.assertRaisesRegex(TOOLS.DatasetError, "non-prediction"):
            TOOLS.select_pilot(manifest, preannotations, size=10)
        manifest, preannotations = self.pilot_fixture()
        preannotations["annotations"][0]["score"] = True
        with self.assertRaisesRegex(TOOLS.DatasetError, "non-prediction"):
            TOOLS.select_pilot(manifest, preannotations, size=10)
        manifest, preannotations = self.pilot_fixture()
        preannotations["annotations"][0]["category_id"] = True
        with self.assertRaisesRegex(TOOLS.DatasetError, "invalid category_id"):
            TOOLS.select_pilot(manifest, preannotations, size=10)

    def test_preview_provenance_detects_semantic_mismatch(self):
        pilot = dataset(
            [{
                "id": 1, "file_name": "a.png", "width": 100, "height": 50,
                "sha256": "image-hash", "split": "pilot",
                "annotation_status": "preannotated", "cigarette_decision": "NG",
                "predicted_decision": "NG", "is_ground_truth": False,
                "authorization_status": "unverified", "source_group": "1",
            }],
            [{
                "id": 1, "image_id": 1, "category_id": 2, "bbox": [1, 2, 3, 4],
                "area": 12, "iscrowd": 0, "score": 0.8,
                "annotation_status": "preannotated", "is_ground_truth": False,
                "detector_version": "detector-v1",
            }], complete=False)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result_path = root / "frame-00000001.json"
            result_path.write_text(json.dumps({
                "sourceFile": "a.png", "width": 100, "height": 50,
                "decision": "NG", "parameterVersion": "detector-v1",
                "defects": [{
                    "classId": 2, "confidence": 0.8, "detectorVersion": "detector-v1",
                    "box": {"x": 1, "y": 2, "width": 3, "height": 4},
                }],
            }), encoding="utf-8")
            (root / "frame-00000001-annotated.png").write_bytes(png_bytes())
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertTrue(report["valid"])
            self.assertEqual(report["verified_record_count"], 1)
            value = json.loads(result_path.read_text(encoding="utf-8"))
            value["parameterVersion"] = "tampered-frame-version"
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("parameterVersion" in error for error in report["errors"]))
            value["parameterVersion"] = "detector-v1"
            value["defects"][0]["classId"] = 3
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("differs" in error for error in report["errors"]))
            pilot["annotations"][0]["category_id"] = 1
            pilot["annotations"][0]["score"] = 1.0
            value["defects"] = [{
                "classId": True, "confidence": True, "detectorVersion": "detector-v1",
                "box": {"x": 1, "y": 2, "width": 3, "height": 4},
            }]
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("differs" in error for error in report["errors"]))
            pilot["annotations"][0]["category_id"] = True
            value["defects"] = [{
                "classId": 1, "confidence": 1.0, "detectorVersion": "detector-v1",
                "box": {"x": 1, "y": 2, "width": 3, "height": 4},
            }]
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("differs" in error for error in report["errors"]))
            value["defects"] = [None]
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("not an object" in error for error in report["errors"]))
            value["defects"] = [{
                "classId": 2, "confidence": 0.8, "detectorVersion": "detector-v1",
                "box": {"x": None, "y": 2, "width": 3, "height": 4},
            }]
            result_path.write_text(json.dumps(value), encoding="utf-8")
            report = TOOLS.verify_pilot_previews(pilot, root)
            self.assertFalse(report["valid"])
            self.assertTrue(any("differs" in error for error in report["errors"]))


class ValidationTests(unittest.TestCase):
    def test_json_and_csv_writes_preserve_existing_file_when_replace_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            json_path = root / "value.json"
            csv_path = root / "value.csv"
            json_path.write_text("original-json", encoding="utf-8")
            csv_path.write_text("original-csv", encoding="utf-8")
            with mock.patch.object(TOOLS.os, "replace", side_effect=OSError("injected")):
                with self.assertRaisesRegex(OSError, "injected"):
                    TOOLS.write_json(json_path, {"changed": True})
                with self.assertRaisesRegex(OSError, "injected"):
                    TOOLS.write_review_csv(csv_path, [{"value": "changed"}])
            self.assertEqual(json_path.read_text(encoding="utf-8"), "original-json")
            self.assertEqual(csv_path.read_text(encoding="utf-8"), "original-csv")
            self.assertEqual(list(root.glob(".*.tmp")), [])

    def test_first_pass_annotated_status_is_valid_but_not_ground_truth(self):
        manifest = fixture_manifest()
        first_pass = dataset(
            [image(1, "a.png", "NG", "annotated", False)],
            [ann(1, 1, 0, [1, 1, 10, 10], "annotated", False)],
            complete=False,
        )
        self.assertEqual(
            TOOLS.validate_annotations(first_pass, manifest, require_reviewed=False), [])
        errors = TOOLS.validate_annotations(first_pass, manifest, require_reviewed=True)
        self.assertTrue(any("not fully reviewed" in error for error in errors))

        first_pass["images"][0]["is_ground_truth"] = True
        first_pass["annotations"][0]["is_ground_truth"] = True
        errors = TOOLS.validate_annotations(first_pass, manifest, require_reviewed=False)
        self.assertGreaterEqual(
            sum("ground-truth flag does not match" in error for error in errors), 2)

    def test_class_catalog_rejects_boolean_ids(self):
        catalog = json.loads((ROOT / "config" / "p5-class-catalog.json").read_text(encoding="utf-8"))
        catalog["classes"][0]["id"] = False
        catalog["classes"][1]["id"] = True
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "catalog.json"
            path.write_text(json.dumps(catalog), encoding="utf-8")
            with self.assertRaisesRegex(TOOLS.DatasetError, "strict integers"):
                TOOLS.configure_classes(path)

    def test_rejects_bbox_outside_image(self):
        manifest = fixture_manifest()
        value = dataset([image(1, "a.png", "NG")], [ann(1, 1, 0, [90, 10, 20, 10])])
        errors = TOOLS.validate_annotations(value, manifest, require_reviewed=True)
        self.assertTrue(any("outside image bounds" in error for error in errors))

    def test_evaluate_rejects_incomplete_ground_truth(self):
        manifest = fixture_manifest(("a.png", "b.png"))
        truth = dataset([image(1, "a.png", "OK")], [], complete=False)
        predictions = dataset([image(1, "a.png", "OK", "preannotated", False)], [], complete=False)
        with self.assertRaisesRegex(TOOLS.DatasetError, "ground truth is incomplete"):
            run_evaluate(truth, predictions, manifest)

    def test_reviewed_ground_truth_rejects_unconfirmed_class(self):
        manifest = fixture_manifest()
        value = dataset([image(1, "a.png", "NG")], [ann(1, 1, 7, [1, 1, 10, 10])])
        errors = TOOLS.validate_annotations(value, manifest, require_reviewed=True)
        self.assertTrue(any("unconfirmed class" in error for error in errors))

    def test_tiny_preannotation_rounding_is_clamped_but_large_overrun_is_not(self):
        tiny, changed = TOOLS.normalize_preannotation_bbox([90.0, 1.0, 10.00005, 5.0], 100, 50)
        self.assertTrue(changed)
        self.assertEqual(tiny[0] + tiny[2], 100.0)
        large, changed = TOOLS.normalize_preannotation_bbox([90.0, 1.0, 10.1, 5.0], 100, 50)
        self.assertFalse(changed)
        self.assertGreater(large[0] + large[2], 100.0)

    def test_rejects_area_that_does_not_match_bbox(self):
        manifest = fixture_manifest()
        annotation = ann(1, 1, 0, [1, 1, 10, 10])
        annotation["area"] = 99
        value = dataset([image(1, "a.png", "NG")], [annotation])
        errors = TOOLS.validate_annotations(value, manifest, require_reviewed=True)
        self.assertTrue(any("area does not match bbox" in error for error in errors))

    def test_evaluate_rejects_training_image_in_test_ground_truth(self):
        manifest = fixture_manifest()
        manifest["images"][0]["split"] = "train"
        truth = dataset([image(1, "a.png", "OK")], [])
        predictions = dataset([image(1, "a.png", "OK", "preannotated", False)], [], complete=False)
        with self.assertRaisesRegex(TOOLS.DatasetError, "evaluation split"):
            run_evaluate(truth, predictions, manifest, split="test")

    def test_prediction_markers_cannot_be_relabelled_as_ground_truth(self):
        manifest = fixture_manifest()
        reviewed_image = image(1, "a.png", "NG")
        reviewed_image["p4_result_present"] = True
        prediction_annotation = ann(1, 1, 1, [1, 1, 10, 10])
        prediction_annotation.update({
            "source": "prediction", "score": 0.99,
            "detector_version": "copied-prediction",
        })
        truth = dataset([reviewed_image], [prediction_annotation])
        errors = TOOLS.validate_annotations(truth, manifest, require_reviewed=True)
        self.assertGreaterEqual(sum("prediction provenance" in error for error in errors), 2)

    def test_attestation_hash_mismatch_refuses_evaluation(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "OK")], [])
        predictions = dataset([image(1, "a.png", "OK", "preannotated", False)], [], complete=False)
        bindings = evaluation_bindings()
        attestation = ground_truth_attestation(bindings)
        attestation["ground_truth_sha256"] = "0" * 64
        with self.assertRaisesRegex(TOOLS.DatasetError, "provenance is invalid"):
            run_evaluate(truth, predictions, manifest, attestation=attestation, bindings=bindings)

    def test_authorization_must_be_approved_in_manifest_and_ground_truth(self):
        manifest = fixture_manifest()
        manifest["images"][0]["authorization_status"] = "unverified"
        truth = dataset([image(1, "a.png", "OK")], [])
        errors = TOOLS.validate_annotations(truth, manifest, require_reviewed=True)
        self.assertTrue(any("authorization_status does not match manifest" in error for error in errors))

    def test_category_metadata_must_match_catalog(self):
        manifest = fixture_manifest()
        value = dataset([image(1, "a.png", "OK")], [])
        value["categories"][0]["mapping_status"] = "tampered"
        errors = TOOLS.validate_annotations(value, manifest, require_reviewed=True)
        self.assertTrue(any("mapping_status" in error for error in errors))

    def test_category_catalog_hash_must_match(self):
        manifest = fixture_manifest()
        value = dataset([image(1, "a.png", "OK")], [])
        value["info"]["class_catalog_sha256"] = "0" * 64
        errors = TOOLS.validate_annotations(value, manifest, require_reviewed=True)
        self.assertTrue(any("class_catalog_sha256" in error for error in errors))

    def test_missing_evidence_binding_refuses_evaluation(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "OK")], [])
        predictions = dataset([image(1, "a.png", "OK", "preannotated", False)], [], complete=False)
        bindings = evaluation_bindings()
        del bindings["engine"]
        with self.assertRaisesRegex(TOOLS.DatasetError, "provenance is invalid"):
            run_evaluate(
                truth, predictions, manifest,
                attestation=ground_truth_attestation(bindings), bindings=bindings)


class EvaluationTests(unittest.TestCase):
    def test_perfect_match(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "NG")], [ann(1, 1, 2, [10, 10, 20, 20])])
        predictions = dataset([image(1, "a.png", "NG", "preannotated", False)],
                              [ann(1, 1, 2, [10, 10, 20, 20], "preannotated", False)], complete=False)
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual((report["micro"]["tp"], report["micro"]["fp"], report["micro"]["fn"]), (1, 0, 0))
        self.assertEqual(report["micro"]["f1"], 1.0)
        self.assertEqual(set(report["evidence_bindings"]), TOOLS.REQUIRED_EVALUATION_BINDINGS)

    def test_wrong_class_populates_confusion_and_fp_fn(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "NG")], [ann(1, 1, 1, [10, 10, 20, 20])])
        predictions = dataset([image(1, "a.png", "NG", "preannotated", False)],
                              [ann(1, 1, 4, [10, 10, 20, 20], "preannotated", False)], complete=False)
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual(report["per_class"][1]["fn"], 1)
        self.assertEqual(report["per_class"][4]["fp"], 1)
        self.assertEqual(report["confusion_matrix"]["rows_true_columns_predicted"][1][4], 1)

    def test_unmatched_boxes_are_fp_and_fn(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "NG")], [ann(1, 1, 0, [0, 0, 10, 10])])
        predictions = dataset([image(1, "a.png", "NG", "preannotated", False)],
                              [ann(1, 1, 0, [80, 30, 10, 10], "preannotated", False)], complete=False)
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual((report["micro"]["tp"], report["micro"]["fp"], report["micro"]["fn"]), (0, 1, 1))
        names = report["confusion_matrix"]["class_names"]
        background = names.index("__background__")
        matrix = report["confusion_matrix"]["rows_true_columns_predicted"]
        self.assertEqual(matrix[0][background], 1)
        self.assertEqual(matrix[background][0], 1)

    def test_class_metrics_are_not_stolen_by_higher_iou_wrong_class(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "NG")], [ann(1, 1, 1, [10, 10, 20, 20])])
        predictions = dataset(
            [image(1, "a.png", "NG", "preannotated", False)],
            [
                ann(1, 1, 4, [10, 10, 20, 20], "preannotated", False),
                ann(2, 1, 1, [11, 10, 20, 20], "preannotated", False),
            ],
            complete=False,
        )
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual(report["per_class"][1]["tp"], 1)
        self.assertEqual(report["per_class"][1]["fn"], 0)
        matrix = report["confusion_matrix"]["rows_true_columns_predicted"]
        background = report["confusion_matrix"]["class_names"].index("__background__")
        self.assertEqual(matrix[1][1], 1)
        self.assertEqual(matrix[1][4], 0)
        self.assertEqual(matrix[background][4], 1)

    def test_review_images_are_representable_and_excluded(self):
        manifest = fixture_manifest(("a.png", "b.png"))
        truth = dataset([
            image(1, "a.png", "REVIEW"),
            image(2, "b.png", "OK"),
        ], [])
        predictions = dataset([
            image(1, "a.png", "OK", "preannotated", False),
            image(2, "b.png", "OK", "preannotated", False),
        ], [], complete=False)
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual(report["image_count"], 1)
        self.assertEqual(report["review_excluded_images"], ["a.png"])

    def test_active_union_macro_penalizes_fp_only_class(self):
        manifest = fixture_manifest()
        truth = dataset([image(1, "a.png", "NG")], [ann(1, 1, 1, [10, 10, 20, 20])])
        predictions = dataset(
            [image(1, "a.png", "NG", "preannotated", False)],
            [
                ann(1, 1, 1, [10, 10, 20, 20], "preannotated", False),
                ann(2, 1, 4, [70, 10, 10, 10], "preannotated", False),
            ],
            complete=False,
        )
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual(report["macro_ground_truth_present_classes"]["precision"], 1.0)
        self.assertEqual(report["macro_active_union_classes"]["precision"], 0.5)

    def test_cigarette_level_false_and_missed_ng(self):
        manifest = fixture_manifest(("a.png", "b.png"))
        truth = dataset([image(1, "a.png", "NG"), image(2, "b.png", "OK")],
                        [ann(1, 1, 3, [1, 1, 10, 10])])
        predictions = dataset([
            image(1, "a.png", "OK", "preannotated", False),
            image(2, "b.png", "NG", "preannotated", False),
        ], [ann(1, 2, 3, [1, 1, 10, 10], "preannotated", False)], complete=False)
        report = run_evaluate(truth, predictions, manifest)
        self.assertEqual(report["cigarette_level"]["missed_ng"], 1)
        self.assertEqual(report["cigarette_level"]["false_ng"], 1)
        self.assertEqual(report["cigarette_level"]["missed_ng_rate"], 1.0)
        self.assertEqual(report["cigarette_level"]["false_ng_rate"], 1.0)


if __name__ == "__main__":
    unittest.main()
