param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
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
$results = New-Object System.Collections.Generic.List[object]

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

function Add-Check {
    param(
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )
    $results.Add([pscustomobject]@{
        Name = $Name
        Passed = $Passed
        Detail = $Detail
    })
}

$msbuild = Get-Command msbuild.exe -ErrorAction SilentlyContinue
Add-Check "MSBuild in PATH" ($null -ne $msbuild) $(if ($msbuild) { $msbuild.Source } else { "Run from a VS 2022 Developer PowerShell" })

$qtRoot = Get-EnvironmentValue "CIGVISION_QT_ROOT"
$qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
if ($null -eq $qmake -and -not [string]::IsNullOrWhiteSpace($qtRoot)) {
    $qmakePath = Join-Path $qtRoot "bin\qmake.exe"
    if (Test-Path $qmakePath) {
        $qmake = Get-Command $qmakePath
    }
}
Add-Check "qmake available" ($null -ne $qmake) $(if ($qmake) { $qmake.Source } else { "Qt 5.9.9 msvc2017_64 is not visible in PATH or CIGVISION_QT_ROOT" })

$halconDebugRoot = Get-EnvironmentValue "CIGVISION_HALCON_DEBUG_ROOT"
if ([string]::IsNullOrWhiteSpace($halconDebugRoot)) {
    $halconDebugRoot = "C:\Program Files\MVTec\HALCON-25.05-Progress"
}
$halconReleaseRoot = Get-EnvironmentValue "CIGVISION_HALCON_RELEASE_ROOT"
if ([string]::IsNullOrWhiteSpace($halconReleaseRoot)) {
    $halconReleaseRoot = "C:\Program Files\MVTec\HALCON-22.11-Steady"
}
$halconRoot = if ($Configuration -eq "Debug") { $halconDebugRoot } else { $halconReleaseRoot }
$halconHeader = Join-Path $halconRoot "include\halconcpp\HalconCpp.h"
$halconLibrary = Join-Path $halconRoot "lib\x64-win64\halcon.lib"
$halconCppLibrary = Join-Path $halconRoot "lib\x64-win64\halconcpp.lib"
$halconRuntime = Join-Path $halconRoot "bin\x64-win64\halcon.dll"
$halconCppRuntime = Join-Path $halconRoot "bin\x64-win64\halconcpp.dll"
Add-Check "HALCON root for $Configuration" (Test-Path $halconRoot) $halconRoot
Add-Check "HALCON C++ header" (Test-Path $halconHeader) $halconHeader
Add-Check "HALCON x64 library" ((Test-Path $halconLibrary) -and (Test-Path $halconCppLibrary)) "$halconLibrary; $halconCppLibrary"
Add-Check "HALCON x64 runtime" ((Test-Path $halconRuntime) -and (Test-Path $halconCppRuntime)) "$halconRuntime; $halconCppRuntime"

$mvsDevelopmentRoot = Get-EnvironmentValue "MVCAM_COMMON_RUNENV"
if ([string]::IsNullOrWhiteSpace($mvsDevelopmentRoot)) {
    $mvsDevelopmentRoot = "C:\Program Files (x86)\MVS\Development"
}
$mvsInclude = Join-Path $mvsDevelopmentRoot "Includes\MvCameraControl.h"
$mvsLibrary = Join-Path $mvsDevelopmentRoot "Libraries\win64\MvCameraControl.lib"
$mvsRuntime = "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64\MvCameraControl.dll"
Add-Check "MVS header" (Test-Path $mvsInclude) $mvsInclude
Add-Check "MVS x64 library" (Test-Path $mvsLibrary) $mvsLibrary
Add-Check "MVS x64 runtime" (Test-Path $mvsRuntime) $mvsRuntime

$daqNaviRoot = Get-EnvironmentValue "CIGVISION_DAQNAVI_ROOT"
if ([string]::IsNullOrWhiteSpace($daqNaviRoot)) {
    $daqNaviRoot = "C:\Advantech\DAQNavi"
}
$daqHeader = Join-Path $daqNaviRoot "Inc\bdaqctrl.h"
$daqRuntime = "C:\Windows\System32\biodaq.dll"
Add-Check "DAQNavi header" (Test-Path $daqHeader) $daqHeader
Add-Check "DAQNavi x64 runtime" (Test-Path $daqRuntime) $daqRuntime

$configPath = Join-Path $sourceDir "config.ini"
$rejectDisabled = Select-String -Path $configPath -Pattern '^rejectEnabled=false$' -Quiet
Add-Check "Real reject disabled" $rejectDisabled $configPath

Add-Check "Solution exists" (Test-Path $solutionPath) $solutionPath

$results | Format-Table -AutoSize

if ($results.Where({ -not $_.Passed }).Count -gt 0) {
    Write-Error "Windows build environment check failed for $Configuration."
    exit 1
}

Write-Host "PASS Windows build environment check for $Configuration."
