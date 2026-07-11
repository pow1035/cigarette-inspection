param(
    [string]$EvidenceRoot = "",
    [string]$P4Results = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $PSScriptRoot "p5_dataset_tools.py"
$tests = Join-Path $repoRoot "tests\p5"
$dataRoot = Join-Path $repoRoot "04_测试数据与样例图片"
$imageRoot = Join-Path $dataRoot "推理测试图片"
$legacyRoot = Join-Path $dataRoot "缺陷样例_predict6"
$classCatalog = Join-Path $repoRoot "config\p5-class-catalog.json"

if (-not (Test-Path -LiteralPath $tool)) { throw "P5 dataset tool is missing: $tool" }
if (-not (Test-Path -LiteralPath $imageRoot)) { throw "P5 image directory is missing: $imageRoot" }
if (-not (Test-Path -LiteralPath $legacyRoot)) { throw "Legacy prediction directory is missing: $legacyRoot" }
if (-not (Test-Path -LiteralPath $classCatalog)) { throw "P5 class catalog is missing: $classCatalog" }
$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $python) { $python = Get-Command python -ErrorAction Stop }
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repoRoot ("artifacts\p5-data-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) { throw "EvidenceRoot must not already exist: $EvidenceRoot" }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

$startedAt = Get-Date
$steps = New-Object System.Collections.Generic.List[object]
$failures = New-Object System.Collections.Generic.List[object]

function Invoke-RecordedStep {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [string]$LogPath,
        [int]$ExpectedExitCode = 0
    )
    $commandText = $python.Source + " " + (($Arguments | ForEach-Object {
        if ($_ -match '\s') { '"' + $_ + '"' } else { $_ }
    }) -join " ")
    $commandText | Set-Content -LiteralPath ($LogPath + ".command.txt") -Encoding UTF8
    if ($Arguments.Where({ $_.Contains('"') }).Count -ne 0) {
        throw "Recorded Python arguments must not contain quote characters."
    }
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
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $exitCode = $process.ExitCode
    $process.Dispose()
    $combined = $stdout + $stderr
    $combined | Set-Content -LiteralPath $LogPath -Encoding UTF8
    if (-not [string]::IsNullOrWhiteSpace($combined)) { Write-Host $combined.TrimEnd() }
    $steps.Add([pscustomobject]@{
        Name = $Name; ExitCode = $exitCode; ExpectedExitCode = $ExpectedExitCode; Log = $LogPath
    })
    if ($exitCode -ne $ExpectedExitCode) {
        $failures.Add([pscustomobject]@{
            Name = $Name; ExitCode = $exitCode; ExpectedExitCode = $ExpectedExitCode; Log = $LogPath
        })
    }
}

$sourcePaths = @(
    $tool,
    $MyInvocation.MyCommand.Path,
    (Join-Path $tests "test_p5_dataset_tools.py"),
    $classCatalog,
    (Join-Path $repoRoot "config\p5-ground-truth-attestation.template.json"),
    (Join-Path $repoRoot "docs\p5-source-inventory.md"),
    (Join-Path $repoRoot "docs\p5-labeling-guide.md")
)

