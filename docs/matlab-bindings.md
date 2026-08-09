# MATLAB Bindings and QA

The `matlab/` tree provides a MATLAB/Octave MEX gateway and an object-oriented
package for profile inspection, color transforms, and profile visualization.

## Windows Desktop Setup

Run the following commands from the repository root in 64-bit PowerShell. The
setup derives paths from the checkout and installed tools; do not copy a
machine-specific checkout, MATLAB, Visual Studio, or vcpkg path into shared
instructions.

Discover the checkout, MATLAB, Visual Studio, and vcpkg:

```powershell
$ErrorActionPreference = 'Stop'
$Repo = (git rev-parse --show-toplevel).Trim()
$Build = Join-Path $Repo 'msvc'

$MatlabCommand = Get-Command matlab -ErrorAction SilentlyContinue
if ($MatlabCommand) {
  $MatlabExe = $MatlabCommand.Source
} else {
  $MatlabPattern = Join-Path $env:ProgramFiles 'MATLAB\*\bin\matlab.exe'
  $MatlabExe = Get-ChildItem $MatlabPattern |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
}
if (-not $MatlabExe) {
  throw 'MATLAB was not found on PATH or under Program Files.'
}

$VsInstallerDir = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
$VsWhere = Join-Path $VsInstallerDir 'vswhere.exe'
if (-not (Test-Path $VsWhere)) {
  throw 'vswhere.exe was not found. Install Visual Studio Build Tools.'
}
$VsWhereArgs = @(
  '-latest'
  '-version', '[17.0,18.0)'
  '-products', '*'
  '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
  '-property', 'installationPath'
)
$VsInstall = & $VsWhere @VsWhereArgs
if (-not $VsInstall) {
  throw 'A Visual Studio installation with the MSVC x64 tools was not found.'
}

if ($env:VCPKG_ROOT) {
  $VcpkgRoot = $env:VCPKG_ROOT
} else {
  $VcpkgRoot = Join-Path $VsInstall 'VC\vcpkg'
}
$Toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
if (-not (Test-Path $Toolchain)) {
  throw "The vcpkg CMake toolchain was not found at $Toolchain."
}
```

Confirm that MATLAB has an installed C++ compiler:

```powershell
& $MatlabExe -batch @"
cfg = mex.getCompilerConfigurations('C++', 'Selected');
assert(~isempty(cfg), 'No C++ MEX compiler is selected. Run mex -setup C++.');
assert(contains(cfg.Name, 'Microsoft Visual C++ 2022'), ...
  'Select Microsoft Visual C++ 2022 with mex -setup C++.');
disp(cfg.Name);
"@
if ($LASTEXITCODE -ne 0) {
  throw "MATLAB compiler discovery failed with exit code $LASTEXITCODE."
}
```

Configure and build a 64-bit Release static library. Argument arrays avoid
PowerShell line-continuation and quoting failures:

```powershell
$ConfigureArgs = @(
  '-S', (Join-Path $Repo 'Build\Cmake')
  '-B', $Build
  '-G', 'Visual Studio 17 2022'
  '-A', 'x64'
  "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
  '-DVCPKG_TARGET_TRIPLET=x64-windows'
  '-DENABLE_TESTS=ON'
  '-DENABLE_TOOLS=ON'
  '-DENABLE_ICCXML=ON'
  '-DENABLE_ICCJSON=ON'
  '-DENABLE_IMAGE_TOOLS=ON'
  '-DENABLE_CMM_TOOLS=ON'
)
cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) {
  throw "CMake configure failed with exit code $LASTEXITCODE."
}

$BuildArgs = @(
  '--build', $Build
  '--config', 'Release'
  '--target', 'IccProfLib2-static', 'iccProfilePlot'
  '--', '/m'
)
cmake @BuildArgs
if ($LASTEXITCODE -ne 0) {
  throw "Release library build failed with exit code $LASTEXITCODE."
}
```

Build the gateway in a batch MATLAB process:

