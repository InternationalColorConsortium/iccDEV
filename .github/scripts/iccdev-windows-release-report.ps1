# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

param(
  [string]$BuildDir = 'out\windows-msvc-release-report',
  [string]$Preset = 'vs2022-x64',
  [ValidateSet('Release', 'RelWithDebInfo', 'MinSizeRel')]
  [string]$Configuration = 'Release',
  [ValidateRange(1, 1000000000)]
  [int]$Pixels = 65536,
  [ValidateRange(1, 101)]
  [int]$Repeats = 5,
  [ValidatePattern('^[1-9][0-9]*(,[1-9][0-9]*)*$')]
  [string]$Threads = '1,2,4,8,16',
  [string[]]$CMakeConfigureArguments = @(),
  [string]$OutputDir = '',
  [string]$ProfileRoot = '',
  [switch]$SkipConfigure,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
  throw 'This report requires Windows'
}
if ($Repeats % 2 -eq 0) {
  throw 'Repeats must be odd so the benchmark median is unambiguous'
}

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$sourceDir = Join-Path $repoRoot 'Build\Cmake'

function Resolve-ReportPath {
  param([string]$Path, [string]$BasePath)

  if ([IO.Path]::IsPathRooted($Path)) {
    return [IO.Path]::GetFullPath($Path)
  }
  return [IO.Path]::GetFullPath((Join-Path $BasePath $Path))
}

$resolvedBuildDir = Resolve-ReportPath $BuildDir $repoRoot
if ($OutputDir) {
  $resolvedOutputDir = Resolve-ReportPath $OutputDir $repoRoot
}
else {
  $resolvedOutputDir = Join-Path $resolvedBuildDir 'reports'
}
New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

$configureLog = Join-Path $resolvedOutputDir 'configure.log'
$buildLog = Join-Path $resolvedOutputDir 'build.log'
$profileLog = Join-Path $resolvedOutputDir 'create-profiles.log'
$benchmarkCsv = Join-Path $resolvedOutputDir 'iccbenchapply.csv'
$benchmarkProgressLog = Join-Path $resolvedOutputDir 'iccbenchapply-progress.log'
$jsonReport = Join-Path $resolvedOutputDir 'windows-release-report.json'
$markdownReport = Join-Path $resolvedOutputDir 'windows-release-report.md'

function Invoke-NativeLogged {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [Parameter(Mandatory = $true)][string[]]$Arguments,
    [Parameter(Mandatory = $true)][string]$LogPath
  )

  & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $LogPath
  $exitCode = $LASTEXITCODE
  if ($exitCode -ne 0) {
    throw "$FilePath failed with exit code $exitCode; see $LogPath"
  }
}

function Read-CMakeCacheValue {
  param([string]$Name)

  $cachePath = Join-Path $resolvedBuildDir 'CMakeCache.txt'
  $match = Select-String -Path $cachePath -Pattern "^$([regex]::Escape($Name))(:[^=]+)?=(.*)$"
  if ($match) {
    return $match.Matches[0].Groups[2].Value
  }
  return ''
}

$configureSeconds = 0.0
if (-not $SkipConfigure) {
  $configureTimer = [Diagnostics.Stopwatch]::StartNew()
  Invoke-NativeLogged cmake (@(
    '--preset', $Preset,
    '-S', $sourceDir,
    '-B', $resolvedBuildDir
  ) + $CMakeConfigureArguments) $configureLog
  $configureTimer.Stop()
  $configureSeconds = $configureTimer.Elapsed.TotalSeconds
}
elseif (-not (Test-Path (Join-Path $resolvedBuildDir 'CMakeCache.txt') -PathType Leaf)) {
  throw "Configured build directory not found: $resolvedBuildDir"
}

$buildSeconds = 0.0
if (-not $SkipBuild) {
  $buildTimer = [Diagnostics.Stopwatch]::StartNew()
  Invoke-NativeLogged cmake @(
    '--build', $resolvedBuildDir,
    '--config', $Configuration,
    '--parallel',
    '--',
    '/clp:PerformanceSummary',
    '/clp:Summary'
  ) $buildLog
  $buildTimer.Stop()
  $buildSeconds = $buildTimer.Elapsed.TotalSeconds
}

$profileSeconds = 0.0
if ($ProfileRoot) {
  $benchmarkRoot = Resolve-ReportPath $ProfileRoot $repoRoot
}
else {
  $profileTimer = [Diagnostics.Stopwatch]::StartNew()
  Invoke-NativeLogged ctest @(
    '--test-dir', $resolvedBuildDir,
    '-C', $Configuration,
    '-R', '^iccdev.windows-create-profiles$',
    '--output-on-failure',
    '--no-tests=error'
  ) $profileLog
  $profileTimer.Stop()
  $profileSeconds = $profileTimer.Elapsed.TotalSeconds
  $benchmarkRoot = Join-Path $resolvedBuildDir 'Testing\ctest-output\windows-testing'
}