function Get-SourceSnapshot {
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

$sourceFilesBefore = Get-SourceSnapshot $sourcePaths

try {
    Invoke-RecordedStep "unit-tests" @("-m", "unittest", "discover", "-s", $tests, "-p", "test_*.py", "-v") `
        (Join-Path $EvidenceRoot "unit-tests.log")
    $unitTestText = Get-Content -LiteralPath (Join-Path $EvidenceRoot "unit-tests.log") -Raw -Encoding UTF8
    $unitTestMatch = [regex]::Match($unitTestText, 'Ran\s+(\d+)\s+tests?')
    $unitTestCount = if ($unitTestMatch.Success) { [int]$unitTestMatch.Groups[1].Value } else { 0 }
    if ($unitTestCount -le 0) {
        $failures.Add([pscustomobject]@{
            Name = "unit-test-discovery"; ExitCode = 1; ExpectedExitCode = 0
            Log = (Join-Path $EvidenceRoot "unit-tests.log")
        })
    }

    $auditOutput = Join-Path $EvidenceRoot "audit"
    Invoke-RecordedStep "dataset-audit" @($tool, "audit", "--images", $imageRoot,
        "--legacy-predictions", $legacyRoot, "--class-catalog", $classCatalog,
        "--output", $auditOutput) `
        (Join-Path $EvidenceRoot "audit.log")

    Invoke-RecordedStep "unreviewed-ground-truth-rejected" @($tool,
        "validate-annotations", "--manifest", (Join-Path $auditOutput "dataset-manifest.json"),
        "--annotations", (Join-Path $auditOutput "annotations-empty.coco.json"),
        "--class-catalog", $classCatalog, "--require-reviewed",
        "--output", (Join-Path $EvidenceRoot "unreviewed-validation.json")) `
        (Join-Path $EvidenceRoot "unreviewed-validation.log") 2

    if (-not [string]::IsNullOrWhiteSpace($P4Results)) {
        $P4Results = [System.IO.Path]::GetFullPath($P4Results)
        Invoke-RecordedStep "p4-preannotation" @($tool, "preannotate", "--manifest",
            (Join-Path $auditOutput "dataset-manifest.json"), "--p4-results", $P4Results,
            "--class-catalog", $classCatalog,
            "--output", (Join-Path $EvidenceRoot "preannotations.coco.json")) `
            (Join-Path $EvidenceRoot "preannotation.log")
    }

    $sourceFilesAfter = Get-SourceSnapshot $sourcePaths
    $beforeByPath = @{}
    $sourceFilesBefore | ForEach-Object { $beforeByPath[$_.Path] = $_.Sha256 }
    $sourceFilesStable = $true
    foreach ($sourceFile in $sourceFilesAfter) {
        if ($beforeByPath[$sourceFile.Path] -ne $sourceFile.Sha256) {
            $sourceFilesStable = $false
            $failures.Add([pscustomobject]@{
                Name = "source-stability"; ExitCode = 1; ExpectedExitCode = 0
                Log = $sourceFile.Path
            })
        }
    }
    $evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceRoot -Recurse -File |
        Where-Object Name -ne "manifest.json" | ForEach-Object {
            [pscustomobject]@{
                Path = $_.FullName
                Length = $_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        })
    $summary = if (Test-Path -LiteralPath (Join-Path $auditOutput "dataset-summary.json")) {
        Get-Content -Raw -LiteralPath (Join-Path $auditOutput "dataset-summary.json") | ConvertFrom-Json
    } else { $null }
    if ($null -eq $summary -or $summary.image_count -ne 116 -or
            $summary.unique_sha256_count -ne 113 -or
            $summary.duplicate_group_count -ne 3 -or
            $summary.legacy_prediction_pair_count -ne 116) {
        $failures.Add([pscustomobject]@{
            Name = "dataset-invariants"; ExitCode = 1; ExpectedExitCode = 0
            Log = (Join-Path $EvidenceRoot "audit\dataset-summary.json")
        })
    }
    $manifest = [ordered]@{
        SchemaVersion = "p5-data-evidence-v1"
        StartedAt = $startedAt.ToString("o")
        CompletedAt = (Get-Date).ToString("o")
        Result = if ($failures.Count -eq 0) { "passed" } else { "failed" }
        RealHardwareUsed = $false
        RealRejectEnabled = $false
        SourceImagesModified = $false
        ImageRoot = $imageRoot
        LegacyPredictionRoot = $legacyRoot
        ClassCatalog = $classCatalog
        P4Results = if ([string]::IsNullOrWhiteSpace($P4Results)) { $null } else { $P4Results }
        DatasetSummary = $summary
        UnitTestCount = $unitTestCount
        Steps = $steps
        Failures = $failures
        SourceFilesStable = $sourceFilesStable
        SourceFilesBefore = $sourceFilesBefore
        SourceFiles = $sourceFilesAfter
        EvidenceFiles = $evidenceFiles
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath `
        (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}
catch {
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $EvidenceRoot "fatal-error.log") -Encoding UTF8
    throw
}

Write-Host "P5 data evidence: $EvidenceRoot"
if ($failures.Count -ne 0) {
    Write-Error "P5 data baseline failed in $($failures.Count) step(s)."
    exit 1
}
Write-Host "P5 data baseline passed."
exit 0
