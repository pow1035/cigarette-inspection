#!/usr/bin/env python3
"""Strict, local-only P8 package preflight.

Exit codes:
  0: the local preflight is ready;
  2: a required external Windows/GPU input is absent;
  3: an input is invalid, unsafe, or does not match its declared identity.

This tool does not execute the product, Windows, a GPU, hardware IO, or reject
outputs. A ready report is local tooling evidence, not P8 product acceptance.
"""

from __future__ import annotations

import argparse
import configparser
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tempfile
import unicodedata
from typing import Any, Iterable


EXIT_READY = 0
EXIT_EXTERNAL_MISSING = 2
EXIT_INVALID = 3

DEFAULT_REPORT_NAME = "p8-preflight-report.json"
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
WINDOWS_DRIVE_RE = re.compile(r"^[A-Za-z]:")

GATE_KEYS = {
    "schemaVersion",
    "localOnly",
    "realIoEnabled",
    "realRejectEnabled",
    "packageManifestSchemaVersion",
    "configIniPath",
    "brandRootPath",
    "brandParameterFileName",
    "brandTemplateDirectoryName",
    "reportFileName",
}
MANIFEST_KEYS = {"schemaVersion", "files"}
MANIFEST_FILE_KEYS = {"path", "size", "sha256"}
HOST_REPORT_KEYS = {
    "schemaVersion",
    "captureId",
    "capturedAtUtc",
    "hostIdSha256",
    "challenge",
    "packageManifestSha256",
    "collector",
    "checks",
    "claims",
}
HOST_COLLECTOR_KEYS = {
    "schemaVersion",
    "repositoryPath",
    "sha256",
}
HOST_CHECK_KEYS = {"id", "status", "detail"}
HOST_CLAIM_KEYS = {
    "collectionComplete",
    "productAcceptance",
    "windowsRuntimeAccepted",
    "gpuRuntimeAccepted",
    "realIoTested",
    "realRejectTested",
}
HOST_REPORT_SCHEMAS = {
    "windows": "p8-windows-host-report-v2",
    "gpu": "p8-gpu-host-report-v2",
}
HOST_REQUIRED_CHECKS = {
    "windows": {
        "host.windows",
        "host.architecture-x64",
        "storage.d-drive",
        "tool.powershell",
        "tool.python",
        "tool.git",
        "tool.msbuild",
        "dependency.qt-5.9.9",
        "dependency.halcon-release",
        "dependency.mvs",
        "dependency.daqnavi",
        "safety.reject-disabled",
    },
    "gpu": {
        "gpu.nvidia-smi",
        "gpu.device",
        "gpu.driver",
        "dependency.cuda",
        "dependency.tensorrt",
        "dependency.opencv",
    },
}
HOST_CAPTURE_ID_RE = re.compile(r"^[0-9a-f]{32}$")
HOST_COLLECTOR_SCHEMA = "cigvision-p8-host-collector-v1"
HOST_COLLECTOR_REPOSITORY_PATH = (
    "scripts/collect_windows_p8_host_reports.ps1"
)
HOST_COLLECTOR_FILE_NAME = "collect_windows_p8_host_reports.ps1"


class StrictInputError(ValueError):
    """Raised when a serialized or filesystem input is not safe and canonical."""


class DuplicateKeyError(StrictInputError):
    """Raised when any JSON object repeats a key."""


class CheckLog:
    def __init__(self) -> None:
        self.checks: list[dict[str, Any]] = []
        self.invalid = False
        self.external_missing: list[str] = []

    def ok(self, name: str, message: str, **details: Any) -> None:
        self.checks.append(
            {"name": name, "status": "ok", "message": message, **details}
        )

    def fail(self, name: str, message: str, **details: Any) -> None:
        self.invalid = True
        self.checks.append(
            {"name": name, "status": "invalid", "message": message, **details}
        )

    def missing(self, name: str, message: str, **details: Any) -> None:
        self.external_missing.append(name)
        self.checks.append(
            {"name": name, "status": "missing", "message": message, **details}
        )


