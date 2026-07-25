#!/usr/bin/env python3
"""Tests for portable P8 Windows evidence verification."""

from __future__ import annotations

import hashlib
import hmac
import importlib.util
import io
import json
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "p8_windows_evidence_verify.py"
SPEC = importlib.util.spec_from_file_location("p8_windows_evidence_verify", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


def digest(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


class EvidenceFixture:
    def __init__(self, root: Path, *, rollback: bool = False) -> None:
        self.root = root
        self.rollback = rollback
        self.package_digest = "1" * 64
        self.release_digest = "2" * 64
        self.release_id = "fixture-20260725-120000"
        self.host_challenge = "c" * 64
        self.evidence_key = bytes(range(32))
        self.key_path = self.root.parent / f"{self.root.name}-evidence.key"
        self.key_path.parent.mkdir(parents=True, exist_ok=True)
        self.key_path.write_bytes(self.evidence_key)
        self._write_children()
        self.manifest = self._manifest()
        self.manifest_digest = self.write_manifest()

    @staticmethod
    def _preflight(
        package_digest: str,
        windows_digest: str,
        gpu_digest: str,
        windows_size: int,
        gpu_size: int,
        host_challenge: str,
    ) -> dict[str, object]:
        collector_digest = digest(
            (ROOT / VERIFY.REQUIRED_PROVENANCE["host-report-collector"][0])
            .read_bytes()
        )
        return {
            "schemaVersion": "p8-local-preflight-report-v1",
            "generatedAtUtc": "2026-07-25T04:00:00+00:00",
            "status": "ready",
            "exitCode": 0,
            "scope": "local-tooling-preflight-only",
            "productAcceptance": False,
            "claims": {
                "localPreflightReady": True,
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
                "gateConfig": "D:\\evidence\\provenance\\gate.json",
                "package": "D:\\package",
                "manifest": "D:\\package-manifest.json",
                "outputDirectory": "D:\\evidence\\preflight",
            },
            "missingExternalInputs": [],
            "checks": [
                {
                    "name": "gate.config",
                    "status": "ok",
                    "message": "bound",
                    "sha256": digest(
                        (ROOT / VERIFY.REQUIRED_PROVENANCE["gate-config"][0])
                        .read_bytes()
                    ),
                },
                {
                    "name": "package.manifest.identity",
                    "status": "ok",
                    "message": "bound",
                    "sha256": package_digest,
                },
                {
                    "name": "package.root",
                    "status": "ok",
                    "message": "safe",
                    "path": "D:\\package",
                },
                {
                    "name": "package.manifest.completeness",
                    "status": "ok",
                    "message": "complete",
                    "fileCount": 2,
                },
                {
                    "name": "package.config",
                    "status": "ok",
                    "message": "safe",
                    "currentBrand": "fixture",
                    "rejectEnabled": False,
                },
                {
                    "name": "package.brand",
                    "status": "ok",
                    "message": "safe",
                    "brand": "fixture",
                    "paraIni": "品牌设置/fixture/para.ini",
                    "templateDirectory": "品牌设置/fixture/模版图片",
                },
                {
                    "name": "external.windows",
                    "status": "ok",
                    "message": "present",
                    "path": "D:\\evidence\\host-inputs\\windows.json",
                    "size": windows_size,
                    "sha256": windows_digest,
                    "schemaVersion": "p8-windows-host-report-v2",
                    "captureId": "a" * 32,
                    "hostIdSha256": "b" * 64,
                    "challenge": host_challenge,
                    "packageManifestSha256": package_digest,
                    "collectorSha256": collector_digest,
                    "collectorAssertionsValidated": True,
                    "productAcceptanceChecked": False,
                    "semanticAcceptanceChecked": False,
                },
                {
                    "name": "external.gpu",
                    "status": "ok",
                    "message": "present",
                    "path": "D:\\evidence\\host-inputs\\gpu.json",
                    "size": gpu_size,
                    "sha256": gpu_digest,
                    "schemaVersion": "p8-gpu-host-report-v2",
                    "captureId": "a" * 32,
                    "hostIdSha256": "b" * 64,
                    "challenge": host_challenge,
                    "packageManifestSha256": package_digest,
                    "collectorSha256": collector_digest,
                    "collectorAssertionsValidated": True,
                    "productAcceptanceChecked": False,
                    "semanticAcceptanceChecked": False,
                },
                {
                    "name": "external.host-report-pair",
                    "status": "ok",
                    "message": "bound",
                    "captureId": "a" * 32,
                    "hostIdSha256": "b" * 64,
                    "challenge": host_challenge,
                    "packageManifestSha256": package_digest,
                    "collectorSha256": collector_digest,
                    "productAcceptanceChecked": False,
                    "semanticAcceptanceChecked": False,
                },
            ],
        }

    @staticmethod
    def _soak() -> dict[str, object]:
        command = ["D:\\package\\CigVision.exe"]
        return {
            "schemaVersion": "p8-soak-evidence-v1",
            "profile": {
                "name": "ci-contract-v1",
                "durationClass": "short",
                "claimScope": "sdk-free-local-contract-only",
                "thresholds": {
                    "minimumSuccessRate": 1.0,
                    "maximumTimeouts": 0,
                    "maximumCrashes": 0,
                    "minimumDiskFreeBytes": 67108864,
                    "maximumOutputBytes": 67108864,
                    "rssWarmupRuns": 1,
                    "maximumRssGrowthBytes": 67108864,
                    "timeoutSeconds": 300.0,
                    "sampleIntervalSeconds": 0.1,
                    "terminationGraceSeconds": 1.0,
                    "roundsPerRestart": 1,
                    "restarts": 1,
                    "plannedRuns": 1,
                },
            },
            "startedAt": "2026-07-25T04:00:00Z",
            "endedAt": "2026-07-25T04:01:00Z",
            "overallResult": "passed-local-tooling",
            "sdkFree": True,
            "productAcceptanceClaimed": False,
            "windowsRuntimeVerified": False,
            "gpu": "not-collected",
            "gpuClaimed": False,
            "commandTemplate": command,
            "cwd": "D:\\package",
            "summary": {
                "plannedRuns": 1,
                "completedRuns": 1,
                "successfulRuns": 1,
                "successRate": 1.0,
                "successRatePercent": 100.0,
                "timeouts": 0,
                "crashes": 0,
            },
            "checks": [
                {"name": name, "status": "passed"}
                for name in (
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
                )
            ],
            "runs": [
                {
                    "restart": 1,
                    "round": 1,
                    "iteration": 1,
                    "argv": command,
                    "cwd": "D:\\package",
                    "exitCode": 0,
                    "timedOut": False,
                    "crashed": False,
                    "succeeded": True,
                    "launchError": "",
                    "terminationReason": "",
                    "outputBudgetExceeded": False,
                    "outputDirectoryFresh": True,
                    "outputDirectory": "runs/restart-001/round-001/output",
                    "processGroupTermination": {
                        "noResidualProcessConfirmed": True,
                    },
                }
            ],
            "failures": [],
            "evidenceFiles": [],
            "manifestExcludedFromSelfHash": True,
        }

    def _write_json(self, relative: str, value: object) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(value, ensure_ascii=False, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    def _write_log(self, relative: str, value: str = "") -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding="utf-8")

    def _write_children(self) -> None:
        collector_digest = digest(
            (ROOT / VERIFY.REQUIRED_PROVENANCE["host-report-collector"][0])
            .read_bytes()
        )
        collector = {
            "schemaVersion": "cigvision-p8-host-collector-v1",
            "repositoryPath": "scripts/collect_windows_p8_host_reports.ps1",
            "sha256": collector_digest,
        }
        claims = {
            "collectionComplete": True,
            "productAcceptance": False,
            "windowsRuntimeAccepted": False,
            "gpuRuntimeAccepted": False,
            "realIoTested": False,
            "realRejectTested": False,
        }
        common = {
            "captureId": "a" * 32,
            "capturedAtUtc": "2026-07-25T04:00:00.0000000Z",
            "hostIdSha256": "b" * 64,
            "challenge": self.host_challenge,
            "packageManifestSha256": self.package_digest,
            "collector": collector,
            "claims": claims,
        }
        windows_report = {
            **common,
            "schemaVersion": "p8-windows-host-report-v2",
            "checks": [
                {"id": check_id, "status": "passed", "detail": "fixture"}
                for check_id in (
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
                )
            ],
        }
        gpu_report = {
            **common,
            "schemaVersion": "p8-gpu-host-report-v2",
            "checks": [
                {"id": check_id, "status": "passed", "detail": "fixture"}
                for check_id in (
                    "gpu.nvidia-smi",
                    "gpu.device",
                    "gpu.driver",
                    "dependency.cuda",
                    "dependency.tensorrt",
                    "dependency.opencv",
                )
            ],
        }
        windows_raw = (
            json.dumps(windows_report, sort_keys=True) + "\n"
        ).encode("utf-8")
        gpu_raw = (
            json.dumps(gpu_report, sort_keys=True) + "\n"
        ).encode("utf-8")
        windows_path = self.root / "host-inputs/windows.json"
        gpu_path = self.root / "host-inputs/gpu.json"
        windows_path.parent.mkdir(parents=True, exist_ok=True)
        windows_path.write_bytes(windows_raw)
        gpu_path.write_bytes(gpu_raw)
        (self.root / ".wrapper-owner").write_text(
            self.host_challenge,
            encoding="ascii",
        )
        (self.root / "host-inputs/.collector-owner").write_bytes(
            (self.host_challenge + "\r\n").encode("ascii")
        )
        preflight = self._preflight(
            self.package_digest,
            digest(windows_raw),
            digest(gpu_raw),
            len(windows_raw),
            len(gpu_raw),
            self.host_challenge,
        )
        self._write_json("preflight/p8-preflight-report.json", preflight)
        self._write_json(
            "preflight-after-soak/p8-preflight-report.json", preflight
        )
        self._write_json("soak/soak-manifest.json", self._soak())
        self._write_json(
            "release/package.stdout.json",
            {
                "artifactKind": "fixture-local-tooling",
                "scope": "fixture-local-tooling-only",
                "releaseId": self.release_id,
                "releaseDir": (
                    f"D:\\CigVision\\releases\\{self.release_id}"
                ),
                "manifestSha256": self.release_digest,
                "fileCount": 2,
                "idempotent": False,
                "sourceManifestSha256": self.package_digest,
            },
        )
        self._write_json(
            "release/verify.stdout.json",
            {
                "artifactKind": "fixture-local-tooling",
                "scope": "fixture-local-tooling-only",
                "releaseId": self.release_id,
                "releaseDir": (
                    f"D:\\CigVision\\releases\\{self.release_id}"
                ),
                "manifestSha256": self.release_digest,
                "fileCount": 2,
                "verified": True,
            },
        )
        previous = {
            "releaseId": "fixture-A",
            "manifestSha256": "5" * 64,
        }
        current = {
            "releaseId": self.release_id,
            "manifestSha256": self.release_digest,
        }
        self._write_json(
            "release/activate.stdout.json",
            {
                "schemaVersion": "p8-fixture-current-state-v1",
                "artifactKind": "fixture-local-tooling",
                "scope": "fixture-local-tooling-only",
                "current": current,
                "previous": previous if self.rollback else None,
            },
        )
        if self.rollback:
            self._write_json(
                "release/rollback.stdout.json",
                {
                    "schemaVersion": "p8-fixture-current-state-v1",
                    "artifactKind": "fixture-local-tooling",
                    "scope": "fixture-local-tooling-only",
                    "current": previous,
                    "previous": current,
                },
            )
        python = "C:\\Python\\python.exe"
        evidence = "D:\\CigVision\\evidence\\p8-windows-test"
        provenance = f"{evidence}\\provenance"
        powershell = (
            "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
        )
        self._write_json(
            "host-collector.stdout.json",
            {
                "schemaVersion": "cigvision-p8-host-collector-summary-v1",
                "status": "collection-complete",
                "exitCode": 0,
                "captureId": "a" * 32,
                "capturedAtUtc": "2026-07-25T04:00:00.0000000Z",
                "hostIdSha256": "b" * 64,
                "challenge": self.host_challenge,
                "packageManifestSha256": self.package_digest,
                "outputDirectory": f"{evidence}\\host-inputs",
                "reports": {
                    "windows": {
                        "path": "windows.json",
                        "sha256": digest(windows_raw),
                        "collectionComplete": True,
                        "failedCheckIds": [],
                    },
                    "gpu": {
                        "path": "gpu.json",
                        "sha256": digest(gpu_raw),
                        "collectionComplete": True,
                        "failedCheckIds": [],
                    },
                },
            },
        )
        self._write_log("host-collector.stderr.log")
        self._write_json(
            "host-collector.stdout.json.command.json",
            [
                powershell,
                "-NoLogo",
                "-NoProfile",
                "-File",
                f"{provenance}\\collect_windows_p8_host_reports.ps1",
                "-RepositoryRoot",
                "D:\\repository",
                "-Package",
                "D:\\package",
                "-OutputDirectory",
                f"{evidence}\\host-inputs",
                "-Challenge",
                self.host_challenge,
                "-PackageManifestSha256",
                self.package_digest,
            ],
        )
        for prefix, output in (
            ("preflight", f"{evidence}\\preflight"),
            (
                "preflight-after-soak",
                f"{evidence}\\preflight-after-soak",
            ),
        ):
            self._write_log(f"{prefix}.stdout.log", "ready\n")
            self._write_log(f"{prefix}.stderr.log")
            self._write_json(
                f"{prefix}.stdout.log.command.json",
                [
                    python,
                    f"{provenance}\\p8_preflight.py",
                    "--gate-config",
                    f"{provenance}\\p8-local-gates-v1.json",
                    "--package",
                    "D:\\package",
                    "--manifest",
                    "D:\\package-manifest.json",
                    "--manifest-sha256",
                    self.package_digest,
                    "--host-challenge",
                    self.host_challenge,
                    "--windows-input",
                    f"{evidence}\\host-inputs\\windows.json",
                    "--gpu-input",
                    f"{evidence}\\host-inputs\\gpu.json",
                    "--output-dir",
                    output,
                ],
            )
        self._write_log("soak.stdout.log", "passed\n")
        self._write_log("soak.stderr.log")
        self._write_json(
            "soak.stdout.log.command.json",
            [
                python,
                f"{provenance}\\p8_soak_evidence.py",
                "--evidence-root",
                f"{evidence}\\soak",
                "--profile",
                "ci-contract-v1",
                "--cwd",
                "D:\\package",
                "--rounds",
                "1",
                "--restarts",
                "1",
                "--timeout-seconds",
                "300",
                "--",
                "D:\\package\\CigVision.exe",
            ],
        )
        release_commands = {
            "package": [
                python,
                f"{provenance}\\p8_release.py",
                "package",
                "--source",
                "D:\\package",
                "--deployment-root",
                "D:\\CigVision",
                "--release-id",
                self.release_id,
                "--source-manifest",
                "D:\\package-manifest.json",
                "--source-manifest-sha256",
                self.package_digest,
            ],
            "verify": [
                python,
                f"{provenance}\\p8_release.py",
                "verify",
                "--deployment-root",
                "D:\\CigVision",
                "--release-id",
                self.release_id,
                "--manifest-sha256",
                self.release_digest,
            ],
            "activate": [
                python,
                f"{provenance}\\p8_release.py",
                "activate",
                "--deployment-root",
                "D:\\CigVision",
                "--release-id",
                self.release_id,
                "--manifest-sha256",
                self.release_digest,
            ],
        }
        if self.rollback:
            release_commands["rollback"] = [
                python,
                f"{provenance}\\p8_release.py",
                "rollback",
                "--deployment-root",
                "D:\\CigVision",
            ]
        for name, command in release_commands.items():
            self._write_log(f"release/{name}.stderr.log")
            self._write_json(
                f"release/{name}.stdout.json.command.json", command
            )
        for _, (repository_path, path) in VERIFY.REQUIRED_PROVENANCE.items():
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / repository_path).read_bytes())

    def file_records(self) -> list[dict[str, object]]:
        records = []
        for path in sorted(self.root.rglob("*")):
            if not path.is_file() or path.name == "wrapper-manifest.json":
                continue
            raw = path.read_bytes()
            records.append(
                {
                    "path": path.relative_to(self.root).as_posix(),
                    "size": len(raw),
                    "sha256": digest(raw),
                }
            )
        return records

    def _manifest(self) -> dict[str, object]:
        steps = list(VERIFY.BASE_STEPS)
        if self.rollback:
            steps.append("release-rollback")
        return {
            "schemaVersion": VERIFY.SCHEMA_VERSION,
            "scope": "local-tooling-on-windows-host-only",
            "overallResult": "passed-local-tooling",
            "productAcceptanceClaimed": False,
            "dDriveTargeted": True,
            "realIoEnabled": None,
            "realRejectEnabled": None,
            "realIoEnabledClaimed": False,
            "realRejectEnabledClaimed": False,
            "packageConfigurationRequiredRealRejectDisabled": True,
            "startedAt": "2026-07-25T12:00:00+08:00",
            "finishedAt": "2026-07-25T12:01:00+08:00",
            "deploymentRoot": "D:\\CigVision",
            "evidenceRoot": "D:\\CigVision\\evidence\\p8-windows-test",
            "releaseId": self.release_id,
            "hostReportChallenge": self.host_challenge,
            "packageManifestSha256": self.package_digest,
            "releaseManifestSha256": self.release_digest,
            "sourceManifestSha256": self.package_digest,
            "sourceVersion": {
                "gitHead": "a" * 40,
                "gitDirty": True,
            },
            "provenanceFiles": self.provenance_records(),
            "rollbackRequested": self.rollback,
            "rollbackExercised": self.rollback,
            "completedSteps": steps,
            "failure": None,
            "files": self.file_records(),
        }

    def provenance_records(self) -> list[dict[str, object]]:
        records = []
        for role, (repository_path, evidence_path) in sorted(
            VERIFY.REQUIRED_PROVENANCE.items()
        ):
            raw = (self.root / evidence_path).read_bytes()
            records.append(
                {
                    "role": role,
                    "repositoryPath": repository_path,
                    "path": evidence_path,
                    "size": len(raw),
                    "sha256": digest(raw),
                }
            )
        return records

    def write_manifest(self, raw: bytes | None = None) -> str:
        if raw is None:
            raw = (
                json.dumps(self.manifest, ensure_ascii=False, indent=2) + "\n"
            ).encode("utf-8")
        (self.root / "wrapper-manifest.json").write_bytes(raw)
        self.manifest_digest = digest(raw)
        self.manifest_hmac = hmac.new(
            self.evidence_key,
            raw,
            hashlib.sha256,
        ).hexdigest()
        return self.manifest_digest

    def refresh_files_and_manifest(self) -> str:
        self.manifest["files"] = self.file_records()
        self.manifest["provenanceFiles"] = self.provenance_records()
        return self.write_manifest()


