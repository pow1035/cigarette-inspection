import csv
import importlib.util
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))
import p5_exploratory_consistency as CONSISTENCY

SPEC = importlib.util.spec_from_file_location(
    "p5_visual_disagreement_pack", SCRIPTS / "p5_visual_disagreement_pack.py")
VISUAL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(VISUAL)


def categories_from_catalog(catalog):
    return [{
        "id": item["id"],
        "name": item["name"],
        "display_name_zh": item["displayNameZh"],
        "mapping_status": item["mappingStatus"],
        "supercategory": "cigarette_defect",
    } for item in catalog["classes"]]


def annotation(annotation_id, image_id, category_id, bbox, role):
    value = {
        "id": annotation_id,
        "image_id": image_id,
        "category_id": category_id,
        "bbox": bbox,
        "area": bbox[2] * bbox[3],
        "iscrowd": 0,
        "annotation_status": "preannotated" if role == "prediction" else "annotated",
        "is_ground_truth": False,
    }
    if role == "prediction":
        value.update({"score": 0.8, "detector_version": "fixture"})
    else:
        value.update({"source": "human", "annotated_by": "annotator-a"})
    return value


def make_fixture(root):
    catalog_path = root / "catalog.json"
    catalog = json.loads((ROOT / "config" / "p5-class-catalog.json").read_text(encoding="utf-8"))
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False), encoding="utf-8")
    catalog_hash = VISUAL.sha256_file(catalog_path)
    images_dir = root / "images"
    images_dir.mkdir()
    pairs = []
    decisions = [("NG", "OK"), ("NG", "REVIEW"), ("NG", "NG")]
    for image_id, (predicted_decision, human_decision) in enumerate(decisions, start=1):
        name = f"sample-{image_id}.jpg"
        Image.new("RGB", (100, 50), (30 * image_id, 40, 60)).save(images_dir / name, "JPEG", quality=95)
        source_hash = VISUAL.sha256_file(images_dir / name)
        common = {
            "id": image_id,
            "file_name": name,
            "width": 100,
            "height": 50,
            "sha256": source_hash,
            "is_ground_truth": False,
        }
        pairs.append((
            {
                **common,
                "annotation_status": "preannotated",
                "predicted_decision": predicted_decision,
                "cigarette_decision": "REVIEW",
            },
            {
                **common,
                "annotation_status": "annotated",
                "cigarette_decision": human_decision,
                "source": "human",
                "annotated_by": "annotator-a",
                "notes": "待确认" if human_decision == "REVIEW" else "",
            },
        ))
    categories = categories_from_catalog(catalog)
    predictions = {
        "info": {
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
            "class_catalog_sha256": catalog_hash,
        },
        "images": [pair[0] for pair in pairs],
        "annotations": [
            annotation(1, 1, 1, [1, 1, 10, 10], "prediction"),
            annotation(2, 3, 7, [20, 5, 10, 10], "prediction"),
        ],
        "categories": categories,
    }
    human = {
        "info": {
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
            "annotation_stage": "pass1",
            "class_catalog_sha256": catalog_hash,
        },
        "images": [pair[1] for pair in pairs],
        "annotations": [
            annotation(11, 2, 4, [2, 2, 8, 8], "human"),
            annotation(12, 3, 5, [20, 5, 10, 10], "human"),
        ],
        "categories": categories,
    }
    prediction_path = root / "predictions.json"
    human_path = root / "human.json"
    prediction_path.write_text(json.dumps(predictions, ensure_ascii=False), encoding="utf-8")
    human_path.write_text(json.dumps(human, ensure_ascii=False), encoding="utf-8")

    analysis = CONSISTENCY.analyze(predictions, human, 0.5)
    analysis_dir = root / "analysis"
    analysis_dir.mkdir()
    summary_path = analysis_dir / "summary.json"
    summary_path.write_text(json.dumps(analysis["summary"], ensure_ascii=False), encoding="utf-8")
    manifest = {
        "ground_truth_used": False,
        "formal_effect_measurement": False,
        "intended_use": "disagreement_triage_only",
        "inputs": {
            "predictions": {"sha256": VISUAL.sha256_file(prediction_path)},
            "human_reference": {"sha256": VISUAL.sha256_file(human_path)},
            "class_catalog": {"sha256": catalog_hash},
        },
        "outputs": {
            "summary.json": {
                "sha256": VISUAL.sha256_file(summary_path),
                "size_bytes": summary_path.stat().st_size,
            }
        },
    }
    (analysis_dir / "manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
    return prediction_path, human_path, catalog_path, analysis_dir, images_dir


def refresh_analysis_fixture(prediction_path, human_path, catalog_path, analysis_dir):
    predictions = json.loads(prediction_path.read_text(encoding="utf-8"))
    human = json.loads(human_path.read_text(encoding="utf-8"))
    analysis = CONSISTENCY.analyze(predictions, human, 0.5)
    summary_path = analysis_dir / "summary.json"
    summary_path.write_text(json.dumps(analysis["summary"], ensure_ascii=False), encoding="utf-8")
    manifest_path = analysis_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["inputs"]["predictions"]["sha256"] = VISUAL.sha256_file(prediction_path)
    manifest["inputs"]["human_reference"]["sha256"] = VISUAL.sha256_file(human_path)
    manifest["inputs"]["class_catalog"]["sha256"] = VISUAL.sha256_file(catalog_path)
    manifest["outputs"] = {
        "summary.json": {
            "sha256": VISUAL.sha256_file(summary_path),
            "size_bytes": summary_path.stat().st_size,
        }
    }
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")


class VisualDisagreementPackTests(unittest.TestCase):
    def test_build_pack_selects_union_and_binds_every_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            output_dir = root / "visual-pack"
            result = VISUAL.build_pack(
                prediction_path, human_path, catalog_path, analysis_dir,
                images_dir, output_dir,
            )
            self.assertEqual(result["summary"]["selected_image_count"], 3)
            self.assertEqual(result["summary"]["decision_different_image_count"], 1)
            self.assertEqual(result["summary"]["human_review_image_count"], 1)
            self.assertEqual(result["summary"]["class_changed_image_count"], 1)
            self.assertEqual(result["summary"]["class_changed_box_count"], 1)
            self.assertFalse(result["summary"]["ground_truth_used"])
            self.assertEqual(len(list((output_dir / "cases").glob("*.jpg"))), 3)
            with (output_dir / "case-index.csv").open(encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual([row["image_id"] for row in rows], ["1", "3", "2"])
            self.assertEqual([row["reason_codes"] for row in rows], [
                "decision_different", "class_changed", "human_review",
            ])
            manifest = json.loads((output_dir / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(len(manifest["source_images"]), 3)
            self.assertEqual(len(manifest["outputs"]), 7)
            for name, binding in manifest["outputs"].items():
                self.assertEqual(binding["sha256"], VISUAL.sha256_file(output_dir / name))
            with Image.open(output_dir / "cases" / "case-001.jpg") as image:
                self.assertEqual(image.size, (300, 160))
            combined = "\n".join(
                (output_dir / name).read_text(encoding="utf-8-sig").lower()
                for name in ("summary.json", "case-index.csv", "index.html", "README.md", "manifest.json")
            )
            for term in VISUAL.FORBIDDEN_OUTPUT_TERMS:
                self.assertNotIn(term.lower(), combined)

    def test_source_hash_mismatch_is_rejected_before_output_creation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            (images_dir / "sample-1.jpg").write_bytes(b"changed")
            output_dir = root / "visual-pack"
            with self.assertRaisesRegex(VISUAL.VisualPackError, "hash mismatch"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, output_dir,
                )
            self.assertFalse(output_dir.exists())

    def test_tampered_analysis_output_binding_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            (analysis_dir / "summary.json").write_text("{}", encoding="utf-8")
            with self.assertRaisesRegex(VISUAL.VisualPackError, "analysis"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, root / "visual-pack",
                )


    def test_bound_file_rejects_dotdot_absolute_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            base = root / "images"
            base.mkdir()
            inside = base / "inside.jpg"
            inside.write_bytes(b"inside")
            outside = root / "outside.jpg"
            outside.write_bytes(b"outside")
            self.assertEqual(
                VISUAL.resolve_bound_file(base, "inside.jpg", "fixture"), inside.resolve())
            with self.assertRaisesRegex(VISUAL.VisualPackError, "escapes"):
                VISUAL.resolve_bound_file(base, "../outside.jpg", "fixture")
            with self.assertRaisesRegex(VISUAL.VisualPackError, "escapes"):
                VISUAL.resolve_bound_file(base, str(outside.resolve()), "fixture")
            link = base / "outside-link.jpg"
            try:
                link.symlink_to(outside)
            except OSError:
                return
            with self.assertRaisesRegex(VISUAL.VisualPackError, "escapes"):
                VISUAL.resolve_bound_file(base, link.name, "fixture")

    def test_build_rejects_bound_source_path_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            outside = root / "outside.jpg"
            outside.write_bytes((images_dir / "sample-1.jpg").read_bytes())
            outside_hash = VISUAL.sha256_file(outside)
            for path in (prediction_path, human_path):
                payload = json.loads(path.read_text(encoding="utf-8"))
                payload["images"][0]["file_name"] = "../outside.jpg"
                payload["images"][0]["sha256"] = outside_hash
                path.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
            refresh_analysis_fixture(prediction_path, human_path, catalog_path, analysis_dir)
            output_dir = root / "visual-pack"
            with self.assertRaisesRegex(VISUAL.VisualPackError, "escapes"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, output_dir,
                )
            self.assertFalse(output_dir.exists())

    def test_analysis_output_path_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            outside = root / "outside.json"
            outside.write_text("{}", encoding="utf-8")
            manifest_path = analysis_dir / "manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["outputs"]["../outside.json"] = {
                "sha256": VISUAL.sha256_file(outside),
                "size_bytes": outside.stat().st_size,
            }
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(VISUAL.VisualPackError, "escapes"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, root / "visual-pack",
                )

    def test_mid_render_failure_removes_staging_and_final_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            output_dir = root / "visual-pack"
            real_render = VISUAL.render_case
            calls = 0

            def fail_second(*args, **kwargs):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise RuntimeError("injected render failure")
                return real_render(*args, **kwargs)

            with patch.object(VISUAL, "render_case", side_effect=fail_second):
                with self.assertRaisesRegex(RuntimeError, "injected"):
                    VISUAL.build_pack(
                        prediction_path, human_path, catalog_path, analysis_dir,
                        images_dir, output_dir,
                    )
            self.assertFalse(output_dir.exists())
            self.assertEqual(list(root.glob(".visual-pack.staging-*")), [])

    def test_bitmap_font_fallback_sanitizes_dynamic_unicode(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source_path = root / "source.jpg"
            Image.new("RGB", (100, 50), "#334455").save(source_path, "JPEG")
            output_path = root / "case.jpg"
            image_info = {
                "id": 1,
                "file_name": "中文图片.jpg",
                "sha256": VISUAL.sha256_file(source_path),
                "width": 100,
                "height": 50,
            }
            prediction = annotation(1, 1, 1, [2, 2, 20, 15], "prediction")
            match = {
                "result_type": "prediction_only",
                "prediction": prediction,
                "human": None,
            }
            categories = {1: {"id": 1, "name": "中文类别"}}
            with patch.object(VISUAL.ImageFont, "truetype", side_effect=ImportError("blocked")), \
                    patch.object(VISUAL.ImageFont, "load_default", side_effect=ImportError("blocked")):
                VISUAL.render_case(
                    source_path, image_info, [prediction], [], [match], categories,
                    ["decision_different"], None, output_path,
                )
            self.assertTrue(output_path.is_file())
            self.assertTrue(VISUAL.raster_ascii("中文图片.jpg").isascii())

    def test_input_hash_and_iou_mismatch_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            prediction_path.write_text("{}", encoding="utf-8")
            with self.assertRaisesRegex(VISUAL.VisualPackError, "analysis input binding mismatch"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, root / "hash-output",
                )
            self.assertFalse((root / "hash-output").exists())
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            with self.assertRaisesRegex(VISUAL.VisualPackError, "recomputed analysis"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, root / "iou-output", iou_threshold=0.6,
                )
            self.assertFalse((root / "iou-output").exists())

    def test_missing_and_dimension_mismatch_leave_no_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            (images_dir / "sample-1.jpg").unlink()
            output_dir = root / "missing-output"
            with self.assertRaisesRegex(VISUAL.VisualPackError, "missing"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, output_dir,
                )
            self.assertFalse(output_dir.exists())
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            source = images_dir / "sample-1.jpg"
            Image.new("RGB", (101, 50), "#223344").save(source, "JPEG")
            new_hash = VISUAL.sha256_file(source)
            for path in (prediction_path, human_path):
                payload = json.loads(path.read_text(encoding="utf-8"))
                payload["images"][0]["sha256"] = new_hash
                path.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
            refresh_analysis_fixture(prediction_path, human_path, catalog_path, analysis_dir)
            output_dir = root / "dimension-output"
            with self.assertRaisesRegex(VISUAL.VisualPackError, "dimensions mismatch"):
                VISUAL.build_pack(
                    prediction_path, human_path, catalog_path, analysis_dir,
                    images_dir, output_dir,
                )
            self.assertFalse(output_dir.exists())

    def test_html_escapes_dynamic_file_name_and_path(self):
        row = {
            "image_id": 1,
            "file_name": '<img src=x onerror="boom">',
            "visual_path": 'cases/x" onerror="boom.jpg',
            "reason_codes": "decision_different",
            "predicted_decision": "OK",
            "human_decision": "NG",
            "prediction_box_count": 0,
            "human_box_count": 1,
            "class_changed_box_count": 0,
        }
        summary = {
            "selected_image_count": 1,
            "decision_different_image_count": 1,
            "human_review_image_count": 0,
            "class_changed_box_count": 0,
        }
        output = VISUAL.render_html([row], summary)
        self.assertNotIn('<img src=x onerror="boom">', output)
        self.assertIn("&lt;img src=x onerror=&quot;boom&quot;&gt;", output)
        self.assertIn("cases/x&quot; onerror=&quot;boom.jpg", output)

    def test_no_selected_cases_is_rejected_before_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path, human_path, catalog_path, analysis_dir, images_dir = make_fixture(root)
            output_dir = root / "visual-pack"
            with patch.object(VISUAL, "select_cases", return_value=[]):
                with self.assertRaisesRegex(VISUAL.VisualPackError, "no disagreement cases"):
                    VISUAL.build_pack(
                        prediction_path, human_path, catalog_path, analysis_dir,
                        images_dir, output_dir,
                    )
            self.assertFalse(output_dir.exists())


if __name__ == "__main__":
    unittest.main()