def _absolute(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def _resolved(path: Path) -> Path:
    return path.resolve(strict=False)


def _is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def _paths_overlap_directory(directory: Path, other: Path, other_is_dir: bool) -> bool:
    directory_resolved = _resolved(directory)
    other_resolved = _resolved(other)
    if directory_resolved == other_resolved:
        return True
    if _is_relative_to(other_resolved, directory_resolved):
        return True
    return other_is_dir and _is_relative_to(directory_resolved, other_resolved)


def _reject_json_constant(token: str) -> Any:
    raise StrictInputError(f"non-finite JSON number is forbidden: {token}")


def _parse_json_float(token: str) -> float:
    value = float(token)
    if not math.isfinite(value):
        raise StrictInputError(f"non-finite JSON number is forbidden: {token}")
    return value


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise DuplicateKeyError(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def load_strict_json_bytes(raw: bytes, label: str) -> Any:
    try:
        text = raw.decode("utf-8-sig", errors="strict")
    except UnicodeDecodeError as exc:
        raise StrictInputError(f"{label} is not valid UTF-8: {exc}") from exc
    try:
        return json.loads(
            text,
            object_pairs_hook=_unique_object,
            parse_constant=_reject_json_constant,
            parse_float=_parse_json_float,
        )
    except (json.JSONDecodeError, StrictInputError) as exc:
        raise StrictInputError(f"{label} is not strict JSON: {exc}") from exc


def _lstat(path: Path, label: str) -> os.stat_result:
    try:
        return path.lstat()
    except OSError as exc:
        raise StrictInputError(f"{label} cannot be inspected: {exc}") from exc


def _is_reparse_point(metadata: os.stat_result) -> bool:
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & reparse_flag)


def _require_no_windows_reparse_ancestors(path: Path, label: str) -> None:
    if os.name != "nt":
        return
    current = _absolute(path)
    while True:
        try:
            metadata = current.lstat()
        except FileNotFoundError:
            pass
        except OSError as exc:
            raise StrictInputError(
                f"{label} ancestor cannot be inspected: {exc}") from exc
        else:
            if _is_reparse_point(metadata):
                raise StrictInputError(
                    f"{label} contains a Windows reparse-point ancestor: {current}")
        if current == current.parent:
            return
        current = current.parent


def _require_regular_non_symlink(path: Path, label: str) -> os.stat_result:
    _require_no_windows_reparse_ancestors(path, label)
    metadata = _lstat(path, label)
    if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
        raise StrictInputError(f"{label} must not be a symbolic link or reparse point")
    if not stat.S_ISREG(metadata.st_mode):
        raise StrictInputError(f"{label} must be a regular file")
    return metadata


def _require_directory_non_symlink(path: Path, label: str) -> os.stat_result:
    _require_no_windows_reparse_ancestors(path, label)
    metadata = _lstat(path, label)
    if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
        raise StrictInputError(f"{label} must not be a symbolic link or reparse point")
    if not stat.S_ISDIR(metadata.st_mode):
        raise StrictInputError(f"{label} must be a directory")
    return metadata


def _read_regular_file(path: Path, label: str) -> bytes:
    _require_regular_non_symlink(path, label)
    try:
        with path.open("rb") as stream:
            opened = os.fstat(stream.fileno())
            if not stat.S_ISREG(opened.st_mode):
                raise StrictInputError(f"{label} changed and is not a regular file")
            return stream.read()
    except OSError as exc:
        raise StrictInputError(f"{label} cannot be read: {exc}") from exc


def _sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _sha256_file(path: Path, label: str) -> tuple[int, str]:
    _require_regular_non_symlink(path, label)
    digest = hashlib.sha256()
    size = 0
    try:
        with path.open("rb") as stream:
            opened = os.fstat(stream.fileno())
            if not stat.S_ISREG(opened.st_mode):
                raise StrictInputError(f"{label} changed and is not a regular file")
            while True:
                block = stream.read(1024 * 1024)
                if not block:
                    break
                size += len(block)
                digest.update(block)
    except OSError as exc:
        raise StrictInputError(f"{label} cannot be hashed: {exc}") from exc
    return size, digest.hexdigest()


def canonical_relative_path(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise StrictInputError(f"{label} must be a non-empty string")
    if "\x00" in value:
        raise StrictInputError(f"{label} contains NUL")
    if value != unicodedata.normalize("NFC", value):
        raise StrictInputError(f"{label} must use Unicode NFC")
    if "\\" in value:
        raise StrictInputError(f"{label} must use '/' separators")
    if value.startswith("/") or value.startswith("//") or WINDOWS_DRIVE_RE.match(value):
        raise StrictInputError(f"{label} must be relative")
    if value.endswith("/") or "//" in value:
        raise StrictInputError(f"{label} is not canonical")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise StrictInputError(f"{label} contains an empty, '.', or '..' component")
    path = PurePosixPath(value)
    if path.is_absolute() or path.as_posix() != value:
        raise StrictInputError(f"{label} is not a canonical relative path")
    return value


def canonical_component(value: Any, label: str) -> str:
    result = canonical_relative_path(value, label)
    if "/" in result:
        raise StrictInputError(f"{label} must be one path component")
    return result


def _collision_key(path: str) -> str:
    return unicodedata.normalize("NFC", path).casefold()


def _path_has_symlink_component(package: Path, relative: str) -> bool:
    current = package
    for part in PurePosixPath(relative).parts:
        current = current / part
        try:
            metadata = current.lstat()
            if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                return True
        except OSError:
            return False
    return False


def _strict_object(
    value: Any, required_keys: set[str], label: str
) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise StrictInputError(f"{label} must be an object")
    actual = set(value)
    if actual != required_keys:
        missing = sorted(required_keys - actual)
        unknown = sorted(actual - required_keys)
        raise StrictInputError(
            f"{label} keys mismatch; missing={missing}, unknown={unknown}"
        )
    return value


def _nonempty_text(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise StrictInputError(f"{label} must be a non-empty string")
    if "\x00" in value:
        raise StrictInputError(f"{label} contains NUL")
    return value


def _utc_timestamp(value: Any, label: str) -> str:
    text = _nonempty_text(value, label)
    normalized = text
    seven_digit_fraction = re.search(
        r"(\.\d{6})\d(?=(?:Z|[+-]\d{2}:\d{2})$)",
        text,
    )
    if seven_digit_fraction is not None:
        normalized = (
            text[:seven_digit_fraction.end(1)]
            + text[seven_digit_fraction.end():]
        )
    try:
        parsed = dt.datetime.fromisoformat(normalized.replace("Z", "+00:00"))
    except ValueError as exc:
        raise StrictInputError(f"{label} must be an ISO-8601 timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() != dt.timedelta(0):
        raise StrictInputError(f"{label} must include the UTC offset")
    return text


def validate_host_report_document(
    document: Any,
    kind: str,
    collector_sha256: str,
    expected_challenge: str,
    expected_package_manifest_sha256: str,
) -> dict[str, str]:
    if kind not in HOST_REPORT_SCHEMAS:
        raise StrictInputError(f"unknown host report kind: {kind!r}")
    label = f"external {kind} host report"
    report = _strict_object(document, HOST_REPORT_KEYS, label)
    if report["schemaVersion"] != HOST_REPORT_SCHEMAS[kind]:
        raise StrictInputError(
            f"{label} schemaVersion must be {HOST_REPORT_SCHEMAS[kind]!r}"
        )
    capture_id = report["captureId"]
    if (
        not isinstance(capture_id, str)
        or not HOST_CAPTURE_ID_RE.fullmatch(capture_id)
    ):
        raise StrictInputError(
            f"{label} captureId must be exactly 32 lowercase hex digits"
        )
    captured_at = _utc_timestamp(report["capturedAtUtc"], f"{label}.capturedAtUtc")
    host_id = report["hostIdSha256"]
    if not isinstance(host_id, str) or not SHA256_RE.fullmatch(host_id):
        raise StrictInputError(
            f"{label} hostIdSha256 must be exactly 64 hex digits"
        )
    if host_id != host_id.lower():
        raise StrictInputError(f"{label} hostIdSha256 must use lowercase hex")
    challenge = report["challenge"]
    if (
        not isinstance(challenge, str)
        or not re.fullmatch(r"[0-9a-f]{64}", challenge)
    ):
        raise StrictInputError(
            f"{label} challenge must be exactly 64 lowercase hex digits"
        )
    if challenge != expected_challenge:
        raise StrictInputError(f"{label} challenge does not match this run")
    package_manifest_sha256 = report["packageManifestSha256"]
    if (
        not isinstance(package_manifest_sha256, str)
        or not re.fullmatch(r"[0-9a-f]{64}", package_manifest_sha256)
    ):
        raise StrictInputError(
            f"{label} packageManifestSha256 must be 64 lowercase hex digits"
        )
    if package_manifest_sha256 != expected_package_manifest_sha256:
        raise StrictInputError(
            f"{label} packageManifestSha256 does not match this package"
        )

    collector = _strict_object(
        report["collector"], HOST_COLLECTOR_KEYS, f"{label}.collector"
    )
    if collector["schemaVersion"] != HOST_COLLECTOR_SCHEMA:
        raise StrictInputError(f"{label} collector schemaVersion mismatch")
    if collector["repositoryPath"] != HOST_COLLECTOR_REPOSITORY_PATH:
        raise StrictInputError(f"{label} collector repositoryPath mismatch")
    reported_collector_sha = collector["sha256"]
    if (
        not isinstance(reported_collector_sha, str)
        or not SHA256_RE.fullmatch(reported_collector_sha)
        or reported_collector_sha != reported_collector_sha.lower()
    ):
        raise StrictInputError(
            f"{label} collector sha256 must be 64 lowercase hex digits"
        )
    if reported_collector_sha != collector_sha256:
        raise StrictInputError(
            f"{label} collector sha256 does not match the trusted collector"
        )

    checks = report["checks"]
    if not isinstance(checks, list):
        raise StrictInputError(f"{label}.checks must be an array")
    by_id: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(checks):
        check = _strict_object(
            value, HOST_CHECK_KEYS, f"{label}.checks[{index}]"
        )
        check_id = _nonempty_text(check["id"], f"{label}.checks[{index}].id")
        if check_id in by_id:
            raise StrictInputError(f"{label}.checks repeats id {check_id!r}")
        if check["status"] not in {"passed", "failed"}:
            raise StrictInputError(
                f"{label}.checks[{index}].status must be 'passed' or 'failed'"
            )
        _nonempty_text(check["detail"], f"{label}.checks[{index}].detail")
        by_id[check_id] = check
    required = HOST_REQUIRED_CHECKS[kind]
    if set(by_id) != required:
        raise StrictInputError(
            f"{label}.checks required IDs mismatch; "
            f"missing={sorted(required - set(by_id))}, "
            f"unknown={sorted(set(by_id) - required)}"
        )

    claims = _strict_object(
        report["claims"], HOST_CLAIM_KEYS, f"{label}.claims"
    )
    failed_checks = sorted(
        check_id
        for check_id, check in by_id.items()
        if check["status"] != "passed"
    )
    expected_complete = not failed_checks
    if claims["collectionComplete"] is not expected_complete:
        raise StrictInputError(
            f"{label} collectionComplete does not match required check status"
        )
    for key in (
        "productAcceptance",
        "windowsRuntimeAccepted",
        "gpuRuntimeAccepted",
        "realIoTested",
        "realRejectTested",
    ):
        if claims[key] is not False:
            raise StrictInputError(f"{label} claims.{key} must be false")
    if failed_checks:
        raise StrictInputError(
            f"{label} has failed required checks: {failed_checks}"
        )

    return {
        "captureId": capture_id,
        "capturedAtUtc": captured_at,
        "hostIdSha256": host_id,
        "challenge": challenge,
        "packageManifestSha256": package_manifest_sha256,
        "collectorSha256": reported_collector_sha,
    }


def validate_gate_document(document: Any) -> dict[str, Any]:
    gate = _strict_object(document, GATE_KEYS, "gate config")
    if gate["schemaVersion"] != "p8-local-preflight-v1":
        raise StrictInputError(
            "gate config schemaVersion must be p8-local-preflight-v1"
        )
    if gate["localOnly"] is not True:
        raise StrictInputError("gate config localOnly must be true")
    if gate["realIoEnabled"] is not False:
        raise StrictInputError("gate config realIoEnabled must be false")
    if gate["realRejectEnabled"] is not False:
        raise StrictInputError("gate config realRejectEnabled must be false")
    fixed_values = {
        "packageManifestSchemaVersion": "p8-package-manifest-v1",
        "configIniPath": "config.ini",
        "brandRootPath": "品牌设置",
        "brandParameterFileName": "para.ini",
        "brandTemplateDirectoryName": "模版图片",
        "reportFileName": DEFAULT_REPORT_NAME,
    }
    for key, expected in fixed_values.items():
        if gate[key] != expected:
            raise StrictInputError(
                f"gate config {key} must be exactly {expected!r}"
            )
    for key in (
        "packageManifestSchemaVersion",
        "reportFileName",
    ):
        canonical_component(gate[key], f"gate config {key}")
    for key in ("configIniPath", "brandRootPath"):
        canonical_relative_path(gate[key], f"gate config {key}")
    for key in ("brandParameterFileName", "brandTemplateDirectoryName"):
        canonical_component(gate[key], f"gate config {key}")
    if not gate["reportFileName"].endswith(".json"):
        raise StrictInputError("gate config reportFileName must end in .json")
    return gate


def _decode_ini(raw: bytes, label: str) -> str:
    try:
        return raw.decode("utf-8-sig", errors="strict")
    except UnicodeDecodeError as exc:
        raise StrictInputError(f"{label} is not valid UTF-8: {exc}") from exc


def _load_strict_ini(raw: bytes, label: str) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(
        interpolation=None,
        strict=True,
        empty_lines_in_values=False,
    )
    parser.optionxform = str
    try:
        parser.read_string(_decode_ini(raw, label), source=label)
    except configparser.Error as exc:
        raise StrictInputError(f"{label} is not strict INI: {exc}") from exc
    if parser.defaults():
        raise StrictInputError(f"{label} must not define DEFAULT values")

    section_keys: dict[str, str] = {}
    for section in parser.sections():
        canonical = _collision_key(unicodedata.normalize("NFC", section))
        if section != unicodedata.normalize("NFC", section):
            raise StrictInputError(f"{label} section names must use Unicode NFC")
        if canonical in section_keys:
            raise StrictInputError(
                f"{label} has case/NFC-conflicting sections: "
                f"{section_keys[canonical]!r} and {section!r}"
            )
        section_keys[canonical] = section

        option_keys: dict[str, str] = {}
        for option in parser[section]:
            if option != unicodedata.normalize("NFC", option):
                raise StrictInputError(f"{label} option names must use Unicode NFC")
            option_key = _collision_key(option)
            if option_key in option_keys:
                raise StrictInputError(
                    f"{label} section {section!r} has case/NFC-conflicting options"
                )
            option_keys[option_key] = option
    return parser


def validate_runtime_ini(raw: bytes) -> str:
    parser = _load_strict_ini(raw, "config.ini")
    if "General" not in parser:
        raise StrictInputError("config.ini must contain exact [General] section")
    if "SystemParams" not in parser:
        raise StrictInputError("config.ini must contain exact [SystemParams] section")

    current_brand_locations = [
        (section, key)
        for section in parser.sections()
        for key in parser[section]
        if _collision_key(key) == "currentbrand"
    ]
    if current_brand_locations != [("General", "CurrentBrand")]:
        raise StrictInputError(
            "config.ini must contain exactly General/CurrentBrand with exact casing"
        )

    reject_locations = [
        (section, key)
        for section in parser.sections()
        for key in parser[section]
        if _collision_key(key) == "rejectenabled"
    ]
    if reject_locations != [("SystemParams", "rejectEnabled")]:
        raise StrictInputError(
            "config.ini must contain exactly SystemParams/rejectEnabled"
        )
    if parser["SystemParams"]["rejectEnabled"].strip() != "false":
        raise StrictInputError("config.ini rejectEnabled must be exactly false")

    brand = parser["General"]["CurrentBrand"].strip()
    return canonical_component(brand, "config.ini General/CurrentBrand")


def validate_brand_ini(raw: bytes) -> None:
    parser = _load_strict_ini(raw, "brand para.ini")
    if not parser.sections():
        raise StrictInputError("brand para.ini must contain at least one section")


def _walk_package(package: Path, excluded: set[str], log: CheckLog) -> set[str]:
    actual: set[str] = set()
    identities: dict[str, str] = {}
    try:
        iterator = os.walk(package, topdown=True, followlinks=False)
        for root_text, directories, files in iterator:
            root = Path(root_text)
            safe_directories: list[str] = []
            for name in sorted(directories):
                candidate = root / name
                relative = candidate.relative_to(package).as_posix()
                try:
                    metadata = candidate.lstat()
                except OSError as exc:
                    log.fail("package.tree", f"cannot inspect directory: {exc}",
                             path=relative)
                    continue
                if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                    log.fail("package.tree",
                             "symbolic-link/reparse-point directory is forbidden",
                             path=relative)
                    continue
                if not stat.S_ISDIR(metadata.st_mode):
                    log.fail("package.tree", "non-directory entry found in directory list",
                             path=relative)
                    continue
                safe_directories.append(name)
            directories[:] = safe_directories

            for name in sorted(files):
                candidate = root / name
                relative = candidate.relative_to(package).as_posix()
                try:
                    metadata = candidate.lstat()
                except OSError as exc:
                    log.fail("package.tree", f"cannot inspect file: {exc}", path=relative)
                    continue
                if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                    log.fail("package.tree",
                             "symbolic-link/reparse-point file is forbidden",
                             path=relative)
                    continue
                if not stat.S_ISREG(metadata.st_mode):
                    log.fail("package.tree", "non-regular package entry is forbidden",
                             path=relative)
                    continue
                try:
                    canonical_relative_path(relative, "package file path")
                except StrictInputError as exc:
                    log.fail("package.tree", str(exc), path=relative)
                    continue
                identity = _collision_key(relative)
                previous = identities.get(identity)
                if previous is not None and previous != relative:
                    log.fail(
                        "package.tree",
                        "package contains a casefold/NFC path conflict",
                        first=previous,
                        second=relative,
                    )
                else:
                    identities[identity] = relative
                if relative not in excluded:
                    actual.add(relative)
    except OSError as exc:
        log.fail("package.tree", f"cannot enumerate package: {exc}", path=str(package))
    return actual


def validate_manifest_and_package(
    document: Any,
    gate: dict[str, Any],
    package: Path,
    manifest_path: Path,
    log: CheckLog,
) -> None:
    try:
        manifest = _strict_object(document, MANIFEST_KEYS, "package manifest")
        if manifest["schemaVersion"] != gate["packageManifestSchemaVersion"]:
            raise StrictInputError("package manifest schemaVersion mismatch")
        records = manifest["files"]
        if not isinstance(records, list) or not records:
            raise StrictInputError("package manifest files must be a non-empty array")
    except StrictInputError as exc:
        log.fail("package.manifest.schema", str(exc))
        return

    entries: dict[str, tuple[int, str]] = {}
    identities: dict[str, str] = {}
    for index, value in enumerate(records):
        label = f"package manifest files[{index}]"
        try:
            record = _strict_object(value, MANIFEST_FILE_KEYS, label)
            path = canonical_relative_path(record["path"], f"{label}.path")
            size = record["size"]
            digest = record["sha256"]
            if isinstance(size, bool) or not isinstance(size, int) or size < 0:
                raise StrictInputError(f"{label}.size must be a non-negative integer")
            if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
                raise StrictInputError(f"{label}.sha256 must be 64 lowercase hex digits")
            if path in entries:
                raise StrictInputError(f"duplicate manifest path: {path!r}")
            identity = _collision_key(path)
            if identity in identities:
                raise StrictInputError(
                    "casefold/NFC-conflicting manifest paths: "
                    f"{identities[identity]!r} and {path!r}"
                )
            identities[identity] = path
            entries[path] = (size, digest)
        except StrictInputError as exc:
            log.fail("package.manifest.entry", str(exc), index=index)

    excluded: set[str] = set()
    manifest_resolved = _resolved(manifest_path)
    package_resolved = _resolved(package)
    if _is_relative_to(manifest_resolved, package_resolved):
        excluded.add(manifest_resolved.relative_to(package_resolved).as_posix())

    actual = _walk_package(package, excluded, log)
    declared = set(entries)
    missing_records = sorted(actual - declared)
    extra_records = sorted(declared - actual)
    if missing_records or extra_records:
        log.fail(
            "package.manifest.completeness",
            "manifest must cover every regular package file exactly once",
            unlistedPackageFiles=missing_records,
            missingDeclaredFiles=extra_records,
        )
    else:
        log.ok(
            "package.manifest.completeness",
            "manifest exactly covers package files",
            fileCount=len(actual),
        )

    for relative, (expected_size, expected_hash) in sorted(entries.items()):
        candidate = package / Path(*PurePosixPath(relative).parts)
        if _path_has_symlink_component(package, relative):
            log.fail(
                "package.file",
                "manifest path contains a symbolic-link component",
                path=relative,
            )
            continue
        try:
            actual_size, actual_hash = _sha256_file(candidate, f"package file {relative}")
        except StrictInputError as exc:
            log.fail("package.file", str(exc), path=relative)
            continue
        if actual_size != expected_size or actual_hash != expected_hash:
            log.fail(
                "package.file",
                "package file size/SHA-256 mismatch",
                path=relative,
                expectedSize=expected_size,
                actualSize=actual_size,
                expectedSha256=expected_hash,
                actualSha256=actual_hash,
            )

    config_relative = gate["configIniPath"]
    if config_relative not in entries:
        log.fail("package.config", "config.ini must be bound by the package manifest",
                 path=config_relative)
        return
    config_path = package / Path(*PurePosixPath(config_relative).parts)
    try:
        brand = validate_runtime_ini(_read_regular_file(config_path, "config.ini"))
    except StrictInputError as exc:
        log.fail("package.config", str(exc), path=config_relative)
        return
    log.ok(
        "package.config",
        "runtime config has exact brand and disabled reject settings",
        currentBrand=brand,
        rejectEnabled=False,
    )

    brand_root_relative = gate["brandRootPath"]
    brand_root = package / Path(*PurePosixPath(brand_root_relative).parts)
    brand_relative = f"{brand_root_relative}/{brand}"
    brand_directory = package / Path(*PurePosixPath(brand_relative).parts)
    para_relative = f"{brand_relative}/{gate['brandParameterFileName']}"
    para_path = package / Path(*PurePosixPath(para_relative).parts)
    template_relative = f"{brand_relative}/{gate['brandTemplateDirectoryName']}"
    template_path = package / Path(*PurePosixPath(template_relative).parts)
    try:
        if _path_has_symlink_component(package, brand_root_relative):
            raise StrictInputError("brand root contains a symbolic-link component")
        _require_directory_non_symlink(brand_root, "brand root")
        if _path_has_symlink_component(package, brand_relative):
            raise StrictInputError("current brand directory contains a symbolic-link component")
        _require_directory_non_symlink(brand_directory, "current brand directory")
        if para_relative not in entries:
            raise StrictInputError("current brand para.ini is not bound by the manifest")
        if _path_has_symlink_component(package, para_relative):
            raise StrictInputError("current brand para.ini contains a symbolic-link component")
        validate_brand_ini(_read_regular_file(para_path, "brand para.ini"))
        if _path_has_symlink_component(package, template_relative):
            raise StrictInputError("brand template directory contains a symbolic-link component")
        _require_directory_non_symlink(template_path, "brand template directory")
    except StrictInputError as exc:
        log.fail(
            "package.brand",
            str(exc),
            brand=brand,
            paraIni=para_relative,
            templateDirectory=template_relative,
        )
        return
    log.ok(
        "package.brand",
        "current brand para.ini and template directory are present and safe",
        brand=brand,
        paraIni=para_relative,
        templateDirectory=template_relative,
    )


def validate_host_input_pair(
    windows_supplied: str | None,
    gpu_supplied: str | None,
    expected_challenge: str,
    expected_package_manifest_sha256: str,
    log: CheckLog,
) -> None:
    collector_path = Path(__file__).resolve().with_name(
        HOST_COLLECTOR_FILE_NAME
    )
    try:
        _, collector_digest = _sha256_file(
            collector_path, "trusted Windows host report collector"
        )
    except StrictInputError as exc:
        log.fail(
            "external.host-report-collector",
            str(exc),
            path=str(collector_path),
        )
        return

    identities: dict[str, dict[str, str]] = {}
    for kind, supplied in (
        ("windows", windows_supplied),
        ("gpu", gpu_supplied),
    ):
        name = f"external.{kind}"
        if not supplied:
            log.missing(name, f"external {kind} input was not supplied")
            continue
        path = _absolute(Path(supplied))
        try:
            metadata = path.lstat()
        except FileNotFoundError:
            log.missing(
                name,
                f"external {kind} input does not exist",
                path=str(path),
            )
            continue
        except OSError as exc:
            log.fail(
                name,
                f"external {kind} input cannot be inspected: {exc}",
                path=str(path),
            )
            continue
        if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
            log.fail(
                name,
                f"external {kind} input must not be a symbolic link or reparse point",
                path=str(path),
            )
            continue
        try:
            raw = _read_regular_file(path, f"external {kind} input")
            identity = validate_host_report_document(
                load_strict_json_bytes(raw, f"external {kind} input"),
                kind,
                collector_digest,
                expected_challenge,
                expected_package_manifest_sha256,
            )
        except StrictInputError as exc:
            log.fail(name, str(exc), path=str(path))
            continue
        identities[kind] = identity
        log.ok(
            name,
            f"external {kind} host report is strict and collector-bound",
            path=str(path),
            size=len(raw),
            sha256=_sha256_bytes(raw),
            schemaVersion=HOST_REPORT_SCHEMAS[kind],
            captureId=identity["captureId"],
            hostIdSha256=identity["hostIdSha256"],
            challenge=identity["challenge"],
            packageManifestSha256=identity["packageManifestSha256"],
            collectorSha256=collector_digest,
            collectorAssertionsValidated=True,
            productAcceptanceChecked=False,
            semanticAcceptanceChecked=False,
        )

    if set(identities) == {"windows", "gpu"}:
        compared_fields = (
            "captureId",
            "capturedAtUtc",
            "hostIdSha256",
            "challenge",
            "packageManifestSha256",
            "collectorSha256",
        )
        drifted = [
            field
            for field in compared_fields
            if identities["windows"][field] != identities["gpu"][field]
        ]
        if drifted:
            log.fail(
                "external.host-report-pair",
                "Windows and GPU host reports are not from the same capture",
                driftedFields=drifted,
            )
        else:
            log.ok(
                "external.host-report-pair",
                "Windows and GPU reports share one host/capture/collector identity",
                captureId=identities["windows"]["captureId"],
                hostIdSha256=identities["windows"]["hostIdSha256"],
                challenge=identities["windows"]["challenge"],
                packageManifestSha256=(
                    identities["windows"]["packageManifestSha256"]
                ),
                collectorSha256=collector_digest,
                productAcceptanceChecked=False,
                semanticAcceptanceChecked=False,
            )


def validate_output_location(
    output_dir: Path,
    package: Path,
    input_files: Iterable[Path],
) -> None:
    if _paths_overlap_directory(output_dir, package, other_is_dir=True):
        raise StrictInputError(
            "output directory must not be inside, equal to, or contain the package"
        )
    for input_path in input_files:
        if _paths_overlap_directory(output_dir, input_path, other_is_dir=False):
            raise StrictInputError(
                f"output directory overlaps input file: {input_path}"
            )
    if output_dir.exists():
        _require_directory_non_symlink(output_dir, "output directory")
    else:
        ancestor = output_dir.parent
        while not ancestor.exists() and ancestor != ancestor.parent:
            ancestor = ancestor.parent
        metadata = _lstat(ancestor, "output directory ancestor")
        if not stat.S_ISDIR(metadata.st_mode):
            raise StrictInputError("output directory ancestor is not a directory")


def atomic_write_json(path: Path, document: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    _require_directory_non_symlink(path.parent, "output directory")
    payload = (
        json.dumps(
            document,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
            allow_nan=False,
        )
        + "\n"
    ).encode("utf-8")
    descriptor = -1
    temporary_name = ""
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
        )
        with os.fdopen(descriptor, "wb") as stream:
            descriptor = -1
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
        temporary_name = ""
        try:
            directory_descriptor = os.open(path.parent, os.O_RDONLY)
        except OSError:
            directory_descriptor = -1
        if directory_descriptor >= 0:
            try:
                os.fsync(directory_descriptor)
            finally:
                os.close(directory_descriptor)
    except OSError as exc:
        raise StrictInputError(f"cannot atomically write report: {exc}") from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        if temporary_name:
            try:
                os.unlink(temporary_name)
            except OSError:
                pass


def _report(
    status: str,
    exit_code: int,
    log: CheckLog,
    gate_path: Path,
    package: Path,
    manifest: Path,
    output_dir: Path,
) -> dict[str, Any]:
    return {
        "schemaVersion": "p8-local-preflight-report-v1",
        "generatedAtUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "status": status,
        "exitCode": exit_code,
        "scope": "local-tooling-preflight-only",
        "productAcceptance": False,
        "claims": {
            "localPreflightReady": exit_code == EXIT_READY,
            "p8ProductAccepted": False,
            "windowsRuntimeAccepted": False,
            "gpuRuntimeAccepted": False,
            "realIoTested": False,
            "realRejectTested": False,
        },
        "safety": {
            "localOnly": True,
            "realIoEnabled": False,
            "realRejectEnabled": False,
        },
        "inputs": {
            "gateConfig": str(gate_path),
            "package": str(package),
            "manifest": str(manifest),
            "outputDirectory": str(output_dir),
        },
        "missingExternalInputs": sorted(log.external_missing),
        "checks": log.checks,
    }


class StrictArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_usage(sys.stderr)
        self.exit(EXIT_INVALID, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    repository_root = Path(__file__).resolve().parent.parent
    parser = StrictArgumentParser(description=__doc__)
    parser.add_argument(
        "--gate-config",
        default=str(repository_root / "config" / "p8-local-gates-v1.json"),
        help="strict P8 local gate config JSON",
    )
    parser.add_argument("--package", required=True, help="package directory to inspect")
    parser.add_argument("--manifest", required=True, help="package manifest JSON")
    parser.add_argument(
        "--manifest-sha256",
        required=True,
        help="externally supplied SHA-256 identity of the manifest",
    )
    parser.add_argument(
        "--windows-input",
        help=(
            "strict p8-windows-host-report-v2 generated by the trusted "
            "Windows collector; absence returns exit 2"
        ),
    )
    parser.add_argument(
        "--gpu-input",
        help=(
            "strict p8-gpu-host-report-v2 from the same trusted collection; "
            "absence returns exit 2"
        ),
    )
    parser.add_argument(
        "--host-challenge",
        required=True,
        help="64-lowercase-hex challenge generated for this wrapper run",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        help="safe directory for the atomic JSON report",
    )
    return parser


def run(args: argparse.Namespace) -> int:
    gate_path = _absolute(Path(args.gate_config))
    package = _absolute(Path(args.package))
    manifest_path = _absolute(Path(args.manifest))
    output_dir = _absolute(Path(args.output_dir))
    optional_inputs = [
        _absolute(Path(value))
        for value in (args.windows_input, args.gpu_input)
        if value
    ]
    log = CheckLog()

    try:
        validate_output_location(
            output_dir,
            package,
            [gate_path, manifest_path, *optional_inputs],
        )
    except StrictInputError as exc:
        error = {
            "schemaVersion": "p8-local-preflight-report-v1",
            "status": "invalid",
            "exitCode": EXIT_INVALID,
            "scope": "local-tooling-preflight-only",
            "productAcceptance": False,
            "error": str(exc),
            "reportWritten": False,
        }
        print(json.dumps(error, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return EXIT_INVALID

    gate: dict[str, Any] | None = None
    try:
        gate_raw = _read_regular_file(gate_path, "gate config")
        gate = validate_gate_document(load_strict_json_bytes(gate_raw, "gate config"))
        log.ok(
            "gate.config",
            "strict local-only gate config is valid",
            sha256=_sha256_bytes(gate_raw),
        )
    except StrictInputError as exc:
        log.fail("gate.config", str(exc), path=str(gate_path))

    manifest_document: Any = None
    try:
        manifest_raw = _read_regular_file(manifest_path, "package manifest")
        actual_manifest_hash = _sha256_bytes(manifest_raw)
        supplied_hash = args.manifest_sha256.strip()
        if not SHA256_RE.fullmatch(supplied_hash):
            raise StrictInputError("--manifest-sha256 must be exactly 64 hex digits")
        if actual_manifest_hash != supplied_hash.lower():
            raise StrictInputError(
                "package manifest SHA-256 does not match --manifest-sha256"
            )
        manifest_document = load_strict_json_bytes(manifest_raw, "package manifest")
        log.ok(
            "package.manifest.identity",
            "manifest matches the externally supplied SHA-256",
            sha256=actual_manifest_hash,
        )
    except StrictInputError as exc:
        log.fail("package.manifest.identity", str(exc), path=str(manifest_path))

    package_valid = False
    try:
        _require_directory_non_symlink(package, "package")
        package_valid = True
        log.ok("package.root", "package is a non-symlink directory", path=str(package))
    except StrictInputError as exc:
        log.fail("package.root", str(exc), path=str(package))

    if gate is not None and manifest_document is not None and package_valid:
        validate_manifest_and_package(
            manifest_document, gate, package, manifest_path, log
        )

    validate_host_input_pair(
        args.windows_input,
        args.gpu_input,
        args.host_challenge,
        args.manifest_sha256.lower(),
        log,
    )

    if log.invalid:
        exit_code = EXIT_INVALID
        status = "invalid"
    elif log.external_missing:
        exit_code = EXIT_EXTERNAL_MISSING
        status = "external-inputs-missing"
    else:
        exit_code = EXIT_READY
        status = "ready"

    report_name = gate["reportFileName"] if gate is not None else DEFAULT_REPORT_NAME
    report_path = output_dir / report_name
    report = _report(
        status, exit_code, log, gate_path, package, manifest_path, output_dir
    )
    try:
        atomic_write_json(report_path, report)
    except StrictInputError as exc:
        print(str(exc), file=sys.stderr)
        return EXIT_INVALID
    print(
        json.dumps(
            {
                "status": status,
                "exitCode": exit_code,
                "report": str(report_path),
                "productAcceptance": False,
            },
            ensure_ascii=False,
            sort_keys=True,
        )
    )
    return exit_code


def main() -> int:
    return run(build_parser().parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