```powershell
$env:ICCDEV_BUILD_DIR = $Build
$env:ICCDEV_REPO_ROOT = $Repo
& $MatlabExe -batch @"
repo_root = getenv('ICCDEV_REPO_ROOT');
cd(repo_root);
addpath(fullfile(repo_root, 'matlab'));
build_mex();
"@
if ($LASTEXITCODE -ne 0) {
  throw "MEX build failed with exit code $LASTEXITCODE."
}
```

The build script selects a Release `IccProfLib2` library. If the selected CMake
tree has `ICC_USE_ZLIB=ON`, it also links the matching Windows import library
and stages the runtime DLL beside `icc_mex.mexw64`.

The Windows archive published by `ci-latest-release.yml` includes the MATLAB
tree, the R2026a MEX gateway, and its zlib runtime. It also preserves the
repository-relative support paths used by QA and examples:

- `Testing/` for profile and luminance fixtures.
- `.github/ci/regression/gamma-2.20703125.icc` for gamma QA.
- `IccProfLib/IccTagBasic.cpp` and `IccProfLib/IccColorimetry.cpp` for the
  independent issue #1475 table check.

Keep those paths intact when extracting or relocating the bundle. Release CI
runs MATLAB from the staged archive tree before upload; testing only the source
checkout does not prove that the published payload is self-contained.

To make the checkout available to new MATLAB Desktop sessions without replacing
existing entries:

```powershell
$MatlabDir = Join-Path $Repo 'matlab'
$UserMatlabPath = [Environment]::GetEnvironmentVariable('MATLABPATH', 'User')
$MatlabPathEntries = @(
  $UserMatlabPath -split ';' |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
if ($MatlabPathEntries -notcontains $MatlabDir) {
  [Environment]::SetEnvironmentVariable(
    'MATLABPATH',
    (@($MatlabDir) + $MatlabPathEntries) -join ';',
    'User')
}
[Environment]::SetEnvironmentVariable('ICCDEV_BUILD_DIR', $Build, 'User')
```

Fully restart MATLAB Desktop after changing user environment variables.

## Profiles

The regression suite can use profiles generated under `Testing/Display`.
Generate them with the platform-native `Testing/CreateAllProfiles.*` workflow,
or copy only canonical `.icc` outputs from a trusted build while preserving
their relative `Testing` directories. Do not copy QA logs, crash artifacts, or
other unrelated untracked files from a long-lived test checkout.

The minimum display set includes:

- `sRGB_D65_MAT.icc`
- `sRGB_D65_colorimetric.icc`
- `LCDDisplay.icc`

## Profile Plotting

`iccdev.plot` renders every graph visualization exposed by the data-first
`iccProfilePlot` tool. It requires MATLAB R2016b+ or GNU Octave 7.1+ for
`jsondecode`, plus a Java-enabled runtime for shell-free process execution:

```matlab
profile_path = fullfile(repo_root, 'Testing', ...
  'sRGB_v4_ICC_preference.icc');
plots = iccdev.plot(profile_path);
```

The function renders curves, chromaticity diagrams, named-color plots, and
other graph descriptors. Raster CLUT descriptors are intentionally left to
`iccProfilePlot raster` or `iccProfileVisualizePlot`. For noninteractive use:

```matlab
plots = iccdev.plot(profile_path, 'Visible', 'off');
close([plots.figure]);
```

The tool is discovered through `ICCDEV_BUILD_DIR`, common repository build
directories, or `PATH`. Pass `BuildDir` or `PlotTool` when selecting another
build explicitly.

## Validation

Run the focused suite:

```matlab
repo_root = fileparts(fileparts(which('build_mex')));
addpath(fullfile(repo_root, 'matlab'));
addpath(fullfile(repo_root, 'matlab', 'tests'));
test_iccdev();
```

Run the extended local QA entry point:

```matlab
run_local_qa();
test_plot();
```

Run the demonstrations:

```matlab
run(fullfile(repo_root, 'matlab', 'examples', 'read_profile.m'));
run(fullfile(repo_root, 'matlab', 'examples', 'color_transform.m'));
run(fullfile(repo_root, 'matlab', 'examples', 'gamma_curve.m'));
```

Reproduce the spectral-viewing luminance calculations from issue #1811
without a C++ build or MEX gateway:

