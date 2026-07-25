#!/usr/bin/env python3
"""Verify a successful P8 Windows preacceptance evidence bundle.

The caller must supply the wrapper manifest SHA-256, HMAC-SHA-256, and
32-byte HMAC key kept outside the evidence directory.  This verifier proves
authenticated bundle integrity and local-tooling contract consistency only;
it never upgrades the evidence to product acceptance.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import hmac
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import shutil
import stat
import sys
import tempfile
import unicodedata
from typing import Any


SCHEMA_VERSION = "p8-windows-preacceptance-wrapper-v4"
RECEIPT_SCHEMA_VERSION = "p8-windows-evidence-import-receipt-v4"
MANIFEST_NAME = "wrapper-manifest.json"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
GIT_HEAD_RE = re.compile(r"^[0-9a-f]{40}$")
BASE_STEPS = (
    "host-report-collection",
    "preflight",
    "soak",
    "preflight-after-soak",
    "release-package",
    "release-verify",
    "release-activate",
)
MANIFEST_KEYS = {
    "schemaVersion",
    "scope",
    "overallResult",
    "productAcceptanceClaimed",
    "dDriveTargeted",
    "realIoEnabled",
    "realRejectEnabled",
    "realIoEnabledClaimed",
    "realRejectEnabledClaimed",
    "packageConfigurationRequiredRealRejectDisabled",
    "startedAt",
    "finishedAt",
    "deploymentRoot",
    "evidenceRoot",
    "releaseId",
    "hostReportChallenge",
    "packageManifestSha256",
    "releaseManifestSha256",
    "sourceManifestSha256",
    "sourceVersion",
    "provenanceFiles",
    "rollbackRequested",
    "rollbackExercised",
    "completedSteps",
    "failure",
    "files",
}
FILE_KEYS = {"path", "size", "sha256"}
SOURCE_VERSION_KEYS = {"gitHead", "gitDirty"}
PROVENANCE_KEYS = {"role", "repositoryPath", "path", "size", "sha256"}
REQUIRED_PROVENANCE = {
    "wrapper": (
        "scripts/run_windows_p8_preacceptance.ps1",
        "provenance/run_windows_p8_preacceptance.ps1",
    ),
    "preflight": (
        "scripts/p8_preflight.py",
        "provenance/p8_preflight.py",
    ),
    "host-report-collector": (
        "scripts/collect_windows_p8_host_reports.ps1",
        "provenance/collect_windows_p8_host_reports.ps1",
    ),
    "soak": (
        "scripts/p8_soak_evidence.py",
        "provenance/p8_soak_evidence.py",
    ),
    "release": (
        "scripts/p8_release.py",
        "provenance/p8_release.py",
    ),
    "evidence-verifier": (
        "scripts/p8_windows_evidence_verify.py",
        "provenance/p8_windows_evidence_verify.py",
    ),
    "gate-config": (
        "config/p8-local-gates-v1.json",
        "provenance/p8-local-gates-v1.json",
    ),
}
PREFLIGHT_REPORT_KEYS = {
    "schemaVersion",
    "generatedAtUtc",
    "status",
    "exitCode",
    "scope",
    "productAcceptance",
    "claims",
    "safety",
    "inputs",
    "missingExternalInputs",
    "checks",
}
PREFLIGHT_CLAIM_KEYS = {
    "localPreflightReady",
    "p8ProductAccepted",
    "windowsRuntimeAccepted",
    "gpuRuntimeAccepted",
    "realIoTested",
    "realRejectTested",
}
PREFLIGHT_INPUT_KEYS = {
    "gateConfig",
    "package",
    "manifest",
    "outputDirectory",
}
PREFLIGHT_CHECK_KEYS = {
    "gate.config": {"name", "status", "message", "sha256"},
    "package.manifest.identity": {"name", "status", "message", "sha256"},
    "package.root": {"name", "status", "message", "path"},
    "package.manifest.completeness": {
        "name", "status", "message", "fileCount",
    },
    "package.config": {
        "name", "status", "message", "currentBrand", "rejectEnabled",
    },
    "package.brand": {
        "name", "status", "message", "brand", "paraIni", "templateDirectory",
    },
    "external.windows": {
        "name", "status", "message", "path", "size", "sha256",
        "schemaVersion", "captureId", "hostIdSha256", "challenge",
        "packageManifestSha256", "collectorSha256",
        "collectorAssertionsValidated", "productAcceptanceChecked",
        "semanticAcceptanceChecked",
    },
    "external.gpu": {
        "name", "status", "message", "path", "size", "sha256",
        "schemaVersion", "captureId", "hostIdSha256", "challenge",
        "packageManifestSha256", "collectorSha256",
        "collectorAssertionsValidated", "productAcceptanceChecked",
        "semanticAcceptanceChecked",
    },
    "external.host-report-pair": {
        "name", "status", "message", "captureId", "hostIdSha256",
        "challenge", "packageManifestSha256", "collectorSha256",
        "productAcceptanceChecked", "semanticAcceptanceChecked",
    },
}


class EvidenceError(ValueError):
    """Raised when evidence is unsafe, incomplete, or internally inconsistent."""


class _DuplicateJsonKey(ValueError):
    pass


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateJsonKey(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def _reject_nonfinite(value: str) -> None:
    raise ValueError(f"non-finite JSON number: {value}")


def _load_json_bytes(raw: bytes, label: str) -> Any:
    try:
        text = raw.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise EvidenceError(f"{label}: JSON must be UTF-8") from exc
    try:
        return json.loads(
            text,
            object_pairs_hook=_strict_object,
            parse_constant=_reject_nonfinite,
        )
    except (_DuplicateJsonKey, ValueError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"{label}: invalid strict JSON: {exc}") from exc


def _is_reparse_point(metadata: os.stat_result) -> bool:
    attributes = getattr(metadata, "st_file_attributes", 0)
    flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & flag)


def _regular_bytes(path: Path, label: str) -> bytes:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise EvidenceError(f"{label}: cannot inspect {path}: {exc}") from exc
    if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
        raise EvidenceError(f"{label}: symbolic links/reparse points are forbidden")
    if not stat.S_ISREG(metadata.st_mode):
        raise EvidenceError(f"{label}: regular file required")
    try:
        return path.read_bytes()
    except OSError as exc:
        raise EvidenceError(f"{label}: cannot read {path}: {exc}") from exc


def _sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _sha256_file(path: Path, label: str) -> tuple[int, str]:
    raw = _regular_bytes(path, label)
    return len(raw), _sha256(raw)


def _current_verifier_sha256() -> str:
    return _sha256_file(
        Path(__file__).resolve(), "offline evidence verifier"
    )[1]


def _evidence_key_bytes(
    evidence_key: bytes | str | os.PathLike[str] | Path,
) -> bytes:
    if isinstance(evidence_key, bytes):
        raw = evidence_key
    else:
        raw = _regular_bytes(
            Path(os.path.abspath(os.fspath(evidence_key))),
            "external evidence HMAC key",
        )
    if len(raw) != 32:
        raise EvidenceError("external evidence HMAC key must be exactly 32 bytes")
    return raw


def _normalize_sha256(value: Any, label: str) -> str:
    if not isinstance(value, str):
        raise EvidenceError(f"{label}: SHA-256 must be a string")
    normalized = value.lower()
    if not SHA256_RE.fullmatch(normalized):
        raise EvidenceError(f"{label}: SHA-256 must be 64 hex digits")
    return normalized


def _exact_object(value: Any, keys: set[str], label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise EvidenceError(f"{label}: object required")
    actual = set(value)
    if actual != keys:
        raise EvidenceError(
            f"{label}: keys mismatch; missing={sorted(keys - actual)}, "
            f"unexpected={sorted(actual - keys)}"
        )
    return value


def _require_bool(value: Any, expected: bool, label: str) -> None:
    if value is not expected:
        raise EvidenceError(f"{label}: must be {expected}")


def _canonical_relative(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise EvidenceError(f"{label}: non-empty relative path required")
    if "\\" in value or unicodedata.normalize("NFC", value) != value:
        raise EvidenceError(f"{label}: canonical NFC POSIX path required")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise EvidenceError(f"{label}: unsafe relative path")
    canonical = path.as_posix()
    if canonical != value:
        raise EvidenceError(f"{label}: non-canonical relative path")
    return canonical


def _collision_key(value: str) -> str:
    return unicodedata.normalize("NFC", value).casefold()


def _windows_d_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise EvidenceError(f"{label}: non-empty Windows path required")
    path = PureWindowsPath(value)
    if not path.is_absolute() or path.drive.casefold() != "d:":
        raise EvidenceError(f"{label}: must be an absolute D:\\ path")
    if any(part == ".." for part in path.parts):
        raise EvidenceError(f"{label}: parent traversal is forbidden")
    return value


def _parse_time(value: Any, label: str) -> dt.datetime:
    if not isinstance(value, str):
        raise EvidenceError(f"{label}: timestamp string required")
    normalized = value
    seven_digit_fraction = re.search(
        r"(\.\d{6})\d(?=(?:Z|[+-]\d{2}:\d{2})$)",
        value,
    )
    if seven_digit_fraction is not None:
        normalized = (
            value[:seven_digit_fraction.end(1)]
            + value[seven_digit_fraction.end():]
        )
    try:
        parsed = dt.datetime.fromisoformat(normalized.replace("Z", "+00:00"))
    except ValueError as exc:
        raise EvidenceError(f"{label}: invalid ISO-8601 timestamp") from exc
    if parsed.tzinfo is None:
        raise EvidenceError(f"{label}: timezone is required")
    return parsed


def _snapshot_files(root: Path) -> dict[str, bytes]:
    result: dict[str, bytes] = {}
    identities: dict[str, str] = {}
    def fail_walk(error: OSError) -> None:
        raise error

    try:
        walker = os.walk(root, followlinks=False, onerror=fail_walk)
        for current, directory_names, file_names in walker:
            current_path = Path(current)
            for name in list(directory_names):
                candidate = current_path / name
                metadata = candidate.lstat()
                if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                    raise EvidenceError(
                        "evidence tree contains a symbolic-link/reparse directory: "
                        f"{candidate.relative_to(root).as_posix()}"
                    )
                if not stat.S_ISDIR(metadata.st_mode):
                    raise EvidenceError(
                        "evidence tree contains a non-directory entry: "
                        f"{candidate.relative_to(root).as_posix()}"
                    )
            for name in file_names:
                candidate = current_path / name
                relative = candidate.relative_to(root).as_posix()
                if relative == MANIFEST_NAME:
                    continue
                relative = _canonical_relative(relative, "evidence file path")
                identity = _collision_key(relative)
                previous = identities.get(identity)
                if previous is not None:
                    raise EvidenceError(
                        f"casefold/NFC-conflicting evidence paths: "
                        f"{previous!r} and {relative!r}"
                    )
                identities[identity] = relative
                result[relative] = _regular_bytes(
                    candidate, f"evidence file {relative}"
                )
    except OSError as exc:
        raise EvidenceError(f"cannot enumerate evidence tree: {exc}") from exc
    return result


def _snapshot_records(snapshot: dict[str, bytes]) -> dict[str, tuple[int, str]]:
    return {
        relative: (len(raw), _sha256(raw))
        for relative, raw in snapshot.items()
    }


def _validate_file_manifest(
    records: Any, snapshot: dict[str, bytes]
) -> dict[str, tuple[int, str]]:
    if not isinstance(records, list) or not records:
        raise EvidenceError("files: non-empty array required")
    declared: dict[str, tuple[int, str]] = {}
    identities: dict[str, str] = {}
    for index, item in enumerate(records):
        record = _exact_object(item, FILE_KEYS, f"files[{index}]")
        path = _canonical_relative(record["path"], f"files[{index}].path")
        size = record["size"]
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise EvidenceError(f"files[{index}].size: non-negative integer required")
        digest = _normalize_sha256(record["sha256"], f"files[{index}].sha256")
        if path in declared:
            raise EvidenceError(f"files: duplicate path {path!r}")
        identity = _collision_key(path)
        if identity in identities:
            raise EvidenceError(
                f"files: casefold/NFC conflict for {identities[identity]!r} "
                f"and {path!r}"
            )
        identities[identity] = path
        declared[path] = (size, digest)
    actual = _snapshot_records(snapshot)
    if declared != actual:
        missing = sorted(set(actual) - set(declared))
        extra = sorted(set(declared) - set(actual))
        mismatched = sorted(
            path for path in set(actual) & set(declared)
            if actual[path] != declared[path]
        )
        raise EvidenceError(
            f"files: bundle mismatch; unlisted={missing}, missing={extra}, "
            f"size-or-sha256={mismatched}"
        )
    return declared


def _validate_source_version(value: Any) -> dict[str, Any]:
    version = _exact_object(value, SOURCE_VERSION_KEYS, "sourceVersion")
    head = version["gitHead"]
    if not isinstance(head, str) or not GIT_HEAD_RE.fullmatch(head):
        raise EvidenceError("sourceVersion.gitHead: 40 lowercase hex digits required")
    if not isinstance(version["gitDirty"], bool):
        raise EvidenceError("sourceVersion.gitDirty: boolean required")
    return version


def _validate_provenance(
    value: Any, declared: dict[str, tuple[int, str]]
) -> list[dict[str, Any]]:
    if not isinstance(value, list):
        raise EvidenceError("provenanceFiles: array required")
    by_role: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(value):
        record = _exact_object(
            item, PROVENANCE_KEYS, f"provenanceFiles[{index}]"
        )
        role = record["role"]
        if not isinstance(role, str) or not role:
            raise EvidenceError(f"provenanceFiles[{index}].role: string required")
        if role in by_role:
            raise EvidenceError(f"provenanceFiles: duplicate role {role!r}")
        repository_path = _canonical_relative(
            record["repositoryPath"],
            f"provenanceFiles[{index}].repositoryPath",
        )
        path = _canonical_relative(
            record["path"], f"provenanceFiles[{index}].path"
        )
        size = record["size"]
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise EvidenceError(
                f"provenanceFiles[{index}].size: non-negative integer required"
            )
        digest = _normalize_sha256(
            record["sha256"], f"provenanceFiles[{index}].sha256"
        )
        if declared.get(path) != (size, digest):
            raise EvidenceError(
                f"provenanceFiles[{index}]: record does not match evidence file"
            )
        by_role[role] = {
            "role": role,
            "repositoryPath": repository_path,
            "path": path,
            "size": size,
            "sha256": digest,
        }
    if set(by_role) != set(REQUIRED_PROVENANCE):
        raise EvidenceError(
            "provenanceFiles: required roles mismatch; "
            f"missing={sorted(set(REQUIRED_PROVENANCE) - set(by_role))}, "
            f"unexpected={sorted(set(by_role) - set(REQUIRED_PROVENANCE))}"
        )
    for role, (repository_path, evidence_path) in REQUIRED_PROVENANCE.items():
        record = by_role[role]
        if (
            record["repositoryPath"] != repository_path
            or record["path"] != evidence_path
        ):
            raise EvidenceError(f"provenanceFiles: {role!r} path binding mismatch")
        trusted_path = Path(__file__).resolve().parent.parent.joinpath(
            *PurePosixPath(repository_path).parts
        )
        trusted_digest = _sha256(
            _regular_bytes(
                trusted_path,
                f"trusted repository source for provenance role {role!r}",
            )
        )
        if record["sha256"] != trusted_digest:
            raise EvidenceError(
                f"provenanceFiles: {role!r} does not match the trusted "
                "repository source"
            )
    return [by_role[role] for role in sorted(by_role)]


def _load_child(snapshot: dict[str, bytes], relative: str) -> dict[str, Any]:
    try:
        raw = snapshot[relative]
    except KeyError as exc:
        raise EvidenceError(f"{relative}: evidence file is missing") from exc
    value = _load_json_bytes(raw, relative)
    if not isinstance(value, dict):
        raise EvidenceError(f"{relative}: JSON object required")
    return value


def _load_command(
    snapshot: dict[str, bytes],
    relative: str,
    expected_script: PureWindowsPath,
    script_index: int = 1,
) -> list[str]:
    try:
        raw = snapshot[relative]
    except KeyError as exc:
        raise EvidenceError(f"{relative}: command transcript is missing") from exc
    value = _load_json_bytes(raw, relative)
    if (
        not isinstance(value, list)
        or len(value) <= script_index
        or any(not isinstance(item, str) or not item for item in value)
    ):
        raise EvidenceError(f"{relative}: non-empty argv array required")
    if PureWindowsPath(value[script_index]) != expected_script:
        raise EvidenceError(
            f"{relative}: expected copied script {str(expected_script)!r}"
        )
    return value


def _exact_option_values(
    argv: list[str],
    start: int,
    options: tuple[str, ...],
    label: str,
) -> dict[str, str]:
    expected_length = start + (2 * len(options))
    if len(argv) != expected_length:
        raise EvidenceError(f"{label}: exact argv shape required")
    values: dict[str, str] = {}
    cursor = start
    for option in options:
        if argv[cursor] != option:
            raise EvidenceError(
                f"{label}: expected {option!r} at argv[{cursor}]"
            )
        value = argv[cursor + 1]
        if not value:
            raise EvidenceError(f"{label}: non-empty {option} value required")
        values[option] = value
        cursor += 2
    return values


def _same_windows_path(first: str, second: str) -> bool:
    return PureWindowsPath(first) == PureWindowsPath(second)


def _positive_command_int(value: str, label: str) -> int:
    try:
        parsed = int(value)
    except ValueError as exc:
        raise EvidenceError(f"{label}: positive integer required") from exc
    if parsed < 1 or str(parsed) != value:
        raise EvidenceError(f"{label}: canonical positive integer required")
    return parsed


def _positive_command_float(value: str, label: str) -> float:
    try:
        parsed = float(value)
    except ValueError as exc:
        raise EvidenceError(f"{label}: positive finite number required") from exc
    if not (parsed > 0.0 and parsed < float("inf")):
        raise EvidenceError(f"{label}: positive finite number required")
    return parsed


def _validate_command_transcripts(
    snapshot: dict[str, bytes],
    wrapper: dict[str, Any],
    soak_document: dict[str, Any],
    package_manifest_sha256: str,
    release_manifest_sha256: str,
    rollback_requested: bool,
) -> None:
    evidence_root = PureWindowsPath(wrapper["evidenceRoot"])
    deployment_root = PureWindowsPath(wrapper["deploymentRoot"])
    provenance_root = evidence_root / "provenance"
    preflight_script = provenance_root / "p8_preflight.py"
    collector_script = provenance_root / "collect_windows_p8_host_reports.ps1"
    soak_script = provenance_root / "p8_soak_evidence.py"
    release_script = provenance_root / "p8_release.py"
    gate_config = provenance_root / "p8-local-gates-v1.json"
    collector = _load_command(
        snapshot,
        "host-collector.stdout.json.command.json",
        collector_script,
        4,
    )
    if collector[1:4] != ["-NoLogo", "-NoProfile", "-File"]:
        raise EvidenceError("host collector command: trusted child invocation required")
    collector_values = _exact_option_values(
        collector,
        5,
        (
            "-RepositoryRoot",
            "-Package",
            "-OutputDirectory",
            "-Challenge",
            "-PackageManifestSha256",
        ),
        "host collector command",
    )
    if collector_values["-Challenge"] != wrapper["hostReportChallenge"]:
        raise EvidenceError("host collector command: wrapper challenge mismatch")
    if (
        collector_values["-PackageManifestSha256"]
        != package_manifest_sha256
    ):
        raise EvidenceError(
            "host collector command: package manifest identity mismatch"
        )
    if not _same_windows_path(
        collector_values["-OutputDirectory"],
        str(evidence_root / "host-inputs"),
    ):
        raise EvidenceError("host collector command: output directory mismatch")
    if not PureWindowsPath(
        collector_values["-RepositoryRoot"]
    ).is_absolute():
        raise EvidenceError(
            "host collector command: absolute RepositoryRoot required"
        )
    _windows_d_path(
        collector_values["-Package"],
        "host collector command -Package",
    )
    preflight_options = (
        "--gate-config",
        "--package",
        "--manifest",
        "--manifest-sha256",
        "--host-challenge",
        "--windows-input",
        "--gpu-input",
        "--output-dir",
    )
    preflight_commands = (
        ("preflight.stdout.log.command.json", evidence_root / "preflight"),
        (
            "preflight-after-soak.stdout.log.command.json",
            evidence_root / "preflight-after-soak",
        ),
    )
    preflight_values: list[dict[str, str]] = []
    for relative, expected_output in preflight_commands:
        argv = _load_command(snapshot, relative, preflight_script)
        values = _exact_option_values(argv, 2, preflight_options, relative)
        if values["--manifest-sha256"].lower() != package_manifest_sha256:
            raise EvidenceError(f"{relative}: package manifest identity mismatch")
        if values["--host-challenge"] != wrapper["hostReportChallenge"]:
            raise EvidenceError(f"{relative}: host challenge mismatch")
        if not _same_windows_path(values["--gate-config"], str(gate_config)):
            raise EvidenceError(f"{relative}: copied gate config path mismatch")
        if not _same_windows_path(values["--output-dir"], str(expected_output)):
            raise EvidenceError(f"{relative}: output directory mismatch")
        if not _same_windows_path(
            values["--windows-input"],
            str(evidence_root / "host-inputs" / "windows.json"),
        ):
            raise EvidenceError(
                f"{relative}: copied Windows host report path mismatch"
            )
        if not _same_windows_path(
            values["--gpu-input"],
            str(evidence_root / "host-inputs" / "gpu.json"),
        ):
            raise EvidenceError(
                f"{relative}: copied GPU host report path mismatch"
            )
        for option in (
            "--package",
            "--manifest",
            "--windows-input",
            "--gpu-input",
        ):
            _windows_d_path(values[option], f"{relative} {option}")
        preflight_values.append(values)
    for option in (
        "--gate-config",
        "--package",
        "--manifest",
        "--manifest-sha256",
        "--host-challenge",
        "--windows-input",
        "--gpu-input",
    ):
        if (
            option in {"--manifest-sha256", "--host-challenge"}
            and preflight_values[0][option].lower()
            == preflight_values[1][option].lower()
        ):
            continue
        if option not in {"--manifest-sha256", "--host-challenge"} and _same_windows_path(
            preflight_values[0][option], preflight_values[1][option]
        ):
            continue
        raise EvidenceError(f"preflight commands: {option} drifted after soak")
    package_path = preflight_values[0]["--package"]
    package_manifest_path = preflight_values[0]["--manifest"]
    if not _same_windows_path(
        collector_values["-Package"],
        package_path,
    ):
        raise EvidenceError(
            "host collector and preflight package paths do not match"
        )

    soak = _load_command(
        snapshot, "soak.stdout.log.command.json", soak_script
    )
    separator_indexes = [
        index for index, value in enumerate(soak) if value == "--"
    ]
    if len(separator_indexes) != 1:
        raise EvidenceError("soak command: exactly one -- separator required")
    separator = separator_indexes[0]
    soak_options = (
        "--evidence-root",
        "--profile",
        "--cwd",
        "--rounds",
        "--restarts",
        "--timeout-seconds",
    )
    soak_values = _exact_option_values(
        soak[:separator], 2, soak_options, "soak command"
    )
    command_template = soak[separator + 1 :]
    if not command_template:
        raise EvidenceError("soak command: executable argv after -- required")
    if soak_values["--profile"] != "ci-contract-v1":
        raise EvidenceError("soak command: ci-contract-v1 profile required")
    if not _same_windows_path(
        soak_values["--evidence-root"], str(evidence_root / "soak")
    ):
        raise EvidenceError("soak command: evidence root mismatch")
    if not _same_windows_path(soak_values["--cwd"], package_path):
        raise EvidenceError("soak command: cwd must equal package")
    executable = PureWindowsPath(command_template[0])
    package_windows = PureWindowsPath(package_path)
    if executable == package_windows or package_windows not in executable.parents:
        raise EvidenceError("soak command: executable must be inside package")
    rounds = _positive_command_int(soak_values["--rounds"], "soak rounds")
    restarts = _positive_command_int(
        soak_values["--restarts"], "soak restarts"
    )
    timeout_seconds = _positive_command_float(
        soak_values["--timeout-seconds"], "soak timeout"
    )
    _validate_soak(
        soak_document,
        command_template,
        package_path,
        soak_values["--evidence-root"],
        rounds,
        restarts,
        timeout_seconds,
    )

    package = _load_command(
        snapshot, "release/package.stdout.json.command.json", release_script
    )
    if len(package) < 3 or package[2] != "package":
        raise EvidenceError("release package command mismatch")
    package_values = _exact_option_values(
        package,
        3,
        (
            "--source",
            "--deployment-root",
            "--release-id",
            "--source-manifest",
            "--source-manifest-sha256",
        ),
        "release package command",
    )
    if package_values["--source-manifest-sha256"].lower() != package_manifest_sha256:
        raise EvidenceError("release package command source identity mismatch")
    if not _same_windows_path(package_values["--source"], package_path):
        raise EvidenceError("release package command source path mismatch")
    if not _same_windows_path(
        package_values["--source-manifest"], package_manifest_path
    ):
        raise EvidenceError("release package command source manifest path mismatch")
    if not _same_windows_path(
        package_values["--deployment-root"], str(deployment_root)
    ):
        raise EvidenceError("release package command deployment root mismatch")
    if package_values["--release-id"] != wrapper["releaseId"]:
        raise EvidenceError("release package command release ID mismatch")

    verify = _load_command(
        snapshot, "release/verify.stdout.json.command.json", release_script
    )
    activate = _load_command(
        snapshot, "release/activate.stdout.json.command.json", release_script
    )
    for name, argv in (("verify", verify), ("activate", activate)):
        if len(argv) < 3 or argv[2] != name:
            raise EvidenceError(f"release {name} command mismatch")
        values = _exact_option_values(
            argv,
            3,
            ("--deployment-root", "--release-id", "--manifest-sha256"),
            f"release {name} command",
        )
        if values["--manifest-sha256"].lower() != release_manifest_sha256:
            raise EvidenceError(f"release {name} command identity mismatch")
        if not _same_windows_path(
            values["--deployment-root"], str(deployment_root)
        ):
            raise EvidenceError(
                f"release {name} command deployment root mismatch"
            )
        if values["--release-id"] != wrapper["releaseId"]:
            raise EvidenceError(f"release {name} command release ID mismatch")
    if rollback_requested:
        rollback = _load_command(
            snapshot,
            "release/rollback.stdout.json.command.json",
            release_script,
        )
        if len(rollback) < 3 or rollback[2] != "rollback":
            raise EvidenceError("release rollback command mismatch")
        rollback_values = _exact_option_values(
            rollback,
            3,
            ("--deployment-root",),
            "release rollback command",
        )
        if not _same_windows_path(
            rollback_values["--deployment-root"], str(deployment_root)
        ):
            raise EvidenceError("release rollback command deployment root mismatch")


def _find_check(document: dict[str, Any], name: str, label: str) -> dict[str, Any]:
    checks = document.get("checks")
    if not isinstance(checks, list):
        raise EvidenceError(f"{label}.checks: array required")
    matches = [
        item for item in checks
        if isinstance(item, dict) and item.get("name") == name
    ]
    if len(matches) != 1:
        raise EvidenceError(f"{label}: exactly one {name!r} check required")
    return matches[0]


def _load_trusted_host_report_validator() -> Any:
    validator_path = Path(__file__).resolve().with_name("p8_preflight.py")
    specification = importlib.util.spec_from_file_location(
        "_cigvision_trusted_p8_preflight",
        validator_path,
    )
    if specification is None or specification.loader is None:
        raise EvidenceError(
            "trusted P8 preflight host-report validator cannot be loaded"
        )
    module = importlib.util.module_from_spec(specification)
    try:
        specification.loader.exec_module(module)
    except Exception as exc:
        raise EvidenceError(
            f"trusted P8 preflight host-report validator failed to load: {exc}"
        ) from exc
    return module


def _validate_host_report_inputs(
    snapshot: dict[str, bytes],
    collector_sha256: str,
    expected_challenge: str,
    expected_package_manifest_sha256: str,
) -> tuple[dict[str, str], dict[str, str]]:
    validator = _load_trusted_host_report_validator()
    identities: dict[str, dict[str, str]] = {}
    digests: dict[str, str] = {}
    for kind, relative in (
        ("windows", "host-inputs/windows.json"),
        ("gpu", "host-inputs/gpu.json"),
    ):
        try:
            raw = snapshot[relative]
        except KeyError as exc:
            raise EvidenceError(f"{relative}: evidence file is missing") from exc
        try:
            document = validator.load_strict_json_bytes(raw, relative)
            identity = validator.validate_host_report_document(
                document,
                kind,
                collector_sha256,
                expected_challenge,
                expected_package_manifest_sha256,
            )
        except validator.StrictInputError as exc:
            raise EvidenceError(f"{relative}: {exc}") from exc
        identities[kind] = identity
        digests[f"external.{kind}"] = _sha256(raw)

    compared_fields = (
        "captureId",
        "capturedAtUtc",
        "hostIdSha256",
        "collectorSha256",
    )
    drifted = [
        field
        for field in compared_fields
        if identities["windows"][field] != identities["gpu"][field]
    ]
    if drifted:
        raise EvidenceError(
            "host input reports are not one collector capture; "
            f"driftedFields={drifted}"
        )
    return digests, identities["windows"]


def _validate_host_collector_summary(
    snapshot: dict[str, bytes],
    wrapper: dict[str, Any],
    host_digests: dict[str, str],
    host_identity: dict[str, str],
) -> None:
    summary = _exact_object(
        _load_child(snapshot, "host-collector.stdout.json"),
        {
            "schemaVersion", "status", "exitCode", "captureId",
            "capturedAtUtc", "hostIdSha256", "challenge",
            "packageManifestSha256", "outputDirectory", "reports",
        },
        "host collector summary",
    )
    if (
        summary["schemaVersion"] != "cigvision-p8-host-collector-summary-v1"
        or summary["status"] != "collection-complete"
        or summary["exitCode"] != 0
    ):
        raise EvidenceError("host collector summary: successful collection required")
    for name in (
        "captureId",
        "capturedAtUtc",
        "hostIdSha256",
        "challenge",
        "packageManifestSha256",
    ):
        if summary[name] != host_identity[name]:
            raise EvidenceError(
                f"host collector summary: {name} identity mismatch"
            )
    if summary["challenge"] != wrapper["hostReportChallenge"]:
        raise EvidenceError("host collector summary: wrapper challenge mismatch")
    if not _same_windows_path(
        summary["outputDirectory"],
        str(PureWindowsPath(wrapper["evidenceRoot"]) / "host-inputs"),
    ):
        raise EvidenceError("host collector summary: output directory mismatch")
    reports = _exact_object(
        summary["reports"],
        {"windows", "gpu"},
        "host collector summary.reports",
    )
    for kind in ("windows", "gpu"):
        report = _exact_object(
            reports[kind],
            {"path", "sha256", "collectionComplete", "failedCheckIds"},
            f"host collector summary.reports.{kind}",
        )
        if (
            report["path"] != f"{kind}.json"
            or report["sha256"] != host_digests[f"external.{kind}"]
            or report["collectionComplete"] is not True
            or report["failedCheckIds"] != []
        ):
            raise EvidenceError(
                f"host collector summary.reports.{kind}: identity mismatch"
            )


def _validate_ownership_markers(
    snapshot: dict[str, bytes],
    expected_challenge: str,
) -> None:
    for relative in (
        ".wrapper-owner",
        "host-inputs/.collector-owner",
    ):
        try:
            raw = snapshot[relative]
        except KeyError as exc:
            raise EvidenceError(f"{relative}: ownership marker is missing") from exc
        try:
            observed = raw.decode("ascii").strip()
        except UnicodeDecodeError as exc:
            raise EvidenceError(
                f"{relative}: ownership marker must be ASCII"
            ) from exc
        if observed != expected_challenge:
            raise EvidenceError(f"{relative}: ownership challenge mismatch")


def _validate_preflight(
    document: dict[str, Any],
    expected_manifest_sha256: str,
    expected_host_challenge: str,
    wrapper_started: dt.datetime,
    wrapper_finished: dt.datetime,
    label: str,
) -> dict[str, str]:
    document = _exact_object(document, PREFLIGHT_REPORT_KEYS, label)
    if document.get("schemaVersion") != "p8-local-preflight-report-v1":
        raise EvidenceError(f"{label}: schemaVersion mismatch")
    if document.get("status") != "ready" or document.get("exitCode") != 0:
        raise EvidenceError(f"{label}: ready/exit 0 required")
    if document.get("scope") != "local-tooling-preflight-only":
        raise EvidenceError(f"{label}: scope mismatch")
    _require_bool(document.get("productAcceptance"), False,
                  f"{label}.productAcceptance")
    if document.get("missingExternalInputs") != []:
        raise EvidenceError(f"{label}: external inputs must be present")
    safety = document.get("safety")
    if safety != {
        "localOnly": True,
        "realIoEnabled": False,
        "realRejectEnabled": False,
    }:
        raise EvidenceError(f"{label}: local-only safety contract mismatch")
    generated_at = _parse_time(
        document["generatedAtUtc"],
        f"{label}.generatedAtUtc",
    )
    if not wrapper_started <= generated_at <= wrapper_finished:
        raise EvidenceError(
            f"{label}: report was not generated during this wrapper run"
        )
    claims = _exact_object(
        document.get("claims"),
        PREFLIGHT_CLAIM_KEYS,
        f"{label}.claims",
    )
    if claims.get("localPreflightReady") is not True:
        raise EvidenceError(f"{label}: local preflight ready claim required")
    for name in (
        "p8ProductAccepted",
        "windowsRuntimeAccepted",
        "gpuRuntimeAccepted",
        "realIoTested",
        "realRejectTested",
    ):
        _require_bool(claims.get(name), False, f"{label}.claims.{name}")
    inputs = _exact_object(
        document.get("inputs"),
        PREFLIGHT_INPUT_KEYS,
        f"{label}.inputs",
    )
    for name, value in inputs.items():
        if not isinstance(value, str) or not value:
            raise EvidenceError(f"{label}.inputs.{name}: non-empty string required")
    checks = document.get("checks")
    if not isinstance(checks, list):
        raise EvidenceError(f"{label}.checks: array required")
    by_name: dict[str, dict[str, Any]] = {}
    for index, item in enumerate(checks):
        if not isinstance(item, dict):
            raise EvidenceError(f"{label}.checks[{index}]: object required")
        name = item.get("name")
        if name not in PREFLIGHT_CHECK_KEYS:
            raise EvidenceError(
                f"{label}.checks[{index}]: unknown check name {name!r}"
            )
        if name in by_name:
            raise EvidenceError(f"{label}.checks: duplicate check {name!r}")
        check = _exact_object(
            item,
            PREFLIGHT_CHECK_KEYS[name],
            f"{label}.checks[{index}]",
        )
        if check["status"] != "ok":
            raise EvidenceError(f"{label}.{name}: status must be ok")
        if not isinstance(check["message"], str) or not check["message"]:
            raise EvidenceError(f"{label}.{name}: message must be non-empty")
        by_name[name] = check
    if set(by_name) != set(PREFLIGHT_CHECK_KEYS):
        raise EvidenceError(
            f"{label}.checks: exact successful check set required"
        )
    package_root = by_name["package.root"]
    if not isinstance(package_root["path"], str) or not package_root["path"]:
        raise EvidenceError(f"{label}.package.root.path: non-empty string required")
    completeness = by_name["package.manifest.completeness"]
    if (
        isinstance(completeness["fileCount"], bool)
        or not isinstance(completeness["fileCount"], int)
        or completeness["fileCount"] < 1
    ):
        raise EvidenceError(
            f"{label}.package.manifest.completeness.fileCount: "
            "positive integer required"
        )
    package_config = by_name["package.config"]
    if (
        not isinstance(package_config["currentBrand"], str)
        or not package_config["currentBrand"]
        or package_config["rejectEnabled"] is not False
    ):
        raise EvidenceError(f"{label}.package.config: safe values required")
    package_brand = by_name["package.brand"]
    for field in ("brand", "paraIni", "templateDirectory"):
        if (
            not isinstance(package_brand[field], str)
            or not package_brand[field]
        ):
            raise EvidenceError(
                f"{label}.package.brand.{field}: non-empty string required"
            )
    identity = _find_check(document, "package.manifest.identity", label)
    if identity.get("sha256") != expected_manifest_sha256:
        raise EvidenceError(f"{label}: package manifest identity mismatch")
    gate = _find_check(document, "gate.config", label)
    gate_digest = _normalize_sha256(
        gate.get("sha256"), f"{label}.gate.config.sha256"
    )
    external_digests: dict[str, str] = {}
    collector_digests: set[str] = set()
    external_checks: dict[str, dict[str, Any]] = {}
    for name in ("external.windows", "external.gpu"):
        external = _find_check(document, name, label)
        external_checks[name] = external
        expected_schema = (
            "p8-windows-host-report-v2"
            if name == "external.windows"
            else "p8-gpu-host-report-v2"
        )
        if external["schemaVersion"] != expected_schema:
            raise EvidenceError(f"{label}.{name}: schemaVersion mismatch")
        if (
            not isinstance(external["path"], str)
            or not external["path"]
            or isinstance(external["size"], bool)
            or not isinstance(external["size"], int)
            or external["size"] < 1
        ):
            raise EvidenceError(f"{label}.{name}: path/size contract mismatch")
        if (
            not isinstance(external["captureId"], str)
            or not re.fullmatch(r"[0-9a-f]{32}", external["captureId"])
        ):
            raise EvidenceError(f"{label}.{name}: captureId mismatch")
        _normalize_sha256(
            external["hostIdSha256"],
            f"{label}.{name}.hostIdSha256",
        )
        if external.get("semanticAcceptanceChecked") is not False:
            raise EvidenceError(
                f"{label}.{name}: semantic acceptance must remain unclaimed"
            )
        if external.get("collectorAssertionsValidated") is not True:
            raise EvidenceError(
                f"{label}.{name}: collector assertions must be validated"
            )
        if external.get("productAcceptanceChecked") is not False:
            raise EvidenceError(
                f"{label}.{name}: product acceptance must remain unclaimed"
            )
        collector_digests.add(_normalize_sha256(
            external.get("collectorSha256"),
            f"{label}.{name}.collectorSha256",
        ))
        external_digests[name] = _normalize_sha256(
            external.get("sha256"), f"{label}.{name}.sha256"
        )
        if external.get("packageManifestSha256") != expected_manifest_sha256:
            raise EvidenceError(
                f"{label}.{name}: package manifest challenge binding mismatch"
            )
        if external.get("challenge") != expected_host_challenge:
            raise EvidenceError(f"{label}.{name}: host challenge mismatch")
    if len(collector_digests) != 1:
        raise EvidenceError(f"{label}: host report collector identity mismatch")
    host_pair = _find_check(document, "external.host-report-pair", label)
    if (
        host_pair.get("semanticAcceptanceChecked") is not False
        or host_pair.get("productAcceptanceChecked") is not False
    ):
        raise EvidenceError(
            f"{label}.external.host-report-pair: acceptance must remain unclaimed"
        )
    collector_digest = next(iter(collector_digests))
    if host_pair.get("collectorSha256") != collector_digest:
        raise EvidenceError(
            f"{label}: host report pair collector identity mismatch"
        )
    if host_pair.get("packageManifestSha256") != expected_manifest_sha256:
        raise EvidenceError(
            f"{label}: host report pair package manifest binding mismatch"
        )
    if host_pair.get("challenge") != expected_host_challenge:
        raise EvidenceError(f"{label}: host report pair challenge mismatch")
    if (
        host_pair.get("challenge")
        != external_checks["external.windows"].get("challenge")
        or host_pair.get("challenge")
        != external_checks["external.gpu"].get("challenge")
    ):
        raise EvidenceError(f"{label}: host report challenge binding mismatch")
    for field in ("captureId", "hostIdSha256"):
        if (
            host_pair.get(field)
            != external_checks["external.windows"].get(field)
            or host_pair.get(field)
            != external_checks["external.gpu"].get(field)
        ):
            raise EvidenceError(f"{label}: host report {field} binding mismatch")
    return {
        "gate.config": gate_digest,
        "host-report-collector": collector_digest,
        **external_digests,
    }


def _validate_soak(
    document: dict[str, Any],
    expected_command: list[str],
    expected_cwd: str,
    expected_evidence_root: str,
    rounds: int,
    restarts: int,
    timeout_seconds: float,
) -> None:
    if document.get("schemaVersion") != "p8-soak-evidence-v1":
        raise EvidenceError("soak manifest: schemaVersion mismatch")
    if document.get("overallResult") != "passed-local-tooling":
        raise EvidenceError("soak manifest: passed-local-tooling required")
    _require_bool(document.get("sdkFree"), True, "soak manifest.sdkFree")
    _require_bool(
        document.get("productAcceptanceClaimed"),
        False,
        "soak manifest.productAcceptanceClaimed",
    )
    _require_bool(
        document.get("windowsRuntimeVerified"),
        False,
        "soak manifest.windowsRuntimeVerified",
    )
    if document.get("gpu") != "not-collected":
        raise EvidenceError("soak manifest: GPU must be not-collected")
    _require_bool(document.get("gpuClaimed"), False, "soak manifest.gpuClaimed")
    if document.get("failures") != []:
        raise EvidenceError("soak manifest: failures must be empty")
    profile = document.get("profile")
    if not isinstance(profile, dict):
        raise EvidenceError("soak manifest.profile: object required")
    if (
        profile.get("name") != "ci-contract-v1"
        or profile.get("durationClass") != "short"
        or profile.get("claimScope") != "sdk-free-local-contract-only"
    ):
        raise EvidenceError("soak manifest: profile identity/scope mismatch")
    thresholds = profile.get("thresholds")
    planned_expected = rounds * restarts
    if not isinstance(thresholds, dict):
        raise EvidenceError("soak manifest.profile.thresholds: object required")
    if (
        thresholds.get("roundsPerRestart") != rounds
        or thresholds.get("restarts") != restarts
        or thresholds.get("plannedRuns") != planned_expected
        or thresholds.get("timeoutSeconds") != timeout_seconds
        or thresholds.get("minimumSuccessRate") != 1.0
        or thresholds.get("maximumTimeouts") != 0
        or thresholds.get("maximumCrashes") != 0
    ):
        raise EvidenceError("soak manifest: profile thresholds mismatch")
    if document.get("commandTemplate") != expected_command:
        raise EvidenceError("soak manifest: command template mismatch")
    if not _same_windows_path(str(document.get("cwd")), expected_cwd):
        raise EvidenceError("soak manifest: cwd mismatch")
    checks = document.get("checks")
    required_gate_names = {
        "all-runs-completed",
        "success-rate",
        "timeouts",
        "crashes",
        "disk-free-floor",
        "output-budget",
        "rss-collected",
        "rss-post-warmup-growth",
        "process-groups-clean",
        "regular-evidence-files",
    }
    if not isinstance(checks, list):
        raise EvidenceError("soak manifest.checks: array required")
    gate_names = [
        item.get("name") for item in checks if isinstance(item, dict)
    ]
    if (
        len(gate_names) != len(checks)
        or len(set(gate_names)) != len(gate_names)
        or set(gate_names) != required_gate_names
        or any(item.get("status") != "passed" for item in checks)
    ):
        raise EvidenceError(
            "soak manifest: exact required passed gate set required"
        )
    summary = document.get("summary")
    if not isinstance(summary, dict):
        raise EvidenceError("soak manifest.summary: object required")
    planned = summary.get("plannedRuns")
    if (
        isinstance(planned, bool)
        or not isinstance(planned, int)
        or planned != planned_expected
        or summary.get("completedRuns") != planned
        or summary.get("successfulRuns") != planned
        or summary.get("successRate") != 1.0
        or summary.get("successRatePercent") != 100.0
        or summary.get("timeouts") != 0
        or summary.get("crashes") != 0
    ):
        raise EvidenceError("soak manifest: run summary does not prove 100% success")
    runs = document.get("runs")
    if not isinstance(runs, list) or len(runs) != planned:
        raise EvidenceError("soak manifest: run count mismatch")
    observed_pairs: set[tuple[int, int]] = set()
    for index, run in enumerate(runs):
        if not isinstance(run, dict):
            raise EvidenceError(f"soak manifest.runs[{index}]: object required")
        if (
            run.get("succeeded") is not True
            or run.get("exitCode") != 0
            or run.get("timedOut") is not False
            or run.get("crashed") is not False
            or run.get("launchError") != ""
            or run.get("terminationReason") != ""
            or run.get("outputBudgetExceeded") is not False
            or run.get("outputDirectoryFresh") is not True
            or not _same_windows_path(str(run.get("cwd")), expected_cwd)
        ):
            raise EvidenceError(f"soak manifest.runs[{index}]: success required")
        restart = run.get("restart")
        round_number = run.get("round")
        iteration = run.get("iteration")
        if (
            isinstance(restart, bool)
            or not isinstance(restart, int)
            or not 1 <= restart <= restarts
            or isinstance(round_number, bool)
            or not isinstance(round_number, int)
            or not 1 <= round_number <= rounds
            or iteration != index + 1
        ):
            raise EvidenceError(
                f"soak manifest.runs[{index}]: run coordinates mismatch"
            )
        expected_output_relative = (
            f"runs/restart-{restart:03d}/round-{round_number:03d}/output"
        )
        if run.get("outputDirectory") != expected_output_relative:
            raise EvidenceError(
                f"soak manifest.runs[{index}]: output directory mismatch"
            )
        replacements = {
            "{output_dir}": str(
                PureWindowsPath(expected_evidence_root)
                / PureWindowsPath(expected_output_relative)
            ),
            "{evidence_root}": expected_evidence_root,
            "{restart}": str(restart),
            "{round}": str(round_number),
            "{iteration}": str(iteration),
        }
        expected_run_command: list[str] = []
        for argument in expected_command:
            expanded = argument
            for token, replacement in replacements.items():
                expanded = expanded.replace(token, replacement)
            expected_run_command.append(expanded)
        if run.get("argv") != expected_run_command:
            raise EvidenceError(
                f"soak manifest.runs[{index}]: expanded command mismatch"
            )
        observed_pairs.add((restart, round_number))
        termination = run.get("processGroupTermination")
        if (
            not isinstance(termination, dict)
            or termination.get("noResidualProcessConfirmed") is not True
        ):
            raise EvidenceError(
                f"soak manifest.runs[{index}]: residual process not disproved"
            )
    expected_pairs = {
        (restart, round_number)
        for restart in range(1, restarts + 1)
        for round_number in range(1, rounds + 1)
    }
    if observed_pairs != expected_pairs:
        raise EvidenceError("soak manifest: restart/round coverage mismatch")


def _release_reference(value: Any, label: str) -> tuple[str, str]:
    reference = _exact_object(
        value, {"releaseId", "manifestSha256"}, label
    )
    release_id = reference["releaseId"]
    if not isinstance(release_id, str) or not release_id:
        raise EvidenceError(f"{label}.releaseId: non-empty string required")
    return release_id, _normalize_sha256(
        reference["manifestSha256"], f"{label}.manifestSha256"
    )


def _validate_release_outputs(
    snapshot: dict[str, bytes],
    deployment_root: str,
    release_id: str,
    release_manifest_sha256: str,
    package_manifest_sha256: str,
    rollback_requested: bool,
) -> None:
    expected_release_dir = str(
        PureWindowsPath(deployment_root) / "releases" / release_id
    )
    packaged = _exact_object(
        _load_child(snapshot, "release/package.stdout.json"),
        {
            "artifactKind",
            "scope",
            "releaseId",
            "releaseDir",
            "manifestSha256",
            "fileCount",
            "idempotent",
            "sourceManifestSha256",
        },
        "release package output",
    )
    if (
        packaged["artifactKind"] != "fixture-local-tooling"
        or packaged["scope"] != "fixture-local-tooling-only"
        or packaged["releaseId"] != release_id
        or packaged["manifestSha256"] != release_manifest_sha256
        or packaged["sourceManifestSha256"] != package_manifest_sha256
        or not _same_windows_path(
            str(packaged["releaseDir"]), expected_release_dir
        )
        or isinstance(packaged["fileCount"], bool)
        or not isinstance(packaged["fileCount"], int)
        or packaged["fileCount"] < 1
        or not isinstance(packaged["idempotent"], bool)
    ):
        raise EvidenceError("release package output identity/scope mismatch")
    verified = _exact_object(
        _load_child(snapshot, "release/verify.stdout.json"),
        {
            "artifactKind",
            "scope",
            "releaseId",
            "releaseDir",
            "manifestSha256",
            "fileCount",
            "verified",
        },
        "release verify output",
    )
    if (
        verified["verified"] is not True
        or verified["artifactKind"] != "fixture-local-tooling"
        or verified["scope"] != "fixture-local-tooling-only"
        or verified["releaseId"] != release_id
        or verified["manifestSha256"] != release_manifest_sha256
        or verified["fileCount"] != packaged["fileCount"]
        or not _same_windows_path(
            str(verified["releaseDir"]), expected_release_dir
        )
    ):
        raise EvidenceError("release verify output identity/scope mismatch")
    activated = _exact_object(
        _load_child(snapshot, "release/activate.stdout.json"),
        {"schemaVersion", "artifactKind", "scope", "current", "previous"},
        "release activate output",
    )
    if activated["schemaVersion"] != "p8-fixture-current-state-v1":
        raise EvidenceError("release activate schema mismatch")
    activated_current = _release_reference(
        activated.get("current"), "release activate.current"
    )
    if activated_current != (release_id, release_manifest_sha256):
        raise EvidenceError("release activate did not select the packaged release")
    if activated["artifactKind"] != "fixture-local-tooling":
        raise EvidenceError("release activate artifact kind mismatch")
    if activated["scope"] != "fixture-local-tooling-only":
        raise EvidenceError("release activate scope mismatch")
    if activated["previous"] is not None:
        _release_reference(activated["previous"], "release activate.previous")
    if rollback_requested:
        previous = _release_reference(
            activated.get("previous"), "release activate.previous"
        )
        rolled_back = _exact_object(
            _load_child(snapshot, "release/rollback.stdout.json"),
            {"schemaVersion", "artifactKind", "scope", "current", "previous"},
            "release rollback output",
        )
        if rolled_back["schemaVersion"] != "p8-fixture-current-state-v1":
            raise EvidenceError("release rollback schema mismatch")
        rollback_current = _release_reference(
            rolled_back.get("current"), "release rollback.current"
        )
        rollback_previous = _release_reference(
            rolled_back.get("previous"), "release rollback.previous"
        )
        if rollback_current != previous or rollback_previous != activated_current:
            raise EvidenceError("release rollback did not restore A after A→B")
        if (
            rolled_back.get("artifactKind") != "fixture-local-tooling"
            or rolled_back.get("scope") != "fixture-local-tooling-only"
        ):
            raise EvidenceError("release rollback scope mismatch")


def _validate_evidence_root(root: Path) -> None:
    try:
        root_metadata = root.lstat()
    except OSError as exc:
        raise EvidenceError(f"evidence root cannot be inspected: {exc}") from exc
    if (
        stat.S_ISLNK(root_metadata.st_mode)
        or _is_reparse_point(root_metadata)
        or not stat.S_ISDIR(root_metadata.st_mode)
    ):
        raise EvidenceError("evidence root must be a real directory")


def _validate_wrapper_document(
    document: dict[str, Any],
) -> tuple[str, str, str, bool, bool]:
    if document["schemaVersion"] != SCHEMA_VERSION:
        raise EvidenceError("wrapper manifest: schemaVersion mismatch")
    if document["scope"] != "local-tooling-on-windows-host-only":
        raise EvidenceError("wrapper manifest: scope mismatch")
    if document["overallResult"] != "passed-local-tooling":
        raise EvidenceError("wrapper manifest: successful evidence required")
    _require_bool(
        document["productAcceptanceClaimed"],
        False,
        "wrapper manifest.productAcceptanceClaimed",
    )
    _require_bool(document["dDriveTargeted"], True,
                  "wrapper manifest.dDriveTargeted")
    if document["realIoEnabled"] is not None or document["realRejectEnabled"] is not None:
        raise EvidenceError("wrapper manifest: real IO/reject state must be unknown")
    _require_bool(
        document["realIoEnabledClaimed"],
        False,
        "wrapper manifest.realIoEnabledClaimed",
    )
    _require_bool(
        document["realRejectEnabledClaimed"],
        False,
        "wrapper manifest.realRejectEnabledClaimed",
    )
    _require_bool(
        document["packageConfigurationRequiredRealRejectDisabled"],
        True,
        "wrapper manifest.packageConfigurationRequiredRealRejectDisabled",
    )
    started = _parse_time(document["startedAt"], "wrapper manifest.startedAt")
    finished = _parse_time(document["finishedAt"], "wrapper manifest.finishedAt")
    if finished < started:
        raise EvidenceError("wrapper manifest: finishedAt precedes startedAt")
    _windows_d_path(document["deploymentRoot"], "wrapper manifest.deploymentRoot")
    _windows_d_path(document["evidenceRoot"], "wrapper manifest.evidenceRoot")
    release_id = document["releaseId"]
    if not isinstance(release_id, str) or not release_id:
        raise EvidenceError("wrapper manifest.releaseId: non-empty string required")
    host_challenge = _normalize_sha256(
        document["hostReportChallenge"],
        "wrapper manifest.hostReportChallenge",
    )
    if document["hostReportChallenge"] != host_challenge:
        raise EvidenceError(
            "wrapper manifest.hostReportChallenge: lowercase hex required"
        )
    package_digest = _normalize_sha256(
        document["packageManifestSha256"],
        "wrapper manifest.packageManifestSha256",
    )
    release_digest = _normalize_sha256(
        document["releaseManifestSha256"],
        "wrapper manifest.releaseManifestSha256",
    )
    source_digest = _normalize_sha256(
        document["sourceManifestSha256"],
        "wrapper manifest.sourceManifestSha256",
    )
    if source_digest != package_digest:
        raise EvidenceError("wrapper manifest: source/package manifest binding mismatch")
    rollback_requested = document["rollbackRequested"]
    rollback_exercised = document["rollbackExercised"]
    if not isinstance(rollback_requested, bool) or not isinstance(
        rollback_exercised, bool
    ):
        raise EvidenceError("wrapper manifest: rollback flags must be booleans")
    if rollback_exercised != rollback_requested:
        raise EvidenceError("wrapper manifest: requested rollback must be exercised")
    expected_steps = list(BASE_STEPS)
    if rollback_requested:
        expected_steps.append("release-rollback")
    if document["completedSteps"] != expected_steps:
        raise EvidenceError("wrapper manifest: completed step sequence mismatch")
    if document["failure"] is not None:
        raise EvidenceError("wrapper manifest: successful bundle cannot contain failure")
    return (
        release_id,
        package_digest,
        release_digest,
        rollback_requested,
        rollback_exercised,
    )


def _required_evidence_files(rollback_requested: bool) -> set[str]:
    required_files = {
        ".wrapper-owner",
        "host-inputs/.collector-owner",
        "host-collector.stdout.json",
        "host-collector.stderr.log",
        "host-collector.stdout.json.command.json",
        "preflight/p8-preflight-report.json",
        "preflight-after-soak/p8-preflight-report.json",
        "soak/soak-manifest.json",
        "preflight.stdout.log",
        "preflight.stderr.log",
        "preflight.stdout.log.command.json",
        "preflight-after-soak.stdout.log",
        "preflight-after-soak.stderr.log",
        "preflight-after-soak.stdout.log.command.json",
        "host-inputs/windows.json",
        "host-inputs/gpu.json",
        "soak.stdout.log",
        "soak.stderr.log",
        "soak.stdout.log.command.json",
        "release/package.stdout.json",
        "release/package.stderr.log",
        "release/package.stdout.json.command.json",
        "release/verify.stdout.json",
        "release/verify.stderr.log",
        "release/verify.stdout.json.command.json",
        "release/activate.stdout.json",
        "release/activate.stderr.log",
        "release/activate.stdout.json.command.json",
    }
    if rollback_requested:
        required_files.update(
            {
                "release/rollback.stdout.json",
                "release/rollback.stderr.log",
                "release/rollback.stdout.json.command.json",
            }
        )
    return required_files


def _validate_snapshot_semantics(
    snapshot: dict[str, bytes],
    document: dict[str, Any],
    release_id: str,
    package_digest: str,
    release_digest: str,
    rollback_requested: bool,
) -> tuple[
    dict[str, tuple[int, str]],
    dict[str, Any],
    list[dict[str, Any]],
]:
    declared = _validate_file_manifest(document["files"], snapshot)
    source_version = _validate_source_version(document["sourceVersion"])
    provenance = _validate_provenance(document["provenanceFiles"], declared)
    _validate_ownership_markers(
        snapshot,
        document["hostReportChallenge"],
    )
    required_files = _required_evidence_files(rollback_requested)
    missing_required = sorted(required_files - set(declared))
    if missing_required:
        raise EvidenceError(
            f"wrapper manifest: required evidence files missing: {missing_required}"
        )

    wrapper_started = _parse_time(
        document["startedAt"],
        "wrapper manifest.startedAt",
    )
    wrapper_finished = _parse_time(
        document["finishedAt"],
        "wrapper manifest.finishedAt",
    )
    preflight_identities = _validate_preflight(
        _load_child(snapshot, "preflight/p8-preflight-report.json"),
        package_digest,
        document["hostReportChallenge"],
        wrapper_started,
        wrapper_finished,
        "preflight",
    )
    post_soak_identities = _validate_preflight(
        _load_child(snapshot, "preflight-after-soak/p8-preflight-report.json"),
        package_digest,
        document["hostReportChallenge"],
        wrapper_started,
        wrapper_finished,
        "preflight-after-soak",
    )
    if preflight_identities != post_soak_identities:
        raise EvidenceError("preflight external/gate identities drifted after soak")
    for identity_name, relative in (
        ("external.windows", "host-inputs/windows.json"),
        ("external.gpu", "host-inputs/gpu.json"),
    ):
        if _sha256(snapshot[relative]) != preflight_identities[identity_name]:
            raise EvidenceError(
                f"preflight {identity_name} identity does not match copied input"
            )
    gate_provenance = next(
        item for item in provenance if item["role"] == "gate-config"
    )
    if preflight_identities["gate.config"] != gate_provenance["sha256"]:
        raise EvidenceError(
            "preflight gate config identity does not match provenance"
        )
    collector_provenance = next(
        item for item in provenance
        if item["role"] == "host-report-collector"
    )
    host_report_digests, host_report_identity = _validate_host_report_inputs(
        snapshot,
        collector_provenance["sha256"],
        document["hostReportChallenge"],
        package_digest,
    )
    captured_at = _parse_time(
        host_report_identity["capturedAtUtc"],
        "host reports capturedAtUtc",
    )
    if not wrapper_started <= captured_at <= wrapper_finished:
        raise EvidenceError(
            "host reports were not captured during this wrapper run"
        )
    _validate_host_collector_summary(
        snapshot,
        document,
        host_report_digests,
        host_report_identity,
    )
    if (
        preflight_identities["host-report-collector"]
        != collector_provenance["sha256"]
    ):
        raise EvidenceError(
            "preflight host report collector identity does not match provenance"
        )
    for identity_name, observed_digest in host_report_digests.items():
        if preflight_identities[identity_name] != observed_digest:
            raise EvidenceError(
                f"preflight {identity_name} identity does not match the "
                "independently validated host report"
            )
    soak_document = _load_child(snapshot, "soak/soak-manifest.json")
    _validate_release_outputs(
        snapshot,
        document["deploymentRoot"],
        release_id,
        release_digest,
        package_digest,
        rollback_requested,
    )
    _validate_command_transcripts(
        snapshot,
        document,
        soak_document,
        package_digest,
        release_digest,
        rollback_requested,
    )
    return declared, source_version, provenance


def verify_evidence(
    evidence_root: str | os.PathLike[str] | Path,
    manifest_sha256: str,
    manifest_hmac_sha256: str,
    evidence_key: bytes | str | os.PathLike[str] | Path,
) -> dict[str, Any]:
    root = Path(os.path.abspath(os.fspath(evidence_root)))
    _require_no_reparse_ancestors(root, "evidence root")
    _validate_evidence_root(root)
    if not isinstance(evidence_key, bytes):
        evidence_key_path = Path(
            os.path.abspath(os.fspath(evidence_key))
        )
        _require_no_reparse_ancestors(
            evidence_key_path,
            "external evidence HMAC key",
        )
        if _paths_overlap(root, evidence_key_path):
            raise EvidenceError(
                "external evidence HMAC key must be outside the evidence bundle"
            )
    supplied_digest = _normalize_sha256(
        manifest_sha256, "wrapper manifest SHA-256"
    )
    manifest_path = root / MANIFEST_NAME
    manifest_raw = _regular_bytes(manifest_path, "wrapper manifest")
    actual_digest = _sha256(manifest_raw)
    if actual_digest != supplied_digest:
        raise EvidenceError(
            "wrapper manifest does not match the externally supplied SHA-256"
        )
    supplied_hmac = _normalize_sha256(
        manifest_hmac_sha256,
        "wrapper manifest HMAC-SHA-256",
    )
    actual_hmac = hmac.new(
        _evidence_key_bytes(evidence_key),
        manifest_raw,
        hashlib.sha256,
    ).hexdigest()
    if not hmac.compare_digest(actual_hmac, supplied_hmac):
        raise EvidenceError(
            "wrapper manifest does not match the external HMAC trust anchor"
        )
    document = _exact_object(
        _load_json_bytes(manifest_raw, "wrapper manifest"),
        MANIFEST_KEYS,
        "wrapper manifest",
    )
    (
        release_id,
        package_digest,
        release_digest,
        rollback_requested,
        rollback_exercised,
    ) = _validate_wrapper_document(document)
    snapshot = _snapshot_files(root)
    declared, source_version, provenance = _validate_snapshot_semantics(
        snapshot,
        document,
        release_id,
        package_digest,
        release_digest,
        rollback_requested,
    )
    if _regular_bytes(manifest_path, "wrapper manifest") != manifest_raw:
        raise EvidenceError("wrapper manifest changed during verification")
    final_snapshot = _snapshot_files(root)
    if _snapshot_records(final_snapshot) != declared:
        raise EvidenceError("evidence tree changed during verification")
    return {
        "schemaVersion": SCHEMA_VERSION,
        "verified": True,
        "overallResult": "passed-local-tooling",
        "productAcceptanceClaimed": False,
        "manifestSha256": actual_digest,
        "manifestHmacSha256": actual_hmac,
        "packageManifestSha256": package_digest,
        "releaseManifestSha256": release_digest,
        "releaseId": release_id,
        "rollbackExercised": rollback_exercised,
        "fileCount": len(declared),
        "sourceVersion": source_version,
        "provenanceFiles": provenance,
    }


def _ensure_real_directory(path: Path) -> None:
    if path.parent != path:
        _ensure_real_directory(path.parent)
    if not path.exists():
        try:
            path.mkdir()
        except OSError as exc:
            raise EvidenceError(f"cannot create import directory {path}: {exc}") from exc
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise EvidenceError(f"cannot inspect import directory {path}: {exc}") from exc
    if (
        stat.S_ISLNK(metadata.st_mode)
        or _is_reparse_point(metadata)
        or not stat.S_ISDIR(metadata.st_mode)
    ):
        raise EvidenceError(f"import path must be a real directory: {path}")


def _require_no_reparse_ancestors(path: Path, label: str) -> None:
    current = path
    while True:
        if current.exists() or current.is_symlink():
            try:
                metadata = current.lstat()
            except OSError as exc:
                raise EvidenceError(f"{label}: cannot inspect {current}: {exc}") from exc
            if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                raise EvidenceError(
                    f"{label}: symbolic-link/reparse-point ancestor is forbidden: "
                    f"{current}"
                )
        if current.parent == current:
            return
        current = current.parent


def _paths_overlap(first: Path, second: Path) -> bool:
    first_resolved = first.resolve(strict=False)
    second_resolved = second.resolve(strict=False)
    return (
        first_resolved == second_resolved
        or first_resolved in second_resolved.parents
        or second_resolved in first_resolved.parents
    )


def _copy_bundle(source: Path, destination: Path) -> None:
    for current, directory_names, file_names in os.walk(source, followlinks=False):
        current_path = Path(current)
        relative_directory = current_path.relative_to(source)
        destination_directory = destination / relative_directory
        destination_directory.mkdir(exist_ok=True)
        for name in directory_names:
            candidate = current_path / name
            metadata = candidate.lstat()
            if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                raise EvidenceError(
                    "source changed during import: symbolic-link/reparse directory"
                )
            if not stat.S_ISDIR(metadata.st_mode):
                raise EvidenceError(
                    "source changed during import: non-directory tree entry"
                )
            (destination_directory / name).mkdir(exist_ok=True)
        for name in file_names:
            candidate = current_path / name
            metadata = candidate.lstat()
            if (
                stat.S_ISLNK(metadata.st_mode)
                or _is_reparse_point(metadata)
                or not stat.S_ISREG(metadata.st_mode)
            ):
                raise EvidenceError(
                    "source changed during import: non-regular file"
                )
            try:
                shutil.copy2(
                    candidate,
                    destination_directory / name,
                    follow_symlinks=False,
                )
            except OSError as exc:
                raise EvidenceError(f"cannot copy evidence file {candidate}: {exc}") from exc


def _write_new_json(path: Path, value: object) -> None:
    raw = (
        json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    ).encode("utf-8")
    descriptor = -1
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "wb", closefd=True) as stream:
            descriptor = -1
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
    except OSError as exc:
        raise EvidenceError(f"cannot write import receipt {path}: {exc}") from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _fsync_directory(path: Path) -> None:
    flags = os.O_RDONLY | getattr(os, "O_DIRECTORY", 0)
    descriptor = -1
    try:
        descriptor = os.open(path, flags)
        os.fsync(descriptor)
    except OSError as exc:
        if os.name != "nt":
            raise EvidenceError(f"cannot fsync import directory {path}: {exc}") from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def _seal_tree(root: Path) -> None:
    files: list[Path] = []
    directories: list[Path] = []
    for current, directory_names, file_names in os.walk(root, followlinks=False):
        current_path = Path(current)
        directories.append(current_path)
        files.extend(current_path / name for name in file_names)
        directories.extend(current_path / name for name in directory_names)
    for path in files:
        try:
            path.chmod(0o444)
        except OSError as exc:
            raise EvidenceError(f"cannot seal imported file {path}: {exc}") from exc
    for path in sorted(set(directories), key=lambda item: len(item.parts), reverse=True):
        try:
            path.chmod(0o555)
        except OSError as exc:
            raise EvidenceError(f"cannot seal imported directory {path}: {exc}") from exc


def _remove_staging(path: Path) -> None:
    if not path.exists() and not path.is_symlink():
        return
    for current, directory_names, file_names in os.walk(path, topdown=False):
        current_path = Path(current)
        try:
            current_path.chmod(0o755)
        except OSError:
            pass
        for name in file_names:
            candidate = current_path / name
            try:
                candidate.chmod(0o644)
            except OSError:
                pass
        for name in directory_names:
            candidate = current_path / name
            try:
                candidate.chmod(0o755)
            except OSError:
                pass
    shutil.rmtree(path)


def _read_receipt(
    path: Path,
    expected_digest: str,
    verification: dict[str, Any],
) -> dict[str, Any]:
    value = _load_json_bytes(_regular_bytes(path, "import receipt"), "import receipt")
    receipt = _exact_object(
        value,
        {
            "schemaVersion",
            "verifierSchemaVersion",
            "verifierSha256",
            "sourceManifestSha256",
            "manifestHmacSha256",
            "bundlePath",
            "scope",
            "overallResult",
            "productAcceptanceClaimed",
            "releaseId",
            "packageManifestSha256",
            "releaseManifestSha256",
            "sourceVersion",
            "provenanceFiles",
            "fileCount",
        },
        "import receipt",
    )
    if (
        receipt["schemaVersion"] != RECEIPT_SCHEMA_VERSION
        or receipt["verifierSchemaVersion"] != SCHEMA_VERSION
        or receipt["verifierSha256"] != _current_verifier_sha256()
        or receipt["sourceManifestSha256"] != expected_digest
        or receipt["manifestHmacSha256"]
        != verification["manifestHmacSha256"]
        or receipt["bundlePath"] != "bundle"
        or receipt["scope"]
        != "portable-windows-local-tooling-evidence-only"
        or receipt["overallResult"] != verification["overallResult"]
        or receipt["productAcceptanceClaimed"] is not False
        or receipt["releaseId"] != verification["releaseId"]
        or receipt["packageManifestSha256"]
        != verification["packageManifestSha256"]
        or receipt["releaseManifestSha256"]
        != verification["releaseManifestSha256"]
        or receipt["sourceVersion"] != verification["sourceVersion"]
        or receipt["provenanceFiles"] != verification["provenanceFiles"]
        or receipt["fileCount"] != verification["fileCount"]
    ):
        raise EvidenceError("existing import receipt identity/scope mismatch")
    return receipt


def _case_safe_import_target(imports: Path, digest: str) -> Path:
    target = imports / digest
    target_identity = _collision_key(digest)
    try:
        entries = list(imports.iterdir())
    except OSError as exc:
        raise EvidenceError(f"cannot enumerate import store {imports}: {exc}") from exc
    for entry in entries:
        identity = _collision_key(entry.name)
        if identity == target_identity and entry.name != digest:
            raise EvidenceError(
                f"casefold/NFC-conflicting import target: {entry.name!r}"
            )
    return target


def _validate_published_import_root(target: Path) -> None:
    try:
        entries = {entry.name: entry for entry in target.iterdir()}
    except OSError as exc:
        raise EvidenceError(
            f"cannot enumerate published import root {target}: {exc}"
        ) from exc
    if set(entries) != {"bundle", "receipt.json"}:
        raise EvidenceError(
            "published import root must contain exactly bundle/ and receipt.json"
        )
    try:
        bundle_metadata = entries["bundle"].lstat()
        receipt_metadata = entries["receipt.json"].lstat()
    except OSError as exc:
        raise EvidenceError(
            f"cannot inspect published import root {target}: {exc}"
        ) from exc
    if (
        stat.S_ISLNK(bundle_metadata.st_mode)
        or _is_reparse_point(bundle_metadata)
        or not stat.S_ISDIR(bundle_metadata.st_mode)
        or stat.S_ISLNK(receipt_metadata.st_mode)
        or _is_reparse_point(receipt_metadata)
        or not stat.S_ISREG(receipt_metadata.st_mode)
    ):
        raise EvidenceError(
            "published import root requires a real bundle directory and receipt file"
        )


def import_evidence(
    evidence_root: str | os.PathLike[str] | Path,
    manifest_sha256: str,
    manifest_hmac_sha256: str,
    evidence_key: bytes | str | os.PathLike[str] | Path,
    store_root: str | os.PathLike[str] | Path,
) -> dict[str, Any]:
    source = Path(os.path.abspath(os.fspath(evidence_root)))
    supplied_digest = _normalize_sha256(
        manifest_sha256, "wrapper manifest SHA-256"
    )
    store_supplied = Path(os.path.abspath(os.fspath(store_root)))
    _require_no_reparse_ancestors(source, "evidence source")
    _require_no_reparse_ancestors(store_supplied, "import store")
    store = store_supplied.resolve(strict=False)
    if _paths_overlap(source, store):
        raise EvidenceError("evidence source and import store must not overlap")
    _ensure_real_directory(store)
    imports = store / "imports"
    _ensure_real_directory(imports)
    target = _case_safe_import_target(imports, supplied_digest)
    if target.exists() or target.is_symlink():
        _ensure_real_directory(target)
        _validate_published_import_root(target)
        verification = verify_evidence(
            target / "bundle",
            supplied_digest,
            manifest_hmac_sha256,
            evidence_key,
        )
        receipt = _read_receipt(
            target / "receipt.json", supplied_digest, verification
        )
        _validate_published_import_root(target)
        return {
            "imported": True,
            "idempotent": True,
            "importRoot": str(target),
            "receipt": receipt,
            "verification": verification,
        }

    verification = verify_evidence(
        source,
        supplied_digest,
        manifest_hmac_sha256,
        evidence_key,
    )
    staging = Path(
        tempfile.mkdtemp(
            prefix=f".{supplied_digest}.staging-",
            dir=imports,
        )
    )
    try:
        bundle = staging / "bundle"
        bundle.mkdir()
        _copy_bundle(source, bundle)
        copied_verification = verify_evidence(
            bundle,
            supplied_digest,
            manifest_hmac_sha256,
            evidence_key,
        )
        if copied_verification != verification:
            raise EvidenceError("source evidence changed while being imported")
        receipt = {
            "schemaVersion": RECEIPT_SCHEMA_VERSION,
            "verifierSchemaVersion": SCHEMA_VERSION,
            "verifierSha256": _current_verifier_sha256(),
            "sourceManifestSha256": supplied_digest,
            "manifestHmacSha256": copied_verification[
                "manifestHmacSha256"
            ],
            "bundlePath": "bundle",
            "scope": "portable-windows-local-tooling-evidence-only",
            "overallResult": copied_verification["overallResult"],
            "productAcceptanceClaimed": False,
            "releaseId": copied_verification["releaseId"],
            "packageManifestSha256": copied_verification[
                "packageManifestSha256"
            ],
            "releaseManifestSha256": copied_verification[
                "releaseManifestSha256"
            ],
            "sourceVersion": copied_verification["sourceVersion"],
            "provenanceFiles": copied_verification["provenanceFiles"],
            "fileCount": copied_verification["fileCount"],
        }
        _write_new_json(staging / "receipt.json", receipt)
        _seal_tree(staging)
        try:
            os.replace(staging, target)
        except OSError as exc:
            if target.is_dir() and not target.is_symlink():
                _remove_staging(staging)
                _validate_published_import_root(target)
                verification = verify_evidence(
                    target / "bundle",
                    supplied_digest,
                    manifest_hmac_sha256,
                    evidence_key,
                )
                existing_receipt = _read_receipt(
                    target / "receipt.json", supplied_digest, verification
                )
                _validate_published_import_root(target)
                return {
                    "imported": True,
                    "idempotent": True,
                    "importRoot": str(target),
                    "receipt": existing_receipt,
                    "verification": verification,
                }
            raise EvidenceError(f"cannot atomically publish evidence import: {exc}") from exc
        _fsync_directory(imports)
        return {
            "imported": True,
            "idempotent": False,
            "importRoot": str(target),
            "receipt": receipt,
            "verification": copied_verification,
        }
    finally:
        if staging.exists() or staging.is_symlink():
            _remove_staging(staging)


class StrictArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_usage(sys.stderr)
        self.exit(3, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = StrictArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    for name in ("verify", "import"):
        command = subparsers.add_parser(name)
        command.add_argument("--evidence-root", required=True, type=Path)
        command.add_argument("--manifest-sha256", required=True)
        command.add_argument("--manifest-hmac-sha256", required=True)
        command.add_argument("--evidence-key", required=True, type=Path)
        if name == "import":
            command.add_argument("--store-root", required=True, type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        if arguments.command == "verify":
            result = verify_evidence(
                arguments.evidence_root,
                arguments.manifest_sha256,
                arguments.manifest_hmac_sha256,
                arguments.evidence_key,
            )
        else:
            result = import_evidence(
                arguments.evidence_root,
                arguments.manifest_sha256,
                arguments.manifest_hmac_sha256,
                arguments.evidence_key,
                arguments.store_root,
            )
    except EvidenceError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 3
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
