param(
    [string]$Manifest = "",
    [string]$EvidenceRoot = "",
    [ValidateRange(0, 60000000)]
    [int]$RejectDelayMicros = 500,
    [ValidateRange(1, 65536)]
    [int]$QueueCapacity = 4,
    [string]$TargetOutput = "simulation-reject",
    [switch]$SkipBuild,
    [string]$Executable = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "CigVision.sln" -File |
    Where-Object { $_.FullName -like "*CigVision*" } |
    Select-Object -First 1 -ExpandProperty FullName
if ([string]::IsNullOrWhiteSpace($solutionPath)) {
    throw "CigVision.sln was not found under $repoRoot."
}
$sourceDirectory = Split-Path -Parent $solutionPath
$driver = Join-Path $PSScriptRoot "p6_windows_simulation_evidence.py"
$buildScript = Join-Path $PSScriptRoot "run_windows_p1_build.ps1"
$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $python) { $python = Get-Command python -ErrorAction Stop }
if (-not (Test-Path -LiteralPath $driver)) { throw "P6 evidence driver is missing: $driver" }

if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = Join-Path $repoRoot "tests\fixtures\p6-simulation-samples.json"
}
$Manifest = [System.IO.Path]::GetFullPath($Manifest)
if (-not (Test-Path -LiteralPath $Manifest -PathType Leaf)) {
    throw "P6 simulation manifest is missing: $Manifest"
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repoRoot ("artifacts\p6-windows-simulation-" +
        (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "EvidenceRoot must not already exist: $EvidenceRoot"
}
if ($TargetOutput.Trim().Length -eq 0 -or
    $TargetOutput.Trim().Length -gt 256) {
    throw "TargetOutput must contain 1 to 256 non-whitespace characters."
}
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null

$startedAt = Get-Date
$failures = New-Object System.Collections.Generic.List[object]
$buildRoot = Join-Path $EvidenceRoot "p1-release-build"
$simulationRoot = Join-Path $EvidenceRoot "simulation-evidence"
$buildExitCode = $null
$driverExitCode = $null
$resolvedExecutable = $null
$configPath = Join-Path $sourceDirectory "config.ini"

function Add-Failure {
    param([string]$Step, [int]$ExitCode = 1, [string]$Detail = "", [string]$Log = "")
    $failures.Add([pscustomobject]@{
        Step = $Step
        ExitCode = $ExitCode
        Detail = $Detail
        Log = $Log
    })
}

function Get-EnvironmentValue {
    param([string]$Name)
    foreach ($scope in @("Process", "User", "Machine")) {
        $value = [Environment]::GetEnvironmentVariable($Name, $scope)
        if (-not [string]::IsNullOrWhiteSpace($value)) { return $value }
    }
    return $null
}

function Get-FileRecord {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject]@{ Path = $Path; Exists = $false; Length = $null; Sha256 = $null }
    }
    $item = Get-Item -LiteralPath $Path
    return [pscustomobject]@{
        Path = $item.FullName
        Exists = $true
        Length = $item.Length
        Sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    }
}

function Add-RuntimePath {
    param([System.Collections.Generic.List[string]]$Paths, [string]$Path)
    if (-not [string]::IsNullOrWhiteSpace($Path) -and
        (Test-Path -LiteralPath $Path -PathType Container) -and
        -not $Paths.Contains($Path)) {
        $Paths.Add($Path)
    }
}

