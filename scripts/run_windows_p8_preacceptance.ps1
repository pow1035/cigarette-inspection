param(
    [Parameter(Mandatory = $true)]
    [string]$Package,
    [Parameter(Mandatory = $true)]
    [string]$PackageManifest,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9A-Fa-f]{64}$")]
    [string]$PackageManifestSha256,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceKeyPath,
    [Parameter(Mandatory = $true)]
    [string[]]$SoakCommand,
    [string]$DeploymentRoot = "D:\CigVision",
    [string]$EvidenceRoot = "",
    [string]$ReleaseId = "",
    [ValidateRange(1, 100000)]
    [int]$Rounds = 1,
    [ValidateRange(1, 100000)]
    [int]$Restarts = 1,
    [ValidateRange(0.1, 86400.0)]
    [double]$TimeoutSeconds = 300.0,
    [switch]$ExerciseRollback
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($null -eq $python) { $python = Get-Command python -ErrorAction Stop }
$git = Get-Command git.exe -ErrorAction SilentlyContinue
if ($null -eq $git) { $git = Get-Command git -ErrorAction Stop }
$powerShellExecutable = (Get-Process -Id $PID).Path

function Resolve-RequiredPath {
    param([string]$Path, [string]$Name, [switch]$Directory)
    $resolved = [System.IO.Path]::GetFullPath($Path)
    $pathType = if ($Directory) { "Container" } else { "Leaf" }
    if (-not (Test-Path -LiteralPath $resolved -PathType $pathType)) {
        throw "$Name is missing or has the wrong type: $resolved"
    }
    return $resolved
}

function Assert-NoReparsePointChain {
    param([string]$Path, [string]$Name)
    $current = [System.IO.Path]::GetFullPath($Path)
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -Force -LiteralPath $current
            if (($item.Attributes -band
                    [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Name contains a Windows reparse point: $current"
            }
        }
        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $current) {
            break
        }
        $current = $parent
    }
}

