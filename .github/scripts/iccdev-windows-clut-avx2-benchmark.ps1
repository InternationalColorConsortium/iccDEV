# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

param(
  [Parameter(Mandatory = $true)]
  [string]$BaselineBuildDir,

  [Parameter(Mandatory = $true)]
  [string]$Avx2BuildDir,

  [ValidateSet('Release')]
  [string]$Configuration = 'Release',

  [ValidateRange(1, 1000000000)]
  [int]$Iterations = 5000000,

  [ValidateRange(1, 101)]
  [int]$Repetitions = 7,

  [ValidateRange(0, 63)]
  [int]$AffinityCpu = [math]::Min(
    63, [math]::Max(0, [Environment]::ProcessorCount - 2)),

  [string]$OutputPath = 'clut-avx2-benchmark.tsv'
)

$ErrorActionPreference = 'Stop'
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if ($Repetitions % 2 -eq 0) {
  throw 'Repetitions must be odd so the median is unambiguous'
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Join-Path $repoRoot '.github\ci\regression\clut-avx2-benchmark.cpp'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path $source -PathType Leaf)) {
  throw "Benchmark source not found: $source"
}
if (-not (Test-Path $vswhere -PathType Leaf)) {
  throw "Visual Studio locator not found: $vswhere"
}
$installationPath = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installationPath) {
  throw 'A Visual Studio installation with the x64 C++ tools was not found'
}
$devShell = Join-Path $installationPath 'Common7\Tools\Launch-VsDevShell.ps1'
if (-not (Test-Path $devShell -PathType Leaf)) {
  throw "Visual Studio developer shell not found: $devShell"
}