```matlab
repo_root = fileparts(fileparts(which('build_mex')));
addpath(fullfile(repo_root, 'matlab'));
addpath(fullfile(repo_root, 'matlab', 'tests'));
test_luminance_normalization();
run(fullfile(repo_root, 'matlab', 'examples', ...
  'luminance_normalization.m'));
```

This check reads the `sRGB_D65_MAT` fixtures authored at Y=1, 300, and
500 cd/m^2, verifies that dividing each XYZ triple by Y produces the same
D65 chromaticity, and records the single-precision behavior of the absolute
`0.01` warning window around Y=1. Hosted CI also builds and runs
`iccdev.luminance-normalization`, which calls the native
`CIccInfo::CheckLuminance` implementation and verifies its status, message,
nominal `0.99`/`1.01` endpoints, and adjacent representable float values.

Run the same native check locally from a CMake tree configured with
`ENABLE_TESTS=ON`:

```powershell
$NativeBuildArgs = @(
  '--build', $Build
  '--config', 'Release'
  '--target', 'iccLuminanceNormalizationTest'
)
cmake @NativeBuildArgs

$NativeTestArgs = @(
  '--test-dir', $Build
  '-C', 'Release'
  '-R', '^iccdev\.luminance-normalization$'
  '--output-on-failure'
  '--no-tests=error'
)
ctest @NativeTestArgs
```

Interpret the layers separately:

- `test_luminance_normalization` confirms the checked-in XML values and
  independently reproduces their double- and single-precision calculations.
- `iccdev.luminance-normalization` calls the compiled C++ method and is
  authoritative for validation status and diagnostic wording.
- Agreement between both layers confirms the fixture evidence and the native
  implementation without making either test self-referential.

## Spectral Colorimetry Cross-Check

