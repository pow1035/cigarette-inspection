param(
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$testProject = Join-Path $repoRoot "tests\CigVision.Offline\CigVision.Offline.vcxproj"
$fixtureManifest = Join-Path $repoRoot "tests\fixtures\p3-samples.json"
$solutionPath = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "CigVision.sln" -File |
    Select-Object -First 1 -ExpandProperty FullName
$sourceDirectory = if ([string]::IsNullOrWhiteSpace($solutionPath)) { "" } else {
    Split-Path -Parent $solutionPath
}
$configPath = if ([string]::IsNullOrWhiteSpace($solutionPath)) { "" } else {
    Join-Path $sourceDirectory "config.ini"
}
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue

if ($null -eq $msbuild) {
    throw "msbuild.exe is not available. Run this script in a VS 2022 Developer PowerShell."
}
if (-not (Test-Path -LiteralPath $testProject)) {
    throw "Offline test project was not found: $testProject"
}
if (-not (Test-Path -LiteralPath $fixtureManifest)) {
    throw "P3 fixture manifest was not found: $fixtureManifest"
}
if ([string]::IsNullOrWhiteSpace($configPath) -or -not (Test-Path -LiteralPath $configPath)) {
    throw "CigVision config.ini was not found."
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceRoot = Join-Path $repoRoot "artifacts\p3-offline-$timestamp"
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$builds = New-Object System.Collections.Generic.List[object]
$failures = New-Object System.Collections.Generic.List[object]
$startedAt = Get-Date
$mainBuildRoot = Join-Path $EvidenceRoot "main-build"
$batchOutput = Join-Path $EvidenceRoot "batch-output"
$startupRoot = Join-Path $EvidenceRoot "startup"
$mainBuildExitCode = $null
$batchExitCode = $null
$invalidManifestExitCode = $null
New-Item -ItemType Directory -Force -Path $batchOutput, $startupRoot | Out-Null

function Add-Failure {
    param([string]$Step, [int]$ExitCode, [string]$Detail = "", [string]$Log = "")
    $failures.Add([pscustomobject]@{
        Step = $Step
        ExitCode = $ExitCode
        Detail = $Detail
        Log = $Log
    })
}

function Get-FileRecord {
    param([string]$Path)
    [pscustomobject]@{
        Path = $Path
        Length = (Get-Item -LiteralPath $Path).Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Save-WindowCapture {
    param([IntPtr]$Handle, [string]$Path)

    if (-not ("P3WindowCapture" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class P3WindowCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdc, uint flags);
}
"@
    }
    Add-Type -AssemblyName System.Drawing
    $rect = New-Object P3WindowCapture+RECT
    if (-not [P3WindowCapture]::GetWindowRect($Handle, [ref]$rect)) { return $false }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) { return $false }
    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $hdc = $graphics.GetHdc()
    try { $captured = [P3WindowCapture]::PrintWindow($Handle, $hdc, 2) }
    finally { $graphics.ReleaseHdc($hdc); $graphics.Dispose() }
    if ($captured) { $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png) }
    $bitmap.Dispose()
    return $captured
}

try {
    foreach ($configuration in @("Debug", "Release")) {
        $configurationRoot = Join-Path $EvidenceRoot "offline-tests\$configuration"
        $outputDirectory = Join-Path $configurationRoot "bin"
        $intermediateDirectory = Join-Path $configurationRoot "obj"
        New-Item -ItemType Directory -Force -Path $outputDirectory, $intermediateDirectory | Out-Null
        $buildLog = Join-Path $EvidenceRoot "msbuild-offline-$($configuration.ToLowerInvariant()).log"
        $testLog = Join-Path $EvidenceRoot "test-offline-$($configuration.ToLowerInvariant()).log"
        $buildArguments = @(
            $testProject, "/m", "/t:Rebuild", "/p:Configuration=$configuration",
            "/p:Platform=x64", "/p:OutDir=$outputDirectory\",
            "/p:IntDir=$intermediateDirectory\", "/v:minimal"
        )
        & $msbuild.Source @buildArguments 2>&1 | Tee-Object -FilePath $buildLog
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0) {
            Add-Failure "offline-test-build-$configuration" $buildExitCode "" $buildLog
            continue
        }
        $testExecutable = Join-Path $outputDirectory "CigVision.Offline.exe"
        & $testExecutable 2>&1 | Tee-Object -FilePath $testLog
        $testExitCode = $LASTEXITCODE
        if ($testExitCode -ne 0) {
            Add-Failure "offline-tests-$configuration" $testExitCode "" $testLog
        }
        $builds.Add([pscustomobject]@{
            Configuration = $configuration
            BuildExitCode = $buildExitCode
            TestExitCode = $testExitCode
            BuildLog = $buildLog
            TestLog = $testLog
            Executable = Get-FileRecord $testExecutable
        })
    }

    $mainBuildLog = Join-Path $EvidenceRoot "main-build-command.log"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_windows_p1_build.ps1") `
            -Configuration Release -EvidenceRoot $mainBuildRoot 2>&1 | Tee-Object -FilePath $mainBuildLog
        $mainBuildExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($mainBuildExitCode -ne 0) {
        Add-Failure "main-release-build" $mainBuildExitCode "" $mainBuildLog
    } else {
        $mainManifest = Get-Content -Raw -LiteralPath (Join-Path $mainBuildRoot "manifest.json") |
            ConvertFrom-Json
        $executable = Join-Path $mainManifest.Builds[0].OutputDirectory "CigVision.exe"
        $qtRoot = [Environment]::GetEnvironmentVariable("CIGVISION_QT_ROOT", "User")
        if ([string]::IsNullOrWhiteSpace($qtRoot)) { $qtRoot = $mainManifest.Environment.QtRoot }
        $halconRoot = [Environment]::GetEnvironmentVariable("CIGVISION_HALCON_RELEASE_ROOT", "User")
        if ([string]::IsNullOrWhiteSpace($halconRoot)) {
            $halconRoot = ($mainManifest.Environment.ExpectedHalconRoots |
                Where-Object { $_.Configuration -eq "Release" }).Path
        }
        $runtimePaths = @(
            (Split-Path -Parent $executable),
            (Join-Path $qtRoot "bin"),
            (Join-Path $halconRoot "bin\x64-win64"),
            "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
        ) | Where-Object { Test-Path -LiteralPath $_ }
        $env:PATH = (($runtimePaths -join ";") + ";" + $env:PATH)

        $batchArgs = "--offline-batch-manifest `"$fixtureManifest`" --offline-output `"$batchOutput`""
        $batchCommand = "`"$executable`" $batchArgs"
        $batchCommand | Set-Content -LiteralPath (Join-Path $EvidenceRoot "batch-command.txt") -Encoding UTF8
        $batchProcess = Start-Process -FilePath $executable -ArgumentList $batchArgs -PassThru -Wait
        $batchExitCode = $batchProcess.ExitCode
        if ($batchExitCode -ne 0) {
            Add-Failure "fixed-sample-batch" $batchExitCode $batchCommand
        }

        $summaryPath = Join-Path $batchOutput "summary.json"
        if (-not (Test-Path -LiteralPath $summaryPath)) {
            Add-Failure "batch-summary" 1 "summary.json was not generated"
        } else {
            $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
            $frameJson = @(Get-ChildItem -LiteralPath $batchOutput -Filter "frame-*.json" -File)
            $framePng = @(Get-ChildItem -LiteralPath $batchOutput -Filter "frame-*.png" -File)
            $statsValid = $summary.state -eq 1 -and $summary.statistics.received -eq 8 -and
                $summary.statistics.processed -eq 8 -and $summary.statistics.ok -eq 4 -and
                $summary.statistics.ng -eq 4 -and $summary.statistics.error -eq 0 -and
                $summary.statistics.dropped -eq 0 -and $summary.statistics.sourceErrors -eq 0 -and
                $summary.statistics.detectorErrors -eq 0 -and $summary.statistics.observerErrors -eq 0 -and
                $summary.statistics.saveFailures -eq 0
            if (-not $statsValid -or $frameJson.Count -ne 8 -or $framePng.Count -ne 8) {
                Add-Failure "batch-output-validation" 1 "Expected 8 JSON, 8 PNG, OK=4, NG=4 and zero errors"
            }
            $inputTrace = Get-Content -Raw -LiteralPath (Join-Path $batchOutput "input-manifest.json") |
                ConvertFrom-Json
            $ngTrace = Get-Content -Raw -LiteralPath (Join-Path $batchOutput "frame-00000002.json") |
                ConvertFrom-Json
            $traceValid = @($inputTrace.samples).Count -eq 8 -and
                -not [string]::IsNullOrWhiteSpace($ngTrace.sourceFile) -and
                $ngTrace.stationId -eq "offline" -and $ngTrace.capturedAtMicros -gt 0 -and
                $ngTrace.width -gt 0 -and $ngTrace.height -gt 0 -and
                -not [string]::IsNullOrWhiteSpace($ngTrace.defects[0].detectorVersion) -and
                $ngTrace.defects[0].box.width -gt 0 -and $ngTrace.defects[0].box.height -gt 0
            if (-not $traceValid) {
                Add-Failure "batch-trace-validation" 1 "Input manifest or complete frame trace is missing"
            }
        }

        $invalidManifest = Join-Path $EvidenceRoot "invalid-manifest.json"
        '{"root":".","samples":[{"path":"","sha256":"bad","expected":"MAYBE"}]}' |
            Set-Content -LiteralPath $invalidManifest -Encoding UTF8
        $invalidOutput = Join-Path $EvidenceRoot "invalid-batch-output"
        $invalidArgs = "--offline-batch-manifest `"$invalidManifest`" --offline-output `"$invalidOutput`""
        $invalidProcess = Start-Process -FilePath $executable -ArgumentList $invalidArgs -PassThru -Wait
        $invalidManifestExitCode = $invalidProcess.ExitCode
        if ($invalidManifestExitCode -ne 2 -or
            (Test-Path -LiteralPath (Join-Path $invalidOutput "summary.json"))) {
            Add-Failure "invalid-manifest-rejection" $invalidManifestExitCode "Expected exit 2 and no summary"
        }

        $startupLog = Join-Path $startupRoot "startup-observation.json"
        $startupScreenshot = Join-Path $startupRoot "offline-window.png"
        $startup = Start-Process -FilePath $executable -ArgumentList "--offline" -PassThru
        $deadline = (Get-Date).AddSeconds(15)
        do {
            Start-Sleep -Milliseconds 250
            $startup.Refresh()
        } while (-not $startup.HasExited -and $startup.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)
        if ($startup.HasExited -or $startup.MainWindowHandle -eq 0) {
            Add-Failure "offline-ui-startup" 1 "Offline window was not observed"
        } else {
            $captured = Save-WindowCapture $startup.MainWindowHandle $startupScreenshot
            $modules = @()
            try {
                $modules = @($startup.Modules | ForEach-Object {
                    [pscustomobject]@{ Name = $_.ModuleName; Path = $_.FileName;
                        Version = $_.FileVersionInfo.FileVersion }
                })
            } catch { $modules = @([pscustomobject]@{ Error = $_.Exception.Message }) }
            [pscustomobject]@{
                ProcessId = $startup.Id
                Responding = $startup.Responding
                MainWindowTitle = $startup.MainWindowTitle
                MainWindowHandle = $startup.MainWindowHandle.ToInt64()
                ScreenshotCaptured = $captured
                OfflineArgument = $true
                ApplicationControlsClicked = $false
                Modules = $modules
            } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $startupLog -Encoding UTF8
            if (-not $captured) { Add-Failure "offline-ui-screenshot" 1 "PrintWindow failed" }
        }
        if (-not $startup.HasExited) {
            [void]$startup.CloseMainWindow()
            if (-not $startup.WaitForExit(5000)) { $startup.Kill(); $startup.WaitForExit() }
        }
    }
}
catch {
    Add-Failure "unhandled-exception" 1 $_.Exception.ToString()
}
finally {
    $p3Files = @(
        (Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "OfflineInspection.h" -File |
            Select-Object -First 1 -ExpandProperty FullName),
        (Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "QtOfflineInspection.h" -File |
            Select-Object -First 1 -ExpandProperty FullName),
        (Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "QtOfflineInspection.cpp" -File |
            Select-Object -First 1 -ExpandProperty FullName),
        (Join-Path $sourceDirectory "CigVision.cpp"),
        (Join-Path $sourceDirectory "CigVision.h"),
        (Join-Path $sourceDirectory "main.cpp"),
        (Join-Path $sourceDirectory "CigVision.vcxproj"),
        (Join-Path $repoRoot "tests\CigVision.Offline\OfflineTests.cpp"),
        $testProject, $fixtureManifest, $PSCommandPath
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_) } |
        ForEach-Object { Get-FileRecord $_ }
    $outputFiles = @()
    if (Test-Path -LiteralPath $batchOutput) {
        $outputFiles = @(Get-ChildItem -LiteralPath $batchOutput -File | Sort-Object Name |
            ForEach-Object { Get-FileRecord $_.FullName })
    }
    $startupFiles = @()
    if (Test-Path -LiteralPath $startupRoot) {
        $startupFiles = @(Get-ChildItem -LiteralPath $startupRoot -File | Sort-Object Name |
            ForEach-Object { Get-FileRecord $_.FullName })
    }
    [pscustomobject]@{
        CommandLine = "powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -EvidenceRoot `"$EvidenceRoot`""
        OverallResult = if ($failures.Count -eq 0) { "passed" } else { "failed" }
        OverallCommandExitCode = if ($failures.Count -eq 0) { 0 } else { 1 }
        StartedAt = $startedAt.ToString("o")
        FinishedAt = (Get-Date).ToString("o")
        MSBuild = $msbuild.Source
        GitHead = (& git -C $repoRoot rev-parse HEAD)
        GitStatus = @(& git -C $repoRoot status --short --untracked-files=all)
        RealRejectEnabled = ((Get-Content -Raw -LiteralPath $configPath) -match "(?im)^rejectEnabled\s*=\s*true")
        OfflineTestBuilds = $builds
        MainBuildRoot = $mainBuildRoot
        MainBuildExitCode = $mainBuildExitCode
        BatchOutput = $batchOutput
        BatchExitCode = $batchExitCode
        InvalidManifestExitCode = $invalidManifestExitCode
        P3SourceFiles = $p3Files
        BatchOutputFiles = $outputFiles
        StartupFiles = $startupFiles
        Failures = $failures
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}

if ($failures.Count -ne 0) {
    Write-Error "P3 offline validation failed. Evidence: $EvidenceRoot"
    exit 1
}
Write-Host "PASS P3 offline validation. Evidence: $EvidenceRoot"
