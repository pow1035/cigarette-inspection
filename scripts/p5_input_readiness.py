#!/usr/bin/env python3
"""Check whether controlled P5 evaluation inputs are present and hash-bound.

This command never modifies controlled inputs.  A fresh Git checkout does not
contain the ignored ``artifacts/`` evidence directories, so it reports missing
external inputs explicitly instead of letting a later evaluation command fail
halfway through or silently downgrade its evidence scope.  ``--output`` may
write a separate readiness report to a caller-selected location.
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
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "p5-input-readiness-v1"
EXPECTED_MODEL_SHA256 = "956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4"
EXPECTED_CLASS_CATALOG_SHA256 = "DDF9415A007246F4CAC0A53BE841EEAD491348E027AC6EA5D5F1A6F13A76C8DC"
REVIEWED_MANIFEST_SCHEMA = "p5-reviewed-truth-evidence-manifest-v1"
FALLBACK_MANIFEST_SCHEMA = "p5-fallback-evidence-v1"
REVIEWED_OUTPUTS = (
    "reviewed-ground-truth.coco.json",
    "approved-pilot-manifest.json",
    "evaluation-predictions.coco.json",
    "ground-truth-attestation.json",
    "promotion-summary.json",
)
SHA256_RE = re.compile(r"^[0-9A-Fa-f]{64}$")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def _symlink_scan_stop(path: Path) -> Path:
    """Return a trusted ancestor at which symlink scanning may stop."""
    absolute = Path(os.path.abspath(path))
    repository = Path(__file__).resolve().parents[1]
    try:
        absolute.relative_to(repository)
    except ValueError:
        # Temporary/custom test roots are kept two levels above the target so
        # both the target and its immediate container are checked without
        # rejecting platform-owned ancestors such as /var on macOS.
        return absolute.parent.parent
    return repository


def _has_symlink_component(path: Path, stop: Path) -> bool:
    current = Path(os.path.abspath(path))
    stop_absolute = Path(os.path.abspath(stop))
    while True:
        if current.is_symlink():
            return True
        if current == stop_absolute or current.parent == current:
            return False
        current = current.parent


def _common_symlink_scan_stop(paths: tuple[Path, ...]) -> Path | None:
    try:
        return Path(os.path.commonpath([os.path.abspath(path) for path in paths]))
    except ValueError:
        return None


def _first_non_directory_ancestor(path: Path, stop: Path) -> Path | None:
    """Return the first existing non-directory that blocks a missing path."""
    current = Path(os.path.abspath(path)).parent
    stop_absolute = Path(os.path.abspath(stop))
    while True:
        if os.path.lexists(current) and not current.is_dir():
            return current
        if current == stop_absolute or current.parent == current:
            return None
        current = current.parent


def _resolve_for_output_guard(path: Path) -> Path:
    try:
        return path.resolve(strict=False)
    except (OSError, RuntimeError) as exc:
        raise ValueError(
            f"readiness output path cannot be resolved safely: {path}: {exc}") from exc


def _path_forms(path: Path) -> tuple[Path, ...]:
    absolute = Path(os.path.abspath(path))
    resolved = _resolve_for_output_guard(absolute)
    return tuple(dict.fromkeys((absolute, resolved)))


def _normalized_path_parts(path: Path) -> tuple[str, ...]:
    """Return a conservative cross-platform filesystem-equivalent spelling."""
    return tuple(
        unicodedata.normalize("NFC", os.path.normcase(part)).casefold()
        for part in Path(os.path.abspath(path)).parts
    )


def _path_within(candidate: Path, root: Path) -> bool:
    candidate_parts = _normalized_path_parts(candidate)
    root_parts = _normalized_path_parts(root)
    return (
        len(candidate_parts) >= len(root_parts)
        and candidate_parts[:len(root_parts)] == root_parts
    )


def _same_existing_path(left: Path, right: Path) -> bool:
    try:
        return os.path.samefile(left, right)
    except (OSError, ValueError):
        return False


def _existing_ancestors(path: Path) -> tuple[Path, ...]:
    ancestors: list[Path] = []
    current = Path(os.path.abspath(path))
    while True:
        if os.path.lexists(current):
            ancestors.append(current)
        if current.parent == current:
            break
        current = current.parent
    return tuple(ancestors)


def _check(
        checks: list[dict[str, Any]], name: str, path: Path,
        *, required: bool, expected_sha256: str | None = None,
        expected_size: int | None = None, reject_symlink: bool = False,
        symlink_stop: Path | None = None, missing_status: str = "missing") -> bool:
    record: dict[str, Any] = {
        "name": name,
        "path": str(path),
        "required": required,
    }
    if expected_sha256:
        record["expected_sha256"] = expected_sha256.upper()
    if expected_size is not None:
        record["expected_size_bytes"] = expected_size
    if reject_symlink and ".." in path.parts:
        record["status"] = "invalid"
        record["detail"] = "parent-directory references are not accepted for controlled inputs"
        checks.append(record)
        return False
    if reject_symlink and (
            path.is_symlink()
            or (symlink_stop is not None and _has_symlink_component(path, symlink_stop))):
        record["status"] = "invalid"
        record["detail"] = "symbolic links are not accepted for controlled evidence"
        checks.append(record)
        return False
    if not path.exists():
        record["status"] = missing_status
        if missing_status != "missing":
            record["detail"] = "required file is absent from an existing input scope"
        checks.append(record)
        return not required and missing_status == "missing"
    if not path.is_file():
        record["status"] = "invalid"
        record["detail"] = "path exists but is not a file"
        checks.append(record)
        return False
    actual_sha256 = sha256_file(path)
    record["actual_sha256"] = actual_sha256
    record["actual_size_bytes"] = path.stat().st_size
    if expected_sha256 and actual_sha256 != expected_sha256.upper():
        record["status"] = "mismatch"
        record["detail"] = "SHA-256 does not match"
        checks.append(record)
        return False
    if expected_size is not None and path.stat().st_size != expected_size:
        record["status"] = "mismatch"
        record["detail"] = "file size does not match"
        checks.append(record)
        return False
    record["status"] = "ok"
    checks.append(record)
    return True


def _check_evidence_directory(
        checks: list[dict[str, Any]], name: str, root: Path,
        *, required: bool, required_outputs: tuple[str, ...] = (),
        expected_schema: str, class_catalog: Path | None = None,
        symlink_stop: Path | None = None) -> bool:
    """Validate an ignored evidence directory without changing it."""
    if ".." in root.parts:
        checks.append({
            "name": name,
            "path": str(root),
            "required": required,
            "status": "invalid",
            "detail": "parent-directory references are not accepted for controlled inputs",
        })
        return False
    if symlink_stop is None:
        symlink_stop = _symlink_scan_stop(root)
    if _has_symlink_component(root, symlink_stop):
        checks.append({
            "name": name,
            "path": str(root),
            "required": required,
            "status": "invalid",
            "detail": "symbolic links are not accepted for controlled evidence",
        })
        return False
    if not root.exists():
        blocking_ancestor = _first_non_directory_ancestor(root, symlink_stop)
        if blocking_ancestor is not None:
            checks.append({
                "name": name,
                "path": str(root),
                "required": required,
                "status": "invalid",
                "detail": (
                    "controlled artifact directory is blocked by a "
                    f"non-directory ancestor: {blocking_ancestor}"
                ),
            })
            return False
        checks.append({
            "name": name,
            "path": str(root),
            "required": required,
            "status": "missing",
            "detail": "controlled artifact directory is absent",
        })
        return not required
    if not root.is_dir():
        checks.append({
            "name": name,
            "path": str(root),
            "required": required,
            "status": "invalid",
            "detail": "artifact path is not a directory",
        })
        return False

    manifest_path = root / "manifest.json"
    if not _check(
            checks, f"{name}.manifest", manifest_path, required=required,
            reject_symlink=True, symlink_stop=symlink_stop,
            missing_status="invalid"):
        return not required
    try:
        manifest = load_json(manifest_path)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        checks.append({
            "name": f"{name}.manifest-schema",
            "path": str(manifest_path),
            "required": required,
            "status": "invalid",
            "detail": str(exc),
        })
        return False

    schema = manifest.get("schema_version")
    if schema != expected_schema:
        checks.append({
            "name": f"{name}.manifest-schema",
            "path": str(manifest_path),
            "required": required,
            "status": "invalid",
            "detail": f"manifest schema_version must be {expected_schema!r}",
        })
        return False
    checks.append({
        "name": f"{name}.manifest-schema",
        "path": str(manifest_path),
        "required": required,
        "status": "ok",
        "schema_version": schema,
    })

    outputs = manifest.get("outputs")
    if not isinstance(outputs, dict) or not outputs:
        checks.append({
            "name": f"{name}.manifest-outputs",
            "path": str(manifest_path),
            "required": required,
            "status": "invalid",
            "detail": "manifest outputs must be a non-empty object",
        })
        return False

    ok = True
    output_names = set(required_outputs) | set(outputs)
    for output_name in sorted(output_names):
        if (not isinstance(output_name, str) or not output_name
                or output_name in {".", ".."}
                or "/" in output_name or "\\" in output_name
                or ":" in output_name or Path(output_name).is_absolute()):
            checks.append({
                "name": f"{name}.output.{output_name}",
                "path": str(root / str(output_name)),
                "required": required,
                "status": "invalid",
                "detail": "manifest output name must be a single relative filename",
            })
            ok = False
            continue
        binding = outputs.get(output_name)
        if not isinstance(binding, dict):
            checks.append({
                "name": f"{name}.output.{output_name}",
                "path": str(root / output_name),
                "required": required,
                "status": "invalid",
                "detail": "manifest output binding must be an object",
            })
            ok = False
            continue
        expected_sha256 = binding.get("sha256")
        expected_size = binding.get("size_bytes")
        if not isinstance(expected_sha256, str) or not SHA256_RE.fullmatch(expected_sha256):
            checks.append({
                "name": f"{name}.output.{output_name}",
                "path": str(root / output_name),
                "required": required,
                "status": "invalid",
                "detail": "manifest output SHA-256 is missing or malformed",
            })
            ok = False
            continue
        if (not isinstance(expected_size, int) or isinstance(expected_size, bool)
                or expected_size < 0):
            checks.append({
                "name": f"{name}.output.{output_name}",
                "path": str(root / output_name),
                "required": required,
                "status": "invalid",
                "detail": "manifest output size_bytes must be a non-negative integer",
            })
            ok = False
            continue
        if not _check(
                checks, f"{name}.output.{output_name}", root / output_name,
                required=required or output_name in required_outputs,
                expected_sha256=expected_sha256, expected_size=expected_size,
                reject_symlink=True, symlink_stop=symlink_stop,
                missing_status="invalid"):
            ok = False

    if name == "reviewed_truth":
        attestation_path = root / "ground-truth-attestation.json"
        if (attestation_path.is_symlink() or not attestation_path.exists()
                or not attestation_path.is_file()):
            # The required-output loop already recorded the precise missing or
            # invalid path classification; avoid adding a second contradictory
            # attestation parse error for the same absent path.
            return False
        try:
            attestation = load_json(attestation_path)
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            checks.append({
                "name": "reviewed_truth.attestation-bindings",
                "path": str(attestation_path),
                "required": required,
                "status": "invalid",
                "detail": str(exc),
            })
            return False
        if attestation.get("schema_version") != "p5-ground-truth-attestation-v1":
            checks.append({
                "name": "reviewed_truth.attestation-schema",
                "path": str(attestation_path),
                "required": required,
                "status": "invalid",
                "detail": "unexpected ground-truth attestation schema_version",
            })
            ok = False
        semantic_fields = {
            "review_method": "human-double-review",
            "authorization_status": "approved",
            "evaluation_split": "pilot",
        }
        for field, expected in semantic_fields.items():
            actual = attestation.get(field)
            field_ok = actual == expected
            checks.append({
                "name": f"reviewed_truth.attestation-{field.replace('_', '-')}",
                "path": str(attestation_path),
                "required": required,
                "status": "ok" if field_ok else "invalid",
                "expected": expected,
                "actual": actual,
            })
            ok = ok and field_ok
        annotated_by = attestation.get("annotated_by")
        reviewed_by = attestation.get("reviewed_by")
        identity_ok = (
            isinstance(annotated_by, str) and bool(annotated_by.strip())
            and isinstance(reviewed_by, str) and bool(reviewed_by.strip())
            and annotated_by.strip() != reviewed_by.strip()
        )
        checks.append({
            "name": "reviewed_truth.attestation-review-identities",
            "path": str(attestation_path),
            "required": required,
            "status": "ok" if identity_ok else "invalid",
            "annotated_by": annotated_by,
            "reviewed_by": reviewed_by,
        })
        ok = ok and identity_ok
        for field in ("reviewed_at", "attested_at"):
            timestamp = attestation.get(field)
            timestamp_ok = False
            if isinstance(timestamp, str) and timestamp.strip():
                try:
                    dt.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
                    timestamp_ok = True
                except ValueError:
                    timestamp_ok = False
            checks.append({
                "name": f"reviewed_truth.attestation-{field.replace('_', '-')}",
                "path": str(attestation_path),
                "required": required,
                "status": "ok" if timestamp_ok else "invalid",
                "actual": timestamp,
            })
            ok = ok and timestamp_ok
        approver = attestation.get("authorization_approved_by")
        approver_ok = isinstance(approver, str) and bool(approver.strip())
        checks.append({
            "name": "reviewed_truth.attestation-authorization-approver",
            "path": str(attestation_path),
            "required": required,
            "status": "ok" if approver_ok else "invalid",
            "actual": approver,
        })
        ok = ok and approver_ok
        source_pass1_hash = attestation.get("source_pass1_sha256")
        source_hash_ok = isinstance(source_pass1_hash, str) and SHA256_RE.fullmatch(source_pass1_hash)
        checks.append({
            "name": "reviewed_truth.attestation-source-pass1",
            "path": str(attestation_path),
            "required": required,
            "status": "ok" if source_hash_ok else "invalid",
            "actual": source_pass1_hash,
        })
        ok = ok and source_hash_ok
        for field, output_name in {
                "ground_truth_sha256": "reviewed-ground-truth.coco.json",
                "manifest_sha256": "approved-pilot-manifest.json",
        }.items():
            attested_hash = attestation.get(field)
            binding = outputs.get(output_name)
            expected_hash = binding.get("sha256") if isinstance(binding, dict) else None
            binding_ok = (
                isinstance(attested_hash, str) and SHA256_RE.fullmatch(attested_hash)
                and isinstance(expected_hash, str) and SHA256_RE.fullmatch(expected_hash)
                and attested_hash.upper() == expected_hash.upper()
            )
            checks.append({
                "name": f"reviewed_truth.attestation-{field.removesuffix('_sha256').replace('_', '-')}",
                "path": str(attestation_path),
                "required": required,
                "status": "ok" if binding_ok else "mismatch",
                "attested_sha256": attested_hash,
                "manifest_output_sha256": expected_hash,
            })
            ok = ok and binding_ok
        attested_catalog = attestation.get("class_catalog_sha256")
        if not isinstance(attested_catalog, str) or not SHA256_RE.fullmatch(attested_catalog):
            checks.append({
                "name": "reviewed_truth.attestation-class-catalog",
                "path": str(attestation_path),
                "required": required,
                "status": "invalid",
                "detail": "attestation class_catalog_sha256 is missing or malformed",
            })
            ok = False
        elif class_catalog is not None and class_catalog.is_file():
            catalog_hash = sha256_file(class_catalog)
            attested_hash = attested_catalog.upper()
            binding_ok = catalog_hash == attested_hash
            checks.append({
                "name": "reviewed_truth.attestation-class-catalog",
                "path": str(class_catalog),
                "required": required,
                "status": "ok" if binding_ok else "mismatch",
                "actual_sha256": catalog_hash,
                "attested_sha256": attested_hash,
            })
            ok = ok and binding_ok
        elif class_catalog is not None:
            checks.append({
                "name": "reviewed_truth.attestation-class-catalog",
                "path": str(class_catalog),
                "required": required,
                "status": "missing",
                "detail": "class catalog is absent",
            })
            ok = False
    return ok


def build_report(
        *, model: Path, class_catalog: Path, reviewed_truth: Path,
        fallback_baseline: Path, require_reviewed: bool,
        require_fallback: bool, expected_model_sha256: str = EXPECTED_MODEL_SHA256,
        expected_class_catalog_sha256: str | None = None) -> dict[str, Any]:
    checks: list[dict[str, Any]] = []
    input_paths = (model, class_catalog, reviewed_truth, fallback_baseline)
    common_symlink_stop = _common_symlink_scan_stop(input_paths)
    _check(
        checks, "model", model, required=True,
        expected_sha256=expected_model_sha256, reject_symlink=True,
        symlink_stop=common_symlink_stop or _symlink_scan_stop(model),
        missing_status="invalid")
    _check(
        checks, "class_catalog", class_catalog, required=True,
        expected_sha256=expected_class_catalog_sha256, reject_symlink=True,
        symlink_stop=common_symlink_stop or _symlink_scan_stop(class_catalog),
        missing_status="invalid")
    _check_evidence_directory(
        checks, "reviewed_truth", reviewed_truth,
        required=require_reviewed, required_outputs=REVIEWED_OUTPUTS,
        expected_schema=REVIEWED_MANIFEST_SCHEMA, class_catalog=class_catalog,
        symlink_stop=common_symlink_stop)
    _check_evidence_directory(
        checks, "fallback_baseline", fallback_baseline,
        required=require_fallback, required_outputs=("evaluation-report.json",),
        expected_schema=FALLBACK_MANIFEST_SCHEMA,
        symlink_stop=common_symlink_stop)

    required_failures = [
        item for item in checks
        if item.get("required") and item.get("status") != "ok"
    ]
    missing = [item for item in required_failures if item.get("status") == "missing"]
    invalid = [item for item in required_failures if item.get("status") != "missing"]
    return {
        "schema_version": SCHEMA_VERSION,
        "ready": not required_failures,
        "manifest_trust": "internal-consistency-only; external evidence-manifest digest must be checked through the controlled recovery channel",
        "requested_scope": {
            "reviewed_truth": require_reviewed,
            "fallback_baseline": require_fallback,
        },
        "checks": checks,
        "missing_required_count": len(missing),
        "invalid_or_mismatched_required_count": len(invalid),
        "missing_required": [item["name"] for item in missing],
        "invalid_or_mismatched_required": [item["name"] for item in invalid],
    }


def write_report(path: Path, report: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def validate_report_output(
        path: Path, *, protected_files: tuple[Path, ...],
        protected_roots: tuple[Path, ...]) -> None:
    if ".." in path.parts:
        raise ValueError("readiness output path must not contain parent-directory references")
    output_forms = _path_forms(path)
    for protected in protected_files:
        protected_forms = _path_forms(protected)
        for output_form in output_forms:
            for protected_form in protected_forms:
                if (
                        _normalized_path_parts(output_form)
                        == _normalized_path_parts(protected_form)
                        or _same_existing_path(output_form, protected_form)):
                    raise ValueError(
                        f"readiness output overlaps controlled input file: {protected}")
    for protected_root in protected_roots:
        root_forms = _path_forms(protected_root)
        for output_form in output_forms:
            for root_form in root_forms:
                if _path_within(output_form, root_form):
                    raise ValueError(
                        "readiness output must be outside controlled artifact root: "
                        f"{protected_root}")
                if _path_within(root_form, output_form):
                    raise ValueError(
                        "readiness output must not occupy an ancestor of controlled "
                        f"artifact root: {protected_root}")

        # Resolved strings do not preserve every filesystem alias.  Existing
        # inode identity closes case-insensitive, Unicode-normalization and
        # mount aliases for roots that are already present.
        for root_form in root_forms:
            if any(
                    _same_existing_path(ancestor, root_form)
                    for output_form in output_forms
                    for ancestor in _existing_ancestors(output_form)):
                raise ValueError(
                    "readiness output must be outside controlled artifact root: "
                    f"{protected_root}")
        for output_form in output_forms:
            if any(
                    _same_existing_path(ancestor, output_form)
                    for root_form in root_forms
                    for ancestor in _existing_ancestors(root_form)):
                raise ValueError(
                    "readiness output must not occupy an ancestor of controlled "
                    f"artifact root: {protected_root}")


def build_parser() -> argparse.ArgumentParser:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--model", type=Path,
        default=repo_root / "03_深度学习模型与TensorRT/模型文件/yanzhi20260120.onnx")
    parser.add_argument(
        "--class-catalog", type=Path,
        default=repo_root / "config/p5-class-catalog.json")
    parser.add_argument(
        "--reviewed-truth-dir", type=Path,
        default=repo_root / "artifacts/p5-reviewed-truth-20260719-124537")
    parser.add_argument(
        "--fallback-baseline-dir", type=Path,
        default=repo_root / "artifacts/p5-fallback-baseline-20260719-161114")
    parser.add_argument("--require-reviewed", action="store_true")
    parser.add_argument("--require-fallback", action="store_true")
    parser.add_argument("--output", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.output:
            validate_report_output(
                args.output,
                protected_files=(args.model, args.class_catalog),
                protected_roots=(args.reviewed_truth_dir, args.fallback_baseline_dir),
            )
        report = build_report(
            model=args.model,
            class_catalog=args.class_catalog,
            reviewed_truth=args.reviewed_truth_dir,
            fallback_baseline=args.fallback_baseline_dir,
            require_reviewed=args.require_reviewed,
            require_fallback=args.require_fallback,
            expected_class_catalog_sha256=EXPECTED_CLASS_CATALOG_SHA256,
        )
        if args.output:
            write_report(args.output, report)
        print(json.dumps({
            "ready": report["ready"],
            "missing_required_count": report["missing_required_count"],
            "invalid_or_mismatched_required_count": report["invalid_or_mismatched_required_count"],
            "missing_required": report["missing_required"],
            "invalid_or_mismatched_required": report["invalid_or_mismatched_required"],
        }, ensure_ascii=False, sort_keys=True))
        if report["invalid_or_mismatched_required_count"]:
            return 3
        if report["missing_required_count"]:
            return 2
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