$binDir = Join-Path $resolvedBuildDir "bin\$Configuration"
$benchmarkExe = Join-Path $binDir 'iccBenchApply.exe'
if (-not (Test-Path $benchmarkExe -PathType Leaf)) {
  throw "iccBenchApply was not built: $benchmarkExe"
}
if (-not (Test-Path $benchmarkRoot -PathType Container)) {
  throw "Generated Windows Testing tree was not found: $benchmarkRoot"
}

$savedEnvironment = @{
  PATH = $env:PATH
  ICCDEV_BENCH_SOURCE_ROOT = $env:ICCDEV_BENCH_SOURCE_ROOT
  ICCDEV_BENCH_BUILD_ROOT = $env:ICCDEV_BENCH_BUILD_ROOT
}
$benchmarkTimer = [Diagnostics.Stopwatch]::StartNew()
try {
  $env:PATH = "$binDir;$env:PATH"
  $env:ICCDEV_BENCH_SOURCE_ROOT = $repoRoot
  $env:ICCDEV_BENCH_BUILD_ROOT = $benchmarkRoot
  & $benchmarkExe -suite -csv -pixels $Pixels -repeats $Repeats -threads $Threads `
    2> $benchmarkProgressLog | Set-Content -Encoding ascii $benchmarkCsv
  $benchmarkExit = $LASTEXITCODE
  if ($benchmarkExit -ne 0) {
    throw "iccBenchApply failed with exit code $benchmarkExit; see $benchmarkProgressLog"
  }
}
finally {
  $benchmarkTimer.Stop()
  foreach ($name in $savedEnvironment.Keys) {
    $value = $savedEnvironment[$name]
    if ($null -eq $value) {
      Remove-Item "env:$name" -ErrorAction SilentlyContinue
    }
    else {
      Set-Item "env:$name" $value
    }
  }
}

$benchmarkRows = @(Import-Csv $benchmarkCsv)
$measuredRows = @($benchmarkRows | Where-Object { $_.status -eq 'ok' })
if (-not $measuredRows.Count) {
  throw "iccBenchApply produced no measured rows: $benchmarkCsv"
}

$benchmarkSummary = foreach ($row in $benchmarkRows) {
  $baseline = $benchmarkRows | Where-Object {
    $_.case -eq $row.case -and $_.threads -eq '1' -and $_.status -eq 'ok'
  } | Select-Object -First 1
  $rate = if ($row.mpx_per_sec) { [double]$row.mpx_per_sec } else { 0.0 }
  $baselineRate = if ($baseline) { [double]$baseline.mpx_per_sec } else { 0.0 }
  [pscustomobject]@{
    case = $row.case
    threads = if ($row.threads) { [int]$row.threads } else { 0 }
    median_mpx_per_sec = $rate
    min_mpx_per_sec = if ($row.min) { [double]$row.min } else { 0.0 }
    max_mpx_per_sec = if ($row.max) { [double]$row.max } else { 0.0 }
    speedup_vs_1 = if ($rate -gt 0.0 -and $baselineRate -gt 0.0) {
      [math]::Round($rate / $baselineRate, 3)
    } else { 0.0 }
    checksum = $row.checksum
    status = $row.status
  }
}

$processor = Get-CimInstance Win32_Processor | Select-Object -First 1
$operatingSystem = Get-CimInstance Win32_OperatingSystem
$computerSystem = Get-CimInstance Win32_ComputerSystem
$volume = Get-Volume -DriveLetter ([IO.Path]::GetPathRoot($resolvedBuildDir).Substring(0, 1))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = $null
if (Test-Path $vswhere -PathType Leaf) {
  $visualStudio = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -format json | ConvertFrom-Json | Select-Object -First 1
}

$projectArtifacts = @(Get-ChildItem $resolvedBuildDir -Recurse -File |
  Where-Object {
    $_.FullName -notmatch '\\vcpkg_installed\\' -and
    $_.FullName -notmatch '\\CMakeFiles\\' -and
    $_.Extension -in '.exe', '.dll', '.lib', '.pdb'
  })
$warningCount = if (Test-Path $buildLog -PathType Leaf) {
  (Select-String -Path $buildLog -Pattern ': warning [A-Z]+[0-9]+:' | Measure-Object).Count
}
else { 0 }

$report = [ordered]@{
  format = 'iccdev-windows-release-report-v1'
  generated_utc = [DateTime]::UtcNow.ToString('o')
  source = [ordered]@{
    commit = (git -C $repoRoot rev-parse HEAD)
    branch = (git -C $repoRoot branch --show-current)
    filesystem = $volume.FileSystem
    git_core_autocrlf = (git -C $repoRoot config --get core.autocrlf)
  }
  host = [ordered]@{
    os = $operatingSystem.Caption
    os_version = $operatingSystem.Version
    os_build = $operatingSystem.BuildNumber
    cpu = $processor.Name.Trim()
    physical_cores = $processor.NumberOfCores
    logical_processors = $processor.NumberOfLogicalProcessors
    memory_bytes = [long]$computerSystem.TotalPhysicalMemory
    volume_free_bytes = [long]$volume.SizeRemaining
  }
  toolchain = [ordered]@{
    cmake = (cmake --version | Select-Object -First 1)
    generator = Read-CMakeCacheValue 'CMAKE_GENERATOR'
    generator_toolset = Read-CMakeCacheValue 'CMAKE_GENERATOR_TOOLSET'
    cxx_compiler = Read-CMakeCacheValue 'CMAKE_CXX_COMPILER'
    cxx_compiler_version = Read-CMakeCacheValue 'CMAKE_CXX_COMPILER_VERSION'
    visual_studio = if ($visualStudio) { $visualStudio.displayName } else { '' }
    visual_studio_version = if ($visualStudio) { $visualStudio.installationVersion } else { '' }
    preset = $Preset
    configuration = $Configuration
  }
  build = [ordered]@{
    configure_seconds = [math]::Round($configureSeconds, 3)
    build_seconds = [math]::Round($buildSeconds, 3)
    profile_generation_seconds = [math]::Round($profileSeconds, 3)
    warnings = $warningCount
    artifacts = $projectArtifacts.Count
    artifact_bytes = [long](($projectArtifacts | Measure-Object Length -Sum).Sum)
    executables = @($projectArtifacts | Where-Object Extension -eq '.exe').Count
    dlls = @($projectArtifacts | Where-Object Extension -eq '.dll').Count
    libraries = @($projectArtifacts | Where-Object Extension -eq '.lib').Count
    pdbs = @($projectArtifacts | Where-Object Extension -eq '.pdb').Count
  }
  benchmark = [ordered]@{
    pixels = $Pixels
    repeats = $Repeats
    threads = $Threads
    elapsed_seconds = [math]::Round($benchmarkTimer.Elapsed.TotalSeconds, 3)
    measured_rows = $measuredRows.Count
    skipped_rows = @($benchmarkRows | Where-Object { $_.status -ne 'ok' }).Count
    rows = @($benchmarkSummary)
  }
}

$report | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8NoBOM $jsonReport

$markdown = [Collections.Generic.List[string]]::new()
$markdown.Add('# Windows Release Performance Report')
$markdown.Add('')
$markdown.Add("Commit: ``$($report.source.commit)``")
$markdown.Add('')
$markdown.Add('## Host and Toolchain')
$markdown.Add('')
$markdown.Add('| Field | Value |')
$markdown.Add('|---|---|')
$markdown.Add("| OS | $($report.host.os) $($report.host.os_version) (build $($report.host.os_build)) |")
$markdown.Add("| CPU | $($report.host.cpu) |")
$markdown.Add("| Cores | $($report.host.physical_cores) physical / $($report.host.logical_processors) logical |")
$markdown.Add("| Memory | $([math]::Round($report.host.memory_bytes / 1GB, 2)) GiB |")
$markdown.Add("| Filesystem | $($report.source.filesystem) |")
$markdown.Add("| Visual Studio | $($report.toolchain.visual_studio) $($report.toolchain.visual_studio_version) |")
$markdown.Add("| CMake | $($report.toolchain.cmake) |")
$markdown.Add("| Generator | $($report.toolchain.generator) $($report.toolchain.generator_toolset) |")
$markdown.Add('')
$markdown.Add('## Build')
$markdown.Add('')
$markdown.Add('| Metric | Value |')
$markdown.Add('|---|---:|')
$markdown.Add("| Configure | $($report.build.configure_seconds) s |")
$markdown.Add("| Build | $($report.build.build_seconds) s |")
$markdown.Add("| Profile generation | $($report.build.profile_generation_seconds) s |")
$markdown.Add("| Compiler warnings | $($report.build.warnings) |")
$markdown.Add("| Project artifacts | $($report.build.artifacts) |")
$markdown.Add("| Project artifact size | $([math]::Round($report.build.artifact_bytes / 1MB, 2)) MiB |")
$markdown.Add('')
$markdown.Add('## iccBenchApply')
$markdown.Add('')
$markdown.Add("Pixels per buffer: $Pixels; repeats: $Repeats; threads: ``$Threads``.")
$markdown.Add('')
$markdown.Add('| Case | Threads | Median Mpx/s | Min | Max | Speedup vs 1 | Status |')
$markdown.Add('|---|---:|---:|---:|---:|---:|---|')
foreach ($row in $benchmarkSummary) {
  $markdown.Add(
    "| $($row.case) | $($row.threads) | $($row.median_mpx_per_sec) | " +
    "$($row.min_mpx_per_sec) | $($row.max_mpx_per_sec) | " +
    "$($row.speedup_vs_1)x | $($row.status) |")
}
$markdown.Add('')
$markdown.Add("Raw CSV: ``$benchmarkCsv``")
$markdown | Set-Content -Encoding utf8NoBOM $markdownReport

Write-Output "[PASS] JSON report: $jsonReport"
Write-Output "[PASS] Markdown report: $markdownReport"
Write-Output "[PASS] iccBenchApply CSV: $benchmarkCsv"