function Test-PathsOverlap {
    param([string]$First, [string]$Second)
    $firstFull = [System.IO.Path]::GetFullPath($First).TrimEnd("\", "/")
    $secondFull = [System.IO.Path]::GetFullPath($Second).TrimEnd("\", "/")
    if ($firstFull.Equals(
            $secondFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    $separator = [System.IO.Path]::DirectorySeparatorChar
    return $firstFull.StartsWith(
        $secondFull + $separator,
        [System.StringComparison]::OrdinalIgnoreCase) -or
        $secondFull.StartsWith(
            $firstFull + $separator,
            [System.StringComparison]::OrdinalIgnoreCase)
}

function Invoke-PythonStep {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [string]$StdoutPath,
        [string]$StderrPath
    )
    $commandLine = @($python.Source) + $Arguments
    ($commandLine | ConvertTo-Json -Compress) |
        Set-Content -LiteralPath ($StdoutPath + ".command.json") -Encoding UTF8
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $stdout = @(& $python.Source @Arguments 2> $StderrPath)
        $exitCode = $LASTEXITCODE
        $stdout | Set-Content -LiteralPath $StdoutPath -Encoding UTF8
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "$Name failed with exit code $exitCode. See $StdoutPath and $StderrPath"
    }
    return ($stdout -join [Environment]::NewLine)
}

function Invoke-PowerShellStep {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [string]$StdoutPath,
        [string]$StderrPath,
        [string]$LockedScriptPath = "",
        [string]$ExpectedScriptSha256 = ""
    )
    $commandLine = @($powerShellExecutable) + $Arguments
    ($commandLine | ConvertTo-Json -Compress) |
        Set-Content -LiteralPath ($StdoutPath + ".command.json") -Encoding UTF8
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $scriptLock = $null
    $sha256 = $null
    try {
        if (-not [string]::IsNullOrWhiteSpace($LockedScriptPath)) {
            if ($ExpectedScriptSha256 -notmatch "^[0-9a-f]{64}$") {
                throw "$Name requires a lowercase expected script SHA-256."
            }
            Assert-NoReparsePointChain $LockedScriptPath `
                "$Name locked script"
            $scriptLock = [System.IO.File]::Open(
                $LockedScriptPath,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                [System.IO.FileShare]::Read)
            Assert-NoReparsePointChain $LockedScriptPath `
                "$Name locked script"
            $sha256 = [System.Security.Cryptography.SHA256]::Create()
            $beforeDigest = $sha256.ComputeHash($scriptLock)
            $beforeSha256 = (
                ($beforeDigest | ForEach-Object { "{0:x2}" -f $_ }) -join "")
            if ($beforeSha256 -ne $ExpectedScriptSha256) {
                throw "$Name script identity changed before execution."
            }
        }
        $stdout = @(& $powerShellExecutable @Arguments 2> $StderrPath)
        $exitCode = $LASTEXITCODE
        $stdout | Set-Content -LiteralPath $StdoutPath -Encoding UTF8
        if ($null -ne $scriptLock) {
            $scriptLock.Position = 0
            $afterDigest = $sha256.ComputeHash($scriptLock)
            $afterSha256 = (
                ($afterDigest | ForEach-Object { "{0:x2}" -f $_ }) -join "")
            if ($afterSha256 -ne $ExpectedScriptSha256) {
                throw "$Name script identity changed during execution."
            }
            Assert-NoReparsePointChain $LockedScriptPath `
                "$Name locked script"
        }
    }
    finally {
        if ($null -ne $sha256) {
            $sha256.Dispose()
        }
        if ($null -ne $scriptLock) {
            $scriptLock.Dispose()
        }
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "$Name failed with exit code $exitCode. See $StdoutPath and $StderrPath"
    }
    return ($stdout -join [Environment]::NewLine)
}

function Get-CryptographicChallenge {
    $bytes = New-Object byte[] 32
    $generator = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    try {
        $generator.GetBytes($bytes)
    }
    finally {
        $generator.Dispose()
    }
    return (($bytes | ForEach-Object { "{0:x2}" -f $_ }) -join "")
}

function Write-ExclusiveOwnershipMarker {
    param(
        [string]$Path,
        [string]$Challenge
    )
    $encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $payload = $encoding.GetBytes($Challenge)
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $stream.Write($payload, 0, $payload.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
}

function Assert-OwnedEvidenceRoot {
    param(
        [string]$Root,
        [string]$MarkerPath,
        [string]$ExpectedChallenge
    )
    Assert-NoReparsePointChain $Root "EvidenceRoot ownership"
    if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
        throw "EvidenceRoot ownership marker is missing."
    }
    $marker = Get-Item -Force -LiteralPath $MarkerPath
    if (($marker.Attributes -band
            [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "EvidenceRoot ownership marker is a reparse point."
    }
    $observed = (
        Get-Content -Raw -LiteralPath $MarkerPath -Encoding UTF8).Trim()
    if ($observed -ne $ExpectedChallenge) {
        throw "EvidenceRoot ownership challenge mismatch."
    }
}

function Get-FileRecord {
    param([string]$Path, [string]$Root)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $item = Get-Item -LiteralPath $Path
    $rootPrefix = [System.IO.Path]::GetFullPath($Root).TrimEnd("\", "/") +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $item.FullName.StartsWith(
            $rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Evidence file is outside EvidenceRoot: $($item.FullName)"
    }
    $relativePath = $item.FullName.Substring($rootPrefix.Length).Replace("\", "/")
    return [ordered]@{
        path = $relativePath
        size = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$Package = Resolve-RequiredPath $Package "Package" -Directory
$PackageManifest = Resolve-RequiredPath $PackageManifest "PackageManifest"
$EvidenceKeyPath = Resolve-RequiredPath $EvidenceKeyPath "EvidenceKeyPath"
Assert-NoReparsePointChain $Package "Package"
Assert-NoReparsePointChain $PackageManifest "PackageManifest"
Assert-NoReparsePointChain $EvidenceKeyPath "EvidenceKeyPath"
$evidenceKey = [System.IO.File]::ReadAllBytes($EvidenceKeyPath)
if ($evidenceKey.Length -ne 32) {
    throw "EvidenceKeyPath must contain exactly 32 bytes."
}
if (Test-PathsOverlap $Package $EvidenceKeyPath) {
    throw "EvidenceKeyPath must be outside Package."
}
$DeploymentRoot = [System.IO.Path]::GetFullPath($DeploymentRoot)
Assert-NoReparsePointChain $DeploymentRoot "DeploymentRoot"
if (-not $DeploymentRoot.StartsWith(
        "D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "DeploymentRoot must be on D:\ for the P8 target-host preacceptance wrapper."
}
if (Test-PathsOverlap $DeploymentRoot $EvidenceKeyPath) {
    throw "EvidenceKeyPath must be outside DeploymentRoot."
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $DeploymentRoot (
        "evidence\p8-windows-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
$EvidenceRoot = [System.IO.Path]::GetFullPath($EvidenceRoot)
Assert-NoReparsePointChain $EvidenceRoot "EvidenceRoot"
if (-not $EvidenceRoot.StartsWith(
        "D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "EvidenceRoot must be on D:\."
}
if (Test-Path -LiteralPath $EvidenceRoot) {
    throw "EvidenceRoot must not already exist: $EvidenceRoot"
}
foreach ($inputPath in @($Package, $PackageManifest, $EvidenceKeyPath)) {
    if (Test-PathsOverlap $EvidenceRoot $inputPath) {
        throw "EvidenceRoot must not overlap package or immutable input paths: $inputPath"
    }
}
if ([string]::IsNullOrWhiteSpace($ReleaseId)) {
    $ReleaseId = "fixture-" + (Get-Date -Format "yyyyMMdd-HHmmss")
}
if ($SoakCommand.Count -eq 0 -or
    @($SoakCommand | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -ne 0) {
    throw "SoakCommand must contain non-empty argv entries."
}
$soakExecutable = Resolve-RequiredPath $SoakCommand[0] "SoakCommand executable"
Assert-NoReparsePointChain $soakExecutable "SoakCommand executable"
$packagePrefix = $Package.TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
if (-not $soakExecutable.StartsWith(
        $packagePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "SoakCommand executable must be a file inside Package."
}
$SoakCommand[0] = $soakExecutable

$preflight = Join-Path $PSScriptRoot "p8_preflight.py"
$hostReportCollector = Join-Path $PSScriptRoot "collect_windows_p8_host_reports.ps1"
$soak = Join-Path $PSScriptRoot "p8_soak_evidence.py"
$release = Join-Path $PSScriptRoot "p8_release.py"
$evidenceVerifier = Join-Path $PSScriptRoot "p8_windows_evidence_verify.py"
$gateConfig = Join-Path $repoRoot "config\p8-local-gates-v1.json"
foreach ($required in @(
        $preflight, $hostReportCollector, $soak, $release,
        $evidenceVerifier, $gateConfig)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required P8 tool is missing: $required"
    }
}
$gitHeadOutput = @(& $git.Source -C $repoRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot resolve the repository Git HEAD for provenance."
}
$gitHead = ($gitHeadOutput -join "").Trim().ToLowerInvariant()
if ($gitHead -notmatch "^[0-9a-f]{40}$") {
    throw "Repository Git HEAD is not a 40-digit SHA-1 identity."
}
$gitStatus = @(& $git.Source -C $repoRoot status --porcelain=v1 `
    --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot inspect the repository dirty state for provenance."
}
$gitDirty = ($gitStatus.Count -ne 0)

$hostChallenge = Get-CryptographicChallenge
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
Assert-NoReparsePointChain $EvidenceRoot "EvidenceRoot after creation"
$evidenceOwnershipPath = Join-Path $EvidenceRoot ".wrapper-owner"
Write-ExclusiveOwnershipMarker $evidenceOwnershipPath $hostChallenge
Assert-OwnedEvidenceRoot $EvidenceRoot $evidenceOwnershipPath $hostChallenge
$startedAt = Get-Date
$completedSteps = New-Object System.Collections.Generic.List[string]
$failure = $null
$packageResult = $null
$provenanceFiles = @()

try {
Assert-OwnedEvidenceRoot $EvidenceRoot $evidenceOwnershipPath $hostChallenge
$provenanceRoot = Join-Path $EvidenceRoot "provenance"
New-Item -ItemType Directory -Path $provenanceRoot | Out-Null
Assert-NoReparsePointChain $provenanceRoot "ProvenanceRoot after creation"
$provenanceSpecs = @(
    [ordered]@{
        role = "wrapper"
        source = $PSCommandPath
        repositoryPath = "scripts/run_windows_p8_preacceptance.ps1"
        targetName = "run_windows_p8_preacceptance.ps1"
    },
    [ordered]@{
        role = "preflight"
        source = $preflight
        repositoryPath = "scripts/p8_preflight.py"
        targetName = "p8_preflight.py"
    },
    [ordered]@{
        role = "host-report-collector"
        source = $hostReportCollector
        repositoryPath = "scripts/collect_windows_p8_host_reports.ps1"
        targetName = "collect_windows_p8_host_reports.ps1"
    },
    [ordered]@{
        role = "soak"
        source = $soak
        repositoryPath = "scripts/p8_soak_evidence.py"
        targetName = "p8_soak_evidence.py"
    },
    [ordered]@{
        role = "release"
        source = $release
        repositoryPath = "scripts/p8_release.py"
        targetName = "p8_release.py"
    },
    [ordered]@{
        role = "evidence-verifier"
        source = $evidenceVerifier
        repositoryPath = "scripts/p8_windows_evidence_verify.py"
        targetName = "p8_windows_evidence_verify.py"
    },
    [ordered]@{
        role = "gate-config"
        source = $gateConfig
        repositoryPath = "config/p8-local-gates-v1.json"
        targetName = "p8-local-gates-v1.json"
    }
)
foreach ($spec in $provenanceSpecs) {
    Assert-OwnedEvidenceRoot $EvidenceRoot `
        $evidenceOwnershipPath $hostChallenge
    $target = Join-Path $provenanceRoot $spec.targetName
    Copy-Item -LiteralPath $spec.source -Destination $target
    $record = Get-FileRecord $target $EvidenceRoot
    $provenanceFiles += [ordered]@{
        role = $spec.role
        repositoryPath = $spec.repositoryPath
        path = $record.path
        size = $record.size
        sha256 = $record.sha256
    }
}
$preflight = Join-Path $provenanceRoot "p8_preflight.py"
$hostReportCollector = Join-Path $provenanceRoot `
    "collect_windows_p8_host_reports.ps1"
$soak = Join-Path $provenanceRoot "p8_soak_evidence.py"
$release = Join-Path $provenanceRoot "p8_release.py"
$evidenceVerifier = Join-Path $provenanceRoot "p8_windows_evidence_verify.py"
$gateConfig = Join-Path $provenanceRoot "p8-local-gates-v1.json"
$preflightRoot = Join-Path $EvidenceRoot "preflight"
$soakRoot = Join-Path $EvidenceRoot "soak"
$releaseLogs = Join-Path $EvidenceRoot "release"
New-Item -ItemType Directory -Path $releaseLogs | Out-Null
$previousStatePath = Join-Path $DeploymentRoot "state\current-release.json"
$previousState = if (Test-Path -LiteralPath $previousStatePath -PathType Leaf) {
    Get-Content -Raw -LiteralPath $previousStatePath -Encoding UTF8 | ConvertFrom-Json
} else { $null }
    $hostInputsRoot = Join-Path $EvidenceRoot "host-inputs"
    Assert-OwnedEvidenceRoot $EvidenceRoot `
        $evidenceOwnershipPath $hostChallenge
    $collectorArguments = @(
        "-NoLogo",
        "-NoProfile",
        "-File", $hostReportCollector,
        "-RepositoryRoot", $repoRoot,
        "-Package", $Package,
        "-OutputDirectory", $hostInputsRoot,
        "-Challenge", $hostChallenge,
        "-PackageManifestSha256",
        $PackageManifestSha256.ToLowerInvariant()
    )
    $collectorProvenance = @($provenanceFiles | Where-Object {
            $_.role -eq "host-report-collector"
        })
    if ($collectorProvenance.Count -ne 1) {
        throw "Host report collector provenance identity is ambiguous."
    }
    $collectorJson = Invoke-PowerShellStep "host-report-collection" `
        $collectorArguments `
        (Join-Path $EvidenceRoot "host-collector.stdout.json") `
        (Join-Path $EvidenceRoot "host-collector.stderr.log") `
        -LockedScriptPath $hostReportCollector `
        -ExpectedScriptSha256 $collectorProvenance[0].sha256
    $collectorResult = $collectorJson | ConvertFrom-Json
    if (
        $collectorResult.status -ne "collection-complete" -or
        $collectorResult.challenge -ne $hostChallenge -or
        $collectorResult.packageManifestSha256 -ne
            $PackageManifestSha256.ToLowerInvariant()
    ) {
        throw "Host report collector did not bind the current wrapper challenge."
    }
    $WindowsInput = Join-Path $hostInputsRoot "windows.json"
    $GpuInput = Join-Path $hostInputsRoot "gpu.json"
    $completedSteps.Add("host-report-collection")

    $preflightArguments = @(
        $preflight,
        "--gate-config", $gateConfig,
        "--package", $Package,
        "--manifest", $PackageManifest,
        "--manifest-sha256", $PackageManifestSha256.ToLowerInvariant(),
        "--host-challenge", $hostChallenge,
        "--windows-input", $WindowsInput,
        "--gpu-input", $GpuInput,
        "--output-dir", $preflightRoot
    )
    [void](Invoke-PythonStep "preflight" $preflightArguments `
        (Join-Path $EvidenceRoot "preflight.stdout.log") `
        (Join-Path $EvidenceRoot "preflight.stderr.log"))
    $completedSteps.Add("preflight")

    $soakArguments = @(
        $soak,
        "--evidence-root", $soakRoot,
        "--profile", "ci-contract-v1",
        "--cwd", $Package,
        "--rounds", [string]$Rounds,
        "--restarts", [string]$Restarts,
        "--timeout-seconds", [string]$TimeoutSeconds,
        "--"
    ) + $SoakCommand
    [void](Invoke-PythonStep "soak" $soakArguments `
        (Join-Path $EvidenceRoot "soak.stdout.log") `
        (Join-Path $EvidenceRoot "soak.stderr.log"))
    $completedSteps.Add("soak")

    $postSoakPreflightArguments = @(
        $preflight,
        "--gate-config", $gateConfig,
        "--package", $Package,
        "--manifest", $PackageManifest,
        "--manifest-sha256", $PackageManifestSha256.ToLowerInvariant(),
        "--host-challenge", $hostChallenge,
        "--windows-input", $WindowsInput,
        "--gpu-input", $GpuInput,
        "--output-dir", (Join-Path $EvidenceRoot "preflight-after-soak")
    )
    [void](Invoke-PythonStep "preflight-after-soak" $postSoakPreflightArguments `
        (Join-Path $EvidenceRoot "preflight-after-soak.stdout.log") `
        (Join-Path $EvidenceRoot "preflight-after-soak.stderr.log"))
    $completedSteps.Add("preflight-after-soak")

    $packageArguments = @(
        $release, "package",
        "--source", $Package,
        "--deployment-root", $DeploymentRoot,
        "--release-id", $ReleaseId,
        "--source-manifest", $PackageManifest,
        "--source-manifest-sha256", $PackageManifestSha256.ToLowerInvariant()
    )
    $packageJson = Invoke-PythonStep "release-package" $packageArguments `
        (Join-Path $releaseLogs "package.stdout.json") `
        (Join-Path $releaseLogs "package.stderr.log")
    $packageResult = $packageJson | ConvertFrom-Json
    if ($packageResult.sourceManifestSha256 -ne
        $PackageManifestSha256.ToLowerInvariant()) {
        throw "release-package did not bind the externally supplied package manifest SHA-256."
    }
    $completedSteps.Add("release-package")

    $verifyArguments = @(
        $release, "verify",
        "--deployment-root", $DeploymentRoot,
        "--release-id", $ReleaseId,
        "--manifest-sha256", $packageResult.manifestSha256
    )
    [void](Invoke-PythonStep "release-verify" $verifyArguments `
        (Join-Path $releaseLogs "verify.stdout.json") `
        (Join-Path $releaseLogs "verify.stderr.log"))
    $completedSteps.Add("release-verify")

    $activateArguments = @(
        $release, "activate",
        "--deployment-root", $DeploymentRoot,
        "--release-id", $ReleaseId,
        "--manifest-sha256", $packageResult.manifestSha256
    )
    $activateJson = Invoke-PythonStep "release-activate" $activateArguments `
        (Join-Path $releaseLogs "activate.stdout.json") `
        (Join-Path $releaseLogs "activate.stderr.log")
    $activateResult = $activateJson | ConvertFrom-Json
    if ($activateResult.current.releaseId -ne $ReleaseId -or
        $activateResult.current.manifestSha256 -ne $packageResult.manifestSha256) {
        throw "release-activate did not return the requested current release identity."
    }
    $completedSteps.Add("release-activate")

    if ($ExerciseRollback) {
        if ($null -eq $previousState) {
            throw "ExerciseRollback requires an existing current release before this run."
        }
        $rollbackArguments = @(
            $release, "rollback",
            "--deployment-root", $DeploymentRoot
        )
        $rollbackJson = Invoke-PythonStep "release-rollback" $rollbackArguments `
            (Join-Path $releaseLogs "rollback.stdout.json") `
            (Join-Path $releaseLogs "rollback.stderr.log")
        $rollbackResult = $rollbackJson | ConvertFrom-Json
        if ($rollbackResult.current.releaseId -ne $previousState.current.releaseId -or
            $rollbackResult.current.manifestSha256 -ne
                $previousState.current.manifestSha256) {
            throw "release-rollback did not restore the original current release identity."
        }
        $completedSteps.Add("release-rollback")
    }
}
catch {
    $failure = $_.Exception.ToString()
}
finally {
    Assert-OwnedEvidenceRoot $EvidenceRoot `
        $evidenceOwnershipPath $hostChallenge
    $wrapperManifestPath = Join-Path $EvidenceRoot "wrapper-manifest.json"
    $evidenceFiles = @(Get-ChildItem -LiteralPath $EvidenceRoot -Recurse -File |
        Where-Object FullName -ne $wrapperManifestPath |
        Sort-Object FullName |
        ForEach-Object { Get-FileRecord $_.FullName $EvidenceRoot })
    $passed = ($null -eq $failure)
    $wrapperManifestDocument = [ordered]@{
        schemaVersion = "p8-windows-preacceptance-wrapper-v4"
        scope = "local-tooling-on-windows-host-only"
        overallResult = if ($passed) { "passed-local-tooling" } else { "failed" }
        productAcceptanceClaimed = $false
        dDriveTargeted = $true
        realIoEnabled = $null
        realRejectEnabled = $null
        realIoEnabledClaimed = $false
        realRejectEnabledClaimed = $false
        packageConfigurationRequiredRealRejectDisabled = $true
        startedAt = $startedAt.ToString("o")
        finishedAt = (Get-Date).ToString("o")
        deploymentRoot = $DeploymentRoot
        evidenceRoot = $EvidenceRoot
        releaseId = $ReleaseId
        hostReportChallenge = $hostChallenge
        packageManifestSha256 = $PackageManifestSha256.ToLowerInvariant()
        releaseManifestSha256 = if ($null -eq $packageResult) {
            $null
        } else {
            $packageResult.manifestSha256
        }
        sourceManifestSha256 = if ($null -eq $packageResult) {
            $null
        } else {
            $packageResult.sourceManifestSha256
        }
        sourceVersion = [ordered]@{
            gitHead = $gitHead
            gitDirty = $gitDirty
        }
        provenanceFiles = $provenanceFiles
        rollbackRequested = [bool]$ExerciseRollback
        rollbackExercised = $completedSteps.Contains("release-rollback")
        completedSteps = @($completedSteps)
        failure = $failure
        files = $evidenceFiles
    }
    $wrapperManifestJson = (
        $wrapperManifestDocument | ConvertTo-Json -Depth 8
    ) + [Environment]::NewLine
    $wrapperManifestEncoding = New-Object System.Text.UTF8Encoding `
        -ArgumentList $false
    $wrapperManifestBytes = $wrapperManifestEncoding.GetBytes(
        $wrapperManifestJson)
    $wrapperManifestStream = [System.IO.File]::Open(
        $wrapperManifestPath,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $wrapperManifestStream.Write(
            $wrapperManifestBytes, 0, $wrapperManifestBytes.Length)
        $wrapperManifestStream.Flush($true)
    }
    finally {
        $wrapperManifestStream.Dispose()
    }
}

$sha256Provider = [System.Security.Cryptography.SHA256]::Create()
try {
    $wrapperManifestSha256 = (
        ($sha256Provider.ComputeHash($wrapperManifestBytes) |
            ForEach-Object { "{0:x2}" -f $_ }) -join "")
}
finally {
    $sha256Provider.Dispose()
}
$hmacProvider = New-Object System.Security.Cryptography.HMACSHA256 `
    -ArgumentList (, $evidenceKey)
try {
    $wrapperManifestHmacSha256 = (
        ($hmacProvider.ComputeHash($wrapperManifestBytes) |
            ForEach-Object { "{0:x2}" -f $_ }) -join "")
}
finally {
    $hmacProvider.Dispose()
    [Array]::Clear($evidenceKey, 0, $evidenceKey.Length)
}
$persistedManifestBytes = [System.IO.File]::ReadAllBytes($wrapperManifestPath)
if (
    [Convert]::ToBase64String($persistedManifestBytes) -ne
    [Convert]::ToBase64String($wrapperManifestBytes)
) {
    throw "Wrapper manifest changed before its trust anchors were emitted."
}
if ($null -ne $failure) {
    Write-Error "P8 Windows preacceptance wrapper failed. Evidence: $EvidenceRoot"
    Write-Error "Wrapper manifest SHA-256: $wrapperManifestSha256"
    Write-Error "Wrapper manifest HMAC-SHA-256: $wrapperManifestHmacSha256"
    exit 1
}
Write-Host "PASS local tooling on Windows host; no product acceptance claim. Evidence: $EvidenceRoot"
Write-Host "Record externally: wrapper manifest SHA-256 $wrapperManifestSha256"
Write-Host (
    "Record externally: wrapper manifest HMAC-SHA-256 " +
    $wrapperManifestHmacSha256)
Write-Host "Trusted offline verify only; never execute provenance code from the unverified evidence bundle:"
Write-Host (
    "python `"<trusted-repository>\scripts\p8_windows_evidence_verify.py`" " +
    "verify --evidence-root `"$EvidenceRoot`" " +
    "--manifest-sha256 $wrapperManifestSha256 " +
    "--manifest-hmac-sha256 $wrapperManifestHmacSha256 " +
    "--evidence-key `"<trusted-evidence-key>`""
)
exit 0
