param(
    [string]$BuildRoot = "build-codex",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Configuration = "Release",
    [string]$OutputDirectory = "ui-captures",
    [string]$Chip = "all",
    [ValidateSet("1180", "1240", "both")]
    [string]$Width = "both",
    [ValidateSet("editor", "edit", "browser", "all")]
    [string]$Workspace = "editor",
    [switch]$ManifestOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildRoot))
$resolvedOutputDirectory = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))

cmake --build $resolvedBuildRoot --config $Configuration --target chipper_ui_snapshot
if ($LASTEXITCODE -ne 0) {
    throw "chipper_ui_snapshot build failed with exit code $LASTEXITCODE"
}

$isWindowsHost = $IsWindows -or $env:OS -eq "Windows_NT"
$executableName = if ($isWindowsHost) { "chipper_ui_snapshot.exe" } else { "chipper_ui_snapshot" }
$executableCandidates = @(
    (Join-Path $resolvedBuildRoot "$Configuration/$executableName"),
    (Join-Path $resolvedBuildRoot $executableName)
)
$snapshotExecutable = $executableCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1

if (-not $snapshotExecutable) {
    throw "Snapshot executable not found. Checked: $($executableCandidates -join ', ')"
}

$arguments = @(
    "--output", $resolvedOutputDirectory,
    "--chip", $Chip,
    "--width", $Width,
    "--workspace", $Workspace
)
if ($ManifestOnly) {
    $arguments += "--manifest-only"
}

& $snapshotExecutable @arguments
if ($LASTEXITCODE -ne 0) {
    throw "chipper_ui_snapshot failed with exit code $LASTEXITCODE"
}

Write-Host "UI captures written to $resolvedOutputDirectory"