Issue
[#1475](https://github.com/InternationalColorConsortium/iccDEV/issues/1475)
distinguishes the legacy 5 nm observer/illuminant direct-sum path from the
registry-aligned weighting tables exposed by `IccColorimetry`.

Run the independent MATLAB calculation:

```matlab
test_colorimetry_issue_1475();
run(fullfile('matlab', 'examples', 'colorimetry_issue_1475.m'));
```

The check reads the checked-in C++ table values and evaluates a perfect
diffuser under D50 with the CIE 1931 2-degree observer:

```text
legacy 5 nm direct sum: X=0.964245566 Y=1.000000000 Z=0.824679094
registry 10 nm table:   X=0.964240837 Y=1.000000000 Z=0.825128117
legacy at 10 nm:        X=0.963956086 Y=1.000000000 Z=0.824057353
canonical CIE 15:       X=0.964220000 Y=1.000000000 Z=0.825210000
ICC PCS icD50XYZ:       X=0.964200000 Y=1.000000000 Z=0.824900000
legacy - registry:      dX=+0.000004728 dY~0 dZ=-0.000449023
legacy - canonical:     dX=+0.000025566 dY=0  dZ=-0.000530906
registry - canonical:   dX=+0.000020837 dY~0 dZ=-0.000081883
legacy - ICC PCS:       dX=+0.000045566 dY=0  dZ=-0.000220906
registry - ICC PCS:     dX=+0.000040837 dY~0 dZ=+0.000228117
```

### Reading the difference

The legacy and registry paths differ in three ways at once -- reduction method,
sample grid, and the underlying illuminant data -- so the `-0.000449` gap cannot
be attributed to any one of them without holding the others fixed. Two of the
three are controlled:

- **Reduction method is not the cause.** `iccdev.colorimetry-methods` asserts
  `same-grid: DirectSum == Weighting` and `== SpragueTo1nm` to `TOL_EXACT`
  (`1e-6`), which is already far inside the `4.5e-4` gap. Driving
  `CIccColorimetricCalculator` with the real D50 SPD and 1931 CMFs instead of
  that test's synthetic data, the three methods return the same `XYZ` to the
  last bit -- measured spread `0.000e+00`. A weighting table is not
  intrinsically closer to CIE than a direct sum is.
- **Sample grid is not the cause either, and has the wrong sign.** Re-summing the
  same legacy data on the 10 nm subgrid gives `Z=0.824057353`, moving `Z` by
  `-0.000622` -- larger than the gap and in the opposite direction. The MATLAB
  check computes this as `grid_effect` and asserts both properties.

What remains is the data. This is consistent with the #1475 finding that all ten
registry weighting tables match the published registry values exactly while
iccDEV's own 5 nm D50 SPD (`icKnownIllums`, inherited in `889db62b`) is the
divergent object.

Which path is "closer" is therefore a choice of reference rather than a measure
of accuracy, and the native test pins **two** references, not one: Part F checks
the registry tables against CIE 15 (`0.96422 / 0.82521`), while Part E6 anchors
the legacy path to the ICC PCS illuminant `icD50XYZ` (`0.9642 / 0.8249`) with an
explicit note that the 5 nm tables sit ~5e-4 low on Z. Against CIE 15 the
registry table is closer; against the ICC PCS white the legacy path is closer.
The MATLAB check asserts both directions so neither reading can be quoted alone.

Hosted CI also runs `iccdev.colorimetry-methods`, which exercises the compiled
registry tables, loaded-table API, direct sum, weighting, Sprague resampling,
normalization, and range reconciliation.

Scope note: the registry tables are reachable through `IccColorimetry`, but
`CIccColorimetricCalculator` is currently referenced only by that regression
test -- no CMM or command-line path consumes it. The registry-aligned reduction
is an available primitive, not a change to what the library computes today.

Issue
[#1451](https://github.com/InternationalColorConsortium/iccDEV/issues/1451)
is downstream of this capability. The registry reduction primitive exists, but
`iccPawgReport` still maps only measured Lab or XYZ columns; spectral-only
characterization data needs a separate ingestion and reduction integration.

Expected coverage includes profile open/read/header checks, CMM pipeline and
bulk apply, per-thread apply handles, parent-close invalidation, native handle
lifecycle checks, single-precision input, missing-profile errors, finite output,
and self-transform accuracy. The gamma example also parses the checked-in
`curveType` fixture independently of IccProfLib and verifies the encoding math
from issue
[#815](https://github.com/InternationalColorConsortium/iccDEV/issues/815).

## curveType Gamma Cross-Check

ICC.1:2022 section 4.9 defines `u8Fixed8Number` as an unsigned 16-bit value
with eight fractional bits. Section 10.6 defines a one-entry `curveType` value
as a gamma exponent in `y = x^gamma`, not an inverse exponent.

The regression profile `.github/ci/regression/gamma-2.20703125.icc` stores the
integer value 565 in each of `rTRC`, `gTRC`, and `bTRC`. MATLAB checks both
equivalent calculations:

```text
encoded gamma:          565 / 256 = 2.20703125          (exact)
normalized storage:     single(565 / 65535)             = 0.00862134713679552
library reconstruction: stored * 65535 / 256            = 2.2070311898824
```

The reconstruction is close to the encoded value but not equal to it, and that
is the point of the check. `IccProfLib` keeps the normalized value in an
`icFloatNumber`, so rounding `565 / 65535` into a 24-bit significand costs a
relative error of up to `2^-24`, which the exact ratio `65535 / 256` carries
straight through. The bound is therefore `gamma * 2^-24` -- `1.32e-07` here --
and the observed error is `6.01e-08`. Modelling this step in double precision
instead would make the comparison succeed with an error of exactly zero no
matter what the library did.

The error is proportional to the gamma, so it is not the same across the
corpus: `gamma-2.3984375.icc` sits at `1.19e-07`, nearly twice as far out. What
must hold for every fixture is that the reconstructed value still rounds back
to the stored integer, which is what `iccFromJson` relies on.

This independently exercises the reconstruction corrected by PR
[#808](https://github.com/InternationalColorConsortium/iccDEV/pull/808). The
native `iccdev.curve-gamma-u8fixed8` CTest asserts the same bound against the
compiled library across all four `gamma-*.icc` fixtures. Run the MATLAB layer
directly with:

```matlab
results = run_gamma_qa();
```

## Rebuild Loop

After changing C++ gateway or IccProfLib code:

```matlab
clear classes
clear mex
repo_root = fileparts(fileparts(which('build_mex')));
build_mex('BuildDir', fullfile(repo_root, 'msvc'));
run_local_qa();
```

Close all `IccProfile`, `IccCmm`, and `IccApply` objects before `clear mex`.

## WSL2 Boundary

WSL2 is useful for Linux builds, sanitizer testing, profile generation, and
Octave compatibility. A Linux `.mex`, `.oct`, static library, or shared library
cannot be loaded by Windows MATLAB. Build the Windows MEX gateway with MATLAB's
configured MSVC compiler and Windows libraries.

## Docker Interoperability

MATLAB interoperates with the published Linux image through the Docker CLI and
profile files. It does not load Linux libraries into the Windows MATLAB
process.

Download the image:

```powershell
docker pull ghcr.io/internationalcolorconsortium/iccdev:latest
```

Run the MATLAB validation:

```matlab
result = run_docker_qa();
disp(result.imageId);
repo_root = fileparts(fileparts(which('build_mex')));
run(fullfile(repo_root, 'matlab', 'examples', 'docker_interop.m'));
```

If MATLAB Desktop does not inherit the Docker CLI directory:

```matlab
add_docker_path(docker_cli_directory);
```

The directory must contain `docker.exe` on Windows or `docker` elsewhere. The
helper updates only the current MATLAB process and does not execute Docker or
QA.

`iccdev.docker_validate` mounts only the selected profile file read-only,
disables container networking, drops Linux capabilities, enables
`no-new-privileges`, limits CPU, memory, and process count, and runs only fixed
`iccDumpProfile` and
`iccRoundTrip` commands. Image references are limited to the official
`ghcr.io/internationalcolorconsortium/iccdev` repository and may use a tag or
an immutable SHA-256 digest.

The output contract is recorded in
`matlab/tests/fixtures/docker_expected.txt`. Hosted CI uses a digest-pinned
image; interactive local use defaults to `latest`.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| `Undefined function 'icc_mex'` | Build the MEX gateway and add `matlab/`, not `+iccdev/private`, to the path. |
| Unresolved `deflate` or `inflate` symbols | Confirm `ICC_USE_ZLIB` in `CMakeCache.txt` and rebuild with the updated `build_mex.m`. |
| MEX load reports a missing DLL | Confirm the required vcpkg runtime DLL is beside `icc_mex.mexw64`. |
| Docker is unavailable | Start Docker Desktop and run `docker version`. |
| Docker image is unavailable | Pull the official image or pass its pinned digest with the `Image` option. |
| Only Debug libraries are found | Build `IccProfLib2-static` with `--config Release`. |
| CMM tests skip | Generate or copy compatible profiles into `Testing/Display`. |
| Stale behavior after rebuild | Close handles, run `clear classes`, run `clear mex`, then rebuild. |

## QA Checklist

- [ ] MATLAB detects a supported C++ compiler.
- [ ] `IccProfLib2-static` builds in Release mode.
- [ ] `build_mex` completes without unresolved dependencies.
- [ ] `test_iccdev` passes without skipped CMM tests.
- [ ] `run_local_qa` passes.
- [ ] All documented examples complete.
- [ ] `run_gamma_qa` decodes all three TRC tags as gamma 2.20703125.
- [ ] Issue #1475 MATLAB QA reproduces the legacy and registry D50 XYZ values.
- [ ] Native luminance and colorimetry CTests pass.
- [ ] `run_docker_qa` passes when Docker is available.
- [ ] Rebuild works after `clear classes` and `clear mex`.
- [ ] Modified `.m` and Markdown files remain ASCII.

## Hosted Workflow

`.github/workflows/ci-matlab.yml` runs for MATLAB-related pull requests to
`master`, selected MATLAB QA branches, and manual dispatch. It uses read-only
permissions, trusted-base sanitizer helpers, SHA-pinned actions, a focused
dependency-free MATLAB calculation stage, native luminance and colorimetry
CTests, checked-in profiles, a digest-pinned container interoperability job,
and no cache or artifact publication.
