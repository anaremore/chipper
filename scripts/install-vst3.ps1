param(
    [string] $Configuration = "Release",
    [string] $BuildRoot = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_BUILD_ROOT)) { "" } else { $env:CHIPPER_BUILD_ROOT }),
    [ValidateSet("Global", "User", "Both")]
    [string] $Scope = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_INSTALL_SCOPE)) { "User" } else { $env:CHIPPER_INSTALL_SCOPE }),
    [string] $Destination = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_DESTINATION)) { "" } else { $env:CHIPPER_VST3_DESTINATION }),
    [string] $FallbackDestination = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_FALLBACK_DIR)) { "" } else { $env:CHIPPER_VST3_FALLBACK_DIR }),
    [switch] $FailOnFallback = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_ALLOW_FALLBACK_INSTALL)) { $true } else { -not ($env:CHIPPER_ALLOW_FALLBACK_INSTALL -match '^(1|true|yes)$') })
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-ChipperBundleBinaryPath {
    param([string] $CandidateBuildRoot)

    $candidateRootPath = if ([System.IO.Path]::IsPathRooted($CandidateBuildRoot)) {
        $CandidateBuildRoot
    } else {
        Join-Path $repoRoot $CandidateBuildRoot
    }

    return Join-Path $candidateRootPath "Chipper_artefacts\$Configuration\VST3\Chipper.vst3\Contents\x86_64-win\Chipper.vst3"
}

function Resolve-ChipperBuildRoot {
    param([string] $RequestedBuildRoot)

    if (-not [string]::IsNullOrWhiteSpace($RequestedBuildRoot)) {
        return $RequestedBuildRoot
    }

    $candidates = @("build-codex", "build")
    $available = @()
    foreach ($candidate in $candidates) {
        $binaryPath = Get-ChipperBundleBinaryPath -CandidateBuildRoot $candidate
        if (Test-Path -LiteralPath $binaryPath) {
            $binary = Get-Item -LiteralPath $binaryPath
            $available += [pscustomobject]@{
                BuildRoot = $candidate
                LastWriteTimeUtc = $binary.LastWriteTimeUtc
            }
        }
    }

    if ($available.Count -eq 0) {
        return "build"
    }

    $selected = $available | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    Write-Host "Auto-selected build root: $($selected.BuildRoot)"
    return $selected.BuildRoot
}

$BuildRoot = Resolve-ChipperBuildRoot -RequestedBuildRoot $BuildRoot
$buildRootPath = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot
} else {
    Join-Path $repoRoot $BuildRoot
}
$source = Join-Path $buildRootPath "Chipper_artefacts\$Configuration\VST3\Chipper.vst3"

if (-not (Test-Path -LiteralPath $source)) {
    throw "VST3 bundle not found: $source"
}

function Get-ChipperBuildInfo {
    $headerPath = Join-Path $buildRootPath "generated\ChipperBuildInfo.h"
    $info = [ordered]@{
        Label = "unknown"
        Version = "unknown"
        GitSha = "unknown"
        GitState = "unknown"
        BuiltAtUtc = "unknown"
    }

    if (-not (Test-Path -LiteralPath $headerPath)) {
        return [pscustomobject]$info
    }

    foreach ($line in Get-Content -LiteralPath $headerPath) {
        if ($line -match 'inline constexpr auto ([A-Za-z]+) = "([^"]*)";') {
            switch ($matches[1]) {
                "version" { $info.Version = $matches[2] }
                "gitSha" { $info.GitSha = $matches[2] }
                "gitState" { $info.GitState = $matches[2] }
                "builtAtUtc" { $info.BuiltAtUtc = $matches[2] }
                "label" { $info.Label = $matches[2] }
            }
        }
    }

    return [pscustomobject]$info
}

function Get-ChipperBundleBinaryPath {
    param([string] $BundlePath)

    return Join-Path $BundlePath "Contents\x86_64-win\Chipper.vst3"
}

function Write-ChipperBundleReport {
    param(
        [string] $Label,
        [string] $BundlePath
    )

    $binaryPath = Get-ChipperBundleBinaryPath -BundlePath $BundlePath
    if (-not (Test-Path -LiteralPath $binaryPath)) {
        Write-Host "$Label binary: not found at $binaryPath"
        return
    }

    $binary = Get-Item -LiteralPath $binaryPath
    Write-Host ("{0} binary: {1} bytes, modified {2}" -f $Label, $binary.Length, $binary.LastWriteTime)
}

