import copy
import csv
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "p5_exploratory_consistency", ROOT / "scripts" / "p5_exploratory_consistency.py")
ANALYSIS = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(ANALYSIS)


def fixture_categories():
    catalog = json.loads((ROOT / "config" / "p5-class-catalog.json").read_text(encoding="utf-8"))
    return [{
        "id": item["id"],
        "name": item["name"],
        "display_name_zh": item["displayNameZh"],
        "mapping_status": item["mappingStatus"],
        "supercategory": "cigarette_defect",
    } for item in catalog["classes"]]


def fixture_image(image_id, predicted_decision, human_decision):
    common = {
        "id": image_id,
        "file_name": f"sample-{image_id}.png",
        "width": 100,
        "height": 50,
        "sha256": f"{image_id:064x}",
        "is_ground_truth": False,
    }
    prediction = {
        **common,
        "annotation_status": "preannotated",
        "predicted_decision": predicted_decision,
        "cigarette_decision": "REVIEW",
    }
    human = {
        **common,
        "annotation_status": "annotated",
        "cigarette_decision": human_decision,
        "source": "human",
        "annotated_by": "annotator-a",
        "notes": "needs business clarification" if human_decision == "REVIEW" else "",
    }
    return prediction, human


def fixture_annotation(annotation_id, image_id, category_id, bbox, role):
    value = {
        "id": annotation_id,
        "image_id": image_id,
        "category_id": category_id,
        "bbox": bbox,
        "area": bbox[2] * bbox[3],
        "iscrowd": 0,
        "is_ground_truth": False,
        "annotation_status": "preannotated" if role == "prediction" else "annotated",
    }
    if role == "prediction":
        value.update({"score": 0.8, "detector_version": "fixture"})
    else:
        value.update({"source": "human", "annotated_by": "annotator-a"})
    return value


def fixture_datasets():
    pairs = [
        fixture_image(1, "NG", "NG"),
        fixture_image(2, "OK", "REVIEW"),
        fixture_image(3, "OK", "NG"),
    ]
    catalog_hash = ANALYSIS.sha256_file(ROOT / "config" / "p5-class-catalog.json")
    predictions = {
        "info": {
            "ground_truth_complete": False,
            "accuracy_metrics_claimed": False,
            "class_catalog_sha256": catalog_hash,
        },
        "images": [pair[0] for pair in pairs],
        "annotations": [
            fixture_annotation(1, 1, 1, [0, 0, 10, 10], "prediction"),
            fixture_annotation(2, 1, 7, [20, 0, 10, 10], "prediction"),
            fixture_annotation(3, 1, 3, [40, 0, 10, 10], "prediction"),
        ],
        "categories": fixture_categories(),
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
            fixture_annotation(11, 1, 1, [0, 0, 10, 10], "human"),
            fixture_annotation(12, 1, 5, [20, 0, 10, 10], "human"),
            fixture_annotation(13, 3, 4, [60, 0, 10, 10], "human"),
        ],
        "categories": fixture_categories(),
    }
    return predictions, human


