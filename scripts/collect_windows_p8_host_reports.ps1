param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,
    [Parameter(Mandatory = $true)]
    [string]$Package,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-f]{64}$")]
    [string]$Challenge,
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9a-f]{64}$")]
    [string]$PackageManifestSha256
)

$ErrorActionPreference = "Stop"
$collectorRepositoryPath = "scripts/collect_windows_p8_host_reports.ps1"
$collectorSchemaVersion = "cigvision-p8-host-collector-v1"

function Write-RejectionAndExit {
    param(
        [string]$ReasonCode,
        [string]$Detail
    )

    if ([string]::IsNullOrWhiteSpace($Detail)) {
        $Detail = "The host-report collection request was rejected."
    }
    $result = [ordered]@{
        schemaVersion = "cigvision-p8-host-collector-summary-v1"
        status = "rejected"
        exitCode = 2
        reasonCode = $ReasonCode
        detail = $Detail
    }
    Write-Output ($result | ConvertTo-Json -Compress -Depth 4)
    exit 2
}

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

function Assert-NoReparsePointChain {
    param(
        [string]$Path,
        [string]$Name
    )

    $current = [System.IO.Path]::GetFullPath($Path)
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -Force -LiteralPath $current
            if (($item.Attributes -band
                    [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "$Name contains an existing reparse point: $current"
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
    param(
        [string]$First,
        [string]$Second
    )

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

function Get-LowerSha256Text {
    param([string]$Text)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $digest = $sha256.ComputeHash($bytes)
        return (($digest | ForEach-Object { "{0:x2}" -f $_ }) -join "")
    }
    finally {
        $sha256.Dispose()
    }
}

function Resolve-Application {
    param([string[]]$Names)

    foreach ($name in $Names) {
        $command = Get-Command $name -CommandType Application `
            -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $command) {
            return $command.Source
        }
    }
    return $null
}

function Get-LockedFileSnapshot {
    param(
        [string]$Path,
        [string]$Name
    )

    Assert-NoReparsePointChain $Path $Name
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read)
    $sha256 = $null
    try {
        Assert-NoReparsePointChain $Path $Name
        $item = Get-Item -Force -LiteralPath $Path
        if (($item.Attributes -band
                [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Name is a reparse-point file: $Path"
        }
        if ($item.Length -le 0 -or $stream.Length -ne $item.Length) {
            throw "$Name is empty or changed while being opened: $Path"
        }
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $digest = $sha256.ComputeHash($stream)
        $digestText = (
            ($digest | ForEach-Object { "{0:x2}" -f $_ }) -join "")
        Assert-NoReparsePointChain $Path $Name
        $finalItem = Get-Item -Force -LiteralPath $Path
        if (($finalItem.Attributes -band
                [System.IO.FileAttributes]::ReparsePoint) -ne 0 -or
            $finalItem.Length -ne $stream.Length) {
            throw "$Name changed while its identity was captured: $Path"
        }
        return [pscustomobject][ordered]@{
            path = $finalItem.FullName
            size = $stream.Length
            sha256 = $digestText
            version = [string]$finalItem.VersionInfo.FileVersion
        }
    }
    finally {
        if ($null -ne $sha256) {
            $sha256.Dispose()
        }
        $stream.Dispose()
    }
}

function Get-ApplicationAssessment {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or
        -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [pscustomobject][ordered]@{
            passed = $false
            path = "not-found"
            version = "unavailable"
            detail = "application file is unavailable"
        }
    }
    try {
        $snapshot = Get-LockedFileSnapshot $Path "application"
    }
    catch {
        return [pscustomobject][ordered]@{
            passed = $false
            path = [System.IO.Path]::GetFullPath($Path)
            version = "unavailable"
            detail = "application identity rejected: $($_.Exception.Message)"
        }
    }
    $version = $snapshot.version
    $passed =
        -not [string]::IsNullOrWhiteSpace($version)
    return [pscustomobject][ordered]@{
        passed = $passed
        path = $snapshot.path
        version = if ([string]::IsNullOrWhiteSpace($version)) {
            "unavailable"
        }
        else {
            $version
        }
        detail = "path=$($snapshot.path); bytes=$($snapshot.size); " +
            "sha256=$($snapshot.sha256); fileVersion=$version; " +
            "lockedReadSnapshot=true; reparseChain=false"
    }
}

function Get-CheckRecord {
    param(
        [string]$Id,
        [bool]$Passed,
        [string]$Detail
    )

    if ([string]::IsNullOrWhiteSpace($Detail)) {
        $Detail = "No detail was available."
    }
    return [pscustomobject][ordered]@{
        id = $Id
        status = if ($Passed) { "passed" } else { "failed" }
        detail = $Detail
    }
}

function Get-MissingContractTokens {
    param(
        [string]$ProjectText,
        [string[]]$Tokens
    )

    $missingTokens = New-Object System.Collections.Generic.List[string]
    foreach ($token in $Tokens) {
        if ([string]::IsNullOrWhiteSpace($ProjectText) -or
            $ProjectText.IndexOf(
                $token, [System.StringComparison]::Ordinal) -lt 0) {
            $missingTokens.Add($token)
        }
    }
    return @($missingTokens)
}

function Get-DependencyAssessment {
    param(
        [string]$Root,
        [string[]]$RequiredPaths,
        [string]$ProjectText,
        [string[]]$ContractTokens
    )

    $problems = New-Object System.Collections.Generic.List[string]
    $fileIdentities = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($Root)) {
        $problems.Add("root environment value is missing")
    }
    else {
        try {
            Assert-NoReparsePointChain $Root "dependency root"
        }
        catch {
            $problems.Add("unsafe dependency root: $($_.Exception.Message)")
        }
    }
    foreach ($requiredPath in $RequiredPaths) {
        if ([string]::IsNullOrWhiteSpace($requiredPath) -or
            -not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            $problems.Add("missing file: $requiredPath")
            continue
        }
        try {
            $requiredSnapshot = Get-LockedFileSnapshot $requiredPath `
                "dependency file"
            $fileIdentities.Add(
                "$($requiredSnapshot.path)|$($requiredSnapshot.size)|" +
                $requiredSnapshot.sha256)
        }
        catch {
            $problems.Add(
                "unsafe dependency file: $requiredPath; " +
                $_.Exception.Message)
        }
    }
    $missingTokens = @(Get-MissingContractTokens $ProjectText $ContractTokens)
    foreach ($missingToken in $missingTokens) {
        $problems.Add("vcxproj token missing: $missingToken")
    }

    if ($problems.Count -eq 0) {
        return [pscustomobject][ordered]@{
            passed = $true
            detail = "root=$Root; files=" + ($fileIdentities -join "; ") +
                "; vcxproj=matched"
        }
    }
    $rootDetail = if ([string]::IsNullOrWhiteSpace($Root)) {
        "not-configured"
    }
    else {
        $Root
    }
    return [pscustomobject][ordered]@{
        passed = $false
        detail = "root=$rootDetail; validFiles=" +
            ($fileIdentities -join "; ") + "; problems=" +
            ($problems -join "; ")
    }
}

function Write-Utf8AtomicNoOverwrite {
    param(
        [string]$Path,
        [string]$Content
    )

    if (Test-Path -LiteralPath $Path) {
        throw "Refusing to overwrite an existing report: $Path"
    }
    $directory = Split-Path -Parent $Path
    $temporaryPath = Join-Path $directory (
        "." + [System.IO.Path]::GetFileName($Path) + "." +
        [Guid]::NewGuid().ToString("N") + ".tmp")
    $encoding = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $payload = $encoding.GetBytes($Content + [Environment]::NewLine)
    $stream = $null
    try {
        $stream = [System.IO.File]::Open(
            $temporaryPath,
            [System.IO.FileMode]::CreateNew,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None)
        $stream.Write($payload, 0, $payload.Length)
        $stream.Flush($true)
        $stream.Dispose()
        $stream = $null
        [System.IO.File]::Move($temporaryPath, $Path)
    }
    finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
        if (Test-Path -LiteralPath $temporaryPath) {
            Remove-Item -Force -LiteralPath $temporaryPath
        }
    }
}

function Assert-OwnedOutputDirectory {
    param(
        [string]$Directory,
        [string]$MarkerPath,
        [string]$ExpectedChallenge
    )

    Assert-NoReparsePointChain $Directory "OutputDirectory ownership"
    if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
        throw "OutputDirectory ownership marker is missing."
    }
    $marker = Get-Item -Force -LiteralPath $MarkerPath
    if (($marker.Attributes -band
            [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "OutputDirectory ownership marker is a reparse point."
    }
    $observedChallenge = (
        Get-Content -Raw -LiteralPath $MarkerPath -Encoding UTF8).Trim()
    if ($observedChallenge -ne $ExpectedChallenge) {
        throw "OutputDirectory ownership challenge mismatch."
    }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    Write-RejectionAndExit "host.not-windows" (
        "This collector only runs on Windows; detected platform " +
        [Environment]::OSVersion.Platform + ".")
}

try {
    $resolvedRepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
    if (-not (Test-Path -LiteralPath $resolvedRepositoryRoot -PathType Container)) {
        throw "RepositoryRoot must be an existing directory: $resolvedRepositoryRoot"
    }
    $resolvedRepositoryRoot = (
        Get-Item -Force -LiteralPath $resolvedRepositoryRoot).FullName
    $resolvedPackage = [System.IO.Path]::GetFullPath($Package)
    if (-not (Test-Path -LiteralPath $resolvedPackage -PathType Container)) {
        throw "Package must be an existing directory: $resolvedPackage"
    }
    $resolvedPackage = (Get-Item -Force -LiteralPath $resolvedPackage).FullName

    $resolvedOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
    $outputRoot = [System.IO.Path]::GetPathRoot($resolvedOutputDirectory)
    if (-not $outputRoot.Equals(
            "D:\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "OutputDirectory must be located on D:\."
    }
    if (Test-Path -LiteralPath $resolvedOutputDirectory) {
        throw "OutputDirectory must not already exist: $resolvedOutputDirectory"
    }
    if (Test-PathsOverlap $resolvedPackage $resolvedOutputDirectory) {
        throw "Package and OutputDirectory must not overlap."
    }
    if (Test-PathsOverlap $resolvedRepositoryRoot $resolvedOutputDirectory) {
        throw "RepositoryRoot and OutputDirectory must not overlap."
    }
    Assert-NoReparsePointChain $resolvedRepositoryRoot "RepositoryRoot"
    Assert-NoReparsePointChain $resolvedPackage "Package"
    Assert-NoReparsePointChain $resolvedOutputDirectory "OutputDirectory"

    if ([string]::IsNullOrWhiteSpace($PSCommandPath) -or
        -not (Test-Path -LiteralPath $PSCommandPath -PathType Leaf)) {
        throw "The collector source path cannot be resolved."
    }
    $collectorSourcePath = [System.IO.Path]::GetFullPath($PSCommandPath)
    $collectorSha256 = (Get-FileHash -LiteralPath $collectorSourcePath `
        -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($collectorSha256 -notmatch "^[0-9a-f]{64}$") {
        throw "The collector SHA-256 is invalid."
    }

    New-Item -ItemType Directory -Path $resolvedOutputDirectory |
        Out-Null
    Assert-NoReparsePointChain $resolvedOutputDirectory "OutputDirectory"
    $ownershipMarkerPath = Join-Path $resolvedOutputDirectory `
        ".collector-owner"
    Write-Utf8AtomicNoOverwrite $ownershipMarkerPath $Challenge
    Assert-OwnedOutputDirectory $resolvedOutputDirectory `
        $ownershipMarkerPath $Challenge
}
catch {
    Write-RejectionAndExit "request.invalid" $_.Exception.Message
}

trap {
    $collectionFailure = $_.Exception.Message
    Write-RejectionAndExit "collection.failed" $collectionFailure
}

$captureId = [Guid]::NewGuid().ToString("N").ToLowerInvariant()
$capturedAtUtc = [DateTime]::UtcNow.ToString(
    "yyyy-MM-ddTHH:mm:ss.fffffffZ",
    [System.Globalization.CultureInfo]::InvariantCulture)

$machineGuid = $null
try {
    $machineGuid = [Microsoft.Win32.Registry]::GetValue(
        "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography",
        "MachineGuid",
        $null)
}
catch {
    $machineGuid = $null
}
if ([string]::IsNullOrWhiteSpace([string]$machineGuid)) {
    $machineGuid = "machine-guid-unavailable"
}
$hostIdentityMaterial = (
    "machineGuid={0}|computer={1}|domain={2}|os64={3}" -f
    $machineGuid,
    $env:COMPUTERNAME,
    $env:USERDOMAIN,
    [Environment]::Is64BitOperatingSystem)
$hostIdSha256 = Get-LowerSha256Text $hostIdentityMaterial

$repoRoot = $resolvedRepositoryRoot
$solutionPath = Get-ChildItem -LiteralPath $repoRoot -Recurse `
    -Filter "CigVision.sln" -File -ErrorAction SilentlyContinue |
    Where-Object {
        Test-Path -LiteralPath (
            Join-Path $_.DirectoryName "CigVision.vcxproj") -PathType Leaf
    } |
    Select-Object -First 1 -ExpandProperty FullName
$projectPath = if ([string]::IsNullOrWhiteSpace($solutionPath)) {
    $null
}
else {
    Join-Path (Split-Path -Parent $solutionPath) "CigVision.vcxproj"
}
$projectText = if ($null -ne $projectPath -and
    (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
    Get-Content -Raw -LiteralPath $projectPath -Encoding UTF8
}
else {
    ""
}

$windowsChecks = New-Object System.Collections.Generic.List[object]
$gpuChecks = New-Object System.Collections.Generic.List[object]

$isWindowsHost = (
    [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT)
$windowsChecks.Add((Get-CheckRecord "host.windows" $isWindowsHost (
    "platform={0}; version={1}" -f
    [Environment]::OSVersion.Platform,
    [Environment]::OSVersion.VersionString)))

$processorArchitecture = [string]$env:PROCESSOR_ARCHITECTURE
$wow64Architecture = [string]$env:PROCESSOR_ARCHITEW6432
$isX64Host = [Environment]::Is64BitOperatingSystem -and
    ($processorArchitecture -eq "AMD64" -or $wow64Architecture -eq "AMD64")
$windowsChecks.Add((Get-CheckRecord "host.architecture-x64" $isX64Host (
    "is64BitOperatingSystem={0}; processorArchitecture={1}; " +
    "processorArchitectureW6432={2}" -f
    [Environment]::Is64BitOperatingSystem,
    $processorArchitecture,
    $wow64Architecture)))

$dDrive = Get-PSDrive -Name D -PSProvider FileSystem `
    -ErrorAction SilentlyContinue
$dDrivePassed = $null -ne $dDrive -and
    (Test-Path -LiteralPath "D:\" -PathType Container) -and
    $resolvedOutputDirectory.StartsWith(
        "D:\", [System.StringComparison]::OrdinalIgnoreCase)
$dDriveDetail = if ($null -eq $dDrive) {
    "D: FileSystem drive is unavailable"
}
else {
    "root=$($dDrive.Root); outputDirectory=$resolvedOutputDirectory"
}
$windowsChecks.Add((Get-CheckRecord "storage.d-drive" $dDrivePassed $dDriveDetail))

$powerShellVersion = $PSVersionTable.PSVersion
$powerShellPassed = $powerShellVersion.Major -gt 5 -or
    ($powerShellVersion.Major -eq 5 -and $powerShellVersion.Minor -ge 1)
$powerShellEdition = if ($PSVersionTable.PSObject.Properties.Name -contains
    "PSEdition") {
    [string]$PSVersionTable.PSEdition
}
else {
    "Desktop"
}
$windowsChecks.Add((Get-CheckRecord "tool.powershell" $powerShellPassed (
    "version=$powerShellVersion; edition=$powerShellEdition; " +
    "compatibleMinimum=5.1")))

$pythonPath = Resolve-Application @("python.exe", "python")
$pythonAssessment = Get-ApplicationAssessment $pythonPath
$windowsChecks.Add((Get-CheckRecord "tool.python" `
    $pythonAssessment.passed $pythonAssessment.detail))

$gitPath = Resolve-Application @("git.exe", "git")
$gitAssessment = Get-ApplicationAssessment $gitPath
$windowsChecks.Add((Get-CheckRecord "tool.git" `
    $gitAssessment.passed $gitAssessment.detail))

$msbuildPath = Resolve-Application @("msbuild.exe")
$msbuildAssessment = Get-ApplicationAssessment $msbuildPath
$msbuildContractMissing = @(Get-MissingContractTokens $projectText @(
        "<PlatformToolset>v143</PlatformToolset>",
        "Release|x64"))
$msbuildPassed = $msbuildAssessment.passed -and
    $msbuildContractMissing.Count -eq 0
$msbuildDetail = if ($msbuildPassed) {
    "$($msbuildAssessment.detail); vcxproj=v143 Release|x64"
}
else {
    "application={0}; missingContract={1}" -f
    $msbuildAssessment.detail,
    $(if ($msbuildContractMissing.Count -eq 0) {
            "none"
        }
        else {
            $msbuildContractMissing -join ", "
        })
}
$windowsChecks.Add((Get-CheckRecord "tool.msbuild" $msbuildPassed $msbuildDetail))

$qtRoot = Get-EnvironmentValue "CIGVISION_QT_ROOT"
$qmakePath = if ([string]::IsNullOrWhiteSpace($qtRoot)) {
    $null
}
else {
    Join-Path $qtRoot "bin\qmake.exe"
}
$qtCorePath = if ([string]::IsNullOrWhiteSpace($qtRoot)) {
    $null
}
else {
    Join-Path $qtRoot "bin\Qt5Core.dll"
}
$qmakeAssessment = Get-ApplicationAssessment $qmakePath
$qtCoreAssessment = Get-ApplicationAssessment $qtCorePath
$qtContractMissing = @(Get-MissingContractTokens $projectText @(
        "<QtInstall>5.9.9_msvc2017_64</QtInstall>",
        '$(Qt_LIBS_)'))
$qtPassed = $qmakeAssessment.passed -and $qtCoreAssessment.passed -and
    $qtCoreAssessment.version -match "^5\.9\.9(?:\.|$)" -and
    $qtContractMissing.Count -eq 0
$qtDetail = "qmake={0}; qtCore={1}; expected=5.9.9; missingContract={2}" -f
    $qmakeAssessment.detail,
    $qtCoreAssessment.detail,
    $(if ($qtContractMissing.Count -eq 0) {
            "none"
        }
        else {
            $qtContractMissing -join ", "
        })
$windowsChecks.Add((Get-CheckRecord "dependency.qt-5.9.9" $qtPassed $qtDetail))

$halconReleaseRoot = Get-EnvironmentValue "CIGVISION_HALCON_RELEASE_ROOT"
if ([string]::IsNullOrWhiteSpace($halconReleaseRoot)) {
    $halconReleaseRoot = "C:\Program Files\MVTec\HALCON-22.11-Steady"
}
$halconAssessment = Get-DependencyAssessment $halconReleaseRoot @(
    (Join-Path $halconReleaseRoot "include\halconcpp\HalconCpp.h"),
    (Join-Path $halconReleaseRoot "lib\x64-win64\halcon.lib"),
    (Join-Path $halconReleaseRoot "lib\x64-win64\halconcpp.lib"),
    (Join-Path $halconReleaseRoot "bin\x64-win64\halcon.dll"),
    (Join-Path $halconReleaseRoot "bin\x64-win64\halconcpp.dll")
) $projectText @(
    '$(CIGVISION_HALCON_RELEASE_ROOT)\include',
    '$(CIGVISION_HALCON_RELEASE_ROOT)\lib\x64-win64',
    "halconcpp.lib;halcon.lib")
$windowsChecks.Add((Get-CheckRecord "dependency.halcon-release" `
    $halconAssessment.passed $halconAssessment.detail))

$mvsRoot = Get-EnvironmentValue "MVCAM_COMMON_RUNENV"
if ([string]::IsNullOrWhiteSpace($mvsRoot)) {
    $mvsRoot = "C:\Program Files (x86)\MVS\Development"
}
$mvsRuntime = "C:\Program Files (x86)\Common Files\MVS\" +
    "Runtime\Win64_x64\MvCameraControl.dll"
$derivedMvsRuntime = [System.IO.Path]::GetFullPath(
    (Join-Path $mvsRoot (
            "..\..\Common Files\MVS\" +
            "Runtime\Win64_x64\MvCameraControl.dll")))
if (Test-Path -LiteralPath $derivedMvsRuntime -PathType Leaf) {
    $mvsRuntime = $derivedMvsRuntime
}
$mvsAssessment = Get-DependencyAssessment $mvsRoot @(
    (Join-Path $mvsRoot "Includes\MvCameraControl.h"),
    (Join-Path $mvsRoot "Libraries\win64\MvCameraControl.lib"),
    $mvsRuntime
) $projectText @(
    '$(MVCAM_COMMON_RUNENV)\Includes',
    '$(MVCAM_COMMON_RUNENV)\Libraries\win64',
    "MvCameraControl.lib")
$windowsChecks.Add((Get-CheckRecord "dependency.mvs" `
    $mvsAssessment.passed $mvsAssessment.detail))

$daqNaviRoot = Get-EnvironmentValue "CIGVISION_DAQNAVI_ROOT"
if ([string]::IsNullOrWhiteSpace($daqNaviRoot)) {
    $daqNaviRoot = "C:\Advantech\DAQNavi"
}
$daqAssessment = Get-DependencyAssessment $daqNaviRoot @(
    (Join-Path $daqNaviRoot "Inc\bdaqctrl.h"),
    "C:\Windows\System32\biodaq.dll"
) $projectText @(
    '$(CIGVISION_DAQNAVI_ROOT)\Inc')
$windowsChecks.Add((Get-CheckRecord "dependency.daqnavi" `
    $daqAssessment.passed $daqAssessment.detail))

$packageConfigPath = Join-Path $resolvedPackage "config.ini"
$rejectDisabledPassed = $false
$rejectDisabledDetail = "config.ini is missing: $packageConfigPath"
if (Test-Path -LiteralPath $packageConfigPath -PathType Leaf) {
    $configItem = Get-Item -Force -LiteralPath $packageConfigPath
    if (($configItem.Attributes -band
            [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        $rejectDisabledDetail = "config.ini is a reparse point: $packageConfigPath"
    }
    else {
        $configText = Get-Content -Raw -LiteralPath $packageConfigPath `
            -Encoding UTF8
        $rejectAssignments = @([regex]::Matches(
                $configText,
                "(?im)^\s*rejectEnabled\s*=\s*([^\s;#]+)\s*(?:[;#].*)?$"))
        $rejectDisabledPassed = $rejectAssignments.Count -eq 1 -and
            $rejectAssignments[0].Groups[1].Value -eq "false"
        $rejectDisabledDetail = if ($rejectDisabledPassed) {
            "package config.ini contains exactly rejectEnabled=false"
        }
        else {
            "package config.ini must contain exactly one rejectEnabled=false assignment"
        }
    }
}
$windowsChecks.Add((Get-CheckRecord "safety.reject-disabled" `
    $rejectDisabledPassed $rejectDisabledDetail))

$nvidiaSmiPath = Join-Path $env:SystemRoot "System32\nvidia-smi.exe"
$nvidiaSmiAssessment = Get-ApplicationAssessment $nvidiaSmiPath
$nvidiaSmiPassed = $nvidiaSmiAssessment.passed
$nvidiaSmiDetail = $nvidiaSmiAssessment.detail
$gpuChecks.Add((Get-CheckRecord "gpu.nvidia-smi" `
    $nvidiaSmiPassed $nvidiaSmiDetail))

$gpuNames = New-Object System.Collections.Generic.List[string]
$gpuDriverVersions = New-Object System.Collections.Generic.List[string]
try {
    $videoControllers = @(Get-CimInstance -ClassName Win32_VideoController `
        -ErrorAction Stop | Where-Object {
            $_.Name -match "(?i)NVIDIA"
        })
    foreach ($controller in $videoControllers) {
        if (-not [string]::IsNullOrWhiteSpace([string]$controller.Name)) {
            $gpuNames.Add(([string]$controller.Name).Trim())
        }
        if (-not [string]::IsNullOrWhiteSpace(
                [string]$controller.DriverVersion)) {
            $gpuDriverVersions.Add(
                ([string]$controller.DriverVersion).Trim())
        }
    }
}
catch {
    $videoControllers = @()
}
$gpuDevicePassed = $gpuNames.Count -gt 0 -and
    @($gpuNames | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -eq 0
$gpuDeviceDetail = if ($gpuDevicePassed) {
    "devices=" + ($gpuNames -join "; ")
}
else {
    "nvidia-smi returned no parseable GPU device name"
}
$gpuChecks.Add((Get-CheckRecord "gpu.device" $gpuDevicePassed $gpuDeviceDetail))

$gpuDriverPassed = $gpuDriverVersions.Count -eq $gpuNames.Count -and
    $gpuDriverVersions.Count -gt 0 -and
    @($gpuDriverVersions |
        Where-Object { $_ -notmatch "^\d+(?:\.\d+)+$" }).Count -eq 0
$gpuDriverDetail = if ($gpuDriverPassed) {
    "driverVersions=" +
        (@($gpuDriverVersions | Select-Object -Unique) -join "; ")
}
else {
    "nvidia-smi returned no valid driver version for every GPU"
}
$gpuChecks.Add((Get-CheckRecord "gpu.driver" $gpuDriverPassed $gpuDriverDetail))

$cudaRoot = Get-EnvironmentValue "CIGVISION_CUDA_ROOT"
if ([string]::IsNullOrWhiteSpace($cudaRoot)) {
    $cudaRoot = Get-EnvironmentValue "CUDA_PATH"
}
$cudaPaths = if ([string]::IsNullOrWhiteSpace($cudaRoot)) {
    @()
}
else {
    @(
        (Join-Path $cudaRoot "include\cuda_runtime_api.h"),
        (Join-Path $cudaRoot "lib\x64\cudart.lib"),
        (Join-Path $cudaRoot "bin\x64\cudart64_13.dll")
    )
}
$cudaAssessment = Get-DependencyAssessment $cudaRoot $cudaPaths `
    $projectText @(
        '$(CIGVISION_CUDA_ROOT)\include',
        '$(CIGVISION_CUDA_ROOT)\lib\x64',
        "cudart.lib",
        '$(CIGVISION_CUDA_ROOT)\bin\x64\cudart64_13.dll')
$gpuChecks.Add((Get-CheckRecord "dependency.cuda" `
    $cudaAssessment.passed $cudaAssessment.detail))

$tensorRtRoot = Get-EnvironmentValue "CIGVISION_TENSORRT_ROOT"
if ([string]::IsNullOrWhiteSpace($tensorRtRoot)) {
    $tensorRtRoot = Get-EnvironmentValue "TENSORRT_PATH"
}
$tensorRtPaths = if ([string]::IsNullOrWhiteSpace($tensorRtRoot)) {
    @()
}
else {
    @(
        (Join-Path $tensorRtRoot "include\NvInfer.h"),
        (Join-Path $tensorRtRoot "lib\nvinfer_10.lib"),
        (Join-Path $tensorRtRoot "bin\nvinfer_10.dll")
    )
}
$tensorRtAssessment = Get-DependencyAssessment $tensorRtRoot `
    $tensorRtPaths $projectText @(
        '$(CIGVISION_TENSORRT_ROOT)\include',
        '$(CIGVISION_TENSORRT_ROOT)\lib',
        "nvinfer_10.lib",
        '$(CIGVISION_TENSORRT_ROOT)\bin\nvinfer_10.dll')
$gpuChecks.Add((Get-CheckRecord "dependency.tensorrt" `
    $tensorRtAssessment.passed $tensorRtAssessment.detail))

$openCvRoot = Get-EnvironmentValue "CIGVISION_OPENCV_ROOT"
if ([string]::IsNullOrWhiteSpace($openCvRoot)) {
    $openCvPath = Get-EnvironmentValue "OPENCV_PATH"
    if (-not [string]::IsNullOrWhiteSpace($openCvPath)) {
        $openCvRoot = Join-Path $openCvPath "build"
    }
}
$openCvPaths = if ([string]::IsNullOrWhiteSpace($openCvRoot)) {
    @()
}
else {
    @(
        (Join-Path $openCvRoot "include\opencv2\imgproc.hpp"),
        (Join-Path $openCvRoot "x64\vc16\lib\opencv_world490.lib"),
        (Join-Path $openCvRoot "x64\vc16\bin\opencv_world490.dll")
    )
}
$openCvAssessment = Get-DependencyAssessment $openCvRoot $openCvPaths `
    $projectText @(
        '$(CIGVISION_OPENCV_ROOT)\include',
        '$(CIGVISION_OPENCV_ROOT)\x64\vc16\lib',
        "opencv_world490.lib",
        'opencv_world490$(CIGVISION_OPENCV_SUFFIX).dll')
$gpuChecks.Add((Get-CheckRecord "dependency.opencv" `
    $openCvAssessment.passed $openCvAssessment.detail))

$failedWindowsChecks = @($windowsChecks |
    Where-Object { $_.status -ne "passed" })
$failedGpuChecks = @($gpuChecks |
    Where-Object { $_.status -ne "passed" })
$windowsCollectionComplete = $windowsChecks.Count -eq 12 -and
    $failedWindowsChecks.Count -eq 0
$gpuCollectionComplete = $gpuChecks.Count -eq 6 -and
    $failedGpuChecks.Count -eq 0

$collectorBinding = [ordered]@{
    schemaVersion = $collectorSchemaVersion
    repositoryPath = $collectorRepositoryPath
    sha256 = $collectorSha256
}
$windowsReport = [ordered]@{
    schemaVersion = "p8-windows-host-report-v2"
    captureId = $captureId
    capturedAtUtc = $capturedAtUtc
    hostIdSha256 = $hostIdSha256
    challenge = $Challenge
    packageManifestSha256 = $PackageManifestSha256
    collector = $collectorBinding
    checks = @($windowsChecks)
    claims = [ordered]@{
        collectionComplete = $windowsCollectionComplete
        productAcceptance = $false
        windowsRuntimeAccepted = $false
        gpuRuntimeAccepted = $false
        realIoTested = $false
        realRejectTested = $false
    }
}
$gpuReport = [ordered]@{
    schemaVersion = "p8-gpu-host-report-v2"
    captureId = $captureId
    capturedAtUtc = $capturedAtUtc
    hostIdSha256 = $hostIdSha256
    challenge = $Challenge
    packageManifestSha256 = $PackageManifestSha256
    collector = $collectorBinding
    checks = @($gpuChecks)
    claims = [ordered]@{
        collectionComplete = $gpuCollectionComplete
        productAcceptance = $false
        windowsRuntimeAccepted = $false
        gpuRuntimeAccepted = $false
        realIoTested = $false
        realRejectTested = $false
    }
}

$windowsReportPath = Join-Path $resolvedOutputDirectory "windows.json"
$gpuReportPath = Join-Path $resolvedOutputDirectory "gpu.json"
try {
    Assert-OwnedOutputDirectory $resolvedOutputDirectory `
        $ownershipMarkerPath $Challenge
    Write-Utf8AtomicNoOverwrite $windowsReportPath (
        $windowsReport | ConvertTo-Json -Depth 8)
    Assert-OwnedOutputDirectory $resolvedOutputDirectory `
        $ownershipMarkerPath $Challenge
    Write-Utf8AtomicNoOverwrite $gpuReportPath (
        $gpuReport | ConvertTo-Json -Depth 8)
}
catch {
    Write-RejectionAndExit "report.write-failed" $_.Exception.Message
}
Assert-OwnedOutputDirectory $resolvedOutputDirectory `
    $ownershipMarkerPath $Challenge

$allChecksPassed = $windowsCollectionComplete -and $gpuCollectionComplete
$summaryExitCode = if ($allChecksPassed) { 0 } else { 1 }
$summary = [ordered]@{
    schemaVersion = "cigvision-p8-host-collector-summary-v1"
    status = if ($allChecksPassed) {
        "collection-complete"
    }
    else {
        "collection-incomplete"
    }
    exitCode = $summaryExitCode
    captureId = $captureId
    capturedAtUtc = $capturedAtUtc
    hostIdSha256 = $hostIdSha256
    challenge = $Challenge
    packageManifestSha256 = $PackageManifestSha256
    outputDirectory = $resolvedOutputDirectory
    reports = [ordered]@{
        windows = [ordered]@{
            path = "windows.json"
            sha256 = (Get-FileHash -LiteralPath $windowsReportPath `
                -Algorithm SHA256).Hash.ToLowerInvariant()
            collectionComplete = $windowsCollectionComplete
            failedCheckIds = @($failedWindowsChecks |
                ForEach-Object { $_.id })
        }
        gpu = [ordered]@{
            path = "gpu.json"
            sha256 = (Get-FileHash -LiteralPath $gpuReportPath `
                -Algorithm SHA256).Hash.ToLowerInvariant()
            collectionComplete = $gpuCollectionComplete
            failedCheckIds = @($failedGpuChecks |
                ForEach-Object { $_.id })
        }
    }
}
Write-Output ($summary | ConvertTo-Json -Compress -Depth 6)
exit $summaryExitCode
