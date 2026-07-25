#!/usr/bin/env python3
"""Static contract checks for the Windows P8 orchestration wrapper."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "scripts" / "run_windows_p8_preacceptance.ps1"
HOST_COLLECTOR = ROOT / "scripts" / "collect_windows_p8_host_reports.ps1"


class P8WindowsWrapperTests(unittest.TestCase):
    def test_host_report_collector_is_read_only_strict_and_non_accepting(
        self,
    ) -> None:
        text = HOST_COLLECTOR.read_text(encoding="utf-8")
        required_ids = {
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
            "gpu.nvidia-smi",
            "gpu.device",
            "gpu.driver",
            "dependency.cuda",
            "dependency.tensorrt",
            "dependency.opencv",
        }
        for value in (
            "p8-windows-host-report-v2",
            "p8-gpu-host-report-v2",
            "cigvision-p8-host-collector-v1",
            "scripts/collect_windows_p8_host_reports.ps1",
            "OutputDirectory must be located on D:\\",
            "OutputDirectory must not already exist",
            "Assert-NoReparsePointChain",
            "Assert-OwnedOutputDirectory",
            '".collector-owner"',
            "Package and OutputDirectory must not overlap",
            "RepositoryRoot and OutputDirectory must not overlap",
            "[System.IO.FileMode]::CreateNew",
            "[System.IO.File]::Move",
            'Write-RejectionAndExit "collection.failed"',
            'Write-RejectionAndExit "host.not-windows"',
            "collectionComplete = $windowsCollectionComplete",
            "collectionComplete = $gpuCollectionComplete",
            "challenge = $Challenge",
            "packageManifestSha256 = $PackageManifestSha256",
            "Get-ApplicationAssessment",
            "Get-LockedFileSnapshot",
            "[System.IO.FileShare]::Read",
            "lockedReadSnapshot=true",
            "Get-CimInstance -ClassName Win32_VideoController",
            "empty or changed while being opened",
            "productAcceptance = $false",
            "windowsRuntimeAccepted = $false",
            "gpuRuntimeAccepted = $false",
            "realIoTested = $false",
            "realRejectTested = $false",
        ):
            self.assertIn(value, text)
        for check_id in required_ids:
            self.assertEqual(text.count(f'"{check_id}"'), 1, check_id)
        self.assertNotIn("productAcceptance = $true", text)
        self.assertNotIn("windowsRuntimeAccepted = $true", text)
        self.assertNotIn("gpuRuntimeAccepted = $true", text)
        self.assertNotIn("Start-Process", text)
        self.assertNotIn("Invoke-Item", text)
        self.assertNotIn("Invoke-ReadOnlyCommand", text)
        self.assertNotIn("& $Executable", text)
        self.assertNotIn("Remove-Item -Recurse", text)

    def test_wrapper_wires_all_local_tools_without_product_claim(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8")
        for required in (
            "p8_preflight.py",
            "collect_windows_p8_host_reports.ps1",
            '"host-report-collector"',
            '"host-inputs"',
            '"windows.json"',
            '"gpu.json"',
            "Get-CryptographicChallenge",
            "Assert-OwnedEvidenceRoot",
            '".wrapper-owner"',
            "Invoke-PowerShellStep",
            "LockedScriptPath",
            "ExpectedScriptSha256",
            "script identity changed before execution",
            "script identity changed during execution",
            "[System.IO.FileShare]::Read",
            '"host-report-collection"',
            '"-RepositoryRoot", $repoRoot',
            '"--host-challenge", $hostChallenge',
            "Host report collector did not bind the current wrapper challenge.",
            "p8_soak_evidence.py",
            "p8_release.py",
            "p8_windows_evidence_verify.py",
            "p8-local-gates-v1.json",
            "PackageManifestSha256",
            "EvidenceKeyPath",
            "must contain exactly 32 bytes",
            "EvidenceKeyPath must be outside Package.",
            "EvidenceKeyPath must be outside DeploymentRoot.",
            "WindowsInput",
            "GpuInput",
            "SoakCommand",
            "ExerciseRollback",
            "preflight-after-soak",
            "--source-manifest-sha256",
            "did not bind the externally supplied package manifest SHA-256",
            "did not restore the original current release identity",
            "D:\\CigVision",
            '"passed-local-tooling"',
            "productAcceptanceClaimed = $false",
            "SoakCommand executable must be a file inside Package",
            "Assert-NoReparsePointChain",
            "Test-PathsOverlap",
            "EvidenceRoot must not overlap package or immutable input paths",
            "FileAttributes]::ReparsePoint",
            "realIoEnabled = $null",
            "realRejectEnabled = $null",
            "realIoEnabledClaimed = $false",
            "realRejectEnabledClaimed = $false",
            '"p8-windows-preacceptance-wrapper-v4"',
            "hostReportChallenge = $hostChallenge",
            "packageManifestSha256 = $PackageManifestSha256.ToLowerInvariant()",
            "sourceManifestSha256",
            "sourceVersion",
            "provenanceFiles",
            "rev-parse HEAD",
            "status --porcelain=v1",
            "Record externally: wrapper manifest SHA-256",
            "Record externally: wrapper manifest HMAC-SHA-256",
            "--manifest-hmac-sha256",
            "--evidence-key",
            "Trusted offline verify only",
            "never execute provenance code from the unverified evidence bundle",
            "<trusted-repository>\\scripts\\p8_windows_evidence_verify.py",
            "verify --evidence-root",
        ):
            self.assertIn(required, text)
        self.assertNotIn("passed-product", text)
        self.assertNotIn("productAcceptanceClaimed = $true", text)
        self.assertNotIn("realIoEnabled = $true", text)
        self.assertNotIn("realRejectEnabled = $true", text)
        self.assertNotIn(
            "Host report inputs changed while being copied into evidence.",
            text,
        )
        self.assertNotIn("path = $item.FullName", text)
        self.assertLess(
            text.index("EvidenceRoot must not overlap package or immutable input paths"),
            text.index("New-Item -ItemType Directory -Path $EvidenceRoot"),
        )
        final_output = text[text.index("Trusted offline verify only") :]
        self.assertNotIn('python `"$evidenceVerifier`"', final_output)


if __name__ == "__main__":
    unittest.main()
