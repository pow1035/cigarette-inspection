#!/usr/bin/env python3
"""Read-only preflight for the P6 simulation manifest and trace.

This utility deliberately does not run Qt, decode images, load TensorRT, or
touch camera/DAQNavi interfaces.  It checks the inputs and (when supplied) a
``simulation-trace.json`` produced by the Qt batch entry point so a target
machine can fail before starting a long run.  The checks establish internal
consistency only; they are not a signature or provenance authority.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import sys
import tempfile
import unicodedata
from pathlib import Path, PureWindowsPath
from typing import Any


TRACE_SCHEMA_VERSION = "cigvision-simulation-trace-v1"
REPORT_SCHEMA_VERSION = "p6-simulation-preflight-v1"
MAX_LOCAL_REPLAY_DELAY_MICROS = 60_000_000
MAX_QUEUE_CAPACITY = 65_536
MAX_TARGET_OUTPUT_BYTES = 256
UINT32_MAX = 4_294_967_295
UINT64_MAX = 18_446_744_073_709_551_615
INT64_MAX = 9_223_372_036_854_775_807
SHA256_RE = re.compile(r"^[0-9A-Fa-f]{64}$")
DECISIONS = {"OK", "NG", "ERROR", "UNKNOWN"}
TRACE_STATUSES = {"SKIPPED", "SIMULATED", "FAILED"}
RECEIPT_STATUSES = {"SKIPPED", "SIMULATED", "FAILED"}


class DuplicateKeyError(ValueError):
    """Raised when a JSON object repeats a key."""


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
            parse_constant=lambda value: (_ for _ in ()).throw(
                ValueError(f"non-finite JSON value: {value}")),
        )
    if not isinstance(value, dict):
        raise ValueError("JSON root must be an object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _absolute(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def _resolved(path: Path) -> Path:
    try:
        return Path(os.path.realpath(os.fspath(path)))
    except (OSError, RuntimeError) as exc:
        raise ValueError(f"path cannot be resolved safely: {path}: {exc}") from exc


def _path_key(path: Path) -> tuple[str, ...]:
    """Conservative NFC/case-folded spelling for cross-platform comparisons."""
    return tuple(
        unicodedata.normalize("NFC", part).casefold()
        for part in _absolute(path).parts
    )


def _path_within(candidate: Path, root: Path) -> bool:
    candidate_key = _path_key(candidate)
    root_key = _path_key(root)
    return len(candidate_key) >= len(root_key) and candidate_key[:len(root_key)] == root_key


def _has_symlink_component(path: Path, stop: Path | None = None) -> bool:
    current = _absolute(path)
    stop_absolute = _absolute(stop) if stop is not None else None
    while True:
        if current.is_symlink():
            return True
        if stop_absolute is not None and current == stop_absolute:
            return False
        if current.parent == current:
            return False
        current = current.parent


def _is_any_platform_absolute(value: str) -> bool:
    return Path(value).is_absolute() or PureWindowsPath(value).is_absolute()


def _portable_relative_parts(value: str) -> tuple[str, ...]:
    # A manifest may have been authored on Windows and checked on macOS/Linux.
    # Treat both separators as separators for the safety check.
    normalized = value.replace("\\", "/")
    return tuple(part for part in normalized.split("/") if part not in ("", "."))


def _relative_walk_has_symlink(base: Path, value: str) -> bool:
    """Inspect components before lexical ``..`` cleanup can hide a symlink."""
    current = _absolute(base)
    for part in value.replace("\\", "/").split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            current = current.parent
            continue
        current = current / part
        if current.is_symlink():
            return True
    return False


def _is_int(value: Any, minimum: int | None = None, maximum: int | None = None) -> bool:
    if isinstance(value, bool) or not isinstance(value, int):
        return False
    return (minimum is None or value >= minimum) and (maximum is None or value <= maximum)


def _nonempty_string(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


class Preflight:
    def __init__(self) -> None:
        self.checks: list[dict[str, Any]] = []
        self.missing: list[str] = []
        self.invalid: list[str] = []
        self.manifest: dict[str, Any] | None = None
        self.manifest_path: Path | None = None
        self.manifest_root: Path | None = None
        self.samples: list[dict[str, Any]] = []

    def check(
        self,
        name: str,
        status: str,
        detail: str = "",
        **fields: Any,
    ) -> None:
        record: dict[str, Any] = {"name": name, "status": status}
        if detail:
            record["detail"] = detail
        record.update(fields)
        self.checks.append(record)
        if status == "missing":
            self.missing.append(name)
        elif status in {"invalid", "mismatch"}:
            self.invalid.append(name)

    def validate_manifest(self, manifest_path: Path) -> None:
        self.manifest_path = _absolute(manifest_path)
        if not self.manifest_path.exists():
            self.check("manifest", "missing", "manifest file does not exist", path=str(self.manifest_path))
            return
        if self.manifest_path.is_symlink() or not self.manifest_path.is_file():
            self.check("manifest", "invalid", "manifest path is not a regular non-symlink file",
                       path=str(self.manifest_path))
            return
        try:
            document = load_json(self.manifest_path)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            self.check("manifest", "invalid", str(exc), path=str(self.manifest_path))
            return
        self.manifest = document

        root_value = document.get("root")
        samples_value = document.get("samples")
        if not isinstance(root_value, str) or not root_value.strip():
            self.check("manifest.root", "invalid", "root must be a non-empty string")
            return
        if not isinstance(samples_value, list) or not samples_value:
            self.check("manifest.samples", "invalid", "samples must be a non-empty array")
            return
        if _is_any_platform_absolute(root_value):
            self.check("manifest.root", "invalid", "root must be relative to the manifest")
            return

        if _relative_walk_has_symlink(self.manifest_path.parent, root_value):
            self.check("manifest.root", "invalid",
                       "root contains a symbolic-link component before path cleanup")
            return
        portable_root = root_value.replace("\\", os.sep)
        root = _absolute(self.manifest_path.parent / portable_root)
        try:
            root_resolved = _resolved(root)
        except ValueError as exc:
            self.check("manifest.root", "invalid", str(exc), path=str(root))
            return
        self.manifest_root = root_resolved
        if _has_symlink_component(root, self.manifest_path.parent):
            self.check("manifest.root", "invalid", "root contains a symbolic-link component",
                       path=str(root))
            return
        if not root.exists():
            self.check("manifest.root", "missing", "root directory does not exist",
                       path=str(root))
            return
        if not root.is_dir() or root.is_symlink():
            self.check("manifest.root", "invalid", "root is not a regular directory",
                       path=str(root))
            return
        self.check("manifest.root", "ok", path=str(root_resolved))

        seen: set[tuple[str, ...]] = set()
        for index, raw_sample in enumerate(samples_value):
            self._validate_sample(index, raw_sample, root_resolved, seen)
        if not self.invalid and not self.missing:
            self.check("manifest", "ok", samples=len(self.samples), root=str(root_resolved))

    def _validate_sample(
        self,
        index: int,
        raw_sample: Any,
        root: Path,
        seen: set[tuple[str, ...]],
    ) -> None:
        name = f"manifest.sample[{index}]"
        if not isinstance(raw_sample, dict):
            self.check(name, "invalid", "sample must be an object")
            return
        path_value = raw_sample.get("path")
        sha_value = raw_sample.get("sha256")
        if not isinstance(path_value, str) or not path_value.strip():
            self.check(f"{name}.path", "invalid", "path must be a non-empty string")
            return
        if _is_any_platform_absolute(path_value):
            self.check(f"{name}.path", "invalid", "sample path must be relative")
            return
        parts = _portable_relative_parts(path_value)
        if any(part == ".." for part in parts):
            self.check(f"{name}.path", "invalid", "parent-directory components are not accepted")
            return
        if not isinstance(sha_value, str) or not SHA256_RE.fullmatch(sha_value):
            self.check(f"{name}.sha256", "invalid", "sha256 must contain exactly 64 hex digits")
            return

        # Use the host separator after the portable traversal check. A Windows
        # manifest therefore remains auditable on POSIX without weakening the
        # containment rule.
        portable_path = path_value.replace("\\", os.sep)
        candidate = _absolute(root / portable_path)
        candidate_resolved = _resolved(candidate)
        if not _path_within(candidate_resolved, root):
            self.check(f"{name}.path", "invalid", "sample path escapes manifest root",
                       path=str(candidate))
            return
        key = _path_key(candidate_resolved)
        if key in seen:
            self.check(f"{name}.path", "invalid", "duplicate sample path")
            return
        seen.add(key)
        if _has_symlink_component(candidate, root):
            self.check(f"{name}.path", "invalid", "sample path contains a symbolic-link component",
                       path=str(candidate))
            return
        if not candidate.exists() or not candidate.is_file() or candidate.is_symlink():
            self.check(f"{name}.path", "invalid", "sample path is not a regular file",
                       path=str(candidate))
            return
        actual_hash = sha256_file(candidate)
        if actual_hash != sha_value.upper():
            self.check(f"{name}.sha256", "mismatch", "sample SHA-256 does not match",
                       expected=sha_value.upper(), actual=actual_hash, path=str(candidate))
            return

        station = raw_sample.get("stationId", "offline")
        camera = raw_sample.get("cameraId", candidate.name)
        cigarette = raw_sample.get("cigaretteNumber", index + 1)
        delay = raw_sample.get("delayBeforeMicros", 0)
        expected = raw_sample.get("expected")
        metadata_valid = True
        if not _nonempty_string(station):
            self.check(f"{name}.stationId", "invalid", "stationId must be a non-empty string")
            metadata_valid = False
        if not _nonempty_string(camera):
            self.check(f"{name}.cameraId", "invalid", "cameraId must be a non-empty string")
            metadata_valid = False
        if not _is_int(cigarette, 1, UINT32_MAX):
            self.check(f"{name}.cigaretteNumber", "invalid",
                       "cigaretteNumber must be an integer from 1 to UINT32_MAX")
            metadata_valid = False
        if not _is_int(delay, 0, MAX_LOCAL_REPLAY_DELAY_MICROS):
            self.check(f"{name}.delayBeforeMicros", "invalid",
                       "delayBeforeMicros must be an integer from 0 to 60000000")
            metadata_valid = False
        if expected is not None and (
                not isinstance(expected, str) or expected not in {"OK", "NG"}):
            self.check(f"{name}.expected", "invalid", "expected must be OK or NG")
            metadata_valid = False
        if metadata_valid:
            self.samples.append({
                "path": str(candidate_resolved),
                "stationId": station.strip(),
                "cameraId": camera.strip(),
                "cigaretteNumber": cigarette,
                "delayBeforeMicros": delay,
                "expected": expected,
            })
            self.check(name, "ok", path=str(candidate_resolved), sha256=actual_hash)

    def validate_trace(self, trace_path: Path) -> None:
        trace_path = _absolute(trace_path)
        if not trace_path.exists():
            self.check("trace", "missing", "trace file does not exist", path=str(trace_path))
            return
        if trace_path.is_symlink() or not trace_path.is_file():
            self.check("trace", "invalid", "trace path is not a regular non-symlink file",
                       path=str(trace_path))
            return
        try:
            trace = load_json(trace_path)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            self.check("trace", "invalid", str(exc), path=str(trace_path))
            return
        self._validate_trace_object(trace, trace_path)

    def _validate_trace_object(self, trace: dict[str, Any], trace_path: Path) -> None:
        if trace.get("schemaVersion") != TRACE_SCHEMA_VERSION:
            self.check("trace.schemaVersion", "invalid", "unexpected trace schemaVersion")
        if trace.get("mode") != "simulation" or trace.get("simulation") is not True:
            self.check("trace.mode", "invalid", "trace is not marked as simulation")
        if trace.get("realIoEnabled") is not False:
            self.check("trace.realIoEnabled", "invalid", "real IO must be disabled")
        if trace.get("runState") != 1:
            self.check("trace.runState", "invalid", "runState must be Completed")
        if trace.get("traceComplete") is not True:
            self.check("trace.traceComplete", "invalid", "simulation trace is not complete")
        if trace.get("traceValidationError", "") not in ("", None):
            self.check("trace.traceValidationError", "invalid", "trace validator reported an error")

        configuration = trace.get("configuration")
        if not isinstance(configuration, dict):
            self.check("trace.configuration", "invalid", "configuration must be an object")
            configuration = {}
        if configuration.get("detector") != "deterministic-fixture-v1":
            self.check("trace.configuration.detector", "invalid",
                       "unexpected simulation detector identity")
        if configuration.get("simulation") is not True:
            self.check("trace.configuration.simulation", "invalid", "configuration is not simulation")
        if configuration.get("rejectMode") != "Simulation":
            self.check("trace.configuration.rejectMode", "invalid", "reject mode must be Simulation")
        reject_delay = configuration.get("rejectDelayMicros")
        reject_delay_valid = _is_int(reject_delay, 0, MAX_LOCAL_REPLAY_DELAY_MICROS)
        if not reject_delay_valid:
            self.check("trace.configuration.rejectDelayMicros", "invalid",
                       "reject delay is outside the local safety range")
        if not _is_int(configuration.get("queueCapacity"), 1, MAX_QUEUE_CAPACITY):
            self.check("trace.configuration.queueCapacity", "invalid",
                       "queue capacity is outside the supported range")
        target = configuration.get("targetOutput")
        target_valid = (_nonempty_string(target) and
                        len(target.encode("utf-8")) <= MAX_TARGET_OUTPUT_BYTES)
        if not target_valid:
            self.check("trace.configuration.targetOutput", "invalid",
                       "target output must be non-empty and at most 256 UTF-8 bytes")
        if configuration.get("overflowPolicy") != "RejectNewest":
            self.check("trace.configuration.overflowPolicy", "invalid",
                       "Qt simulation trace must use RejectNewest")

        if self.manifest_path is not None:
            trace_manifest = trace.get("manifestPath")
            if not isinstance(trace_manifest, str) or not trace_manifest.strip():
                self.check("trace.manifestPath", "invalid", "manifestPath is required")
            else:
                supplied = _resolved(self.manifest_path)
                recorded = _resolved(Path(trace_manifest) if Path(trace_manifest).is_absolute()
                                     else trace_path.parent / trace_manifest)
                if _path_key(supplied) != _path_key(recorded):
                    self.check("trace.manifestPath", "mismatch",
                               "trace manifestPath does not match the supplied manifest",
                               expected=str(supplied), actual=str(recorded))

        statistics = trace.get("statistics")
        if not isinstance(statistics, dict):
            self.check("trace.statistics", "invalid", "statistics must be an object")
            return
        stat_names = ("received", "processed", "ok", "ng", "error", "dropped",
                      "observed", "ngCandidates", "commands", "simulated", "skipped", "failed")
        statistics_valid = True
        for stat_name in stat_names:
            if not _is_int(statistics.get(stat_name), 0, UINT64_MAX):
                self.check(f"trace.statistics.{stat_name}", "invalid",
                           "statistic must be a non-negative integer")
                statistics_valid = False

        raw_traces = trace.get("traces")
        if not isinstance(raw_traces, list):
            self.check("trace.traces", "invalid", "traces must be an array")
            return
        trace_count = trace.get("traceCount")
        if not _is_int(trace_count, 0, UINT64_MAX) or trace_count != len(raw_traces):
            self.check("trace.traceCount", "mismatch", "traceCount does not match traces length")
        if statistics.get("observed") != len(raw_traces):
            self.check("trace.statistics.observed", "mismatch",
                       "observed does not match traces length")
        if self.samples and len(raw_traces) != len(self.samples):
            self.check("trace.samples", "mismatch",
                       "trace count does not match manifest sample count")

        counts = {
            "SKIPPED": 0,
            "SIMULATED": 0,
            "FAILED": 0,
            "commands": 0,
            "ok": 0,
            "ng": 0,
            "error": 0,
        }
        for index, raw_trace in enumerate(raw_traces):
            self._validate_trace_entry(
                index,
                raw_trace,
                counts,
                target if target_valid else None,
                reject_delay if reject_delay_valid else None,
            )
        if not statistics_valid:
            return
        if statistics.get("skipped") != counts["SKIPPED"]:
            self.check("trace.statistics.skipped", "mismatch", "skipped count differs from trace")
        if statistics.get("simulated") != counts["SIMULATED"]:
            self.check("trace.statistics.simulated", "mismatch", "simulated count differs from trace")
        if statistics.get("failed") != counts["FAILED"]:
            self.check("trace.statistics.failed", "mismatch", "failed count differs from trace")
        if statistics.get("commands") != counts["commands"]:
            self.check("trace.statistics.commands", "mismatch", "command count differs from trace")
        if statistics.get("ngCandidates") != counts["ng"]:
            self.check("trace.statistics.ngCandidates", "mismatch", "NG count differs from trace")
        for decision_name in ("ok", "ng", "error"):
            if statistics.get(decision_name) != counts[decision_name]:
                self.check(f"trace.statistics.{decision_name}", "mismatch",
                           f"{decision_name} count differs from trace")
        if statistics.get("processed") != len(raw_traces):
            self.check("trace.statistics.processed", "mismatch",
                       "processed count differs from observed trace count")
        if statistics.get("received") != statistics.get("processed", -1) + statistics.get("dropped", -2):
            self.check("trace.statistics.received", "mismatch",
                       "received is not processed plus dropped")
        if statistics.get("processed") != (
                statistics.get("ok", -1) + statistics.get("ng", -1) +
                statistics.get("error", -1)):
            self.check("trace.statistics.processed", "mismatch",
                       "processed is not OK plus NG plus error")
        if (statistics.get("error") != 0 or statistics.get("dropped") != 0 or
                statistics.get("failed") != 0 or
                statistics.get("commands") != statistics.get("simulated") or
                statistics.get("skipped") + statistics.get("simulated") !=
                statistics.get("observed")):
            self.check("trace.statistics.completion", "invalid",
                       "completed simulation contains errors, drops or failed commands")

    def _validate_trace_entry(
        self,
        index: int,
        raw_trace: Any,
        counts: dict[str, int],
        target_output: str | None,
        reject_delay: int | None,
    ) -> None:
        name = f"trace.traces[{index}]"
        if not isinstance(raw_trace, dict):
            self.check(name, "invalid", "trace entry must be an object")
            return
        frame_id = raw_trace.get("frameId")
        station = raw_trace.get("stationId")
        camera = raw_trace.get("cameraId")
        cigarette = raw_trace.get("cigaretteNumber")
        observed = raw_trace.get("observedAtMicros")
        decision = raw_trace.get("decision")
        status = raw_trace.get("status")
        if (not _is_int(frame_id, 1, UINT64_MAX) or not _nonempty_string(station) or
                not _nonempty_string(camera)):
            self.check(name, "invalid", "frame metadata is incomplete")
            return
        if (not _is_int(cigarette, 1, UINT32_MAX) or
                not _is_int(observed, 1, INT64_MAX)):
            self.check(name, "invalid", "cigarette number or observed clock is invalid")
            return
        if (not isinstance(decision, str) or decision not in DECISIONS or
                not isinstance(status, str) or status not in TRACE_STATUSES):
            self.check(name, "invalid", "decision or trace status is outside the supported domain")
            return
        if raw_trace.get("simulation") is not True:
            self.check(name, "invalid", "trace entry is not marked as simulation")
        if decision == "UNKNOWN":
            self.check(name, "invalid", "completed trace cannot contain UNKNOWN decisions")
        if index < len(self.samples) and frame_id == index + 1:
            sample = self.samples[index]
            if (station != sample["stationId"] or camera != sample["cameraId"] or
                    cigarette != sample["cigaretteNumber"]):
                self.check(name, "mismatch", "trace frame metadata differs from manifest")
            expected = sample.get("expected")
            if expected is not None and decision != expected:
                self.check(name, "mismatch", "trace decision differs from manifest expectation")
        elif self.samples:
            self.check(name, "mismatch", "frame IDs must follow manifest order")

        command = raw_trace.get("command")
        execution = raw_trace.get("execution")
        if not isinstance(execution, dict):
            self.check(name, "invalid", "execution receipt must be an object")
            return
        if (not _is_int(execution.get("frameId"), 1, UINT64_MAX) or
                execution.get("frameId") != frame_id or
                not _is_int(execution.get("completedAtMicros"), 1, INT64_MAX)):
            self.check(name, "invalid", "execution receipt is not bound to frame and clock")
        receipt_status = execution.get("status")
        if not isinstance(receipt_status, str) or receipt_status not in RECEIPT_STATUSES:
            self.check(name, "invalid", "execution status is not Simulation-safe")

        has_command = isinstance(command, dict)
        if command is not None and not has_command:
            self.check(name, "invalid", "command must be an object or null")
        if has_command:
            counts["commands"] += 1
            command_target = command.get("targetOutput")
            scheduled = command.get("scheduledAtMicros")
            if (not _is_int(command.get("frameId"), 1, UINT64_MAX) or
                    command.get("frameId") != frame_id or
                    not _is_int(command.get("cigaretteNumber"), 1, UINT32_MAX) or
                    command.get("cigaretteNumber") != cigarette or
                    command.get("mode") != "Simulation" or
                    not _nonempty_string(command_target) or
                    len(command_target.encode("utf-8")) > MAX_TARGET_OUTPUT_BYTES or
                    not _is_int(scheduled, 1, INT64_MAX)):
                self.check(name, "invalid", "command is not a valid Simulation command")
            if target_output is not None and command_target != target_output:
                self.check(name, "mismatch", "command target differs from trace configuration")
            if (reject_delay is not None and observed <= INT64_MAX - reject_delay and
                    scheduled != observed + reject_delay):
                self.check(name, "mismatch", "command schedule differs from configured delay")
            if isinstance(execution.get("completedAtMicros"), int) and isinstance(
                    command.get("scheduledAtMicros"), int) and execution["completedAtMicros"] < command["scheduledAtMicros"]:
                self.check(name, "invalid", "receipt completed before command schedule")

        error_code = raw_trace.get("errorCode", "")
        error_message = raw_trace.get("errorMessage", "")
        execution_error_code = execution.get("errorCode", "")
        execution_error_message = execution.get("errorMessage", "")
        if status == "SKIPPED":
            counts["SKIPPED"] += 1
            if (decision == "NG" or has_command or receipt_status != "SKIPPED" or
                    error_code not in ("", None) or error_message not in ("", None) or
                    execution_error_code not in ("", None) or
                    execution_error_message not in ("", None) or
                    execution.get("completedAtMicros") != observed):
                self.check(name, "invalid", "skipped trace contains a command or non-skipped receipt")
        elif status == "SIMULATED":
            counts["SIMULATED"] += 1
            if (decision != "NG" or not has_command or receipt_status != "SIMULATED" or
                    error_code not in ("", None) or error_message not in ("", None) or
                    execution_error_code not in ("", None) or
                    execution_error_message not in ("", None)):
                self.check(name, "invalid", "simulated trace is missing an NG command or receipt")
        elif status == "FAILED":
            counts["FAILED"] += 1
            if (not _nonempty_string(error_code) or not _nonempty_string(error_message) or
                    receipt_status != "FAILED" or error_code != execution_error_code or
                    error_message != execution_error_message):
                self.check(name, "invalid", "failed trace error fields are not bound to the receipt")
        if decision == "OK":
            counts["ok"] += 1
        elif decision == "NG":
            counts["ng"] += 1
        elif decision == "ERROR":
            counts["error"] += 1

    def report(self, require_trace: bool, trace_requested: bool) -> dict[str, Any]:
        status = "ready"
        if self.invalid:
            status = "invalid"
        elif self.missing:
            status = "missing"
        return {
            "schemaVersion": REPORT_SCHEMA_VERSION,
            "generatedAt": dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z"),
            "ready": status == "ready",
            "status": status,
            "requireTrace": require_trace,
            "traceRequested": trace_requested,
            "missingRequired": sorted(set(self.missing)),
            "invalidOrMismatched": sorted(set(self.invalid)),
            "checks": self.checks,
            "manifestPath": str(self.manifest_path) if self.manifest_path else None,
            "manifestSampleCount": len(self.samples),
        }


def _output_overlaps_input(output: Path, inputs: tuple[Path, ...]) -> bool:
    output_key = _path_key(output)
    for input_path in inputs:
        input_key = _path_key(input_path)
        if output_key == input_key:
            return True
        try:
            if output.exists() and input_path.exists() and os.path.samefile(output, input_path):
                return True
        except (OSError, ValueError):
            pass
    return False


def write_report(path: Path, report: dict[str, Any], inputs: tuple[Path, ...]) -> None:
    path = _absolute(path)
    if path.is_symlink() or _output_overlaps_input(path, inputs):
        raise ValueError("output must not overwrite the manifest or trace")
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
                "w", encoding="utf-8", dir=path.parent, prefix=f".{path.name}.",
                suffix=".tmp", delete=False) as stream:
            temporary = stream.name
            json.dump(report, stream, ensure_ascii=False, indent=2)
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


def build_report(args: argparse.Namespace) -> tuple[Preflight, dict[str, Any], int]:
    preflight = Preflight()
    manifest_path = _absolute(Path(args.manifest))
    preflight.validate_manifest(manifest_path)
    trace_requested = args.trace is not None or args.require_trace
    if trace_requested:
        trace_path = _absolute(Path(args.trace)) if args.trace else manifest_path.parent / "simulation-trace.json"
        preflight.validate_trace(trace_path)
    report = preflight.report(args.require_trace, trace_requested)
    if preflight.invalid:
        status = 3
    elif preflight.missing:
        status = 2
    else:
        status = 0
    return preflight, report, status


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, help="P6 JSON manifest to inspect")
    parser.add_argument("--trace", help="optional simulation-trace.json to inspect")
    parser.add_argument("--require-trace", action="store_true",
                        help="require simulation-trace.json (defaults next to manifest)")
    parser.add_argument("--output", help="write an atomic JSON report to this path")
    args = parser.parse_args(argv)

    try:
        _, report, status = build_report(args)
        inputs = tuple(path for path in (
            _absolute(Path(args.manifest)),
            _absolute(Path(args.trace)) if args.trace else None,
        ) if path is not None)
        if args.output:
            write_report(Path(args.output), report, inputs)
        else:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        if status == 0:
            print("P6 simulation preflight: PASS", file=sys.stderr)
        elif status == 2:
            print("P6 simulation preflight: required input missing", file=sys.stderr)
        else:
            print("P6 simulation preflight: INVALID", file=sys.stderr)
        return status
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"P6 simulation preflight: INVALID: {exc}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