$savedEnvironment = @{
  PATH = $env:PATH
  INCLUDE = $env:INCLUDE
  LIB = $env:LIB
  LIBPATH = $env:LIBPATH
}
$env:PATH = @(
  "$env:SystemRoot\System32"
  "$env:SystemRoot"
  "$env:SystemRoot\System32\WindowsPowerShell\v1.0"
  (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer')
) -join ';'
Remove-Item env:INCLUDE, env:LIB, env:LIBPATH -ErrorAction SilentlyContinue
try {
  . $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

function Read-CMakeCacheValue {
  param(
    [string]$CachePath,
    [string]$Name
  )

  $pattern = '^' + [regex]::Escape($Name) + ':[^=]+='
  $line = Select-String -Path $CachePath -Pattern $pattern
  if (-not $line -or $line.Count -ne 1) {
    throw "CMake cache is missing unique metadata '$Name': $CachePath"
  }
  ($line.Line -split '=', 2)[1]
}

function Resolve-Build {
  param(
    [string]$Label,
    [string]$BuildDir
  )

  $resolved = (Resolve-Path $BuildDir).Path
  $cache = Join-Path $resolved 'CMakeCache.txt'
  $binDir = Join-Path $resolved "bin\$Configuration"
  $library = Join-Path $resolved "IccProfLib\$Configuration\IccProfLib2.lib"
  if (-not (Test-Path $cache -PathType Leaf) -or
      -not (Test-Path $library -PathType Leaf) -or
      -not (Test-Path (Join-Path $binDir 'IccProfLib2.dll') -PathType Leaf)) {
    throw "$Label build is incomplete: $resolved"
  }

  $cmakeSourceDir = Read-CMakeCacheValue $cache 'RefIccMAX_SOURCE_DIR'
  $sourceDir = Split-Path -Parent (Split-Path -Parent $cmakeSourceDir)
  $toolset = Read-CMakeCacheValue $cache 'CMAKE_GENERATOR_TOOLSET'
  [pscustomobject]@{
    Label = $Label
    BuildDir = $resolved
    BinDir = $binDir
    Library = $library
    SourceDir = $sourceDir
    Avx2Effective = Read-CMakeCacheValue $cache 'ICCDEV_AVX2_EFFECTIVE'
    Avx512Effective = Read-CMakeCacheValue $cache 'ICCDEV_AVX512_EFFECTIVE'
    Avx2Debug = Read-CMakeCacheValue $cache 'ICC_AVX2_CLUT_DEBUG'
    Generator = Read-CMakeCacheValue $cache 'CMAKE_GENERATOR'
    GeneratorInstance = Read-CMakeCacheValue $cache 'CMAKE_GENERATOR_INSTANCE'
    GeneratorPlatform = Read-CMakeCacheValue $cache 'CMAKE_GENERATOR_PLATFORM'
    GeneratorToolset = $toolset
    CompilerArchiver = Read-CMakeCacheValue $cache 'CMAKE_CXX_COMPILER_AR'
    Compiler = if ($toolset -eq 'ClangCL') { 'clang-cl.exe' } else { 'cl.exe' }
  }
}

$builds = @(
  Resolve-Build -Label 'baseline' -BuildDir $BaselineBuildDir
  Resolve-Build -Label 'avx2' -BuildDir $Avx2BuildDir
)

if ($builds[0].Avx2Effective -ne 'OFF' -or
    $builds[0].Avx512Effective -ne 'OFF') {
  throw 'Baseline build must have AVX2 and AVX-512 disabled'
}
if ($builds[1].Avx2Effective -ne 'ON' -or
    $builds[1].Avx512Effective -ne 'OFF') {
  throw 'AVX2 build must have AVX2 enabled and AVX-512 disabled'
}
if ($builds[0].Avx2Debug -ne 'OFF' -or $builds[1].Avx2Debug -ne 'OFF') {
  throw 'Baseline and AVX2 builds must disable ICC_AVX2_CLUT_DEBUG'
}
if ($builds[0].SourceDir -ine $builds[1].SourceDir) {
  throw 'Baseline and AVX2 builds must use the same source tree'
}
foreach ($property in @(
    'Generator',
    'GeneratorInstance',
    'GeneratorPlatform',
    'GeneratorToolset',
    'CompilerArchiver')) {
  if ($builds[0].$property -ine $builds[1].$property) {
    throw "Baseline and AVX2 build metadata differ: $property"
  }
}

$executables = @{}
foreach ($build in $builds) {
  $exe = Join-Path $build.BinDir 'clut-avx2-benchmark.exe'
  $includeDir = Join-Path $build.SourceDir 'IccProfLib'
  & $build.Compiler /nologo /O2 /EHsc /DICCPROFLIBDLL_IMPORTS `
    "/I$includeDir" $source /link "/OUT:$exe" $build.Library
  if ($LASTEXITCODE -ne 0) {
    throw "$($build.Label) benchmark compile failed with exit $LASTEXITCODE"
  }
  $executables[$build.Label] = $exe
}

$hostProbe = & $executables.avx2 15 1 1 $AffinityCpu
if ($LASTEXITCODE -ne 0) {
  throw 'AVX2 host capability probe failed'
}
if ($hostProbe -notmatch 'host_avx2=1') {
  throw 'The benchmark host does not expose AVX2 runtime support'
}

$comparison = foreach ($outputs in 8..16) {
  $samples = @{
    baseline = [Collections.Generic.List[double]]::new()
    avx2 = [Collections.Generic.List[double]]::new()
  }
  $outputVectors = @{}
  for ($sample = 0; $sample -lt $Repetitions; $sample++) {
    $order = if (($sample + $outputs) % 2 -eq 0) {
      @('baseline', 'avx2')
    }
    else {
      @('avx2', 'baseline')
    }
    foreach ($label in $order) {
      $line = & $executables[$label] $outputs $Iterations 1 $AffinityCpu
      if ($LASTEXITCODE -ne 0) {
        throw "$label benchmark failed for $outputs outputs"
      }
      if ($line -notmatch 'median_ns=([0-9.]+) calls_per_s=([0-9.]+) checksum=([0-9.]+) outputs_hex=([0-9a-f,]+)') {
        throw "Unexpected benchmark output: $line"
      }
      $samples[$label].Add([double]$Matches[1])
      $outputVectors[$label] = $Matches[4]
    }
  }

  $baselineSamples = $samples.baseline | Sort-Object
  $avx2Samples = $samples.avx2 | Sort-Object
  $middle = [int][math]::Floor($Repetitions / 2)
  $baselineMedian = $baselineSamples[$middle]
  $avx2Median = $avx2Samples[$middle]
  [pscustomobject]@{
    outputs = $outputs
    affinity_cpu = $AffinityCpu
    baseline_median_ns = $baselineMedian
    avx2_median_ns = $avx2Median
    improvement_pct = [math]::Round(
      (($baselineMedian - $avx2Median) / $baselineMedian) * 100.0, 3)
    output_vector_match = $outputVectors.baseline -ceq $outputVectors.avx2
  }
}

$outputMismatch = $comparison | Where-Object { -not $_.output_vector_match }
if ($outputMismatch) {
  $failedOutputs = ($outputMismatch.outputs -join ', ')
  throw "Baseline and AVX2 output vectors differ for outputs: $failedOutputs"
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$comparison | Export-Csv -Delimiter "`t" -NoTypeInformation -Encoding ascii -Path $resolvedOutput
$comparison | Format-Table -AutoSize
Write-Output "[PASS] benchmark report: $resolvedOutput"
}
finally {
  $env:PATH = $savedEnvironment.PATH
  foreach ($name in 'INCLUDE', 'LIB', 'LIBPATH') {
    $value = $savedEnvironment[$name]
    if ($null -eq $value) {
      Remove-Item "env:$name" -ErrorAction SilentlyContinue
    }
    else {
      Set-Item "env:$name" $value
    }
  }
}
