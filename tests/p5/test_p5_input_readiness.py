import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "p5_input_readiness", ROOT / "scripts" / "p5_input_readiness.py")
READY = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(READY)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class InputReadinessTests(unittest.TestCase):
    def test_fresh_checkout_reports_missing_required_external_inputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("{}", encoding="utf-8")
            report = READY.build_report(
                model=model,
                class_catalog=catalog,
                reviewed_truth=root / "reviewed",
                fallback_baseline=root / "fallback",
                require_reviewed=True,
                require_fallback=True,
                expected_model_sha256=digest(model),
            )
            self.assertFalse(report["ready"])
            self.assertEqual(report["missing_required_count"], 2)
            self.assertEqual(report["invalid_or_mismatched_required_count"], 0)

    def test_missing_base_inputs_are_invalid_not_external_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            report = READY.build_report(
                model=root / "model.onnx", class_catalog=root / "catalog.json",
                reviewed_truth=root / "reviewed", fallback_baseline=root / "fallback",
                require_reviewed=False, require_fallback=False)
            self.assertFalse(report["ready"])
            self.assertEqual(report["missing_required_count"], 0)
            self.assertIn("model", report["invalid_or_mismatched_required"])
            self.assertIn("class_catalog", report["invalid_or_mismatched_required"])

    def test_valid_reviewed_and_fallback_manifests_are_hash_checked(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "config" / "p5-class-catalog.json"
            model.write_bytes(b"model")
            catalog.parent.mkdir()
            catalog.write_text("catalog", encoding="utf-8")
            reviewed = root / "reviewed"
            output_names = list(READY.REVIEWED_OUTPUTS)
            output_hashes = {}
            for name in output_names:
                path = reviewed / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding="utf-8")
                output_hashes[name] = {"sha256": digest(path), "size_bytes": path.stat().st_size}
            attestation = reviewed / "ground-truth-attestation.json"
            # The attestation is also a manifest-bound output and cross-binds
            # the reviewed truth, approved pilot manifest and class catalog.
            write_json(attestation, {
                "schema_version": "p5-ground-truth-attestation-v1",
                "review_method": "human-double-review",
                "authorization_status": "approved",
                "evaluation_split": "pilot",
                "annotated_by": "annotator",
                "reviewed_by": "reviewer",
                "reviewed_at": "2026-07-19T12:00:00+08:00",
                "attested_at": "2026-07-19T12:01:00+08:00",
                "authorization_approved_by": "project-owner",
                "source_pass1_sha256": "1" * 64,
                "ground_truth_sha256": output_hashes["reviewed-ground-truth.coco.json"]["sha256"],
                "manifest_sha256": output_hashes["approved-pilot-manifest.json"]["sha256"],
                "class_catalog_sha256": digest(catalog),
            })
            output_hashes["ground-truth-attestation.json"] = {
                "sha256": digest(attestation), "size_bytes": attestation.stat().st_size,
            }
            write_json(reviewed / "manifest.json", {
                "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
                "outputs": output_hashes,
            })
            fallback = root / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(report_path), "size_bytes": report_path.stat().st_size,
                }},
            })
            result = READY.build_report(
                model=model,
                class_catalog=catalog,
                reviewed_truth=reviewed,
                fallback_baseline=fallback,
                require_reviewed=True,
                require_fallback=True,
                expected_model_sha256=digest(model),
            )
            self.assertTrue(result["ready"], result)
            self.assertEqual(result["invalid_or_mismatched_required_count"], 0)

    def test_attestation_catalog_hash_is_checked_for_custom_artifact_root(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "config" / "p5-class-catalog.json"
            model.write_bytes(b"model")
            catalog.parent.mkdir()
            catalog.write_text("catalog", encoding="utf-8")
            reviewed = root / "reviewed"
            output_hashes = {}
            for name in READY.REVIEWED_OUTPUTS:
                path = reviewed / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding="utf-8")
                output_hashes[name] = {"sha256": digest(path), "size_bytes": path.stat().st_size}
            attestation = reviewed / "ground-truth-attestation.json"
            write_json(attestation, {
                "schema_version": "p5-ground-truth-attestation-v1",
                "review_method": "human-double-review",
                "authorization_status": "approved",
                "evaluation_split": "pilot",
                "annotated_by": "annotator",
                "reviewed_by": "reviewer",
                "reviewed_at": "2026-07-19T12:00:00+08:00",
                "attested_at": "2026-07-19T12:01:00+08:00",
                "authorization_approved_by": "project-owner",
                "source_pass1_sha256": "1" * 64,
                "ground_truth_sha256": output_hashes["reviewed-ground-truth.coco.json"]["sha256"],
                "manifest_sha256": output_hashes["approved-pilot-manifest.json"]["sha256"],
                "class_catalog_sha256": "0" * 64,
            })
            output_hashes["ground-truth-attestation.json"] = {
                "sha256": digest(attestation), "size_bytes": attestation.stat().st_size,
            }
            write_json(reviewed / "manifest.json", {
                "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
                "outputs": output_hashes,
            })
            result = READY.build_report(
                model=model, class_catalog=catalog, reviewed_truth=reviewed,
                fallback_baseline=root / "fallback", require_reviewed=True,
                require_fallback=False, expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("reviewed_truth.attestation-class-catalog",
                          result["invalid_or_mismatched_required"])

    def test_attestation_ground_truth_binding_is_checked(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            reviewed = root / "reviewed"
            output_hashes = {}
            for name in READY.REVIEWED_OUTPUTS:
                path = reviewed / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding="utf-8")
                output_hashes[name] = {"sha256": digest(path), "size_bytes": path.stat().st_size}
            attestation = reviewed / "ground-truth-attestation.json"
            write_json(attestation, {
                "schema_version": "p5-ground-truth-attestation-v1",
                "review_method": "human-double-review",
                "authorization_status": "approved",
                "evaluation_split": "pilot",
                "annotated_by": "annotator",
                "reviewed_by": "reviewer",
                "reviewed_at": "2026-07-19T12:00:00+08:00",
                "attested_at": "2026-07-19T12:01:00+08:00",
                "authorization_approved_by": "project-owner",
                "source_pass1_sha256": "1" * 64,
                "ground_truth_sha256": "0" * 64,
                "manifest_sha256": output_hashes["approved-pilot-manifest.json"]["sha256"],
                "class_catalog_sha256": digest(catalog),
            })
            output_hashes["ground-truth-attestation.json"] = {
                "sha256": digest(attestation), "size_bytes": attestation.stat().st_size,
            }
            write_json(reviewed / "manifest.json", {
                "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
                "outputs": output_hashes,
            })
            result = READY.build_report(
                model=model, class_catalog=catalog, reviewed_truth=reviewed,
                fallback_baseline=root / "fallback", require_reviewed=True,
                require_fallback=False, expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("reviewed_truth.attestation-ground-truth",
                          result["invalid_or_mismatched_required"])

    def test_missing_reviewed_timestamp_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            reviewed = root / "reviewed"
            output_hashes = {}
            for name in READY.REVIEWED_OUTPUTS:
                path = reviewed / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name, encoding="utf-8")
                output_hashes[name] = {"sha256": digest(path), "size_bytes": path.stat().st_size}
            attestation = reviewed / "ground-truth-attestation.json"
            write_json(attestation, {
                "schema_version": "p5-ground-truth-attestation-v1",
                "review_method": "human-double-review",
                "authorization_status": "approved",
                "evaluation_split": "pilot",
                "annotated_by": "annotator",
                "reviewed_by": "reviewer",
                "attested_at": "2026-07-19T12:01:00+08:00",
                "authorization_approved_by": "project-owner",
                "source_pass1_sha256": "1" * 64,
                "ground_truth_sha256": output_hashes["reviewed-ground-truth.coco.json"]["sha256"],
                "manifest_sha256": output_hashes["approved-pilot-manifest.json"]["sha256"],
                "class_catalog_sha256": digest(catalog),
            })
            output_hashes["ground-truth-attestation.json"] = {
                "sha256": digest(attestation), "size_bytes": attestation.stat().st_size,
            }
            write_json(reviewed / "manifest.json", {
                "schema_version": "p5-reviewed-truth-evidence-manifest-v1",
                "outputs": output_hashes,
            })
            result = READY.build_report(
                model=model, class_catalog=catalog, reviewed_truth=reviewed,
                fallback_baseline=root / "fallback", require_reviewed=True,
                require_fallback=False, expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("reviewed_truth.attestation-reviewed-at",
                          result["invalid_or_mismatched_required"])

    def test_manifest_path_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {
                    "../escaped.json": {"sha256": digest(report_path)},
                    "evaluation-report.json": {
                        "sha256": digest(report_path), "size_bytes": report_path.stat().st_size,
                    },
                },
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("fallback_baseline.output.../escaped.json",
                          result["invalid_or_mismatched_required"])

    def test_symlinked_controlled_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            fallback.mkdir()
            outside = root / "outside-report.json"
            outside.write_text("report", encoding="utf-8")
            report_path = fallback / "evaluation-report.json"
            try:
                report_path.symlink_to(outside)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(outside), "size_bytes": outside.stat().st_size,
                }},
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("fallback_baseline.output.evaluation-report.json",
                          result["invalid_or_mismatched_required"])

    def test_broken_symlink_is_invalid_not_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            fallback.mkdir()
            report_path = fallback / "evaluation-report.json"
            try:
                report_path.symlink_to(root / "does-not-exist.json")
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": "0" * 64, "size_bytes": 1,
                }},
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline.output.evaluation-report.json",
                          result["invalid_or_mismatched_required"])

    def test_missing_output_size_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(report_path),
                }},
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline.output.evaluation-report.json",
                          result["invalid_or_mismatched_required"])

    def test_required_input_symlinks_are_invalid_not_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            real_model = root / "real-model.onnx"
            real_catalog = root / "real-catalog.json"
            real_model.write_bytes(b"model")
            real_catalog.write_text("catalog", encoding="utf-8")
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            try:
                model.symlink_to(real_model)
                catalog.symlink_to(real_catalog)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=root / "fallback",
                require_reviewed=False, require_fallback=False,
                expected_model_sha256=digest(real_model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("model", result["invalid_or_mismatched_required"])
            self.assertIn("class_catalog", result["invalid_or_mismatched_required"])

    def test_artifact_parent_symlink_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            real_artifacts = root / "real-artifacts"
            real_artifacts.mkdir()
            linked_artifacts = root / "artifacts"
            try:
                linked_artifacts.symlink_to(real_artifacts, target_is_directory=True)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            fallback = linked_artifacts / "fallback"
            fallback.mkdir()
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline", result["invalid_or_mismatched_required"])

    def test_deep_artifact_ancestor_symlink_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            outside = root / "outside"
            outside.mkdir()
            link = root / "link"
            try:
                link.symlink_to(outside, target_is_directory=True)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            fallback = link / "level1" / "level2" / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir(parents=True)
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(report_path), "size_bytes": report_path.stat().st_size,
                }},
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline", result["invalid_or_mismatched_required"])

    def test_symlink_then_parent_reference_cannot_hide_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            outside = root / "outside"
            target = outside / "target"
            target.mkdir(parents=True)
            link = root / "link"
            try:
                link.symlink_to(target, target_is_directory=True)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            fallback = outside / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(report_path), "size_bytes": report_path.stat().st_size,
                }},
            })
            disguised = root / "link" / ".." / "fallback"
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=disguised,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline", result["invalid_or_mismatched_required"])

    def test_tampered_output_is_a_mismatch_not_a_missing_input(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("original", encoding="utf-8")
            expected = digest(report_path)
            report_path.write_text("tampered", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "p5-fallback-evidence-v1",
                "outputs": {"evaluation-report.json": {
                    "sha256": expected, "size_bytes": len(b"original"),
                }},
            })
            result = READY.build_report(
                model=model,
                class_catalog=catalog,
                reviewed_truth=root / "reviewed",
                fallback_baseline=fallback,
                require_reviewed=False,
                require_fallback=True,
                expected_model_sha256=digest(model),
            )
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertGreater(result["invalid_or_mismatched_required_count"], 0)

    def test_wrong_manifest_schema_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            report_path = fallback / "evaluation-report.json"
            report_path.parent.mkdir()
            report_path.write_text("report", encoding="utf-8")
            write_json(fallback / "manifest.json", {
                "schema_version": "unexpected-schema",
                "outputs": {"evaluation-report.json": {
                    "sha256": digest(report_path), "size_bytes": report_path.stat().st_size,
                }},
            })
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertIn("fallback_baseline.manifest-schema",
                          result["invalid_or_mismatched_required"])

    def test_existing_artifact_scope_with_missing_manifest_is_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            fallback = root / "fallback"
            fallback.mkdir()
            result = READY.build_report(
                model=model, class_catalog=catalog,
                reviewed_truth=root / "reviewed", fallback_baseline=fallback,
                require_reviewed=False, require_fallback=True,
                expected_model_sha256=digest(model))
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("fallback_baseline.manifest", result["invalid_or_mismatched_required"])

    def test_missing_artifact_roots_blocked_by_file_ancestor_are_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            artifacts = root / "artifacts"
            artifacts.write_text("not a directory", encoding="utf-8")
            result = READY.build_report(
                model=model,
                class_catalog=catalog,
                reviewed_truth=artifacts / "reviewed",
                fallback_baseline=artifacts / "fallback",
                require_reviewed=True,
                require_fallback=True,
                expected_model_sha256=digest(model),
            )
            self.assertFalse(result["ready"])
            self.assertEqual(result["missing_required_count"], 0)
            self.assertIn("reviewed_truth", result["invalid_or_mismatched_required"])
            self.assertIn("fallback_baseline", result["invalid_or_mismatched_required"])

    def test_output_cannot_overwrite_model_or_class_catalog(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            before = {model: digest(model), catalog: digest(catalog)}
            for protected in (model, catalog):
                result = READY.main([
                    "--model", str(model),
                    "--class-catalog", str(catalog),
                    "--reviewed-truth-dir", str(root / "reviewed"),
                    "--fallback-baseline-dir", str(root / "fallback"),
                    "--output", str(protected),
                ])
                self.assertEqual(result, 3)
            self.assertEqual({path: digest(path) for path in before}, before)

    def test_output_cannot_overwrite_model_through_filesystem_aliases(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            catalog = root / "catalog.json"
            catalog.write_text("catalog", encoding="utf-8")
            aliases = (
                (root / "Protected.MODEL", root / "protected.model"),
                (root / "modél.onnx", root / "mode\u0301l.onnx"),
            )
            for model, output in aliases:
                with self.subTest(output=output.name):
                    model.write_bytes(b"model")
                    before = digest(model)
                    result = READY.main([
                        "--model", str(model),
                        "--class-catalog", str(catalog),
                        "--reviewed-truth-dir", str(root / "reviewed"),
                        "--fallback-baseline-dir", str(root / "fallback"),
                        "--output", str(output),
                    ])
                    self.assertEqual(result, 3)
                    self.assertEqual(digest(model), before)

    def test_output_cannot_create_files_inside_controlled_artifact_roots(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            reviewed = root / "reviewed"
            fallback = root / "fallback"
            for output in (reviewed / "readiness.json", fallback / "readiness.json"):
                result = READY.main([
                    "--model", str(model),
                    "--class-catalog", str(catalog),
                    "--reviewed-truth-dir", str(reviewed),
                    "--fallback-baseline-dir", str(fallback),
                    "--output", str(output),
                ])
                self.assertEqual(result, 3)
                self.assertFalse(output.exists())
            self.assertFalse(reviewed.exists())
            self.assertFalse(fallback.exists())

    def test_output_cannot_use_alias_of_missing_or_existing_artifact_root(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            aliases = (
                (root / "ControlledEvidence", root / "controlledevidence"),
                (root / "évidence", root / "e\u0301vidence"),
            )
            for reviewed, alias in aliases:
                with self.subTest(alias=alias.name, state="missing"):
                    output = alias / "readiness.json"
                    result = READY.main([
                        "--model", str(model),
                        "--class-catalog", str(catalog),
                        "--reviewed-truth-dir", str(reviewed),
                        "--fallback-baseline-dir", str(root / "fallback"),
                        "--output", str(output),
                    ])
                    self.assertEqual(result, 3)
                    self.assertFalse(output.exists())
                    self.assertFalse(reviewed.exists())
                    self.assertFalse(alias.exists())
                reviewed.mkdir()
                with self.subTest(alias=alias.name, state="existing"):
                    output = alias / "readiness-existing.json"
                    result = READY.main([
                        "--model", str(model),
                        "--class-catalog", str(catalog),
                        "--reviewed-truth-dir", str(reviewed),
                        "--fallback-baseline-dir", str(root / "fallback"),
                        "--output", str(output),
                    ])
                    self.assertEqual(result, 3)
                    self.assertFalse(output.exists())
                    self.assertEqual(list(reviewed.iterdir()), [])

    def test_output_cannot_occupy_artifact_root_ancestor(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            artifacts = root / "artifacts"
            result = READY.main([
                "--model", str(model),
                "--class-catalog", str(catalog),
                "--reviewed-truth-dir", str(artifacts / "reviewed"),
                "--fallback-baseline-dir", str(artifacts / "fallback"),
                "--output", str(artifacts),
            ])
            self.assertEqual(result, 3)
            self.assertFalse(artifacts.exists())

    def test_output_symlink_loop_is_rejected_with_cli_exit_three(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            model = root / "model.onnx"
            catalog = root / "catalog.json"
            model.write_bytes(b"model")
            catalog.write_text("catalog", encoding="utf-8")
            left = root / "left"
            right = root / "right"
            try:
                left.symlink_to(right, target_is_directory=True)
                right.symlink_to(left, target_is_directory=True)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            before = {model: digest(model), catalog: digest(catalog)}
            result = READY.main([
                "--model", str(model),
                "--class-catalog", str(catalog),
                "--reviewed-truth-dir", str(root / "reviewed"),
                "--fallback-baseline-dir", str(root / "fallback"),
                "--output", str(left / "readiness.json"),
            ])
            self.assertEqual(result, 3)
            self.assertEqual({path: digest(path) for path in before}, before)


if __name__ == "__main__":
    unittest.main()
