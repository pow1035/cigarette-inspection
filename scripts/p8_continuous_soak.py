#!/usr/bin/env python3
"""Run and verify a duration-enforced SDK-free CigVision soak.

This is deliberately separate from p8_soak_evidence.py v1, whose short
contract remains part of the frozen Windows evidence workflow.  The v2 tool
requires a project runtime summary, measures one live process group over time,
and cannot weaken its tracked profiles through command-line threshold flags.
"""

from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import hashlib
import json
import math
import os
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import threading
import time
import types
from pathlib import Path, PurePosixPath
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = Path(__file__).resolve()
TOOL_SOURCE_PATH = "scripts/p8_continuous_soak.py"
PROFILE_PATH = ROOT / "config" / "p8-continuous-soak-profiles-v1.json"
LEGACY_PATH = ROOT / "scripts" / "p8_soak_evidence.py"
LEGACY_SOURCE_PATH = "scripts/p8_soak_evidence.py"


def _capture_source_bytes(path: Path) -> bytes:
    """Read a regular source file and fail closed if it changes mid-capture."""
    try:
        first = path.lstat()
        if not stat.S_ISREG(first.st_mode):
            raise RuntimeError(f"source is not a regular file: {path}")
        data = path.read_bytes()
        second = path.lstat()
    except OSError as error:
        raise RuntimeError(f"cannot capture source: {path}") from error
    if (
        first.st_dev,
        first.st_ino,
        first.st_size,
        first.st_mtime_ns,
    ) != (
        second.st_dev,
        second.st_ino,
        second.st_size,
        second.st_mtime_ns,
    ):
        raise RuntimeError(f"source changed while capturing: {path}")
    return data


TOOL_SOURCE_BYTES = _capture_source_bytes(TOOL_PATH)
TOOL_SOURCE_SHA256 = hashlib.sha256(TOOL_SOURCE_BYTES).hexdigest()
LEGACY_SOURCE_BYTES = _capture_source_bytes(LEGACY_PATH)
LEGACY_SOURCE_SHA256 = hashlib.sha256(LEGACY_SOURCE_BYTES).hexdigest()
LEGACY = types.ModuleType("p8_soak_evidence")
LEGACY.__file__ = str(LEGACY_PATH)
LEGACY.__package__ = ""
exec(compile(LEGACY_SOURCE_BYTES, str(LEGACY_PATH), "exec"), LEGACY.__dict__)

MANIFEST_SCHEMA = "p8-continuous-soak-evidence-v2"
SAMPLES_SCHEMA = "p8-continuous-soak-samples-v1"
RUNTIME_SCHEMA = "cigvision-local-soak-runtime-v1"
PROGRESS_SCHEMA = "cigvision-local-soak-progress-v1"
BUILD_PROVENANCE_SCHEMA = "p8-continuous-soak-build-provenance-v1"
TOOL_DEPENDENCIES_SCHEMA = "p8-continuous-soak-tool-dependencies-v1"
PROFILE_SNAPSHOT_SCHEMA = "p8-continuous-soak-profile-snapshot-v2"

TRUSTED_POSIX_PS_PATHS = frozenset({"/bin/ps", "/usr/bin/ps"})
PS_VERSION_ARGUMENTS = ("--version",)

PROJECT_RUNTIME_KIND = "cigvision-project-local-soak"
TEST_RUNTIME_KIND = "test-helper"
PROJECT_RUNTIME_SOURCES = (
    "tests/CigVision.LocalSoak/LocalSoakRuntime.cpp",
    "01_上位机_QT_新版_CigVision/源码/core/BoundedQueue.h",
    "01_上位机_QT_新版_CigVision/源码/core/InspectionContracts.h",
    "01_上位机_QT_新版_CigVision/源码/core/InspectionInterfaces.h",
    "01_上位机_QT_新版_CigVision/源码/core/OfflineInspection.h",
    "01_上位机_QT_新版_CigVision/源码/core/ProductParameterProfile.h",
    "01_上位机_QT_新版_CigVision/源码/core/ProductRuntimeState.h",
    "01_上位机_QT_新版_CigVision/源码/core/Sha256.h",
)
PROJECT_COMPILE_FLAGS = (
    "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-pthread",
)
PROJECT_INCLUDE_PATH = "01_上位机_QT_新版_CigVision/源码"

def _is_msvc_compiler(path: str | os.PathLike[str]) -> bool:
    """Identify cl.exe so controlled Windows builds can use native flags."""
    return Path(os.fspath(path)).name.casefold() in {"cl", "cl.exe"}


def _project_compile_spec(
    compiler: str,
    standard: str,
    output: str,
) -> tuple[tuple[str, ...], list[str]]:
    """Return compiler flags and argv for MSVC or the existing GNU toolchain."""
    source = str(ROOT / PROJECT_RUNTIME_SOURCES[0])
    include = str(ROOT / PROJECT_INCLUDE_PATH)
    if _is_msvc_compiler(compiler):
        flags = ("/nologo", f"/std:{standard}", "/EHsc", "/O2", "/W3", "/WX")
        return flags, [compiler, *flags, f"/I{include}", source, f"/Fe:{output}"]
    flags = PROJECT_COMPILE_FLAGS
    return flags, [compiler, f"-std={standard}", *flags, f"-I{include}", source, "-o", output]


LOCKED_PROFILES: dict[str, dict[str, Any]] = {
    "contract-test-v1": {
        "durationClass": "test-only-short",
        "claimScope": "sdk-free-test-contract-only",
        "roundsPerRestart": 1,
        "restarts": 1,
        "minimumDurationSeconds": 0.12,
        "timeoutSeconds": 2.0,
        "sampleIntervalSeconds": 0.02,
        "terminationGraceSeconds": 0.5,
        "rssWarmupSeconds": 0.04,
        "minimumSampleCount": 4,
        "minimumProcessGroupCpuSeconds": 0.01,
        "maximumIntraRunRssGrowthBytes": 16 * 1024 * 1024,
        "minimumProgressRecordCount": 2,
        "maximumProgressGapSeconds": 0.5,
        "minimumSessionsCompleted": 1,
        "minimumFramesProcessed": 128,
        "minimumDiskFreeBytes": 1,
        "maximumOutputBytes": 4 * 1024 * 1024,
        "runtimeContractSchema": RUNTIME_SCHEMA,
    },
    "local-sdkfree-v1": {
        "durationClass": "continuous-local",
        "claimScope": "sdk-free-local-continuous-evidence-only",
        "roundsPerRestart": 2,
        "restarts": 1,
        "minimumDurationSeconds": 300.0,
        "timeoutSeconds": 330.0,
        "sampleIntervalSeconds": 1.0,
        "terminationGraceSeconds": 5.0,
        "rssWarmupSeconds": 60.0,
        "minimumSampleCount": 240,
        "minimumProcessGroupCpuSeconds": 10.0,
        "maximumIntraRunRssGrowthBytes": 64 * 1024 * 1024,
        "minimumProgressRecordCount": 240,
        "maximumProgressGapSeconds": 5.0,
        "minimumSessionsCompleted": 240,
        "minimumFramesProcessed": 122880,
        "minimumDiskFreeBytes": 1024 * 1024 * 1024,
        "maximumOutputBytes": 64 * 1024 * 1024,
        "runtimeContractSchema": RUNTIME_SCHEMA,
    },
}

RUNTIME_KEYS = {
    "schemaVersion", "sdkFree", "realIoEnabled", "realRejectEnabled",
    "productAcceptanceClaimed", "completed", "restart", "round", "iteration",
    "targetDurationSeconds", "durationSeconds", "sessionsCompleted",
    "framesReceived", "framesProcessed", "ok", "ng", "error", "dropped",
    "sourceErrors", "detectorErrors", "observerErrors", "saveFailures",
    "resultStores", "archiveStores", "productStateProcessed",
    "productStateOk", "productStateNg", "productStateError",
    "progressRecords",
    "maximumQueueDepth", "invariantsPassed",
}

PROGRESS_KEYS = {
    "schemaVersion", "restart", "round", "iteration", "sequence",
    "elapsedSeconds", "sessionsCompleted", "framesProcessed", "ok", "ng",
    "error", "productStateProcessed", "productStateOk", "productStateNg",
    "productStateError", "realIoEnabled", "realRejectEnabled",
    "invariantsPassed",
}

RUN_RESULT_KEYS = {
    "restart", "round", "iteration", "argv", "cwd", "startedAt", "endedAt",
    "durationSeconds", "processLifetimeSeconds", "exitCode", "timedOut", "crashed", "processSucceeded",
    "continuousContractSatisfied", "succeeded", "launchError",
    "terminationReason", "processGroupTermination", "outputDirectory", "stdout",
    "stderr", "samples", "finalOutputBytes", "maximumObservedOutputBytes",
    "minimumObservedDiskFreeBytes", "sampleCount", "completeResourceSampleCount",
    "resourceSampleCompleteness", "postWarmupSampleCount",
    "intraRunRssGrowthBytes", "cpuMonotonic", "cpuIntervalCount",
    "cpuPercentP50", "cpuPercentP95", "processGroupCpuTotalSeconds",
    "runtimeContract", "runtimeContractError",
    "commandRecord", "resultRecord",
}

SAMPLE_KEYS = {
    "sampledAt", "elapsedSeconds", "processGroupRssBytes",
    "processGroupCpuSeconds", "processGroupProcessCount", "resourceSource",
    "outputBytes", "diskFreeBytes", "gpu",
}

TERMINATION_KEYS = {
    "attempted", "method", "gracefulSignalSent", "forceSignalSent",
    "noResidualProcessConfirmed", "residualProcessIds", "trackedProcessIds",
    "treeEnumerationSucceeded",
}

MANIFEST_KEYS = {
    "schemaVersion", "profile", "startedAt", "endedAt", "overallResult",
    "sdkFree", "productAcceptanceClaimed", "windowsRuntimeVerified", "gpu",
    "gpuClaimed", "commandTemplate", "cwd", "summary", "checks", "runs",
    "failures", "evidenceFiles", "manifestExcludedFromSelfHash",
    "buildProvenance", "toolDependencies",
}


class ContinuousSoakError(ValueError):
    """Raised when a profile, runtime contract, or evidence bundle is invalid."""


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContinuousSoakError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise ContinuousSoakError(f"non-finite JSON number: {value}")


def _load_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            return json.load(
                stream,
                object_pairs_hook=_strict_object,
                parse_constant=_reject_constant,
            )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ContinuousSoakError(f"cannot read JSON {path}: {error}") from error


def _stable_regular_bytes(path: Path, label: str) -> tuple[Path, bytes]:
    try:
        if not path.is_absolute() or path.is_symlink():
            raise ContinuousSoakError(f"{label}: trusted absolute regular file required")
        resolved = path.resolve(strict=True)
        data = _capture_source_bytes(resolved)
    except RuntimeError as error:
        raise ContinuousSoakError(f"{label}: {error}") from error
    except OSError as error:
        raise ContinuousSoakError(f"{label}: regular file unavailable") from error
    return resolved, data


def _assert_tool_sources_unchanged() -> None:
    expected = (
        (TOOL_PATH, TOOL_SOURCE_BYTES, "continuous soak tool"),
        (LEGACY_PATH, LEGACY_SOURCE_BYTES, "legacy soak helper"),
    )
    for path, loaded_bytes, label in expected:
        resolved, current = _stable_regular_bytes(path, label)
        if resolved != path.resolve(strict=True) or current != loaded_bytes:
            raise ContinuousSoakError(f"{label} changed after it was loaded")


