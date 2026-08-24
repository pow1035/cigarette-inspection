#!/usr/bin/env python3
"""Tests for the target-machine P6 evidence orchestration."""

from __future__ import annotations

import importlib.util
import io
import json
import os
import shutil
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "p6_windows_simulation_evidence.py"
FAKE_RUNTIME = Path(__file__).with_name("fake_cigvision_runtime.py")
SPEC = importlib.util.spec_from_file_location("p6_windows_simulation_evidence", SCRIPT_PATH)
assert SPEC and SPEC.loader
EVIDENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVIDENCE)


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def run_cli(arguments: list[str]) -> int:
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return EVIDENCE.main(arguments)


class P6WindowsSimulationEvidenceTests(unittest.TestCase):
    def make_fixture(self, root: Path, *, reject_enabled: str = "false") -> tuple[Path, Path, Path]:
        images = root / "images"
        images.mkdir()
        first = images / "a.bin"
        second = images / "b.bin"
        first.write_bytes(b"first-image")
        second.write_bytes(b"second-image")
        import hashlib

        manifest = root / "manifest.json"
        write_json(manifest, {
            "root": "images",
            "samples": [
                {
                    "path": "a.bin",
                    "sha256": hashlib.sha256(first.read_bytes()).hexdigest().upper(),
                    "expected": "OK",
                    "stationId": "station-a",
                    "cameraId": "camera-a",
                    "cigaretteNumber": 7001,
                    "delayBeforeMicros": 0,
                },
                {
                    "path": "b.bin",
                    "sha256": hashlib.sha256(second.read_bytes()).hexdigest().upper(),
                    "expected": "NG",
                    "stationId": "station-b",
                    "cameraId": "camera-b",
                    "cigaretteNumber": 7002,
                    "delayBeforeMicros": 250,
                },
            ],
        })
        config = root / "config.ini"
        config.write_text(
            "[SystemParams]\nrejectEnabled=" + reject_enabled + "\n",
            encoding="utf-8",
        )
        runner = root / "CigVision.py"
        shutil.copy2(FAKE_RUNTIME, runner)
        runner.chmod(0o755)
        return manifest, config, runner

    def args(self, manifest: Path, config: Path, runner: Path, evidence_root: Path) -> list[str]:
        return [
            "--executable", str(runner),
            "--manifest", str(manifest),
            "--config-ini", str(config),
            "--evidence-root", str(evidence_root),
            "--reject-delay-micros", "500",
            "--queue-capacity", "4",
            "--target-output", "simulation-reject",
            "--allow-non-windows-test",
        ]

    def test_success_collects_runtime_and_negative_matrix(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root)
            evidence_root = root / "evidence"
            status = run_cli(self.args(manifest, config, runner, evidence_root))
            self.assertEqual(status, 0)
            report = json.loads((evidence_root / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(report["overallResult"], "passed-test-only")
            self.assertFalse(report["targetWindowsRuntimeVerified"])
            self.assertTrue(report["nonWindowsTestOnly"])
            self.assertFalse(report["realIoEnabled"])
            self.assertEqual(report["failures"], [])
            self.assertEqual(report["counts"]["manifestSamples"], 2)
            self.assertEqual(report["counts"]["frameJson"], 2)
            self.assertEqual(report["counts"]["framePng"], 2)
            self.assertTrue((evidence_root / "trace-preflight.json").is_file())
            negative_steps = {
                step["name"] for step in report["steps"] if step["name"].startswith("negative-")
            }
            self.assertEqual(len(negative_steps), 6)

    def test_reject_enabled_true_stops_before_launch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root, reject_enabled="true")
            evidence_root = root / "evidence"
            status = run_cli(self.args(manifest, config, runner, evidence_root))
            self.assertEqual(status, 1)
            report = json.loads((evidence_root / "manifest.json").read_text(encoding="utf-8"))
            self.assertTrue(any(item["step"] == "config.real-reject-disabled"
                                for item in report["failures"]))
            self.assertFalse((evidence_root / "runtime-output").exists())

    def test_non_windows_requires_explicit_test_flag(self):
        if EVIDENCE.platform.system() == "Windows":
            self.skipTest("this assertion is specific to the non-Windows development host")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root)
            status = run_cli([
                "--executable", str(runner), "--manifest", str(manifest),
                "--config-ini", str(config), "--evidence-root", str(root / "evidence"),
            ])
            self.assertEqual(status, 2)
            self.assertFalse((root / "evidence").exists())

    def test_tampered_runtime_outputs_are_rejected(self):
        for mode in ("missing-frame-json", "real-io-trace", "summary-mismatch",
                     "input-camera-mismatch", "frame-decision-array", "invalid-png"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                manifest, config, runner = self.make_fixture(root)
                evidence_root = root / "evidence"
                with patch.dict(os.environ, {"P6_FAKE_MODE": mode}):
                    status = run_cli(self.args(manifest, config, runner, evidence_root))
                self.assertEqual(status, 1)
                report = json.loads((evidence_root / "manifest.json").read_text(encoding="utf-8"))
                self.assertTrue(report["failures"], mode)

    def test_negative_cli_wrong_exit_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root)
            evidence_root = root / "evidence"
            with patch.dict(os.environ, {"P6_FAKE_MODE": "wrong-negative-exit"}):
                status = run_cli(self.args(manifest, config, runner, evidence_root))
            self.assertEqual(status, 1)
            report = json.loads((evidence_root / "manifest.json").read_text(encoding="utf-8"))
            self.assertTrue(any(item["step"].endswith(".exit") and
                                "negative" in item["step"] for item in report["failures"]))

    def test_invalid_manifest_is_reported_by_preflight(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root)
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["samples"][0]["sha256"] = "0" * 64
            write_json(manifest, value)
            evidence_root = root / "evidence"
            status = run_cli(self.args(manifest, config, runner, evidence_root))
            self.assertEqual(status, 1)
            report = json.loads((evidence_root / "manifest.json").read_text(encoding="utf-8"))
            self.assertTrue(any(item["step"] == "manifest-preflight.exit"
                                for item in report["failures"]))
            self.assertFalse((evidence_root / "runtime-output").exists())

    def test_existing_evidence_root_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, config, runner = self.make_fixture(root)
            evidence_root = root / "evidence"
            evidence_root.mkdir()
            marker = evidence_root / "marker.txt"
            marker.write_text("keep", encoding="utf-8")
            status = run_cli(self.args(manifest, config, runner, evidence_root))
            self.assertEqual(status, 2)
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_windows_wrapper_wires_release_build_and_driver_safely(self):
        wrapper = (ROOT / "scripts" / "run_windows_p6_simulation.ps1").read_text(
            encoding="utf-8")
        for required in (
            "run_windows_p1_build.ps1",
            "p6_windows_simulation_evidence.py",
            "-Configuration Release",
            "--reject-delay-micros",
            "--queue-capacity",
            "--target-output",
            "--config-ini",
            "RealIoEnabled = $simulationManifest.realIoEnabled",
        ):
            self.assertIn(required, wrapper)
        self.assertNotIn("--allow-non-windows-test", wrapper)


if __name__ == "__main__":
    unittest.main()
