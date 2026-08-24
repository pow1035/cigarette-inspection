import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "production_acceptance_status", ROOT / "scripts" / "production_acceptance_status.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ProductionAcceptanceStatusTests(unittest.TestCase):
    def test_missing_external_inputs_is_blocked_and_prioritized(self):
        readiness = {
            "ready": False,
            "missing_required": ["reviewed_truth", "fallback_baseline"],
            "invalid_or_mismatched_required": [],
        }
        with mock.patch.object(MODULE, "_run_readiness", return_value=(2, readiness, "")):
            code, report = MODULE.build_status(ROOT)
        self.assertEqual(code, 2)
        self.assertEqual(report["status"], "blocked")
        self.assertEqual(report["next_actions"][0]["id"], "restore-controlled-p5-artifacts")
        self.assertFalse(report["production_acceptance_claimed"])

    def test_invalid_inputs_are_not_treated_as_missing(self):
        readiness = {
            "ready": False,
            "missing_required": [],
            "invalid_or_mismatched_required": ["reviewed_truth.manifest"],
        }
        with mock.patch.object(MODULE, "_run_readiness", return_value=(3, readiness, "")):
            code, report = MODULE.build_status(ROOT)
        self.assertEqual(code, 2)
        self.assertEqual(report["gates"][0]["status"], "blocked-invalid-inputs")
        self.assertEqual(report["gates"][0]["detail"]["missing_required"], [])

    def test_readiness_pass_does_not_close_external_gates(self):
        readiness = {"ready": True, "missing_required": [], "invalid_or_mismatched_required": []}
        with mock.patch.object(MODULE, "_run_readiness", return_value=(0, readiness, "")):
            code, report = MODULE.build_status(ROOT)
        self.assertEqual(code, 2)
        self.assertEqual(report["gates"][0]["status"], "pass")
        self.assertTrue(all(g["status"] == "blocked-unverified" for g in report["gates"][2:]))

    def test_cli_writes_machine_readable_output(self):
        readiness = {"ready": False, "missing_required": ["reviewed_truth", "fallback_baseline"],
                     "invalid_or_mismatched_required": []}
        with tempfile.TemporaryDirectory() as temp_dir, mock.patch.object(
                MODULE, "_run_readiness", return_value=(2, readiness, "")):
            output = Path(temp_dir) / "status.json"
            code = MODULE.main(["--repo-root", str(ROOT), "--output", str(output)])
            self.assertEqual(code, 2)
            parsed = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(parsed["schema_version"], MODULE.SCHEMA_VERSION)
            self.assertEqual(parsed["next_actions"][0]["id"], "restore-controlled-p5-artifacts")


if __name__ == "__main__":
    unittest.main()