function Get-ChipperDefaultVst3Root {
    param([string] $InstallScope)

    if ($InstallScope -eq "User") {
        if (-not [string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_USER_DIR)) {
            return $env:CHIPPER_VST3_USER_DIR
        }

        return Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_GLOBAL_DIR)) {
        return $env:CHIPPER_VST3_GLOBAL_DIR
    }

    if (-not [string]::IsNullOrWhiteSpace($env:ProgramFiles)) {
        return Join-Path $env:ProgramFiles "Common Files\VST3"
    }

    return "C:\Program Files\Common Files\VST3"
}

function Test-ChipperSamePath {
    param(
        [string] $Left,
        [string] $Right
    )

    $leftFullPath = [System.IO.Path]::GetFullPath($Left)
    $rightFullPath = [System.IO.Path]::GetFullPath($Right)
    return [System.String]::Equals($leftFullPath, $rightFullPath, [System.StringComparison]::OrdinalIgnoreCase)
}

function Show-ChipperOtherScopeInstallWarning {
    param(
        [string] $InstalledBundlePath,
        [string] $InstalledScope
    )

    $otherScope = if ($InstalledScope -eq "User") { "Global" } else { "User" }
    $otherRoot = Get-ChipperDefaultVst3Root -InstallScope $otherScope
    $otherBundle = Join-Path $otherRoot "Chipper.vst3"

    if ((Test-ChipperSamePath -Left $InstalledBundlePath -Right $otherBundle) -or
        -not (Test-Path -LiteralPath $otherBundle)) {
        return
    }

    Write-Warning "Another Chipper.vst3 exists in the $otherScope VST3 folder:"
    Write-ChipperBundleReport -Label "$otherScope install" -BundlePath $otherBundle
    Write-Warning "Some hosts scan the global VST3 folder before the user VST3 folder, so this can look like an old build is still installed."

    $buildRootSuggestion = if ([string]::IsNullOrWhiteSpace($BuildRoot)) { "build" } else { $BuildRoot }
    if ($otherScope -eq "Global") {
        Write-Warning "To keep both copies in sync, open PowerShell as Administrator and run: .\install-vst3.ps1 -Scope Both -BuildRoot $buildRootSuggestion"
    } else {
        Write-Warning "To keep both copies in sync, open PowerShell as Administrator and run: .\install-vst3.ps1 -Scope Both -BuildRoot $buildRootSuggestion"
    }
}

function Write-ChipperInstalledBuildMarker {
    param(
        [string] $BundlePath,
        [pscustomobject] $BuildInfo
    )

    $markerPath = Join-Path $BundlePath "ChipperBuildInfo.txt"
    $binaryPath = Get-ChipperBundleBinaryPath -BundlePath $BundlePath
    $binary = if (Test-Path -LiteralPath $binaryPath) { Get-Item -LiteralPath $binaryPath } else { $null }
    $lines = @(
        "Name: Chipper",
        "Build: $($BuildInfo.Label)",
        "Version: $($BuildInfo.Version)",
        "Git SHA: $($BuildInfo.GitSha)",
        "Git State: $($BuildInfo.GitState)",
        "Built UTC: $($BuildInfo.BuiltAtUtc)",
        "Installed: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')"
    )

    if ($null -ne $binary) {
        $lines += "Binary Modified: $($binary.LastWriteTime)"
        $lines += "Binary Bytes: $($binary.Length)"
    }

    Set-Content -LiteralPath $markerPath -Value $lines
}

$buildInfo = Get-ChipperBuildInfo
$activeInstallScope = $Scope
Write-Host "Source build: $($buildInfo.Label) ($($buildInfo.GitState), built $($buildInfo.BuiltAtUtc))"
Write-ChipperBundleReport -Label "Source" -BundlePath $source

