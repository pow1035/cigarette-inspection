#!/usr/bin/env python3
"""Run and audit the P6 Qt simulation batch on a Windows target.

The production entry point remains Simulation-only: this driver never opens a
camera or DAQNavi device and never supplies a Real reject mode.  It validates
the input before launch, checks every emitted frame/result/summary/trace, runs
CLI rejection cases, and writes a hash-bound evidence manifest.  A hidden-in-
plain-sight test flag permits the same orchestration to be exercised with a
fake executable on non-Windows CI; that mode is always labelled test-only and
cannot claim Windows runtime verification.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Callable


SCHEMA_VERSION = "p6-windows-simulation-evidence-v1"
MAX_REJECT_DELAY_MICROS = 60_000_000
MAX_QUEUE_CAPACITY = 65_536
MAX_TARGET_OUTPUT_BYTES = 256
FRAME_JSON_RE = re.compile(r"^frame-([0-9]{8})\.json$")
FRAME_PNG_RE = re.compile(r"^frame-([0-9]{8})\.png$")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
ACTIVE_REJECT_RE = re.compile(
    r"^\s*rejectEnabled\s*=\s*([^\s;#]+)", re.IGNORECASE | re.MULTILINE)
SCRIPT_PATH = Path(__file__).resolve()
SCRIPT_DIR = SCRIPT_PATH.parent
REPO_ROOT = SCRIPT_DIR.parent
PREFLIGHT_PATH = SCRIPT_DIR / "p6_simulation_preflight.py"


class EvidenceError(ValueError):
    """Raised for an invalid invocation that must not start the Qt process."""


class DuplicateKeyError(ValueError):
    """Raised when an evidence JSON object repeats a key."""


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(
            stream,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=lambda item: (_ for _ in ()).throw(
                ValueError(f"non-finite JSON value: {item}")),
        )
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def has_png_signature(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            return stream.read(len(PNG_SIGNATURE)) == PNG_SIGNATURE
    except OSError:
        return False


def atomic_write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as stream:
            temporary = stream.name
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass


def _absolute(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def _path_key(path: Path) -> str:
    return os.path.normcase(os.path.realpath(os.fspath(_absolute(path)))).casefold()


def _regular_file(path: Path, label: str) -> Path:
    value = _absolute(path)
    if value.is_symlink() or not value.is_file():
        raise EvidenceError(f"{label} must be a regular non-symlink file: {value}")
    return Path(os.path.realpath(os.fspath(value)))


def _validate_new_evidence_root(path: Path, inputs: tuple[Path, ...]) -> Path:
    value = _absolute(path)
    if value.exists() or value.is_symlink():
        raise EvidenceError(f"evidence root must not already exist: {value}")
    value_key = _path_key(value)
    for input_path in inputs:
        input_key = _path_key(input_path)
        if value_key == input_key or input_key.startswith(value_key + os.sep.casefold()):
            raise EvidenceError(f"evidence root must not contain an input: {value}")
    return value


def _file_record(path: Path, base: Path | None = None) -> dict[str, Any]:
    item = _regular_file(path, "evidence file")
    display: str
    if base is not None:
        try:
            display = str(item.relative_to(base))
        except ValueError:
            display = str(item)
    else:
        display = str(item)
    return {
        "path": display.replace("\\", "/"),
        "length": item.stat().st_size,
        "sha256": sha256_file(item),
    }


def _git_output(arguments: list[str]) -> list[str] | str | None:
    try:
        completed = subprocess.run(
            ["git", "-C", str(REPO_ROOT), *arguments],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    text = completed.stdout.rstrip("\r\n")
    return text.splitlines() if "\n" in text else text


def _as_text(value: str | bytes | None) -> str:
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value


def _positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return parsed


def _bounded_int(minimum: int, maximum: int) -> Callable[[str], int]:
    def parse(value: str) -> int:
        parsed = int(value)
        if parsed < minimum or parsed > maximum:
            raise argparse.ArgumentTypeError(
                f"value must be from {minimum} to {maximum}")
        return parsed
    return parse


def _reject_disabled(config_path: Path) -> tuple[bool, str]:
    text = config_path.read_text(encoding="utf-8-sig")
    values = ACTIVE_REJECT_RE.findall(text)
    if len(values) != 1:
        return False, "config.ini must contain exactly one active rejectEnabled setting"
    normalized = values[0].strip().casefold()
    if normalized in {"false", "0", "no", "off"}:
        return True, "rejectEnabled=false"
    if normalized in {"true", "1", "yes", "on"}:
        return False, "rejectEnabled must be false before simulation evidence collection"
    return False, f"rejectEnabled has an unsupported value: {values[0]}"


class EvidenceCollector:
    def __init__(
        self,
        root: Path,
        executable: Path,
        manifest: Path,
        config: Path,
        reject_delay_micros: int,
        queue_capacity: int,
        target_output: str,
        timeout_seconds: int,
        allow_non_windows_test: bool,
        invocation: list[str],
    ) -> None:
        self.root = root
        self.executable = executable
        self.manifest = manifest
        self.config = config
        self.reject_delay_micros = reject_delay_micros
        self.queue_capacity = queue_capacity
        self.target_output = target_output
        self.timeout_seconds = timeout_seconds
        self.allow_non_windows_test = allow_non_windows_test
        self.invocation = invocation
        self.started_at = dt.datetime.now(dt.timezone.utc)
        self.steps: list[dict[str, Any]] = []
        self.checks: list[dict[str, Any]] = []
        self.failures: list[dict[str, Any]] = []
        self.runtime_output = self.root / "runtime-output"
        self.manifest_sample_count = 0
        self.frame_json_count = 0
        self.frame_png_count = 0
        self.root.mkdir(parents=True)
        (self.root / "commands").mkdir()
        (self.root / "logs").mkdir()

    def fail(self, step: str, detail: str, exit_code: int | None = None) -> None:
        failure: dict[str, Any] = {"step": step, "detail": detail}
        if exit_code is not None:
            failure["exitCode"] = exit_code
        self.failures.append(failure)

    def check(self, name: str, condition: bool, detail: str = "") -> bool:
        record: dict[str, Any] = {
            "name": name,
            "status": "passed" if condition else "failed",
        }
        if detail:
            record["detail"] = detail
        self.checks.append(record)
        if not condition:
            self.fail(name, detail or "check failed")
        return condition

    def run_process(self, name: str, command: list[str]) -> int | None:
        command_record = {
            "arguments": command,
            "workingDirectory": str(REPO_ROOT),
            "shell": False,
        }
        atomic_write_json(self.root / "commands" / f"{name}.json", command_record)
        stdout_path = self.root / "logs" / f"{name}.stdout.log"
        stderr_path = self.root / "logs" / f"{name}.stderr.log"
        started = dt.datetime.now(dt.timezone.utc)
        timed_out = False
        launch_error = ""
        exit_code: int | None = None
        stdout = ""
        stderr = ""
        try:
            completed = subprocess.run(
                command,
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=self.timeout_seconds,
                check=False,
            )
            exit_code = completed.returncode
            stdout = completed.stdout
            stderr = completed.stderr
        except subprocess.TimeoutExpired as exc:
            timed_out = True
            stdout = _as_text(exc.stdout)
            stderr = _as_text(exc.stderr)
            launch_error = f"timed out after {self.timeout_seconds} seconds"
        except OSError as exc:
            launch_error = f"process launch failed: {exc}"
        stdout_path.write_text(stdout, encoding="utf-8")
        stderr_path.write_text(stderr, encoding="utf-8")
        self.steps.append({
            "name": name,
            "exitCode": exit_code,
            "timedOut": timed_out,
            "launchError": launch_error,
            "startedAt": started.isoformat().replace("+00:00", "Z"),
            "finishedAt": dt.datetime.now(dt.timezone.utc).isoformat().replace(
                "+00:00", "Z"),
            "command": str((self.root / "commands" / f"{name}.json").relative_to(self.root)),
            "stdout": str(stdout_path.relative_to(self.root)),
            "stderr": str(stderr_path.relative_to(self.root)),
        })
        if launch_error:
            self.fail(name, launch_error)
        return exit_code

    def run_preflight(self, name: str, trace: Path | None = None) -> bool:
        report = self.root / f"{name}.json"
        command = [
            sys.executable,
            str(PREFLIGHT_PATH),
            "--manifest",
            str(self.manifest),
            "--output",
            str(report),
        ]
        if trace is not None:
            command.extend(["--trace", str(trace), "--require-trace"])
        exit_code = self.run_process(name, command)
        return self.check(f"{name}.exit", exit_code == 0,
                          f"expected exit 0, observed {exit_code}")

    def simulation_command(self, output: Path | None = None) -> list[str]:
        command = [
            str(self.executable),
            "--simulation-batch-manifest",
            str(self.manifest),
        ]
        if output is not None:
            command.extend(["--simulation-output", str(output)])
        command.extend([
            "--simulation-reject-delay-micros",
            str(self.reject_delay_micros),
            "--simulation-queue-capacity",
            str(self.queue_capacity),
            "--simulation-target-output",
            self.target_output,
        ])
        return command

    def _canonical_samples(self) -> list[dict[str, Any]]:
        document = load_json(self.manifest)
        root_value = document["root"]
        samples_value = document["samples"]
        source_root = Path(os.path.realpath(os.fspath(
            _absolute(self.manifest.parent / root_value.replace("\\", os.sep)))))
        samples: list[dict[str, Any]] = []
        for index, raw in enumerate(samples_value):
            source = Path(os.path.realpath(os.fspath(
                _absolute(source_root / raw["path"].replace("\\", os.sep)))))
            samples.append({
                "source": source,
                "sha256": raw["sha256"].upper(),
                "stationId": raw.get("stationId", "offline").strip(),
                "cameraId": raw.get("cameraId", source.name).strip(),
                "cigaretteNumber": raw.get("cigaretteNumber", index + 1),
                "delayBeforeMicros": raw.get("delayBeforeMicros", 0),
                "expected": raw.get("expected"),
            })
        return samples

    def validate_runtime_output(self) -> bool:
        required = {
            "input-manifest": self.runtime_output / "input-manifest.json",
            "summary": self.runtime_output / "summary.json",
            "trace": self.runtime_output / "simulation-trace.json",
        }
        if not self.check("runtime-output.directory", self.runtime_output.is_dir() and
                          not self.runtime_output.is_symlink(),
                          "runtime-output must be a regular directory"):
            return False
        for name, path in required.items():
            if not self.check(f"runtime-output.{name}", path.is_file() and not path.is_symlink(),
                              f"required output is missing or unsafe: {path.name}"):
                return False
        try:
            canonical = self._canonical_samples()
            input_manifest = load_json(required["input-manifest"])
            summary = load_json(required["summary"])
        except (OSError, ValueError, KeyError, TypeError) as exc:
            self.fail("runtime-output.json", str(exc))
            return False

        self.manifest_sample_count = len(canonical)
        input_samples = input_manifest.get("samples")
        if not self.check("runtime-output.input-count",
                          isinstance(input_samples, list) and len(input_samples) == len(canonical),
                          "input-manifest sample count differs from the supplied manifest"):
            return False

        expected_frame_names = {
            f"frame-{index:08d}.json" for index in range(1, len(canonical) + 1)
        }
        expected_png_names = {
            f"frame-{index:08d}.png" for index in range(1, len(canonical) + 1)
        }
        actual_frame_names = {
            item.name for item in self.runtime_output.iterdir()
            if item.is_file() and not item.is_symlink() and FRAME_JSON_RE.fullmatch(item.name)
        }
        actual_png_names = {
            item.name for item in self.runtime_output.iterdir()
            if item.is_file() and not item.is_symlink() and FRAME_PNG_RE.fullmatch(item.name)
        }
        self.frame_json_count = len(actual_frame_names)
        self.frame_png_count = len(actual_png_names)
        self.check("runtime-output.frame-json-set", actual_frame_names == expected_frame_names,
                   "frame JSON set does not exactly match manifest order")
        self.check("runtime-output.frame-png-set", actual_png_names == expected_png_names,
                   "frame PNG set does not exactly match manifest order")

        decisions: list[str] = []
        for index, sample in enumerate(canonical, start=1):
            input_item = input_samples[index - 1]
            if not isinstance(input_item, dict):
                self.fail(f"runtime-output.input[{index}]", "input-manifest item is not an object")
                continue
            input_ok = (
                isinstance(input_item.get("sourcePath"), str) and
                _path_key(Path(input_item["sourcePath"])) == _path_key(sample["source"]) and
                isinstance(input_item.get("sha256"), str) and
                input_item["sha256"].upper() == sample["sha256"] and
                input_item.get("stationId") == sample["stationId"] and
                input_item.get("cameraId") == sample["cameraId"] and
                input_item.get("cigaretteNumber") == sample["cigaretteNumber"] and
                input_item.get("delayBeforeMicros") == sample["delayBeforeMicros"] and
                input_item.get("expected") == sample["expected"]
            )
            self.check(f"runtime-output.input[{index}]", input_ok,
                       "input-manifest metadata/hash binding differs from source manifest")

            frame_path = self.runtime_output / f"frame-{index:08d}.json"
            png_path = self.runtime_output / f"frame-{index:08d}.png"
            if not frame_path.is_file() or frame_path.is_symlink():
                self.fail(f"runtime-output.frame[{index}]",
                          "frame JSON is missing or is a symbolic link")
                continue
            try:
                frame = load_json(frame_path)
            except (OSError, ValueError) as exc:
                self.fail(f"runtime-output.frame[{index}]", str(exc))
                continue
            decision = frame.get("decision")
            if isinstance(decision, str):
                decisions.append(decision)
            decision_valid = isinstance(decision, str) and decision in ("OK", "NG")
            error_code = frame.get("errorCode", "")
            error_message = frame.get("errorMessage", "")
            frame_ok = (
                frame.get("frameId") == index and
                frame.get("stationId") == sample["stationId"] and
                frame.get("cameraId") == sample["cameraId"] and
                frame.get("cigaretteNumber") == sample["cigaretteNumber"] and
                decision_valid and
                (sample["expected"] is None or decision == sample["expected"]) and
                frame.get("parameterVersion") == "simulation-fixture-v1" and
                error_code in ("", None) and
                error_message in ("", None)
            )
            self.check(f"runtime-output.frame[{index}]", frame_ok,
                       "frame result metadata/decision is not bound to the manifest")
            self.check(f"runtime-output.png[{index}]",
                       png_path.is_file() and not png_path.is_symlink() and
                       png_path.stat().st_size >= len(PNG_SIGNATURE) and
                       has_png_signature(png_path),
                       "frame PNG is missing, unsafe or has an invalid signature")

        statistics = summary.get("statistics")
        issues = summary.get("issues")
        ok_count = decisions.count("OK")
        ng_count = decisions.count("NG")
        summary_ok = isinstance(statistics, dict) and (
            summary.get("state") == 1 and
            statistics.get("received") == len(canonical) and
            statistics.get("processed") == len(canonical) and
            statistics.get("ok") == ok_count and
            statistics.get("ng") == ng_count and
            statistics.get("error") == 0 and
            statistics.get("sourceErrors") == 0 and
            statistics.get("detectorErrors") == 0 and
            statistics.get("observerErrors") == 0 and
            statistics.get("saveFailures") == 0 and
            statistics.get("dropped") == 0 and
            isinstance(issues, list) and not issues
        )
        self.check("runtime-output.summary", summary_ok,
                   "summary is incomplete or its accounting differs from frame results")
        return not self.failures

    def run_negative_cli_matrix(self) -> None:
        negative_root = self.root / "negative-cli"
        negative_root.mkdir()
        malformed_manifest = negative_root / "delay-too-large-manifest.json"
        document = load_json(self.manifest)
        source_root = Path(os.path.realpath(os.fspath(
            _absolute(self.manifest.parent / document["root"].replace("\\", os.sep)))))
        document["root"] = os.path.relpath(source_root, malformed_manifest.parent)
        document["samples"][0]["delayBeforeMicros"] = MAX_REJECT_DELAY_MICROS + 1
        atomic_write_json(malformed_manifest, document)

        cases: list[tuple[str, list[str], list[Path]]] = []
        missing_output = [
            str(self.executable),
            "--simulation-batch-manifest",
            str(self.manifest),
        ]
        cases.append(("negative-missing-output", missing_output, []))

        conflict_output = negative_root / "conflict-simulation-output"
        conflict_offline = negative_root / "conflict-offline-output"
        cases.append(("negative-conflicting-output", [
            str(self.executable),
            "--simulation-batch-manifest", str(self.manifest),
            "--simulation-output", str(conflict_output),
            "--offline-output", str(conflict_offline),
        ], [conflict_output, conflict_offline]))

        for name, option, value in (
            ("negative-zero-capacity", "--simulation-queue-capacity", "0"),
            ("negative-delay-too-large", "--simulation-reject-delay-micros",
             str(MAX_REJECT_DELAY_MICROS + 1)),
            ("negative-blank-target", "--simulation-target-output", "   "),
        ):
            output = negative_root / f"{name}-output"
            command = self.simulation_command(output)
            option_index = command.index(option)
            command[option_index + 1] = value
            cases.append((name, command, [output]))

        malformed_output = negative_root / "malformed-manifest-output"
        malformed_command = self.simulation_command(malformed_output)
        manifest_index = malformed_command.index("--simulation-batch-manifest")
        malformed_command[manifest_index + 1] = str(malformed_manifest)
        cases.append(("negative-malformed-manifest", malformed_command, [malformed_output]))

        for name, command, forbidden_outputs in cases:
            exit_code = self.run_process(name, command)
            self.check(f"{name}.exit", exit_code == 2,
                       f"expected parser/manifest rejection exit 2, observed {exit_code}")
            for output in forbidden_outputs:
                self.check(f"{name}.no-output", not output.exists(),
                           f"rejected invocation created output: {output}")

    def source_records(self) -> list[dict[str, Any]]:
        source_directory = REPO_ROOT / "01_上位机_QT_新版_CigVision" / "源码"
        paths = [
            self.executable,
            self.manifest,
            self.config,
            SCRIPT_PATH,
            PREFLIGHT_PATH,
            SCRIPT_DIR / "run_windows_p6_simulation.ps1",
            source_directory / "main.cpp",
            source_directory / "adapters" / "qt" / "QtOfflineInspection.h",
            source_directory / "adapters" / "qt" / "QtOfflineInspection.cpp",
            source_directory / "core" / "BatchCommandLine.h",
            source_directory / "core" / "OfflineInspection.h",
            source_directory / "core" / "RealtimeSimulation.h",
        ]
        return [_file_record(path, REPO_ROOT) for path in paths if path.is_file()]

    def evidence_records(self) -> list[dict[str, Any]]:
        manifest_path = self.root / "manifest.json"
        return [
            _file_record(path, self.root)
            for path in sorted(self.root.rglob("*"))
            if path.is_file() and path != manifest_path and not path.is_symlink()
        ]

    def finalize(self) -> int:
        windows_host = platform.system() == "Windows"
        passed = not self.failures
        if passed and windows_host:
            overall = "passed"
        elif passed and self.allow_non_windows_test:
            overall = "passed-test-only"
        else:
            overall = "failed"
        manifest = {
            "schemaVersion": SCHEMA_VERSION,
            "overallResult": overall,
            "overallCommandExitCode": 0 if passed else 1,
            "startedAt": self.started_at.isoformat().replace("+00:00", "Z"),
            "finishedAt": dt.datetime.now(dt.timezone.utc).isoformat().replace(
                "+00:00", "Z"),
            "invocation": self.invocation,
            "host": {
                "system": platform.system(),
                "release": platform.release(),
                "python": platform.python_version(),
            },
            "targetWindowsRuntimeVerified": passed and windows_host,
            "nonWindowsTestOnly": bool(self.allow_non_windows_test and not windows_host),
            "realHardwareUsed": False,
            "realIoEnabled": False,
            "realRejectEnabled": False,
            "detector": "deterministic-fixture-v1",
            "accuracyMetricsClaimed": False,
            "tensorRtEvidenceClaimed": False,
            "configuration": {
                "rejectDelayMicros": self.reject_delay_micros,
                "queueCapacity": self.queue_capacity,
                "targetOutput": self.target_output,
                "overflowPolicy": "RejectNewest",
            },
            "counts": {
                "manifestSamples": self.manifest_sample_count,
                "frameJson": self.frame_json_count,
                "framePng": self.frame_png_count,
            },
            "gitHead": _git_output(["rev-parse", "HEAD"]),
            "gitStatus": _git_output(["status", "--short", "--untracked-files=all"]),
            "steps": self.steps,
            "checks": self.checks,
            "failures": self.failures,
            "sourceFiles": self.source_records(),
            "evidenceFiles": self.evidence_records(),
        }
        atomic_write_json(self.root / "manifest.json", manifest)
        return 0 if passed else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True,
                        help="built CigVision.exe (or an executable test double)")
    parser.add_argument("--manifest", required=True,
                        help="P6 simulation input manifest")
    parser.add_argument("--config-ini", required=True,
                        help="CigVision config.ini; rejectEnabled must be false")
    parser.add_argument("--evidence-root", required=True,
                        help="new directory for runtime output and evidence")
    parser.add_argument("--reject-delay-micros", type=_bounded_int(
        0, MAX_REJECT_DELAY_MICROS), default=500)
    parser.add_argument("--queue-capacity", type=_bounded_int(
        1, MAX_QUEUE_CAPACITY), default=4)
    parser.add_argument("--target-output", default="simulation-reject")
    parser.add_argument("--timeout-seconds", type=_positive_int, default=120)
    parser.add_argument("--allow-non-windows-test", action="store_true",
                        help=argparse.SUPPRESS)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    invocation = [str(SCRIPT_PATH), *(argv if argv is not None else sys.argv[1:])]
    if platform.system() != "Windows" and not arguments.allow_non_windows_test:
        print("P6 Windows evidence: target Windows host is required", file=sys.stderr)
        return 2
    target_bytes = arguments.target_output.strip().encode("utf-8")
    if not target_bytes or len(target_bytes) > MAX_TARGET_OUTPUT_BYTES:
        print("P6 Windows evidence: target output must contain 1 to 256 UTF-8 bytes",
              file=sys.stderr)
        return 2

    try:
        executable = _regular_file(Path(arguments.executable), "executable")
        manifest = _regular_file(Path(arguments.manifest), "manifest")
        config = _regular_file(Path(arguments.config_ini), "config.ini")
        preflight = _regular_file(PREFLIGHT_PATH, "P6 preflight")
        root = _validate_new_evidence_root(
            Path(arguments.evidence_root), (executable, manifest, config, preflight))
    except (OSError, ValueError) as exc:
        print(f"P6 Windows evidence: invalid invocation: {exc}", file=sys.stderr)
        return 2

    collector = EvidenceCollector(
        root=root,
        executable=executable,
        manifest=manifest,
        config=config,
        reject_delay_micros=arguments.reject_delay_micros,
        queue_capacity=arguments.queue_capacity,
        target_output=arguments.target_output.strip(),
        timeout_seconds=arguments.timeout_seconds,
        allow_non_windows_test=arguments.allow_non_windows_test,
        invocation=invocation,
    )
    try:
        reject_safe, reject_detail = _reject_disabled(config)
        if not collector.check("config.real-reject-disabled", reject_safe, reject_detail):
            return collector.finalize()
        if not collector.run_preflight("manifest-preflight"):
            return collector.finalize()

        simulation_exit = collector.run_process(
            "simulation-runtime", collector.simulation_command(collector.runtime_output))
        if not collector.check("simulation-runtime.exit", simulation_exit == 0,
                               f"expected exit 0, observed {simulation_exit}"):
            return collector.finalize()
        collector.validate_runtime_output()
        trace_path = collector.runtime_output / "simulation-trace.json"
        if trace_path.is_file():
            collector.run_preflight("trace-preflight", trace_path)
        collector.run_negative_cli_matrix()
    except Exception as exc:  # Preserve partial target evidence on unexpected failures.
        collector.fail("unhandled-exception", f"{type(exc).__name__}: {exc}")

    status = collector.finalize()
    if status == 0:
        label = "PASS (test-only)" if arguments.allow_non_windows_test and platform.system() != "Windows" else "PASS"
        print(f"P6 Windows simulation evidence: {label}: {root}")
    else:
        print(f"P6 Windows simulation evidence: FAILED: {root}", file=sys.stderr)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
