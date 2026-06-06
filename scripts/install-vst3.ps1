param(
    [string] $Configuration = "Release",
    [string] $BuildRoot = "build",
    [ValidateSet("Global", "User")]
    [string] $Scope = "Global",
    [string] $Destination
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

New-Item -ItemType Directory -Force -Path $Destination | Out-Null

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
    Remove-Item -Recurse -Force -LiteralPath $targetFullPath
}

Copy-Item -Recurse -Force -LiteralPath $source -Destination $destinationRoot

Write-Host "Installed Chipper.vst3 to $destinationRoot"
