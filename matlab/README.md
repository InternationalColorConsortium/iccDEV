# iccdev - MATLAB/Octave Bindings for RefIccMAX

MATLAB bindings for the [RefIccMAX (iccDEV)](https://github.com/InternationalColorConsortium/iccDEV)
ICC color profile library, built as a MEX extension.

## Features

- **Read ICC profiles** - open and inspect ICC v2/v4/v5 (iccMAX) profile headers
- **Color transforms** - build multi-profile CMM pipelines and apply pixel transforms
- **Thread-safe apply** - create per-thread apply handles
- **MATLAB OOP** - classes in `+iccdev` package namespace
- **NumPy-compatible** - handles column-major <-> row-major transpose automatically
- **Compatible** - MATLAB R2015b+ and GNU Octave 6+

## Requirements

- MATLAB R2015b+ (with MEX compiler) or GNU Octave 6+
- C++17 compiler (MSVC, GCC, or Clang)
- IccProfLib2 built (static library)

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
addpath('/path/to/iccDEV/matlab');
build_mex('BuildDir', '/path/to/iccDEV/build-matlab-full');
```

Or with environment variable:

```bash
export ICCDEV_BUILD_DIR=/path/to/iccDEV/build-matlab-full
```

```matlab
build_mex();
```

### 3. Use it

```matlab
addpath('/path/to/iccDEV/matlab');

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
from `mex.getCompilerConfigurations('C++', 'Installed')`:

```powershell
cmake -S Build\Cmake -B msvc -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build msvc --config Release --target IccProfLib2-static -- /m
```

```matlab
build_mex('BuildDir', 'C:\path\to\iccDEV\msvc');
```

When `ICC_USE_ZLIB=ON`, `build_mex` reads `CMakeCache.txt`, links the matching
vcpkg zlib import library, and copies its runtime DLL beside `icc_mex`.

### Linux / macOS

```matlab
build_mex('BuildDir', '/path/to/build-matlab-full');
```

### GNU Octave

```bash
export ICCDEV_BUILD_DIR=/path/to/build-matlab-full
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
cmake --build msvc --config Release --target iccCurveGammaU8Fixed8Test
ctest --test-dir msvc -C Release `
  -R '^iccdev\.curve-gamma-u8fixed8$' `
  --output-on-failure --no-tests=error
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
cmake --build msvc --config Release `
  --target iccLuminanceNormalizationTest
ctest --test-dir msvc -C Release `
  -R '^iccdev\.luminance-normalization$' `
  --output-on-failure --no-tests=error
```

The MATLAB check is an independent fixture/arithmetic model. The native CTest
is authoritative for warning status and message behavior; both should pass.

See [MATLAB bindings and QA](../docs/matlab-bindings.md) for the Windows
desktop workflow, profile generation, WSL2 boundaries, troubleshooting, and
repeatable validation checklist.

## License

BSD 3-Clause - same as the parent RefIccMAX project. See [LICENSE.md](../LICENSE.md).
