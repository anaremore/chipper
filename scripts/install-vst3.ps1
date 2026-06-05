param(
    [string] $Configuration = "Release",
    [string] $Destination = "C:\Program Files\Common Files\VST3"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot "build\Chipper_artefacts\$Configuration\VST3\Chipper.vst3"

if (-not (Test-Path -LiteralPath $source)) {
    throw "VST3 bundle not found: $source"
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item -Recurse -Force -LiteralPath $source -Destination $Destination

Write-Host "Installed Chipper.vst3 to $Destination"
