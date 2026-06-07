param(
    [string] $Configuration = "Release",
    [string] $BuildRoot = "build",
    [ValidateSet("Global", "User")]
    [string] $Scope = "Global",
    [string] $Destination,
    [string] $FallbackDestination,
    [switch] $FailOnFallback
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRootPath = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot
} else {
    Join-Path $repoRoot $BuildRoot
}
$source = Join-Path $buildRootPath "Chipper_artefacts\$Configuration\VST3\Chipper.vst3"

if (-not (Test-Path -LiteralPath $source)) {
    throw "VST3 bundle not found: $source"
}

function New-ChipperFallbackDestination {
    if ($PSBoundParameters.ContainsKey("FallbackDestination") -and -not [string]::IsNullOrWhiteSpace($FallbackDestination)) {
        return $FallbackDestination
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    return Join-Path $env:TEMP "Chipper-VST3-install-$stamp"
}

function Copy-ChipperBundle {
    param(
        [string] $SourceBundle,
        [string] $DestinationRoot,
        [string] $MessagePrefix = "Installed"
    )

    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    Copy-Item -Recurse -Force -LiteralPath $SourceBundle -Destination $DestinationRoot
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

    if ($Scope -ne "User" -or -not (Test-Path -LiteralPath $PathToRepair)) {
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

if (-not $PSBoundParameters.ContainsKey("Destination") -or [string]::IsNullOrWhiteSpace($Destination)) {
    if ($Scope -eq "User") {
        $Destination = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
    } else {
        $Destination = "C:\Program Files\Common Files\VST3"
    }
}

function Test-ProcessElevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ($Scope -eq "Global" -and -not (Test-ProcessElevated)) {
    throw "Global VST3 install requires an elevated PowerShell. Use -Scope User for a no-UAC install to %LOCALAPPDATA%\Programs\Common\VST3."
}

try {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
} catch {
    Copy-ChipperBundleToFallback -Reason "Could not create or access requested VST3 destination '$Destination': $($_.Exception.Message)"
    return
}

$destinationRoot = (Resolve-Path -LiteralPath $Destination).ProviderPath
$target = Join-Path $destinationRoot "Chipper.vst3"
$targetFullPath = [System.IO.Path]::GetFullPath($target)
$destinationFullPath = [System.IO.Path]::GetFullPath($destinationRoot)
$destinationPrefix = $destinationFullPath.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar,
    [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
$targetBinary = Join-Path $targetFullPath "Contents\x86_64-win\Chipper.vst3"

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

if (-not $targetFullPath.StartsWith($destinationPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    (Split-Path -Leaf $targetFullPath) -ne "Chipper.vst3") {
    throw "Refusing to remove unexpected VST3 target: $targetFullPath"
}

if (Test-Path -LiteralPath $targetFullPath) {
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
        Copy-ChipperBundleToFallback -Reason "Could not remove existing Chipper.vst3 at '$targetFullPath'."
        return
    }
}

if (-not (Copy-ChipperBundleWithRetry -SourceBundle $source -DestinationRoot $destinationRoot)) {
    Copy-ChipperBundleToFallback -Reason "Could not copy Chipper.vst3 to '$destinationRoot'."
    return
}
