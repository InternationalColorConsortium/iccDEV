###############################################################
#
## Copyright (c) 2025-2026 International Color Consortium.
## All rights reserved.
## https://color.org
#
## Intent: Configure, build, and smoke-test a Windows ASan iccDEV tree.
#
###############################################################

[CmdletBinding()]
param(
    [string] $RepositoryUrl = "https://github.com/InternationalColorConsortium/iccDEV.git",
    [string] $Branch = "master",
    [string] $WorkRoot = (Get-Location).Path,
    [string] $BuildDir = "out\windows-asan",
    [string] $Preset = "vs2022-x64",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string] $Configuration = "Debug",
    [string] $CTestRegex = "iccdev.windows-icc-dump-profile-smoke",
    [switch] $UseCurrentCheckout,
    [switch] $SkipVcpkgInstall,
    [switch] $SkipTests,
    [switch] $ConfigureOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSDefaultParameterValues["*:ErrorAction"] = "Stop"

function Write-Section {
    param([string] $Message)
    Write-Host ""
    Write-Host "==================== $Message ====================" -ForegroundColor Green
}

function Invoke-Checked {
    param(
        [string] $FilePath,
        [string[]] $Arguments,
        [string] $WorkingDirectory = (Get-Location).Path
    )

    Write-Host "> $FilePath $($Arguments -join ' ')"
    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$FilePath exited with code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

function Get-RepositoryRoot {
    if ($UseCurrentCheckout) {
        $root = (Resolve-Path $WorkRoot).Path
        if (-not (Test-Path (Join-Path $root "Build\Cmake\CMakeLists.txt"))) {
            throw "UseCurrentCheckout requires WorkRoot to contain Build\Cmake\CMakeLists.txt: $root"
        }
        return $root
    }

    $root = Join-Path (Resolve-Path $WorkRoot).Path "iccDEV"
    if (-not (Test-Path $root)) {
        Invoke-Checked "git" @("clone", "--branch", $Branch, "--single-branch", $RepositoryUrl, $root) $WorkRoot
    }
    else {
        Invoke-Checked "git" @("fetch", "origin", $Branch) $root
        Invoke-Checked "git" @("checkout", $Branch) $root
        Invoke-Checked "git" @("pull", "--ff-only", "origin", $Branch) $root
    }
    return $root
}

function Initialize-VcpkgRoot {
    if (Test-Path env:VCPKG_ROOT) {
        return
    }

    $vcpkg = Get-Command "vcpkg" -ErrorAction SilentlyContinue
    if (-not $vcpkg) {
        throw "VCPKG_ROOT is not set and vcpkg was not found in PATH"
    }

    $env:VCPKG_ROOT = Split-Path -Parent $vcpkg.Source
    Write-Host "VCPKG_ROOT=$env:VCPKG_ROOT"
}

Write-Section "Starting Windows iccDEV ASan build"
Write-Host "Repository: $RepositoryUrl"
Write-Host "Branch: $Branch"
Write-Host "Preset: $Preset"
Write-Host "Configuration: $Configuration"

$env:ASAN_OPTIONS = "detect_leaks=0:halt_on_error=1:abort_on_error=1"

Initialize-VcpkgRoot
$repoRoot = Get-RepositoryRoot
$buildPath = Join-Path $repoRoot $BuildDir

Write-Section "Source revision"
Invoke-Checked "git" @("--no-pager", "log", "--oneline", "-5") $repoRoot

if (-not $SkipVcpkgInstall) {
    Write-Section "vcpkg dependencies"
    Invoke-Checked "vcpkg" @("install", "--x-manifest-root=$repoRoot\Build\Cmake") $repoRoot
}

Write-Section "Configure"
$configureArgs = @(
    "--preset", $Preset,
    "-S", "Build\Cmake",
    "-B", $buildPath,
    "-DENABLE_ASAN=ON",
    "-DENABLE_TESTS=ON",
    "-DENABLE_TOOLS=ON",
    "-DICCDEV_WINDOWS_UNIFIED_RUNTIME=ON",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)
Invoke-Checked "cmake" $configureArgs $repoRoot

if ($ConfigureOnly) {
    Write-Section "Configure-only run complete"
    exit 0
}

Write-Section "Build"
Invoke-Checked "cmake" @("--build", $buildPath, "--config", $Configuration, "--parallel") $repoRoot

$runtimeDir = Join-Path $buildPath "bin\$Configuration"
if (-not (Test-Path $runtimeDir)) {
    $runtimeDir = Join-Path $buildPath "bin"
}
if (-not (Test-Path $runtimeDir)) {
    throw "Expected runtime directory was not created under $buildPath\bin"
}

Write-Section "Runtime artifacts"
$artifacts = Get-ChildItem -File -Path $runtimeDir |
    Where-Object { $_.Extension -in @(".exe", ".dll") } |
    Sort-Object Name
foreach ($artifact in $artifacts) {
    Write-Host ("{0} {1}" -f $artifact.Name, $artifact.Length)
}

if (-not $SkipTests) {
    Write-Section "CTest smoke"
    Invoke-Checked "ctest" @(
        "--test-dir", $buildPath,
        "-C", $Configuration,
        "-R", $CTestRegex,
        "--output-on-failure",
        "--no-tests=error"
    ) $repoRoot
}

Write-Section "ICC profile report"
$profiles = Get-ChildItem -Path (Join-Path $repoRoot "Testing") -Filter "*.icc" -Recurse -File
$profiles | Group-Object { $_.Directory.FullName } | Sort-Object Name | ForEach-Object {
    Write-Host ("{0}: {1} .icc profiles" -f $_.Name, $_.Count)
}
Write-Host ("Total .icc profiles found: {0}" -f $profiles.Count)
Write-Host "[OK] Windows iccDEV ASan build script completed"
