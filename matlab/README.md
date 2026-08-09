# iccdev - MATLAB/Octave Bindings for RefIccMAX

MATLAB bindings for the [RefIccMAX (iccDEV)](https://github.com/InternationalColorConsortium/iccDEV)
ICC color profile library, built as a MEX extension.

## Features

- **Read ICC profiles** - open and inspect ICC v2/v4/v5 (iccMAX) profile headers
- **Color transforms** - build multi-profile CMM pipelines and apply pixel transforms
- **Thread-safe apply** - create per-thread apply handles
- **MATLAB OOP** - classes in `+iccdev` package namespace
- **Profile plots** - render data from the `iccProfilePlot` visualization model
- **NumPy-compatible** - handles column-major <-> row-major transpose automatically
- **Compatible** - MATLAB R2015b+ and GNU Octave 6+

## Requirements

- MATLAB R2015b+ (with MEX compiler) or GNU Octave 6+
- C++17 compiler (MSVC, GCC, or Clang)
- IccProfLib2 built (static library)

`iccdev.plot` additionally requires MATLAB R2016b+ or GNU Octave 7.1+ for
`jsondecode`, plus a Java-enabled runtime for shell-free process execution.

## Quick Start

### 1. Build IccProfLib2

```bash
cmake -S Build/Cmake -B build-matlab-full \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTS=ON \
  -DENABLE_TOOLS=ON \
  -DENABLE_ICCXML=ON \
  -DENABLE_ICCJSON=ON \
  -DENABLE_IMAGE_TOOLS=ON \
  -DENABLE_CMM_TOOLS=ON
cmake --build build-matlab-full -j$(nproc)
ctest --test-dir build-matlab-full --output-on-failure --no-tests=error
```

### 2. Build the MEX extension

```matlab
repo_root = pwd;
addpath(fullfile(repo_root, 'matlab'));
build_mex('BuildDir', fullfile(repo_root, 'build-matlab-full'));
```

Or with environment variable:

```bash
export ICCDEV_BUILD_DIR="$PWD/build-matlab-full"
```

```matlab
build_mex();
```

### 3. Use it

```matlab
repo_root = pwd;
addpath(fullfile(repo_root, 'matlab'));

% Read a profile
p = iccdev.IccProfile('sRGB.icc');
hdr = p.header();
fprintf('Version: %s\n', hdr.versionString);
fprintf('Color space: %s\n', iccdev.sig_to_str(uint32(hdr.colorSpace)));
p.close();

% Color transform
cmm = iccdev.IccCmm();
cmm.attach('input.icc', 'intent', iccdev.RenderingIntent.Perceptual);
cmm.attach('output.icc');
cmm.begin();

result = cmm.apply([0.5, 0.3, 0.1]);
fprintf('Result: [%.4f, %.4f, %.4f]\n', result);

% Bulk transform (N x channels matrix)
pixels = rand(1000, 3, 'single');
results = cmm.apply(pixels);

cmm.close();
```

## API Reference

### Classes

| Class | Description |
|-------|-------------|
| `iccdev.IccProfile` | Read/open ICC profiles, inspect header |
| `iccdev.IccCmm` | Color Management Module - build & apply transforms |
| `iccdev.IccApply` | Thread-safe per-thread apply handle |

### Constants

| Class | Description |
|-------|-------------|
| `iccdev.ColorSpace` | ICC color space signatures (RGB, CMYK, Lab, XYZ, ...) |
| `iccdev.RenderingIntent` | Perceptual, RelativeColorimetric, Saturation, Absolute |
| `iccdev.Interpolation` | Linear, Tetrahedral |

### Functions

| Function | Description |
|----------|-------------|
| `iccdev.sig_to_str(sig)` | Convert 4-byte ICC signature to ASCII string |
| `iccdev.plot(profile, ...)` | Render all graph visualizations exposed by `iccProfilePlot` |
| `iccdev.qa.check_luminance_normalization()` | Reproduce spectral-viewing luminance scaling and warning-window checks |
| `iccdev.docker_available(image)` | Check Docker daemon and image availability |
| `iccdev.docker_validate(profile, ...)` | Run containerized dump and round-trip validation |
| `build_mex(...)` | Build the MEX extension |
| `run_local_qa()` | Run local MEX regression and stress checks |
| `run_gamma_qa()` | Verify issue #815 curveType u8Fixed8 gamma decoding |
| `run_docker_qa(image)` | Validate the published container and output contract |

### iccdev.IccProfile

```matlab
p = iccdev.IccProfile('path/to/profile.icc');
p = iccdev.IccProfile('path/to/profile.icc', 'lazy', false);

hdr = p.header();    % Returns struct with all header fields
p.display();         % Print header summary
p.close();
```

Header struct fields:
- `size`, `cmmId`, `version`, `deviceClass`, `colorSpace`, `pcs`
- `magic`, `platform`, `flags`, `manufacturer`, `model`
- `attributes`, `renderingIntent`, `creator`
- `illuminantX/Y/Z`, `dateYear/Month/Day/Hours/Minutes/Seconds`
- `profileId` (16-element uint8)
- `versionString` (computed, e.g., `'4.3.0'`)

### iccdev.IccCmm

```matlab
cmm = iccdev.IccCmm();
cmm.attach('profile.icc', 'intent', iccdev.RenderingIntent.Perceptual);
cmm.begin();

result = cmm.apply([0.5, 0.3, 0.1]);        % Single pixel
results = cmm.apply(rand(100, 3, 'single')); % Bulk

ah = cmm.get_apply();  % Thread-safe handle
r = ah.apply([0.5, 0.3, 0.1]);
ah.close();

cmm.close();
```

`IccApply` keeps its parent `IccCmm` object alive while the apply handle is open.
If the parent CMM is closed explicitly, child apply handles are invalidated and
subsequent `apply()` calls raise `iccdev:parentClosed` instead of using freed
native state.

## Build Configuration

### Windows (MSVC)

Configure and build a Release tree with the same MSVC toolchain MATLAB reports
from `mex.getCompilerConfigurations('C++', 'Selected')`. The canonical
PowerShell workflow in
[MATLAB bindings and QA](../docs/matlab-bindings.md#windows-desktop-setup)
derives the checkout, MATLAB, Visual Studio, and vcpkg paths and uses argument
arrays to avoid quoting and line-continuation errors.

```powershell
$Repo = (git rev-parse --show-toplevel).Trim()
$Build = Join-Path $Repo 'msvc'
# Discover $Toolchain as documented in docs/matlab-bindings.md.
$ConfigureArgs = @(
  '-S', (Join-Path $Repo 'Build\Cmake')
  '-B', $Build
  '-G', 'Visual Studio 17 2022'
  '-A', 'x64'
  "-DCMAKE_TOOLCHAIN_FILE=$Toolchain"
)
cmake @ConfigureArgs
cmake --build $Build --config Release --target IccProfLib2-static -- /m
```

```matlab
repo_root = fileparts(fileparts(which('build_mex')));
build_mex('BuildDir', fullfile(repo_root, 'msvc'));
```

When `ICC_USE_ZLIB=ON`, `build_mex` reads `CMakeCache.txt`, links the matching
vcpkg zlib import library, and copies its runtime DLL beside `icc_mex`.

Build the data-first plotting CLI when using `iccdev.plot`:

```powershell
cmake --build $Build --config Release --target iccProfilePlot --parallel
```

Then render every graph exposed by a profile:

```matlab
plots = iccdev.plot(profile_path);
```

Use `'Visible', 'off'` for automated checks. `iccdev.plot` searches
`ICCDEV_BUILD_DIR`, common repository build directories, and `PATH`; use the
`BuildDir` or `PlotTool` option to select an explicit build.

### Linux / macOS

```matlab
repo_root = fileparts(fileparts(which('build_mex')));
build_mex('BuildDir', fullfile(repo_root, 'build-matlab-full'));
```

### GNU Octave

```bash
export ICCDEV_BUILD_DIR="$PWD/build-matlab-full"
octave --eval "addpath('matlab'); build_mex();"
```

## Testing

```matlab
addpath('matlab');
addpath('matlab/tests');
test_iccdev();
```

Run the complete regression, profile, bulk transform, apply-handle, and
missing-profile smoke checks with:

```matlab
run_local_qa();
test_plot();
```

Verify the ICC.1 `curveType` gamma calculation used by issue
[#815](https://github.com/InternationalColorConsortium/iccDEV/issues/815)
and PR
[#808](https://github.com/InternationalColorConsortium/iccDEV/pull/808):

```matlab
run_gamma_qa();
run('matlab/examples/gamma_curve.m');
```

The checked-in fixture stores 565 as a `u8Fixed8Number`, so the decoded gamma is
`565 / 256 = 2.20703125` exactly. The library's normalized internal storage is
`565 / 65535` held in an `icFloatNumber`, so reconstructing it as
`stored * 65535 / 256` gives `2.2070311898824` -- within `gamma * 2^-24` of the
encoded value, not equal to it, and still rounding back to 565.

Validate the compiled library against the same arithmetic across all four
`gamma-*.icc` fixtures:

```powershell
$NativeBuildArgs = @(
  '--build', $Build
  '--config', 'Release'
  '--target', 'iccCurveGammaU8Fixed8Test'
)
cmake @NativeBuildArgs
$NativeTestArgs = @(
  '--test-dir', $Build
  '-C', 'Release'
  '-R', '^iccdev\.curve-gamma-u8fixed8$'
  '--output-on-failure'
  '--no-tests=error'
)
ctest @NativeTestArgs
```

Validate the same profile with the published container:

```matlab
run_docker_qa();
run('matlab/examples/docker_interop.m');
```

Reproduce the issue #1811 spectral-viewing luminance calculations without
building the MEX gateway:

```matlab
addpath('matlab');
addpath('matlab/tests');
test_luminance_normalization();
run('matlab/examples/luminance_normalization.m');
```

Validate the compiled `CIccInfo::CheckLuminance` implementation separately
from a CMake tree configured with `ENABLE_TESTS=ON`:

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

Reproduce issue
[#1475](https://github.com/InternationalColorConsortium/iccDEV/issues/1475)
by comparing the legacy 5 nm D50 direct sum with the registry-loaded 10 nm
weighting table:

```matlab
test_colorimetry_issue_1475();
run('matlab/examples/colorimetry_issue_1475.m');
```

The MATLAB check parses the tables directly from `IccTagBasic.cpp` and
`IccColorimetry.cpp`. For a perfect diffuser under D50 with the CIE 1931
2-degree observer, the legacy path produces `Z=0.824679094` while the registry
table produces `Z=0.825128117`, a gap of `-0.000449`.

That gap is a difference in the illuminant *data*, not in how it is integrated:

- **Not the reduction method.** `iccdev.colorimetry-methods` asserts
  `DirectSum == Weighting == SpragueTo1nm` to `TOL_EXACT` on a common grid, so a
  weighting table is not intrinsically closer to CIE than a direct sum.
- **Not the sample grid.** Re-summing the same legacy data on the 10 nm subgrid
  moves `Z` by `-0.000622` -- larger than the gap and opposite in sign. The
  check computes this as `grid_effect` and asserts it.

Which path is "closer" is a choice of reference, so the check pins both
directions: against CIE 15 (`Z=0.82521`) the registry table wins
(`-0.000082` versus `-0.000531`), while against the ICC PCS illuminant
`icD50XYZ` (`Z=0.8249`, what the native test anchors the legacy path to) the
legacy path wins (`-0.000221` versus `+0.000228`).

Note the registry table is reachable through `IccColorimetry` but is not yet on
any live path: `CIccColorimetricCalculator` is referenced only by the
colorimetry-methods regression test, so this is a parallel API rather than a
change to what the CMM currently computes.

Issue
[#1451](https://github.com/InternationalColorConsortium/iccDEV/issues/1451)
is related but remains a separate ingestion concern: `iccPawgReport` currently
requires measured Lab or XYZ columns and does not derive PCS values from
spectral-only characterization rows.

The MATLAB check is an independent fixture/arithmetic model. The native CTest
is authoritative for warning status and message behavior; both should pass.

See [MATLAB bindings and QA](../docs/matlab-bindings.md) for the Windows
desktop workflow, profile generation, WSL2 boundaries, troubleshooting, and
repeatable validation checklist.

## License

BSD 3-Clause - same as the parent RefIccMAX project. See [LICENSE.md](../LICENSE.md).
