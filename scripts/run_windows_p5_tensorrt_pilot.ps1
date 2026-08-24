param(
    [Parameter(Mandatory = $true)]
    [string]$PilotPackage,
    [Parameter(Mandatory = $true)]
    [string]$EnginePath,
    [Parameter(Mandatory = $true)]
    [string]$ModelPath,
    [Parameter(Mandatory = $true)]
    [string]$DetectorConfigPath,
    [string]$ExecutablePath = "",
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$failures = New-Object System.Collections.Generic.List[object]
$transcriptStarted = $false
$exitCode = 1

function Get-FileRecord {
    param([Parameter(Mandatory = $true)][string]$Path)
    $item = Get-Item -LiteralPath $Path -ErrorAction Stop
    [ordered]@{
        path = $item.FullName
        length = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

function Get-NearestRankPercentile {
    param([double[]]$Values, [double]$Percentile)
    if ($Values.Count -eq 0) { return $null }
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Max(0, [Math]::Ceiling($Percentile * $sorted.Count) - 1)
    return $sorted[$index]
}

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repoRoot (
        "artifacts\p5-tensorrt-pilot-runtime-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "EvidenceRoot must not already exist: $EvidenceRoot"
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

try {
    Start-Transcript -LiteralPath (Join-Path $EvidenceRoot "full-terminal.log") | Out-Null
    $transcriptStarted = $true

    $package = Get-Item -LiteralPath $PilotPackage -ErrorAction Stop
    $engine = Get-Item -LiteralPath $EnginePath -ErrorAction Stop
    $model = Get-Item -LiteralPath $ModelPath -ErrorAction Stop
    $configTemplate = Get-Item -LiteralPath $DetectorConfigPath -ErrorAction Stop
    if (-not $package.PSIsContainer) {
        throw "PilotPackage must be a directory: $PilotPackage"
    }

    $selectionPath = Join-Path $package.FullName "pilot-selection.json"
    $imagesPath = Join-Path $package.FullName "images"
    if (-not (Test-Path -LiteralPath $selectionPath) -or
        -not (Test-Path -LiteralPath $imagesPath -PathType Container)) {
        throw "PilotPackage must contain pilot-selection.json and images/."
    }

    if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
        $ExecutablePath = Get-ChildItem -LiteralPath $repoRoot -Filter "CigVision.exe" `
            -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object FullName -Match '\\x64\\Release\\CigVision\.exe$' |
            Select-Object -First 1 -ExpandProperty FullName
    }
    $executable = Get-Item -LiteralPath $ExecutablePath -ErrorAction Stop

    $sourceDirectory = Get-ChildItem -LiteralPath $repoRoot -Filter "CigVision.sln" `
        -File -Recurse | Select-Object -First 1 -ExpandProperty DirectoryName
    $configIni = Join-Path $sourceDirectory "config.ini"
    if (-not (Test-Path -LiteralPath $configIni)) {
        throw "CigVision config.ini is missing."
    }
    $rejectSettings = @(
        Select-String -LiteralPath $configIni -Pattern "(?im)^rejectEnabled\s*=\s*(\S+)"
    )
    if ($rejectSettings.Count -ne 1 -or
        $rejectSettings[0].Matches[0].Groups[1].Value.ToLowerInvariant() -ne "false") {
        throw "P5 TensorRT pilot requires exactly one rejectEnabled=false setting."
    }

    $tensorRtRoot = $env:TENSORRT_PATH
    $trtexec = if ([string]::IsNullOrWhiteSpace($tensorRtRoot)) { "" } else {
        Join-Path $tensorRtRoot "bin\trtexec.exe"
    }
    if (-not (Test-Path -LiteralPath $trtexec)) {
        throw "TENSORRT_PATH does not identify a TensorRT tree with bin\trtexec.exe."
    }
    $trtexecItem = Get-Item -LiteralPath $trtexec -ErrorAction Stop
    $tensorRtVersion = $trtexecItem.VersionInfo.FileVersion
    if ([string]::IsNullOrWhiteSpace($tensorRtVersion)) {
        $tensorRtVersion = Split-Path -Leaf $tensorRtRoot
    }
    if ([string]::IsNullOrWhiteSpace($tensorRtVersion)) {
        throw "Unable to determine the TensorRT version."
    }

    $selection = Get-Content -Raw -LiteralPath $selectionPath -Encoding UTF8 |
        ConvertFrom-Json
    $selected = @($selection.selected)
    if ($selection.schema_version -ne "p5-pilot-selection-v1" -or
        $selection.selected_count -ne 30 -or $selected.Count -ne 30) {
        throw "Pilot selection must be schema p5-pilot-selection-v1 with 30 images."
    }

    $samples = @()
    foreach ($item in $selected) {
        if (-not ($item.file_name -is [string]) -or
            -not ($item.sha256 -is [string])) {
            throw "Pilot selection contains an invalid image binding."
        }
        $image = Get-Item -LiteralPath (Join-Path $imagesPath $item.file_name) `
            -ErrorAction Stop
        $actualHash = (Get-FileHash -LiteralPath $image.FullName `
            -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $item.sha256.ToLowerInvariant()) {
            throw "Pilot image hash mismatch: $($item.file_name)"
        }
        $samples += [ordered]@{
            file_name = $item.file_name
            path = $image.FullName
            sha256 = $actualHash
        }
    }

    $inputManifestPath = Join-Path $EvidenceRoot "pilot-input-manifest.json"
    [ordered]@{
        root = "."
        groundTruthAvailable = $false
        samples = $samples
    } | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $inputManifestPath -Encoding UTF8

    $detectorConfig = Get-Content -Raw -LiteralPath $configTemplate.FullName `
        -Encoding UTF8 | ConvertFrom-Json
    $detectorConfig.enginePath = $engine.FullName
    $runtimeConfigPath = Join-Path $EvidenceRoot "detector-config.json"
    $detectorConfig | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $runtimeConfigPath -Encoding UTF8

    & $trtexec --loadEngine="$($engine.FullName)" --getPlanVersionOnly 2>&1 |
        Tee-Object -FilePath (Join-Path $EvidenceRoot "engine-plan-version.log")
    if ($LASTEXITCODE -ne 0) {
        throw "TensorRT engine plan-version check failed with exit $LASTEXITCODE."
    }

    $runtimePaths = @(
        $executable.DirectoryName,
        (Join-Path $tensorRtRoot "bin"),
        (Join-Path $env:CUDA_PATH "bin\x64"),
        (Join-Path $env:OPENCV_PATH "build\x64\vc16\bin"),
        (Join-Path $env:OPENCV_PATH "x64\vc16\bin"),
        (Join-Path ([Environment]::GetEnvironmentVariable(
            "CIGVISION_QT_ROOT", "User")) "bin"),
        (Join-Path ([Environment]::GetEnvironmentVariable(
            "CIGVISION_HALCON_RELEASE_ROOT", "User")) "bin\x64-win64"),
        "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
    ) | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_)
    }
    $env:PATH = (($runtimePaths -join ";") + ";" + $env:PATH)

    $batchOutput = Join-Path $EvidenceRoot "batch-output"
    New-Item -ItemType Directory -Path $batchOutput | Out-Null
    $batchArguments = @(
        "--tensorrt-batch-manifest", "`"$inputManifestPath`"",
        "--offline-output", "`"$batchOutput`"",
        "--detector-config", "`"$runtimeConfigPath`""
    )
    ($executable.FullName + " " + ($batchArguments -join " ")) |
        Set-Content -LiteralPath (Join-Path $EvidenceRoot "command.txt") -Encoding UTF8

    $startedAt = Get-Date
    $process = Start-Process -FilePath $executable.FullName `
        -ArgumentList $batchArguments -PassThru -Wait
    $finishedAt = Get-Date
    if ($process.ExitCode -ne 0) {
        throw "CigVision TensorRT pilot exited with code $($process.ExitCode)."
    }

    $summaryPath = Join-Path $batchOutput "summary.json"
    $summary = Get-Content -Raw -LiteralPath $summaryPath -Encoding UTF8 |
        ConvertFrom-Json
    $resultFiles = @(Get-ChildItem -LiteralPath $batchOutput `
        -Filter "frame-*.json" -File)
    $pngFiles = @(Get-ChildItem -LiteralPath $batchOutput -Filter "*.png" -File)
    $sourcePng = @($pngFiles | Where-Object Name -Match '^frame-[0-9]{8}\.png$')
    $annotatedPng = @($pngFiles |
        Where-Object Name -Match '^frame-[0-9]{8}-annotated\.png$')
    if ($summary.state -ne 1 -or $summary.statistics.processed -ne 30 -or
        $summary.statistics.error -ne 0 -or $summary.statistics.dropped -ne 0 -or
        $summary.statistics.saveFailures -ne 0 -or $resultFiles.Count -ne 30 -or
        $sourcePng.Count -ne 30 -or $annotatedPng.Count -ne 30) {
        throw "TensorRT pilot output counts or summary invariants failed."
    }

    $pilotManifest = Join-Path $package.FullName "pilot-manifest.json"
    $predictionsPath = Join-Path $EvidenceRoot "tensorrt-predictions.coco.json"
    & python (Join-Path $PSScriptRoot "p5_dataset_tools.py") preannotate `
        --manifest $pilotManifest --p4-results $batchOutput `
        --output $predictionsPath --split pilot --class-catalog `
        (Join-Path $repoRoot "config\p5-class-catalog.json")
    if ($LASTEXITCODE -ne 0) {
        throw "P5 preannotation conversion failed with exit $LASTEXITCODE."
    }
    & python (Join-Path $PSScriptRoot "p5_dataset_tools.py") validate-annotations `
        --manifest $pilotManifest --annotations $predictionsPath `
        --split pilot `
        --class-catalog (Join-Path $repoRoot "config\p5-class-catalog.json")
    if ($LASTEXITCODE -ne 0) {
        throw "P5 prediction validation failed with exit $LASTEXITCODE."
    }

    $predictions = Get-Content -Raw -LiteralPath $predictionsPath -Encoding UTF8 |
        ConvertFrom-Json
    $elapsedMs = @($resultFiles | ForEach-Object {
        $result = Get-Content -Raw -LiteralPath $_.FullName -Encoding UTF8 |
            ConvertFrom-Json
        [double]$result.elapsedMicros / 1000.0
    })
    $classCounts = [ordered]@{}
    foreach ($annotation in $predictions.annotations) {
        $key = [string]$annotation.category_id
        if (-not $classCounts.Contains($key)) { $classCounts[$key] = 0 }
        $classCounts[$key] += 1
    }

    [ordered]@{
        schema_version = "p5-tensorrt-pilot-runtime-v1"
        status = "PASS"
        backend = "TensorRT"
        tensorrt_version = $tensorRtVersion
        formal_p4_tensorrt_evidence = $true
        engine_plan_version_verified = $true
        started_at = $startedAt.ToString("o")
        finished_at = $finishedAt.ToString("o")
        exit_code = $process.ExitCode
        sample_count = 30
        result_json_count = $resultFiles.Count
        source_png_count = $sourcePng.Count
        annotated_png_count = $annotatedPng.Count
        processed = $summary.statistics.processed
        errors = $summary.statistics.error
        dropped = $summary.statistics.dropped
        save_failures = $summary.statistics.saveFailures
        predicted_boxes = @($predictions.annotations).Count
        class_counts = $classCounts
        input_images = @($samples | ForEach-Object {
            [ordered]@{ file_name = $_.file_name; sha256 = $_.sha256 }
        })
        elapsed_ms = [ordered]@{
            minimum = ($elapsedMs | Measure-Object -Minimum).Minimum
            median = Get-NearestRankPercentile $elapsedMs 0.5
            p95 = Get-NearestRankPercentile $elapsedMs 0.95
            maximum = ($elapsedMs | Measure-Object -Maximum).Maximum
            mean = ($elapsedMs | Measure-Object -Average).Average
            sample_count = $elapsedMs.Count
        }
        runner_script = Get-FileRecord $PSCommandPath
        safety_config = Get-FileRecord $configIni
        executable = Get-FileRecord $executable.FullName
        model = Get-FileRecord $model.FullName
        engine = Get-FileRecord $engine.FullName
        detector_config = Get-FileRecord $runtimeConfigPath
        input_manifest = Get-FileRecord $inputManifestPath
        predictions = Get-FileRecord $predictionsPath
        accuracy_claimed = $false
        ground_truth_available = $false
        safety = [ordered]@{
            reject_enabled = $false
            hardware_initialized = $false
            reject_commands_generated = $false
        }
    } | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath (Join-Path $EvidenceRoot "runtime-record.json") `
            -Encoding UTF8

    $exitCode = 0
}
catch {
    $failures.Add([ordered]@{
        message = $_.Exception.Message
        type = $_.Exception.GetType().FullName
    })
    Write-Error $_ -ErrorAction Continue
}
finally {
    $failureJson = if ($failures.Count -eq 0) {
        "[]"
    } else {
        ConvertTo-Json -InputObject @($failures | ForEach-Object { $_ }) -Depth 5
    }
    $failureJson |
        Set-Content -LiteralPath (Join-Path $EvidenceRoot "failures.json") -Encoding UTF8
    if ($transcriptStarted) {
        Stop-Transcript | Out-Null
    }

    $files = @(Get-ChildItem -LiteralPath $EvidenceRoot -File -Recurse |
        Where-Object Name -Ne "manifest.json" | Sort-Object FullName)
    $entries = @($files | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($EvidenceRoot.Length + 1).Replace("\", "/")
            length = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    })
    [ordered]@{
        schema_version = "p5-tensorrt-pilot-runtime-manifest-v1"
        status = if ($exitCode -eq 0) { "PASS" } else { "FAIL" }
        command_exit_code = $exitCode
        accuracy_claimed = $false
        ground_truth_available = $false
        failure_count = $failures.Count
        entries = $entries
    } | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}

Write-Output "Evidence: $EvidenceRoot"
Write-Output ("Manifest SHA-256: " +
    (Get-FileHash -LiteralPath (Join-Path $EvidenceRoot "manifest.json") `
        -Algorithm SHA256).Hash)
exit $exitCode
