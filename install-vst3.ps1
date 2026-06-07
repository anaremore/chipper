param(
    [string] $Configuration = "Release",
    [string] $BuildRoot = "build-codex",
    [ValidateSet("Global", "User")]
    [string] $Scope = "User",
    [string] $Destination,
    [string] $FallbackDestination,
    [switch] $AllowFallback
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
