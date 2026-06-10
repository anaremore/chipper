param(
    [string] $Configuration = "Release",
    [string] $BuildRoot = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_BUILD_ROOT)) { "" } else { $env:CHIPPER_BUILD_ROOT }),
    [ValidateSet("Global", "User", "Both")]
    [string] $Scope = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_INSTALL_SCOPE)) { "User" } else { $env:CHIPPER_INSTALL_SCOPE }),
    [string] $Destination = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_DESTINATION)) { "" } else { $env:CHIPPER_VST3_DESTINATION }),
    [string] $FallbackDestination = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_VST3_FALLBACK_DIR)) { "" } else { $env:CHIPPER_VST3_FALLBACK_DIR }),
    [switch] $AllowFallback = $(if ([string]::IsNullOrWhiteSpace($env:CHIPPER_ALLOW_FALLBACK_INSTALL)) { $false } else { $env:CHIPPER_ALLOW_FALLBACK_INSTALL -match '^(1|true|yes)$' })
)

$ErrorActionPreference = "Stop"

$installer = Join-Path $PSScriptRoot "scripts\install-vst3.ps1"
if (-not (Test-Path -LiteralPath $installer)) {
    throw "Installer not found: $installer"
}

$arguments = @{
    Configuration = $Configuration
    BuildRoot = $BuildRoot
    Scope = $Scope
}

if (-not [string]::IsNullOrWhiteSpace($Destination)) {
    $arguments.Destination = $Destination
}

if (-not [string]::IsNullOrWhiteSpace($FallbackDestination)) {
    $arguments.FallbackDestination = $FallbackDestination
}

if (-not $AllowFallback) {
    $arguments.FailOnFallback = $true
}

& $installer @arguments
