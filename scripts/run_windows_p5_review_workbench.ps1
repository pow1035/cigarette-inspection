param(
    [string]$Package = "",
    [string]$Workspace = "",
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
    throw "Pass the reviewed authoritative package explicitly with -Package; latest-artifact discovery is intentionally disabled."
}

$Package = [System.IO.Path]::GetFullPath($Package)
if (-not (Test-Path -LiteralPath $Package -PathType Container)) {
    throw "P5 review package does not exist: $Package"
}

if ([string]::IsNullOrWhiteSpace($Workspace)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $Workspace = Join-Path $repoRoot "artifacts\p5-review-workbench-$timestamp"
}
$Workspace = [System.IO.Path]::GetFullPath($Workspace)
New-Item -ItemType Directory -Path $Workspace -Force | Out-Null

$python = Get-Command python -ErrorAction SilentlyContinue
if ($null -eq $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if ($null -eq $python) {
    throw "Python was not found on PATH."
}

$url = "http://127.0.0.1:$Port/"
Write-Host "P5 review package: $Package"
Write-Host "P5 review workspace: $Workspace"
Write-Host "P5 review URL: $url"

$arguments = @($server, "--package", $Package, "--workspace", $Workspace,
    "--host", "127.0.0.1", "--port", $Port)
if ([System.IO.Path]::GetFileNameWithoutExtension($python.Source) -eq "py") {
    $arguments = @("-3") + $arguments
}
& $python.Source @arguments
exit $LASTEXITCODE