class P8WindowsEvidenceVerifyTests(unittest.TestCase):
    def make_fixture(
        self, temporary: str, *, rollback: bool = False
    ) -> EvidenceFixture:
        return EvidenceFixture(
            Path(temporary).resolve() / "evidence", rollback=rollback
        )

    def verify_fixture(
        self,
        fixture: EvidenceFixture,
        manifest_digest: str | None = None,
        manifest_hmac: str | None = None,
    ) -> dict[str, object]:
        return VERIFY.verify_evidence(
            fixture.root,
            manifest_digest or fixture.manifest_digest,
            manifest_hmac or fixture.manifest_hmac,
            fixture.evidence_key,
        )

    def import_fixture(
        self,
        fixture: EvidenceFixture,
        store: Path,
    ) -> dict[str, object]:
        return VERIFY.import_evidence(
            fixture.root,
            fixture.manifest_digest,
            fixture.manifest_hmac,
            fixture.evidence_key,
            store,
        )

    def test_valid_portable_bundle_is_verified_without_product_claim(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            result = self.verify_fixture(fixture)
            self.assertTrue(result["verified"])
            self.assertEqual(result["overallResult"], "passed-local-tooling")
            self.assertFalse(result["productAcceptanceClaimed"])
            self.assertFalse(result["rollbackExercised"])

    def test_valid_a_to_b_to_a_rollback_is_verified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary, rollback=True)
            result = self.verify_fixture(fixture)
            self.assertTrue(result["rollbackExercised"])

    def test_external_manifest_digest_is_mandatory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "externally supplied SHA-256"
            ):
                self.verify_fixture(fixture, "0" * 64)
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "external HMAC trust anchor"
            ):
                self.verify_fixture(
                    fixture,
                    manifest_hmac="0" * 64,
                )
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "exactly 32 bytes"
            ):
                VERIFY.verify_evidence(
                    fixture.root,
                    fixture.manifest_digest,
                    fixture.manifest_hmac,
                    b"short",
                )

    def test_duplicate_key_and_nonfinite_json_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            valid = json.dumps(fixture.manifest, separators=(",", ":"))
            cases = (
                (
                    valid[:-1] + ',"overallResult":"passed-local-tooling"}'
                ).encode("utf-8"),
                valid.replace(
                    '"productAcceptanceClaimed":false',
                    '"productAcceptanceClaimed":NaN',
                ).encode("utf-8"),
            )
            for raw in cases:
                with self.subTest(raw=raw[-80:]):
                    supplied = fixture.write_manifest(raw)
                    with self.assertRaisesRegex(
                        VERIFY.EvidenceError, "invalid strict JSON"
                    ):
                        self.verify_fixture(fixture, supplied)

    def test_file_tamper_and_unlisted_file_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            target = fixture.root / "release/verify.stdout.json"
            target.write_bytes(target.read_bytes() + b" ")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "bundle mismatch"
            ):
                self.verify_fixture(fixture)
            fixture = self.make_fixture(str(Path(temporary) / "second"))
            (fixture.root / "unexpected.bin").write_bytes(b"x")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "bundle mismatch"
            ):
                self.verify_fixture(fixture)

    def test_coordinated_false_claim_and_source_drift_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            fixture.manifest["productAcceptanceClaimed"] = True
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "productAcceptanceClaimed"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "collector"))
            preflight_path = (
                fixture.root / "preflight/p8-preflight-report.json"
            )
            preflight = json.loads(
                preflight_path.read_text(encoding="utf-8")
            )
            for check in preflight["checks"]:
                if check["name"] == "external.windows":
                    check["collectorAssertionsValidated"] = False
            fixture._write_json(
                "preflight/p8-preflight-report.json", preflight
            )
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "collector assertions"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "host-input"))
            (fixture.root / "host-inputs/windows.json").write_bytes(
                b'{"coordinated":"replacement"}\n'
            )
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "does not match copied input"
            ):
                self.verify_fixture(fixture, supplied)
            fixture.manifest["productAcceptanceClaimed"] = False
            fixture.manifest["sourceManifestSha256"] = "9" * 64
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "source/package manifest binding"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "forged-host"))
            replacement = b'{"forged":"not-a-host-report"}\n'
            (fixture.root / "host-inputs/windows.json").write_bytes(replacement)
            for prefix in ("preflight", "preflight-after-soak"):
                relative = f"{prefix}/p8-preflight-report.json"
                report_path = fixture.root / relative
                report = json.loads(report_path.read_text(encoding="utf-8"))
                next(
                    item
                    for item in report["checks"]
                    if item["name"] == "external.windows"
                )["sha256"] = digest(replacement)
                fixture._write_json(relative, report)
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "host-inputs/windows.json"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "provenance"))
            provenance_path = fixture.root / "provenance/p8_preflight.py"
            provenance_path.write_bytes(
                provenance_path.read_bytes() + b"\n# coordinated drift\n"
            )
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "trusted repository source"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "preflight-schema"))
            preflight_path = (
                fixture.root / "preflight/p8-preflight-report.json"
            )
            preflight = json.loads(
                preflight_path.read_text(encoding="utf-8")
            )
            preflight["claims"]["unknownAcceptance"] = True
            fixture._write_json(
                "preflight/p8-preflight-report.json",
                preflight,
            )
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "keys mismatch"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "replay"))
            replay_time = "2000-01-01T00:00:00.0000000Z"
            replay_digests: dict[str, str] = {}
            for kind in ("windows", "gpu"):
                relative = f"host-inputs/{kind}.json"
                report = json.loads(
                    (fixture.root / relative).read_text(encoding="utf-8")
                )
                report["capturedAtUtc"] = replay_time
                fixture._write_json(relative, report)
                replay_digests[kind] = digest(
                    (fixture.root / relative).read_bytes()
                )
            for prefix in ("preflight", "preflight-after-soak"):
                relative = f"{prefix}/p8-preflight-report.json"
                report = json.loads(
                    (fixture.root / relative).read_text(encoding="utf-8")
                )
                for check in report["checks"]:
                    if check["name"] == "external.windows":
                        check["sha256"] = replay_digests["windows"]
                    elif check["name"] == "external.gpu":
                        check["sha256"] = replay_digests["gpu"]
                fixture._write_json(relative, report)
            summary_path = fixture.root / "host-collector.stdout.json"
            summary = json.loads(summary_path.read_text(encoding="utf-8"))
            summary["capturedAtUtc"] = replay_time
            summary["reports"]["windows"]["sha256"] = replay_digests["windows"]
            summary["reports"]["gpu"]["sha256"] = replay_digests["gpu"]
            fixture._write_json("host-collector.stdout.json", summary)
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "not captured during this wrapper run"
            ):
                self.verify_fixture(fixture, supplied)

    def test_child_semantics_are_checked_after_coordinated_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            soak_path = fixture.root / "soak/soak-manifest.json"
            soak = json.loads(soak_path.read_text(encoding="utf-8"))
            soak["productAcceptanceClaimed"] = True
            fixture._write_json("soak/soak-manifest.json", soak)
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "productAcceptanceClaimed"
            ):
                self.verify_fixture(fixture, supplied)

    def test_unsafe_case_conflicting_and_symlink_paths_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "must be outside the evidence bundle"
            ):
                VERIFY.verify_evidence(
                    fixture.root,
                    fixture.manifest_digest,
                    fixture.manifest_hmac,
                    fixture.root / ".wrapper-owner",
                )
            fixture.manifest["files"][0]["path"] = "/absolute.bin"
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "unsafe relative path"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "case"))
            duplicate = dict(fixture.manifest["files"][0])
            duplicate["path"] = duplicate["path"].swapcase()
            fixture.manifest["files"].append(duplicate)
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "casefold/NFC"
            ):
                self.verify_fixture(fixture, supplied)

            fixture = self.make_fixture(str(Path(temporary) / "link"))
            try:
                (fixture.root / "linked.bin").symlink_to(
                    fixture.root / "release/verify.stdout.json"
                )
            except OSError as exc:
                self.skipTest(f"symbolic links unavailable: {exc}")
            fixture.manifest["files"] = fixture.file_records()
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "symbolic links/reparse"
            ):
                self.verify_fixture(fixture, supplied)

    def test_completed_step_drift_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            fixture.manifest["completedSteps"] = list(
                reversed(fixture.manifest["completedSteps"])
            )
            supplied = fixture.write_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "completed step sequence"
            ):
                self.verify_fixture(fixture, supplied)

    def test_command_transcript_identity_drift_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            command_path = (
                fixture.root / "release/package.stdout.json.command.json"
            )
            command = json.loads(command_path.read_text(encoding="utf-8"))
            index = command.index("--source-manifest-sha256") + 1
            command[index] = "9" * 64
            fixture._write_json(
                "release/package.stdout.json.command.json", command
            )
            supplied = fixture.refresh_files_and_manifest()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "command source identity mismatch"
            ):
                self.verify_fixture(fixture, supplied)

    def test_snapshot_closes_hash_then_parse_race(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            original = VERIFY._validate_file_manifest

            def mutate_after_snapshot(records, snapshot):
                result = original(records, snapshot)
                target = fixture.root / "soak/soak-manifest.json"
                target.write_bytes(target.read_bytes() + b" ")
                return result

            with mock.patch.object(
                VERIFY,
                "_validate_file_manifest",
                side_effect=mutate_after_snapshot,
            ):
                with self.assertRaisesRegex(
                    VERIFY.EvidenceError, "changed during verification"
                ):
                    self.verify_fixture(fixture)

    def test_enumeration_error_and_symlinked_root_ancestor_are_rejected(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            fixture = self.make_fixture(str(root / "real"))

            def denied_walk(*args, **kwargs):
                kwargs["onerror"](PermissionError("denied"))
                return iter(())

            with mock.patch.object(VERIFY.os, "walk", side_effect=denied_walk):
                with self.assertRaisesRegex(
                    VERIFY.EvidenceError, "cannot enumerate evidence tree"
                ):
                    self.verify_fixture(fixture)

            linked = root / "linked"
            try:
                linked.symlink_to(root / "real", target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symbolic links unavailable: {exc}")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "symbolic-link/reparse-point ancestor"
            ):
                VERIFY.verify_evidence(
                    linked / "evidence",
                    fixture.manifest_digest,
                    fixture.manifest_hmac,
                    fixture.evidence_key,
                )

    def test_coordinated_command_and_soak_false_greens_are_rejected(
        self,
    ) -> None:
        cases = (
            "profile",
            "gate-config",
            "fictional-gate",
            "timeout",
            "external-drift",
            "gate-provenance",
            "release-unknown",
        )
        for case in cases:
            with self.subTest(case=case), tempfile.TemporaryDirectory() as temporary:
                fixture = self.make_fixture(temporary)
                if case == "gate-config":
                    relative = "preflight.stdout.log.command.json"
                    command_path = fixture.root / relative
                    command = json.loads(
                        command_path.read_text(encoding="utf-8")
                    )
                    index = command.index("--gate-config")
                    del command[index : index + 2]
                    fixture._write_json(relative, command)
                elif case in {"external-drift", "gate-provenance"}:
                    prefixes = (
                        ("preflight",)
                        if case == "external-drift"
                        else ("preflight", "preflight-after-soak")
                    )
                    for prefix in prefixes:
                        relative = f"{prefix}/p8-preflight-report.json"
                        report = json.loads(
                            (fixture.root / relative).read_text(
                                encoding="utf-8"
                            )
                        )
                        check_name = (
                            "external.windows"
                            if case == "external-drift"
                            else "gate.config"
                        )
                        next(
                            item
                            for item in report["checks"]
                            if item["name"] == check_name
                        )["sha256"] = "9" * 64
                        fixture._write_json(relative, report)
                elif case == "release-unknown":
                    relative = "release/verify.stdout.json"
                    release = json.loads(
                        (fixture.root / relative).read_text(encoding="utf-8")
                    )
                    release["productAcceptanceClaimed"] = True
                    fixture._write_json(relative, release)
                else:
                    relative = "soak/soak-manifest.json"
                    soak = json.loads(
                        (fixture.root / relative).read_text(encoding="utf-8")
                    )
                    if case == "profile":
                        soak["profile"]["name"] = "invented"
                    elif case == "fictional-gate":
                        soak["checks"][0]["name"] = "invented"
                    else:
                        soak["runs"][0]["timedOut"] = True
                    fixture._write_json(relative, soak)
                supplied = fixture.refresh_files_and_manifest()
                with self.assertRaises(VERIFY.EvidenceError):
                    self.verify_fixture(fixture, supplied)

    def test_real_soak_tool_artifact_matches_strict_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            evidence = root / "soak"
            command = [sys.executable, "-c", "raise SystemExit(0)"]
            completed = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "scripts/p8_soak_evidence.py"),
                    "--evidence-root",
                    str(evidence),
                    "--rounds",
                    "1",
                    "--restarts",
                    "1",
                    "--timeout-seconds",
                    "5",
                    "--",
                    *command,
                ],
                cwd=root,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            document = json.loads(
                (evidence / "soak-manifest.json").read_text(encoding="utf-8")
            )
            windows_command = ["D:\\package\\CigVision.exe"]
            document["commandTemplate"] = windows_command
            document["cwd"] = "D:\\package"
            document["runs"][0]["argv"] = windows_command
            document["runs"][0]["cwd"] = "D:\\package"
            VERIFY._validate_soak(
                document,
                windows_command,
                "D:\\package",
                "D:\\evidence\\soak",
                1,
                1,
                5.0,
            )

    def test_cli_has_distinct_success_and_invalid_exit_codes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                status = VERIFY.main(
                    [
                        "verify",
                        "--evidence-root",
                        str(fixture.root),
                        "--manifest-sha256",
                        fixture.manifest_digest,
                        "--manifest-hmac-sha256",
                        fixture.manifest_hmac,
                        "--evidence-key",
                        str(fixture.key_path),
                    ]
                )
            self.assertEqual(status, 0, stderr.getvalue())
            self.assertTrue(json.loads(stdout.getvalue())["verified"])
            with redirect_stdout(io.StringIO()), redirect_stderr(stderr):
                status = VERIFY.main(
                    [
                        "verify",
                        "--evidence-root",
                        str(fixture.root),
                        "--manifest-sha256",
                        "0" * 64,
                        "--manifest-hmac-sha256",
                        fixture.manifest_hmac,
                        "--evidence-key",
                        str(fixture.key_path),
                    ]
                )
            self.assertEqual(status, 3)

    def test_import_is_atomic_portable_and_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            first = self.import_fixture(fixture, store)
            self.assertTrue(first["imported"])
            self.assertFalse(first["idempotent"])
            imported = (
                store / "imports" / fixture.manifest_digest
            )
            self.assertTrue((imported / "receipt.json").is_file())
            verified = VERIFY.verify_evidence(
                imported / "bundle",
                fixture.manifest_digest,
                fixture.manifest_hmac,
                fixture.evidence_key,
            )
            self.assertTrue(verified["verified"])
            second = self.import_fixture(fixture, store)
            self.assertTrue(second["idempotent"])
            self.assertEqual(
                second["receipt"]["sourceManifestSha256"],
                fixture.manifest_digest,
            )
            self.assertEqual(
                second["receipt"]["manifestHmacSha256"],
                fixture.manifest_hmac,
            )
            self.assertEqual(
                second["receipt"]["verifierSchemaVersion"],
                VERIFY.SCHEMA_VERSION,
            )
            self.assertEqual(
                second["receipt"]["scope"],
                "portable-windows-local-tooling-evidence-only",
            )
            rogue = imported / "rogue.bin"
            imported.chmod(0o755)
            rogue.write_bytes(b"rogue")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "contain exactly"
            ):
                self.import_fixture(fixture, store)

    def test_import_copy_failure_leaves_no_partial_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            with mock.patch.object(
                VERIFY.shutil, "copy2", side_effect=OSError("injected")
            ):
                with self.assertRaisesRegex(
                    VERIFY.EvidenceError, "cannot copy evidence file"
                ):
                    self.import_fixture(fixture, store)
            imports = store / "imports"
            self.assertEqual(list(imports.iterdir()), [])

    def test_tampered_existing_import_is_not_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            first = self.import_fixture(fixture, store)
            target = (
                Path(first["importRoot"])
                / "bundle/release/verify.stdout.json"
            )
            target.chmod(0o644)
            target.write_bytes(target.read_bytes() + b" ")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "bundle mismatch"
            ):
                self.import_fixture(fixture, store)

    def test_tampered_import_receipt_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            first = self.import_fixture(fixture, store)
            receipt_path = Path(first["importRoot"]) / "receipt.json"
            receipt_path.chmod(0o644)
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            receipt["releaseId"] = "wrong-release"
            receipt_path.write_text(
                json.dumps(receipt, sort_keys=True) + "\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "receipt identity/scope"
            ):
                self.import_fixture(fixture, store)

    def test_receipt_rejects_removed_timestamp_and_verifier_identity_drift(
        self,
    ) -> None:
        for mutation in ("obsolete-timestamp", "verifier"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                fixture = self.make_fixture(temporary)
                store = Path(temporary).resolve() / "store"
                first = self.import_fixture(fixture, store)
                receipt_path = Path(first["importRoot"]) / "receipt.json"
                receipt_path.chmod(0o644)
                receipt = json.loads(
                    receipt_path.read_text(encoding="utf-8")
                )
                if mutation == "obsolete-timestamp":
                    receipt["importedAtUtc"] = "2026-07-25T00:00:00+00:00"
                else:
                    receipt["verifierSha256"] = "0" * 64
                receipt_path.write_text(
                    json.dumps(receipt, sort_keys=True) + "\n",
                    encoding="utf-8",
                )
                with self.assertRaises(VERIFY.EvidenceError):
                    self.import_fixture(fixture, store)

    def test_import_rejects_overlapping_store(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "must not overlap"
            ):
                self.import_fixture(fixture, fixture.root / "store")

    def test_import_rejects_case_conflicting_digest_target(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            imports = store / "imports"
            imports.mkdir(parents=True)
            (imports / fixture.manifest_digest.upper()).mkdir()
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "casefold/NFC-conflicting import target"
            ):
                self.import_fixture(fixture, store)

    def test_import_rejects_symlinked_store_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            root = Path(temporary).resolve()
            real_store = root / "real-store"
            real_store.mkdir()
            linked_store = root / "linked-store"
            try:
                linked_store.symlink_to(real_store, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"symbolic links unavailable: {exc}")
            with self.assertRaisesRegex(
                VERIFY.EvidenceError, "symbolic-link/reparse-point ancestor"
            ):
                self.import_fixture(fixture, linked_store)

    def test_import_cli_writes_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = self.make_fixture(temporary)
            store = Path(temporary).resolve() / "store"
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                status = VERIFY.main(
                    [
                        "import",
                        "--evidence-root",
                        str(fixture.root),
                        "--manifest-sha256",
                        fixture.manifest_digest,
                        "--manifest-hmac-sha256",
                        fixture.manifest_hmac,
                        "--evidence-key",
                        str(fixture.key_path),
                        "--store-root",
                        str(store),
                    ]
                )
            self.assertEqual(status, 0, stderr.getvalue())
            result = json.loads(stdout.getvalue())
            self.assertTrue(result["imported"])
            self.assertTrue(
                (Path(result["importRoot"]) / "receipt.json").is_file()
            )


if __name__ == "__main__":
    unittest.main()
