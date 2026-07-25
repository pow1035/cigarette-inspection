#!/usr/bin/env python3
"""Adversarial tests for the P8 fixture release transaction tool."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import json
import os
import stat
import tempfile
from types import SimpleNamespace
import unicodedata
import unittest
from unittest import mock
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts" / "p8_release.py"
SPEC = importlib.util.spec_from_file_location("p8_release", MODULE_PATH)
assert SPEC and SPEC.loader
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_source(root: Path, name: str, payload: bytes) -> Path:
    source = root / f"source-{name}"
    (source / "bin").mkdir(parents=True)
    (source / "bin" / "fixture.bin").write_bytes(payload)
    (source / "metadata.txt").write_text(f"release={name}\n", encoding="utf-8")
    return source


def make_writable(path: Path) -> None:
    if not path.exists() or path.is_symlink():
        return
    for directory, directory_names, file_names in os.walk(path, topdown=False):
        for name in file_names:
            os.chmod(Path(directory) / name, 0o600)
        for name in directory_names:
            os.chmod(Path(directory) / name, 0o700)
    os.chmod(path, 0o700)


def rewrite_manifest(release_dir: Path, value: object) -> str:
    make_writable(release_dir)
    path = release_dir / RELEASE.MANIFEST_NAME
    path.write_text(
        json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
        encoding="utf-8",
    )
    return digest(path)


def run_cli(arguments: list[str]) -> tuple[int, str, str]:
    output = io.StringIO()
    error = io.StringIO()
    with redirect_stdout(output), redirect_stderr(error):
        result = RELEASE.main(arguments)
    return result, output.getvalue(), error.getvalue()


class P8ReleaseTests(unittest.TestCase):
    def test_windows_reparse_attribute_is_rejected_by_contract(self):
        self.assertTrue(RELEASE._is_reparse_point(
            SimpleNamespace(st_file_attributes=0x400)))
        self.assertFalse(RELEASE._is_reparse_point(
            SimpleNamespace(st_file_attributes=0)))

    def test_manifest_hash_and_parse_use_one_snapshot_and_detect_late_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            packaged = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A")
            release_dir = deployment / "releases" / "A"
            manifest_path = release_dir / RELEASE.MANIFEST_NAME
            original_reader = RELEASE._read_regular_bytes
            mutated = False

            def read_then_mutate(path: Path) -> bytes:
                nonlocal mutated
                raw = original_reader(path)
                if path == manifest_path and not mutated:
                    mutated = True
                    make_writable(release_dir)
                    value = json.loads(raw)
                    value["artifactKind"] = "tampered-after-hash"
                    manifest_path.write_text(
                        json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")
                return raw

            with mock.patch.object(
                    RELEASE, "_read_regular_bytes", side_effect=read_then_mutate):
                with self.assertRaisesRegex(RELEASE.ReleaseError, "manifest changed"):
                    RELEASE.verify_release(
                        release_dir, packaged["manifestSha256"])

    def test_bound_source_manifest_controls_packaged_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = write_source(root, "A", b"bound")
            source_manifest = root / "package-manifest.json"
            files = []
            for path in sorted(candidate for candidate in source.rglob("*")
                               if candidate.is_file()):
                files.append({
                    "path": path.relative_to(source).as_posix(),
                    "size": path.stat().st_size,
                    "sha256": digest(path),
                })
            source_manifest.write_text(json.dumps({
                "schemaVersion": "p8-package-manifest-v1",
                "files": files,
            }, sort_keys=True) + "\n", encoding="utf-8")
            source_digest = digest(source_manifest)
            result = RELEASE.package_release(
                source, root / "deployment", "A",
                source_manifest=source_manifest,
                source_manifest_sha256=source_digest,
            )
            self.assertEqual(result["sourceManifestSha256"], source_digest)

            (source / "bin" / "fixture.bin").write_bytes(b"changed")
            with self.assertRaisesRegex(RELEASE.ReleaseError, "source changed"):
                RELEASE.package_release(
                    source, root / "deployment", "B",
                    source_manifest=source_manifest,
                    source_manifest_sha256=source_digest,
                )

    def test_package_and_verify_strict_manifest_and_roles(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = write_source(root, "A", b"fixture-a")
            deployment = root / "deployment"
            result = RELEASE.package_release(
                source,
                deployment,
                "A",
                role_map={"bin/fixture.bin": "fixture-engine"},
            )

            self.assertEqual(result["artifactKind"], "fixture-local-tooling")
            self.assertEqual(result["scope"], "fixture-local-tooling-only")
            self.assertFalse(result["idempotent"])
            release_dir = deployment / "releases" / "A"
            manifest = RELEASE.verify_release(
                release_dir, result["manifestSha256"].upper()
            )
            self.assertEqual(manifest["schemaVersion"], "p8-fixture-release-v1")
            self.assertEqual(manifest["artifactKind"], "fixture-local-tooling")
            self.assertEqual(
                [item["path"] for item in manifest["files"]],
                ["bin/fixture.bin", "metadata.txt"],
            )
            self.assertEqual(manifest["files"][0]["role"], "fixture-engine")
            self.assertEqual(manifest["files"][0]["size"], len(b"fixture-a"))
            self.assertEqual(
                manifest["files"][0]["sha256"], hashlib.sha256(b"fixture-a").hexdigest()
            )
            if os.name != "nt":
                self.assertFalse(
                    release_dir.stat().st_mode & stat.S_IWUSR,
                    "published release directory should be read-only",
                )
            make_writable(release_dir)

    def test_verify_rejects_payload_tampering_extra_files_and_wrong_external_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            packaged = RELEASE.package_release(
                write_source(root, "A", b"original"), deployment, "A"
            )
            release_dir = deployment / "releases" / "A"
            with self.assertRaises(RELEASE.ReleaseError):
                RELEASE.verify_release(release_dir, "0" * 64)

            make_writable(release_dir)
            (release_dir / "bin" / "fixture.bin").write_bytes(b"tampered")
            with self.assertRaisesRegex(RELEASE.ReleaseError, "mismatch"):
                RELEASE.verify_release(release_dir, packaged["manifestSha256"])

            (release_dir / "bin" / "fixture.bin").write_bytes(b"original")
            (release_dir / "extra.txt").write_text("untracked", encoding="utf-8")
            with self.assertRaisesRegex(RELEASE.ReleaseError, "file set mismatch"):
                RELEASE.verify_release(release_dir, packaged["manifestSha256"])

    def test_manifest_tampering_requires_matching_external_hash_then_fails_schema(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            packaged = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            release_dir = deployment / "releases" / "A"
            manifest_path = release_dir / RELEASE.MANIFEST_NAME
            make_writable(release_dir)
            value = json.loads(manifest_path.read_text(encoding="utf-8"))
            value["artifactKind"] = "windows-product-package"
            new_hash = rewrite_manifest(release_dir, value)

            with self.assertRaisesRegex(RELEASE.ReleaseError, "manifest SHA-256 mismatch"):
                RELEASE.verify_release(release_dir, packaged["manifestSha256"])
            with self.assertRaisesRegex(RELEASE.ReleaseError, "artifactKind"):
                RELEASE.verify_release(release_dir, new_hash)

    def test_strict_schema_rejects_unknown_and_duplicate_json_keys(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            packaged = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            release_dir = deployment / "releases" / "A"
            manifest_path = release_dir / RELEASE.MANIFEST_NAME
            make_writable(release_dir)
            value = json.loads(manifest_path.read_text(encoding="utf-8"))
            value["windowsProductQualified"] = True
            unknown_hash = rewrite_manifest(release_dir, value)
            with self.assertRaisesRegex(RELEASE.ReleaseError, "exactly"):
                RELEASE.verify_release(release_dir, unknown_hash)

            duplicate = (
                '{"artifactKind":"fixture-local-tooling",'
                '"artifactKind":"fixture-local-tooling",'
                '"files":[],"releaseId":"A",'
                '"schemaVersion":"p8-fixture-release-v1",'
                '"scope":"fixture-local-tooling-only"}\n'
            ).encode()
            manifest_path.write_bytes(duplicate)
            with self.assertRaisesRegex(RELEASE.ReleaseError, "duplicate JSON key"):
                RELEASE.verify_release(
                    release_dir, hashlib.sha256(duplicate).hexdigest()
                )
            self.assertNotEqual(packaged["manifestSha256"], digest(manifest_path))

    def test_activate_b_then_rollback_to_a_is_atomic_and_keeps_b(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            package_a = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            package_b = RELEASE.package_release(
                write_source(root, "B", b"b"), deployment, "B"
            )
            state_a = RELEASE.activate_release(
                deployment, "A", package_a["manifestSha256"]
            )
            self.assertEqual(state_a["current"]["releaseId"], "A")
            self.assertIsNone(state_a["previous"])

            state_b = RELEASE.activate_release(
                deployment, "B", package_b["manifestSha256"]
            )
            self.assertEqual(state_b["current"]["releaseId"], "B")
            self.assertEqual(state_b["previous"]["releaseId"], "A")

            rolled_back = RELEASE.rollback_release(deployment)
            self.assertEqual(rolled_back["current"]["releaseId"], "A")
            self.assertEqual(rolled_back["previous"]["releaseId"], "B")
            self.assertTrue((deployment / "releases" / "A").is_dir())
            self.assertTrue((deployment / "releases" / "B").is_dir())
            disk_state = json.loads(
                (deployment / "state" / "current-release.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(disk_state, rolled_back)
            make_writable(deployment / "releases" / "A")
            make_writable(deployment / "releases" / "B")

    def test_smoke_failure_preserves_current_state_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            package_a = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            package_b = RELEASE.package_release(
                write_source(root, "B", b"b"), deployment, "B"
            )
            RELEASE.activate_release(deployment, "A", package_a["manifestSha256"])
            state_path = deployment / "state" / "current-release.json"
            old_bytes = state_path.read_bytes()

            with self.assertRaisesRegex(RELEASE.ReleaseError, "smoke"):
                RELEASE.activate_release(
                    deployment,
                    "B",
                    package_b["manifestSha256"],
                    smoke_command=lambda _: False,
                )
            self.assertEqual(state_path.read_bytes(), old_bytes)
            self.assertEqual(
                RELEASE.read_current_state(deployment)["current"]["releaseId"], "A"
            )

            with self.assertRaisesRegex(RELEASE.ReleaseError, "smoke"):
                RELEASE.rollback_release(
                    deployment,
                    release_id="B",
                    manifest_sha256=package_b["manifestSha256"],
                    smoke_command=lambda _: 7,
                )
            self.assertEqual(state_path.read_bytes(), old_bytes)
            make_writable(deployment / "releases" / "A")
            make_writable(deployment / "releases" / "B")

    def test_integrity_failure_preserves_current_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            package_a = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            package_b = RELEASE.package_release(
                write_source(root, "B", b"b"), deployment, "B"
            )
            RELEASE.activate_release(deployment, "A", package_a["manifestSha256"])
            state_path = deployment / "state" / "current-release.json"
            old_bytes = state_path.read_bytes()
            make_writable(deployment / "releases" / "B")
            (deployment / "releases" / "B" / "bin" / "fixture.bin").write_bytes(
                b"tampered"
            )
            with self.assertRaisesRegex(RELEASE.ReleaseError, "mismatch"):
                RELEASE.activate_release(
                    deployment, "B", package_b["manifestSha256"]
                )
            self.assertEqual(state_path.read_bytes(), old_bytes)
            make_writable(deployment / "releases" / "A")

    def test_paths_reject_parent_absolute_windows_and_noncanonical_forms(self):
        valid_file = {
            "path": "ok/file.bin",
            "role": "fixture-payload",
            "size": 1,
            "sha256": hashlib.sha256(b"x").hexdigest(),
        }

        def manifest(path: str) -> dict:
            item = dict(valid_file)
            item["path"] = path
            return {
                "schemaVersion": "p8-fixture-release-v1",
                "artifactKind": "fixture-local-tooling",
                "scope": "fixture-local-tooling-only",
                "releaseId": "A",
                "files": [item],
            }

        for attack in (
            "../outside.bin",
            "dir/../outside.bin",
            "/absolute.bin",
            r"C:\absolute.bin",
            r"dir\file.bin",
            "dir//file.bin",
            "./file.bin",
            "file.bin/",
        ):
            with self.subTest(path=attack):
                with self.assertRaises(RELEASE.ReleaseError):
                    RELEASE.validate_manifest(manifest(attack))

        decomposed = unicodedata.normalize("NFD", "café.bin")
        if decomposed != "café.bin":
            with self.assertRaisesRegex(RELEASE.ReleaseError, "NFC"):
                RELEASE.validate_manifest(manifest(decomposed))

    def test_casefold_and_nfc_collisions_are_rejected(self):
        base = {
            "role": "fixture-payload",
            "size": 1,
            "sha256": hashlib.sha256(b"x").hexdigest(),
        }
        value = {
            "schemaVersion": "p8-fixture-release-v1",
            "artifactKind": "fixture-local-tooling",
            "scope": "fixture-local-tooling-only",
            "releaseId": "A",
            "files": [
                {"path": "Dir/a.bin", **base},
                {"path": "dir/b.bin", **base},
            ],
        }
        with self.assertRaisesRegex(RELEASE.ReleaseError, "collision"):
            RELEASE.validate_manifest(value)

        value["files"] = [
            {"path": "Straße.bin", **base},
            {"path": "STRASSE.bin", **base},
        ]
        with self.assertRaisesRegex(RELEASE.ReleaseError, "collision"):
            RELEASE.validate_manifest(value)

    @unittest.skipUnless(hasattr(os, "symlink"), "symlink support required")
    def test_package_and_verify_reject_symlinks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = write_source(root, "A", b"a")
            outside = root / "outside.bin"
            outside.write_bytes(b"outside")
            try:
                os.symlink(outside, source / "link.bin")
            except OSError as exc:
                self.skipTest(f"cannot create symlink: {exc}")
            with self.assertRaisesRegex(RELEASE.ReleaseError, "symlink"):
                RELEASE.package_release(source, root / "deployment", "A")

            (source / "link.bin").unlink()
            packaged = RELEASE.package_release(source, root / "deployment", "A")
            release_dir = root / "deployment" / "releases" / "A"
            make_writable(release_dir)
            payload = release_dir / "bin" / "fixture.bin"
            payload.unlink()
            os.symlink(outside, payload)
            with self.assertRaisesRegex(RELEASE.ReleaseError, "symlink"):
                RELEASE.verify_release(release_dir, packaged["manifestSha256"])

    def test_package_and_activate_are_idempotent_but_release_id_cannot_mutate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = write_source(root, "A", b"a")
            deployment = root / "deployment"
            first = RELEASE.package_release(source, deployment, "A")
            manifest_path = deployment / "releases" / "A" / RELEASE.MANIFEST_NAME
            manifest_bytes = manifest_path.read_bytes()
            second = RELEASE.package_release(source, deployment, "A")
            self.assertTrue(second["idempotent"])
            self.assertEqual(first["manifestSha256"], second["manifestSha256"])
            self.assertEqual(manifest_path.read_bytes(), manifest_bytes)

            first_state = RELEASE.activate_release(
                deployment, "A", first["manifestSha256"]
            )
            state_path = deployment / "state" / "current-release.json"
            state_bytes = state_path.read_bytes()
            second_state = RELEASE.activate_release(
                deployment, "A", first["manifestSha256"]
            )
            self.assertEqual(first_state, second_state)
            self.assertEqual(state_path.read_bytes(), state_bytes)

            make_writable(deployment / "releases" / "A")
            (source / "bin" / "fixture.bin").write_bytes(b"different")
            with self.assertRaises(RELEASE.ReleaseError):
                RELEASE.package_release(source, deployment, "A")
            self.assertEqual(manifest_path.read_bytes(), manifest_bytes)

    def test_shared_directories_are_never_overwritten_or_deleted(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            deployment.mkdir()
            sentinels: dict[str, bytes] = {}
            for name in RELEASE.SHARED_DIRECTORY_NAMES:
                path = deployment / name / "owner-content.bin"
                path.parent.mkdir()
                content = f"owned-{name}".encode()
                path.write_bytes(content)
                sentinels[name] = content

            package_a = RELEASE.package_release(
                write_source(root, "A", b"a"), deployment, "A"
            )
            package_b = RELEASE.package_release(
                write_source(root, "B", b"b"), deployment, "B"
            )
            RELEASE.activate_release(deployment, "A", package_a["manifestSha256"])
            RELEASE.activate_release(deployment, "B", package_b["manifestSha256"])
            RELEASE.rollback_release(deployment)

            for name, content in sentinels.items():
                self.assertEqual(
                    (deployment / name / "owner-content.bin").read_bytes(), content
                )
            make_writable(deployment / "releases" / "A")
            make_writable(deployment / "releases" / "B")

    def test_release_path_attacks_and_case_colliding_release_ids_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = write_source(root, "A", b"a")
            deployment = root / "deployment"
            for attack in ("../A", "/tmp/A", r"C:\A", ".", "a/b"):
                with self.subTest(release_id=attack):
                    with self.assertRaises(RELEASE.ReleaseError):
                        RELEASE.package_release(source, deployment, attack)

            packaged = RELEASE.package_release(source, deployment, "ReleaseA")
            with self.assertRaisesRegex(RELEASE.ReleaseError, "collision"):
                RELEASE.package_release(source, deployment, "releasea")
            RELEASE.verify_release(
                deployment / "releases" / "ReleaseA",
                packaged["manifestSha256"],
            )
            make_writable(deployment / "releases" / "ReleaseA")

    def test_cli_supports_all_four_commands(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            deployment = root / "deployment"
            source_a = write_source(root, "A", b"a")
            source_b = write_source(root, "B", b"b")

            status, output, error = run_cli(
                [
                    "package",
                    "--source",
                    str(source_a),
                    "--root",
                    str(deployment),
                    "--release-id",
                    "A",
                ]
            )
            self.assertEqual((status, error), (0, ""))
            package_a = json.loads(output)

            status, output, error = run_cli(
                [
                    "verify",
                    "--release-dir",
                    str(deployment / "releases" / "A"),
                    "--manifest-sha256",
                    package_a["manifestSha256"],
                ]
            )
            self.assertEqual((status, error), (0, ""))
            self.assertTrue(json.loads(output)["verified"])

            status, output, error = run_cli(
                [
                    "activate",
                    "--root",
                    str(deployment),
                    "--release-id",
                    "A",
                    "--manifest-sha256",
                    package_a["manifestSha256"],
                ]
            )
            self.assertEqual((status, error), (0, ""))
            self.assertEqual(json.loads(output)["current"]["releaseId"], "A")

            package_b = RELEASE.package_release(source_b, deployment, "B")
            RELEASE.activate_release(deployment, "B", package_b["manifestSha256"])
            status, output, error = run_cli(
                ["rollback", "--root", str(deployment)]
            )
            self.assertEqual((status, error), (0, ""))
            self.assertEqual(json.loads(output)["current"]["releaseId"], "A")
            make_writable(deployment / "releases" / "A")
            make_writable(deployment / "releases" / "B")


if __name__ == "__main__":
    unittest.main()