def _probe_resource_collector(path: Path) -> dict[str, Any]:
    argv = [str(path), *PS_VERSION_ARGUMENTS]
    environment = os.environ.copy()
    environment.update({"LC_ALL": "C", "LANG": "C"})
    try:
        completed = subprocess.run(
            argv,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=environment,
            timeout=5.0,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ContinuousSoakError(
            f"cannot probe system ps resource collector: {error}"
        ) from error
    output_size = len(completed.stdout.encode("utf-8")) + len(
        completed.stderr.encode("utf-8")
    )
    if output_size > 64 * 1024:
        raise ContinuousSoakError("system ps version probe output is too large")
    return {
        "argv": argv,
        "exitCode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def _resource_collector_record() -> dict[str, Any]:
    if os.name == "nt":
        return {
            "platform": sys.platform,
            "kind": "windows-native",
            "resourceSource": "windows-process-tree",
            "canonicalPath": "",
            "invocationPath": "",
            "size": 0,
            "sha256": "",
            "versionProbe": None,
            "resourceArgv": [],
            "treeArgv": [],
        }
    if sys.platform.startswith("linux"):
        output_schema = "pid=,pgid=,rss=,stat="
        resource_source = "proc-process-group"
    elif sys.platform == "darwin":
        output_schema = "pid=,pgid=,rss=,time=,stat="
        resource_source = "ps-process-group"
    else:
        raise ContinuousSoakError("unsupported POSIX resource collector platform")
    candidate_value = shutil.which("ps")
    if candidate_value is None:
        raise ContinuousSoakError("system ps resource collector is required")
    candidate = Path(os.path.abspath(candidate_value))
    resolved, data = _stable_regular_bytes(candidate, "system ps resource collector")
    if str(resolved) not in TRUSTED_POSIX_PS_PATHS:
        raise ContinuousSoakError(
            "PATH ps must resolve to trusted /bin/ps or /usr/bin/ps"
        )
    return {
        "platform": sys.platform,
        "kind": "posix-ps",
        "resourceSource": resource_source,
        "canonicalPath": str(resolved),
        "invocationPath": str(resolved),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "versionProbe": _probe_resource_collector(resolved),
        "resourceArgv": [str(resolved), "-axo", output_schema],
        "treeArgv": [str(resolved), "-axo", "pid=,pgid=,stat="],
    }


def _assert_resource_collector_identity(record: dict[str, Any]) -> None:
    expected_keys = {
        "platform", "kind", "resourceSource", "canonicalPath",
        "invocationPath", "size", "sha256", "versionProbe",
        "resourceArgv", "treeArgv",
    }
    if not isinstance(record, dict) or set(record) != expected_keys:
        raise ContinuousSoakError("resource collector exact shape mismatch")
    if record["platform"] != sys.platform:
        raise ContinuousSoakError("resource collector platform mismatch")
    if record["kind"] == "windows-native":
        if record != _resource_collector_record():
            raise ContinuousSoakError("Windows native resource collector mismatch")
        return
    if record["kind"] != "posix-ps":
        raise ContinuousSoakError("unsupported resource collector kind")
    path = Path(str(record["canonicalPath"]))
    resolved, data = _stable_regular_bytes(path, "recorded ps resource collector")
    expected = _resource_collector_record()
    if (
        str(resolved) != record["canonicalPath"]
        or record["invocationPath"] != record["canonicalPath"]
        or record["size"] != len(data)
        or record["sha256"] != hashlib.sha256(data).hexdigest()
        or record != expected
    ):
        raise ContinuousSoakError("recorded ps resource collector identity mismatch")


def _load_profiles() -> dict[str, dict[str, Any]]:
    document = _load_json(PROFILE_PATH)
    if not isinstance(document, dict) or set(document) != {"schemaVersion", "profiles"}:
        raise ContinuousSoakError("continuous soak profile document shape mismatch")
    if document["schemaVersion"] != "p8-continuous-soak-profiles-v1":
        raise ContinuousSoakError("continuous soak profile schema mismatch")
    if document["profiles"] != LOCKED_PROFILES:
        raise ContinuousSoakError("tracked continuous soak profiles differ from locked values")
    return LOCKED_PROFILES


def _number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ContinuousSoakError(f"{label}: finite number required")
    result = float(value)
    if not math.isfinite(result):
        raise ContinuousSoakError(f"{label}: finite number required")
    return result


def _integer(value: Any, label: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ContinuousSoakError(f"{label}: integer >= {minimum} required")
    return value


def _boolean(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        raise ContinuousSoakError(f"{label}: boolean required")
    return value


def _string(value: Any, label: str) -> str:
    if not isinstance(value, str):
        raise ContinuousSoakError(f"{label}: string required")
    return value


def _timestamp(value: Any, label: str) -> dt.datetime:
    text = _string(value, label)
    if not text.endswith("Z"):
        raise ContinuousSoakError(f"{label}: UTC Z timestamp required")
    try:
        parsed = dt.datetime.fromisoformat(text[:-1] + "+00:00")
    except ValueError as error:
        raise ContinuousSoakError(f"{label}: invalid timestamp") from error
    if parsed.tzinfo is None or parsed.utcoffset() != dt.timedelta(0):
        raise ContinuousSoakError(f"{label}: UTC timestamp required")
    return parsed


def _evidence_path(root: Path, value: Any, label: str) -> Path:
    text = _string(value, label)
    pure = PurePosixPath(text)
    if (
        not text
        or text != pure.as_posix()
        or pure.is_absolute()
        or "\\" in text
        or any(":" in part for part in pure.parts)
        or any(part in {"", ".", ".."} for part in pure.parts)
    ):
        raise ContinuousSoakError(f"{label}: safe relative POSIX path required")
    return root.joinpath(*pure.parts)


def _regular_evidence_file(root: Path, value: Any, label: str) -> Path:
    path = _evidence_path(root, value, label)
    try:
        info = path.lstat()
    except OSError as error:
        raise ContinuousSoakError(f"{label}: evidence file unavailable") from error
    if not stat.S_ISREG(info.st_mode):
        raise ContinuousSoakError(f"{label}: regular evidence file required")
    return path


def _evidence_directory(root: Path, value: Any, label: str) -> Path:
    path = _evidence_path(root, value, label)
    try:
        info = path.lstat()
    except OSError as error:
        raise ContinuousSoakError(f"{label}: evidence directory unavailable") from error
    if not stat.S_ISDIR(info.st_mode):
        raise ContinuousSoakError(f"{label}: regular evidence directory required")
    return path


def _validate_file_reference(root: Path, value: Any, label: str) -> Path:
    if not isinstance(value, dict) or set(value) != {"path", "size", "sha256"}:
        raise ContinuousSoakError(f"{label}: file reference shape mismatch")
    path = _regular_evidence_file(root, value["path"], f"{label}.path")
    expected = LEGACY._file_reference(path, root)
    if value != expected:
        raise ContinuousSoakError(f"{label}: file reference mismatch")
    return path


def _tool_source_record(relative: str, data: bytes) -> dict[str, Any]:
    return {
        "repoRelativeSourcePath": relative,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def _snapshot_tool_dependencies(
    evidence_root: Path,
    resource_collector: dict[str, Any],
) -> dict[str, Any]:
    _assert_tool_sources_unchanged()
    _assert_resource_collector_identity(resource_collector)
    provenance = evidence_root / "provenance"
    provenance.mkdir(exist_ok=True)
    source_root = provenance / "tool-sources"
    source_root.mkdir()
    source_values = (
        (TOOL_SOURCE_PATH, TOOL_SOURCE_BYTES, "000-p8_continuous_soak.py"),
        (LEGACY_SOURCE_PATH, LEGACY_SOURCE_BYTES, "001-p8_soak_evidence.py"),
    )
    source_records: list[dict[str, Any]] = []
    source_snapshots: list[dict[str, Any]] = []
    for relative, data, filename in source_values:
        snapshot = source_root / filename
        snapshot.write_bytes(data)
        record = _tool_source_record(relative, data)
        if (
            snapshot.stat().st_size != record["size"]
            or LEGACY._sha256_file(snapshot) != record["sha256"]
        ):
            raise ContinuousSoakError("tool source changed while snapshotting")
        source_records.append(record)
        source_snapshots.append({
            "sourcePath": relative,
            "size": record["size"],
            "sha256": record["sha256"],
            "snapshot": LEGACY._file_reference(snapshot, evidence_root),
        })

    resource_snapshot_reference: dict[str, Any] | None = None
    if resource_collector["kind"] == "posix-ps":
        resource_root = provenance / "resource-tools"
        resource_root.mkdir()
        resource_snapshot = resource_root / "ps"
        shutil.copyfile(resource_collector["canonicalPath"], resource_snapshot)
        if (
            resource_snapshot.stat().st_size != resource_collector["size"]
            or LEGACY._sha256_file(resource_snapshot)
                != resource_collector["sha256"]
        ):
            raise ContinuousSoakError("ps resource collector changed while snapshotting")
        resource_snapshot_reference = {
            "sourcePath": resource_collector["canonicalPath"],
            "size": resource_collector["size"],
            "sha256": resource_collector["sha256"],
            "snapshot": LEGACY._file_reference(resource_snapshot, evidence_root),
        }

    record_path = provenance / "tool-dependencies.json"
    LEGACY._atomic_write_json(record_path, {
        "schemaVersion": TOOL_DEPENDENCIES_SCHEMA,
        "sources": source_records,
        "resourceCollector": resource_collector,
    })
    return {
        "record": LEGACY._file_reference(record_path, evidence_root),
        "sourceSnapshots": source_snapshots,
        "resourceCollectorSnapshot": resource_snapshot_reference,
    }


def _verify_tool_dependencies_snapshot(root: Path, value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != {
        "record", "sourceSnapshots", "resourceCollectorSnapshot",
    }:
        raise ContinuousSoakError("manifest tool dependencies exact shape mismatch")
    record_path = _validate_file_reference(root, value["record"], "tools.record")
    document = _load_json(record_path)
    if not isinstance(document, dict) or set(document) != {
        "schemaVersion", "sources", "resourceCollector",
    } or document["schemaVersion"] != TOOL_DEPENDENCIES_SCHEMA:
        raise ContinuousSoakError("tool dependencies record mismatch")
    records = document["sources"]
    snapshots = value["sourceSnapshots"]
    expected_sources = (
        (TOOL_SOURCE_PATH, TOOL_PATH, TOOL_SOURCE_BYTES),
        (LEGACY_SOURCE_PATH, LEGACY_PATH, LEGACY_SOURCE_BYTES),
    )
    if (
        not isinstance(records, list)
        or not isinstance(snapshots, list)
        or len(records) != len(expected_sources)
        or len(snapshots) != len(expected_sources)
    ):
        raise ContinuousSoakError("tool source inventory mismatch")
    for index, (record, snapshot, expected) in enumerate(
        zip(records, snapshots, expected_sources)
    ):
        relative, current_path, loaded_bytes = expected
        if not isinstance(record, dict) or set(record) != {
            "repoRelativeSourcePath", "size", "sha256",
        }:
            raise ContinuousSoakError("tool source record exact shape mismatch")
        if not isinstance(snapshot, dict) or set(snapshot) != {
            "sourcePath", "size", "sha256", "snapshot",
        }:
            raise ContinuousSoakError("tool source snapshot exact shape mismatch")
        snapshot_path = _validate_file_reference(
            root, snapshot["snapshot"], f"tools.sourceSnapshots[{index}]"
        )
        resolved, current_bytes = _stable_regular_bytes(
            current_path, f"current tool source {relative}"
        )
        expected_path = (ROOT / relative).resolve(strict=True)
        expected_record = _tool_source_record(relative, loaded_bytes)
        if (
            resolved != expected_path
            or current_bytes != loaded_bytes
            or record != expected_record
            or snapshot["sourcePath"] != relative
            or snapshot["size"] != record["size"]
            or snapshot["sha256"] != record["sha256"]
            or snapshot_path.stat().st_size != record["size"]
            or LEGACY._sha256_file(snapshot_path) != record["sha256"]
        ):
            raise ContinuousSoakError("tool source record/snapshot/current mismatch")

    collector = document["resourceCollector"]
    _assert_resource_collector_identity(collector)
    collector_snapshot = value["resourceCollectorSnapshot"]
    if collector["kind"] == "posix-ps":
        if not isinstance(collector_snapshot, dict) or set(collector_snapshot) != {
            "sourcePath", "size", "sha256", "snapshot",
        }:
            raise ContinuousSoakError("ps collector snapshot exact shape mismatch")
        snapshot_path = _validate_file_reference(
            root,
            collector_snapshot["snapshot"],
            "tools.resourceCollectorSnapshot.snapshot",
        )
        if (
            collector_snapshot["sourcePath"] != collector["canonicalPath"]
            or collector_snapshot["size"] != collector["size"]
            or collector_snapshot["sha256"] != collector["sha256"]
            or
            snapshot_path.stat().st_size != collector["size"]
            or LEGACY._sha256_file(snapshot_path) != collector["sha256"]
        ):
            raise ContinuousSoakError("ps collector snapshot identity mismatch")
    elif collector_snapshot is not None:
        raise ContinuousSoakError("native collector must not have a binary snapshot")
    return collector


def _resolved_regular_file(value: Any, label: str, base: Path | None = None) -> Path:
    text = _string(value, label)
    path = Path(text)
    if not path.is_absolute():
        if base is None:
            raise ContinuousSoakError(f"{label}: absolute path required")
        path = base / path
    try:
        resolved = path.resolve(strict=True)
        info = resolved.lstat()
    except OSError as error:
        raise ContinuousSoakError(f"{label}: regular file unavailable") from error
    if not stat.S_ISREG(info.st_mode):
        raise ContinuousSoakError(f"{label}: regular file required")
    return resolved


def _validate_build_file_record(
    value: Any,
    label: str,
    base: Path | None = None,
) -> Path:
    if not isinstance(value, dict) or set(value) != {"path", "size", "sha256"}:
        raise ContinuousSoakError(f"{label}: build file record shape mismatch")
    path = _resolved_regular_file(value["path"], f"{label}.path", base)
    if (
        _integer(value["size"], f"{label}.size", 1) != path.stat().st_size
        or _string(value["sha256"], f"{label}.sha256")
            != LEGACY._sha256_file(path)
    ):
        raise ContinuousSoakError(f"{label}: build file identity mismatch")
    return path


def _validate_build_provenance(
    path: Path,
    command: list[str],
    profile_name: str,
) -> tuple[dict[str, Any], Path, list[Path]]:
    document = _load_json(path)
    if not isinstance(document, dict) or set(document) != {
        "schemaVersion", "runtimeKind", "compiler", "compile",
        "sourceFiles", "executable",
    }:
        raise ContinuousSoakError("build provenance exact shape mismatch")
    if document["schemaVersion"] != BUILD_PROVENANCE_SCHEMA:
        raise ContinuousSoakError("build provenance schema mismatch")
    runtime_kind = _string(document["runtimeKind"], "build runtimeKind")
    if runtime_kind not in {PROJECT_RUNTIME_KIND, TEST_RUNTIME_KIND}:
        raise ContinuousSoakError("unsupported build runtime kind")
    if profile_name == "local-sdkfree-v1" and runtime_kind != PROJECT_RUNTIME_KIND:
        raise ContinuousSoakError("local continuous profile requires the project C++ runner")

    compiler = document["compiler"]
    if not isinstance(compiler, dict) or set(compiler) != {
        "path", "size", "sha256", "version",
    }:
        raise ContinuousSoakError("build compiler exact shape mismatch")
    compiler_path = _validate_build_file_record(
        {key: compiler[key] for key in ("path", "size", "sha256")},
        "build compiler",
    )
    if not _string(compiler["version"], "build compiler.version").strip():
        raise ContinuousSoakError("build compiler version must be nonempty")

    compile_record = document["compile"]
    if not isinstance(compile_record, dict) or set(compile_record) != {
        "standard", "flags", "includePath", "argv",
    }:
        raise ContinuousSoakError("build compile exact shape mismatch")
    standard = _string(compile_record["standard"], "build compile.standard")
    flags = compile_record["flags"]
    if not isinstance(flags, list) or not all(isinstance(item, str) for item in flags):
        raise ContinuousSoakError("build compile flags must be strings")
    include_path = _string(compile_record["includePath"], "build compile.includePath")
    compile_argv = compile_record["argv"]
    if not isinstance(compile_argv, list) or not all(
        isinstance(item, str) and item for item in compile_argv
    ):
        raise ContinuousSoakError("build compile argv must contain strings")

    executable = _validate_build_file_record(document["executable"], "build executable")
    command_executable = _resolved_regular_file(command[0], "command executable")
    if executable != command_executable:
        raise ContinuousSoakError("build provenance executable does not match command")

    source_values = document["sourceFiles"]
    if not isinstance(source_values, list) or not source_values:
        raise ContinuousSoakError("build sourceFiles must be a nonempty array")
    source_paths: list[Path] = []
    if runtime_kind == PROJECT_RUNTIME_KIND:
        if standard not in ({"c++17", "c++14"} if profile_name == "contract-test-v1"
                            else {"c++17"}):
            raise ContinuousSoakError("project runner standard is not allowed by profile")
        expected_flags, expected_argv = _project_compile_spec(
            str(compiler_path), standard, document["executable"]["path"]
        )
        if tuple(flags) != expected_flags or include_path != PROJECT_INCLUDE_PATH:
            raise ContinuousSoakError("project runner compile flags mismatch")
        recorded_paths = tuple(
            _string(item.get("path") if isinstance(item, dict) else None,
                    "build source.path")
            for item in source_values
        )
        if recorded_paths != PROJECT_RUNTIME_SOURCES:
            raise ContinuousSoakError("project runner source set mismatch")
        for index, item in enumerate(source_values):
            source_paths.append(_validate_build_file_record(
                item, f"build sourceFiles[{index}]", ROOT,
            ))
        if compile_argv != expected_argv:
            raise ContinuousSoakError("project runner compile argv mismatch")
    else:
        if profile_name != "contract-test-v1":
            raise ContinuousSoakError("test helper is allowed only by contract profile")
        if (
            standard != "python" or flags != [] or include_path != ""
            or compile_argv != []
        ):
            raise ContinuousSoakError("test-helper compile record mismatch")
        for index, item in enumerate(source_values):
            source_paths.append(_validate_build_file_record(
                item, f"build sourceFiles[{index}]",
            ))
        if executable not in source_paths:
            raise ContinuousSoakError("test-helper command source is not provenance-bound")
    return document, executable, source_paths


def _snapshot_build_provenance(
    evidence_root: Path,
    document: dict[str, Any],
    executable: Path,
    source_paths: list[Path],
) -> dict[str, Any]:
    provenance = evidence_root / "provenance"
    record_path = provenance / "build-provenance.json"
    LEGACY._atomic_write_json(record_path, document)
    executable_snapshot = provenance / f"runtime-executable{executable.suffix}"
    shutil.copy2(executable, executable_snapshot)
    if LEGACY._sha256_file(executable_snapshot) != document["executable"]["sha256"]:
        raise ContinuousSoakError("runtime executable changed while snapshotting")
    source_root = provenance / "runtime-sources"
    source_root.mkdir()
    source_snapshots: list[dict[str, Any]] = []
    for index, (record, source) in enumerate(zip(document["sourceFiles"], source_paths)):
        snapshot = source_root / f"{index:03d}-{source.name}"
        shutil.copyfile(source, snapshot)
        if LEGACY._sha256_file(snapshot) != record["sha256"]:
            raise ContinuousSoakError("runtime source changed while snapshotting")
        source_snapshots.append({
            "sourcePath": record["path"],
            "snapshot": LEGACY._file_reference(snapshot, evidence_root),
        })
    return {
        "runtimeKind": document["runtimeKind"],
        "record": LEGACY._file_reference(record_path, evidence_root),
        "executableSnapshot": LEGACY._file_reference(
            executable_snapshot, evidence_root
        ),
        "sourceSnapshots": source_snapshots,
    }


def _verify_build_provenance_snapshot(
    root: Path,
    value: Any,
    command_template: list[str],
    profile_name: str,
) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != {
        "runtimeKind", "record", "executableSnapshot", "sourceSnapshots",
    }:
        raise ContinuousSoakError("manifest build provenance exact shape mismatch")
    record_path = _validate_file_reference(root, value["record"], "build.record")
    executable_snapshot = _validate_file_reference(
        root, value["executableSnapshot"], "build.executableSnapshot"
    )
    document = _load_json(record_path)
    if not isinstance(document, dict) or set(document) != {
        "schemaVersion", "runtimeKind", "compiler", "compile",
        "sourceFiles", "executable",
    } or document["schemaVersion"] != BUILD_PROVENANCE_SCHEMA:
        raise ContinuousSoakError("snapshotted build provenance mismatch")
    runtime_kind = document["runtimeKind"]
    if value["runtimeKind"] != runtime_kind or runtime_kind not in {
        PROJECT_RUNTIME_KIND, TEST_RUNTIME_KIND,
    }:
        raise ContinuousSoakError("snapshotted runtime kind mismatch")
    if profile_name == "local-sdkfree-v1" and runtime_kind != PROJECT_RUNTIME_KIND:
        raise ContinuousSoakError("local profile build provenance kind mismatch")
    executable = document["executable"]
    if not isinstance(executable, dict) or set(executable) != {
        "path", "size", "sha256",
    }:
        raise ContinuousSoakError("snapshotted executable record shape mismatch")
    if (
        not command_template
        or command_template[0] != (
            "{evidence_root}/" + value["executableSnapshot"]["path"]
        )
        or executable_snapshot.stat().st_size != executable["size"]
        or LEGACY._sha256_file(executable_snapshot) != executable["sha256"]
    ):
        raise ContinuousSoakError("snapshotted executable identity mismatch")
    compiler = document["compiler"]
    if not isinstance(compiler, dict) or set(compiler) != {
        "path", "size", "sha256", "version",
    }:
        raise ContinuousSoakError("snapshotted compiler record shape mismatch")
    _string(compiler["path"], "snapshotted compiler.path")
    _integer(compiler["size"], "snapshotted compiler.size", 1)
    _string(compiler["sha256"], "snapshotted compiler.sha256")
    if not _string(compiler["version"], "snapshotted compiler.version").strip():
        raise ContinuousSoakError("snapshotted compiler version is empty")
    source_records = document["sourceFiles"]
    source_snapshots = value["sourceSnapshots"]
    if (
        not isinstance(source_records, list)
        or not isinstance(source_snapshots, list)
        or len(source_records) != len(source_snapshots)
        or not source_records
    ):
        raise ContinuousSoakError("snapshotted source inventory mismatch")
    for index, (record, snapshot) in enumerate(zip(source_records, source_snapshots)):
        if not isinstance(record, dict) or set(record) != {"path", "size", "sha256"}:
            raise ContinuousSoakError("snapshotted source record shape mismatch")
        if not isinstance(snapshot, dict) or set(snapshot) != {"sourcePath", "snapshot"}:
            raise ContinuousSoakError("manifest source snapshot shape mismatch")
        snapshot_path = _validate_file_reference(
            root, snapshot["snapshot"], f"build.sourceSnapshots[{index}]"
        )
        if (
            snapshot["sourcePath"] != record["path"]
            or snapshot_path.stat().st_size != record["size"]
            or LEGACY._sha256_file(snapshot_path) != record["sha256"]
        ):
            raise ContinuousSoakError("snapshotted source identity mismatch")
    compile_record = document["compile"]
    if not isinstance(compile_record, dict) or set(compile_record) != {
        "standard", "flags", "includePath", "argv",
    }:
        raise ContinuousSoakError("snapshotted compile record shape mismatch")
    if runtime_kind == PROJECT_RUNTIME_KIND:
        expected_flags, expected_argv = _project_compile_spec(
            compiler["path"], compile_record["standard"], executable["path"]
        )
        if (
            tuple(item["path"] for item in source_records) != PROJECT_RUNTIME_SOURCES
            or tuple(compile_record["flags"]) != expected_flags
            or compile_record["includePath"] != PROJECT_INCLUDE_PATH
            or compile_record["argv"] != expected_argv
            or compile_record["standard"] not in (
                {"c++17", "c++14"} if profile_name == "contract-test-v1"
                else {"c++17"}
            )
        ):
            raise ContinuousSoakError("snapshotted project build contract mismatch")
        for record in source_records:
            current = ROOT / record["path"]
            if (
                not current.is_file()
                or current.is_symlink()
                or current.stat().st_size != record["size"]
                or LEGACY._sha256_file(current) != record["sha256"]
            ):
                raise ContinuousSoakError("current project source differs from evidence")
    elif (
        profile_name != "contract-test-v1"
        or compile_record != {
            "standard": "python", "flags": [], "includePath": "", "argv": [],
        }
    ):
        raise ContinuousSoakError("snapshotted test-helper contract mismatch")
    return document


def _parse_cpu_time(value: str) -> float:
    text = value.strip()
    days = 0
    if "-" in text:
        day_text, text = text.split("-", 1)
        days = int(day_text)
    fields = text.split(":")
    if len(fields) == 3:
        hours, minutes, seconds = int(fields[0]), int(fields[1]), float(fields[2])
    elif len(fields) == 2:
        hours, minutes, seconds = 0, int(fields[0]), float(fields[1])
    else:
        raise ValueError("unsupported CPU time")
    return days * 86400.0 + hours * 3600.0 + minutes * 60.0 + seconds


def _parse_linux_proc_cpu_seconds(value: str, clock_ticks: int) -> float:
    closing = value.rfind(")")
    if closing < 0 or clock_ticks <= 0:
        raise ValueError("invalid Linux process stat")
    fields = value[closing + 1:].split()
    if len(fields) < 13:
        raise ValueError("short Linux process stat")
    user_ticks = int(fields[11])
    system_ticks = int(fields[12])
    if user_ticks < 0 or system_ticks < 0:
        raise ValueError("negative Linux CPU ticks")
    return (user_ticks + system_ticks) / float(clock_ticks)


def _linux_cpu_seconds(pid: int) -> float | None:
    try:
        clock_ticks = int(os.sysconf("SC_CLK_TCK"))
        value = (Path("/proc") / str(pid) / "stat").read_text(
            encoding="ascii", errors="strict"
        )
        return _parse_linux_proc_cpu_seconds(value, clock_ticks)
    except (OSError, UnicodeError, ValueError):
        return None


def _posix_group_resources(
    group_id: int,
    resource_collector: dict[str, Any],
) -> tuple[int, float, int, str] | None:
    linux = sys.platform.startswith("linux")
    try:
        completed = subprocess.run(
            resource_collector["resourceArgv"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="ascii",
            errors="replace",
            timeout=2.0,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    rss_bytes = 0
    cpu_seconds = 0.0
    count = 0
    try:
        for line in completed.stdout.splitlines():
            fields = line.split()
            expected = 4 if linux else 5
            if len(fields) < expected or int(fields[1]) != group_id:
                continue
            process_state = fields[3] if linux else fields[4]
            if process_state.startswith("Z"):
                continue
            rss_bytes += int(fields[2]) * 1024
            if linux:
                process_cpu = _linux_cpu_seconds(int(fields[0]))
                if process_cpu is None:
                    return None
                cpu_seconds += process_cpu
            else:
                cpu_seconds += _parse_cpu_time(fields[3])
            count += 1
    except (ValueError, OverflowError):
        return None
    if count == 0:
        return None
    return (
        rss_bytes,
        cpu_seconds,
        count,
        resource_collector["resourceSource"],
    )


def _posix_live_process_group_pids(
    group_id: int,
    resource_collector: dict[str, Any],
) -> list[int] | None:
    try:
        completed = subprocess.run(
            resource_collector["treeArgv"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="ascii",
            errors="replace",
            timeout=1.0,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    live: list[int] = []
    for line in completed.stdout.splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        try:
            pid = int(fields[0])
            process_group = int(fields[1])
        except ValueError:
            continue
        if process_group == group_id and not fields[2].startswith("Z"):
            live.append(pid)
    return sorted(set(live))


def _posix_process_group_exists(
    group_id: int,
    resource_collector: dict[str, Any],
) -> bool:
    live_pids = _posix_live_process_group_pids(group_id, resource_collector)
    if live_pids is not None:
        return bool(live_pids)
    try:
        os.killpg(group_id, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _posix_wait_until_gone(
    group_id: int,
    seconds: float,
    resource_collector: dict[str, Any],
) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if not _posix_process_group_exists(group_id, resource_collector):
            return True
        time.sleep(min(0.025, max(0.0, deadline - time.monotonic())))
    return not _posix_process_group_exists(group_id, resource_collector)


def _windows_cpu_seconds(pid: int, process_handle: int | None = None) -> float | None:
    if os.name != "nt":
        return None
    try:
        from ctypes import wintypes

        class FileTime(ctypes.Structure):
            _fields_ = [("low", wintypes.DWORD), ("high", wintypes.DWORD)]

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.OpenProcess.restype = ctypes.c_void_p
        kernel32.OpenProcess.argtypes = [
            wintypes.DWORD, wintypes.BOOL, wintypes.DWORD,
        ]
        kernel32.GetProcessTimes.restype = wintypes.BOOL
        kernel32.GetProcessTimes.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(FileTime),
            ctypes.POINTER(FileTime),
            ctypes.POINTER(FileTime),
            ctypes.POINTER(FileTime),
        ]
        kernel32.CloseHandle.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = process_handle or kernel32.OpenProcess(0x1000, False, pid)
        if not handle:
            return None
        try:
            creation = FileTime()
            exit_time = FileTime()
            kernel = FileTime()
            user = FileTime()
            if not kernel32.GetProcessTimes(
                handle,
                ctypes.byref(creation),
                ctypes.byref(exit_time),
                ctypes.byref(kernel),
                ctypes.byref(user),
            ):
                return None
            kernel_ticks = (int(kernel.high) << 32) | int(kernel.low)
            user_ticks = (int(user.high) << 32) | int(user.low)
            return (kernel_ticks + user_ticks) / 10_000_000.0
        finally:
            if not process_handle:
                kernel32.CloseHandle(handle)
    except (AttributeError, OSError, TypeError, ValueError):
        return None


def _windows_rss_bytes(pid: int, process_handle: int | None = None) -> int | None:
    if os.name != "nt":
        return None
    try:
        from ctypes import wintypes

        class ProcessMemoryCounters(ctypes.Structure):
            _fields_ = [
                ("cb", wintypes.DWORD),
                ("pageFaultCount", wintypes.DWORD),
                ("peakWorkingSetSize", ctypes.c_size_t),
                ("workingSetSize", ctypes.c_size_t),
                ("quotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("quotaPagedPoolUsage", ctypes.c_size_t),
                ("quotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("quotaNonPagedPoolUsage", ctypes.c_size_t),
                ("pagefileUsage", ctypes.c_size_t),
                ("peakPagefileUsage", ctypes.c_size_t),
            ]

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        psapi = ctypes.WinDLL("psapi", use_last_error=True)
        kernel32.OpenProcess.restype = ctypes.c_void_p
        kernel32.OpenProcess.argtypes = [
            wintypes.DWORD, wintypes.BOOL, wintypes.DWORD,
        ]
        kernel32.CloseHandle.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
        psapi.GetProcessMemoryInfo.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ProcessMemoryCounters),
            wintypes.DWORD,
        ]
        handle = process_handle or kernel32.OpenProcess(0x0400 | 0x0010, False, pid)
        if not handle:
            return None
        try:
            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            if not psapi.GetProcessMemoryInfo(
                handle, ctypes.byref(counters), counters.cb,
            ):
                return None
            return int(counters.workingSetSize)
        finally:
            if not process_handle:
                kernel32.CloseHandle(handle)
    except (AttributeError, OSError, TypeError, ValueError):
        return None


def _windows_group_resources(
    root_pid: int, root_process_handle: int | None = None
) -> tuple[int, float, int, str] | None:
    process_ids = LEGACY._windows_process_tree_pids(root_pid)
    if process_ids is None and root_process_handle is None:
        return None
    if process_ids is None:
        process_ids = [root_pid]
    rss_bytes = 0
    cpu_seconds = 0.0
    count = 0
    for pid in process_ids:
        root_handle = root_process_handle if pid == root_pid else None
        rss = _windows_rss_bytes(pid, root_handle)
        cpu = _windows_cpu_seconds(
            pid, root_process_handle if pid == root_pid else None
        )
        if rss is None or cpu is None:
            return None
        rss_bytes += rss
        cpu_seconds += cpu
        count += 1
    if count == 0:
        return None
    return rss_bytes, cpu_seconds, count, "windows-process-tree"


def _group_resources(
    root_pid: int,
    group_id: int,
    resource_collector: dict[str, Any],
    root_process_handle: int | None = None,
) -> tuple[int, float, int, str] | None:
    if os.name == "nt":
        return _windows_group_resources(root_pid, root_process_handle)
    return _posix_group_resources(group_id, resource_collector)


def _sample(
    root_pid: int,
    group_id: int,
    output_dir: Path,
    stdout_path: Path,
    stderr_path: Path,
    evidence_root: Path,
    started_monotonic: float,
    resource_collector: dict[str, Any],
    root_process_handle: int | None = None,
) -> dict[str, Any]:
    resources = _group_resources(
        root_pid, group_id, resource_collector, root_process_handle
    )
    try:
        disk_free = shutil.disk_usage(evidence_root).free
    except OSError:
        disk_free = None
    return {
        "sampledAt": LEGACY._utc_now(),
        "elapsedSeconds": round(time.monotonic() - started_monotonic, 6),
        "processGroupRssBytes": None if resources is None else resources[0],
        "processGroupCpuSeconds": None if resources is None else round(resources[1], 6),
        "processGroupProcessCount": None if resources is None else resources[2],
        "resourceSource": "not-collected" if resources is None else resources[3],
        "outputBytes": LEGACY._directory_size((output_dir, stdout_path, stderr_path)),
        "diskFreeBytes": disk_free,
        "gpu": "not-collected",
    }


def _terminate_process_group_v2(
    process: subprocess.Popen[Any],
    group_id: int,
    grace_seconds: float,
    tracked_process_ids: set[int] | None = None,
    resource_collector: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Terminate a v2 process tree without racing the dedicated wait thread."""
    tracked = set(tracked_process_ids or ())
    tracked.add(process.pid)
    result: dict[str, Any] = {
        "attempted": True,
        "method": "taskkill-tree" if os.name == "nt" else "posix-process-group",
        "gracefulSignalSent": False,
        "forceSignalSent": False,
        "noResidualProcessConfirmed": False,
        "residualProcessIds": [],
        "trackedProcessIds": sorted(tracked),
        "treeEnumerationSucceeded": os.name != "nt",
    }
    if os.name == "nt":
        descendants = LEGACY._windows_process_tree_pids(process.pid)
        result["treeEnumerationSucceeded"] = descendants is not None
        if descendants is not None:
            tracked.update(descendants)
        targets = [process.pid] + sorted(tracked - {process.pid})
        for target_pid in targets:
            if not LEGACY._process_exists(target_pid):
                continue
            try:
                completed = subprocess.run(
                    ["taskkill", "/PID", str(target_pid), "/T", "/F"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    timeout=max(1.0, grace_seconds),
                    check=False,
                )
                result["forceSignalSent"] = (
                    result["forceSignalSent"] or completed.returncode == 0
                )
            except (OSError, subprocess.TimeoutExpired):
                if target_pid == process.pid:
                    try:
                        process.kill()
                        result["forceSignalSent"] = True
                    except OSError:
                        pass
        residual = LEGACY._wait_windows_pids_gone(tracked, grace_seconds)
        result["trackedProcessIds"] = sorted(tracked)
        result["residualProcessIds"] = residual
        result["noResidualProcessConfirmed"] = (
            bool(result["treeEnumerationSucceeded"]) and not residual
        )
        return result

    if resource_collector is None:
        resource_collector = _resource_collector_record()
    _assert_resource_collector_identity(resource_collector)
    try:
        os.killpg(group_id, signal.SIGTERM)
        result["gracefulSignalSent"] = True
    except (ProcessLookupError, PermissionError):
        pass
    if not _posix_wait_until_gone(group_id, grace_seconds, resource_collector):
        try:
            os.killpg(group_id, signal.SIGKILL)
            result["forceSignalSent"] = True
        except (ProcessLookupError, PermissionError):
            pass
    result["noResidualProcessConfirmed"] = _posix_wait_until_gone(
        group_id, grace_seconds, resource_collector
    )
    residual_pids = _posix_live_process_group_pids(group_id, resource_collector)
    if residual_pids is not None:
        result["residualProcessIds"] = residual_pids
        result["noResidualProcessConfirmed"] = not residual_pids
    return result


def _percentile(values: list[float], percentile: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    rank = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[rank]


def _resource_metrics(
    samples: list[dict[str, Any]], thresholds: dict[str, Any]
) -> dict[str, Any]:
    complete = [
        item for item in samples
        if item.get("processGroupRssBytes") is not None
        and item.get("processGroupCpuSeconds") is not None
        and item.get("processGroupProcessCount") is not None
    ]
    warmup = float(thresholds["rssWarmupSeconds"])
    post_warmup = [item for item in complete if float(item["elapsedSeconds"]) >= warmup]
    rss_growth: int | None = None
    if post_warmup:
        baseline = int(post_warmup[0]["processGroupRssBytes"])
        rss_growth = max(
            0,
            max(int(item["processGroupRssBytes"]) for item in post_warmup) - baseline,
        )
    cpu_percentages: list[float] = []
    cpu_monotonic = True
    for before, after in zip(complete, complete[1:]):
        wall_delta = float(after["elapsedSeconds"]) - float(before["elapsedSeconds"])
        cpu_delta = float(after["processGroupCpuSeconds"]) - float(
            before["processGroupCpuSeconds"]
        )
        if wall_delta <= 0.0 or cpu_delta < -1e-6:
            cpu_monotonic = False
            continue
        cpu_percentages.append(max(0.0, cpu_delta) / wall_delta * 100.0)
    return {
        "sampleCount": len(samples),
        "completeResourceSampleCount": len(complete),
        "resourceSampleCompleteness": len(complete) / len(samples) if samples else 0.0,
        "postWarmupSampleCount": len(post_warmup),
        "intraRunRssGrowthBytes": rss_growth,
        "cpuMonotonic": cpu_monotonic,
        "cpuIntervalCount": len(cpu_percentages),
        "cpuPercentP50": _percentile(cpu_percentages, 0.50),
        "cpuPercentP95": _percentile(cpu_percentages, 0.95),
        "processGroupCpuTotalSeconds": (
            None
            if not complete
            else max(float(item["processGroupCpuSeconds"]) for item in complete)
        ),
    }


def _load_progress_records(path: Path) -> list[dict[str, Any]]:
    try:
        lines = path.read_text(encoding="utf-8-sig").splitlines()
    except (OSError, UnicodeError) as error:
        raise ContinuousSoakError(f"cannot read progress records {path}: {error}") from error
    if not lines or any(not line.strip() for line in lines):
        raise ContinuousSoakError("runtime progress must contain nonblank NDJSON records")
    records: list[dict[str, Any]] = []
    for index, line in enumerate(lines):
        try:
            value = json.loads(
                line,
                object_pairs_hook=_strict_object,
                parse_constant=_reject_constant,
            )
        except (json.JSONDecodeError, ContinuousSoakError) as error:
            raise ContinuousSoakError(
                f"runtime progress record {index + 1} is invalid: {error}"
            ) from error
        if not isinstance(value, dict) or set(value) != PROGRESS_KEYS:
            raise ContinuousSoakError("runtime progress exact shape mismatch")
        records.append(value)
    return records


def _validate_progress_contract(
    path: Path,
    restart: int,
    round_number: int,
    iteration: int,
    runtime: dict[str, Any],
    thresholds: dict[str, Any],
) -> dict[str, Any]:
    if path.is_symlink() or not path.is_file():
        raise ContinuousSoakError("runtime progress must be a regular file")
    records = _load_progress_records(path)
    # A process may emit progress timestamps quantized to the same tick on
    # Windows. Accept equal samples while rejecting any time reversal.
    previous_elapsed = -1e-12
    previous_sessions = 0
    previous_frames = 0
    maximum_gap = 0.0
    for expected_sequence, record in enumerate(records, start=1):
        label = f"runtime progress {expected_sequence}"
        if record["schemaVersion"] != PROGRESS_SCHEMA:
            raise ContinuousSoakError(f"{label}: schema mismatch")
        if (
            _integer(record["restart"], f"{label}.restart", 1) != restart
            or _integer(record["round"], f"{label}.round", 1) != round_number
            or _integer(record["iteration"], f"{label}.iteration", 1) != iteration
            or _integer(record["sequence"], f"{label}.sequence", 1)
                != expected_sequence
        ):
            raise ContinuousSoakError(f"{label}: coordinate/sequence mismatch")
        elapsed = _number(record["elapsedSeconds"], f"{label}.elapsedSeconds")
        if elapsed < previous_elapsed:
            raise ContinuousSoakError(f"{label}: elapsed time must be monotonic")
        maximum_gap = max(maximum_gap, elapsed - previous_elapsed)
        previous_elapsed = elapsed
        sessions = _integer(record["sessionsCompleted"], f"{label}.sessions", 1)
        frames = _integer(record["framesProcessed"], f"{label}.frames", 1)
        if sessions <= previous_sessions or frames <= previous_frames:
            raise ContinuousSoakError(f"{label}: work counters must strictly increase")
        previous_sessions = sessions
        previous_frames = frames
        ok = _integer(record["ok"], f"{label}.ok")
        ng = _integer(record["ng"], f"{label}.ng")
        error = _integer(record["error"], f"{label}.error")
        product_processed = _integer(
            record["productStateProcessed"], f"{label}.productStateProcessed"
        )
        product_ok = _integer(record["productStateOk"], f"{label}.productStateOk")
        product_ng = _integer(record["productStateNg"], f"{label}.productStateNg")
        product_error = _integer(
            record["productStateError"], f"{label}.productStateError"
        )
        if frames != ok + ng + error or error != 0:
            raise ContinuousSoakError(f"{label}: decision conservation failed")
        if (
            product_processed != frames
            or (product_ok, product_ng, product_error) != (ok, ng, error)
        ):
            raise ContinuousSoakError(f"{label}: product-state conservation failed")
        if (
            record["realIoEnabled"] is not False
            or record["realRejectEnabled"] is not False
            or record["invariantsPassed"] is not True
        ):
            raise ContinuousSoakError(f"{label}: safety/invariant claim mismatch")
    runtime_duration = _number(runtime["durationSeconds"], "runtime duration")
    maximum_gap = max(maximum_gap, runtime_duration - previous_elapsed)
    if runtime_duration < previous_elapsed:
        raise ContinuousSoakError("runtime progress extends beyond final duration")
    if len(records) != _integer(runtime["progressRecords"], "runtime progressRecords", 1):
        raise ContinuousSoakError("runtime progress count mismatch")
    if len(records) < int(thresholds["minimumProgressRecordCount"]):
        raise ContinuousSoakError("runtime progress record minimum not met")
    if maximum_gap > float(thresholds["maximumProgressGapSeconds"]):
        raise ContinuousSoakError("runtime progress gap exceeded")
    final = records[-1]
    for key in (
        "sessionsCompleted", "framesProcessed", "ok", "ng", "error",
        "productStateProcessed", "productStateOk", "productStateNg",
        "productStateError",
    ):
        if final[key] != runtime[key]:
            raise ContinuousSoakError(f"runtime progress final counter mismatch: {key}")
    if int(runtime["sessionsCompleted"]) < int(thresholds["minimumSessionsCompleted"]):
        raise ContinuousSoakError("runtime session minimum not met")
    if int(runtime["framesProcessed"]) < int(thresholds["minimumFramesProcessed"]):
        raise ContinuousSoakError("runtime frame minimum not met")
    return {
        "path": path.name,
        "size": path.stat().st_size,
        "sha256": LEGACY._sha256_file(path),
        "recordCount": len(records),
        "maximumGapSeconds": maximum_gap,
        "firstElapsedSeconds": records[0]["elapsedSeconds"],
        "lastElapsedSeconds": records[-1]["elapsedSeconds"],
    }


def _validate_runtime_contract(
    path: Path,
    restart: int,
    round_number: int,
    iteration: int,
    thresholds: dict[str, Any],
) -> dict[str, Any]:
    if path.is_symlink() or not path.is_file():
        raise ContinuousSoakError("runtime contract must be a regular file")
    document = _load_json(path)
    if not isinstance(document, dict) or set(document) != RUNTIME_KEYS:
        raise ContinuousSoakError("runtime contract exact shape mismatch")
    if document["schemaVersion"] != RUNTIME_SCHEMA:
        raise ContinuousSoakError("runtime contract schema mismatch")
    for key, expected in (
        ("sdkFree", True),
        ("realIoEnabled", False),
        ("realRejectEnabled", False),
        ("productAcceptanceClaimed", False),
        ("completed", True),
        ("invariantsPassed", True),
    ):
        if document[key] is not expected:
            raise ContinuousSoakError(f"runtime contract {key} mismatch")
    if (
        _integer(document["restart"], "runtime.restart", 1) != restart
        or _integer(document["round"], "runtime.round", 1) != round_number
        or _integer(document["iteration"], "runtime.iteration", 1) != iteration
    ):
        raise ContinuousSoakError("runtime contract run coordinates mismatch")
    target_duration = _number(document["targetDurationSeconds"], "runtime target duration")
    duration = _number(document["durationSeconds"], "runtime duration")
    minimum_duration = float(thresholds["minimumDurationSeconds"])
    if target_duration < minimum_duration or duration < minimum_duration:
        raise ContinuousSoakError("runtime contract did not meet minimum duration")
    sessions = _integer(document["sessionsCompleted"], "runtime sessions", 1)
    counts = {
        key: _integer(document[key], f"runtime {key}")
        for key in (
            "framesReceived", "framesProcessed", "ok", "ng", "error", "dropped",
            "sourceErrors", "detectorErrors", "observerErrors", "saveFailures",
            "resultStores", "archiveStores", "productStateProcessed",
            "productStateOk", "productStateNg", "productStateError",
            "progressRecords",
            "maximumQueueDepth",
        )
    }
    processed = counts["framesProcessed"]
    if processed <= 0 or counts["framesReceived"] != processed:
        raise ContinuousSoakError("runtime frame receive/process conservation failed")
    if processed != counts["ok"] + counts["ng"] + counts["error"]:
        raise ContinuousSoakError("runtime decision conservation failed")
    if any(counts[key] != 0 for key in (
        "error", "dropped", "sourceErrors", "detectorErrors", "observerErrors",
        "saveFailures",
    )):
        raise ContinuousSoakError("runtime reported an error or loss")
    if any(counts[key] != processed for key in (
        "resultStores", "archiveStores", "productStateProcessed",
    )):
        raise ContinuousSoakError("runtime sink/archive/product-state conservation failed")
    if (
        counts["productStateOk"] != counts["ok"]
        or counts["productStateNg"] != counts["ng"]
        or counts["productStateError"] != counts["error"]
    ):
        raise ContinuousSoakError("runtime product-state decision conservation failed")
    progress = _validate_progress_contract(
        path.parent / "local-soak-progress.ndjson",
        restart,
        round_number,
        iteration,
        document,
        thresholds,
    )
    return {
        "path": path.name,
        "size": path.stat().st_size,
        "sha256": LEGACY._sha256_file(path),
        "durationSeconds": duration,
        "sessionsCompleted": sessions,
        "framesProcessed": processed,
        "ok": counts["ok"],
        "ng": counts["ng"],
        "productStateOk": counts["productStateOk"],
        "productStateNg": counts["productStateNg"],
        "productStateError": counts["productStateError"],
        "progress": progress,
        "maximumQueueDepth": counts["maximumQueueDepth"],
    }


def _expand_command(
    template: list[str],
    output_dir: Path,
    evidence_root: Path,
    restart: int,
    round_number: int,
    iteration: int,
    minimum_duration: float,
) -> list[str]:
    expanded = LEGACY._expand_command(
        template, output_dir, evidence_root, restart, round_number, iteration
    )
    return [
        value.replace("{minimum_duration_seconds}", str(minimum_duration))
        for value in expanded
    ]


def _confirm_windows_process_tree_v2(
    process: subprocess.Popen[Any],
    tracked_process_ids: set[int],
    grace: float,
    termination: dict[str, Any],
) -> tuple[dict[str, Any], bool]:
    """Require two fresh Windows tree enumerations before a clean verdict."""
    final_tree_ok = True
    for confirmation_index in range(2):
        descendants = LEGACY._windows_process_tree_pids(process.pid)
        if descendants is None:
            final_tree_ok = False
            break
        tracked_process_ids.update(descendants)
        if confirmation_index == 0:
            time.sleep(min(0.05, grace))
    residual_ids = LEGACY._wait_windows_pids_gone(tracked_process_ids, 0.0)
    termination["method"] = (
        termination["method"]
        if termination["attempted"]
        else "windows-final-tree-confirmation"
    )
    termination["trackedProcessIds"] = sorted(tracked_process_ids)
    termination["residualProcessIds"] = residual_ids
    termination["treeEnumerationSucceeded"] = final_tree_ok
    termination["noResidualProcessConfirmed"] = final_tree_ok and not residual_ids
    if residual_ids:
        termination = _terminate_process_group_v2(
            process, process.pid, grace, tracked_process_ids
        )
    return termination, bool(final_tree_ok and not residual_ids)


def _run_once(
    *,
    evidence_root: Path,
    command_template: list[str],
    cwd: Path,
    restart: int,
    round_number: int,
    iteration: int,
    thresholds: dict[str, Any],
    resource_collector: dict[str, Any] | None = None,
) -> dict[str, Any]:
    if resource_collector is None:
        resource_collector = _resource_collector_record()
    _assert_resource_collector_identity(resource_collector)
    run_dir = (
        evidence_root / "runs" / f"restart-{restart:03d}" / f"round-{round_number:03d}"
    )
    run_dir.mkdir(parents=True, exist_ok=False)
    output_dir = run_dir / "output"
    output_dir.mkdir()
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"
    samples_path = run_dir / "samples.json"
    result_path = run_dir / "result.json"
    command_path = run_dir / "command.json"
    minimum_duration = float(thresholds["minimumDurationSeconds"])
    argv = _expand_command(
        command_template,
        output_dir,
        evidence_root,
        restart,
        round_number,
        iteration,
        minimum_duration,
    )
    # Windows does not execute Unix-shebang Python helpers directly. Keep the
    # recorded command faithful to the process we actually launch.
    if os.name == "nt" and argv and Path(argv[0]).suffix.lower() == ".py":
        argv = [sys.executable, *argv]
    environment = os.environ.copy()
    environment.update({
        "P8_SOAK_OUTPUT_DIR": str(output_dir),
        "P8_SOAK_EVIDENCE_ROOT": str(evidence_root),
        "P8_SOAK_RESTART": str(restart),
        "P8_SOAK_ROUND": str(round_number),
        "P8_SOAK_ITERATION": str(iteration),
        "P8_SOAK_PROFILE": str(thresholds["profileName"]),
    })
    LEGACY._atomic_write_json(command_path, {
        "argv": argv,
        "cwd": str(cwd),
        "shell": False,
        "outputDirectory": str(output_dir),
    })

    started_at = LEGACY._utc_now()
    started_monotonic = time.monotonic()
    exit_code: int | None = None
    timed_out = False
    launch_error = ""
    termination_reason = ""
    termination: dict[str, Any] = {
        "attempted": False,
        "method": "not-required",
        "gracefulSignalSent": False,
        "forceSignalSent": False,
        "noResidualProcessConfirmed": os.name != "nt",
        "residualProcessIds": [],
        "trackedProcessIds": [],
        "treeEnumerationSucceeded": os.name != "nt",
    }
    samples: list[dict[str, Any]] = []
    process: subprocess.Popen[Any] | None = None
    process_exited_monotonic: float | None = None
    sample_stop_event = threading.Event()
    sample_violation_event = threading.Event()
    sample_state = {"terminationReason": "", "error": ""}
    sampler_thread: threading.Thread | None = None
    group_id: int | None = None
    tracked_process_ids: set[int] = set()
    tracked_process_lock = threading.Lock()
    timeout_seconds = float(thresholds["timeoutSeconds"])
    interval = float(thresholds["sampleIntervalSeconds"])
    grace = float(thresholds["terminationGraceSeconds"])
    maximum_output = int(thresholds["maximumOutputBytes"])
    minimum_disk = int(thresholds["minimumDiskFreeBytes"])

    with stdout_path.open("wb") as stdout_stream, stderr_path.open("wb") as stderr_stream:
        try:
            options: dict[str, Any] = {
                "cwd": cwd,
                "env": environment,
                "stdin": subprocess.DEVNULL,
                "stdout": stdout_stream,
                "stderr": stderr_stream,
                "shell": False,
            }
            if os.name == "nt":
                options["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
            else:
                options["start_new_session"] = True
            process = subprocess.Popen(argv, **options)
            group_id = process.pid
            tracked_process_ids.add(process.pid)

            def sample_process_group() -> None:
                try:
                    while not sample_stop_event.is_set():
                        if os.name == "nt":
                            descendants = LEGACY._windows_process_tree_pids(
                                process.pid
                            )
                            if descendants is not None:
                                with tracked_process_lock:
                                    tracked_process_ids.update(descendants)
                        sample = _sample(
                            process.pid,
                            group_id,
                            output_dir,
                            stdout_path,
                            stderr_path,
                            evidence_root,
                            started_monotonic,
                            resource_collector,
                        )
                        if sample_stop_event.is_set():
                            break
                        samples.append(sample)
                        if int(sample["outputBytes"]) > maximum_output:
                            sample_state["terminationReason"] = "output-budget"
                            sample_violation_event.set()
                            break
                        if (
                            sample["diskFreeBytes"] is None
                            or int(sample["diskFreeBytes"]) < minimum_disk
                        ):
                            sample_state["terminationReason"] = "disk-floor"
                            sample_violation_event.set()
                            break
                        if sample_stop_event.wait(interval):
                            break
                except Exception as error:
                    sample_state["terminationReason"] = "resource-monitor"
                    sample_state["error"] = f"{type(error).__name__}: {error}"
                    sample_violation_event.set()

            sampler_thread = threading.Thread(
                target=sample_process_group,
                name=f"p8-soak-sampler-{process.pid}",
                daemon=True,
            )
            sampler_thread.start()
            deadline = started_monotonic + timeout_seconds
            wait_slice = min(0.02, max(0.005, interval / 2.0))
            while True:
                if sample_violation_event.is_set():
                    termination_reason = str(sample_state["terminationReason"])
                    sample_stop_event.set()
                    sampler_thread.join(timeout=2.5)
                    with tracked_process_lock:
                        tracked_snapshot = set(tracked_process_ids)
                    termination = _terminate_process_group_v2(
                        process,
                        group_id,
                        grace,
                        tracked_snapshot,
                        resource_collector,
                    )
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    timed_out = True
                    termination_reason = "timeout"
                    sample_stop_event.set()
                    sampler_thread.join(timeout=2.5)
                    with tracked_process_lock:
                        tracked_snapshot = set(tracked_process_ids)
                    termination = _terminate_process_group_v2(
                        process,
                        group_id,
                        grace,
                        tracked_snapshot,
                        resource_collector,
                    )
                    break
                try:
                    exit_code = process.wait(timeout=min(wait_slice, remaining))
                    process_exited_monotonic = time.monotonic()
                    break
                except subprocess.TimeoutExpired:
                    continue
            if exit_code is None:
                try:
                    exit_code = process.wait(timeout=max(0.1, grace))
                    process_exited_monotonic = time.monotonic()
                except subprocess.TimeoutExpired:
                    termination_reason = termination_reason or "residual-process"
            # A short-lived Windows process may exit between sampler ticks. Keep
            # the Popen handle alive and take one final CPU/RSS sample through it
            # before stopping the sampler, since OpenProcess(pid) can no longer
            # open the exited PID.
            if os.name == "nt" and process is not None and exit_code is not None:
                final_sample = _sample(
                    process.pid,
                    group_id,
                    output_dir,
                    stdout_path,
                    stderr_path,
                    evidence_root,
                    started_monotonic,
                    resource_collector,
                    getattr(process, "_handle", None),
                )
                samples.append(final_sample)
            sample_stop_event.set()
            sampler_thread.join(timeout=2.5)
        except OSError as error:
            launch_error = f"{type(error).__name__}: {error}"
        finally:
            sample_stop_event.set()
            if sampler_thread is not None:
                sampler_thread.join(timeout=2.5)
            stdout_stream.flush()
            stderr_stream.flush()

    if process is not None and group_id is not None:
        if os.name == "nt":
            termination, windows_tree_clean = _confirm_windows_process_tree_v2(
                process, tracked_process_ids, grace, termination
            )
            if not windows_tree_clean:
                termination_reason = termination_reason or "residual-process"
        elif _posix_process_group_exists(group_id, resource_collector):
            cleanup = _terminate_process_group_v2(
                process,
                group_id,
                grace,
                tracked_process_ids,
                resource_collector,
            )
            termination = cleanup if not termination["attempted"] else termination
            termination_reason = termination_reason or "residual-process"

    if sampler_thread is not None and sampler_thread.is_alive():
        sample_state["error"] = sample_state["error"] or "resource sampler did not stop"
        sample_state["terminationReason"] = "resource-monitor"
    if sample_state["error"]:
        launch_error = launch_error or str(sample_state["error"])
        termination_reason = termination_reason or str(
            sample_state["terminationReason"]
        )
    _assert_tool_sources_unchanged()
    _assert_resource_collector_identity(resource_collector)
    exited_at = process_exited_monotonic
    if exited_at is None:
        exited_at = started_monotonic if process is None else time.monotonic()
    process_lifetime = round(max(0.0, exited_at - started_monotonic), 6)
    final_output_bytes = LEGACY._directory_size((output_dir, stdout_path, stderr_path))
    maximum_observed_output = max(
        [final_output_bytes] + [int(item["outputBytes"]) for item in samples],
        default=final_output_bytes,
    )
    disk_values = [
        int(item["diskFreeBytes"])
        for item in samples
        if item.get("diskFreeBytes") is not None
    ]
    resources = _resource_metrics(samples, thresholds)
    contract_error = ""
    runtime_contract: dict[str, Any] | None = None
    try:
        runtime_contract = _validate_runtime_contract(
            output_dir / "local-soak-summary.json",
            restart,
            round_number,
            iteration,
            thresholds,
        )
    except ContinuousSoakError as error:
        contract_error = str(error)
    duration = (
        0.0
        if runtime_contract is None
        else float(runtime_contract["durationSeconds"])
    )
    process_succeeded = (
        not launch_error
        and exit_code == 0
        and not timed_out
        and termination_reason == ""
        and termination["noResidualProcessConfirmed"]
        and termination["treeEnumerationSucceeded"]
    )
    contract_satisfied = (
        duration >= minimum_duration
        and resources["sampleCount"] >= int(thresholds["minimumSampleCount"])
        and resources["resourceSampleCompleteness"] == 1.0
        and resources["postWarmupSampleCount"] > 0
        and resources["intraRunRssGrowthBytes"] is not None
        and resources["intraRunRssGrowthBytes"]
            <= int(thresholds["maximumIntraRunRssGrowthBytes"])
        and resources["cpuMonotonic"]
        and resources["cpuIntervalCount"]
            >= max(1, int(thresholds["minimumSampleCount"]) - 1)
        and resources["processGroupCpuTotalSeconds"] is not None
        and resources["processGroupCpuTotalSeconds"]
            >= float(thresholds["minimumProcessGroupCpuSeconds"])
        and runtime_contract is not None
        and maximum_observed_output <= maximum_output
        and bool(disk_values)
        and min(disk_values) >= minimum_disk
    )
    crashed = (
        exit_code not in (None, 0)
        and not timed_out
        and termination_reason not in {"output-budget", "disk-floor", "residual-process"}
    )
    LEGACY._atomic_write_json(samples_path, {
        "schemaVersion": SAMPLES_SCHEMA,
        "restart": restart,
        "round": round_number,
        "iteration": iteration,
        "samples": samples,
    })
    result: dict[str, Any] = {
        "restart": restart,
        "round": round_number,
        "iteration": iteration,
        "argv": argv,
        "cwd": str(cwd),
        "startedAt": started_at,
        "endedAt": LEGACY._utc_now(),
        "durationSeconds": duration,
        "processLifetimeSeconds": process_lifetime,
        "exitCode": exit_code,
        "timedOut": timed_out,
        "crashed": crashed,
        "processSucceeded": process_succeeded,
        "continuousContractSatisfied": contract_satisfied,
        "succeeded": process_succeeded and contract_satisfied,
        "launchError": launch_error,
        "terminationReason": termination_reason,
        "processGroupTermination": termination,
        "outputDirectory": LEGACY._relative(output_dir, evidence_root),
        "stdout": LEGACY._file_reference(stdout_path, evidence_root),
        "stderr": LEGACY._file_reference(stderr_path, evidence_root),
        "samples": LEGACY._relative(samples_path, evidence_root),
        "finalOutputBytes": final_output_bytes,
        "maximumObservedOutputBytes": maximum_observed_output,
        "minimumObservedDiskFreeBytes": min(disk_values) if disk_values else None,
        **resources,
        "runtimeContract": runtime_contract,
        "runtimeContractError": contract_error,
        "commandRecord": LEGACY._relative(command_path, evidence_root),
        "resultRecord": LEGACY._relative(result_path, evidence_root),
    }
    LEGACY._atomic_write_json(result_path, result)
    return result


def _gate(name: str, passed: bool, observed: Any, requirement: str) -> dict[str, Any]:
    return {
        "name": name,
        "status": "passed" if passed else "failed",
        "observed": observed,
        "requirement": requirement,
    }


def _evaluate(
    runs: list[dict[str, Any]], thresholds: dict[str, Any]
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    planned = int(thresholds["roundsPerRestart"]) * int(thresholds["restarts"])
    successful = sum(bool(item["succeeded"]) for item in runs)
    durations = [float(item["durationSeconds"]) for item in runs]
    sample_counts = [int(item["sampleCount"]) for item in runs]
    rss_growth = [
        item["intraRunRssGrowthBytes"]
        for item in runs
        if item["intraRunRssGrowthBytes"] is not None
    ]
    cpu_p50 = [item["cpuPercentP50"] for item in runs if item["cpuPercentP50"] is not None]
    cpu_p95 = [item["cpuPercentP95"] for item in runs if item["cpuPercentP95"] is not None]
    cpu_totals = [
        item["processGroupCpuTotalSeconds"]
        for item in runs if item["processGroupCpuTotalSeconds"] is not None
    ]
    disk_values = [
        item["minimumObservedDiskFreeBytes"]
        for item in runs
        if item["minimumObservedDiskFreeBytes"] is not None
    ]
    summary = {
        "plannedRuns": planned,
        "completedRuns": len(runs),
        "successfulRuns": successful,
        "successRate": successful / planned if planned else 0.0,
        "minimumDurationSeconds": min(durations) if durations else None,
        "minimumSampleCount": min(sample_counts) if sample_counts else 0,
        "maximumIntraRunRssGrowthBytes": max(rss_growth) if rss_growth else None,
        "cpuPercentP50Maximum": max(cpu_p50) if cpu_p50 else None,
        "cpuPercentP95Maximum": max(cpu_p95) if cpu_p95 else None,
        "minimumProcessGroupCpuSeconds": min(cpu_totals) if cpu_totals else None,
        "minimumObservedDiskFreeBytes": min(disk_values) if disk_values else None,
        "totalFramesProcessed": sum(
            int(item["runtimeContract"]["framesProcessed"])
            for item in runs if item["runtimeContract"] is not None
        ),
    }
    checks = [
        _gate("all-runs-completed", len(runs) == planned, len(runs), f"exactly {planned}"),
        _gate("process-exit-success", all(item["processSucceeded"] for item in runs),
              sum(not item["processSucceeded"] for item in runs), "0 failed processes"),
        _gate("minimum-duration", bool(durations) and min(durations) >= float(
            thresholds["minimumDurationSeconds"]), min(durations) if durations else None,
            f">= {thresholds['minimumDurationSeconds']} seconds per run"),
        _gate("minimum-samples", bool(sample_counts) and min(sample_counts) >= int(
            thresholds["minimumSampleCount"]), min(sample_counts) if sample_counts else 0,
            f">= {thresholds['minimumSampleCount']} samples per run"),
        _gate("resource-sample-completeness", all(
            item["resourceSampleCompleteness"] == 1.0 for item in runs),
            [item["resourceSampleCompleteness"] for item in runs], "1.0 for every run"),
        _gate("intra-run-rss-growth", len(rss_growth) == len(runs) and max(
            rss_growth, default=0) <= int(thresholds["maximumIntraRunRssGrowthBytes"]),
            max(rss_growth) if rss_growth else None,
            f"<= {thresholds['maximumIntraRunRssGrowthBytes']} bytes after warm-up"),
        _gate("cpu-collected-monotonic", all(item["cpuMonotonic"] and
            item["cpuIntervalCount"] >= max(1, int(thresholds["minimumSampleCount"]) - 1)
            for item in runs), [item["cpuIntervalCount"] for item in runs],
            "monotonic cumulative CPU with an interval for each required sample"),
        _gate("minimum-process-group-cpu", len(cpu_totals) == len(runs) and min(
            cpu_totals, default=0.0) >= float(
                thresholds["minimumProcessGroupCpuSeconds"]
            ), min(cpu_totals) if cpu_totals else None,
            f">= {thresholds['minimumProcessGroupCpuSeconds']} CPU seconds per run"),
        _gate("runtime-contracts", all(item["runtimeContract"] is not None for item in runs),
              [item["runtimeContractError"] for item in runs], "all CigVision contracts valid"),
        _gate("timeouts", sum(bool(item["timedOut"]) for item in runs) == 0,
              sum(bool(item["timedOut"]) for item in runs), "0"),
        _gate("crashes", sum(bool(item["crashed"]) for item in runs) == 0,
              sum(bool(item["crashed"]) for item in runs), "0"),
        _gate("disk-free-floor", bool(disk_values) and min(disk_values) >= int(
            thresholds["minimumDiskFreeBytes"]), min(disk_values) if disk_values else None,
            f">= {thresholds['minimumDiskFreeBytes']} bytes"),
        _gate("output-budget", all(item["maximumObservedOutputBytes"] <= int(
            thresholds["maximumOutputBytes"]) for item in runs),
            sum(int(item["finalOutputBytes"]) for item in runs),
            f"each run <= {thresholds['maximumOutputBytes']} bytes"),
        _gate("process-groups-clean", all(
            item["processGroupTermination"]["noResidualProcessConfirmed"]
            and item["processGroupTermination"]["treeEnumerationSucceeded"]
            for item in runs),
            sum(not (
                item["processGroupTermination"]["noResidualProcessConfirmed"]
                and item["processGroupTermination"]["treeEnumerationSucceeded"]
            ) for item in runs), "0 unconfirmed residual process groups"),
    ]
    return summary, checks


def _copy_provenance(evidence_root: Path, profile_name: str) -> dict[str, Any]:
    _assert_tool_sources_unchanged()
    provenance = evidence_root / "provenance"
    provenance.mkdir()
    profile_copy = provenance / "profile.json"
    LEGACY._atomic_write_json(profile_copy, {
        "schemaVersion": PROFILE_SNAPSHOT_SCHEMA,
        "name": profile_name,
        "thresholds": LOCKED_PROFILES[profile_name],
        "sourcePath": "config/p8-continuous-soak-profiles-v1.json",
        "sourceSha256": LEGACY._sha256_file(PROFILE_PATH),
        "toolSha256": TOOL_SOURCE_SHA256,
        "legacyToolSha256": LEGACY_SOURCE_SHA256,
    })
    return LEGACY._file_reference(profile_copy, evidence_root)


def _evidence_files_v2(root: Path) -> tuple[list[dict[str, Any]], list[str]]:
    records: list[dict[str, Any]] = []
    unsafe: list[str] = []
    manifest_path = root / "soak-manifest.json"
    for directory, directory_names, file_names in os.walk(root, followlinks=False):
        base = Path(directory)
        retained_directories: list[str] = []
        for name in directory_names:
            path = base / name
            try:
                info = path.lstat()
            except FileNotFoundError:
                unsafe.append(f"evidence directory disappeared: {LEGACY._relative(path, root)}")
                continue
            if name.endswith(".tmp"):
                unsafe.append(f"temporary directory forbidden: {LEGACY._relative(path, root)}")
            elif stat.S_ISLNK(info.st_mode):
                unsafe.append(f"symlink directory excluded: {LEGACY._relative(path, root)}")
            elif not stat.S_ISDIR(info.st_mode):
                unsafe.append(f"non-directory entry excluded: {LEGACY._relative(path, root)}")
            else:
                retained_directories.append(name)
        directory_names[:] = retained_directories
        for name in file_names:
            path = base / name
            if path == manifest_path:
                continue
            try:
                info = path.lstat()
            except FileNotFoundError:
                unsafe.append(f"evidence file disappeared: {LEGACY._relative(path, root)}")
                continue
            relative = LEGACY._relative(path, root)
            if name.endswith(".tmp"):
                unsafe.append(f"temporary evidence file forbidden: {relative}")
                continue
            if stat.S_ISLNK(info.st_mode):
                unsafe.append(f"symlink file excluded: {relative}")
                continue
            if not stat.S_ISREG(info.st_mode):
                unsafe.append(f"non-regular evidence excluded: {relative}")
                continue
            records.append({
                "path": relative,
                "size": info.st_size,
                "sha256": LEGACY._sha256_file(path),
            })
    records.sort(key=lambda item: item["path"])
    return records, unsafe


def _verify_hash_inventory(root: Path, document: dict[str, Any]) -> None:
    actual, unsafe = _evidence_files_v2(root)
    if unsafe:
        raise ContinuousSoakError(f"unsafe evidence files: {unsafe}")
    recorded = document.get("evidenceFiles")
    if not isinstance(recorded, list) or recorded != actual:
        raise ContinuousSoakError("evidence file inventory/hash mismatch")


def _validate_samples_document(
    path: Path,
    restart: int,
    round_number: int,
    iteration: int,
    expected_resource_source: str,
) -> list[dict[str, Any]]:
    document = _load_json(path)
    if not isinstance(document, dict) or set(document) != {
        "schemaVersion", "restart", "round", "iteration", "samples",
    }:
        raise ContinuousSoakError("continuous soak samples exact shape mismatch")
    if document["schemaVersion"] != SAMPLES_SCHEMA:
        raise ContinuousSoakError("continuous soak samples schema mismatch")
    if (
        _integer(document["restart"], "samples.restart", 1) != restart
        or _integer(document["round"], "samples.round", 1) != round_number
        or _integer(document["iteration"], "samples.iteration", 1) != iteration
    ):
        raise ContinuousSoakError("continuous soak sample coordinates mismatch")
    samples = document["samples"]
    if not isinstance(samples, list):
        raise ContinuousSoakError("continuous soak samples must be an array")
    previous_elapsed = -1.0
    for index, sample in enumerate(samples):
        label = f"samples[{index}]"
        if not isinstance(sample, dict) or set(sample) != SAMPLE_KEYS:
            raise ContinuousSoakError(f"{label}: exact shape mismatch")
        _timestamp(sample["sampledAt"], f"{label}.sampledAt")
        elapsed = _number(sample["elapsedSeconds"], f"{label}.elapsedSeconds")
        if elapsed < 0.0 or elapsed < previous_elapsed:
            raise ContinuousSoakError(f"{label}: elapsed time must be monotonic")
        previous_elapsed = elapsed
        resource_values = (
            sample["processGroupRssBytes"],
            sample["processGroupCpuSeconds"],
            sample["processGroupProcessCount"],
        )
        if all(value is None for value in resource_values):
            if sample["resourceSource"] != "not-collected":
                raise ContinuousSoakError(f"{label}: missing resource source mismatch")
        elif any(value is None for value in resource_values):
            raise ContinuousSoakError(f"{label}: partial resource sample")
        else:
            _integer(resource_values[0], f"{label}.processGroupRssBytes")
            cpu = _number(resource_values[1], f"{label}.processGroupCpuSeconds")
            if cpu < 0.0:
                raise ContinuousSoakError(f"{label}: CPU seconds must be nonnegative")
            _integer(resource_values[2], f"{label}.processGroupProcessCount", 1)
            if sample["resourceSource"] != expected_resource_source:
                raise ContinuousSoakError(f"{label}: resource collector provenance mismatch")
        _integer(sample["outputBytes"], f"{label}.outputBytes")
        if sample["diskFreeBytes"] is not None:
            _integer(sample["diskFreeBytes"], f"{label}.diskFreeBytes")
        if sample["gpu"] != "not-collected":
            raise ContinuousSoakError(f"{label}: GPU collection claim mismatch")
    return samples


def _validate_termination(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != TERMINATION_KEYS:
        raise ContinuousSoakError("process-group termination exact shape mismatch")
    for key in (
        "attempted", "gracefulSignalSent", "forceSignalSent",
        "noResidualProcessConfirmed", "treeEnumerationSucceeded",
    ):
        _boolean(value[key], f"termination.{key}")
    _string(value["method"], "termination.method")
    for key in ("residualProcessIds", "trackedProcessIds"):
        identifiers = value[key]
        if not isinstance(identifiers, list):
            raise ContinuousSoakError(f"termination.{key}: array required")
        normalized = [_integer(item, f"termination.{key}", 1) for item in identifiers]
        if normalized != sorted(set(normalized)):
            raise ContinuousSoakError(f"termination.{key}: sorted unique IDs required")
    return value


def _validate_run_record(
    root: Path,
    run: Any,
    thresholds: dict[str, Any],
    expected_restart: int,
    expected_round: int,
    expected_iteration: int,
    expected_resource_source: str,
) -> dict[str, Any]:
    if not isinstance(run, dict) or set(run) != RUN_RESULT_KEYS:
        raise ContinuousSoakError("continuous soak run exact shape mismatch")
    restart = _integer(run["restart"], "run.restart", 1)
    round_number = _integer(run["round"], "run.round", 1)
    iteration = _integer(run["iteration"], "run.iteration", 1)
    if (restart, round_number, iteration) != (
        expected_restart, expected_round, expected_iteration,
    ):
        raise ContinuousSoakError("continuous soak run ordering/coordinates mismatch")

    result_path = _regular_evidence_file(root, run["resultRecord"], "run.resultRecord")
    result_document = _load_json(result_path)
    if result_document != run:
        raise ContinuousSoakError("continuous soak result record mismatch")

    command_path = _regular_evidence_file(root, run["commandRecord"], "run.commandRecord")
    command_document = _load_json(command_path)
    if not isinstance(command_document, dict) or set(command_document) != {
        "argv", "cwd", "shell", "outputDirectory",
    }:
        raise ContinuousSoakError("continuous soak command record shape mismatch")
    argv = run["argv"]
    if not isinstance(argv, list) or not argv or not all(
        isinstance(item, str) and item for item in argv
    ):
        raise ContinuousSoakError("continuous soak argv must contain nonempty strings")
    cwd = _string(run["cwd"], "run.cwd")
    expected_output_relative = (
        f"runs/restart-{restart:03d}/round-{round_number:03d}/output"
    )
    if run["outputDirectory"] != expected_output_relative:
        raise ContinuousSoakError("continuous soak output directory mismatch")
    output_dir = _evidence_directory(root, run["outputDirectory"], "run.outputDirectory")
    expected_run_root = output_dir.parent
    if result_path != expected_run_root / "result.json":
        raise ContinuousSoakError("continuous soak result path mismatch")
    if command_path != expected_run_root / "command.json":
        raise ContinuousSoakError("continuous soak command path mismatch")
    if command_document != {
        "argv": argv,
        "cwd": cwd,
        "shell": False,
        "outputDirectory": str(output_dir),
    }:
        raise ContinuousSoakError("continuous soak command/run binding mismatch")

    stdout_path = _validate_file_reference(root, run["stdout"], "run.stdout")
    stderr_path = _validate_file_reference(root, run["stderr"], "run.stderr")
    if stdout_path != expected_run_root / "stdout.log" or stderr_path != expected_run_root / "stderr.log":
        raise ContinuousSoakError("continuous soak log path mismatch")
    samples_path = _regular_evidence_file(root, run["samples"], "run.samples")
    if samples_path != expected_run_root / "samples.json":
        raise ContinuousSoakError("continuous soak samples path mismatch")
    samples = _validate_samples_document(
        samples_path, restart, round_number, iteration, expected_resource_source,
    )
    metrics = _resource_metrics(samples, thresholds)
    for key, value in metrics.items():
        if run[key] != value:
            raise ContinuousSoakError(f"continuous soak derived metric mismatch: {key}")

    started_at = _timestamp(run["startedAt"], "run.startedAt")
    ended_at = _timestamp(run["endedAt"], "run.endedAt")
    if ended_at < started_at:
        raise ContinuousSoakError("continuous soak run timestamps are reversed")
    duration = _number(run["durationSeconds"], "run.durationSeconds")
    if duration < 0.0:
        raise ContinuousSoakError("continuous soak duration must be nonnegative")
    process_lifetime = _number(
        run["processLifetimeSeconds"], "run.processLifetimeSeconds"
    )
    if process_lifetime < 0.0 or duration > process_lifetime + 0.05:
        raise ContinuousSoakError(
            "verified runtime duration exceeds observed process lifetime"
        )
    exit_code = run["exitCode"]
    if exit_code is not None and (isinstance(exit_code, bool) or not isinstance(exit_code, int)):
        raise ContinuousSoakError("continuous soak exit code must be an integer or null")
    timed_out = _boolean(run["timedOut"], "run.timedOut")
    launch_error = _string(run["launchError"], "run.launchError")
    termination_reason = _string(run["terminationReason"], "run.terminationReason")
    termination = _validate_termination(run["processGroupTermination"])

    final_output = LEGACY._directory_size((output_dir, stdout_path, stderr_path))
    if run["finalOutputBytes"] != final_output:
        raise ContinuousSoakError("continuous soak final output size mismatch")
    maximum_output = max(
        [final_output] + [int(item["outputBytes"]) for item in samples],
        default=final_output,
    )
    if run["maximumObservedOutputBytes"] != maximum_output:
        raise ContinuousSoakError("continuous soak maximum output size mismatch")
    disk_values = [
        int(item["diskFreeBytes"])
        for item in samples if item["diskFreeBytes"] is not None
    ]
    minimum_disk = min(disk_values) if disk_values else None
    if run["minimumObservedDiskFreeBytes"] != minimum_disk:
        raise ContinuousSoakError("continuous soak minimum disk value mismatch")

    contract = _validate_runtime_contract(
        output_dir / "local-soak-summary.json",
        restart,
        round_number,
        iteration,
        thresholds,
    )
    if run["runtimeContract"] != contract or run["runtimeContractError"] != "":
        raise ContinuousSoakError("continuous soak runtime contract summary mismatch")
    if duration != float(contract["durationSeconds"]):
        raise ContinuousSoakError(
            "continuous soak verified duration/runtime contract mismatch"
        )

    process_succeeded = (
        not launch_error
        and exit_code == 0
        and not timed_out
        and termination_reason == ""
        and termination["noResidualProcessConfirmed"]
        and termination["treeEnumerationSucceeded"]
    )
    contract_satisfied = (
        duration >= float(thresholds["minimumDurationSeconds"])
        and metrics["sampleCount"] >= int(thresholds["minimumSampleCount"])
        and metrics["resourceSampleCompleteness"] == 1.0
        and metrics["postWarmupSampleCount"] > 0
        and metrics["intraRunRssGrowthBytes"] is not None
        and metrics["intraRunRssGrowthBytes"]
            <= int(thresholds["maximumIntraRunRssGrowthBytes"])
        and metrics["cpuMonotonic"]
        and metrics["cpuIntervalCount"]
            >= max(1, int(thresholds["minimumSampleCount"]) - 1)
        and metrics["processGroupCpuTotalSeconds"] is not None
        and metrics["processGroupCpuTotalSeconds"]
            >= float(thresholds["minimumProcessGroupCpuSeconds"])
        and maximum_output <= int(thresholds["maximumOutputBytes"])
        and bool(disk_values)
        and min(disk_values) >= int(thresholds["minimumDiskFreeBytes"])
    )
    crashed = (
        exit_code not in (None, 0)
        and not timed_out
        and termination_reason not in {"output-budget", "disk-floor", "residual-process"}
    )
    expected_flags = {
        "processSucceeded": process_succeeded,
        "continuousContractSatisfied": contract_satisfied,
        "succeeded": process_succeeded and contract_satisfied,
        "crashed": crashed,
    }
    for key, expected in expected_flags.items():
        if _boolean(run[key], f"run.{key}") is not expected:
            raise ContinuousSoakError(f"continuous soak derived outcome mismatch: {key}")
    return run


def verify_evidence(root: Path) -> None:
    root = LEGACY._absolute(root)
    try:
        root_info = root.lstat()
    except OSError as error:
        raise ContinuousSoakError("continuous soak evidence root unavailable") from error
    if not stat.S_ISDIR(root_info.st_mode):
        raise ContinuousSoakError("continuous soak evidence root must be a regular directory")
    document = _load_json(root / "soak-manifest.json")
    if not isinstance(document, dict) or set(document) != MANIFEST_KEYS:
        raise ContinuousSoakError("continuous soak manifest exact shape mismatch")
    if document["schemaVersion"] != MANIFEST_SCHEMA:
        raise ContinuousSoakError("continuous soak manifest schema mismatch")
    if (
        document["sdkFree"] is not True
        or document["productAcceptanceClaimed"] is not False
        or document["windowsRuntimeVerified"] is not False
        or document["gpu"] != "not-collected"
        or document["gpuClaimed"] is not False
        or document["manifestExcludedFromSelfHash"] is not True
    ):
        raise ContinuousSoakError("continuous soak manifest claim boundary mismatch")
    manifest_started = _timestamp(document["startedAt"], "manifest.startedAt")
    manifest_ended = _timestamp(document["endedAt"], "manifest.endedAt")
    if manifest_ended < manifest_started:
        raise ContinuousSoakError("continuous soak manifest timestamps are reversed")
    profile = document["profile"]
    if not isinstance(profile, dict) or set(profile) != {
        "name", "durationClass", "claimScope", "thresholds", "snapshot",
    }:
        raise ContinuousSoakError("continuous soak profile record exact shape mismatch")
    profile_name = _string(profile["name"], "profile.name")
    profiles = _load_profiles()
    if (
        profile_name not in profiles
        or profile["thresholds"] != profiles[profile_name]
        or profile["durationClass"] != profiles[profile_name]["durationClass"]
        or profile["claimScope"] != profiles[profile_name]["claimScope"]
    ):
        raise ContinuousSoakError("continuous soak manifest profile mismatch")
    _verify_hash_inventory(root, document)
    resource_collector = _verify_tool_dependencies_snapshot(
        root, document["toolDependencies"]
    )
    snapshot_path = _validate_file_reference(root, profile["snapshot"], "profile.snapshot")
    snapshot = _load_json(snapshot_path)
    if snapshot != {
        "schemaVersion": PROFILE_SNAPSHOT_SCHEMA,
        "name": profile_name,
        "thresholds": profiles[profile_name],
        "sourcePath": "config/p8-continuous-soak-profiles-v1.json",
        "sourceSha256": LEGACY._sha256_file(PROFILE_PATH),
        "toolSha256": TOOL_SOURCE_SHA256,
        "legacyToolSha256": LEGACY_SOURCE_SHA256,
    }:
        raise ContinuousSoakError("continuous soak profile snapshot mismatch")
    thresholds = dict(profiles[profile_name])
    thresholds["profileName"] = profile_name
    command_template = document["commandTemplate"]
    if not isinstance(command_template, list) or not command_template or not all(
        isinstance(item, str) and item for item in command_template
    ):
        raise ContinuousSoakError("continuous soak command template mismatch")
    _verify_build_provenance_snapshot(
        root,
        document["buildProvenance"],
        command_template,
        profile_name,
    )
    _string(document["cwd"], "manifest.cwd")
    runs = document["runs"]
    if not isinstance(runs, list):
        raise ContinuousSoakError("continuous soak runs must be an array")
    expected_coordinates: list[tuple[int, int, int]] = []
    expected_iteration = 0
    for expected_restart in range(1, int(thresholds["restarts"]) + 1):
        for expected_round in range(1, int(thresholds["roundsPerRestart"]) + 1):
            expected_iteration += 1
            expected_coordinates.append(
                (expected_restart, expected_round, expected_iteration)
            )
    if len(runs) != len(expected_coordinates):
        raise ContinuousSoakError("continuous soak does not contain every planned run")
    validated_runs = [
        _validate_run_record(
            root,
            run,
            thresholds,
            coordinates[0],
            coordinates[1],
            coordinates[2],
            resource_collector["resourceSource"],
        )
        for run, coordinates in zip(runs, expected_coordinates)
    ]
    if any(run["argv"] != (
        ([sys.executable] if os.name == "nt" and command_template[0].lower().endswith(".py") else [])
        + [
        value.replace("{output_dir}", str(root / run["outputDirectory"]))
            .replace("{evidence_root}", str(root))
            .replace("{restart}", str(run["restart"]))
            .replace("{round}", str(run["round"]))
            .replace("{iteration}", str(run["iteration"]))
            .replace(
                "{minimum_duration_seconds}",
                str(float(thresholds["minimumDurationSeconds"])),
            )
        for value in command_template
        ]
    ) for run in validated_runs):
        raise ContinuousSoakError("continuous soak command template expansion mismatch")
    if any(run["cwd"] != document["cwd"] for run in validated_runs):
        raise ContinuousSoakError("continuous soak cwd binding mismatch")
    if validated_runs:
        first_started = min(_timestamp(run["startedAt"], "run.startedAt")
                            for run in validated_runs)
        last_ended = max(_timestamp(run["endedAt"], "run.endedAt")
                         for run in validated_runs)
        if manifest_started > first_started or manifest_ended < last_ended:
            raise ContinuousSoakError("continuous soak manifest does not envelope run times")
    summary, checks = _evaluate(runs, thresholds)
    checks.append(_gate("regular-evidence-files", True, [],
                        "no symlink or non-regular evidence files"))
    if document.get("summary") != summary or document.get("checks") != checks:
        raise ContinuousSoakError("continuous soak summary/check recomputation mismatch")
    if (
        not checks
        or any(check["status"] != "passed" for check in checks)
        or any(run["succeeded"] is not True for run in validated_runs)
        or summary["successfulRuns"] != summary["plannedRuns"]
    ):
        raise ContinuousSoakError("continuous soak manifest contains a failed gate or run")
    expected_result = (
        "passed-test-contract" if profile_name == "contract-test-v1"
        else "passed-local-continuous-tooling"
    )
    if document["overallResult"] != expected_result or document["failures"] != []:
        raise ContinuousSoakError("continuous soak manifest is not a valid pass")


def _run(arguments: argparse.Namespace) -> int:
    profiles = _load_profiles()
    profile_name = arguments.profile
    thresholds = dict(profiles[profile_name])
    thresholds["profileName"] = profile_name
    command = list(arguments.command)
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        raise ContinuousSoakError("runtime arguments or a helper command are required after --")
    _assert_tool_sources_unchanged()
    resource_collector = _resource_collector_record()
    _assert_resource_collector_identity(resource_collector)
    evidence_root = LEGACY._new_evidence_root(arguments.evidence_root)
    cwd = LEGACY._absolute(arguments.cwd)
    if not cwd.is_dir():
        raise ContinuousSoakError(f"working directory does not exist: {cwd}")

    has_external_provenance = arguments.build_provenance is not None
    has_project_build_option = (
        arguments.compiler is not None or arguments.standard is not None
    )
    if has_external_provenance and has_project_build_option:
        raise ContinuousSoakError(
            "external helper provenance and controlled project build are mutually exclusive"
        )

    build_temporary: tempfile.TemporaryDirectory[str] | None = None
    if has_external_provenance:
        build_document, runtime_executable, runtime_sources = (
            _validate_build_provenance(
                LEGACY._absolute(arguments.build_provenance),
                command,
                profile_name,
            )
        )
        if build_document["runtimeKind"] != TEST_RUNTIME_KIND:
            raise ContinuousSoakError(
                "external build provenance is allowed only for contract test helpers; "
                "project runners must be built by run"
            )
        command[0] = str(runtime_executable)
    else:
        if arguments.compiler is None or arguments.standard is None:
            raise ContinuousSoakError(
                "controlled project runs require both --compiler and --standard"
            )
        if profile_name == "local-sdkfree-v1" and arguments.standard != "c++17":
            raise ContinuousSoakError("local continuous profile requires C++17")
        build_temporary = tempfile.TemporaryDirectory(
            prefix="p8-controlled-project-build."
        )
        try:
            build_document, runtime_executable, runtime_sources = (
                _build_project_runtime(
                    arguments.compiler,
                    arguments.standard,
                    Path(build_temporary.name) / "cigvision-local-soak",
                )
            )
        except Exception:
            build_temporary.cleanup()
            raise
        command.insert(0, str(runtime_executable))

    evidence_root.mkdir()
    manifest_started_at = LEGACY._utc_now()
    profile_reference = _copy_provenance(evidence_root, profile_name)
    try:
        build_reference = _snapshot_build_provenance(
            evidence_root,
            build_document,
            runtime_executable,
            runtime_sources,
        )
        tool_dependencies_reference = _snapshot_tool_dependencies(
            evidence_root, resource_collector
        )
    finally:
        if build_temporary is not None:
            build_temporary.cleanup()
    command[0] = (
        "{evidence_root}/" + build_reference["executableSnapshot"]["path"]
    )
    runs: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    iteration = 0
    try:
        for restart in range(1, int(thresholds["restarts"]) + 1):
            for round_number in range(1, int(thresholds["roundsPerRestart"]) + 1):
                iteration += 1
                runs.append(_run_once(
                    evidence_root=evidence_root,
                    command_template=command,
                    cwd=cwd,
                    restart=restart,
                    round_number=round_number,
                    iteration=iteration,
                    thresholds=thresholds,
                    resource_collector=resource_collector,
                ))
    except Exception as error:
        failures.append({
            "kind": "orchestration-error",
            "detail": f"{type(error).__name__}: {error}",
        })
    summary, checks = _evaluate(runs, thresholds)
    _assert_tool_sources_unchanged()
    _assert_resource_collector_identity(resource_collector)
    evidence_files, unsafe = _evidence_files_v2(evidence_root)
    checks.append(_gate("regular-evidence-files", not unsafe, unsafe,
                        "no symlink or non-regular evidence files"))
    for check in checks:
        if check["status"] == "failed":
            failures.append({
                "kind": "gate-failure",
                "gate": check["name"],
                "observed": check["observed"],
                "requirement": check["requirement"],
            })
    passed = not failures
    overall_result = "failed"
    if passed:
        overall_result = (
            "passed-test-contract" if profile_name == "contract-test-v1"
            else "passed-local-continuous-tooling"
        )
    manifest = {
        "schemaVersion": MANIFEST_SCHEMA,
        "profile": {
            "name": profile_name,
            "durationClass": thresholds["durationClass"],
            "claimScope": thresholds["claimScope"],
            "thresholds": profiles[profile_name],
            "snapshot": profile_reference,
        },
        "buildProvenance": build_reference,
        "toolDependencies": tool_dependencies_reference,
        "startedAt": manifest_started_at,
        "endedAt": LEGACY._utc_now(),
        "overallResult": overall_result,
        "sdkFree": True,
        "productAcceptanceClaimed": False,
        "windowsRuntimeVerified": False,
        "gpu": "not-collected",
        "gpuClaimed": False,
        "commandTemplate": command,
        "cwd": str(cwd),
        "summary": summary,
        "checks": checks,
        "runs": runs,
        "failures": failures,
        "evidenceFiles": evidence_files,
        "manifestExcludedFromSelfHash": True,
    }
    LEGACY._atomic_write_json(evidence_root / "soak-manifest.json", manifest)
    if not passed:
        print(f"P8 continuous soak failed: {evidence_root}", file=sys.stderr)
        return 1
    verify_evidence(evidence_root)
    print(f"P8 continuous soak passed and self-verified: {evidence_root}")
    return 0


def _build_project_runtime(
    compiler_value: Path | str,
    standard: str,
    executable_value: Path,
) -> tuple[dict[str, Any], Path, list[Path]]:
    if standard not in {"c++17", "c++14"}:
        raise ContinuousSoakError("unsupported project runner C++ standard")
    compiler = _resolved_regular_file(os.fspath(compiler_value), "compiler")
    requested_output = LEGACY._new_evidence_root(executable_value)
    if _is_msvc_compiler(str(compiler)) and requested_output.suffix.casefold() != ".exe":
        requested_output = Path(str(requested_output) + ".exe")
    try:
        executable_output = requested_output.parent.resolve(strict=True) / requested_output.name
    except OSError as error:
        raise ContinuousSoakError("controlled runtime output parent unavailable") from error
    try:
        version_arguments = [] if _is_msvc_compiler(str(compiler)) else ["--version"]
        completed = subprocess.run(
            [str(compiler), *version_arguments],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10.0,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ContinuousSoakError(f"cannot query compiler version: {error}") from error
    version_lines = completed.stdout.splitlines()
    if completed.returncode != 0 or not version_lines or not version_lines[0].strip():
        raise ContinuousSoakError("compiler version query failed")
    source_files: list[dict[str, Any]] = []
    source_paths: list[Path] = []
    for relative in PROJECT_RUNTIME_SOURCES:
        source = ROOT / relative
        if source.is_symlink() or not source.is_file():
            raise ContinuousSoakError(f"project runtime source unavailable: {relative}")
        source_paths.append(source)
        source_files.append({
            "path": relative,
            "size": source.stat().st_size,
            "sha256": LEGACY._sha256_file(source),
        })
    compile_flags, compile_argv = _project_compile_spec(
        str(compiler), standard, str(executable_output)
    )
    try:
        build = subprocess.run(
            compile_argv,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=120.0,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ContinuousSoakError(f"controlled project runner build failed: {error}") from error
    if build.returncode != 0:
        raise ContinuousSoakError(
            "controlled project runner build failed: " + build.stdout[-4000:]
        )
    executable = _resolved_regular_file(
        str(executable_output), "controlled runtime executable"
    )
    if requested_output.is_symlink() or executable != requested_output.resolve(strict=True):
        raise ContinuousSoakError("controlled build did not create the requested regular file")
    for record in source_files:
        source = ROOT / record["path"]
        if (
            source.stat().st_size != record["size"]
            or LEGACY._sha256_file(source) != record["sha256"]
        ):
            raise ContinuousSoakError("project runtime source changed during compilation")
    document = {
        "schemaVersion": BUILD_PROVENANCE_SCHEMA,
        "runtimeKind": PROJECT_RUNTIME_KIND,
        "compiler": {
            "path": str(compiler),
            "size": compiler.stat().st_size,
            "sha256": LEGACY._sha256_file(compiler),
            "version": version_lines[0].strip(),
        },
        "compile": {
            "standard": standard,
            "flags": list(compile_flags),
            "includePath": PROJECT_INCLUDE_PATH,
            "argv": compile_argv,
        },
        "sourceFiles": source_files,
        "executable": {
            "path": str(executable),
            "size": executable.stat().st_size,
            "sha256": LEGACY._sha256_file(executable),
        },
    }
    return document, executable, source_paths


def _create_project_build_provenance(arguments: argparse.Namespace) -> int:
    output = LEGACY._new_evidence_root(arguments.output)
    document, _, _ = _build_project_runtime(
        arguments.compiler,
        arguments.standard,
        arguments.executable,
    )
    LEGACY._atomic_write_json(output, document)
    print(f"P8 continuous soak build provenance written: {output}")
    return 0


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="P8 duration-enforced SDK-free soak")
    subparsers = parser.add_subparsers(dest="mode", required=True)
    run = subparsers.add_parser("run")
    run.add_argument("--evidence-root", required=True, type=Path)
    run.add_argument("--profile", choices=tuple(LOCKED_PROFILES), required=True)
    run.add_argument("--cwd", type=Path, default=Path.cwd())
    run.add_argument("--build-provenance", type=Path)
    run.add_argument("--compiler", type=Path)
    run.add_argument("--standard", choices=("c++17", "c++14"))
    run.add_argument("command", nargs=argparse.REMAINDER)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--evidence-root", required=True, type=Path)
    provenance = subparsers.add_parser("build-provenance")
    provenance.add_argument("--output", required=True, type=Path)
    provenance.add_argument("--compiler", required=True, type=Path)
    provenance.add_argument("--standard", choices=("c++17", "c++14"), required=True)
    provenance.add_argument("--executable", required=True, type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _build_parser().parse_args(argv)
    try:
        if arguments.mode == "verify":
            verify_evidence(LEGACY._absolute(arguments.evidence_root))
            print(f"P8 continuous soak evidence verified: {arguments.evidence_root}")
            return 0
        if arguments.mode == "build-provenance":
            return _create_project_build_provenance(arguments)
        return _run(arguments)
    except (ContinuousSoakError, LEGACY.InvocationError) as error:
        print(f"P8 continuous soak error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
