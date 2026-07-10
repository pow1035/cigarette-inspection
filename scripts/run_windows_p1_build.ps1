param(
    [ValidateSet("Debug", "Release", "All")]
    [string]$Configuration = "All",
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot "01_上位机_QT_新版_CigVision\源码"
$solutionPath = Join-Path $sourceDir "CigVision.sln"

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceRoot = Join-Path $repoRoot "artifacts\p1-windows-$timestamp"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
if ($null -eq $msbuild) {
    throw "msbuild.exe is not in PATH. Run this script from VS 2022 Developer PowerShell."
}

$configurations = if ($Configuration -eq "All") {
    @("Debug", "Release")
} else {
    @($Configuration)
}

$manifest = New-Object System.Collections.Generic.List[object]
foreach ($current in $configurations) {
    & (Join-Path $PSScriptRoot "check_windows_build_env.ps1") -Configuration $current

    $logPath = Join-Path $EvidenceRoot "msbuild-$($current.ToLowerInvariant()).log"
    & $msbuild.Source $solutionPath /m /t:Rebuild "/p:Configuration=$current" /p:Platform=x64 /v:minimal 2>&1 |
        Tee-Object -FilePath $logPath
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        throw "MSBuild failed for $current with exit code $buildExitCode. See $logPath"
    }

    $outputDir = Join-Path $sourceDir "x64\$current"
    $requiredOutputs = @(
        (Join-Path $outputDir "CigVision.exe"),
        (Join-Path $outputDir "process.dll")
    )
    foreach ($output in $requiredOutputs) {
        if (-not (Test-Path $output)) {
            throw "Required output is missing after $current build: $output"
        }
    }

    $files = Get-ChildItem -Path $outputDir -File | Sort-Object Name | ForEach-Object {
        [pscustomobject]@{
            Name = $_.Name
            Length = $_.Length
            Sha256 = (Get-FileHash -Algorithm SHA256 -Path $_.FullName).Hash
        }
    }
    $manifest.Add([pscustomobject]@{
        Configuration = $current
        BuildExitCode = $buildExitCode
        Log = $logPath
        OutputDirectory = $outputDir
        Files = $files
    })
}

$qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
$environment = [pscustomobject]@{
    CapturedAt = (Get-Date).ToString("o")
    ComputerName = $env:COMPUTERNAME
    Windows = [System.Environment]::OSVersion.VersionString
    MSBuild = $msbuild.Source
    QtVersion = if ($qmake) { (& $qmake.Source -query QT_VERSION) } else { "not-found" }
    GitHead = (& git -C $repoRoot rev-parse HEAD)
}

$evidence = [pscustomobject]@{
    Environment = $environment
    Builds = $manifest
}
$manifestPath = Join-Path $EvidenceRoot "manifest.json"
$evidence | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path $manifestPath

Write-Host "PASS P1 Windows builds. Evidence: $EvidenceRoot"
