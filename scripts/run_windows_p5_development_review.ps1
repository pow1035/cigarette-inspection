param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("train", "validation")]
    [string]$Split,
    [ValidateSet("annotator", "reviewer")]
    [string]$Mode = "annotator",
    [string]$Package = "",
    [string]$Workspace = "",
    [string]$Pass1 = "",
    [ValidateRange(1, 65535)]
    [int]$Port = 8765
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$server = Join-Path $repoRoot "tools\p5_review_workbench\server.py"

if (-not (Test-Path -LiteralPath $server -PathType Leaf)) {
    throw "P5 review workbench server is missing: $server"
}
if ([string]::IsNullOrWhiteSpace($Package)) {
    throw "Pass the frozen P5 development package explicitly with -Package."
}

$Package = [System.IO.Path]::GetFullPath($Package)
if (-not (Test-Path -LiteralPath $Package -PathType Container)) {
    throw "P5 development package does not exist: $Package"
}
if ($Mode -eq "reviewer") {
    if ([string]::IsNullOrWhiteSpace($Pass1)) {
        throw "Reviewer mode requires the immutable pass1 export with -Pass1."
    }
    $Pass1 = [System.IO.Path]::GetFullPath($Pass1)
    if (-not (Test-Path -LiteralPath $Pass1 -PathType Leaf)) {
        throw "P5 pass1 export does not exist: $Pass1"
    }
} elseif (-not [string]::IsNullOrWhiteSpace($Pass1)) {
    throw "-Pass1 is accepted only when -Mode reviewer is selected."
}

function Test-DirectoryOverlap {
    param(
        [Parameter(Mandatory = $true)][string]$Left,
        [Parameter(Mandatory = $true)][string]$Right
    )
    $separator = [System.IO.Path]::DirectorySeparatorChar
    $leftPath = [System.IO.Path]::GetFullPath($Left).TrimEnd("\", "/")
    $rightPath = [System.IO.Path]::GetFullPath($Right).TrimEnd("\", "/")
    if ([string]::Equals(
            $leftPath, $rightPath,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    $leftPrefix = $leftPath + $separator
    $rightPrefix = $rightPath + $separator
    return $leftPrefix.StartsWith(
        $rightPrefix, [System.StringComparison]::OrdinalIgnoreCase
    ) -or $rightPrefix.StartsWith(
        $leftPrefix, [System.StringComparison]::OrdinalIgnoreCase
    )
}

if ([string]::IsNullOrWhiteSpace($Workspace)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $Workspace = Join-Path $repoRoot "artifacts\p5-development-$Mode-$Split-$timestamp"
}
$Workspace = [System.IO.Path]::GetFullPath($Workspace)
$staticDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot "tools\p5_review_workbench\static")
)
if ((Test-DirectoryOverlap -Left $Workspace -Right $Package) -or
        (Test-DirectoryOverlap -Left $Workspace -Right $staticDirectory)) {
    throw "P5 review workspace must not overlap the package or static assets."
}

$python = Get-Command python -ErrorAction SilentlyContinue
if ($null -eq $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if ($null -eq $python) {
    throw "Python was not found on PATH."
}

$url = "http://127.0.0.1:$Port/"
Write-Host "P5 development package: $Package"
Write-Host "P5 development split: $Split"
Write-Host "P5 workbench mode: $Mode"
if ($Mode -eq "reviewer") {
    Write-Host "P5 immutable pass1: $Pass1"
}
Write-Host "P5 review workspace: $Workspace"
Write-Host "P5 review URL: $url"
Write-Host "All workbench exports remain is_ground_truth=false until a separate approval gate."

$arguments = @(
    $server, "--package", $Package, "--split", $Split,
    "--mode", $Mode, "--workspace", $Workspace,
    "--host", "127.0.0.1", "--port", $Port
)
if ($Mode -eq "reviewer") {
    $arguments += @("--pass1", $Pass1)
}
if ([System.IO.Path]::GetFileNameWithoutExtension($python.Source) -eq "py") {
    $arguments = @("-3") + $arguments
}
& $python.Source @arguments
exit $LASTEXITCODE