class ExploratoryConsistencyTests(unittest.TestCase):
    def test_spatial_first_results_and_review_exclusion(self):
        predictions, human = fixture_datasets()
        result = ANALYSIS.analyze(predictions, human, 0.5)
        summary = result["summary"]
        self.assertFalse(summary["ground_truth_used"])
        self.assertFalse(summary["formal_effect_measurement"])
        self.assertEqual(summary["image_counts"], {
            "total": 3,
            "decision_same": 1,
            "decision_different": 1,
            "human_review_excluded": 1,
        })
        self.assertEqual(summary["box_counts"], {
            "prediction_total": 3,
            "human_reference_total": 3,
            "spatial_match_same_class": 1,
            "spatial_match_class_changed": 1,
            "prediction_only": 1,
            "human_only": 1,
            "result_rows": 4,
        })
        self.assertEqual(summary["class_changed"], [{
            "prediction_category_id": 7,
            "prediction_category_name": fixture_categories()[7]["name"],
            "human_category_id": 5,
            "human_category_name": fixture_categories()[5]["name"],
            "count": 1,
        }])
        self.assertEqual(len(result["image_rows"]), 3)
        self.assertEqual(len(result["review_rows"]), 1)

    def test_identity_mismatch_is_rejected(self):
        predictions, human = fixture_datasets()
        human["images"][0]["sha256"] = "f" * 64
        with self.assertRaisesRegex(ANALYSIS.InputContractError, "identity mismatch"):
            ANALYSIS.analyze(predictions, human)

    def test_ground_truth_claim_is_rejected(self):
        predictions, human = fixture_datasets()
        for mutation in ("info", "image", "annotation"):
            candidate = copy.deepcopy(human)
            if mutation == "info":
                candidate["info"]["ground_truth_complete"] = True
            elif mutation == "image":
                candidate["images"][0]["is_ground_truth"] = True
            else:
                candidate["annotations"][0]["is_ground_truth"] = True
            with self.subTest(mutation=mutation):
                with self.assertRaises(ANALYSIS.InputContractError):
                    ANALYSIS.analyze(predictions, candidate)

    def test_all_non_ground_truth_gates_are_explicit_on_both_inputs(self):
        predictions, human = fixture_datasets()
        mutations = []
        for role, base in (("predictions", predictions), ("human", human)):
            candidate = copy.deepcopy(base)
            candidate["info"]["ground_truth_complete"] = True
            mutations.append((f"{role}-complete", role, candidate))
            candidate = copy.deepcopy(base)
            candidate["info"]["accuracy_metrics_claimed"] = True
            mutations.append((f"{role}-effect-claim", role, candidate))
            candidate = copy.deepcopy(base)
            del candidate["images"][0]["is_ground_truth"]
            mutations.append((f"{role}-missing-image-flag", role, candidate))
            candidate = copy.deepcopy(base)
            del candidate["annotations"][0]["is_ground_truth"]
            mutations.append((f"{role}-missing-annotation-flag", role, candidate))
        candidate = copy.deepcopy(human)
        candidate["info"]["annotation_stage"] = "reviewed"
        mutations.append(("human-wrong-stage", "human", candidate))

        for name, role, candidate in mutations:
            with self.subTest(name=name):
                pred_input = candidate if role == "predictions" else predictions
                human_input = candidate if role == "human" else human
                with self.assertRaises(ANALYSIS.InputContractError):
                    ANALYSIS.analyze(pred_input, human_input)

    def test_role_status_and_human_source_are_strict(self):
        predictions, human = fixture_datasets()
        mutations = []
        for section in ("images", "annotations"):
            candidate = copy.deepcopy(predictions)
            candidate[section][0]["annotation_status"] = "annotated"
            mutations.append((f"prediction-{section}-status", "prediction", candidate))

            candidate = copy.deepcopy(human)
            candidate[section][0]["annotation_status"] = "preannotated"
            mutations.append((f"human-{section}-status", "human", candidate))
            candidate = copy.deepcopy(human)
            candidate[section][0]["source"] = "model"
            mutations.append((f"human-{section}-source", "human", candidate))

        for name, role, candidate in mutations:
            with self.subTest(name=name):
                pred_input = candidate if role == "prediction" else predictions
                human_input = candidate if role == "human" else human
                with self.assertRaises(ANALYSIS.InputContractError):
                    ANALYSIS.analyze(pred_input, human_input)

    def test_ok_and_ng_decisions_must_match_box_presence(self):
        predictions, human = fixture_datasets()
        mutations = []

        candidate = copy.deepcopy(predictions)
        candidate["images"][0]["predicted_decision"] = "OK"
        mutations.append(("prediction-ok-with-box", "prediction", candidate))
        candidate = copy.deepcopy(predictions)
        candidate["images"][1]["predicted_decision"] = "NG"
        mutations.append(("prediction-ng-without-box", "prediction", candidate))
        candidate = copy.deepcopy(human)
        candidate["images"][0]["cigarette_decision"] = "OK"
        mutations.append(("human-ok-with-box", "human", candidate))
        candidate = copy.deepcopy(human)
        candidate["images"][1]["cigarette_decision"] = "NG"
        mutations.append(("human-ng-without-box", "human", candidate))

        for name, role, candidate in mutations:
            with self.subTest(name=name):
                pred_input = candidate if role == "prediction" else predictions
                human_input = candidate if role == "human" else human
                with self.assertRaisesRegex(
                        ANALYSIS.InputContractError,
                        "OK but contains defect boxes|NG but has no defect boxes"):
                    ANALYSIS.analyze(pred_input, human_input)

        review_predictions = copy.deepcopy(predictions)
        review_predictions["images"][0]["predicted_decision"] = "REVIEW"
        review_human = copy.deepcopy(human)
        review_human["images"][0]["cigarette_decision"] = "REVIEW"
        ANALYSIS.analyze(review_predictions, review_human)

    def test_hash_bound_catalog_requires_all_coco_category_fields(self):
        predictions, human = fixture_datasets()
        mutations = {
            "id": 99,
            "name": "forged-name",
            "display_name_zh": "伪造类别",
            "mapping_status": "forged-status",
            "supercategory": "forged-supercategory",
        }
        for field, value in mutations.items():
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temp:
                pred_candidate = copy.deepcopy(predictions)
                human_candidate = copy.deepcopy(human)
                pred_candidate["categories"][0][field] = value
                human_candidate["categories"][0][field] = value
                root = Path(temp)
                prediction_path = root / "predictions.json"
                human_path = root / "human.json"
                prediction_path.write_text(
                    json.dumps(pred_candidate, ensure_ascii=False), encoding="utf-8")
                human_path.write_text(
                    json.dumps(human_candidate, ensure_ascii=False), encoding="utf-8")
                with self.assertRaisesRegex(
                        ANALYSIS.InputContractError, "class catalog"):
                    ANALYSIS.run(
                        prediction_path,
                        human_path,
                        ROOT / "config" / "p5-class-catalog.json",
                        root / "analysis",
                    )

    def test_iou_boundary_and_equal_iou_tie_are_deterministic(self):
        boundary = ANALYSIS.match_image_boxes(
            [fixture_annotation(1, 1, 1, [0, 0, 10, 10], "prediction")],
            [fixture_annotation(11, 1, 1, [0, 0, 5, 10], "human")],
            0.5,
        )
        self.assertEqual(len(boundary), 1)
        self.assertEqual(boundary[0]["result_type"], "spatial_match_same_class")
        self.assertEqual(boundary[0]["iou"], 0.5)

        tied = ANALYSIS.match_image_boxes(
            [
                fixture_annotation(2, 1, 1, [0, 0, 10, 10], "prediction"),
                fixture_annotation(1, 1, 1, [0, 0, 10, 10], "prediction"),
            ],
            [fixture_annotation(11, 1, 1, [0, 0, 10, 10], "human")],
            0.5,
        )
        matched = [row for row in tied if row["result_type"] == "spatial_match_same_class"]
        self.assertEqual(len(matched), 1)
        self.assertEqual(matched[0]["prediction"]["id"], 1)
        self.assertEqual(
            [row["prediction"]["id"] for row in tied if row["result_type"] == "prediction_only"],
            [2],
        )
    def test_run_writes_six_bound_outputs_without_formal_metric_terms(self):
        predictions, human = fixture_datasets()
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prediction_path = root / "predictions.json"
            human_path = root / "human.json"
            prediction_path.write_text(json.dumps(predictions), encoding="utf-8")
            human_path.write_text(json.dumps(human), encoding="utf-8")
            output_dir = root / "analysis"
            ANALYSIS.run(
                prediction_path,
                human_path,
                ROOT / "config" / "p5-class-catalog.json",
                output_dir,
                0.5,
            )
            expected = {
                "summary.json", "image-disagreements.csv", "box-matches.csv",
                "review-cases.csv", "report.md", "manifest.json",
            }
            self.assertEqual({path.name for path in output_dir.iterdir()}, expected)
            with (output_dir / "image-disagreements.csv").open(encoding="utf-8-sig", newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 3)
            with (output_dir / "review-cases.csv").open(encoding="utf-8-sig", newline="") as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 1)
            manifest = json.loads((output_dir / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(set(manifest["outputs"]), expected - {"manifest.json"})
            for name, binding in manifest["outputs"].items():
                self.assertEqual(binding["sha256"], ANALYSIS.sha256_file(output_dir / name))
            combined = "\n".join(
                path.read_text(encoding="utf-8-sig").lower()
                for path in output_dir.iterdir()
                if path.suffix in {".json", ".csv", ".md"}
            )
            for forbidden in ("precision", "recall", "false_positive", "false_negative"):
                self.assertNotIn(forbidden, combined)


if __name__ == "__main__":
    unittest.main()