try {
    if (-not $SkipBuild) {
        $buildLog = Join-Path $EvidenceRoot "p1-build-wrapper.log"
        $previousPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            powershell -ExecutionPolicy Bypass -File $buildScript -Configuration Release `
                -EvidenceRoot $buildRoot 2>&1 | Tee-Object -FilePath $buildLog
            $buildExitCode = $LASTEXITCODE
        }
        finally { $ErrorActionPreference = $previousPreference }
        if ($buildExitCode -ne 0) {
            Add-Failure "p1-release-build" $buildExitCode "Release build failed" $buildLog
        }
    }
    else {
        $buildExitCode = 0
    }

    if (-not [string]::IsNullOrWhiteSpace($Executable)) {
        $resolvedExecutable = [System.IO.Path]::GetFullPath($Executable)
    }
    elseif ($buildExitCode -eq 0) {
        $buildManifestPath = Join-Path $buildRoot "manifest.json"
        if (-not (Test-Path -LiteralPath $buildManifestPath -PathType Leaf)) {
            Add-Failure "p1-build-manifest" 1 "Release build manifest is missing" $buildRoot
        }
        else {
            $buildManifest = Get-Content -Raw -LiteralPath $buildManifestPath -Encoding UTF8 |
                ConvertFrom-Json
            $release = @($buildManifest.Builds | Where-Object Configuration -eq "Release" |
                Select-Object -First 1)
            if ($release.Count -ne 1) {
                Add-Failure "p1-release-output" 1 "Release build output is not recorded" $buildManifestPath
            }
            else {
                $resolvedExecutable = Join-Path $release[0].OutputDirectory "CigVision.exe"
            }
        }
    }

    if ($failures.Count -eq 0) {
        if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
            Add-Failure "simulation-executable" 1 "CigVision.exe is missing" $resolvedExecutable
        }
        if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
            Add-Failure "simulation-config" 1 "config.ini is missing" $configPath
        }
    }

    if ($failures.Count -eq 0) {
        $runtimePaths = New-Object System.Collections.Generic.List[string]
        Add-RuntimePath $runtimePaths (Split-Path -Parent $resolvedExecutable)
        $qtRoot = Get-EnvironmentValue "CIGVISION_QT_ROOT"
        $halconRoot = Get-EnvironmentValue "CIGVISION_HALCON_RELEASE_ROOT"
        $mvsRoot = Get-EnvironmentValue "MVCAM_COMMON_RUNENV"
        $daqRoot = Get-EnvironmentValue "CIGVISION_DAQNAVI_ROOT"
        $tensorRtRoot = Get-EnvironmentValue "TENSORRT_PATH"
        $cudaRoot = Get-EnvironmentValue "CUDA_PATH"
        $openCvRoot = Get-EnvironmentValue "OPENCV_PATH"
        if (-not [string]::IsNullOrWhiteSpace($qtRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $qtRoot "bin")
        }
        if (-not [string]::IsNullOrWhiteSpace($halconRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $halconRoot "bin\x64-win64")
        }
        Add-RuntimePath $runtimePaths "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
        if (-not [string]::IsNullOrWhiteSpace($mvsRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $mvsRoot "..\..\Common Files\MVS\Runtime\Win64_x64")
        }
        if (-not [string]::IsNullOrWhiteSpace($daqRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $daqRoot "bin")
        }
        if (-not [string]::IsNullOrWhiteSpace($tensorRtRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $tensorRtRoot "bin")
        }
        if (-not [string]::IsNullOrWhiteSpace($cudaRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $cudaRoot "bin")
        }
        if (-not [string]::IsNullOrWhiteSpace($openCvRoot)) {
            Add-RuntimePath $runtimePaths (Join-Path $openCvRoot "build\x64\vc16\bin")
        }
        $env:PATH = (($runtimePaths -join ";") + ";" + $env:PATH)

        $driverLog = Join-Path $EvidenceRoot "simulation-driver.log"
        $driverArguments = @(
            $driver,
            "--executable", $resolvedExecutable,
            "--manifest", $Manifest,
            "--config-ini", $configPath,
            "--evidence-root", $simulationRoot,
            "--reject-delay-micros", [string]$RejectDelayMicros,
            "--queue-capacity", [string]$QueueCapacity,
            "--target-output", $TargetOutput
        )
        ($python.Source + " " + ($driverArguments -join " ")) |
            Set-Content -LiteralPath ($driverLog + ".command.txt") -Encoding UTF8
        $previousPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $python.Source @driverArguments 2>&1 | Tee-Object -FilePath $driverLog
            $driverExitCode = $LASTEXITCODE
        }
        finally { $ErrorActionPreference = $previousPreference }
        if ($driverExitCode -ne 0) {
            Add-Failure "p6-simulation-evidence" $driverExitCode "Evidence driver failed" $driverLog
        }
    }
}
catch {
    Add-Failure "unhandled-exception" 1 $_.Exception.ToString()
}
finally {
    $simulationManifestPath = Join-Path $simulationRoot "manifest.json"
    $simulationManifest = if (Test-Path -LiteralPath $simulationManifestPath -PathType Leaf) {
        Get-Content -Raw -LiteralPath $simulationManifestPath -Encoding UTF8 |
            ConvertFrom-Json
    } else { $null }
    $evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceRoot -Recurse -File |
        Where-Object { $_.FullName -ne (Join-Path $EvidenceRoot "manifest.json") } |
        Sort-Object FullName | ForEach-Object { Get-FileRecord $_.FullName })
    $overallPassed = ($failures.Count -eq 0 -and $driverExitCode -eq 0)
    [ordered]@{
        SchemaVersion = "p6-windows-simulation-wrapper-v1"
        OverallResult = if ($overallPassed) { "passed" } else { "failed" }
        OverallCommandExitCode = if ($overallPassed) { 0 } else { 1 }
        StartedAt = $startedAt.ToString("o")
        FinishedAt = (Get-Date).ToString("o")
        Windows = [Environment]::OSVersion.VersionString
        Python = $python.Source
        GitHead = (& git -C $repoRoot rev-parse HEAD)
        GitStatus = @(& git -C $repoRoot status --short --untracked-files=all)
        Manifest = Get-FileRecord $Manifest
        Config = Get-FileRecord $configPath
        Executable = if ([string]::IsNullOrWhiteSpace($resolvedExecutable)) {
            [pscustomobject]@{ Path = $null; Exists = $false; Length = $null; Sha256 = $null }
        } else { Get-FileRecord $resolvedExecutable }
        BuildPerformed = (-not $SkipBuild)
        BuildExitCode = $buildExitCode
        DriverExitCode = $driverExitCode
        SimulationEvidenceManifest = if ($simulationManifest) {
            [pscustomobject]@{
                Path = $simulationManifestPath
                OverallResult = $simulationManifest.overallResult
                TargetWindowsRuntimeVerified = $simulationManifest.targetWindowsRuntimeVerified
                RealIoEnabled = $simulationManifest.realIoEnabled
                Failures = $simulationManifest.failures
            }
        } else { $null }
        Failures = $failures
        EvidenceFiles = $evidenceFiles
    } | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath `
        (Join-Path $EvidenceRoot "manifest.json") -Encoding UTF8
}

if ($failures.Count -ne 0 -or $driverExitCode -ne 0) {
    Write-Error "P6 Windows simulation evidence failed. Evidence: $EvidenceRoot"
    exit 1
}
Write-Host "PASS P6 Windows simulation evidence. Evidence: $EvidenceRoot"
exit 0
