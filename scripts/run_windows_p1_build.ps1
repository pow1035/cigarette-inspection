param(
    [ValidateSet("Debug", "Release", "All")]
    [string]$Configuration = "All",
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Get-ChildItem -LiteralPath $repoRoot -Recurse -Filter "CigVision.sln" -File |
    Where-Object { $_.FullName -like "*CigVision*" } |
    Select-Object -First 1 -ExpandProperty FullName
if ([string]::IsNullOrWhiteSpace($solutionPath)) {
    throw "CigVision.sln was not found under $repoRoot."
}
$sourceDir = Split-Path -Parent $solutionPath

function Get-EnvironmentValue {
    param([string]$Name)

    foreach ($scope in @("Process", "User", "Machine")) {
        $value = [Environment]::GetEnvironmentVariable($Name, $scope)
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value
        }
    }
    return $null
}

$qtRoot = Get-EnvironmentValue "CIGVISION_QT_ROOT"
$qtMsBuildPath = Get-EnvironmentValue "CIGVISION_QT_MSBUILD"
$mvsDevelopmentRoot = Get-EnvironmentValue "MVCAM_COMMON_RUNENV"
$daqNaviRoot = Get-EnvironmentValue "CIGVISION_DAQNAVI_ROOT"
$halconDebugRoot = Get-EnvironmentValue "CIGVISION_HALCON_DEBUG_ROOT"
$halconReleaseRoot = Get-EnvironmentValue "CIGVISION_HALCON_RELEASE_ROOT"
if ([string]::IsNullOrWhiteSpace($mvsDevelopmentRoot)) {
    $mvsDevelopmentRoot = "C:\Program Files (x86)\MVS\Development"
}
if ([string]::IsNullOrWhiteSpace($daqNaviRoot)) {
    $daqNaviRoot = "C:\Advantech\DAQNavi"
}
if ([string]::IsNullOrWhiteSpace($halconDebugRoot)) {
    $halconDebugRoot = "C:\Program Files\MVTec\HALCON-25.05-Progress"
}
if ([string]::IsNullOrWhiteSpace($halconReleaseRoot)) {
    $halconReleaseRoot = "C:\Program Files\MVTec\HALCON-22.11-Steady"
}
if (-not [string]::IsNullOrWhiteSpace($qtRoot)) {
    $qtBin = Join-Path $qtRoot "bin"
    if (Test-Path (Join-Path $qtBin "qmake.exe")) {
        $env:PATH = "$qtBin;$env:PATH"
    }
}

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $EvidenceRoot = Join-Path $repoRoot "artifacts\p1-windows-$timestamp"
}
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null

$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue

function Get-InstalledDependencyProducts {
    $registryRoots = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    foreach ($registryRoot in $registryRoots) {
        Get-ItemProperty -Path $registryRoot -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -match "Visual Studio|Qt|HALCON|MVS|Machine Vision|DAQNavi|Advantech" } |
            Select-Object DisplayName, DisplayVersion, InstallLocation, Publisher
    }
}

