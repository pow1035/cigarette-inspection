param(
    [ValidateSet("Debug", "Release", "All")]
    [string]$Configuration = "All",
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $repoRoot "tests\CigVision.Contracts\CigVision.Contracts.vcxproj"
$contractsHeader = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "InspectionContracts.h" -File |
    Select-Object -First 1
if ($null -eq $contractsHeader) {
    throw "InspectionContracts.h was not found under $repoRoot."
}
$coreDirectory = $contractsHeader.DirectoryName
$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue

if ($null -eq $msbuild) {
    throw "msbuild.exe is not available. Run this script in a VS 2022 Developer PowerShell."
}
if (-not (Test-Path -LiteralPath $projectPath)) {
    throw "Contract test project was not found: $projectPath"
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceRoot = Join-Path $repoRoot "artifacts\p2-contracts-$timestamp"
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$configurations = if ($Configuration -eq "All") { @("Debug", "Release") } else { @($Configuration) }
$builds = New-Object System.Collections.Generic.List[object]
$failures = New-Object System.Collections.Generic.List[object]
$startedAt = Get-Date
$manifestPath = Join-Path $EvidenceRoot "manifest.json"
$commandLine = $MyInvocation.Line
if ([string]::IsNullOrWhiteSpace($commandLine)) {
    $commandLine = "powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Configuration $Configuration -EvidenceRoot `"$EvidenceRoot`""
}

try {
    foreach ($current in $configurations) {
        $configurationRoot = Join-Path $EvidenceRoot $current
        $outputDirectory = Join-Path $configurationRoot "bin"
        $intermediateDirectory = Join-Path $configurationRoot "obj"
        New-Item -ItemType Directory -Force -Path $outputDirectory, $intermediateDirectory | Out-Null

        $buildLog = Join-Path $EvidenceRoot "msbuild-$($current.ToLowerInvariant()).log"
        $buildArgs = @(
            $projectPath,
            "/m",
            "/t:Rebuild",
            "/p:Configuration=$current",
            "/p:Platform=x64",
            "/p:OutDir=$outputDirectory\",
            "/p:IntDir=$intermediateDirectory\",
            "/v:minimal"
        )
        & $msbuild.Source @buildArgs 2>&1 | Tee-Object -FilePath $buildLog
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0) {
            $failures.Add([pscustomobject]@{
                Configuration = $current
                Step = "msbuild"
                ExitCode = $buildExitCode
                Log = $buildLog
            })
            continue
        }

        $executable = Join-Path $outputDirectory "CigVision.Contracts.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            $failures.Add([pscustomobject]@{
                Configuration = $current
                Step = "output-check"
                ExitCode = 1
                Detail = $executable
            })
            continue
        }

        $testLog = Join-Path $EvidenceRoot "test-$($current.ToLowerInvariant()).log"
        & $executable 2>&1 | Tee-Object -FilePath $testLog
        $testExitCode = $LASTEXITCODE
        if ($testExitCode -ne 0) {
            $failures.Add([pscustomobject]@{
                Configuration = $current
                Step = "contract-tests"
                ExitCode = $testExitCode
                Log = $testLog
            })
        }

        $builds.Add([pscustomobject]@{
            Configuration = $current
            BuildExitCode = $buildExitCode
            TestExitCode = $testExitCode
            BuildLog = $buildLog
            TestLog = $testLog
            Executable = [pscustomobject]@{
                Path = $executable
                Length = (Get-Item -LiteralPath $executable).Length
                Sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
            }
        })
    }
}
catch {
    $failures.Add([pscustomobject]@{
        Configuration = $Configuration
        Step = "unhandled-exception"
        ExitCode = 1
        Detail = $_.Exception.ToString()
    })
}
finally {
    $contractFiles = @(
        $contractsHeader.FullName,
        (Join-Path $coreDirectory "InspectionInterfaces.h"),
        (Join-Path $coreDirectory "BoundedQueue.h"),
        (Join-Path $repoRoot "tests\CigVision.Contracts\ContractTests.cpp"),
        $projectPath,
        $PSCommandPath
    ) | ForEach-Object {
        $path = $_
        [pscustomobject]@{
            Path = $path
            Length = (Get-Item -LiteralPath $path).Length
            Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        }
    }

    $evidence = [pscustomobject]@{
        CommandLine = $commandLine
        OverallResult = if ($failures.Count -eq 0) { "passed" } else { "failed" }
        OverallCommandExitCode = if ($failures.Count -eq 0) { 0 } else { 1 }
        StartedAt = $startedAt.ToString("o")
        FinishedAt = (Get-Date).ToString("o")
        MSBuild = $msbuild.Source
        GitHead = (& git -C $repoRoot rev-parse HEAD)
        GitStatus = @(& git -C $repoRoot status --short --untracked-files=all)
        ContractFiles = $contractFiles
        Builds = $builds
        Failures = $failures
    }
    $evidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}

if ($failures.Count -ne 0) {
    Write-Error "P2 contract tests failed. Evidence: $EvidenceRoot"
    exit 1
}

Write-Host "PASS P2 contract tests. Evidence: $EvidenceRoot"
