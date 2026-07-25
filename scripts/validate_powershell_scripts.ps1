[CmdletBinding()]
param(
    [string]$Root = "",
    [switch]$RequireScriptAnalyzer,
    [string]$ScriptAnalyzerModulePath = ""
)

$ErrorActionPreference = "Stop"
$minimumAnalyzerVersion = [version]"1.25.0"

function Write-PowerShellGateFailure {
    param(
        [int]$ExitCode,
        [string]$Status,
        [string]$ErrorCode,
        [string]$Message
    )

    [ordered]@{
        schemaVersion = "cigvision-powershell-static-gate-v1"
        scope = "parser-and-static-analysis-only"
        status = $Status
        errorCode = $ErrorCode
        message = $Message
        powershellVersion = $PSVersionTable.PSVersion.ToString()
        windowsHost = (
            [Environment]::OSVersion.Platform -eq
            [System.PlatformID]::Win32NT
        )
        windowsRuntimeClaimed = $false
        scriptAnalyzerRequired = [bool]$RequireScriptAnalyzer
        scriptAnalyzerMinimumVersion = $minimumAnalyzerVersion.ToString()
    } | ConvertTo-Json -Depth 4 -Compress
    [Console]::Error.WriteLine($Message)
    exit $ExitCode
}

$scriptDirectory = Split-Path -Parent $PSCommandPath
$defaultRoot = Split-Path -Parent $scriptDirectory
$candidateRoot = if ([string]::IsNullOrWhiteSpace($Root)) {
    $defaultRoot
} else {
    $Root
}

try {
    $repositoryRoot = (Resolve-Path -LiteralPath $candidateRoot).Path
} catch {
    Write-PowerShellGateFailure 3 "invalid-input" "repository-root-unresolved" (
        "Repository root cannot be resolved: $candidateRoot"
    )
}
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    Write-PowerShellGateFailure 3 "invalid-input" "repository-root-not-directory" (
        "Repository root must be a directory: $repositoryRoot"
    )
}

$scriptsRoot = Join-Path $repositoryRoot "scripts"
if (-not (Test-Path -LiteralPath $scriptsRoot -PathType Container)) {
    Write-PowerShellGateFailure 3 "invalid-input" "scripts-directory-missing" (
        "No PowerShell scripts directory was found under $repositoryRoot"
    )
}
$scanRoots = @($scriptsRoot)
$powerShellTestsRoot = Join-Path $repositoryRoot "tests/powershell"
if (Test-Path -LiteralPath $powerShellTestsRoot -PathType Container) {
    $scanRoots += $powerShellTestsRoot
}
$files = @(
    $scanRoots | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Filter "*.ps1" -File -Recurse
    } |
        Sort-Object FullName
)
if ($files.Count -eq 0) {
    Write-PowerShellGateFailure 3 "invalid-input" "powershell-scripts-missing" (
        "No PowerShell scripts were discovered under $scriptsRoot"
    )
}

$parserFindings = @()
foreach ($file in $files) {
    $tokens = $null
    $parseErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $file.FullName,
        [ref]$tokens,
        [ref]$parseErrors
    )
    foreach ($parseError in @($parseErrors)) {
        $parserFindings += [ordered]@{
            file = $file.FullName.Substring($repositoryRoot.Length).
                TrimStart("\", "/").Replace("\", "/")
            line = $parseError.Extent.StartLineNumber
            column = $parseError.Extent.StartColumnNumber
            errorId = $parseError.ErrorId
            message = $parseError.Message
        }
    }
}

