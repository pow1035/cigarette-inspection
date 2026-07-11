param(
    [string]$EvidenceRoot = "",
    [string]$Manifest = "",
    [string]$Preannotations = "",
    [string]$P4Results = "",
    [int]$Size = 30
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $PSScriptRoot "p5_dataset_tools.py"
$tests = Join-Path $repoRoot "tests\p5"
$classCatalog = Join-Path $repoRoot "config\p5-class-catalog.json"
if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = Join-Path $repoRoot "artifacts\p5-data-20260711-170640\audit\dataset-manifest.json"
}
if ([string]::IsNullOrWhiteSpace($Preannotations)) {
    $Preannotations = Join-Path $repoRoot "artifacts\p5-data-20260711-170640\preannotations.coco.json"
}
if ([string]::IsNullOrWhiteSpace($P4Results)) {
    $P4Results = Join-Path $repoRoot "artifacts\p4-tensorrt-20260711-150510\batch-output"
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repoRoot ("artifacts\p5-pilot-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
$manifestInput = Get-Content -LiteralPath $Manifest -Raw -Encoding UTF8 | ConvertFrom-Json
$imageRoot = $manifestInput.image_root
if ([string]::IsNullOrWhiteSpace($imageRoot)) { throw "Manifest image_root is missing." }

foreach ($path in @($tool, $tests, $classCatalog, $imageRoot, $Manifest, $Preannotations, $P4Results)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required P5 pilot input is missing: $path" }
}
if ($Size -le 0) { throw "Size must be positive." }
if (Test-Path -LiteralPath $EvidenceRoot) { throw "EvidenceRoot must not already exist: $EvidenceRoot" }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$packageRoot = Join-Path $EvidenceRoot "review-package"
$imagesOutput = Join-Path $packageRoot "images"
$previewsOutput = Join-Path $packageRoot "previews"
New-Item -ItemType Directory -Path $imagesOutput, $previewsOutput | Out-Null

$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $python) { $python = Get-Command python -ErrorAction Stop }
$startedAt = Get-Date
$steps = New-Object System.Collections.Generic.List[object]
$failures = New-Object System.Collections.Generic.List[object]
$sourcePaths = @(
    $tool,
    $MyInvocation.MyCommand.Path,
    (Join-Path $tests "test_p5_dataset_tools.py"),
    $classCatalog,
    $Manifest,
    $Preannotations
)

function Invoke-RecordedPython {
    param([string]$Name, [string[]]$Arguments, [string]$LogPath)
    ($python.Source + " " + (($Arguments | ForEach-Object {
        if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
    }) -join " ")) | Set-Content -LiteralPath ($LogPath + ".command.txt") -Encoding UTF8
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $python.Source
    $startInfo.Arguments = (($Arguments | ForEach-Object { '"' + $_ + '"' }) -join ' ')
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $combined = $stdoutTask.Result + $stderrTask.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    $combined | Set-Content -LiteralPath $LogPath -Encoding UTF8
    if (-not [string]::IsNullOrWhiteSpace($combined)) { Write-Host $combined.TrimEnd() }
    $steps.Add([pscustomobject]@{ Name = $Name; ExitCode = $exitCode; Log = $LogPath })
    if ($exitCode -ne 0) {
        $failures.Add([pscustomobject]@{ Name = $Name; ExitCode = $exitCode; Log = $LogPath })
    }
}

function Get-ImageSnapshot {
    return @(Get-ChildItem -LiteralPath $imageRoot -File | Sort-Object Name | ForEach-Object {
        [pscustomobject]@{
            Path = $_.FullName
            Length = $_.Length
            Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })
}

function Get-FileSnapshot {
    param([string[]]$Paths)
    return @($Paths | ForEach-Object {
        $item = Get-Item -LiteralPath $_
        [pscustomobject]@{
            Path = $item.FullName
            Length = $item.Length
            Sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        }
    })
}

$sourceImagesBefore = Get-ImageSnapshot
$sourceFilesBefore = Get-FileSnapshot $sourcePaths
try {
    Invoke-RecordedPython "unit-tests" @(
        "-m", "unittest", "discover", "-s", $tests, "-p", "test_*.py", "-v"
    ) (Join-Path $EvidenceRoot "unit-tests.log")
    $testText = Get-Content -LiteralPath (Join-Path $EvidenceRoot "unit-tests.log") -Raw -Encoding UTF8
    $testMatch = [regex]::Match($testText, 'Ran\s+(\d+)\s+tests?')
    $unitTestCount = if ($testMatch.Success) { [int]$testMatch.Groups[1].Value } else { 0 }
    if ($unitTestCount -le 0) {
        $failures.Add([pscustomobject]@{ Name = "unit-test-discovery"; ExitCode = 1; Log = "unit-tests.log" })
    }

    Invoke-RecordedPython "select-pilot" @(
        $tool, "select-pilot", "--manifest", $Manifest, "--preannotations", $Preannotations,
        "--class-catalog", $classCatalog, "--size", [string]$Size, "--output", $packageRoot
    ) (Join-Path $EvidenceRoot "select-pilot.log")

    $selectionPath = Join-Path $packageRoot "pilot-selection.json"
    $pilotManifestPath = Join-Path $packageRoot "pilot-manifest.json"
    $pilotCocoPath = Join-Path $packageRoot "pilot-preannotations.coco.json"
    $reviewCsvPath = Join-Path $packageRoot "pilot-review.csv"
    foreach ($path in @($selectionPath, $pilotManifestPath, $pilotCocoPath, $reviewCsvPath)) {
        if (-not (Test-Path -LiteralPath $path)) { throw "Pilot package output is missing: $path" }
    }
    $selection = Get-Content -LiteralPath $selectionPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $pilotManifest = Get-Content -LiteralPath $pilotManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $pilotCoco = Get-Content -LiteralPath $pilotCocoPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $reviewRows = @(Import-Csv -LiteralPath $reviewCsvPath -Encoding UTF8)
    if ($selection.selected_count -ne $Size -or $selection.uncovered_features.Count -ne 0 -or
            $selection.canonical_only -ne $true -or $selection.ground_truth_available -ne $false -or
            $selection.accuracy_metrics_claimed -ne $false) {
        $failures.Add([pscustomobject]@{ Name = "pilot-selection-invariants"; ExitCode = 1; Log = $selectionPath })
    }
    $pilotCanonicalCount = @($pilotManifest.images | Where-Object {
        $_.canonical -eq $true -and $_.split -eq "pilot"
    }).Count
    if ($pilotCanonicalCount -ne $Size -or $reviewRows.Count -ne $Size -or
            @($reviewRows | Where-Object review_status -ne "pending").Count -ne 0) {
        $failures.Add([pscustomobject]@{ Name = "pilot-package-completeness"; ExitCode = 1; Log = $packageRoot })
    }
    $reviewCount = @($pilotCoco.images | Where-Object cigarette_decision -eq "REVIEW").Count
    if ($reviewCount -le 0 -or @($pilotCoco.images | Where-Object is_ground_truth -ne $false).Count -ne 0 -or
            @($pilotCoco.annotations | Where-Object is_ground_truth -ne $false).Count -ne 0) {
        $failures.Add([pscustomobject]@{ Name = "pilot-ground-truth-guard"; ExitCode = 1; Log = $pilotCocoPath })
    }

    $previewProvenancePath = Join-Path $packageRoot "pilot-preview-provenance.json"
    Invoke-RecordedPython "verify-pilot-previews" @(
        $tool, "verify-pilot-previews", "--pilot", $pilotCocoPath,
        "--p4-results", $P4Results, "--output", $previewProvenancePath
    ) (Join-Path $EvidenceRoot "verify-pilot-previews.log")
    $previewProvenance = Get-Content -LiteralPath $previewProvenancePath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($previewProvenance.valid -ne $true -or $previewProvenance.verified_record_count -ne $Size -or
            $previewProvenance.errors.Count -ne 0) {
        $failures.Add([pscustomobject]@{ Name = "preview-provenance"; ExitCode = 1; Log = $previewProvenancePath })
    }
    $previewBySource = @{}
    foreach ($record in $previewProvenance.records) { $previewBySource[$record.file_name] = $record }
    $copiedPreviewBindings = New-Object System.Collections.Generic.List[object]
    foreach ($item in $selection.selected) {
        $source = Join-Path $imageRoot $item.file_name
        $copy = Join-Path $imagesOutput $item.file_name
        Copy-Item -LiteralPath $source -Destination $copy
        $copiedHash = (Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($copiedHash -ne $item.sha256) {
            $failures.Add([pscustomobject]@{ Name = "copied-image-hash"; ExitCode = 1; Log = $copy })
        }
        if ($previewBySource.ContainsKey($item.file_name)) {
            $previewRecord = $previewBySource[$item.file_name]
            $previewName = [System.IO.Path]::GetFileNameWithoutExtension($item.file_name) + ".annotated.png"
            $previewCopy = Join-Path $previewsOutput $previewName
            Copy-Item -LiteralPath $previewRecord.preview_path -Destination $previewCopy
            $previewCopyHash = (Get-FileHash -LiteralPath $previewCopy -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($previewCopyHash -ne $previewRecord.preview_sha256) {
                $failures.Add([pscustomobject]@{ Name = "copied-preview-hash"; ExitCode = 1; Log = $previewCopy })
            }
            $copiedPreviewBindings.Add([pscustomobject]@{
                FileName = $item.file_name
                SourcePath = $previewRecord.preview_path
                SourceSha256 = $previewRecord.preview_sha256
                CopiedPath = $previewCopy
                CopiedSha256 = $previewCopyHash
                FrameJsonPath = $previewRecord.frame_json_path
                FrameJsonSha256 = $previewRecord.frame_json_sha256
            })
        }
    }
    $copiedImageCount = @(Get-ChildItem -LiteralPath $imagesOutput -File).Count
    $previewCount = @(Get-ChildItem -LiteralPath $previewsOutput -File).Count
    if ($copiedImageCount -ne $Size -or $previewCount -ne $Size) {
        $failures.Add([pscustomobject]@{ Name = "review-package-files"; ExitCode = 1; Log = $packageRoot })
    }

    $sourceImagesAfter = Get-ImageSnapshot
    $sourceImagesStable = (
        ($sourceImagesBefore | ConvertTo-Json -Depth 4 -Compress) -eq
        ($sourceImagesAfter | ConvertTo-Json -Depth 4 -Compress))
    if (-not $sourceImagesStable) {
        $failures.Add([pscustomobject]@{ Name = "source-image-stability"; ExitCode = 1; Log = $imageRoot })
    }
    $sourceFilesAfter = Get-FileSnapshot $sourcePaths
    $sourceFilesStable = (
        ($sourceFilesBefore | ConvertTo-Json -Depth 4 -Compress) -eq
        ($sourceFilesAfter | ConvertTo-Json -Depth 4 -Compress))
    if (-not $sourceFilesStable) {
        $failures.Add([pscustomobject]@{ Name = "source-file-stability"; ExitCode = 1; Log = $repoRoot })
    }

    $evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceRoot -Recurse -File |
        Where-Object Name -ne "manifest.json" | ForEach-Object {
            [pscustomobject]@{
                Path = $_.FullName
                Length = $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        })
    $evidenceManifest = [ordered]@{
        SchemaVersion = "p5-pilot-evidence-v1"
        StartedAt = $startedAt.ToString("o")
        CompletedAt = (Get-Date).ToString("o")
        Result = if ($failures.Count -eq 0) { "passed" } else { "failed" }
        RealHardwareUsed = $false
        RealRejectEnabled = $false
        GroundTruthAvailable = $false
        AccuracyMetricsClaimed = $false
        RequestedSize = $Size
        SelectedCount = $selection.selected_count
        ReviewCount = $reviewCount
        CopiedImageCount = $copiedImageCount
        PreviewCount = $previewCount
        UnitTestCount = $unitTestCount
        SourceImagesStable = $sourceImagesStable
        SourceFilesStable = $sourceFilesStable
        PilotCanonicalCount = $pilotCanonicalCount
        ReviewCsvRowCount = $reviewRows.Count
        Inputs = [ordered]@{
            Manifest = [ordered]@{ Path = $Manifest; Sha256 = (Get-FileHash -LiteralPath $Manifest -Algorithm SHA256).Hash }
            Preannotations = [ordered]@{ Path = $Preannotations; Sha256 = (Get-FileHash -LiteralPath $Preannotations -Algorithm SHA256).Hash }
            P4Results = $P4Results
            ClassCatalog = [ordered]@{ Path = $classCatalog; Sha256 = (Get-FileHash -LiteralPath $classCatalog -Algorithm SHA256).Hash }
        }
        SourceFiles = $sourceFilesAfter
        PreviewBindings = $copiedPreviewBindings
        Steps = $steps
        Failures = $failures
        EvidenceFiles = $evidenceFiles
    }
    $evidenceManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath `
        (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}
catch {
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $EvidenceRoot "fatal-error.log") -Encoding UTF8
    throw
}

Write-Host "P5 pilot evidence: $EvidenceRoot"
if ($failures.Count -ne 0) {
    Write-Error "P5 pilot evidence failed in $($failures.Count) check(s)."
    exit 1
}
Write-Host "P5 pilot evidence passed."
exit 0
