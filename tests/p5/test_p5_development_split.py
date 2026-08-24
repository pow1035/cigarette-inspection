import copy
import csv
import hashlib
import importlib.util
import json
import os
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = ROOT / "config" / "p5-class-catalog.json"
CATALOG = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
CATALOG_HASH = hashlib.sha256(CATALOG_PATH.read_bytes()).hexdigest()
SPEC = importlib.util.spec_from_file_location(
    "p5_development_split", ROOT / "scripts" / "p5_development_split.py")
TOOLS = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TOOLS)


def png_bytes(width=20, height=10, value=0):
    def chunk(kind, data):
        return (
            struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        )
    rows = b"".join(b"\x00" + bytes([value]) * width for _ in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def sha256(payload):
    return hashlib.sha256(payload).hexdigest()


def coco_categories():
    return [
        {
            "id": item["id"],
            "name": item["name"],
            "display_name_zh": item["displayNameZh"],
            "mapping_status": item["mappingStatus"],
            "supercategory": "cigarette_defect",
        }
        for item in CATALOG["classes"]
    ]


class Fixture:
    def __init__(self, root):
        self.root = Path(root)
        self.source = self.root / "source"
        self.source.mkdir()
        self.records = []
        self.images = []
        self.annotations = []
        self.next_image_id = 1
        self.next_annotation_id = 1

    def add(self, name, split, source_group="1", decision="NG", category_id=0,
            width=20, height=10, value=None):
        if value is None:
            value = self.next_image_id
        payload = png_bytes(width, height, value)
        (self.source / name).write_bytes(payload)
        digest = sha256(payload)
        image_id = self.next_image_id
        self.next_image_id += 1
        self.records.append({
            "file_name": name,
            "relative_path": name,
            "sha256": digest,
            "width": width,
            "height": height,
            "format": "PNG",
            "source_group": source_group,
            "canonical": True,
            "canonical_file_name": name,
            "duplicate_group": None,
            "split": split,
            "authorization_status": "unverified",
        })
        self.images.append({
            "id": image_id,
            "file_name": name,
            "sha256": digest,
            "width": width,
            "height": height,
            "source_group": source_group,
            "split": "unassigned",
            "annotation_status": "preannotated",
            "cigarette_decision": decision,
            "is_ground_truth": False,
            "authorization_status": "unverified",
            "p4_result_present": True,
        })
        if category_id is not None:
            self.annotations.append({
                "id": self.next_annotation_id,
                "image_id": image_id,
                "category_id": category_id,
                "bbox": [1, 1, 4, 4],
                "area": 16,
                "iscrowd": 0,
                "score": 0.8,
                "annotation_status": "preannotated",
                "is_ground_truth": False,
                "detector_version": "fixture-detector",
            })
            self.next_annotation_id += 1
        return name

    def alias(self, canonical_name, alias_name):
        owner = next(record for record in self.records
                     if record["file_name"] == canonical_name)
        (self.source / alias_name).write_bytes((self.source / canonical_name).read_bytes())
        alias = copy.deepcopy(owner)
        alias.update({
            "file_name": alias_name,
            "relative_path": alias_name,
            "canonical": False,
            "canonical_file_name": canonical_name,
            "duplicate_group": f"sha256:{owner['sha256']}",
        })
        owner["duplicate_group"] = alias["duplicate_group"]
        self.records.append(alias)

    def write(self):
        manifest = {
            "schema_version": "p5-dataset-manifest-v1",
            "images": self.records,
            "pilot_selection": {
                "algorithm": "fixture",
                "size": sum(record["canonical"] and record["split"] == "pilot"
                            for record in self.records),
            },
        }
        preannotations = {
            "info": {
                "schema_version": "p5-coco-v1",
                "ground_truth_complete": False,
                "accuracy_metrics_claimed": False,
                "class_catalog_sha256": CATALOG_HASH,
            },
            "images": self.images,
            "annotations": self.annotations,
            "categories": coco_categories(),
        }
        manifest_path = self.root / "pilot-manifest.json"
        preannotations_path = self.root / "preannotations.coco.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        preannotations_path.write_text(json.dumps(preannotations), encoding="utf-8")
        return manifest_path, preannotations_path


def standard_fixture(root):
    fixture = Fixture(root)
    fixture.add("1_20260101000000000.png", "pilot", source_group="21")
    fixture.add("1_20260101000010000.png", "pilot", source_group="21")
    fixture.add("1_20260101000100000.png", "unassigned", category_id=None)
    fixture.add("1_20260101000102000.png", "unassigned", category_id=1)
    fixture.add("1_20260101000200000.png", "unassigned")
    fixture.add("1_20260101000202000.png", "unassigned", decision="OK", category_id=None)
    fixture.add("1_20260101000300000.png", "unassigned", source_group="22")
    fixture.add("1_20260101000302000.png", "unassigned", source_group="22")
    fixture.alias("1_20260101000200000.png", "1_20260101000200001.png")
    return fixture


class DevelopmentSplitTests(unittest.TestCase):
    def build(self, fixture, output_name="package", validation_size=2,
              event_gap_ms=1000):
        manifest, preannotations = fixture.write()
        output = fixture.root / output_name
        report = TOOLS.build_package(
            manifest, preannotations, fixture.source, output,
            validation_size=validation_size, event_gap_ms=event_gap_ms)
        return output, report

    def test_success_outputs_predictions_only_atomic_review_package(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            output, report = self.build(fixture)
            self.assertEqual(report["train_count"], 4)
            self.assertEqual(report["validation_actual_size"], 2)
            expected = {
                "development-manifest.json",
                "train-preannotations.coco.json",
                "validation-preannotations.coco.json",
                "selection.json",
                "review.csv",
                "manifest.json",
            }
            self.assertTrue(expected <= {path.name for path in output.iterdir()})
            evidence = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertFalse(evidence["ground_truth"])
            self.assertFalse(evidence["accuracy_metrics_claimed"])
            self.assertFalse(evidence["training_complete"])
            self.assertEqual(evidence["human_review_status"], "pending")
            self.assertEqual(evidence["event_grouping"]["event_gap_ms"], 1000)
            self.assertEqual(
                len(list((output / "train" / "images").glob("*.png"))), 4)
            self.assertEqual(
                len(list((output / "validation" / "images").glob("*.png"))), 2)

    def test_deterministic_outputs_and_feature_balance(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            first, _ = self.build(fixture, "first")
            second, _ = self.build(fixture, "second")
            first_files = {
                path.relative_to(first).as_posix(): path.read_bytes()
                for path in first.rglob("*") if path.is_file()
            }
            second_files = {
                path.relative_to(second).as_posix(): path.read_bytes()
                for path in second.rglob("*") if path.is_file()
            }
            self.assertEqual(first_files, second_files)
            selection = json.loads((first / "selection.json").read_text(encoding="utf-8"))
            self.assertIn("source_group=1", selection["feature_balance"])
            self.assertIn("dimensions=20x10", selection["feature_balance"])
            self.assertIn("decision=NG", selection["feature_balance"])
            self.assertIn("class_id=0:dakoucuoya", selection["feature_balance"])

    def test_forged_or_incomplete_class_catalog_binding_is_rejected(self):
        mutations = [
            (
                "hash",
                lambda value: value["info"].update(
                    class_catalog_sha256="0" * 64),
                "class_catalog_sha256",
            ),
            (
                "name",
                lambda value: value["categories"][0].update(name="FORGED"),
                "does not match",
            ),
            (
                "display",
                lambda value: value["categories"][0].update(
                    display_name_zh="FORGED"),
                "does not match",
            ),
            (
                "mapping",
                lambda value: value["categories"][0].update(
                    mapping_status="forged"),
                "does not match",
            ),
            (
                "incomplete",
                lambda value: value["categories"].pop(),
                "full class catalog",
            ),
        ]
        for label, mutate, pattern in mutations:
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temp:
                fixture = standard_fixture(temp)
                manifest, preannotations = fixture.write()
                value = json.loads(preannotations.read_text(encoding="utf-8"))
                mutate(value)
                preannotations.write_text(json.dumps(value), encoding="utf-8")
                with self.assertRaisesRegex(TOOLS.DevelopmentSplitError, pattern):
                    TOOLS.build_package(
                        manifest,
                        preannotations,
                        fixture.source,
                        fixture.root / "package",
                        validation_size=2,
                    )

    def test_input_drift_before_publish_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            original_validate = TOOLS.validate_manifest

            def mutate_after_snapshot(value):
                result = original_validate(value)
                manifest.write_bytes(manifest.read_bytes() + b" ")
                return result

            with mock.patch.object(
                    TOOLS, "validate_manifest", side_effect=mutate_after_snapshot):
                with self.assertRaisesRegex(
                        TOOLS.DevelopmentSplitError, "changed during"):
                    TOOLS.build_package(
                        manifest,
                        preannotations,
                        fixture.source,
                        fixture.root / "package",
                        validation_size=2,
                    )
            self.assertFalse((fixture.root / "package").exists())
            self.assertEqual(list(fixture.root.glob(".package.staging-*")), [])

    def test_synchronized_eight_class_catalog_forgery_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            forged_catalog = fixture.root / "eight-class-catalog.json"
            catalog = copy.deepcopy(CATALOG)
            catalog["classes"].pop()
            forged_catalog.write_text(json.dumps(catalog), encoding="utf-8")
            value = json.loads(preannotations.read_text(encoding="utf-8"))
            value["categories"].pop()
            value["info"]["class_catalog_sha256"] = sha256(
                forged_catalog.read_bytes())
            preannotations.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOLS.DevelopmentSplitError, "exactly 9 classes"):
                TOOLS.build_package(
                    manifest,
                    preannotations,
                    fixture.source,
                    fixture.root / "package",
                    validation_size=2,
                    class_catalog_path=forged_catalog,
                )

    def test_equal_score_event_tie_break_is_sha256_then_file_name(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = Fixture(temp)
            fixture.add("1_20260101000000000.png", "pilot")
            names = [
                fixture.add(f"1_20260101000{index}00000.png", "unassigned")
                for index in range(1, 5)
            ]
            manifest, preannotations = fixture.write()
            output = fixture.root / "package"
            TOOLS.build_package(
                manifest, preannotations, fixture.source, output,
                validation_size=1)
            selection = json.loads(
                (output / "selection.json").read_text(encoding="utf-8"))
            expected = min(
                names,
                key=lambda name: (
                    next(record["sha256"] for record in fixture.records
                         if record["file_name"] == name),
                    name,
                ),
            )
            self.assertEqual(
                selection["validation_events"][0]["members"], [expected])
            self.assertEqual(
                selection["tie_break"],
                "event members ordered by sha256 then file_name")

    def test_pilot_never_leaks_and_pilot_only_feature_is_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            output, _ = self.build(fixture)
            manifest = json.loads(
                (output / "development-manifest.json").read_text(encoding="utf-8"))
            canonical = {item["file_name"]: item for item in manifest["images"]
                         if item["canonical"]}
            pilot = {name for name, item in canonical.items() if item["split"] == "pilot"}
            train = {name for name, item in canonical.items() if item["split"] == "train"}
            validation = {
                name for name, item in canonical.items() if item["split"] == "validation"
            }
            self.assertFalse(pilot & (train | validation))
            selection = json.loads((output / "selection.json").read_text(encoding="utf-8"))
            self.assertIn(
                "source_group=21",
                {item["feature"] for item in selection["pilot_only_features"]},
            )

    def test_duplicate_alias_inherits_canonical_split(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            output, _ = self.build(fixture)
            manifest = json.loads(
                (output / "development-manifest.json").read_text(encoding="utf-8"))
            records = {item["file_name"]: item for item in manifest["images"]}
            self.assertEqual(
                records["1_20260101000200001.png"]["split"],
                records["1_20260101000200000.png"]["split"],
            )
            self.assertEqual(
                records["1_20260101000200001.png"]["provisional_event_group_id"],
                records["1_20260101000200000.png"]["provisional_event_group_id"],
            )

    def test_pilot_adjacent_unassigned_event_is_excluded(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            adjacent = fixture.add(
                "1_20260101000000500.png", "unassigned", source_group="21")
            output, report = self.build(fixture)
            self.assertEqual(report["excluded_near_pilot_count"], 1)
            selection = json.loads((output / "selection.json").read_text(encoding="utf-8"))
            excluded = {
                name for event in selection["excluded_near_pilot"]
                for name in event["unassigned_members"]
            }
            self.assertEqual(excluded, {adjacent})
            manifest = json.loads(
                (output / "development-manifest.json").read_text(encoding="utf-8"))
            record = next(item for item in manifest["images"]
                          if item["file_name"] == adjacent)
            self.assertEqual(record["split"], "unassigned")
            with (output / "review.csv").open(encoding="utf-8", newline="") as handle:
                row = next(item for item in csv.DictReader(handle)
                           if item["file_name"] == adjacent)
            self.assertIn("pilot-adjacent", row["excluded_reason"])
            self.assertFalse((output / "train" / "images" / adjacent).exists())
            self.assertFalse((output / "validation" / "images" / adjacent).exists())

    def test_development_event_is_never_split(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            fixture.add("1_20260101000100500.png", "unassigned")
            output, _ = self.build(fixture, validation_size=3)
            manifest = json.loads(
                (output / "development-manifest.json").read_text(encoding="utf-8"))
            groups = {}
            for item in manifest["images"]:
                if item["canonical"] and item["split"] in {"train", "validation"}:
                    groups.setdefault(item["provisional_event_group_id"], set()).add(item["split"])
            self.assertTrue(groups)
            self.assertTrue(all(len(splits) == 1 for splits in groups.values()))

    def test_event_gap_boundary_is_inclusive_and_configurable(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = Fixture(temp)
            fixture.add("1_20260101000000000.png", "pilot")
            fixture.add("1_20260101000100000.png", "unassigned")
            fixture.add("1_20260101000101000.png", "unassigned")
            fixture.add("1_20260101000200000.png", "unassigned")
            fixture.add("1_20260101000300000.png", "unassigned")
            manifest, preannotations = fixture.write()
            groups, by_name, _ = TOOLS.build_event_groups(
                TOOLS.validate_manifest(json.loads(manifest.read_text()))[1], 1000)
            self.assertEqual(
                by_name["1_20260101000100000.png"],
                by_name["1_20260101000101000.png"],
            )
            _, by_name_short, _ = TOOLS.build_event_groups(
                TOOLS.validate_manifest(json.loads(manifest.read_text()))[1], 999)
            self.assertNotEqual(
                by_name_short["1_20260101000100000.png"],
                by_name_short["1_20260101000101000.png"],
            )
            output = fixture.root / "bad-gap"
            with self.assertRaisesRegex(TOOLS.DevelopmentSplitError, "positive integer"):
                TOOLS.build_package(
                    manifest, preannotations, fixture.source, output,
                    validation_size=1, event_gap_ms=0)

    def test_invalid_coverage_and_identity_are_rejected(self):
        mutations = [
            ("coverage", lambda value: value["images"].pop(), "cover canonical"),
            ("hash", lambda value: value["images"][0].update(sha256="0" * 64),
             "sha256"),
            ("dimension", lambda value: value["images"][0].update(width=999),
             "width"),
            ("source", lambda value: value["images"][0].update(source_group="forged"),
             "source_group"),
            ("split", lambda value: value["images"][2].update(split="validation"),
             "split"),
            ("truth", lambda value: value["images"][0].update(is_ground_truth=True),
             "ground truth"),
        ]
        for label, mutate, pattern in mutations:
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temp:
                fixture = standard_fixture(temp)
                manifest, preannotations = fixture.write()
                value = json.loads(preannotations.read_text(encoding="utf-8"))
                mutate(value)
                preannotations.write_text(json.dumps(value), encoding="utf-8")
                with self.assertRaisesRegex(
                        TOOLS.DevelopmentSplitError, pattern):
                    TOOLS.build_package(
                        manifest, preannotations, fixture.source,
                        fixture.root / "package", validation_size=2)

    def test_path_escape_and_symlink_escape_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["images"][2]["relative_path"] = "../outside.png"
            manifest.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(TOOLS.DevelopmentSplitError, "dot segments"):
                TOOLS.build_package(
                    manifest, preannotations, fixture.source,
                    fixture.root / "path-escape", validation_size=2)

        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            outside = fixture.root / "outside.png"
            outside.write_bytes(
                (fixture.source / fixture.records[2]["file_name"]).read_bytes())
            link = fixture.source / "escape.png"
            try:
                link.symlink_to(outside)
            except OSError as exc:
                original_resolve = Path.resolve

                def simulated_reparse_resolve(path, strict=False):
                    if path == link:
                        return outside.resolve()
                    return original_resolve(path, strict=strict)

                with mock.patch.object(
                        Path, "resolve", autospec=True,
                        side_effect=simulated_reparse_resolve):
                    with self.assertRaisesRegex(
                            TOOLS.DevelopmentSplitError, "escapes source root"):
                        TOOLS.resolve_source_path(
                            fixture.source.resolve(), "escape.png")
                return
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["images"][2]["relative_path"] = "escape.png"
            manifest.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(
                    TOOLS.DevelopmentSplitError, "escapes source root"):
                TOOLS.build_package(
                    manifest, preannotations, fixture.source,
                    fixture.root / "symlink-escape", validation_size=2)

    def test_source_tamper_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            (fixture.source / fixture.records[2]["file_name"]).write_bytes(
                png_bytes(value=250))
            with self.assertRaisesRegex(TOOLS.DevelopmentSplitError, "SHA-256 mismatch"):
                TOOLS.build_package(
                    manifest, preannotations, fixture.source,
                    fixture.root / "package", validation_size=2)

    def test_existing_destination_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            output = fixture.root / "package"
            output.mkdir()
            marker = output / "keep.txt"
            marker.write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(TOOLS.DevelopmentSplitError, "already exists"):
                TOOLS.build_package(
                    manifest, preannotations, fixture.source, output,
                    validation_size=2)
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_failure_removes_staging_and_leaves_no_half_package(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            output = fixture.root / "package"
            original = TOOLS.copy_verified_source
            calls = 0

            def fail_after_one(*args, **kwargs):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("injected copy failure")
                return original(*args, **kwargs)

            with mock.patch.object(
                    TOOLS, "copy_verified_source", side_effect=fail_after_one):
                with self.assertRaisesRegex(OSError, "injected"):
                    TOOLS.build_package(
                        manifest, preannotations, fixture.source, output,
                        validation_size=2)
            self.assertFalse(output.exists())
            self.assertEqual(list(fixture.root.glob(".package.staging-*")), [])

    def test_validation_size_boundaries_and_nearest_event_size(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            manifest, preannotations = fixture.write()
            for bad in (0, 6):
                with self.subTest(bad=bad), self.assertRaisesRegex(
                        TOOLS.DevelopmentSplitError, "between|positive integer"):
                    TOOLS.build_package(
                        manifest, preannotations, fixture.source,
                        fixture.root / f"bad-{bad}", validation_size=bad)
            one = fixture.root / "one"
            report = TOOLS.build_package(
                manifest, preannotations, fixture.source, one, validation_size=1)
            self.assertEqual(report["validation_actual_size"], 1)

        with tempfile.TemporaryDirectory() as temp:
            fixture = Fixture(temp)
            fixture.add("1_20260101000000000.png", "pilot")
            fixture.add("1_20260101000100000.png", "unassigned")
            fixture.add("1_20260101000100500.png", "unassigned")
            fixture.add("1_20260101000200000.png", "unassigned")
            fixture.add("1_20260101000200500.png", "unassigned")
            manifest, preannotations = fixture.write()
            nearest = fixture.root / "nearest"
            report = TOOLS.build_package(
                manifest, preannotations, fixture.source, nearest,
                validation_size=1)
            self.assertEqual(report["validation_actual_size"], 2)
            selection = json.loads(
                (nearest / "selection.json").read_text(encoding="utf-8"))
            self.assertTrue(any("closest deterministic size" in item
                                for item in selection["limitations"]))

    def test_unparseable_timestamp_uses_independent_event_and_is_recorded(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = standard_fixture(temp)
            name = fixture.add("no-timestamp.png", "unassigned")
            output, _ = self.build(fixture)
            selection = json.loads((output / "selection.json").read_text(encoding="utf-8"))
            self.assertIn(
                name,
                {item["file_name"] for item in selection["unparseable_timestamps"]},
            )
            manifest = json.loads(
                (output / "development-manifest.json").read_text(encoding="utf-8"))
            record = next(item for item in manifest["images"]
                          if item["file_name"] == name)
            self.assertIn(":unparsed:", record["provisional_event_group_id"])


if __name__ == "__main__":
    unittest.main()
