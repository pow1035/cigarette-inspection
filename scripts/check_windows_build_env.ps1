param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot "01_上位机_QT_新版_CigVision\源码"
$results = New-Object System.Collections.Generic.List[object]

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

$qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
Add-Check "qmake in PATH" ($null -ne $qmake) $(if ($qmake) { $qmake.Source } else { "Qt 5.9.9 msvc2017_64 is not visible in PATH" })

$halconRoot = if ($Configuration -eq "Debug") {
    "C:\Program Files\MVTec\HALCON-25.05-Progress"
} else {
    "C:\Program Files\MVTec\HALCON-22.11-Steady"
}
Add-Check "HALCON for $Configuration" (Test-Path $halconRoot) $halconRoot

$mvsInclude = "C:\Program Files (x86)\MVS\Development\Includes\MvCameraControl.h"
$mvsLibrary = "C:\Program Files (x86)\MVS\Development\Libraries\win64\MvCameraControl.lib"
Add-Check "MVS header" (Test-Path $mvsInclude) $mvsInclude
Add-Check "MVS x64 library" (Test-Path $mvsLibrary) $mvsLibrary

$daqHeader = "C:\Advantech\DAQNavi\Inc\bdaqctrl.h"
Add-Check "DAQNavi header" (Test-Path $daqHeader) $daqHeader

$configPath = Join-Path $sourceDir "config.ini"
$rejectDisabled = Select-String -Path $configPath -Pattern '^rejectEnabled=false$' -Quiet
Add-Check "Real reject disabled" $rejectDisabled $configPath

$solutionPath = Join-Path $sourceDir "CigVision.sln"
Add-Check "Solution exists" (Test-Path $solutionPath) $solutionPath

$results | Format-Table -AutoSize

if ($results.Where({ -not $_.Passed }).Count -gt 0) {
    Write-Error "Windows build environment check failed for $Configuration."
    exit 1
}

Write-Host "PASS Windows build environment check for $Configuration."