function New-ChipperFallbackDestination {
    if ($PSBoundParameters.ContainsKey("FallbackDestination") -and -not [string]::IsNullOrWhiteSpace($FallbackDestination)) {
        return $FallbackDestination
    }

    if (-not [string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_FALLBACK_DIR)) {
        return $env:CHIPPER_VST3_FALLBACK_DIR
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    return Join-Path $env:TEMP "Chipper-VST3-install-$stamp"
}

function Copy-ChipperLegalNotices {
    param(
        [string] $BundlePath
    )

    $resourcesPath = Join-Path $BundlePath "Contents\Resources"
    $legalPath = Join-Path $resourcesPath "Legal"
    New-Item -ItemType Directory -Force -Path $legalPath | Out-Null

    $noticesSource = Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"
    if (Test-Path -LiteralPath $noticesSource) {
        Copy-Item -Force -LiteralPath $noticesSource -Destination (Join-Path $legalPath "THIRD_PARTY_NOTICES.md")
    }

    $projectLicenseSource = Join-Path $repoRoot "LICENSE"
    if (Test-Path -LiteralPath $projectLicenseSource) {
        Copy-Item -Force -LiteralPath $projectLicenseSource -Destination (Join-Path $legalPath "LICENSE")
    }

    $emu2413License = Join-Path $repoRoot "ThirdParty\emu2413\LICENSE"
    if (Test-Path -LiteralPath $emu2413License) {
        $emu2413Path = Join-Path $legalPath "emu2413"
        New-Item -ItemType Directory -Force -Path $emu2413Path | Out-Null
        Copy-Item -Force -LiteralPath $emu2413License -Destination (Join-Path $emu2413Path "LICENSE")
    }

    $emu76489License = Join-Path $repoRoot "ThirdParty\emu76489\LICENSE"
    if (Test-Path -LiteralPath $emu76489License) {
        $emu76489Path = Join-Path $legalPath "emu76489"
        New-Item -ItemType Directory -Force -Path $emu76489Path | Out-Null
        Copy-Item -Force -LiteralPath $emu76489License -Destination (Join-Path $emu76489Path "LICENSE")
    }

    $emu2149License = Join-Path $repoRoot "ThirdParty\emu2149\LICENSE"
    if (Test-Path -LiteralPath $emu2149License) {
        $emu2149Path = Join-Path $legalPath "emu2149"
        New-Item -ItemType Directory -Force -Path $emu2149Path | Out-Null
        Copy-Item -Force -LiteralPath $emu2149License -Destination (Join-Path $emu2149Path "LICENSE")
    }

    $emu2212License = Join-Path $repoRoot "ThirdParty\emu2212\LICENSE"
    if (Test-Path -LiteralPath $emu2212License) {
        $emu2212Path = Join-Path $legalPath "emu2212"
        New-Item -ItemType Directory -Force -Path $emu2212Path | Out-Null
        Copy-Item -Force -LiteralPath $emu2212License -Destination (Join-Path $emu2212Path "LICENSE")
    }

    $ymfmLicense = Join-Path $repoRoot "ThirdParty\ymfm\LICENSE"
    if (Test-Path -LiteralPath $ymfmLicense) {
        $ymfmPath = Join-Path $legalPath "ymfm"
        New-Item -ItemType Directory -Force -Path $ymfmPath | Out-Null
        Copy-Item -Force -LiteralPath $ymfmLicense -Destination (Join-Path $ymfmPath "LICENSE")
    }
}

function Copy-ChipperBundle {
    param(
        [string] $SourceBundle,
        [string] $DestinationRoot,
        [string] $MessagePrefix = "Installed"
    )

    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    Copy-Item -Recurse -Force -LiteralPath $SourceBundle -Destination $DestinationRoot
    Copy-ChipperLegalNotices -BundlePath (Join-Path $DestinationRoot "Chipper.vst3")
    Write-Host "$MessagePrefix Chipper.vst3 to $DestinationRoot"
}

function Copy-ChipperBundleToFallback {
    param(
        [string] $Reason
    )

    if ($FailOnFallback) {
        throw "$Reason No fallback install was written because -FailOnFallback was set."
    }

    $fallbackRoot = New-ChipperFallbackDestination
    Write-Warning "$Reason"
    Write-Warning "Falling back to a writable VST3 bundle folder: $fallbackRoot"
    Copy-ChipperBundle -SourceBundle $source -DestinationRoot $fallbackRoot -MessagePrefix "Installed fallback"
    Write-Warning "Add this folder to your host's VST3 scan paths, or fix permissions on the requested VST3 folder and rerun the installer."
}

function Grant-ChipperCurrentUserAccess {
    param(
        [string] $PathToRepair
    )

    if ($activeInstallScope -ne "User" -or -not (Test-Path -LiteralPath $PathToRepair)) {
        return $false
    }

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    Write-Host "Repairing user permissions for $PathToRepair"
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $icaclsOutput = & icacls $PathToRepair /grant "$($identity):(OI)(CI)F" /T /C 2>&1
        $icaclsExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if ($icaclsExitCode -ne 0) {
        Write-Warning ($icaclsOutput -join [Environment]::NewLine)
        return $false
    }

    return $true
}

function Remove-ChipperBundleWithRetry {
    param(
        [string] $BundlePath
    )

    try {
        Remove-Item -Recurse -Force -LiteralPath $BundlePath
        return $true
    } catch {
        $firstError = $_.Exception.Message
        if (Grant-ChipperCurrentUserAccess -PathToRepair $BundlePath) {
            try {
                Remove-Item -Recurse -Force -LiteralPath $BundlePath
                return $true
            } catch {
                Write-Warning "Permission repair completed, but removal still failed: $($_.Exception.Message)"
            }
        }

        Write-Warning "Could not remove existing Chipper.vst3 at '$BundlePath': $firstError"
        return $false
    }
}

function Get-ChipperLikelyHostProcesses {
    $hostNamePatterns = @(
        "*Ableton*",
        "*Live*",
        "*REAPER*",
        "*Bitwig*",
        "*FL*",
        "*Cubase*",
        "*Studio One*",
        "*Nuendo*",
        "*Cakewalk*",
        "*Waveform*"
    )

    $matches = @()
    foreach ($process in Get-Process) {
        foreach ($pattern in $hostNamePatterns) {
            if ($process.ProcessName -like $pattern) {
                $matches += [pscustomobject]@{
                    Id = $process.Id
                    Name = $process.ProcessName
                    Path = $process.Path
                }
                break
            }
        }
    }

    return $matches | Sort-Object Name, Id
}

function Show-ChipperInstallDiagnostics {
    param([string] $Reason)

    Write-Warning $Reason
    $likelyHosts = @(Get-ChipperLikelyHostProcesses)
    if ($likelyHosts.Count -eq 0) {
        Write-Warning "No common DAW/plugin host process names were detected, but a host or scanner may still be holding the VST3 bundle."
        return
    }

    Write-Warning "Close these likely host/scanner processes, then rerun the installer:"
    foreach ($hostProcess in $likelyHosts) {
        Write-Warning ("  PID {0}: {1} ({2})" -f $hostProcess.Id, $hostProcess.Name, $hostProcess.Path)
    }
}

function Copy-ChipperBundleWithRetry {
    param(
        [string] $SourceBundle,
        [string] $DestinationRoot
    )

    try {
        Copy-ChipperBundle -SourceBundle $SourceBundle -DestinationRoot $DestinationRoot
        return $true
    } catch {
        $firstError = $_.Exception.Message
        $destinationBundle = Join-Path $DestinationRoot "Chipper.vst3"
        if (Grant-ChipperCurrentUserAccess -PathToRepair $destinationBundle) {
            try {
                Copy-ChipperBundle -SourceBundle $SourceBundle -DestinationRoot $DestinationRoot
                return $true
            } catch {
                Write-Warning "Permission repair completed, but copy still failed: $($_.Exception.Message)"
            }
        }

        Write-Warning "Could not copy Chipper.vst3 to '$DestinationRoot': $firstError"
        return $false
    }
}

function Test-ProcessElevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-ChipperModuleHolders {
    param([string] $PluginBinary)

    if (-not (Test-Path -LiteralPath $PluginBinary)) {
        return @()
    }

    $pluginFullPath = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $PluginBinary).ProviderPath)
    $holders = @()
    foreach ($process in Get-Process) {
        try {
            foreach ($module in $process.Modules) {
                if ([System.String]::Equals($module.FileName, $pluginFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
                    $holders += [pscustomobject]@{
                        Id = $process.Id
                        Name = $process.ProcessName
                        Path = $process.Path
                    }
                    break
                }
            }
        } catch {
            # Some system processes do not allow module inspection.
        }
    }

    return $holders
}

