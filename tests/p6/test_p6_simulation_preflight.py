#!/usr/bin/env python3
"""Adversarial tests for the SDK-free P6 simulation preflight."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import json
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts" / "p6_simulation_preflight.py"
SPEC = importlib.util.spec_from_file_location("p6_simulation_preflight", MODULE_PATH)
assert SPEC and SPEC.loader
PREFLIGHT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREFLIGHT)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def run_cli(arguments: list[str]) -> int:
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return PREFLIGHT.main(arguments)


class P6SimulationPreflightTests(unittest.TestCase):
    def make_manifest(self, root: Path, *, expected: str | None = "OK") -> tuple[Path, Path]:
        data_root = root / "images"
        data_root.mkdir()
        first = data_root / "a.bin"
        second = data_root / "b.bin"
        first.write_bytes(b"first-image-bytes")
        second.write_bytes(b"second-image-bytes")
        manifest = root / "manifest.json"
        samples = [
            {
                "path": "a.bin",
                "sha256": digest(first),
                "expected": expected,
                "stationId": "station-a",
                "cameraId": "camera-a",
                "cigaretteNumber": 7001,
                "delayBeforeMicros": 0,
            },
            {
                "path": "b.bin",
                "sha256": digest(second),
                "expected": "NG",
                "stationId": "station-b",
                "cameraId": "camera-b",
                "cigaretteNumber": 7002,
                "delayBeforeMicros": 250,
            },
        ]
        write_json(manifest, {"root": "images", "samples": samples})
        return manifest, data_root

    def make_trace(self, manifest: Path, *, real_io: bool = False) -> Path:
        trace = manifest.parent / "simulation-trace.json"
        entries = [
            {
                "frameId": 1,
                "stationId": "station-a",
                "cameraId": "camera-a",
                "cigaretteNumber": 7001,
                "observedAtMicros": 1000,
                "decision": "OK",
                "status": "SKIPPED",
                "simulation": True,
                "command": None,
                "execution": {
                    "frameId": 1,
                    "status": "SKIPPED",
                    "completedAtMicros": 1000,
                    "errorCode": "",
                    "errorMessage": "",
                },
                "errorCode": "",
                "errorMessage": "",
            },
            {
                "frameId": 2,
                "stationId": "station-b",
                "cameraId": "camera-b",
                "cigaretteNumber": 7002,
                "observedAtMicros": 1250,
                "decision": "NG",
                "status": "SIMULATED",
                "simulation": True,
                "command": {
                    "frameId": 2,
                    "cigaretteNumber": 7002,
                    "targetOutput": "simulation-reject",
                    "scheduledAtMicros": 1750,
                    "mode": "Simulation",
                },
                "execution": {
                    "frameId": 2,
                    "status": "SIMULATED",
                    "completedAtMicros": 1750,
                    "errorCode": "",
                    "errorMessage": "",
                },
                "errorCode": "",
                "errorMessage": "",
            },
        ]
        write_json(trace, {
            "schemaVersion": "cigvision-simulation-trace-v1",
            "mode": "simulation",
            "simulation": True,
            "realIoEnabled": real_io,
            "manifestPath": str(manifest.resolve()),
            "runState": 1,
            "traceCount": 2,
            "traceComplete": True,
            "traceValidationError": "",
            "configuration": {
                "detector": "deterministic-fixture-v1",
                "rejectMode": "Simulation",
                "simulation": True,
                "rejectDelayMicros": 500,
                "queueCapacity": 4,
                "targetOutput": "simulation-reject",
                "overflowPolicy": "RejectNewest",
            },
            "statistics": {
                "received": 2,
                "processed": 2,
                "ok": 1,
                "ng": 1,
                "error": 0,
                "dropped": 0,
                "observed": 2,
                "ngCandidates": 1,
                "commands": 1,
                "simulated": 1,
                "skipped": 1,
                "failed": 0,
            },
            "traces": entries,
        })
        return trace

    def test_manifest_only_passes_and_hashes_are_checked(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest, _ = self.make_manifest(Path(temp))
            result = run_cli(["--manifest", str(manifest)])
            self.assertEqual(result, 0)

    def test_valid_trace_passes_and_report_is_atomic(self):
        with tempfile.TemporaryDirectory() as temp:
            manifest, _ = self.make_manifest(Path(temp))
            trace = self.make_trace(manifest)
            report = Path(temp) / "reports" / "preflight.json"
            result = run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
                "--require-trace", "--output", str(report),
            ])
            self.assertEqual(result, 0)
            value = json.loads(report.read_text(encoding="utf-8"))
            self.assertTrue(value["ready"])
            self.assertEqual(value["manifestSampleCount"], 2)

    def test_missing_manifest_or_trace_is_exit_two(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.assertEqual(run_cli(["--manifest", str(root / "missing.json")]), 2)
            manifest, _ = self.make_manifest(root)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--require-trace",
            ]), 2)

    def test_hash_mismatch_and_path_escape_are_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, _ = self.make_manifest(root)
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["samples"][0]["sha256"] = "0" * 64
            write_json(manifest, value)
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)
            value["samples"][0]["sha256"] = digest(root / "images" / "a.bin")
            value["samples"][0]["path"] = "../outside.bin"
            write_json(manifest, value)
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)

    def test_metadata_bounds_and_duplicate_keys_are_invalid(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, _ = self.make_manifest(root)
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["samples"][0]["delayBeforeMicros"] = 60_000_001
            value["samples"][0]["cigaretteNumber"] = 0
            write_json(manifest, value)
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)
            manifest.write_text(
                '{"root":"images","root":"images","samples":[]}',
                encoding="utf-8",
            )
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)

            array_root = root / "array-expected"
            array_root.mkdir()
            manifest, _ = self.make_manifest(array_root)
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["samples"][0]["expected"] = ["OK"]
            write_json(manifest, value)
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)

    def test_trace_rejects_real_io_and_command_binding_tamper(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, _ = self.make_manifest(root)
            trace = self.make_trace(manifest, real_io=True)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["realIoEnabled"] = False
            value["traces"][1]["command"]["cigaretteNumber"] = 9999
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["traces"][1]["command"]["targetOutput"] = "other-output"
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["traces"][1]["command"]["scheduledAtMicros"] = 1751
            value["traces"][1]["execution"]["completedAtMicros"] = 1751
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)

    def test_trace_rejects_early_receipt_and_unbound_failure_error(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, _ = self.make_manifest(root)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["traces"][1]["execution"]["completedAtMicros"] = 100
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)

            for field, mutate in (
                ("decision", lambda entry: entry.__setitem__("decision", ["OK"])),
                ("status", lambda entry: entry.__setitem__("status", ["SKIPPED"])),
                ("execution.status", lambda entry: entry["execution"].__setitem__(
                    "status", ["SKIPPED"])),
            ):
                with self.subTest(field=field):
                    trace = self.make_trace(manifest)
                    value = json.loads(trace.read_text(encoding="utf-8"))
                    mutate(value["traces"][0])
                    write_json(trace, value)
                    self.assertEqual(run_cli([
                        "--manifest", str(manifest), "--trace", str(trace),
                    ]), 3)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["statistics"]["received"] = "2"
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)
            trace = self.make_trace(manifest)
            value = json.loads(trace.read_text(encoding="utf-8"))
            value["traces"][1]["execution"]["completedAtMicros"] = 1750
            value["traces"][1]["status"] = "FAILED"
            value["traces"][1]["execution"]["status"] = "FAILED"
            value["traces"][1]["errorCode"] = "A"
            value["traces"][1]["errorMessage"] = "B"
            value["traces"][1]["execution"]["errorCode"] = "C"
            value["traces"][1]["execution"]["errorMessage"] = "B"
            value["statistics"]["failed"] = 1
            value["statistics"]["simulated"] = 0
            write_json(trace, value)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
            ]), 3)

    def test_output_cannot_overwrite_manifest_or_trace(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, _ = self.make_manifest(root)
            trace = self.make_trace(manifest)
            before_manifest = manifest.read_bytes()
            before_trace = trace.read_bytes()
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--output", str(manifest),
            ]), 3)
            self.assertEqual(run_cli([
                "--manifest", str(manifest), "--trace", str(trace),
                "--output", str(trace),
            ]), 3)
            self.assertEqual(manifest.read_bytes(), before_manifest)
            self.assertEqual(trace.read_bytes(), before_trace)

    def test_symlinked_sample_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest, data_root = self.make_manifest(root)
            target = data_root / "a.bin"
            link = data_root / "alias.bin"
            try:
                link.symlink_to(target)
            except (OSError, NotImplementedError):
                self.skipTest("symbolic links are unavailable on this host")
            value = json.loads(manifest.read_text(encoding="utf-8"))
            value["samples"][0]["path"] = "alias.bin"
            value["samples"][0]["sha256"] = digest(target)
            write_json(manifest, value)
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)

            outside_images = root / "outside" / "images"
            outside_images.mkdir(parents=True)
            outside_sample = outside_images / "a.bin"
            outside_sample.write_bytes(b"outside-image")
            lexical_link = root / "link"
            lexical_link.symlink_to(root / "outside" / "target", target_is_directory=False)
            # The link target is deliberately absent; the lexical symlink
            # check must reject this before path normalization can hide it.
            write_json(manifest, {
                "root": "link/../images",
                "samples": [{
                    "path": "a.bin",
                    "sha256": digest(outside_sample),
                    "expected": "OK",
                }],
            })
            self.assertEqual(run_cli(["--manifest", str(manifest)]), 3)


if __name__ == "__main__":
    unittest.main()
