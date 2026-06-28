param(
    [string] $BuildRoot = "",
    [string] $Config = "Release",
    [string] $Renderer = "",
    [string] $Python = "python",
    [string] $WorkDir = "",
    [int] $MinPresets = 150,
    [int] $MinPerChip = 10,
    [double] $Seconds = 0.05,
    [double] $MinPeak = 0.005,
    [double] $MaxPeak = 0.98
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $candidateBuildRoots = @("build", "build-codex") |
        ForEach-Object { Join-Path $repoRoot $_ } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container }

    $BuildRoot = $candidateBuildRoots | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
        $BuildRoot = Join-Path $repoRoot "build"
    }
}

$buildPath = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot
} else {
    Join-Path $repoRoot $BuildRoot
}

if ([string]::IsNullOrWhiteSpace($Renderer)) {
    $candidateRenderers = @(
        (Join-Path $buildPath "$Config\chipper_render.exe"),
        (Join-Path $buildPath "$Config\chipper_render"),
        (Join-Path $buildPath "chipper_render.exe"),
        (Join-Path $buildPath "chipper_render"),
        (Join-Path $buildPath "Release\chipper_render.exe"),
        (Join-Path $buildPath "Release\chipper_render")
    )

    $Renderer = $candidateRenderers |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($Renderer) -or -not (Test-Path -LiteralPath $Renderer -PathType Leaf)) {
    throw "Renderer not found under $buildPath. Build it first with: cmake --build $BuildRoot --config $Config --target chipper_render, or pass -Renderer explicitly."
}

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    $WorkDir = Join-Path $buildPath "preset-qa"
}

$catalogPath = Join-Path $buildPath "preset-catalog.json"

Write-Host "Listing factory presets with $Renderer"
& $Renderer --list-presets --debug $catalogPath
if ($LASTEXITCODE -ne 0) {
    throw "Preset catalog export failed with exit code $LASTEXITCODE"
}

Write-Host "Validating preset catalog metadata"
& $Python (Join-Path $repoRoot "tests\assert_preset_catalog_json.py") $catalogPath --min-presets $MinPresets --min-per-chip $MinPerChip
if ($LASTEXITCODE -ne 0) {
    throw "Preset catalog validation failed with exit code $LASTEXITCODE"
}

$catalog = Get-Content -LiteralPath $catalogPath -Raw | ConvertFrom-Json
Write-Host ("Factory presets: {0}" -f $catalog.summary.totalPresetCount)
foreach ($chip in $catalog.summary.chipCounts) {
    Write-Host ("  {0,-28} {1,3}" -f $chip.chip, $chip.presetCount)
}

Write-Host "Rendering every factory preset for audibility"
& $Python (Join-Path $repoRoot "tests\assert_factory_presets_audible.py") `
    --renderer $Renderer `
    --work-dir $WorkDir `
    --seconds $Seconds `
    --min-peak $MinPeak `
    --max-peak $MaxPeak
if ($LASTEXITCODE -ne 0) {
    throw "Preset audibility validation failed with exit code $LASTEXITCODE"
}

Write-Host "Preset QA passed. Catalog: $catalogPath"
Write-Host "Preset QA artifacts: $WorkDir"
