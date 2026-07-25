[CmdletBinding()]
param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
$testDirectory = Split-Path -Parent $PSCommandPath
$defaultRoot = Split-Path -Parent (Split-Path -Parent $testDirectory)
$repositoryRoot = if ([string]::IsNullOrWhiteSpace($Root)) {
    $defaultRoot
} else {
    (Resolve-Path -LiteralPath $Root).Path
}
$validator = Join-Path $repositoryRoot "scripts/validate_powershell_scripts.ps1"
$powerShellExecutable = (Get-Process -Id $PID).Path

function Invoke-ChildPowerShell {
    param(
        [string[]]$ChildArguments,
        [hashtable]$EnvironmentOverrides = @{}
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $powerShellExecutable
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @("-NoLogo", "-NoProfile", "-File", $validator)) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    foreach ($argument in $ChildArguments) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
    foreach ($name in $EnvironmentOverrides.Keys) {
        $startInfo.Environment[$name] = [string]$EnvironmentOverrides[$name]
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    [pscustomobject]@{
        ExitCode = $process.ExitCode
        Stdout = $stdout.GetAwaiter().GetResult().Trim()
        Stderr = $stderr.GetAwaiter().GetResult().Trim()
    }
}

function Assert-Equal {
    param(
        $Actual,
        $Expected,
        [string]$Context
    )

    if ($Actual -ne $Expected) {
        throw "$Context expected '$Expected' but received '$Actual'"
    }
}

$temporaryRoot = Join-Path (
    [System.IO.Path]::GetTempPath()
) "cigvision-powershell-gate-$([guid]::NewGuid().ToString('N'))"

try {
    $valid = Invoke-ChildPowerShell @(
        "-Root", $repositoryRoot, "-RequireScriptAnalyzer"
    )
    Assert-Equal $valid.ExitCode 0 "valid repository exit"
    $validReport = $valid.Stdout | ConvertFrom-Json
    Assert-Equal $validReport.status "passed" "valid repository status"
    Assert-Equal $validReport.windowsRuntimeClaimed $false (
        "static gate runtime claim"
    )
    Assert-Equal $validReport.parserFindingCount 0 (
        "valid repository parser findings"
    )
    Assert-Equal $validReport.analyzerFindingCount 0 (
        "valid repository analyzer findings"
    )
    Assert-Equal $validReport.scriptAnalyzerMinimumVersion "1.25.0" (
        "minimum analyzer version"
    )
    Assert-Equal @($validReport.syntaxCompatibilityTargets).Count 1 (
        "syntax compatibility target count"
    )
    Assert-Equal @($validReport.syntaxCompatibilityTargets)[0] "5.1" (
        "syntax compatibility target"
    )

    $invalidRoot = Join-Path $temporaryRoot "invalid"
    $invalidScripts = Join-Path $invalidRoot "scripts"
    [void](New-Item -ItemType Directory -Force -Path $invalidScripts)
    Set-Content -LiteralPath (Join-Path $invalidScripts "bad.ps1") -Value @(
        "function Get-BrokenFixture {"
    )
    $invalid = Invoke-ChildPowerShell @(
        "-Root", $invalidRoot, "-RequireScriptAnalyzer"
    )
    Assert-Equal $invalid.ExitCode 1 "invalid script exit"
    $invalidReport = $invalid.Stdout | ConvertFrom-Json
    Assert-Equal $invalidReport.status "failed" "invalid script status"
    if ($invalidReport.parserFindingCount -lt 1) {
        throw "invalid script must report at least one parser finding"
    }

    $incompatibleRoot = Join-Path $temporaryRoot "incompatible"
    $incompatibleScripts = Join-Path $incompatibleRoot "scripts"
    [void](New-Item -ItemType Directory -Force -Path $incompatibleScripts)
    Set-Content -LiteralPath (
        Join-Path $incompatibleScripts "modern-syntax.ps1"
    ) -Value @(
        '$condition = $true',
        '$value = $condition ? "yes" : "no"'
    )
    $incompatible = Invoke-ChildPowerShell @(
        "-Root", $incompatibleRoot, "-RequireScriptAnalyzer"
    )
    Assert-Equal $incompatible.ExitCode 1 "incompatible syntax exit"
    $incompatibleReport = $incompatible.Stdout | ConvertFrom-Json
    Assert-Equal $incompatibleReport.parserFindingCount 0 (
        "incompatible syntax parser findings"
    )
    if (
        "PSUseCompatibleSyntax" -notin
        @($incompatibleReport.analyzerFindings.rule)
    ) {
        throw "PowerShell 7 syntax must trigger PSUseCompatibleSyntax"
    }

    $missingRoot = Join-Path $temporaryRoot "missing"
    $missing = Invoke-ChildPowerShell @("-Root", $missingRoot)
    Assert-Equal $missing.ExitCode 3 "missing root exit"
    $missingReport = $missing.Stdout | ConvertFrom-Json
    Assert-Equal $missingReport.status "invalid-input" "missing root status"
    Assert-Equal $missingReport.errorCode "repository-root-unresolved" (
        "missing root error code"
    )

    $noScriptsRoot = Join-Path $temporaryRoot "no-scripts"
    [void](New-Item -ItemType Directory -Force -Path $noScriptsRoot)
    $noScripts = Invoke-ChildPowerShell @("-Root", $noScriptsRoot)
    Assert-Equal $noScripts.ExitCode 3 "missing scripts directory exit"
    $noScriptsReport = $noScripts.Stdout | ConvertFrom-Json
    Assert-Equal $noScriptsReport.errorCode "scripts-directory-missing" (
        "missing scripts directory error code"
    )

    $emptyScriptsRoot = Join-Path $temporaryRoot "empty-scripts"
    [void](New-Item -ItemType Directory -Force -Path (
        Join-Path $emptyScriptsRoot "scripts"
    ))
    $emptyScripts = Invoke-ChildPowerShell @("-Root", $emptyScriptsRoot)
    Assert-Equal $emptyScripts.ExitCode 3 "empty scripts directory exit"
    $emptyScriptsReport = $emptyScripts.Stdout | ConvertFrom-Json
    Assert-Equal $emptyScriptsReport.errorCode "powershell-scripts-missing" (
        "empty scripts directory error code"
    )

    $emptyModules = Join-Path $temporaryRoot "empty-modules"
    [void](New-Item -ItemType Directory -Force -Path $emptyModules)
    $missingAnalyzer = Invoke-ChildPowerShell -ChildArguments @(
        "-Root", $repositoryRoot,
        "-RequireScriptAnalyzer",
        "-ScriptAnalyzerModulePath", $emptyModules
    )
    Assert-Equal $missingAnalyzer.ExitCode 2 "missing analyzer exit"
    $missingAnalyzerReport = $missingAnalyzer.Stdout | ConvertFrom-Json
    Assert-Equal $missingAnalyzerReport.status "blocked" (
        "missing analyzer status"
    )
    Assert-Equal (
        $missingAnalyzerReport.errorCode
    ) "script-analyzer-unavailable" "missing analyzer error code"

    [ordered]@{
        schemaVersion = "cigvision-powershell-static-gate-tests-v1"
        status = "passed"
        caseCount = 7
        powershellVersion = $PSVersionTable.PSVersion.ToString()
        windowsRuntimeClaimed = $false
    } | ConvertTo-Json -Compress
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
