param(
    [Parameter(Mandatory = $true)]
    [string]$EnginePath,
    [ValidateRange(0.0, 1.0)]
    [double]$ConfidenceThreshold = 0.25,
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$engine = Get-Item -LiteralPath $EnginePath -ErrorAction Stop
$dataRoot = Get-ChildItem -LiteralPath $repoRoot -Directory |
    Where-Object Name -Like "04_*" | Select-Object -First 1
$imageRoot = if ($null -eq $dataRoot) { "" } else {
    Get-ChildItem -LiteralPath $dataRoot.FullName -Directory |
        Where-Object {
            @(Get-ChildItem -LiteralPath $_.FullName -File |
                Where-Object Extension -Match '^\.(jpg|jpeg|png|bmp)$').Count -eq 116
        } | Select-Object -First 1 -ExpandProperty FullName
}
$sourceDirectory = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "CigVision.sln" -File |
    Select-Object -First 1 -ExpandProperty DirectoryName
$configIni = Join-Path $sourceDirectory "config.ini"
$tensorRtRoot = $env:TENSORRT_PATH
$cudaRoot = $env:CUDA_PATH
$openCvRoot = if ([string]::IsNullOrWhiteSpace($env:OPENCV_PATH)) { "" } else {
    Join-Path $env:OPENCV_PATH "build"
}
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue

if ($null -eq $msbuild) { throw "msbuild.exe is unavailable; use VS 2022 Developer PowerShell." }
if (-not (Test-Path -LiteralPath $imageRoot)) { throw "P4 image directory is missing: $imageRoot" }
if (-not (Test-Path -LiteralPath $configIni)) { throw "CigVision config.ini is missing." }
if ([string]::IsNullOrWhiteSpace($tensorRtRoot) -or
    -not (Test-Path -LiteralPath (Join-Path $tensorRtRoot "include\NvInfer.h"))) {
    throw "TENSORRT_PATH does not identify a TensorRT development tree."
}
if ([string]::IsNullOrWhiteSpace($cudaRoot) -or
    -not (Test-Path -LiteralPath (Join-Path $cudaRoot "include\cuda_runtime_api.h"))) {
    throw "CUDA_PATH does not identify a CUDA development tree."
}
if ([string]::IsNullOrWhiteSpace($openCvRoot) -or
    -not (Test-Path -LiteralPath (Join-Path $openCvRoot "include\opencv2\imgproc.hpp")) -or
    -not (Test-Path -LiteralPath (Join-Path $openCvRoot "x64\vc16\bin\opencv_world490.dll"))) {
    throw "OPENCV_PATH does not identify the required OpenCV 4.9 tree."
}
if ((Get-Content -Raw -LiteralPath $configIni) -match "(?im)^rejectEnabled\s*=\s*true") {
    throw "P4 refuses to run while real reject is enabled."
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repoRoot ("artifacts\p4-tensorrt-" +
        (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "EvidenceRoot must not already exist: $EvidenceRoot"
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

$startedAt = Get-Date
$failures = New-Object System.Collections.Generic.List[object]
$mainBuildRoot = Join-Path $EvidenceRoot "main-build"
$batchOutput = Join-Path $EvidenceRoot "batch-output"
$manifestPath = Join-Path $EvidenceRoot "fixed-input-manifest.json"
$detectorConfigPath = Join-Path $EvidenceRoot "detector-config.json"
$invalidConfigPath = Join-Path $EvidenceRoot "invalid-detector-config.json"
$invalidOutput = Join-Path $EvidenceRoot "invalid-config-output"
$batchExitCode = $null
$invalidConfigExitCode = $null
$invalidShapeExitCode = $null
$malformedCliExitCode = $null
$conflictingCliExitCode = $null
$mainBuildExitCode = $null

function Add-Failure {
    param([string]$Step, [int]$ExitCode, [string]$Detail = "", [string]$Log = "")
    $failures.Add([pscustomobject]@{ Step = $Step; ExitCode = $ExitCode;
        Detail = $Detail; Log = $Log })
}

function Get-FileRecord {
    param([string]$Path)
    $item = Get-Item -LiteralPath $Path
    [pscustomobject]@{ Path = $item.FullName; Length = $item.Length;
        Sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash }
}

function Get-Percentile {
    param([double[]]$Values, [double]$Percentile)
    if ($Values.Count -eq 0) { return $null }
    $ordered = @($Values | Sort-Object)
    $index = [Math]::Ceiling(($Percentile / 100.0) * $ordered.Count) - 1
    return $ordered[[Math]::Max(0, [Math]::Min($ordered.Count - 1, $index))]
}

try {
    $files = @(Get-ChildItem -LiteralPath $imageRoot -File |
        Where-Object Extension -Match '^\.(jpg|jpeg|png|bmp)$' | Sort-Object Name)
    if ($files.Count -ne 116) {
        Add-Failure "fixed-input-count" 1 "Expected 116 images, found $($files.Count)"
    }
    $samples = @($files | ForEach-Object {
        [ordered]@{ path = $_.FullName;
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    [ordered]@{ root = "."; groundTruthAvailable = $false; samples = $samples } |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    $engineHash = (Get-FileHash -LiteralPath $engine.FullName -Algorithm SHA256).Hash
    [ordered]@{
        schemaVersion = "cigvision-tensorrt-detector-v2"
        enginePath = $engine.FullName
        modelSha256 = $engineHash.ToLowerInvariant()
        inputTensorName = "images"
        outputTensorName = "output0"
        inputWidth = 992
        inputHeight = 992
        classConfidenceThresholds = @(0..8 | ForEach-Object { $ConfidenceThreshold })
        classNames = @("dakoucuoya", "feiyan", "jiamo", "lvzuizhezhou", "quezui",
            "yanbangposun", "yanbangzangwu", "wuzi", "jietou")
        disabledClassIds = @()
        detectorVersion = "yanzhi20260120-trt10-$($engineHash.Substring(0, 8).ToLowerInvariant())"
        parameterVersion = "tensorrt-parameters-v2"
        preprocessMode = "stretch-rgb-f32"
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $detectorConfigPath -Encoding UTF8

    $buildLog = Join-Path $EvidenceRoot "main-build-command.log"
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "run_windows_p1_build.ps1") `
            -Configuration Release -EvidenceRoot $mainBuildRoot 2>&1 | Tee-Object -FilePath $buildLog
        $mainBuildExitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
    if ($mainBuildExitCode -ne 0) {
        Add-Failure "main-release-build" $mainBuildExitCode "" $buildLog
    }
    else {
        $mainManifest = Get-Content -Raw -LiteralPath (Join-Path $mainBuildRoot "manifest.json") |
            ConvertFrom-Json
        $executable = Join-Path $mainManifest.Builds[0].OutputDirectory "CigVision.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            Add-Failure "main-executable" 1 $executable
        }
        else {
            $qtRoot = [Environment]::GetEnvironmentVariable("CIGVISION_QT_ROOT", "User")
            $halconRoot = [Environment]::GetEnvironmentVariable(
                "CIGVISION_HALCON_RELEASE_ROOT", "User")
            $runtimePaths = @(
                (Split-Path -Parent $executable),
                (Join-Path $qtRoot "bin"),
                (Join-Path $halconRoot "bin\x64-win64"),
                "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64",
                (Join-Path $tensorRtRoot "bin"),
                (Join-Path $cudaRoot "bin\x64"),
                (Join-Path $openCvRoot "x64\vc16\bin")
            ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and
                (Test-Path -LiteralPath $_) }
            $env:PATH = (($runtimePaths -join ";") + ";" + $env:PATH)

            New-Item -ItemType Directory -Path $batchOutput | Out-Null
            $batchArguments = @("--tensorrt-batch-manifest", "`"$manifestPath`"",
                "--offline-output", "`"$batchOutput`"", "--detector-config",
                "`"$detectorConfigPath`"")
            ($executable + " " + ($batchArguments -join " ")) |
                Set-Content -LiteralPath (Join-Path $EvidenceRoot "batch-command.txt") -Encoding UTF8
            $batchProcess = Start-Process -FilePath $executable -ArgumentList $batchArguments `
                -PassThru -Wait
            $batchExitCode = $batchProcess.ExitCode
            if ($batchExitCode -ne 0) { Add-Failure "fixed-image-batch" $batchExitCode }

            $summaryPath = Join-Path $batchOutput "summary.json"
            $resultFiles = @(Get-ChildItem -LiteralPath $batchOutput -Filter "frame-*.json" -File)
            $allPng = @(Get-ChildItem -LiteralPath $batchOutput -Filter "*.png" -File)
            $sourcePng = @($allPng | Where-Object Name -Match '^frame-[0-9]{8}\.png$')
            $annotatedPng = @($allPng |
                Where-Object Name -Match '^frame-[0-9]{8}-annotated\.png$')
            if (-not (Test-Path -LiteralPath $summaryPath) -or $resultFiles.Count -ne 116 -or
                $sourcePng.Count -ne 116 -or $annotatedPng.Count -ne 116) {
                Add-Failure "batch-output-count" 1 "Expected summary, 116 JSON, 116 source PNG and 116 annotated PNG"
            }
            else {
                $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
                if ($summary.state -ne 1 -or $summary.statistics.received -ne 116 -or
                    $summary.statistics.processed -ne 116 -or $summary.statistics.error -ne 0 -or
                    $summary.statistics.dropped -ne 0 -or $summary.statistics.saveFailures -ne 0) {
                    Add-Failure "batch-summary" 1 "Batch did not complete 116 frames without errors"
                }

                $results = @($resultFiles | Sort-Object Name | ForEach-Object {
                    Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json
                })
                $latencies = [double[]]@($results | ForEach-Object { $_.elapsedMicros / 1000.0 })
                $classCounts = @{}
                $confidenceValues = New-Object System.Collections.Generic.List[double]
                foreach ($result in $results) {
                    foreach ($defect in @($result.defects)) {
                        $key = [string]$defect.classId
                        $classCounts[$key] = 1 + [int]($classCounts[$key])
                        $confidenceValues.Add([double]$defect.confidence)
                    }
                }
                $fixedSamples = @(Get-Content -Raw -LiteralPath $manifestPath |
                    ConvertFrom-Json | Select-Object -ExpandProperty samples)
                $hashGroups = @($fixedSamples | Group-Object -Property sha256 |
                    Where-Object { $_.Count -gt 1 })
                $determinismIssues = New-Object System.Collections.Generic.List[string]
                foreach ($group in $hashGroups) {
                    $indices = New-Object System.Collections.Generic.List[int]
                    for ($sampleIndex = 0; $sampleIndex -lt $fixedSamples.Count; $sampleIndex++) {
                        if ($fixedSamples[$sampleIndex].sha256 -eq $group.Name) {
                            $indices.Add($sampleIndex)
                        }
                    }
                    $signatures = @($indices | ForEach-Object {
                        @($results[$_].defects) | ConvertTo-Json -Compress -Depth 5
                    } | Select-Object -Unique)
                    if ($signatures.Count -ne 1) { $determinismIssues.Add($group.Name) }
                }
                if ($determinismIssues.Count -ne 0) {
                    Add-Failure "duplicate-input-determinism" 1 ($determinismIssues -join ",")
                }
                [pscustomobject]@{
                    GroundTruthAvailable = $false
                    AccuracyMetricsClaimed = $false
                    Processed = $results.Count
                    Ok = $summary.statistics.ok
                    Ng = $summary.statistics.ng
                    DetectionCount = $confidenceValues.Count
                    ClassCounts = $classCounts
                    Confidence = [pscustomobject]@{
                        Minimum = if ($confidenceValues.Count) { ($confidenceValues | Measure-Object -Minimum).Minimum } else { $null }
                        Median = Get-Percentile ([double[]]$confidenceValues) 50
                        Maximum = if ($confidenceValues.Count) { ($confidenceValues | Measure-Object -Maximum).Maximum } else { $null }
                    }
                    DetectorLatencyMs = [pscustomobject]@{
                        Minimum = ($latencies | Measure-Object -Minimum).Minimum
                        Median = Get-Percentile $latencies 50
                        P95 = Get-Percentile $latencies 95
                        P99 = Get-Percentile $latencies 99
                        Maximum = ($latencies | Measure-Object -Maximum).Maximum
                    }
                    DuplicateHashGroups = @($hashGroups).Count
                    DuplicateDeterminismIssues = $determinismIssues
                } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath `
                    (Join-Path $EvidenceRoot "effect-summary.json") -Encoding UTF8
            }

            $invalid = Get-Content -Raw -LiteralPath $detectorConfigPath | ConvertFrom-Json
            $invalid.enginePath = Join-Path $EvidenceRoot "missing.engine"
            $invalid | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $invalidConfigPath -Encoding UTF8
            New-Item -ItemType Directory -Path $invalidOutput | Out-Null
            $invalidArguments = @("--tensorrt-batch-manifest", "`"$manifestPath`"",
                "--offline-output", "`"$invalidOutput`"", "--detector-config",
                "`"$invalidConfigPath`"")
            $invalidProcess = Start-Process -FilePath $executable -ArgumentList $invalidArguments `
                -PassThru -Wait
            $invalidConfigExitCode = $invalidProcess.ExitCode
            $initializationErrorPath = Join-Path $invalidOutput "initialization-error.json"
            if ($invalidConfigExitCode -ne 4 -or
                -not (Test-Path -LiteralPath $initializationErrorPath)) {
                Add-Failure "invalid-config-rejection" $invalidConfigExitCode "Expected exit 4 and error JSON"
            }

            $invalidShape = Get-Content -Raw -LiteralPath $detectorConfigPath | ConvertFrom-Json
            $invalidShape.inputWidth = -1
            $invalidShapePath = Join-Path $EvidenceRoot "invalid-shape-config.json"
            $invalidShapeOutput = Join-Path $EvidenceRoot "invalid-shape-output"
            $invalidShape | ConvertTo-Json -Depth 5 |
                Set-Content -LiteralPath $invalidShapePath -Encoding UTF8
            New-Item -ItemType Directory -Path $invalidShapeOutput | Out-Null
            $shapeArguments = @("--tensorrt-batch-manifest", "`"$manifestPath`"",
                "--offline-output", "`"$invalidShapeOutput`"", "--detector-config",
                "`"$invalidShapePath`"")
            $shapeProcess = Start-Process -FilePath $executable -ArgumentList $shapeArguments `
                -PassThru -Wait
            $invalidShapeExitCode = $shapeProcess.ExitCode
            $invalidShapeErrorPath = Join-Path $invalidShapeOutput "initialization-error.json"
            if ($invalidShapeExitCode -ne 4 -or
                -not (Test-Path -LiteralPath $invalidShapeErrorPath)) {
                Add-Failure "invalid-shape-rejection" $invalidShapeExitCode `
                    "Expected exit 4 before detector allocation"
            }

            $malformedArguments = @("--tensorrt-batch-manifest", "`"$manifestPath`"",
                "--offline-output", "`"$(Join-Path $EvidenceRoot 'malformed-output')`"")
            $malformedProcess = Start-Process -FilePath $executable `
                -ArgumentList $malformedArguments -PassThru -Wait
            $malformedCliExitCode = $malformedProcess.ExitCode
            if ($malformedCliExitCode -ne 2) {
                Add-Failure "malformed-cli-rejection" $malformedCliExitCode `
                    "Missing detector config must exit 2 without opening UI"
            }

            $conflictingArguments = @("--tensorrt-batch-manifest", "`"$manifestPath`"",
                "--offline-batch-manifest", "`"$manifestPath`"", "--offline-output",
                "`"$(Join-Path $EvidenceRoot 'conflicting-output')`"", "--detector-config",
                "`"$detectorConfigPath`"")
            $conflictingProcess = Start-Process -FilePath $executable `
                -ArgumentList $conflictingArguments -PassThru -Wait
            $conflictingCliExitCode = $conflictingProcess.ExitCode
            if ($conflictingCliExitCode -ne 2) {
                Add-Failure "conflicting-cli-rejection" $conflictingCliExitCode `
                    "Conflicting batch modes must exit 2 without opening UI"
            }
        }
    }
}
catch {
    Add-Failure "unhandled-exception" 1 $_.Exception.ToString()
}
finally {
    $environment = [pscustomobject]@{
        Windows = [Environment]::OSVersion.VersionString
        MSBuild = $msbuild.Source
        TensorRtRoot = $tensorRtRoot
        CudaRoot = $cudaRoot
        OpenCvRoot = $openCvRoot
        NvidiaSmi = @(& nvidia-smi --query-gpu=name,driver_version,memory.total,compute_cap `
            --format=csv,noheader 2>&1)
    }
    $evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceRoot -Recurse -File |
        Where-Object Name -ne "manifest.json" | Sort-Object FullName |
        ForEach-Object { Get-FileRecord $_.FullName })
    $p4SourcePaths = @(
        (Join-Path $sourceDirectory "adapters\tensorrt\TensorRtDetector.h"),
        (Join-Path $sourceDirectory "adapters\tensorrt\TensorRtDetector.cpp"),
        (Join-Path $sourceDirectory "adapters\qt\QtOfflineInspection.h"),
        (Join-Path $sourceDirectory "adapters\qt\QtOfflineInspection.cpp"),
        (Join-Path $sourceDirectory "core\InspectionContracts.h"),
        (Join-Path $sourceDirectory "core\InspectionInterfaces.h"),
        (Join-Path $sourceDirectory "core\OfflineInspection.h"),
        (Join-Path $sourceDirectory "main.cpp"),
        (Join-Path $sourceDirectory "CigVision.vcxproj"),
        $PSCommandPath
    )
    $p4SourceFiles = @($p4SourcePaths | Where-Object { Test-Path -LiteralPath $_ } |
        ForEach-Object { Get-FileRecord $_ })
    [pscustomobject]@{
        CommandLine = "powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -EnginePath `"$($engine.FullName)`" -ConfidenceThreshold $ConfidenceThreshold -EvidenceRoot `"$EvidenceRoot`""
        OverallResult = if ($failures.Count -eq 0) { "passed" } else { "failed" }
        OverallCommandExitCode = if ($failures.Count -eq 0) { 0 } else { 1 }
        StartedAt = $startedAt.ToString("o")
        FinishedAt = (Get-Date).ToString("o")
        GitHead = (& git -C $repoRoot rev-parse HEAD)
        GitStatus = @(& git -C $repoRoot status --short --untracked-files=all)
        Environment = $environment
        Engine = Get-FileRecord $engine.FullName
        MainBuildExitCode = $mainBuildExitCode
        BatchExitCode = $batchExitCode
        InvalidConfigExitCode = $invalidConfigExitCode
        InvalidShapeExitCode = $invalidShapeExitCode
        MalformedCliExitCode = $malformedCliExitCode
        ConflictingCliExitCode = $conflictingCliExitCode
        GroundTruthAvailable = $false
        AccuracyMetricsClaimed = $false
        RealRejectEnabled = $false
        Failures = $failures
        P4SourceFiles = $p4SourceFiles
        Files = $evidenceFiles
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath `
        (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}

if ($failures.Count -ne 0) {
    Write-Error "P4 TensorRT validation failed. Evidence: $EvidenceRoot"
    exit 1
}
Write-Host "PASS P4 TensorRT validation. Evidence: $EvidenceRoot"