function Install-ChipperBundleToDestination {
    param(
        [string] $InstallScope,
        [string] $InstallDestination,
        [bool] $WarnAboutOtherScope = $true
    )

    $script:activeInstallScope = $InstallScope

    try {
        New-Item -ItemType Directory -Force -Path $InstallDestination | Out-Null
    } catch {
        Copy-ChipperBundleToFallback -Reason "Could not create or access requested VST3 destination '$InstallDestination': $($_.Exception.Message)"
        return
    }

    $destinationRoot = (Resolve-Path -LiteralPath $InstallDestination).ProviderPath
    $target = Join-Path $destinationRoot "Chipper.vst3"
    $targetFullPath = [System.IO.Path]::GetFullPath($target)
    $destinationFullPath = [System.IO.Path]::GetFullPath($destinationRoot)
    $destinationPrefix = $destinationFullPath.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    $targetBinary = Join-Path $targetFullPath "Contents\x86_64-win\Chipper.vst3"

    if (-not $targetFullPath.StartsWith($destinationPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $targetFullPath) -ne "Chipper.vst3") {
        throw "Refusing to remove unexpected VST3 target: $targetFullPath"
    }

    if (Test-Path -LiteralPath $targetFullPath) {
        Write-ChipperBundleReport -Label "Installed before replace" -BundlePath $targetFullPath
        $moduleHolders = @(Get-ChipperModuleHolders -PluginBinary $targetBinary)
        if ($moduleHolders.Count -gt 0) {
            Write-Host "Chipper.vst3 is currently loaded by:"
            foreach ($holder in $moduleHolders) {
                Write-Host ("  PID {0}: {1} ({2})" -f $holder.Id, $holder.Name, $holder.Path)
            }

            throw "Close the host process and rerun the installer so Chipper.vst3 can be replaced safely."
        }

        Write-Host "Removing existing Chipper.vst3 from $destinationRoot"
        if (-not (Remove-ChipperBundleWithRetry -BundlePath $targetFullPath)) {
            Show-ChipperInstallDiagnostics -Reason "The existing Chipper.vst3 could not be removed."
            Copy-ChipperBundleToFallback -Reason "Could not remove existing Chipper.vst3 at '$targetFullPath'."
            return
        }
    }

    if (-not (Copy-ChipperBundleWithRetry -SourceBundle $source -DestinationRoot $destinationRoot)) {
        Show-ChipperInstallDiagnostics -Reason "The new Chipper.vst3 could not be copied into the destination."
        Copy-ChipperBundleToFallback -Reason "Could not copy Chipper.vst3 to '$destinationRoot'."
        return
    }

    Write-ChipperInstalledBuildMarker -BundlePath $targetFullPath -BuildInfo $buildInfo
    Write-ChipperBundleReport -Label "Installed after replace" -BundlePath $targetFullPath
    Write-Host "Installed build marker: $(Join-Path $targetFullPath 'ChipperBuildInfo.txt')"
    Write-Host "Installed legal notices: $(Join-Path $targetFullPath 'Contents\Resources\Legal\THIRD_PARTY_NOTICES.md')"

    if ($WarnAboutOtherScope) {
        Show-ChipperOtherScopeInstallWarning -InstalledBundlePath $targetFullPath -InstalledScope $InstallScope
    }
}

