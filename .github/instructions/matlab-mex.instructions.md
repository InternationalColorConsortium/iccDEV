---
applyTo: "matlab/**"
---

# MATLAB/Octave MEX Bindings -- Path-Specific Instructions

## What This Is

MATLAB/Octave interface to IccProfLib via a single MEX gateway binary.
An OOP wrapper layer in the `+iccdev` package namespace provides
MATLAB-idiomatic classes on top of the MEX function calls.

## File Structure

```
matlab/
  mex/
    icc_mex.cpp           # MEX gateway (702 lines) -- action-string dispatch
  +iccdev/
    +qa/
      check_luminance_normalization.m  # Issue #1811 fixture calculation model
    IccProfile.m          # Profile class (open, read, header, color_space)
    IccCmm.m              # Color management module wrapper
    IccApply.m            # Thread-safe per-thread apply handle
    plot.m                # Render graphs from the iccProfilePlot data model
    ColorSpace.m          # Color space signature enum
    RenderingIntent.m     # Rendering intent enum
    Interpolation.m       # Interpolation method enum
    sig_to_str.m          # Signature to string utility
  examples/
    read_profile.m        # Profile reading example
    color_transform.m     # Color transform example
    luminance_normalization.m # Explain issue #1811 fixture scaling
    gamma_curve.m         # curveType u8Fixed8 gamma calculation example
  build_mex.m             # Build script (auto-detects library location)
  run_gamma_qa.m          # Issue #815 gamma fixture and math verification
  add_docker_path.m       # Opt-in current-process Docker PATH helper
  tests/
    test_iccdev.m         # Test suite (MATLAB/Octave compatible)
    test_luminance_normalization.m # Dependency-free issue #1811 check
  README.md               # Usage documentation
```

## Architecture

### MEX Gateway Pattern

`icc_mex.cpp` uses a single-entry-point design with action-string dispatch:

```matlab
result = icc_mex('action_name', arg1, arg2, ...);
```

Actions include: `profile_open`, `profile_read`, `profile_header`, `profile_free`,
`cmm_create`, `cmm_attach`, `cmm_begin`, `cmm_info`, `cmm_apply`, `cmm_free`,
`apply_create`, `apply_apply`, `apply_free`.

### Handle Registry

A global handle table maps integer handles to C++ object pointers:
- `profile_open` returns a handle (integer)
- Subsequent calls pass the handle to identify the object
- `profile_free` releases the handle and deletes the C++ object
- Prevents dangling pointers across MATLAB/Octave workspace operations

### OOP Layer

The `+iccdev` package provides MATLAB classes that wrap MEX calls:

```matlab
prof = iccdev.IccProfile('path/to/profile.icc');
hdr = prof.get_header();
disp(hdr.version);

cmm = iccdev.IccCmm();
cmm.attach('input.icc');
cmm.attach('output.icc');
cmm.begin();
out = cmm.apply(pixelData);
```

## Build

`docs/matlab-bindings.md` is the canonical user workflow, including portable
Windows PowerShell discovery for the checkout, MATLAB, Visual Studio, and
vcpkg. Do not publish machine-specific absolute paths in examples.

### MATLAB

```matlab
cd matlab
build_mex
```

`build_mex.m` searches for `IccProfLib2-static` in these directories
(relative to the build root, which defaults to `../Build`):
1. `ICCDEV_BUILD_DIR` environment variable (if set)
2. `../Build/` (default cmake build dir)
3. `../Build/IccProfLib/`
4. `../Build/lib/`
5. `../Build/Release/` and `../Build/Debug/`
6. `../Build/IccProfLib/Release/` and `../Build/IccProfLib/Debug/`

### Octave

```bash
cd matlab && octave --eval "build_mex"
```

## Test

Run from the repository root so both the package namespace and tests are on
the MATLAB/Octave path:

```matlab
addpath('matlab');
addpath('matlab/tests');
test_iccdev();
```

Or from Octave at the repository root:
```bash
octave --eval "addpath('matlab'); addpath('matlab/tests'); test_iccdev();"
```

For the extended local smoke and stress checks:

```matlab
addpath('matlab');
run_local_qa();
run_gamma_qa();
test_add_docker_path();
test_plot();
```

For Docker CLI interoperability:

```matlab
run_docker_qa();
```

For issue #1811 luminance calculations, first run the dependency-free MATLAB
model:

```matlab
test_luminance_normalization();
run('matlab/examples/luminance_normalization.m');
```

Then run the native C++ implementation check from a CMake tree configured with
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

The MATLAB layer validates XML fixture scaling and emulates `icFloatNumber`
precision. The native CTest calls `CIccInfo::CheckLuminance` and pins its
warning status, message, nominal `0.99`/`1.01` endpoints, adjacent float
values, and physical Y values 5, 300, and 500.

## Dependencies

- MATLAB R2015b+ or GNU Octave 6+
- Pre-built IccProfLib2-static library (run cmake build first)
- C++ compiler supported by MATLAB's MEX infrastructure

## Common Pitfalls

- `build_mex.m` expects the iccDEV CMake build to be completed first. Use the
  platform-specific argument-array workflow in `docs/matlab-bindings.md`;
  avoid Bash operators such as `&&` and `$(nproc)` in PowerShell instructions.
- `iccdev.plot` also requires the `iccProfilePlot` executable. Build that CMake
  target and keep its build root in `ICCDEV_BUILD_DIR`, or pass `BuildDir` or
  `PlotTool` explicitly. Plotting requires MATLAB R2016b+ or GNU Octave 7.1+
  with `jsondecode` and a Java-enabled runtime.
- Normal MATLAB/Octave builds reject Debug-only `IccProfLib2` libraries. Use a
  Release iccDEV build, or call `build_mex('Debug', true)` only with a compatible
  debug MATLAB/Octave runtime.
- A static `IccProfLib2` built with `ICC_USE_ZLIB=ON` requires zlib at MEX link
  and load time. `build_mex.m` must read the selected build's `CMakeCache.txt`,
  link the matching vcpkg import library on Windows, and stage its runtime DLL
  beside the MEX binary.
- MATLAB package `private/` directories cannot be added to the global path.
  Native-only tests must call the hidden package bridge rather than invoking
  `icc_mex` directly from `matlab/tests`.
- The handle registry is global -- calling `clear mex` invalidates all handles.
  Always close profiles explicitly before clearing MEX state.
- MATLAB's MEX compiler may differ from the system compiler.
  Use `mex -setup C++` to configure if build fails.
- Canonical workflow and troubleshooting: `docs/matlab-bindings.md`.
- Repeatable agent workflow: `.github/skills/matlab-bindings-test/SKILL.md`.
- Keep `matlab.prj`, `matlab/resources/`, and MATLAB-generated project control
  files local-only. Review staged and untracked files before pushing.
