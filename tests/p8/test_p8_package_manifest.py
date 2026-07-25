#!/usr/bin/env python3
"""Tests for strict P8 package manifest generation."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import json
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "p8_package_manifest.py"
SPEC = importlib.util.spec_from_file_location("p8_package_manifest", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MANIFEST = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MANIFEST)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class P8PackageManifestTests(unittest.TestCase):
    def make_package(self, root: Path) -> Path:
        package = root / "package"
        (package / "bin").mkdir(parents=True)
        (package / "config.ini").write_text(
            "[General]\nCurrentBrand=品牌A\n\n"
            "[SystemParams]\nrejectEnabled=false\npulseCount=2\n",
            encoding="utf-8",
        )
        (package / "bin/CigVision.exe").write_bytes(b"fixture")
        return package

    def test_success_is_deterministic_and_binds_every_file(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            output = root / "package-manifest.json"
            result = MANIFEST.generate_manifest(package, output)
            document = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(
                document["schemaVersion"], "p8-package-manifest-v1"
            )
            self.assertEqual(result["manifestSha256"], sha256(output))
            self.assertEqual(result["fileCount"], 2)
            self.assertFalse(result["productAcceptanceClaimed"])
            self.assertEqual(
                [record["path"] for record in document["files"]],
                ["bin/CigVision.exe", "config.ini"],
            )
            for record in document["files"]:
                path = package / record["path"]
                self.assertEqual(record["size"], path.stat().st_size)
                self.assertEqual(record["sha256"], sha256(path))

    def test_existing_or_inside_package_output_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            existing = root / "existing.json"
            existing.write_text("preserve", encoding="utf-8")
            with self.assertRaisesRegex(
                MANIFEST.ManifestError, "must not already exist"
            ):
                MANIFEST.generate_manifest(package, existing)
            self.assertEqual(existing.read_text(encoding="utf-8"), "preserve")
            with self.assertRaisesRegex(
                MANIFEST.ManifestError, "outside the package"
            ):
                MANIFEST.generate_manifest(
                    package, package / "package-manifest.json"
                )

    def test_empty_and_symlinked_package_entries_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            empty = root / "empty"
            empty.mkdir()
            with self.assertRaisesRegex(
                MANIFEST.ManifestError, "at least one regular file"
            ):
                MANIFEST.generate_manifest(empty, root / "empty.json")

            package = self.make_package(root)
            try:
                (package / "linked.bin").symlink_to(package / "config.ini")
            except OSError as exc:
                self.skipTest(f"symbolic links unavailable: {exc}")
            with self.assertRaisesRegex(
                MANIFEST.ManifestError, "regular non-link file"
            ):
                MANIFEST.generate_manifest(package, root / "linked.json")

    def test_casefold_conflict_and_non_nfc_path_are_rejected(self) -> None:
        with self.assertRaisesRegex(
            MANIFEST.ManifestError, "must use NFC"
        ):
            MANIFEST._canonical_relative("e\u0301.txt")
        records = [
            {"path": "A.txt", "size": 1, "sha256": "0" * 64},
            {"path": "a.txt", "size": 1, "sha256": "1" * 64},
        ]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            with mock.patch.object(
                MANIFEST,
                "_scan_package",
                return_value=records,
            ):
                with self.assertRaisesRegex(
                    MANIFEST.ManifestError, "casefold/NFC-conflicting"
                ):
                    MANIFEST.generate_manifest(package, root / "conflict.json")

    def test_package_mutation_between_scans_leaves_no_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            output = root / "mutated.json"
            first = MANIFEST._scan_package(package)
            second = [dict(record) for record in first]
            second[0]["size"] += 1
            with mock.patch.object(
                MANIFEST,
                "_scan_package",
                side_effect=[first, second],
            ):
                with self.assertRaisesRegex(
                    MANIFEST.ManifestError, "changed while"
                ):
                    MANIFEST.generate_manifest(package, output)
            self.assertFalse(output.exists())

            denied = root / "denied.json"

            def denied_walk(*args, **kwargs):
                kwargs["onerror"](PermissionError("denied"))
                return iter(())

            with mock.patch.object(
                MANIFEST.os, "walk", side_effect=denied_walk
            ):
                with self.assertRaisesRegex(
                    MANIFEST.ManifestError, "cannot enumerate package"
                ):
                    MANIFEST.generate_manifest(package, denied)
            self.assertFalse(denied.exists())

    def test_windows_reparse_contract_and_symlinked_output_parent(self) -> None:
        self.assertTrue(
            MANIFEST._is_reparse_point(
                SimpleNamespace(st_file_attributes=0x400)
            )
        )
        self.assertFalse(
            MANIFEST._is_reparse_point(
                SimpleNamespace(st_file_attributes=0)
            )
        )
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            real = root / "real"
            real.mkdir()
            linked = root / "linked"
            try:
                linked.symlink_to(real, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symbolic links unavailable: {exc}")
            with self.assertRaisesRegex(
                MANIFEST.ManifestError,
                "symbolic-link/reparse-point ancestor",
            ):
                MANIFEST.generate_manifest(package, linked / "manifest.json")

    def test_cli_success_and_invalid_exit_codes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            package = self.make_package(root)
            output = root / "manifest.json"
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                status = MANIFEST.main(
                    ["--package", str(package), "--output", str(output)]
                )
            self.assertEqual(status, 0, stderr.getvalue())
            self.assertEqual(
                json.loads(stdout.getvalue())["manifestSha256"],
                sha256(output),
            )
            with redirect_stdout(io.StringIO()), redirect_stderr(stderr):
                status = MANIFEST.main(
                    ["--package", str(package), "--output", str(output)]
                )
            self.assertEqual(status, 3)


if __name__ == "__main__":
    unittest.main()