$analyzerCandidates = if (
    [string]::IsNullOrWhiteSpace($ScriptAnalyzerModulePath)
) {
    @(Get-Module -ListAvailable -Name PSScriptAnalyzer)
} else {
    $explicitAnalyzerPath = [System.IO.Path]::GetFullPath(
        $ScriptAnalyzerModulePath)
    $analyzerManifests = if (
        Test-Path -LiteralPath $explicitAnalyzerPath -PathType Leaf
    ) {
        @(Get-Item -Force -LiteralPath $explicitAnalyzerPath)
    } elseif (
        Test-Path -LiteralPath $explicitAnalyzerPath -PathType Container
    ) {
        @(Get-ChildItem -LiteralPath $explicitAnalyzerPath `
            -Filter "PSScriptAnalyzer.psd1" -File -Recurse)
    } else {
        @()
    }
    @($analyzerManifests | ForEach-Object {
            Get-Module -ListAvailable -Name $_.FullName
        })
}
$analyzerModule = @(
    $analyzerCandidates | Sort-Object Version -Descending
) | Select-Object -First 1
if (
    $null -ne $analyzerModule -and
    $analyzerModule.Version -lt $minimumAnalyzerVersion
) {
    $analyzerModule = $null
}
if ($RequireScriptAnalyzer -and $null -eq $analyzerModule) {
    Write-PowerShellGateFailure 2 "blocked" "script-analyzer-unavailable" (
        "PSScriptAnalyzer $minimumAnalyzerVersion or newer is required."
    )
}

$analyzerFindings = @()
$excludedAnalyzerRules = @(
    # Operator-facing evidence wrappers intentionally emit progress to the host.
    "PSAvoidUsingWriteHost",
    # Local helper invocations are short and unambiguous in these scripts.
    "PSAvoidUsingPositionalParameters",
    # Internal helpers are not exported as a public PowerShell module API.
    "PSUseSingularNouns"
)
$syntaxCompatibilityTargets = @("5.1")
$analyzerSettings = @{
    IncludeDefaultRules = $true
    ExcludeRules = $excludedAnalyzerRules
    Rules = @{
        PSUseCompatibleSyntax = @{
            Enable = $true
            TargetVersions = $syntaxCompatibilityTargets
        }
    }
}
if ($null -ne $analyzerModule) {
    Import-Module $analyzerModule.Path -Force
    $analyzerFindings = @(
        $files | ForEach-Object {
            Invoke-ScriptAnalyzer `
                -Path $_.FullName `
                -Settings $analyzerSettings
        } | ForEach-Object {
                [ordered]@{
                    file = $_.ScriptPath.Substring($repositoryRoot.Length).
                        TrimStart("\", "/").Replace("\", "/")
                    line = $_.Line
                    column = $_.Column
                    rule = $_.RuleName
                    message = $_.Message
                }
            }
    )
}

$isWindowsHost = (
    [Environment]::OSVersion.Platform -eq
    [System.PlatformID]::Win32NT
)
$passed = (
    $parserFindings.Count -eq 0 -and
    $analyzerFindings.Count -eq 0
)
$result = [ordered]@{
    schemaVersion = "cigvision-powershell-static-gate-v1"
    scope = "parser-and-static-analysis-only"
    status = if (-not $passed) {
        "failed"
    } elseif ($null -eq $analyzerModule) {
        "passed-parser-only"
    } else {
        "passed"
    }
    powershellVersion = $PSVersionTable.PSVersion.ToString()
    windowsHost = $isWindowsHost
    windowsRuntimeClaimed = $false
    scriptCount = $files.Count
    parserFindingCount = $parserFindings.Count
    parserFindings = $parserFindings
    scriptAnalyzerRequired = [bool]$RequireScriptAnalyzer
    scriptAnalyzerAvailable = ($null -ne $analyzerModule)
    scriptAnalyzerMinimumVersion = $minimumAnalyzerVersion.ToString()
    scriptAnalyzerVersion = if ($null -eq $analyzerModule) {
        $null
    } else {
        $analyzerModule.Version.ToString()
    }
    analyzerExcludedRules = $excludedAnalyzerRules
    syntaxCompatibilityTargets = $syntaxCompatibilityTargets
    analyzerFindingCount = $analyzerFindings.Count
    analyzerFindings = $analyzerFindings
}
$result | ConvertTo-Json -Depth 8 -Compress
if (-not $passed) {
    exit 1
}
exit 0
