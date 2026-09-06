param(
    [string] $BuildRoot = "build-check",
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string] $Config = "Release",
    [ValidateRange(1, 64)]
    [int] $Parallel = 2,
    [string] $Generator = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = if ([IO.Path]::IsPathRooted($BuildRoot)) { $BuildRoot } else { Join-Path $repoRoot $BuildRoot }

$configureArguments = @('-S', $repoRoot, '-B', $buildPath, "-DCMAKE_BUILD_TYPE=$Config", '-DCHIPPER_BUILD_TESTS=ON')
if (-not [string]::IsNullOrWhiteSpace($Generator)) { $configureArguments += @('-G', $Generator) }
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed ($LASTEXITCODE)" }
& cmake --build $buildPath --config $Config --parallel $Parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
$testArguments = @('--test-dir', $buildPath, '-C', $Config, '--output-on-failure', '--parallel', $Parallel, '--no-tests=error')
if ($IsLinux -and [string]::IsNullOrWhiteSpace($env:DISPLAY)) {
    if (-not (Get-Command xvfb-run -ErrorAction SilentlyContinue)) { throw 'Install xvfb to run GUI tests on a headless Linux machine.' }
    & xvfb-run -a ctest @testArguments
} else {
    & ctest @testArguments
}
if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)" }
