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

$destinationRoot = (Resolve-Path -LiteralPath $Destination).ProviderPath
$target = Join-Path $destinationRoot "Chipper.vst3"
$targetFullPath = [System.IO.Path]::GetFullPath($target)
$destinationFullPath = [System.IO.Path]::GetFullPath($destinationRoot)
$destinationPrefix = $destinationFullPath.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar,
    [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not $targetFullPath.StartsWith($destinationPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    (Split-Path -Leaf $targetFullPath) -ne "Chipper.vst3") {
    throw "Refusing to remove unexpected VST3 target: $targetFullPath"
}

if (Test-Path -LiteralPath $targetFullPath) {
    Write-Host "Removing existing Chipper.vst3 from $destinationRoot"
    Remove-Item -Recurse -Force -LiteralPath $targetFullPath
}

Copy-Item -Recurse -Force -LiteralPath $source -Destination $destinationRoot

Write-Host "Installed Chipper.vst3 to $destinationRoot"
