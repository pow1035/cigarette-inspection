#!/usr/bin/env python3
"""Generate an immutable strict manifest for a P8 package directory.

The output must be outside the package and must not already exist.  The package
is scanned before and after serialization; success is reported only when the
file set, sizes, and SHA-256 identities remain stable.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import stat
import sys
import unicodedata
from typing import Any


SCHEMA_VERSION = "p8-package-manifest-v1"


class ManifestError(ValueError):
    """Raised when a package cannot be represented safely and deterministically."""


def _is_reparse_point(metadata: os.stat_result) -> bool:
    attributes = getattr(metadata, "st_file_attributes", 0)
    flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return bool(attributes & flag)


def _require_no_reparse_ancestors(path: Path, label: str) -> None:
    current = path
    while True:
        if current.exists() or current.is_symlink():
            try:
                metadata = current.lstat()
            except OSError as exc:
                raise ManifestError(
                    f"{label}: cannot inspect path ancestor {current}: {exc}"
                ) from exc
            if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                raise ManifestError(
                    f"{label}: symbolic-link/reparse-point ancestor is forbidden: "
                    f"{current}"
                )
        if current.parent == current:
            return
        current = current.parent


def _canonical_relative(value: Any) -> str:
    if not isinstance(value, str) or not value or "\\" in value:
        raise ManifestError("package path must be a non-empty POSIX relative path")
    if unicodedata.normalize("NFC", value) != value:
        raise ManifestError(f"package path must use NFC: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise ManifestError(f"unsafe package path: {value!r}")
    canonical = path.as_posix()
    if canonical != value:
        raise ManifestError(f"non-canonical package path: {value!r}")
    return canonical


def _collision_key(value: str) -> str:
    return unicodedata.normalize("NFC", value).casefold()


def _hash_file(path: Path, label: str) -> tuple[int, str]:
    try:
        metadata = path.lstat()
    except OSError as exc:
        raise ManifestError(f"{label}: cannot inspect file: {exc}") from exc
    if (
        stat.S_ISLNK(metadata.st_mode)
        or _is_reparse_point(metadata)
        or not stat.S_ISREG(metadata.st_mode)
    ):
        raise ManifestError(f"{label}: regular non-link file required")
    size = 0
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                size += len(chunk)
                digest.update(chunk)
    except OSError as exc:
        raise ManifestError(f"{label}: cannot read file: {exc}") from exc
    return size, digest.hexdigest()


def _validate_records(records: list[dict[str, Any]]) -> None:
    if not records:
        raise ManifestError("package must contain at least one regular file")
    identities: dict[str, str] = {}
    previous_path: str | None = None
    for index, record in enumerate(records):
        if set(record) != {"path", "size", "sha256"}:
            raise ManifestError(f"package record {index}: keys mismatch")
        relative = _canonical_relative(record["path"])
        size = record["size"]
        digest = record["sha256"]
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise ManifestError(
                f"package record {index}: non-negative integer size required"
            )
        if (
            not isinstance(digest, str)
            or len(digest) != 64
            or any(character not in "0123456789abcdef" for character in digest)
        ):
            raise ManifestError(
                f"package record {index}: lowercase SHA-256 required"
            )
        identity = _collision_key(relative)
        previous = identities.get(identity)
        if previous is not None:
            raise ManifestError(
                "casefold/NFC-conflicting package paths: "
                f"{previous!r} and {relative!r}"
            )
        identities[identity] = relative
        if previous_path is not None and relative <= previous_path:
            raise ManifestError("package records must be strictly path-sorted")
        previous_path = relative


def _scan_package(package: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    identities: dict[str, str] = {}
    try:
        def fail_walk(error: OSError) -> None:
            raise error

        walker = os.walk(package, followlinks=False, onerror=fail_walk)
        for current, directory_names, file_names in walker:
            current_path = Path(current)
            for name in directory_names:
                candidate = current_path / name
                metadata = candidate.lstat()
                if stat.S_ISLNK(metadata.st_mode) or _is_reparse_point(metadata):
                    raise ManifestError(
                        "package contains a symbolic-link/reparse-point directory: "
                        f"{candidate.relative_to(package).as_posix()}"
                    )
                if not stat.S_ISDIR(metadata.st_mode):
                    raise ManifestError(
                        "package contains a non-directory tree entry: "
                        f"{candidate.relative_to(package).as_posix()}"
                    )
            for name in file_names:
                candidate = current_path / name
                relative = _canonical_relative(
                    candidate.relative_to(package).as_posix()
                )
                identity = _collision_key(relative)
                previous = identities.get(identity)
                if previous is not None:
                    raise ManifestError(
                        f"casefold/NFC-conflicting package paths: "
                        f"{previous!r} and {relative!r}"
                    )
                identities[identity] = relative
                size, digest = _hash_file(candidate, f"package file {relative}")
                records.append(
                    {
                        "path": relative,
                        "size": size,
                        "sha256": digest,
                    }
                )
    except OSError as exc:
        raise ManifestError(f"cannot enumerate package: {exc}") from exc
    records.sort(key=lambda record: str(record["path"]))
    _validate_records(records)
    return records


def _manifest_bytes(records: list[dict[str, Any]]) -> bytes:
    return (
        json.dumps(
            {"schemaVersion": SCHEMA_VERSION, "files": records},
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        )
        + "\n"
    ).encode("utf-8")


def _write_new_file(path: Path, raw: bytes) -> None:
    descriptor = -1
    created = False
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        created = True
        with os.fdopen(descriptor, "wb", closefd=True) as stream:
            descriptor = -1
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
    except OSError as exc:
        if created:
            try:
                path.unlink()
            except OSError:
                pass
        raise ManifestError(f"cannot create manifest {path}: {exc}") from exc
    finally:
        if descriptor >= 0:
            os.close(descriptor)


def generate_manifest(
    package: str | os.PathLike[str] | Path,
    output: str | os.PathLike[str] | Path,
) -> dict[str, Any]:
    package_path = Path(os.path.abspath(os.fspath(package)))
    output_path = Path(os.path.abspath(os.fspath(output)))
    _require_no_reparse_ancestors(package_path, "package")
    _require_no_reparse_ancestors(output_path.parent, "manifest output")
    try:
        package_metadata = package_path.lstat()
        output_parent_metadata = output_path.parent.lstat()
    except OSError as exc:
        raise ManifestError(f"cannot inspect package/output paths: {exc}") from exc
    if (
        stat.S_ISLNK(package_metadata.st_mode)
        or _is_reparse_point(package_metadata)
        or not stat.S_ISDIR(package_metadata.st_mode)
    ):
        raise ManifestError("package must be a real directory")
    if (
        stat.S_ISLNK(output_parent_metadata.st_mode)
        or _is_reparse_point(output_parent_metadata)
        or not stat.S_ISDIR(output_parent_metadata.st_mode)
    ):
        raise ManifestError("manifest output parent must be a real directory")
    if output_path == package_path or package_path in output_path.parents:
        raise ManifestError("manifest output must be outside the package")
    if output_path.exists() or output_path.is_symlink():
        raise ManifestError("manifest output must not already exist")

    records = _scan_package(package_path)
    _validate_records(records)
    raw = _manifest_bytes(records)
    if _scan_package(package_path) != records:
        raise ManifestError("package changed while its manifest was generated")
    _write_new_file(output_path, raw)
    try:
        if _scan_package(package_path) != records:
            raise ManifestError("package changed after manifest serialization")
        try:
            actual_raw = output_path.read_bytes()
        except OSError as exc:
            raise ManifestError(
                f"cannot verify created manifest {output_path}: {exc}"
            ) from exc
        if actual_raw != raw:
            raise ManifestError("manifest bytes changed after creation")
    except Exception:
        try:
            output_path.unlink()
        except OSError:
            pass
        raise
    digest = hashlib.sha256(raw).hexdigest()
    return {
        "schemaVersion": SCHEMA_VERSION,
        "manifest": str(output_path),
        "manifestSha256": digest,
        "fileCount": len(records),
        "productAcceptanceClaimed": False,
    }


class StrictArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        self.print_usage(sys.stderr)
        self.exit(3, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = StrictArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    try:
        result = generate_manifest(arguments.package, arguments.output)
    except ManifestError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 3
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