if ($Scope -eq "Both") {
    if ($PSBoundParameters.ContainsKey("Destination") -and -not [string]::IsNullOrWhiteSpace($Destination)) {
        throw "-Destination cannot be combined with -Scope Both. Use -Scope User or -Scope Global for a custom destination."
    }

    if (-not (Test-ProcessElevated)) {
        throw "Both-scope VST3 install requires an elevated PowerShell because the global VST3 folder is under Program Files. Use -Scope User for a no-UAC install."
    }

    Install-ChipperBundleToDestination -InstallScope "User" -InstallDestination (Get-ChipperDefaultVst3Root -InstallScope "User") -WarnAboutOtherScope $false
    Write-Host ""
    Install-ChipperBundleToDestination -InstallScope "Global" -InstallDestination (Get-ChipperDefaultVst3Root -InstallScope "Global") -WarnAboutOtherScope $false
    Write-Host "Installed Chipper.vst3 to both User and Global VST3 folders."
    return
}

if (-not $PSBoundParameters.ContainsKey("Destination") -or [string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Get-ChipperDefaultVst3Root -InstallScope $Scope
}

if ($Scope -eq "Global" -and -not (Test-ProcessElevated)) {
    throw "Global VST3 install requires an elevated PowerShell. Use -Scope User, or set CHIPPER_INSTALL_SCOPE=User, for a no-UAC install to %LOCALAPPDATA%\Programs\Common\VST3."
}

Install-ChipperBundleToDestination -InstallScope $Scope -InstallDestination $Destination -WarnAboutOtherScope $true