function Get-FileEvidence {
    param([string]$Path)

    $exists = Test-Path $Path
    [pscustomobject]@{
        Path = $Path
        Exists = $exists
        FileVersion = if ($exists) { (Get-Item -LiteralPath $Path).VersionInfo.FileVersion } else { $null }
        Sha256 = if ($exists) { (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash } else { $null }
    }
}

function Get-EnvironmentSnapshot {
    param(
        [System.Management.Automation.CommandInfo]$MSBuildCommand
    )

    $qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
    $halconRoots = @(
        [pscustomobject]@{ Configuration = "Debug"; Path = $halconDebugRoot },
        [pscustomobject]@{ Configuration = "Release"; Path = $halconReleaseRoot }
    ) | ForEach-Object {
        $root = $_.Path
        [pscustomobject]@{
            Configuration = $_.Configuration
            Path = $root
            Exists = Test-Path $root
            Files = @(
                (Get-FileEvidence (Join-Path $root "include\halconcpp\HalconCpp.h")),
                (Get-FileEvidence (Join-Path $root "lib\x64-win64\halcon.lib")),
                (Get-FileEvidence (Join-Path $root "lib\x64-win64\halconcpp.lib")),
                (Get-FileEvidence (Join-Path $root "bin\x64-win64\halcon.dll")),
                (Get-FileEvidence (Join-Path $root "bin\x64-win64\halconcpp.dll"))
            )
        }
    }

    [pscustomobject]@{
        CapturedAt = (Get-Date).ToString("o")
        ComputerName = $env:COMPUTERNAME
        Windows = [System.Environment]::OSVersion.VersionString
        PowerShell = $PSVersionTable.PSVersion.ToString()
        VisualStudioVersion = $env:VisualStudioVersion
        VSINSTALLDIR = $env:VSINSTALLDIR
        MSBuild = if ($MSBuildCommand) { $MSBuildCommand.Source } else { "not-found" }
        QtQmake = if ($qmake) { $qmake.Source } else { "not-found" }
        QtVersion = if ($qmake) { (& $qmake.Source -query QT_VERSION) } else { "not-found" }
        QtRoot = $qtRoot
        QtMsBuild = $qtMsBuildPath
        HALCONROOT = $env:HALCONROOT
        HALCONARCH = $env:HALCONARCH
        ExpectedHalconRoots = $halconRoots
        MVSDevelopmentRoot = $mvsDevelopmentRoot
        MVSHeader = Join-Path $mvsDevelopmentRoot "Includes\MvCameraControl.h"
        MVSHeaderExists = Test-Path (Join-Path $mvsDevelopmentRoot "Includes\MvCameraControl.h")
        MVSLibrary = Join-Path $mvsDevelopmentRoot "Libraries\win64\MvCameraControl.lib"
        MVSLibraryExists = Test-Path (Join-Path $mvsDevelopmentRoot "Libraries\win64\MvCameraControl.lib")
        MVSRuntime = Get-FileEvidence "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64\MvCameraControl.dll"
        DAQNaviRoot = $daqNaviRoot
        DAQNaviHeader = Join-Path $daqNaviRoot "Inc\bdaqctrl.h"
        DAQNaviHeaderExists = Test-Path (Join-Path $daqNaviRoot "Inc\bdaqctrl.h")
        DAQNaviRuntime = Get-FileEvidence "C:\Windows\System32\biodaq.dll"
        InstalledDependencyProducts = @(Get-InstalledDependencyProducts)
        GitHead = (& git -C $repoRoot rev-parse HEAD)
        GitStatus = @(& git -C $repoRoot status --short --untracked-files=all)
        SourceSnapshot = Get-FileEvidence $sourceSnapshotPath
    }
}

$configurations = if ($Configuration -eq "All") {
    @("Debug", "Release")
} else {
    @($Configuration)
}

$manifest = New-Object System.Collections.Generic.List[object]
$failures = New-Object System.Collections.Generic.List[object]
$sourceSnapshotPath = Join-Path $EvidenceRoot "source-snapshot.diff"
& git -C $repoRoot diff --binary --no-ext-diff HEAD | Set-Content -LiteralPath $sourceSnapshotPath -Encoding UTF8
$environment = Get-EnvironmentSnapshot -MSBuildCommand $msbuild
$manifestPath = Join-Path $EvidenceRoot "manifest.json"
$startedAt = Get-Date
$commandLine = $MyInvocation.Line
if ([string]::IsNullOrWhiteSpace($commandLine)) {
    $commandLine = "powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Configuration $Configuration -EvidenceRoot `"$EvidenceRoot`""
}

try {
    foreach ($current in $configurations) {
        $envLogPath = Join-Path $EvidenceRoot "env-$($current.ToLowerInvariant()).log"
        $previousErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "check_windows_build_env.ps1") -Configuration $current 2>&1 |
                Tee-Object -FilePath $envLogPath
            $envExitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
        if ($envExitCode -ne 0) {
            $failures.Add([pscustomobject]@{
                Configuration = $current
                Step = "environment-check"
                ExitCode = $envExitCode
                Log = $envLogPath
            })
            continue
        }

        $logPath = Join-Path $EvidenceRoot "msbuild-$($current.ToLowerInvariant()).log"
        $msbuildArgs = @($solutionPath, "/m", "/t:Rebuild", "/p:Configuration=$current", "/p:Platform=x64", "/v:minimal")
        if (-not [string]::IsNullOrWhiteSpace($qtRoot)) {
            $msbuildArgs += "/p:QtInstall=$qtRoot"
        }
        if (-not [string]::IsNullOrWhiteSpace($qtMsBuildPath)) {
            $msbuildArgs += "/p:QtMsBuild=$qtMsBuildPath"
        }
        if (-not [string]::IsNullOrWhiteSpace($daqNaviRoot)) {
            $msbuildArgs += "/p:CIGVISION_DAQNAVI_ROOT=$daqNaviRoot"
        }
        $msbuildArgs += "/p:MVCAM_COMMON_RUNENV=$mvsDevelopmentRoot"
        $msbuildArgs += "/p:CIGVISION_HALCON_DEBUG_ROOT=$halconDebugRoot"
        $msbuildArgs += "/p:CIGVISION_HALCON_RELEASE_ROOT=$halconReleaseRoot"
        & $msbuild.Source @msbuildArgs 2>&1 |
            Tee-Object -FilePath $logPath
        $buildExitCode = $LASTEXITCODE
        if ($buildExitCode -ne 0) {
            $failures.Add([pscustomobject]@{
                Configuration = $current
                Step = "msbuild"
                ExitCode = $buildExitCode
                Log = $logPath
            })
            continue
        }

        $outputDir = Join-Path $sourceDir "x64\$current"
        $requiredOutputs = @(
            (Join-Path $outputDir "CigVision.exe"),
            (Join-Path $outputDir "process.dll")
        )
        $missingOutput = $false
        foreach ($output in $requiredOutputs) {
            if (-not (Test-Path $output)) {
                $missingOutput = $true
                $failures.Add([pscustomobject]@{
                    Configuration = $current
                    Step = "output-check"
                    ExitCode = 1
                    Log = $logPath
                    Detail = $output
                })
            }
        }
        if ($missingOutput) {
            continue
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
            EnvironmentCheckExitCode = $envExitCode
            BuildExitCode = $buildExitCode
            EnvironmentLog = $envLogPath
            Log = $logPath
            OutputDirectory = $outputDir
            Files = $files
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
    $finishedAt = Get-Date
    $overallExitCode = if ($failures.Count -gt 0) { 1 } else { 0 }
    $overallResult = if ($failures.Count -eq 0) {
        "passed"
    } elseif ($manifest.Count -gt 0) {
        "partial"
    } else {
        $buildFailure = $failures | Where-Object { $_.Step -ne "environment-check" } | Select-Object -First 1
        if ($null -ne $buildFailure) {
            "failed-$($buildFailure.Step)"
        } else {
            "failed-before-msbuild"
        }
    }
    $evidence = [pscustomobject]@{
        CommandLine = $commandLine
        OverallResult = $overallResult
        OverallCommandExitCode = $overallExitCode
        StartedAt = $startedAt.ToString("o")
        FinishedAt = $finishedAt.ToString("o")
        Environment = $environment
        Builds = $manifest
        Failures = $failures
    }
    $evidence | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -Path $manifestPath
}

if ($failures.Count -gt 0) {
    Write-Error "P1 Windows validation failed. Evidence: $EvidenceRoot"
    exit 1
}

Write-Host "PASS P1 Windows builds. Evidence: $EvidenceRoot"
