#!/usr/bin/env python3
"""Tests for the strict P8 local package preflight."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
import unicodedata
import importlib.util


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY_ROOT / "scripts" / "p8_preflight.py"
GATE_CONFIG = REPOSITORY_ROOT / "config" / "p8-local-gates-v1.json"
HOST_COLLECTOR = (
    REPOSITORY_ROOT / "scripts" / "collect_windows_p8_host_reports.ps1"
)
REPORT_NAME = "p8-preflight-report.json"
SPEC = importlib.util.spec_from_file_location("p8_preflight", SCRIPT)
assert SPEC and SPEC.loader
PREFLIGHT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREFLIGHT)


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(65536)
            if not block:
                return digest.hexdigest()
            digest.update(block)


class P8PreflightTests(unittest.TestCase):
    def test_windows_reparse_attribute_is_rejected_by_contract(self) -> None:
        self.assertTrue(PREFLIGHT._is_reparse_point(
            SimpleNamespace(st_file_attributes=0x400)))
        self.assertFalse(PREFLIGHT._is_reparse_point(
            SimpleNamespace(st_file_attributes=0)))

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.package = self.root / "package"
        self.output = self.root / "output"
        self.manifest = self.root / "package-manifest.json"
        self.windows_input = self.root / "windows-evidence.json"
        self.gpu_input = self.root / "gpu-evidence.json"

        (self.package / "品牌设置" / "品牌A" / "模版图片").mkdir(parents=True)
        (self.package / "bin").mkdir()
        (self.package / "config.ini").write_text(
            "[General]\n"
            "CurrentBrand=品牌A\n"
            "Camera1=1\n"
            "\n"
            "[SystemParams]\n"
            "rejectEnabled=false\n"
            "pulseCount=2\n",
            encoding="utf-8",
        )
        (self.package / "品牌设置" / "品牌A" / "para.ini").write_text(
            "[DeepLearningParams]\n"
            "jointRollThreshold=0.5\n",
            encoding="utf-8",
        )
        (self.package / "品牌设置" / "品牌A" / "模版图片" / "1.jpg").write_bytes(
            b"fixture-template"
        )
        (self.package / "bin" / "CigVision.exe").write_bytes(b"fixture-executable")
        self.capture_id = "1" * 32
        self.host_id = "2" * 64
        self.host_challenge = "3" * 64
        self.captured_at = "2026-07-26T00:00:00.0000000Z"
        self.package_manifest_sha256 = self.write_manifest()
        self.write_host_reports()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def manifest_document(self) -> dict[str, object]:
        files = []
        for path in sorted(candidate for candidate in self.package.rglob("*")
                           if candidate.is_file() and not candidate.is_symlink()):
            relative = path.relative_to(self.package).as_posix()
            files.append(
                {
                    "path": relative,
                    "size": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
        return {"schemaVersion": "p8-package-manifest-v1", "files": files}

    def host_report(self, kind: str) -> dict[str, object]:
        return {
            "schemaVersion": PREFLIGHT.HOST_REPORT_SCHEMAS[kind],
            "captureId": self.capture_id,
            "capturedAtUtc": self.captured_at,
            "hostIdSha256": self.host_id,
            "challenge": self.host_challenge,
            "packageManifestSha256": self.package_manifest_sha256,
            "collector": {
                "schemaVersion": PREFLIGHT.HOST_COLLECTOR_SCHEMA,
                "repositoryPath": PREFLIGHT.HOST_COLLECTOR_REPOSITORY_PATH,
                "sha256": sha256_file(HOST_COLLECTOR),
            },
            "checks": [
                {"id": check_id, "status": "passed", "detail": "fixture passed"}
                for check_id in sorted(PREFLIGHT.HOST_REQUIRED_CHECKS[kind])
            ],
            "claims": {
                "collectionComplete": True,
                "productAcceptance": False,
                "windowsRuntimeAccepted": False,
                "gpuRuntimeAccepted": False,
                "realIoTested": False,
                "realRejectTested": False,
            },
        }

    def write_host_reports(
        self,
        windows: dict[str, object] | None = None,
        gpu: dict[str, object] | None = None,
    ) -> None:
        for path, document in (
            (
                self.windows_input,
                windows if windows is not None else self.host_report("windows"),
            ),
            (
                self.gpu_input,
                gpu if gpu is not None else self.host_report("gpu"),
            ),
        ):
            path.write_text(
                json.dumps(
                    document,
                    ensure_ascii=False,
                    sort_keys=True,
                    separators=(",", ":"),
                    allow_nan=False,
                )
                + "\n",
                encoding="utf-8",
            )

    def write_manifest(
        self,
        document: dict[str, object] | None = None,
        raw: bytes | None = None,
    ) -> str:
        if raw is None:
            raw = (
                json.dumps(
                    document if document is not None else self.manifest_document(),
                    ensure_ascii=False,
                    sort_keys=True,
                    separators=(",", ":"),
                    allow_nan=False,
                )
                + "\n"
            ).encode("utf-8")
        self.manifest.write_bytes(raw)
        return sha256_bytes(raw)

    def run_preflight(
        self,
        *,
        manifest_hash: str | None = None,
        include_windows: bool = True,
        include_gpu: bool = True,
        output: Path | None = None,
    ) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable,
            str(SCRIPT),
            "--gate-config",
            str(GATE_CONFIG),
            "--package",
            str(self.package),
            "--manifest",
            str(self.manifest),
            "--manifest-sha256",
            manifest_hash or sha256_file(self.manifest),
            "--host-challenge",
            self.host_challenge,
            "--output-dir",
            str(output or self.output),
        ]
        if include_windows:
            command.extend(["--windows-input", str(self.windows_input)])
        if include_gpu:
            command.extend(["--gpu-input", str(self.gpu_input)])
        environment = os.environ.copy()
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        return subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=environment,
            check=False,
        )

    def read_report(self) -> dict[str, object]:
        return json.loads((self.output / REPORT_NAME).read_text(encoding="utf-8"))

    def assert_invalid(self, result: subprocess.CompletedProcess[str]) -> None:
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        report = self.read_report()
        self.assertEqual(report["status"], "invalid")
        self.assertFalse(report["productAcceptance"])

    def test_success_is_local_ready_without_product_acceptance_claim(self) -> None:
        result = self.run_preflight()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = self.read_report()
        self.assertEqual(report["status"], "ready")
        self.assertEqual(report["exitCode"], 0)
        self.assertEqual(report["scope"], "local-tooling-preflight-only")
        self.assertFalse(report["productAcceptance"])
        self.assertTrue(report["safety"]["localOnly"])
        self.assertFalse(report["safety"]["realIoEnabled"])
        self.assertFalse(report["safety"]["realRejectEnabled"])
        self.assertFalse(report["claims"]["p8ProductAccepted"])
        checks = {check["name"]: check for check in report["checks"]}
        self.assertTrue(
            checks["external.windows"]["collectorAssertionsValidated"]
        )
        self.assertTrue(checks["external.gpu"]["collectorAssertionsValidated"])
        self.assertIn("external.host-report-pair", checks)
        self.assertFalse(
            checks["external.host-report-pair"]["productAcceptanceChecked"]
        )

    def test_missing_external_windows_or_gpu_input_returns_2(self) -> None:
        for include_windows, include_gpu, expected in (
            (False, True, "external.windows"),
            (True, False, "external.gpu"),
            (False, False, "external.windows"),
        ):
            with self.subTest(windows=include_windows, gpu=include_gpu):
                if self.output.exists():
                    for child in self.output.iterdir():
                        child.unlink()
                result = self.run_preflight(
                    include_windows=include_windows,
                    include_gpu=include_gpu,
                )
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                report = self.read_report()
                self.assertEqual(report["status"], "external-inputs-missing")
                self.assertIn(expected, report["missingExternalInputs"])
                self.assertFalse(report["productAcceptance"])

    def test_package_hash_tamper_is_rejected(self) -> None:
        target = self.package / "bin" / "CigVision.exe"
        original = target.read_bytes()
        target.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        self.assert_invalid(self.run_preflight())

    def test_external_manifest_sha256_mismatch_is_rejected(self) -> None:
        self.assert_invalid(self.run_preflight(manifest_hash="0" * 64))

    def test_manifest_duplicate_key_and_nonfinite_numbers_are_rejected(self) -> None:
        valid_files = json.dumps(
            self.manifest_document()["files"],
            ensure_ascii=False,
            separators=(",", ":"),
        )
        cases = {
            "duplicate": (
                '{"schemaVersion":"p8-package-manifest-v1",'
                '"schemaVersion":"p8-package-manifest-v1",'
                f'"files":{valid_files}}}\n'
            ).encode("utf-8"),
            "NaN": (
                '{"schemaVersion":"p8-package-manifest-v1",'
                f'"files":{valid_files},"unexpected":NaN}}\n'
            ).encode("utf-8"),
            "Infinity": (
                '{"schemaVersion":"p8-package-manifest-v1",'
                f'"files":{valid_files},"unexpected":Infinity}}\n'
            ).encode("utf-8"),
        }
        for name, raw in cases.items():
            with self.subTest(case=name):
                self.write_manifest(raw=raw)
                self.assert_invalid(self.run_preflight())

    def test_path_traversal_is_rejected(self) -> None:
        document = self.manifest_document()
        document["files"].append(
            {
                "path": "../escape.bin",
                "size": 0,
                "sha256": hashlib.sha256(b"").hexdigest(),
            }
        )
        self.write_manifest(document)
        self.assert_invalid(self.run_preflight())

    def test_symlink_in_package_is_rejected(self) -> None:
        link = self.package / "linked.exe"
        try:
            link.symlink_to(self.package / "bin" / "CigVision.exe")
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"symlinks unavailable: {exc}")
        document = self.manifest_document()
        document["files"].append(
            {
                "path": "linked.exe",
                "size": (self.package / "bin" / "CigVision.exe").stat().st_size,
                "sha256": sha256_file(self.package / "bin" / "CigVision.exe"),
            }
        )
        self.write_manifest(document)
        self.assert_invalid(self.run_preflight())

    def test_broken_external_symlink_is_invalid_not_missing(self) -> None:
        self.windows_input.unlink()
        try:
            self.windows_input.symlink_to(self.root / "missing-windows-evidence.json")
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"symlinks unavailable: {exc}")
        self.assert_invalid(self.run_preflight())

    def test_host_report_requires_exact_schema_and_strict_json(self) -> None:
        cases = {
            "unknown-top-level-key": lambda document: document.update(
                {"unexpected": True}
            ),
            "wrong-schema": lambda document: document.update(
                {"schemaVersion": "p8-windows-host-report-v0"}
            ),
            "uppercase-host-id": lambda document: document.update(
                {"hostIdSha256": "A" * 64}
            ),
            "non-utc-time": lambda document: document.update(
                {"capturedAtUtc": "2026-07-26T08:00:00+08:00"}
            ),
            "wrong-challenge": lambda document: document.update(
                {"challenge": "4" * 64}
            ),
            "wrong-package": lambda document: document.update(
                {"packageManifestSha256": "5" * 64}
            ),
        }
        for name, mutate in cases.items():
            with self.subTest(case=name):
                document = self.host_report("windows")
                mutate(document)
                self.write_host_reports(windows=document)
                self.assert_invalid(self.run_preflight())

        self.windows_input.write_bytes(
            b'{"schemaVersion":"p8-windows-host-report-v2",'
            b'"schemaVersion":"p8-windows-host-report-v2"}\n'
        )
        self.assert_invalid(self.run_preflight())

    def test_host_report_requires_exact_passed_check_set(self) -> None:
        cases: dict[str, object] = {}
        missing = self.host_report("windows")
        missing["checks"] = missing["checks"][1:]
        cases["missing"] = missing
        duplicate = self.host_report("windows")
        duplicate["checks"].append(dict(duplicate["checks"][0]))
        cases["duplicate"] = duplicate
        failed = self.host_report("windows")
        failed["checks"][0]["status"] = "failed"
        failed["claims"]["collectionComplete"] = False
        cases["failed"] = failed
        unknown = self.host_report("windows")
        unknown["checks"][0]["id"] = "dependency.unknown"
        cases["unknown"] = unknown
        for name, document in cases.items():
            with self.subTest(case=name):
                self.write_host_reports(windows=document)
                self.assert_invalid(self.run_preflight())

    def test_host_report_pair_must_share_capture_and_host_identity(self) -> None:
        for field, value in (
            ("captureId", "3" * 32),
            ("capturedAtUtc", "2026-07-26T00:00:01Z"),
            ("hostIdSha256", "4" * 64),
            ("challenge", "5" * 64),
            ("packageManifestSha256", "6" * 64),
        ):
            with self.subTest(field=field):
                gpu = self.host_report("gpu")
                gpu[field] = value
                self.write_host_reports(gpu=gpu)
                self.assert_invalid(self.run_preflight())

    def test_host_report_collector_identity_is_trusted_and_exact(self) -> None:
        for field, value in (
            ("schemaVersion", "cigvision-p8-host-collector-v0"),
            ("repositoryPath", "scripts/untrusted.ps1"),
            ("sha256", "0" * 64),
        ):
            with self.subTest(field=field):
                windows = self.host_report("windows")
                windows["collector"][field] = value
                self.write_host_reports(windows=windows)
                self.assert_invalid(self.run_preflight())

    def test_host_report_cannot_claim_runtime_or_product_acceptance(self) -> None:
        for claim in (
            "productAcceptance",
            "windowsRuntimeAccepted",
            "gpuRuntimeAccepted",
            "realIoTested",
            "realRejectTested",
        ):
            with self.subTest(claim=claim):
                windows = self.host_report("windows")
                windows["claims"][claim] = True
                self.write_host_reports(windows=windows)
                self.assert_invalid(self.run_preflight())

    def test_casefold_and_nfc_manifest_path_conflicts_are_rejected(self) -> None:
        base = self.manifest_document()
        source = base["files"][0]
        conflict_pairs = (
            ("Case.bin", "case.bin"),
            (
                unicodedata.normalize("NFC", "café.bin"),
                unicodedata.normalize("NFD", "café.bin"),
            ),
        )
        for first, second in conflict_pairs:
            with self.subTest(first=first, second=second):
                document = {
                    "schemaVersion": "p8-package-manifest-v1",
                    "files": [
                        *base["files"],
                        {
                            "path": first,
                            "size": source["size"],
                            "sha256": source["sha256"],
                        },
                        {
                            "path": second,
                            "size": source["size"],
                            "sha256": source["sha256"],
                        },
                    ],
                }
                self.write_manifest(document)
                self.assert_invalid(self.run_preflight())

    def test_reject_enabled_true_is_rejected(self) -> None:
        config_path = self.package / "config.ini"
        config_path.write_text(
            config_path.read_text(encoding="utf-8").replace(
                "rejectEnabled=false", "rejectEnabled=true"
            ),
            encoding="utf-8",
        )
        self.write_manifest()
        self.assert_invalid(self.run_preflight())

    def test_output_directory_overlap_is_rejected_without_writing_report(self) -> None:
        overlapping_output = self.package / "preflight-output"
        result = self.run_preflight(output=overlapping_output)
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        self.assertFalse((overlapping_output / REPORT_NAME).exists())
        error = json.loads(result.stderr)
        self.assertFalse(error["productAcceptance"])
        self.assertFalse(error["reportWritten"])


if __name__ == "__main__":
    unittest.main()
