#!/usr/bin/env python3
"""Verify an externally supplied SHA-256 for a recovered P5 evidence file."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


SCHEMA_VERSION = "p5-external-digest-verification-v1"
HEX64 = re.compile(r"^[0-9a-fA-F]{64}$")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify(path: Path, expected: str) -> tuple[int, dict[str, object]]:
    report: dict[str, object] = {
        "schema_version": SCHEMA_VERSION,
        "path": str(path),
        "expected_sha256": expected.lower(),
    }
    if not HEX64.fullmatch(expected):
        report.update({"status": "invalid-expected-digest", "detail": "expected digest must be 64 hexadecimal characters"})
        return 3, report
    if not path.exists():
        report.update({"status": "missing", "detail": "digest target does not exist"})
        return 3, report
    if not path.is_file():
        report.update({"status": "invalid", "detail": "digest target is not a regular file"})
        return 3, report
    actual = sha256_file(path)
    report["actual_sha256"] = actual
    if actual != expected.lower():
        report.update({"status": "mismatch", "detail": "SHA-256 does not match the external recovery record"})
        return 2, report
    report["status"] = "match"
    return 0, report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", type=Path, required=True, help="recovered evidence-manifest or other bound file")
    parser.add_argument("--expected-sha256", required=True, help="digest supplied through the controlled recovery channel")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    code, report = verify(args.file, args.expected_sha256.strip())
    payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8", newline="\n")
    print(payload, end="")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
