#!/usr/bin/env python3
"""Adversarial tests for the duration-enforced P8 continuous soak tool."""

from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import datetime as dt
import hashlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from unittest import mock
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_PATH = ROOT / "scripts" / "p8_continuous_soak.py"
SPEC = importlib.util.spec_from_file_location("p8_continuous_soak", SCRIPT_PATH)
assert SPEC and SPEC.loader
SOAK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SOAK)


def run_cli(arguments: list[str]) -> int:
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return SOAK.main(arguments)


def write_json(path: Path, value: object) -> None:
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_record(path: Path) -> dict[str, object]:
    resolved = path.resolve(strict=True)
    return {
        "path": str(resolved),
        "size": resolved.stat().st_size,
        "sha256": digest(resolved),
    }


def parse_timestamp(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def write_runtime(root: Path) -> Path:
    runtime = root / "continuous_runtime.py"
    runtime.write_text(
        "#!/usr/bin/env python3\n"
        "from __future__ import annotations\n"
        + textwrap.dedent(
            """
            import json
            import pathlib
            import sys
            import time

            output = pathlib.Path(sys.argv[1])
            target = float(sys.argv[2])
            restart = int(sys.argv[3])
            round_number = int(sys.argv[4])
            iteration = int(sys.argv[5])
            mode = sys.argv[6]
            started = time.monotonic()
            held = None
            sessions = 0
            processed = 0
            checksum = 0
            progress = (output / "local-soak-progress.ndjson").open(
                "w", encoding="utf-8"
            )
            run_for = max(target + 0.15, 0.32)
            if mode == "memory-growth":
                run_for = max(target + 0.8, 0.92)
            while time.monotonic() - started < run_for:
                sessions += 1
                for value in range(12000):
                    checksum = (checksum + value * 17) & 0xFFFFFFFF
                elapsed = time.monotonic() - started
                processed += 512
                ok = processed // 2
                progress.write(json.dumps({
                    "schemaVersion": "cigvision-local-soak-progress-v1",
                    "restart": restart,
                    "round": round_number,
                    "iteration": iteration,
                    "sequence": sessions,
                    "elapsedSeconds": elapsed,
                    "sessionsCompleted": sessions,
                    "framesProcessed": processed,
                    "ok": ok,
                    "ng": processed - ok,
                    "error": 0,
                    "productStateProcessed": processed,
                    "productStateOk": ok,
                    "productStateNg": processed - ok,
                    "productStateError": 0,
                    "realIoEnabled": False,
                    "realRejectEnabled": False,
                    "invariantsPassed": True,
                }) + "\\n")
                progress.flush()
                if mode == "memory-growth" and held is None and elapsed >= 0.40:
                    held = bytearray(32 * 1024 * 1024)
                    for offset in range(0, len(held), 4096):
                        held[offset] = (offset // 4096) & 0xFF
                time.sleep(0.003)
            duration = time.monotonic() - started
            ok = processed // 2
            progress.close()
            summary = {
                "schemaVersion": "cigvision-local-soak-runtime-v1",
                "sdkFree": True,
                "realIoEnabled": False,
                "realRejectEnabled": False,
                "productAcceptanceClaimed": False,
                "completed": True,
                "restart": restart,
                "round": round_number,
                "iteration": iteration,
                "targetDurationSeconds": target,
                "durationSeconds": duration,
                "sessionsCompleted": sessions,
                "framesReceived": processed,
                "framesProcessed": processed,
                "ok": ok,
                "ng": processed - ok,
                "error": 0,
                "dropped": 0,
                "sourceErrors": 0,
                "detectorErrors": 0,
                "observerErrors": 0,
                "saveFailures": 0,
                "resultStores": processed,
                "archiveStores": processed,
                "productStateProcessed": processed,
                "productStateOk": ok,
                "productStateNg": processed - ok,
                "productStateError": 0,
                "progressRecords": sessions,
                "maximumQueueDepth": 1,
                "invariantsPassed": True,
            }
            staging = output / "local-soak-summary.json.tmp"
            staging.write_text(json.dumps(summary), encoding="utf-8")
            staging.replace(output / "local-soak-summary.json")
            print(f"runtime checksum={checksum} sessions={sessions}")
            """
        ),
        encoding="utf-8",
    )
    runtime.chmod(0o755)
    return runtime


def write_build_provenance(root: Path, runtime: Path) -> Path:
    executable = Path(sys.executable).resolve(strict=True)
    path = root / f"{runtime.stem}-build-provenance.json"
    write_json(path, {
        "schemaVersion": "p8-continuous-soak-build-provenance-v1",
        "runtimeKind": "test-helper",
        "compiler": {
            **file_record(executable),
            "version": sys.version.split()[0],
        },
        "compile": {
            "standard": "python",
            "flags": [],
            "includePath": "",
            "argv": [],
        },
        "sourceFiles": [file_record(runtime)],
        "executable": file_record(runtime),
    })
    return path


def write_sleep_forgery_runtime(root: Path) -> Path:
    runtime = root / "sleep_forgery_runtime.sh"
    runtime.write_text(
        """#!/bin/sh
set -eu
output=$1
printf '%s\n' \
  '{"schemaVersion":"cigvision-local-soak-progress-v1","restart":1,"round":1,"iteration":1,"sequence":1,"elapsedSeconds":0.05,"sessionsCompleted":1,"framesProcessed":128,"ok":64,"ng":64,"error":0,"productStateProcessed":128,"productStateOk":64,"productStateNg":64,"productStateError":0,"realIoEnabled":false,"realRejectEnabled":false,"invariantsPassed":true}' \
  '{"schemaVersion":"cigvision-local-soak-progress-v1","restart":1,"round":1,"iteration":1,"sequence":2,"elapsedSeconds":0.20,"sessionsCompleted":2,"framesProcessed":256,"ok":128,"ng":128,"error":0,"productStateProcessed":256,"productStateOk":128,"productStateNg":128,"productStateError":0,"realIoEnabled":false,"realRejectEnabled":false,"invariantsPassed":true}' \
  > "$output/local-soak-progress.ndjson"
printf '%s\n' \
  '{"schemaVersion":"cigvision-local-soak-runtime-v1","sdkFree":true,"realIoEnabled":false,"realRejectEnabled":false,"productAcceptanceClaimed":false,"completed":true,"restart":1,"round":1,"iteration":1,"targetDurationSeconds":0.12,"durationSeconds":0.32,"sessionsCompleted":2,"framesReceived":256,"framesProcessed":256,"ok":128,"ng":128,"error":0,"dropped":0,"sourceErrors":0,"detectorErrors":0,"observerErrors":0,"saveFailures":0,"resultStores":256,"archiveStores":256,"productStateProcessed":256,"productStateOk":128,"productStateNg":128,"productStateError":0,"progressRecords":2,"maximumQueueDepth":1,"invariantsPassed":true}' \
  > "$output/local-soak-summary.json.tmp"
mv "$output/local-soak-summary.json.tmp" "$output/local-soak-summary.json"
sleep 0.35
""",
        encoding="utf-8",
    )
    runtime.chmod(0o755)
    return runtime


class P8ContinuousSoakTests(unittest.TestCase):
    def arguments(
        self,
        root: Path,
        evidence_name: str,
        runtime: Path | None = None,
        mode: str = "healthy",
        command: list[str] | None = None,
    ) -> tuple[list[str], Path]:
        evidence = root / evidence_name
        assert runtime is not None
        if command is None:
            command = [
                str(runtime),
                "{output_dir}",
                "{minimum_duration_seconds}",
                "{restart}",
                "{round}",
                "{iteration}",
                mode,
            ]
        build_provenance = write_build_provenance(root, runtime)
        return [
            "run",
            "--evidence-root", str(evidence),
            "--profile", "contract-test-v1",
            "--cwd", str(root),
            "--build-provenance", str(build_provenance),
            "--",
            *command,
        ], evidence

    @staticmethod
    def manifest(evidence: Path) -> dict[str, object]:
        return json.loads(
            (evidence / "soak-manifest.json").read_text(encoding="utf-8")
        )

    @staticmethod
    def refresh_inventory(manifest: dict[str, object], evidence: Path, relative: str) -> None:
        path = evidence / relative
        for record in manifest["evidenceFiles"]:
            if record["path"] == relative:
                record["size"] = path.stat().st_size
                record["sha256"] = digest(path)
                return
        raise AssertionError(f"inventory record not found: {relative}")

    def test_instant_exit_zero_fails_duration_and_sample_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = root / "instant_runtime.py"
            runtime.write_text("#!/usr/bin/env python3\npass\n", encoding="utf-8")
            runtime.chmod(0o755)
            command = [str(runtime)]
            arguments, evidence = self.arguments(
                root, "instant-evidence", runtime, command=command,
            )
            self.assertEqual(run_cli(arguments), 1)
            manifest = self.manifest(evidence)
            run = manifest["runs"][0]
            checks = {item["name"]: item for item in manifest["checks"]}
            self.assertEqual(run["exitCode"], 0)
            self.assertFalse(run["succeeded"])
            self.assertEqual(run["durationSeconds"], 0.0)
            self.assertLess(run["durationSeconds"], 0.12)
            self.assertIsNone(run["runtimeContract"])
            self.assertEqual(checks["minimum-duration"]["status"], "failed")
            self.assertEqual(
                run_cli(["verify", "--evidence-root", str(evidence)]), 2,
            )

            direct_evidence = root / "direct-instant-evidence"
            direct_evidence.mkdir()
            thresholds = dict(SOAK.LOCKED_PROFILES["contract-test-v1"])
            thresholds["profileName"] = "contract-test-v1"
            direct = SOAK._run_once(
                evidence_root=direct_evidence,
                command_template=[sys.executable, "-c", "pass"],
                cwd=root,
                restart=1,
                round_number=1,
                iteration=1,
                thresholds=thresholds,
            )
            self.assertLess(direct["processLifetimeSeconds"], 0.12)
            self.assertEqual(direct["durationSeconds"], 0.0)
            self.assertLess(direct["sampleCount"], 4)
            _, direct_checks = SOAK._evaluate([direct], thresholds)
            direct_by_name = {item["name"]: item for item in direct_checks}
            self.assertEqual(
                direct_by_name["minimum-duration"]["status"], "failed",
            )
            self.assertEqual(
                direct_by_name["minimum-samples"]["status"], "failed",
            )

            forged_runtime = write_sleep_forgery_runtime(root)
            forged_evidence = root / "sleep-forgery-evidence"
            forged_evidence.mkdir()
            forged_thresholds = dict(thresholds)
            forged = SOAK._run_once(
                evidence_root=forged_evidence,
                command_template=[
                    str(forged_runtime),
                    "{output_dir}",
                    "{minimum_duration_seconds}",
                    "{restart}",
                    "{round}",
                    "{iteration}",
                ],
                cwd=root,
                restart=1,
                round_number=1,
                iteration=1,
                thresholds=forged_thresholds,
            )
            self.assertIsNotNone(forged["runtimeContract"])
            self.assertEqual(forged["durationSeconds"], 0.32)
            self.assertIsNotNone(forged["processGroupCpuTotalSeconds"])
            self.assertLess(
                forged["processGroupCpuTotalSeconds"],
                forged_thresholds["minimumProcessGroupCpuSeconds"],
            )
            self.assertFalse(forged["continuousContractSatisfied"])


    def test_healthy_continuous_runtime_passes_and_reverifies(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root)
            arguments, evidence = self.arguments(root, "healthy-evidence", runtime)
            self.assertEqual(run_cli(arguments), 0)
            self.assertEqual(
                run_cli(["verify", "--evidence-root", str(evidence)]), 0,
            )
            manifest = self.manifest(evidence)
            run = manifest["runs"][0]
            self.assertEqual(manifest["overallResult"], "passed-test-contract")
            self.assertTrue(run["succeeded"])
            self.assertEqual(
                run["durationSeconds"], run["runtimeContract"]["durationSeconds"],
            )
            self.assertGreaterEqual(
                run["processLifetimeSeconds"], run["durationSeconds"],
            )
            self.assertGreaterEqual(run["sampleCount"], 4)
            self.assertGreater(run["runtimeContract"]["framesProcessed"], 0)
            self.assertFalse(manifest["productAcceptanceClaimed"])
            self.assertFalse(manifest["windowsRuntimeVerified"])
            self.assertLessEqual(
                parse_timestamp(manifest["startedAt"]),
                parse_timestamp(run["startedAt"]),
            )
            self.assertGreaterEqual(
                parse_timestamp(manifest["endedAt"]),
                parse_timestamp(run["endedAt"]),
            )
            result = json.loads(
                (evidence / run["resultRecord"]).read_text(encoding="utf-8")
            )
            self.assertEqual(result, run)

    def test_post_warmup_memory_growth_exceeds_locked_limit(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root)
            arguments, evidence = self.arguments(
                root, "memory-evidence", runtime, "memory-growth",
            )
            self.assertEqual(run_cli(arguments), 1)
            manifest = self.manifest(evidence)
            run = manifest["runs"][0]
            checks = {item["name"]: item for item in manifest["checks"]}
            self.assertIsNotNone(run["runtimeContract"])
            self.assertGreater(
                run["intraRunRssGrowthBytes"],
                SOAK.LOCKED_PROFILES["contract-test-v1"][
                    "maximumIntraRunRssGrowthBytes"
                ],
            )
            self.assertEqual(checks["intra-run-rss-growth"]["status"], "failed")
            self.assertFalse(run["continuousContractSatisfied"])

    def test_verify_rejects_samples_result_checks_and_empty_pass_flip(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root)
            arguments, evidence = self.arguments(root, "base-evidence", runtime)
            self.assertEqual(run_cli(arguments), 0)

            cases = [
                "samples", "result", "checks", "binary-provenance",
                "helper-snapshot", "dependency-manifest", "dependency-record",
                "unlisted-tmp",
                "empty-pass-flip",
            ]
            if os.name != "nt":
                cases.extend(("ps-snapshot", "ps-identity"))
            for case in cases:
                with self.subTest(case=case):
                    tampered = root / f"tampered-{case}"
                    shutil.copytree(evidence, tampered)
                    manifest = self.manifest(tampered)
                    run = manifest["runs"][0]
                    if case == "samples":
                        relative = run["samples"]
                        path = tampered / relative
                        document = json.loads(path.read_text(encoding="utf-8"))
                        self.assertGreaterEqual(len(document["samples"]), 2)
                        document["samples"][1]["elapsedSeconds"] = document["samples"][0][
                            "elapsedSeconds"
                        ]
                        write_json(path, document)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "result":
                        relative = run["resultRecord"]
                        path = tampered / relative
                        document = json.loads(path.read_text(encoding="utf-8"))
                        document["exitCode"] = 9
                        write_json(path, document)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "checks":
                        manifest["checks"][0]["observed"] = 999
                    elif case == "binary-provenance":
                        reference = manifest["buildProvenance"]["executableSnapshot"]
                        relative = reference["path"]
                        path = tampered / relative
                        path.write_bytes(path.read_bytes() + b"tamper")
                        reference["size"] = path.stat().st_size
                        reference["sha256"] = digest(path)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "helper-snapshot":
                        reference = manifest["toolDependencies"][
                            "sourceSnapshots"
                        ][1]["snapshot"]
                        relative = reference["path"]
                        path = tampered / relative
                        path.write_bytes(path.read_bytes() + b"\n# tamper\n")
                        reference["size"] = path.stat().st_size
                        reference["sha256"] = digest(path)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "dependency-record":
                        reference = manifest["toolDependencies"]["record"]
                        relative = reference["path"]
                        path = tampered / relative
                        document = json.loads(path.read_text(encoding="utf-8"))
                        document["sources"][1]["sha256"] = "0" * 64
                        write_json(path, document)
                        reference["size"] = path.stat().st_size
                        reference["sha256"] = digest(path)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "dependency-manifest":
                        manifest["toolDependencies"]["sourceSnapshots"][1][
                            "sha256"
                        ] = "0" * 64
                    elif case == "ps-snapshot":
                        reference = manifest["toolDependencies"][
                            "resourceCollectorSnapshot"
                        ]["snapshot"]
                        if reference is None:
                            continue
                        relative = reference["path"]
                        path = tampered / relative
                        path.write_bytes(path.read_bytes() + b"tamper")
                        reference["size"] = path.stat().st_size
                        reference["sha256"] = digest(path)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "ps-identity":
                        reference = manifest["toolDependencies"]["record"]
                        relative = reference["path"]
                        path = tampered / relative
                        document = json.loads(path.read_text(encoding="utf-8"))
                        document["resourceCollector"]["sha256"] = "0" * 64
                        write_json(path, document)
                        reference["size"] = path.stat().st_size
                        reference["sha256"] = digest(path)
                        self.refresh_inventory(manifest, tampered, relative)
                    elif case == "unlisted-tmp":
                        (tampered / "injected-unhashed.tmp").write_text(
                            "must not be ignored", encoding="utf-8"
                        )
                    else:
                        thresholds = dict(SOAK.LOCKED_PROFILES["contract-test-v1"])
                        thresholds["profileName"] = "contract-test-v1"
                        summary, checks = SOAK._evaluate([], thresholds)
                        checks.append(SOAK._gate(
                            "regular-evidence-files", True, [],
                            "no symlink or non-regular evidence files",
                        ))
                        manifest["runs"] = []
                        manifest["summary"] = summary
                        manifest["checks"] = checks
                        manifest["overallResult"] = "passed-test-contract"
                        manifest["failures"] = []
                    write_json(tampered / "soak-manifest.json", manifest)
                    self.assertEqual(
                        run_cli(["verify", "--evidence-root", str(tampered)]), 2,
                    )

    def test_tracked_profiles_equal_locked_values_and_have_no_cli_thresholds(self):
        linux_stat = "123 (worker name) " + " ".join(
            ["R", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "25", "15"]
        )
        self.assertEqual(SOAK._parse_linux_proc_cpu_seconds(linux_stat, 100), 0.4)
        document = json.loads(SOAK.PROFILE_PATH.read_text(encoding="utf-8"))
        self.assertEqual(document["profiles"], SOAK.LOCKED_PROFILES)
        self.assertEqual(SOAK._load_profiles(), SOAK.LOCKED_PROFILES)
        collector = SOAK._resource_collector_record()
        if os.name != "nt":
            self.assertIn(
                collector["canonicalPath"], SOAK.TRUSTED_POSIX_PS_PATHS,
            )
            self.assertEqual(
                collector["versionProbe"]["argv"][0],
                collector["canonicalPath"],
            )
            self.assertEqual(
                collector["resourceArgv"][0], collector["canonicalPath"],
            )
            self.assertEqual(
                collector["treeArgv"][0], collector["canonicalPath"],
            )
        parser = SOAK._build_parser()
        subparsers_action = next(
            action for action in parser._actions if hasattr(action, "choices")
            and isinstance(action.choices, dict)
        )
        run_parser = subparsers_action.choices["run"]
        option_strings = {
            option
            for action in run_parser._actions
            for option in action.option_strings
        }
        self.assertEqual(option_strings, {
            "-h", "--help", "--evidence-root", "--profile", "--cwd",
            "--build-provenance", "--compiler", "--standard",
        })
        for forbidden in (
            "--minimum-duration-seconds", "--minimum-sample-count",
            "--maximum-rss-growth-bytes", "--minimum-disk-free-bytes",
            "--timeout-seconds", "--rounds", "--restarts",
        ):
            self.assertNotIn(forbidden, option_strings)

        fake_process = mock.Mock(pid=123)
        termination_template = {
            "attempted": False,
            "method": "not-required",
            "gracefulSignalSent": False,
            "forceSignalSent": False,
            "noResidualProcessConfirmed": False,
            "residualProcessIds": [],
            "trackedProcessIds": [],
            "treeEnumerationSucceeded": False,
        }
        with mock.patch.object(
            SOAK.LEGACY,
            "_windows_process_tree_pids",
            side_effect=([123, 456], None),
        ), mock.patch.object(
            SOAK.LEGACY, "_wait_windows_pids_gone", return_value=[]
        ), mock.patch.object(SOAK.time, "sleep"):
            failed_termination, failed_clean = (
                SOAK._confirm_windows_process_tree_v2(
                    fake_process, {123}, 0.5, dict(termination_template)
                )
            )
        self.assertFalse(failed_clean)
        self.assertFalse(failed_termination["treeEnumerationSucceeded"])
        self.assertFalse(failed_termination["noResidualProcessConfirmed"])
        self.assertEqual(failed_termination["trackedProcessIds"], [123, 456])

        with mock.patch.object(
            SOAK.LEGACY,
            "_windows_process_tree_pids",
            side_effect=([123, 456], [123, 456]),
        ), mock.patch.object(
            SOAK.LEGACY, "_wait_windows_pids_gone", return_value=[]
        ), mock.patch.object(SOAK.time, "sleep"):
            passed_termination, passed_clean = (
                SOAK._confirm_windows_process_tree_v2(
                    fake_process, {123}, 0.5, dict(termination_template)
                )
            )
        self.assertTrue(passed_clean)
        self.assertTrue(passed_termination["treeEnumerationSucceeded"])
        self.assertTrue(passed_termination["noResidualProcessConfirmed"])
        self.assertEqual(passed_termination["trackedProcessIds"], [123, 456])

        for missing_option in ("--profile", "--evidence-root"):
            completed = subprocess.run(
                [str(ROOT / "scripts" / "run_p8_local_continuous_soak.sh"),
                 missing_option],
                cwd=ROOT,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 2)
            self.assertIn("requires a value", completed.stderr)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runtime = write_runtime(root)
            provenance = write_build_provenance(root, runtime)

            drift_helper = root / "p8_soak_evidence.py"
            drift_helper.write_bytes(SOAK.LEGACY_SOURCE_BYTES + b"\n# drift\n")
            drift_arguments, drift_evidence = self.arguments(
                root, "legacy-drift-evidence", runtime,
            )
            with mock.patch.object(SOAK, "LEGACY_PATH", drift_helper):
                self.assertEqual(run_cli(drift_arguments), 2)
            self.assertFalse(drift_evidence.exists())

            if os.name != "nt":
                fake_bin = root / "fake-bin"
                fake_bin.mkdir()
                fake_ps = fake_bin / "ps"
                fake_ps.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
                fake_ps.chmod(0o755)
                shadow_arguments, shadow_evidence = self.arguments(
                    root, "path-shadow-evidence", runtime,
                )
                with mock.patch.dict(os.environ, {"PATH": str(fake_bin)}):
                    self.assertEqual(run_cli(shadow_arguments), 2)
                self.assertFalse(shadow_evidence.exists())

            with self.assertRaises(SOAK.ContinuousSoakError):
                SOAK._validate_build_provenance(
                    provenance,
                    [str(runtime)],
                    "local-sdkfree-v1",
                )

            compiler = shutil.which("g++")
            self.assertIsNotNone(compiler)
            generated = root / "project-build-provenance.json"
            generated_runtime = root / "generated-project-runtime"
            self.assertFalse(generated_runtime.exists())
            self.assertEqual(run_cli([
                "build-provenance",
                "--output", str(generated),
                "--compiler", str(compiler),
                "--standard", "c++17",
                "--executable", str(generated_runtime),
            ]), 0)
            self.assertTrue(generated_runtime.is_file())
            generated_document = json.loads(generated.read_text(encoding="utf-8"))
            self.assertEqual(
                generated_document["compile"]["argv"][-1],
                str(generated_runtime.resolve()),
            )

            controlled_evidence = root / "controlled-project-evidence"
            self.assertEqual(run_cli([
                "run",
                "--evidence-root", str(controlled_evidence),
                "--profile", "contract-test-v1",
                "--cwd", str(ROOT),
                "--compiler", str(compiler),
                "--standard", "c++17",
                "--",
                "--output-dir", "{output_dir}",
                "--duration-seconds", "{minimum_duration_seconds}",
                "--frames-per-session", "512",
                "--restart", "{restart}",
                "--round", "{round}",
                "--iteration", "{iteration}",
            ]), 0)
            controlled_manifest = self.manifest(controlled_evidence)
            self.assertEqual(
                controlled_manifest["buildProvenance"]["runtimeKind"],
                SOAK.PROJECT_RUNTIME_KIND,
            )
            self.assertTrue(controlled_manifest["commandTemplate"][0].startswith(
                "{evidence_root}/provenance/runtime-executable"
            ))
            self.assertEqual(
                run_cli(["verify", "--evidence-root", str(controlled_evidence)]),
                0,
            )

            fake_document = dict(generated_document)
            fake_document["compile"] = dict(generated_document["compile"])
            fake_document["executable"] = file_record(Path(sys.executable))
            fake_document["compile"]["argv"] = list(
                generated_document["compile"]["argv"]
            )
            fake_document["compile"]["argv"][-1] = str(
                Path(sys.executable).resolve()
            )
            fake_provenance = root / "forged-project-provenance.json"
            write_json(fake_provenance, fake_document)
            fake_evidence = root / "forged-project-evidence"
            self.assertEqual(run_cli([
                "run",
                "--evidence-root", str(fake_evidence),
                "--profile", "contract-test-v1",
                "--cwd", str(ROOT),
                "--build-provenance", str(fake_provenance),
                "--", str(Path(sys.executable).resolve()), "-c", "pass",
            ]), 2)
            self.assertFalse(fake_evidence.exists())


if __name__ == "__main__":
    unittest.main()
