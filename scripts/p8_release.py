#!/usr/bin/env python3
"""Build, verify, activate, and roll back immutable P8 fixture releases.

This is local fixture tooling.  Its artifacts are deliberately identified as
``fixture-local-tooling`` and are not Windows product release packages.

Only the Python standard library is used so the same integrity contract can be
checked from a minimal Python 3 installation.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import ntpath
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import unicodedata
from pathlib import Path, PurePosixPath
from typing import Any, Callable, Iterable, Mapping, Sequence, Union


MANIFEST_SCHEMA = "p8-fixture-release-v1"
STATE_SCHEMA = "p8-fixture-current-state-v1"
ARTIFACT_KIND = "fixture-local-tooling"
ARTIFACT_SCOPE = "fixture-local-tooling-only"
MANIFEST_NAME = "release-manifest.json"
SHARED_DIRECTORY_NAMES = ("config", "brands", "data", "logs", "evidence")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
RELEASE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
ROLE_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
MANIFEST_KEYS = {
    "schemaVersion", "artifactKind", "scope", "releaseId", "files",
}
FILE_KEYS = {"path", "role", "size", "sha256"}
SOURCE_MANIFEST_KEYS = {"schemaVersion", "files"}
SOURCE_FILE_KEYS = {"path", "size", "sha256"}
SOURCE_MANIFEST_SCHEMA = "p8-package-manifest-v1"
STATE_KEYS = {"schemaVersion", "artifactKind", "scope", "current", "previous"}
REFERENCE_KEYS = {"releaseId", "manifestSha256"}

SmokeCommand = Union[Sequence[str], str, Callable[[Path], object]]


class ReleaseError(ValueError):
    """A release contract, integrity, or transaction error."""


class _DuplicateJsonKey(ValueError):
    pass


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    value: dict[str, Any] = {}
    for key, item in pairs:
        if key in value:
            raise _DuplicateJsonKey(f"duplicate JSON key: {key!r}")
        value[key] = item
    return value


def _read_json_bytes(raw: bytes, source: Path) -> Any:
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ReleaseError(f"{source}: JSON must be UTF-8") from exc
    try:
        return json.loads(text, object_pairs_hook=_strict_object)
    except (_DuplicateJsonKey, json.JSONDecodeError) as exc:
        raise ReleaseError(f"{source}: invalid strict JSON: {exc}") from exc


def _read_json(path: Path) -> Any:
    return _read_json_bytes(_read_regular_bytes(path), path)


def _json_bytes(value: object) -> bytes:
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            indent=2,
            separators=(",", ": "),
        )
        + "\n"
    ).encode("utf-8")


def _normalize_sha256(value: str, field: str) -> str:
    if not isinstance(value, str):
        raise ReleaseError(f"{field} must be a SHA-256 string")
    normalized = value.lower()
    if not SHA256_RE.fullmatch(normalized):
        raise ReleaseError(f"{field} must contain exactly 64 hexadecimal digits")
    return normalized


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        _require_no_windows_reparse_ancestors(path)
        before = path.lstat()
        if stat.S_ISLNK(before.st_mode) or _is_reparse_point(before):
            raise ReleaseError(f"symlink or reparse point is forbidden: {path}")
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise ReleaseError(f"cannot safely open regular file {path}: {exc}") from exc
    try:
        information = os.fstat(descriptor)
        if not stat.S_ISREG(information.st_mode) or _is_reparse_point(information):
            raise ReleaseError(f"not a regular file: {path}")
        with os.fdopen(descriptor, "rb", closefd=False) as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
    finally:
        os.close(descriptor)
    return digest.hexdigest()


def _is_reparse_point(metadata: os.stat_result) -> bool:
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & reparse_flag)


def _require_no_windows_reparse_ancestors(path: Path) -> None:
    if os.name != "nt":
        return
    current = Path(os.path.abspath(os.fspath(path)))
    while True:
        try:
            metadata = current.lstat()
        except FileNotFoundError:
            pass
        except OSError as exc:
            raise ReleaseError(
                f"cannot inspect path ancestor {current}: {exc}") from exc
        else:
            if _is_reparse_point(metadata):
                raise ReleaseError(
                    f"Windows reparse-point ancestor is forbidden: {current}")
        if current == current.parent:
            return
        current = current.parent


def _read_regular_bytes(path: Path) -> bytes:
    _require_no_windows_reparse_ancestors(path)
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        before = path.lstat()
        if stat.S_ISLNK(before.st_mode) or _is_reparse_point(before):
            raise ReleaseError(f"symlink or reparse point is forbidden: {path}")
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise ReleaseError(f"cannot safely open regular file {path}: {exc}") from exc
    try:
        information = os.fstat(descriptor)
        if not stat.S_ISREG(information.st_mode) or _is_reparse_point(information):
            raise ReleaseError(f"not a regular non-reparse file: {path}")
        chunks: list[bytes] = []
        while True:
            chunk = os.read(descriptor, 1024 * 1024)
            if not chunk:
                return b"".join(chunks)
            chunks.append(chunk)
    finally:
        os.close(descriptor)


def _validate_release_id(value: str) -> str:
    if not isinstance(value, str) or not RELEASE_ID_RE.fullmatch(value):
        raise ReleaseError(
            "release id must match [A-Za-z0-9][A-Za-z0-9._-]{0,127}"
        )
    if value != unicodedata.normalize("NFC", value) or value in {".", ".."}:
        raise ReleaseError("release id must be NFC-normalized and path-safe")
    return value


def _validate_role(value: str) -> str:
    if not isinstance(value, str) or not ROLE_RE.fullmatch(value):
        raise ReleaseError(
            "role must match [A-Za-z0-9][A-Za-z0-9._-]{0,127}"
        )
    return value


def canonical_relative_path(value: str) -> str:
    """Validate and return a portable, NFC, relative POSIX path."""
    if not isinstance(value, str) or not value:
        raise ReleaseError("file path must be a non-empty string")
    if value != unicodedata.normalize("NFC", value):
        raise ReleaseError(f"path is not NFC-normalized: {value!r}")
    if "\\" in value:
        raise ReleaseError(f"backslash is forbidden in canonical path: {value!r}")
    drive, _ = ntpath.splitdrive(value)
    if drive or value.startswith(("/", "\\")):
        raise ReleaseError(f"absolute path is forbidden: {value!r}")
    if any(ord(character) < 32 or ord(character) == 127 for character in value):
        raise ReleaseError(f"control character is forbidden in path: {value!r}")
    if ":" in value:
        raise ReleaseError(f"colon is forbidden in portable path: {value!r}")
    path = PurePosixPath(value)
    parts = path.parts
    if (
        not parts
        or any(part in {"", ".", ".."} for part in parts)
        or value != "/".join(parts)
    ):
        raise ReleaseError(f"path is not a canonical relative path: {value!r}")
    for part in parts:
        if part.endswith((" ", ".")):
            raise ReleaseError(
                f"path component has an ambiguous trailing character: {value!r}"
            )
    return value


def _path_collision_key(path: str) -> str:
    return unicodedata.normalize("NFC", path).casefold()


def _register_node(
    path: str,
    kind: str,
    seen: dict[str, tuple[str, str]],
) -> None:
    canonical_relative_path(path)
    key = _path_collision_key(path)
    previous = seen.get(key)
    if previous is not None and previous != (path, kind):
        raise ReleaseError(
            "casefold/NFC path collision or file/directory conflict: "
            f"{previous[0]!r} ({previous[1]}) vs {path!r} ({kind})"
        )
    seen[key] = (path, kind)


def _validate_manifest_path_set(paths: Iterable[str]) -> None:
    seen: dict[str, tuple[str, str]] = {}
    for raw_path in paths:
        path = canonical_relative_path(raw_path)
        parts = path.split("/")
        for index in range(1, len(parts)):
            _register_node("/".join(parts[:index]), "directory", seen)
        _register_node(path, "file", seen)


def _ensure_real_directory(path: Path, *, create: bool) -> None:
    _require_no_windows_reparse_ancestors(path)
    if path.exists() or path.is_symlink():
        metadata = path.lstat()
        if (path.is_symlink() or _is_reparse_point(metadata) or
                not stat.S_ISDIR(metadata.st_mode)):
            raise ReleaseError(
                f"real directory required, symlink/reparse point forbidden: {path}")
        return
    if not create:
        raise ReleaseError(f"directory does not exist: {path}")
    try:
        path.mkdir(parents=True, exist_ok=False)
    except OSError as exc:
        raise ReleaseError(f"cannot create directory {path}: {exc}") from exc
    if path.is_symlink() or not path.is_dir():
        raise ReleaseError(f"failed to create a real directory: {path}")


def _scan_tree(root: Path) -> tuple[dict[str, Path], set[str]]:
    _require_no_windows_reparse_ancestors(root)
    try:
        root_metadata = root.lstat()
    except OSError as exc:
        raise ReleaseError(f"cannot inspect source/release directory {root}: {exc}") from exc
    if (stat.S_ISLNK(root_metadata.st_mode) or _is_reparse_point(root_metadata) or
            not stat.S_ISDIR(root_metadata.st_mode)):
        raise ReleaseError(f"real source/release directory required: {root}")
    files: dict[str, Path] = {}
    directories: set[str] = set()
    seen: dict[str, tuple[str, str]] = {}

    def visit(directory: Path, relative_parts: tuple[str, ...]) -> None:
        try:
            entries = sorted(os.scandir(directory), key=lambda item: item.name)
        except OSError as exc:
            raise ReleaseError(f"cannot enumerate directory {directory}: {exc}") from exc
        for entry in entries:
            relative = "/".join((*relative_parts, entry.name))
            canonical_relative_path(relative)
            try:
                information = entry.stat(follow_symlinks=False)
            except OSError as exc:
                raise ReleaseError(f"cannot stat {entry.path}: {exc}") from exc
            if entry.is_symlink() or _is_reparse_point(information):
                raise ReleaseError(
                    f"symlink or reparse point is forbidden: {relative}")
            if stat.S_ISDIR(information.st_mode):
                _register_node(relative, "directory", seen)
                directories.add(relative)
                visit(Path(entry.path), (*relative_parts, entry.name))
            elif stat.S_ISREG(information.st_mode):
                _register_node(relative, "file", seen)
                files[relative] = Path(entry.path)
            else:
                raise ReleaseError(
                    f"only regular files and directories are allowed: {relative}"
                )

    visit(root, ())
    return files, directories


def _manifest_parent_directories(paths: Iterable[str]) -> set[str]:
    result: set[str] = set()
    for path in paths:
        parts = path.split("/")
        for index in range(1, len(parts)):
            result.add("/".join(parts[:index]))
    return result


def _validate_manifest(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != MANIFEST_KEYS:
        raise ReleaseError(
            f"manifest must contain exactly these keys: {sorted(MANIFEST_KEYS)}"
        )
    if value["schemaVersion"] != MANIFEST_SCHEMA:
        raise ReleaseError(f"schemaVersion must be {MANIFEST_SCHEMA!r}")
    if value["artifactKind"] != ARTIFACT_KIND:
        raise ReleaseError(f"artifactKind must be {ARTIFACT_KIND!r}")
    if value["scope"] != ARTIFACT_SCOPE:
        raise ReleaseError(f"scope must be {ARTIFACT_SCOPE!r}")
    release_id = _validate_release_id(value["releaseId"])
    raw_files = value["files"]
    if not isinstance(raw_files, list) or not raw_files:
        raise ReleaseError("manifest files must be a non-empty list")

    validated_files: list[dict[str, Any]] = []
    paths: list[str] = []
    for index, item in enumerate(raw_files):
        if not isinstance(item, dict) or set(item) != FILE_KEYS:
            raise ReleaseError(
                f"files[{index}] must contain exactly these keys: {sorted(FILE_KEYS)}"
            )
        path = canonical_relative_path(item["path"])
        if path == MANIFEST_NAME:
            raise ReleaseError(f"{MANIFEST_NAME} cannot list itself")
        role = _validate_role(item["role"])
        size = item["size"]
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ReleaseError(f"files[{index}].size must be a non-negative integer")
        digest = _normalize_sha256(item["sha256"], f"files[{index}].sha256")
        if digest != item["sha256"]:
            raise ReleaseError(f"files[{index}].sha256 must use lowercase hex")
        paths.append(path)
        validated_files.append(
            {"path": path, "role": role, "size": size, "sha256": digest}
        )

    _validate_manifest_path_set(paths)
    if paths != sorted(paths):
        raise ReleaseError("manifest files must be sorted by canonical path")
    if len(set(paths)) != len(paths):
        raise ReleaseError("manifest file paths must be unique")
    return {
        "schemaVersion": MANIFEST_SCHEMA,
        "artifactKind": ARTIFACT_KIND,
        "scope": ARTIFACT_SCOPE,
        "releaseId": release_id,
        "files": validated_files,
    }


def validate_manifest(value: Any) -> dict[str, Any]:
    """Public strict manifest validator used by tests and other local tooling."""
    return _validate_manifest(value)


def _load_bound_source_manifest(
    path: Path,
    expected_sha256: str,
) -> tuple[dict[str, tuple[int, str]], str]:
    expected_digest = _normalize_sha256(
        expected_sha256, "source manifest SHA-256")
    raw = _read_regular_bytes(path)
    actual_digest = hashlib.sha256(raw).hexdigest()
    if actual_digest != expected_digest:
        raise ReleaseError(
            "source manifest SHA-256 mismatch: "
            f"expected {expected_digest}, got {actual_digest}"
        )
    value = _read_json_bytes(raw, path)
    if not isinstance(value, dict) or set(value) != SOURCE_MANIFEST_KEYS:
        raise ReleaseError(
            "source manifest must contain exactly schemaVersion and files"
        )
    if value["schemaVersion"] != SOURCE_MANIFEST_SCHEMA:
        raise ReleaseError(
            f"source manifest schemaVersion must be {SOURCE_MANIFEST_SCHEMA!r}"
        )
    raw_files = value["files"]
    if not isinstance(raw_files, list) or not raw_files:
        raise ReleaseError("source manifest files must be a non-empty list")
    records: dict[str, tuple[int, str]] = {}
    identities: dict[str, str] = {}
    for index, item in enumerate(raw_files):
        if not isinstance(item, dict) or set(item) != SOURCE_FILE_KEYS:
            raise ReleaseError(
                f"source manifest files[{index}] must contain exactly "
                f"{sorted(SOURCE_FILE_KEYS)}"
            )
        relative = canonical_relative_path(item["path"])
        size = item["size"]
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ReleaseError(
                f"source manifest files[{index}].size must be non-negative")
        digest = _normalize_sha256(
            item["sha256"], f"source manifest files[{index}].sha256")
        if digest != item["sha256"]:
            raise ReleaseError(
                f"source manifest files[{index}].sha256 must use lowercase hex")
        identity = _path_collision_key(relative)
        if relative in records or identity in identities:
            raise ReleaseError(
                f"duplicate or casefold/NFC-conflicting source path: {relative!r}")
        records[relative] = (size, digest)
        identities[identity] = relative
    _validate_manifest_path_set(records)
    return records, actual_digest


def _validate_reference(value: Any, field: str) -> dict[str, str]:
    if not isinstance(value, dict) or set(value) != REFERENCE_KEYS:
        raise ReleaseError(
            f"{field} must contain exactly these keys: {sorted(REFERENCE_KEYS)}"
        )
    return {
        "releaseId": _validate_release_id(value["releaseId"]),
        "manifestSha256": _normalize_sha256(
            value["manifestSha256"], f"{field}.manifestSha256"
        ),
    }


def _validate_state(value: Any) -> dict[str, Any]:
    if not isinstance(value, dict) or set(value) != STATE_KEYS:
        raise ReleaseError(
            f"state must contain exactly these keys: {sorted(STATE_KEYS)}"
        )
    if value["schemaVersion"] != STATE_SCHEMA:
        raise ReleaseError(f"state schemaVersion must be {STATE_SCHEMA!r}")
    if value["artifactKind"] != ARTIFACT_KIND:
        raise ReleaseError(f"state artifactKind must be {ARTIFACT_KIND!r}")
    if value["scope"] != ARTIFACT_SCOPE:
        raise ReleaseError(f"state scope must be {ARTIFACT_SCOPE!r}")
    current = _validate_reference(value["current"], "current")
    previous = value["previous"]
    if previous is not None:
        previous = _validate_reference(previous, "previous")
    return {
        "schemaVersion": STATE_SCHEMA,
        "artifactKind": ARTIFACT_KIND,
        "scope": ARTIFACT_SCOPE,
        "current": current,
        "previous": previous,
    }


def _copy_and_hash(source: Path, destination: Path) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        _require_no_windows_reparse_ancestors(source)
        before = source.lstat()
        if stat.S_ISLNK(before.st_mode) or _is_reparse_point(before):
            raise ReleaseError(
                f"source symlink or reparse point is forbidden: {source}")
        source_descriptor = os.open(source, flags)
    except OSError as exc:
        raise ReleaseError(f"cannot safely open source file {source}: {exc}") from exc
    try:
        source_info = os.fstat(source_descriptor)
        if (not stat.S_ISREG(source_info.st_mode) or
                _is_reparse_point(source_info)):
            raise ReleaseError(f"source is not a regular file: {source}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            destination_descriptor = os.open(
                destination,
                os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_BINARY", 0),
                0o644,
            )
        except OSError as exc:
            raise ReleaseError(f"cannot create staged file {destination}: {exc}") from exc
        try:
            while True:
                chunk = os.read(source_descriptor, 1024 * 1024)
                if not chunk:
                    break
                digest.update(chunk)
                size += len(chunk)
                view = memoryview(chunk)
                while view:
                    written = os.write(destination_descriptor, view)
                    view = view[written:]
            os.fsync(destination_descriptor)
        finally:
            os.close(destination_descriptor)
    finally:
        os.close(source_descriptor)
    return size, digest.hexdigest()


def _write_new_file(path: Path, raw: bytes) -> None:
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    except OSError as exc:
        raise ReleaseError(f"cannot create {path}: {exc}") from exc
    try:
        view = memoryview(raw)
        while view:
            written = os.write(descriptor, view)
            view = view[written:]
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _fsync_directory(path: Path) -> None:
    flags = os.O_RDONLY
    if hasattr(os, "O_DIRECTORY"):
        flags |= os.O_DIRECTORY
    try:
        descriptor = os.open(path, flags)
    except OSError:
        return
    try:
        os.fsync(descriptor)
    except OSError:
        pass
    finally:
        os.close(descriptor)


def _make_tree_writable(path: Path) -> None:
    if not path.exists() or path.is_symlink():
        return
    for directory, directory_names, file_names in os.walk(path, topdown=False):
        for name in file_names:
            try:
                os.chmod(Path(directory) / name, 0o600)
            except OSError:
                pass
        for name in directory_names:
            try:
                os.chmod(Path(directory) / name, 0o700)
            except OSError:
                pass
    try:
        os.chmod(path, 0o700)
    except OSError:
        pass


def _remove_staging_tree(path: Path) -> None:
    if not path.exists() and not path.is_symlink():
        return
    _make_tree_writable(path)
    shutil.rmtree(path)


def _seal_release_tree(path: Path) -> None:
    files, directories = _scan_tree(path)
    for file_path in files.values():
        try:
            os.chmod(file_path, 0o444)
        except OSError as exc:
            raise ReleaseError(f"cannot make release file read-only: {file_path}: {exc}") from exc
    for relative in sorted(directories, key=lambda item: item.count("/"), reverse=True):
        try:
            os.chmod(path.joinpath(*relative.split("/")), 0o555)
        except OSError as exc:
            raise ReleaseError(f"cannot seal release directory {relative}: {exc}") from exc
    try:
        os.chmod(path, 0o555)
    except OSError as exc:
        raise ReleaseError(f"cannot seal release directory {path}: {exc}") from exc


def _case_safe_release_target(releases: Path, release_id: str) -> Path:
    target = releases / release_id
    wanted = _path_collision_key(release_id)
    try:
        entries = list(os.scandir(releases))
    except OSError as exc:
        raise ReleaseError(f"cannot enumerate releases directory {releases}: {exc}") from exc
    for entry in entries:
        if entry.name.startswith("."):
            continue
        if _path_collision_key(entry.name) == wanted and entry.name != release_id:
            raise ReleaseError(
                f"casefold/NFC release id collision: {entry.name!r} vs {release_id!r}"
            )
    return target


def _verify_release(
    release_dir: Path,
    expected_manifest_sha256: str,
    *,
    require_directory_name: bool,
) -> dict[str, Any]:
    expected_digest = _normalize_sha256(
        expected_manifest_sha256, "expected manifest SHA-256"
    )
    try:
        release_metadata = release_dir.lstat()
    except OSError as exc:
        raise ReleaseError(f"cannot inspect release directory {release_dir}: {exc}") from exc
    if (stat.S_ISLNK(release_metadata.st_mode) or
            _is_reparse_point(release_metadata) or
            not stat.S_ISDIR(release_metadata.st_mode)):
        raise ReleaseError(f"real release directory required: {release_dir}")
    manifest_path = release_dir / MANIFEST_NAME
    manifest_raw = _read_regular_bytes(manifest_path)
    actual_manifest_digest = hashlib.sha256(manifest_raw).hexdigest()
    if actual_manifest_digest != expected_digest:
        raise ReleaseError(
            "manifest SHA-256 mismatch: "
            f"expected {expected_digest}, got {actual_manifest_digest}"
        )
    manifest = _validate_manifest(_read_json_bytes(manifest_raw, manifest_path))
    if require_directory_name and manifest["releaseId"] != release_dir.name:
        raise ReleaseError(
            f"release directory name {release_dir.name!r} does not match "
            f"manifest releaseId {manifest['releaseId']!r}"
        )

    actual_files, actual_directories = _scan_tree(release_dir)
    expected_file_paths = {MANIFEST_NAME}
    expected_file_paths.update(item["path"] for item in manifest["files"])
    if set(actual_files) != expected_file_paths:
        missing = sorted(expected_file_paths - set(actual_files))
        extra = sorted(set(actual_files) - expected_file_paths)
        raise ReleaseError(
            f"release file set mismatch; missing={missing}, extra={extra}"
        )
    expected_directories = _manifest_parent_directories(expected_file_paths)
    if actual_directories != expected_directories:
        missing = sorted(expected_directories - actual_directories)
        extra = sorted(actual_directories - expected_directories)
        raise ReleaseError(
            f"release directory set mismatch; missing={missing}, extra={extra}"
        )

    for item in manifest["files"]:
        path = actual_files[item["path"]]
        try:
            size = os.stat(path, follow_symlinks=False).st_size
        except OSError as exc:
            raise ReleaseError(f"cannot stat release file {item['path']}: {exc}") from exc
        if size != item["size"]:
            raise ReleaseError(
                f"size mismatch for {item['path']}: expected {item['size']}, got {size}"
            )
        digest = sha256_file(path)
        if digest != item["sha256"]:
            raise ReleaseError(
                f"SHA-256 mismatch for {item['path']}: "
                f"expected {item['sha256']}, got {digest}"
            )
    if sha256_file(manifest_path) != expected_digest:
        raise ReleaseError("manifest changed while the release was being verified")
    return manifest


def verify_release(
    release_dir: str | os.PathLike[str] | Path,
    expected_manifest_sha256: str,
) -> dict[str, Any]:
    """Strictly verify a release against an externally supplied manifest hash."""
    return _verify_release(
        Path(release_dir),
        expected_manifest_sha256,
        require_directory_name=True,
    )


def _validate_role_map(
    role_map: Mapping[str, str] | None,
    source_paths: set[str],
) -> dict[str, str]:
    result: dict[str, str] = {}
    if role_map is None:
        return result
    if not isinstance(role_map, Mapping):
        raise ReleaseError("role map must be a JSON object or mapping")
    seen: dict[str, tuple[str, str]] = {}
    for raw_path, raw_role in role_map.items():
        path = canonical_relative_path(raw_path)
        _register_node(path, "file", seen)
        if path not in source_paths:
            raise ReleaseError(f"role map references a missing source file: {path}")
        result[path] = _validate_role(raw_role)
    return result


def package_release(
    source: str | os.PathLike[str] | Path,
    deployment_root: str | os.PathLike[str] | Path,
    release_id: str,
    *,
    default_role: str = "fixture-payload",
    role_map: Mapping[str, str] | None = None,
    source_manifest: str | os.PathLike[str] | Path | None = None,
    source_manifest_sha256: str | None = None,
) -> dict[str, Any]:
    """Create an immutable ``releases/<release_id>`` fixture snapshot.

    Repeating the same package operation is idempotent.  Reusing a release id
    for different bytes is rejected and the existing immutable release is left
    untouched.
    """
    source_path = Path(source)
    root = Path(deployment_root)
    release_id = _validate_release_id(release_id)
    default_role = _validate_role(default_role)
    if (source_manifest is None) != (source_manifest_sha256 is None):
        raise ReleaseError(
            "source manifest and source manifest SHA-256 must be supplied together")
    if source_path.is_symlink() or not source_path.is_dir():
        raise ReleaseError(f"real source directory required: {source_path}")

    try:
        source_absolute = source_path.absolute()
        root_absolute = root.absolute()
        common = Path(os.path.commonpath((source_absolute, root_absolute)))
    except (OSError, ValueError) as exc:
        raise ReleaseError(f"cannot compare source and deployment paths: {exc}") from exc
    if common == source_absolute:
        raise ReleaseError(
            "deployment root cannot equal or be nested under the package source"
        )

    source_files, _ = _scan_tree(source_path)
    if not source_files:
        raise ReleaseError("package source must contain at least one regular file")
    if MANIFEST_NAME in source_files:
        raise ReleaseError(f"source path {MANIFEST_NAME!r} is reserved")
    bound_source: dict[str, tuple[int, str]] | None = None
    bound_source_digest: str | None = None
    if source_manifest is not None and source_manifest_sha256 is not None:
        bound_source, bound_source_digest = _load_bound_source_manifest(
            Path(source_manifest), source_manifest_sha256)
        if set(bound_source) != set(source_files):
            missing = sorted(set(bound_source) - set(source_files))
            extra = sorted(set(source_files) - set(bound_source))
            raise ReleaseError(
                "source tree does not match the externally bound manifest; "
                f"missing={missing}, extra={extra}"
            )
    roles = _validate_role_map(role_map, set(source_files))

    _ensure_real_directory(root, create=True)
    releases = root / "releases"
    _ensure_real_directory(releases, create=True)
    target = _case_safe_release_target(releases, release_id)
    try:
        staging = Path(
            tempfile.mkdtemp(prefix=f".{release_id}.staging-", dir=releases)
        )
    except OSError as exc:
        raise ReleaseError(f"cannot create release staging directory: {exc}") from exc

    try:
        records: list[dict[str, Any]] = []
        for relative in sorted(source_files):
            destination = staging.joinpath(*relative.split("/"))
            size, digest = _copy_and_hash(source_files[relative], destination)
            if bound_source is not None and bound_source[relative] != (size, digest):
                expected_size, expected_digest = bound_source[relative]
                raise ReleaseError(
                    f"source changed while packaging {relative!r}: "
                    f"expected size/hash {expected_size}/{expected_digest}, "
                    f"got {size}/{digest}"
                )
            records.append(
                {
                    "path": relative,
                    "role": roles.get(relative, default_role),
                    "size": size,
                    "sha256": digest,
                }
            )
        manifest = _validate_manifest(
            {
                "schemaVersion": MANIFEST_SCHEMA,
                "artifactKind": ARTIFACT_KIND,
                "scope": ARTIFACT_SCOPE,
                "releaseId": release_id,
                "files": records,
            }
        )
        manifest_bytes = _json_bytes(manifest)
        manifest_digest = hashlib.sha256(manifest_bytes).hexdigest()
        _write_new_file(staging / MANIFEST_NAME, manifest_bytes)
        _verify_release(
            staging, manifest_digest, require_directory_name=False
        )

        if target.exists() or target.is_symlink():
            if target.is_symlink() or not target.is_dir():
                raise ReleaseError(f"immutable release target is not a real directory: {target}")
            _verify_release(target, manifest_digest, require_directory_name=True)
            _remove_staging_tree(staging)
            return {
                "artifactKind": ARTIFACT_KIND,
                "scope": ARTIFACT_SCOPE,
                "releaseId": release_id,
                "releaseDir": str(target),
                "manifestSha256": manifest_digest,
                "fileCount": len(records),
                "idempotent": True,
                "sourceManifestSha256": bound_source_digest,
            }

        _seal_release_tree(staging)
        try:
            os.replace(staging, target)
        except OSError as exc:
            if target.is_dir() and not target.is_symlink():
                try:
                    _verify_release(target, manifest_digest, require_directory_name=True)
                except ReleaseError:
                    raise ReleaseError(
                        f"cannot atomically publish release {release_id}: {exc}"
                    ) from exc
                _remove_staging_tree(staging)
                return {
                    "artifactKind": ARTIFACT_KIND,
                    "scope": ARTIFACT_SCOPE,
                    "releaseId": release_id,
                    "releaseDir": str(target),
                    "manifestSha256": manifest_digest,
                    "fileCount": len(records),
                    "idempotent": True,
                    "sourceManifestSha256": bound_source_digest,
                }
            raise ReleaseError(
                f"cannot atomically publish release {release_id}: {exc}"
            ) from exc
        _fsync_directory(releases)
        _verify_release(target, manifest_digest, require_directory_name=True)
        return {
            "artifactKind": ARTIFACT_KIND,
            "scope": ARTIFACT_SCOPE,
            "releaseId": release_id,
            "releaseDir": str(target),
            "manifestSha256": manifest_digest,
            "fileCount": len(records),
            "idempotent": False,
            "sourceManifestSha256": bound_source_digest,
        }
    finally:
        if staging.exists() or staging.is_symlink():
            _remove_staging_tree(staging)


def _state_path(deployment_root: Path) -> Path:
    return deployment_root / "state" / "current-release.json"


def read_current_state(
    deployment_root: str | os.PathLike[str] | Path,
) -> dict[str, Any] | None:
    root = Path(deployment_root)
    state_directory = root / "state"
    if state_directory.is_symlink():
        raise ReleaseError(f"state directory cannot be a symlink: {state_directory}")
    path = _state_path(root)
    if not path.exists() and not path.is_symlink():
        return None
    if path.is_symlink() or not path.is_file():
        raise ReleaseError(f"current release state must be a regular file: {path}")
    return _validate_state(_read_json(path))


def _atomic_write_state(deployment_root: Path, state_value: dict[str, Any]) -> None:
    state = _validate_state(state_value)
    state_directory = deployment_root / "state"
    _ensure_real_directory(deployment_root, create=True)
    _ensure_real_directory(state_directory, create=True)
    destination = state_directory / "current-release.json"
    if destination.is_symlink():
        raise ReleaseError(f"state file cannot be a symlink: {destination}")
    raw = _json_bytes(state)
    descriptor = -1
    temporary_name = ""
    try:
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=".current-release.", suffix=".tmp", dir=state_directory
        )
        with os.fdopen(descriptor, "wb") as handle:
            descriptor = -1
            handle.write(raw)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_name, destination)
        temporary_name = ""
        _fsync_directory(state_directory)
    except OSError as exc:
        raise ReleaseError(f"cannot atomically update current release state: {exc}") from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)
        if temporary_name:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass


def _run_smoke(
    smoke_command: SmokeCommand | None,
    release_dir: Path,
    release_id: str,
    manifest_sha256: str,
    timeout_seconds: float,
) -> None:
    if smoke_command is None:
        return
    if timeout_seconds <= 0:
        raise ReleaseError("smoke timeout must be greater than zero")
    if callable(smoke_command):
        try:
            result = smoke_command(release_dir)
        except Exception as exc:
            raise ReleaseError(f"smoke check raised an exception: {exc}") from exc
        if result is False or (
            isinstance(result, int)
            and not isinstance(result, bool)
            and result != 0
        ):
            raise ReleaseError(f"smoke check failed with result {result!r}")
        return
    if isinstance(smoke_command, str):
        command = shlex.split(smoke_command, posix=os.name != "nt")
    else:
        command = [str(item) for item in smoke_command]
    if not command or any(not item for item in command):
        raise ReleaseError("smoke command must contain at least one non-empty argument")
    environment = os.environ.copy()
    environment.update(
        {
            "P8_RELEASE_DIR": str(release_dir),
            "P8_RELEASE_ID": release_id,
            "P8_MANIFEST_SHA256": manifest_sha256,
            "P8_ARTIFACT_KIND": ARTIFACT_KIND,
        }
    )
    try:
        completed = subprocess.run(
            command,
            cwd=release_dir,
            env=environment,
            check=False,
            timeout=timeout_seconds,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ReleaseError(f"smoke command could not complete: {exc}") from exc
    if completed.returncode != 0:
        raise ReleaseError(
            f"smoke command failed with exit code {completed.returncode}"
        )


def _release_reference(release_id: str, manifest_sha256: str) -> dict[str, str]:
    return {
        "releaseId": _validate_release_id(release_id),
        "manifestSha256": _normalize_sha256(
            manifest_sha256, "manifest SHA-256"
        ),
    }


def _switch_current(
    deployment_root: Path,
    target: dict[str, str],
    *,
    smoke_command: SmokeCommand | None,
    smoke_timeout: float,
    require_existing_state: bool,
) -> dict[str, Any]:
    if deployment_root.is_symlink() or not deployment_root.is_dir():
        raise ReleaseError(f"real deployment root required: {deployment_root}")
    releases = deployment_root / "releases"
    _ensure_real_directory(releases, create=False)
    target_dir = _case_safe_release_target(releases, target["releaseId"])
    manifest = verify_release(target_dir, target["manifestSha256"])
    if manifest["releaseId"] != target["releaseId"]:
        raise ReleaseError("target release id does not match its verified manifest")

    existing = read_current_state(deployment_root)
    if require_existing_state and existing is None:
        raise ReleaseError("rollback requires an existing current release state")
    _run_smoke(
        smoke_command,
        target_dir,
        target["releaseId"],
        target["manifestSha256"],
        smoke_timeout,
    )
    if existing is not None and existing["current"] == target:
        return existing
    new_state = {
        "schemaVersion": STATE_SCHEMA,
        "artifactKind": ARTIFACT_KIND,
        "scope": ARTIFACT_SCOPE,
        "current": target,
        "previous": existing["current"] if existing is not None else None,
    }
    _atomic_write_state(deployment_root, new_state)
    return _validate_state(new_state)


def activate_release(
    deployment_root: str | os.PathLike[str] | Path,
    release_id: str,
    manifest_sha256: str,
    *,
    smoke_command: SmokeCommand | None = None,
    smoke_timeout: float = 60.0,
) -> dict[str, Any]:
    """Verify and atomically make a fixture release current."""
    target = _release_reference(release_id, manifest_sha256)
    return _switch_current(
        Path(deployment_root),
        target,
        smoke_command=smoke_command,
        smoke_timeout=smoke_timeout,
        require_existing_state=False,
    )


def rollback_release(
    deployment_root: str | os.PathLike[str] | Path,
    release_id: str | None = None,
    manifest_sha256: str | None = None,
    *,
    smoke_command: SmokeCommand | None = None,
    smoke_timeout: float = 60.0,
) -> dict[str, Any]:
    """Verify and atomically roll back to the recorded or explicit release."""
    root = Path(deployment_root)
    existing = read_current_state(root)
    if existing is None:
        raise ReleaseError("rollback requires an existing current release state")
    if release_id is None:
        if manifest_sha256 is not None:
            raise ReleaseError("manifest SHA-256 requires an explicit rollback release id")
        if existing["previous"] is None:
            raise ReleaseError("no previous release is recorded for rollback")
        target = existing["previous"]
    else:
        release_id = _validate_release_id(release_id)
        if manifest_sha256 is None:
            matching = [
                reference
                for reference in (existing["current"], existing["previous"])
                if reference is not None and reference["releaseId"] == release_id
            ]
            if not matching:
                raise ReleaseError(
                    "explicit rollback target requires an external manifest SHA-256"
                )
            target = matching[0]
        else:
            target = _release_reference(release_id, manifest_sha256)
    return _switch_current(
        root,
        target,
        smoke_command=smoke_command,
        smoke_timeout=smoke_timeout,
        require_existing_state=True,
    )


def _load_role_map(path: Path | None, assignments: list[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    if path is not None:
        value = _read_json(path)
        if not isinstance(value, dict):
            raise ReleaseError("role-map JSON must be an object of path-to-role entries")
        for raw_path, raw_role in value.items():
            if not isinstance(raw_path, str) or not isinstance(raw_role, str):
                raise ReleaseError("role-map keys and values must be strings")
            result[raw_path] = raw_role
    for assignment in assignments:
        if "=" not in assignment:
            raise ReleaseError("--role-for must use PATH=ROLE syntax")
        raw_path, raw_role = assignment.split("=", 1)
        if raw_path in result:
            raise ReleaseError(f"duplicate role assignment for {raw_path!r}")
        result[raw_path] = raw_role
    return result


def _release_dir_from_args(arguments: argparse.Namespace) -> Path:
    if arguments.release_dir is not None:
        if arguments.deployment_root is not None or arguments.release_id is not None:
            raise ReleaseError(
                "--release-dir cannot be combined with --deployment-root/--release-id"
            )
        return arguments.release_dir
    if arguments.deployment_root is None or arguments.release_id is None:
        raise ReleaseError(
            "verify requires --release-dir or both --deployment-root and --release-id"
        )
    release_id = _validate_release_id(arguments.release_id)
    return arguments.deployment_root / "releases" / release_id


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "P8 immutable fixture-local-tooling release manager; "
            "it does not produce or qualify a Windows product package."
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    package = subparsers.add_parser("package", help="create an immutable fixture release")
    package.add_argument("--source", type=Path, required=True)
    package.add_argument(
        "--deployment-root", "--root", dest="deployment_root", type=Path, required=True
    )
    package.add_argument("--release-id", required=True)
    package.add_argument("--role", default="fixture-payload", help="default file role")
    package.add_argument("--role-map", type=Path)
    package.add_argument("--source-manifest", type=Path)
    package.add_argument("--source-manifest-sha256")
    package.add_argument(
        "--role-for", action="append", default=[], metavar="PATH=ROLE"
    )

    verify = subparsers.add_parser("verify", help="verify every release file")
    verify.add_argument("--release-dir", type=Path)
    verify.add_argument("--deployment-root", "--root", dest="deployment_root", type=Path)
    verify.add_argument("--release-id")
    verify.add_argument("--manifest-sha256", required=True)

    for name in ("activate", "rollback"):
        command = subparsers.add_parser(
            name, help=f"verify and atomically {name} a fixture release"
        )
        command.add_argument(
            "--deployment-root", "--root", dest="deployment_root", type=Path, required=True
        )
        command.add_argument("--release-id", required=name == "activate")
        command.add_argument("--manifest-sha256", required=name == "activate")
        command.add_argument(
            "--smoke-command",
            help="quoted command run without a shell from the verified release directory",
        )
        command.add_argument("--smoke-timeout", type=float, default=60.0)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        if arguments.command == "package":
            role_map = _load_role_map(arguments.role_map, arguments.role_for)
            result = package_release(
                arguments.source,
                arguments.deployment_root,
                arguments.release_id,
                default_role=arguments.role,
                role_map=role_map,
                source_manifest=arguments.source_manifest,
                source_manifest_sha256=arguments.source_manifest_sha256,
            )
        elif arguments.command == "verify":
            release_dir = _release_dir_from_args(arguments)
            manifest = verify_release(
                release_dir, arguments.manifest_sha256
            )
            result = {
                "artifactKind": ARTIFACT_KIND,
                "scope": ARTIFACT_SCOPE,
                "releaseId": manifest["releaseId"],
                "releaseDir": str(release_dir),
                "manifestSha256": _normalize_sha256(
                    arguments.manifest_sha256, "manifest SHA-256"
                ),
                "fileCount": len(manifest["files"]),
                "verified": True,
            }
        elif arguments.command == "activate":
            result = activate_release(
                arguments.deployment_root,
                arguments.release_id,
                arguments.manifest_sha256,
                smoke_command=arguments.smoke_command,
                smoke_timeout=arguments.smoke_timeout,
            )
        else:
            result = rollback_release(
                arguments.deployment_root,
                release_id=arguments.release_id,
                manifest_sha256=arguments.manifest_sha256,
                smoke_command=arguments.smoke_command,
                smoke_timeout=arguments.smoke_timeout,
            )
    except ReleaseError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
