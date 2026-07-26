#!/usr/bin/env python3
"""Tests for the P8 SDK-free soak evidence orchestrator."""

from __future__ import annotations

import hashlib
import argparse
import ctypes
import importlib.util
import io
import json
import os
import sys
import tempfile
import textwrap
import time
from types import SimpleNamespace
import unittest
from unittest import mock
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "p8_soak_evidence.py"
SPEC = importlib.util.spec_from_file_location("p8_soak_evidence", SCRIPT_PATH)
assert SPEC and SPEC.loader
SOAK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SOAK)


def run_cli(arguments: list[str]) -> int:
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return SOAK.main(arguments)


def write_runtime(root: Path, body: str) -> Path:
    runtime = root / "fake_runtime.py"
    runtime.write_text(
        "#!/usr/bin/env python3\n"
        "from __future__ import annotations\n"
        + textwrap.dedent(body),
        encoding="utf-8",
    )
    return runtime


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class P8SoakEvidenceTests(unittest.TestCase):
    def test_nonfinite_positive_float_arguments_are_rejected(self):
        for value in ("inf", "Infinity", "1e999", "nan", "-inf"):
            with self.subTest(value=value):
                with self.assertRaises(argparse.ArgumentTypeError):
                    SOAK._positive_float(value)

    def test_windows_cleanup_confirms_tracked_child_processes(self):
        killed: set[int] = set()

        class FakeProcess:
            pid = 100

            @staticmethod
            def wait(timeout: float) -> int:
                return 0

            @staticmethod
            def kill() -> None:
                killed.add(100)

        def fake_exists(pid: int) -> bool:
            return pid not in killed

        def fake_taskkill(arguments: list[str], **_: object) -> object:
            killed.add(int(arguments[2]))
            return SimpleNamespace(returncode=0)

        with mock.patch.object(SOAK.os, "name", "nt"), \
                mock.patch.object(
                    SOAK, "_windows_process_tree_pids", return_value=[100, 101]), \
                mock.patch.object(SOAK, "_process_exists", side_effect=fake_exists), \
                mock.patch.object(SOAK.subprocess, "run", side_effect=fake_taskkill):
            result = SOAK._terminate_process_group(
                FakeProcess(), 100, 0.01, {100, 101})

        self.assertTrue(result["noResidualProcessConfirmed"])
        self.assertEqual(result["residualProcessIds"], [])
        self.assertEqual(result["trackedProcessIds"], [100, 101])
        self.assertTrue(result["treeEnumerationSucceeded"])
        self.assertEqual(killed, {100, 101})

    def test_windows_process_enumeration_api_failures_are_unconfirmed(self):
        class FakeFunction:
            def __init__(self, values: list[object]):
                self.values = list(values)
                self.restype = None
                self.argtypes = None

            def __call__(self, *_: object) -> object:
                return self.values.pop(0)

        class FakeKernel:
            def __init__(self, first: bool, next_value: bool):
                self.CreateToolhelp32Snapshot = FakeFunction([1])
                self.Process32FirstW = FakeFunction([first])
                self.Process32NextW = FakeFunction([next_value])
                self.CloseHandle = FakeFunction([True])

        for first, next_value, label in (
            (False, False, "first"),
            (True, False, "next"),
        ):
            with self.subTest(api=label):
                fake_kernel = FakeKernel(first, next_value)
                with mock.patch.object(SOAK.os, "name", "nt"), \
                        mock.patch.object(
                            ctypes, "WinDLL", return_value=fake_kernel, create=True), \
                        mock.patch.object(
                            ctypes, "set_last_error", return_value=None, create=True), \
                        mock.patch.object(
                            ctypes, "get_last_error", return_value=5, create=True):
                    self.assertIsNone(SOAK._windows_process_tree_pids(100))

    def arguments(
        self,
        root: Path,
        runtime: Path,
        *,
        rounds: int = 1,
        restarts: int = 1,
        timeout: float = 2.0,
        maximum_output: int = 4 * 1024 * 1024,
        extra_command: list[str] | None = None,
    ) -> tuple[list[str], Path]:
        evidence = root / "evidence"
        command = [
            os.fspath(Path(sys.executable)),
            str(runtime),
            "{output_dir}",
            "{restart}",
            "{round}",
            "{iteration}",
        ]
        if extra_command:
            command.extend(extra_command)
        return [
            "--evidence-root", str(evidence),
            "--profile", "ci-contract-v1",
            "--cwd", str(root),
            "--rounds", str(rounds),
            "--restarts", str(restarts),
            "--timeout-seconds", str(timeout),
            "--sample-interval-seconds", "0.02",
            "--termination-grace-seconds", "0.5",
            "--min-disk-free-bytes", "1",
            "--max-output-bytes", str(maximum_output),
            "--max-rss-growth-bytes", str(1024 * 1024 * 1024),
            "--",
            *command,
        ], evidence

    def load_manifest(self, evidence: Path) -> dict[str, object]:
        return json.loads(
            (evidence / "soak-manifest.json").read_text(encoding="utf-8"))

    def test_success_uses_independent_fresh_directories_and_records_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, """
                import json
                import os
                import pathlib
                import sys
                import time

                output = pathlib.Path(sys.argv[1])
                assert output.is_dir()
                assert not list(output.iterdir())
                assert os.environ["P8_SOAK_OUTPUT_DIR"] == str(output)
                payload = {
                    "argv": sys.argv,
                    "restart": sys.argv[2],
                    "round": sys.argv[3],
                    "iteration": sys.argv[4],
                }
                (output / "runtime.json").write_text(json.dumps(payload), encoding="utf-8")
                print("fake runtime stdout")
                print("fake runtime stderr", file=sys.stderr)
                time.sleep(0.06)
            """)
            arguments, evidence = self.arguments(
                root, runtime, rounds=2, restarts=2)
            self.assertEqual(run_cli(arguments), 0)

            manifest = self.load_manifest(evidence)
            self.assertEqual(manifest["overallResult"], "passed-local-tooling")
            self.assertTrue(manifest["sdkFree"])
            self.assertFalse(manifest["productAcceptanceClaimed"])
            self.assertFalse(manifest["windowsRuntimeVerified"])
            self.assertEqual(manifest["gpu"], "not-collected")
            self.assertFalse(manifest["gpuClaimed"])
            self.assertEqual(manifest["profile"]["name"], "ci-contract-v1")
            self.assertEqual(manifest["profile"]["durationClass"], "short")
            self.assertEqual(manifest["summary"]["plannedRuns"], 4)
            self.assertEqual(manifest["summary"]["successfulRuns"], 4)
            self.assertEqual(manifest["summary"]["successRate"], 1.0)
            self.assertEqual(manifest["summary"]["timeouts"], 0)
            self.assertEqual(manifest["summary"]["crashes"], 0)

            output_directories: set[str] = set()
            for run in manifest["runs"]:
                self.assertEqual(run["exitCode"], 0)
                self.assertFalse(run["timedOut"])
                self.assertTrue(run["succeeded"])
                self.assertEqual(run["cwd"], str(root))
                self.assertTrue(run["startedAt"].endswith("Z"))
                self.assertTrue(run["endedAt"].endswith("Z"))
                self.assertEqual(run["gpu"], "not-collected")
                self.assertIn("path", run["stdout"])
                self.assertIn("path", run["stderr"])
                output_directories.add(run["outputDirectory"])
                output = evidence / run["outputDirectory"]
                self.assertTrue((output / "runtime.json").is_file())
                samples = json.loads(
                    (evidence / run["samples"]).read_text(encoding="utf-8"))
                self.assertTrue(samples["samples"])
                for sample in samples["samples"]:
                    self.assertIn("parentProcessRssBytes", sample)
                    self.assertIn("outputBytes", sample)
                    self.assertIn("diskFreeBytes", sample)
                    self.assertEqual(sample["gpu"], "not-collected")
                runtime_record = json.loads(
                    (output / "runtime.json").read_text(encoding="utf-8"))
                self.assertEqual(runtime_record["argv"][1], str(output))
            self.assertEqual(len(output_directories), 4)

    def test_manifest_lists_size_and_sha256_for_every_evidence_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, """
                import pathlib
                import sys
                output = pathlib.Path(sys.argv[1])
                (output / "artifact.bin").write_bytes(b"hash-me")
            """)
            arguments, evidence = self.arguments(root, runtime)
            self.assertEqual(run_cli(arguments), 0)
            manifest = self.load_manifest(evidence)
            listed = {
                item["path"]: item for item in manifest["evidenceFiles"]
            }
            actual = {
                path.relative_to(evidence).as_posix()
                for path in evidence.rglob("*")
                if path.is_file() and path.name != "soak-manifest.json"
            }
            self.assertEqual(set(listed), actual)
            self.assertNotIn("soak-manifest.json", listed)
            self.assertTrue(manifest["manifestExcludedFromSelfHash"])
            self.assertIn(
                "runs/restart-001/round-001/output/artifact.bin", listed)
            for relative, record in listed.items():
                path = evidence / relative
                self.assertTrue(path.is_file())
                self.assertEqual(record["size"], path.stat().st_size)
                self.assertEqual(record["sha256"], digest(path))
            self.assertFalse(list(evidence.rglob("*.tmp")))

    def test_crash_is_preserved_and_fails_gate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, """
                import os
                if os.name != "nt":
                    import resource
                    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                os.abort()
            """)
            arguments, evidence = self.arguments(root, runtime)
            self.assertEqual(run_cli(arguments), 1)
            manifest = self.load_manifest(evidence)
            self.assertEqual(manifest["overallResult"], "failed")
            self.assertEqual(manifest["summary"]["successfulRuns"], 0)
            self.assertEqual(manifest["summary"]["crashes"], 1)
            self.assertNotEqual(manifest["runs"][0]["exitCode"], 0)
            self.assertTrue(manifest["runs"][0]["crashed"])
            self.assertTrue((evidence / "runs/restart-001/round-001/result.json").is_file())

    def test_timeout_kills_process_group_and_confirms_no_residual(self):
        if os.name == "nt":
            self.skipTest("POSIX process-group residual assertion")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, """
                import pathlib
                import subprocess
                import sys
                import time

                output = pathlib.Path(sys.argv[1])
                child = subprocess.Popen([
                    sys.executable, "-c", "import time; time.sleep(60)"
                ])
                (output / "child.pid").write_text(str(child.pid), encoding="ascii")
                time.sleep(60)
            """)
            arguments, evidence = self.arguments(
                root, runtime, timeout=0.15)
            self.assertEqual(run_cli(arguments), 1)
            manifest = self.load_manifest(evidence)
            run = manifest["runs"][0]
            self.assertTrue(run["timedOut"])
            self.assertEqual(run["terminationReason"], "timeout")
            self.assertTrue(
                run["processGroupTermination"]["noResidualProcessConfirmed"])
            self.assertEqual(manifest["summary"]["timeouts"], 1)
            child_pid = int((
                evidence
                / "runs/restart-001/round-001/output/child.pid"
            ).read_text(encoding="ascii"))
            deadline = time.monotonic() + 1.0
            while time.monotonic() < deadline and SOAK._process_exists(child_pid):
                time.sleep(0.02)
            self.assertFalse(SOAK._process_exists(child_pid))

    def test_output_budget_violation_is_nonzero_with_partial_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, """
                import pathlib
                import sys
                output = pathlib.Path(sys.argv[1])
                (output / "too-large.bin").write_bytes(b"x" * 8192)
            """)
            arguments, evidence = self.arguments(
                root, runtime, maximum_output=1024)
            self.assertEqual(run_cli(arguments), 1)
            manifest = self.load_manifest(evidence)
            run = manifest["runs"][0]
            self.assertTrue(run["outputBudgetExceeded"])
            output_gate = next(
                item for item in manifest["checks"]
                if item["name"] == "output-budget"
            )
            self.assertEqual(output_gate["status"], "failed")
            self.assertTrue(
                (evidence / run["outputDirectory"] / "too-large.bin").is_file())
            self.assertTrue(manifest["evidenceFiles"])

    def test_existing_evidence_root_is_never_reused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root, "raise SystemExit(0)\n")
            arguments, evidence = self.arguments(root, runtime)
            evidence.mkdir()
            sentinel = evidence / "sentinel.txt"
            sentinel.write_text("keep", encoding="utf-8")
            self.assertEqual(run_cli(arguments), 2)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")
            self.assertFalse((evidence / "soak-manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
