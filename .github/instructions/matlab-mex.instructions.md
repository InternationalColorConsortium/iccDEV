---
applyTo: "matlab/**"
---

# MATLAB/Octave MEX Bindings -- Path-Specific Instructions

## What This Is

MATLAB/Octave interface to IccProfLib via a single MEX gateway binary.
An OOP wrapper layer in the `+iccdev` package namespace provides
MATLAB-idiomatic classes on top of the MEX function calls.

For Copilot review criteria, use the concise companion file
`matlab-code-review.instructions.md`; keep implementation conventions here.

## File Structure

```
matlab/
  mex/
    icc_mex.cpp           # MEX gateway (702 lines) -- action-string dispatch
  +iccdev/
    +qa/
      check_luminance_normalization.m  # Issue #1811 fixture calculation model
      check_colorimetry_issue_1475.m   # Issue #1475 D50 table comparison
      audit_pawg_q1.m          # PAWG Q1 metric calculation/native comparison
      bounded_grid.m           # Sample-budgeted device grid construction
      delta_e_2000.m           # Independently tested CIEDE2000 calculation
      pcs_to_lab.m             # Lab/XYZ internal PCS decoding
      path_entries.m         # Safe PATH splitting without current-dir entries
      path_contains.m        # Platform-aware PATH entry comparison
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
    colorimetry_issue_1475.m  # Explain issue #1475 D50 reduction differences
    gamma_curve.m         # curveType u8Fixed8 gamma calculation example
  build_mex.m             # Build script (auto-detects library location)
  run_gamma_qa.m          # Issue #815 gamma fixture and math verification
  add_docker_path.m       # Opt-in current-process Docker PATH helper
  tests/
    test_iccdev.m         # Test suite (MATLAB/Octave compatible)
    test_luminance_normalization.m # Dependency-free issue #1811 check
    test_colorimetry_issue_1475.m  # Dependency-free issue #1475 check
    test_pawg_q1.m             # MATLAB/native PAWG Q1 agreement
    test_usage_guidance.m      # Actionable missing-argument usage examples
    fixtures/default_usage_examples.txt # Expected identifiers and commands
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
repo_root = fileparts(fileparts(which('build_mex')));
profile_path = fullfile(repo_root, 'Testing', ...
  'sRGB_v4_ICC_preference.icc');
prof = iccdev.IccProfile(profile_path);
hdr = prof.header();
disp(hdr.versionString);
prof.close();

cmm = iccdev.IccCmm();
cmm.attach(profile_path);
cmm.attach(profile_path);
cmm.begin();
out = cmm.apply([0.5, 0.3, 0.1]);
cmm.close();
```

## Build

`docs/matlab-bindings.md` is the canonical user workflow, including portable
Windows PowerShell discovery for the checkout, MATLAB, Visual Studio, and
vcpkg. The Windows build root is `repo\msvc`. Do not publish machine-specific
absolute paths in examples. Use PowerShell for Windows commands, MATLAB
`setenv` for current-process environment changes, and reserve shell `export`
for explicitly labelled Unix-shell blocks.

### MATLAB

```matlab
cd matlab
build_mex
```

On Windows, the canonical local build root is `repo\msvc`; set
`ICCDEV_BUILD_DIR` from PowerShell or use MATLAB `setenv` before `build_mex`.
Do not present shell `export` as MATLAB or Windows syntax.

`build_mex.m` searches for `IccProfLib2-static` in these directories
(relative to its selected build root; when run from `matlab/`, the fallback
build root is `../Build`):
1. `ICCDEV_BUILD_DIR` environment variable (if set)
2. `../Build/` (fallback CMake build dir)
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
test_usage_guidance();
test_add_docker_path();
test_plot();
```

Public constructors and functions that require arguments must reject a bare
invocation with a stable `iccdev:*Required` identifier and a working example.
Keep `test_usage_guidance.m` and its fixture synchronized when adding or
renaming a public entry point.

For MATLAB-only workflow or documentation iteration, use
`.github/scripts/preflight-safety-checks.sh --fast-lane=matlab` and the focused
MATLAB tests. Do not run unrelated broad CTest suites unless native CTest
behavior is part of the change.

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

For issue #1475 and TN-06 tristimulus QA, run both the independent MATLAB model
and the native reduction-method contract:

```matlab
test_colorimetry_issue_1475();
run(fullfile(repo_root, 'matlab', 'examples', ...
  'colorimetry_issue_1475.m'));
```

```powershell
$Repo = (git rev-parse --show-toplevel).Trim()
$Build = Join-Path $Repo 'msvc'
cmake --build $Build --config Release --target iccColorimetryMethodsTest -- /m
$ColorimetryTestArgs = @(
  '--test-dir', $Build
  '-C', 'Release'
  '-R', '^iccdev\.colorimetry-methods$'
  '--output-on-failure'
  '--no-tests=error'
)
ctest @ColorimetryTestArgs
```

Shell `export` is valid only in explicitly labelled Unix shell examples. In
MATLAB use `setenv('ICCDEV_BUILD_DIR', build_dir)`; in Windows PowerShell use
`$env:ICCDEV_BUILD_DIR = $Build`.

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
- `iccdev.qa.audit_pawg_q1` requires `iccPawgReport`, `jsondecode`, and a
  Java-enabled runtime. Keep `iccPawgReport` in the minimum Release target
  list used by `docs/matlab-bindings.md`; build
  `iccPawgQ1QualityContractTest` for native agreement evidence, and verify
  `$Build\bin\Release\iccPawgReport.exe` exists before running
  `run_local_qa()`. Compare against its structured Q1 JSON metrics rather than
  parsing the human-readable detail string;
  MATLAB must calculate PCS decoding, CIEDE2000, aggregation, and verdicts
  independently rather than using native metrics as its result. Both paths use
  IccProfLib CMM transforms and must reject grids above the shared sample budget.
- `iccdev.to_json` and `iccdev.from_json` require a Java-enabled runtime plus
  the `iccToJson` and `iccFromJson` executables. Build and stage both tools
  before running `test_json_bindings`; do not add the package `private/`
  directory to MATLAB's global path to reach the process bridge.
- Windows release artifacts must retain `iccToJson.exe`, `iccFromJson.exe`,
  `iccPawgReport.exe`, `matlab/tests/test_json_bindings.m`,
  `matlab/tests/test_lut_type_range.m`,
  `matlab/+iccdev/+qa/audit_pawg_q1.m`, its three calculation helpers,
  `matlab/tests/test_pawg_q1.m`, and
  `Testing/sRGB_v4_ICC_preference.icc` plus
  `Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc`. Staged QA must put
  the bundle root on
  MATLAB's `PATH` before `run_local_qa()` so the flat packaged tool is found.
- MATLAB Desktop can start in the repository root or `matlab/`. Derive the root
  by checking for `Build/Cmake`; do not blindly append another `matlab` segment
  to `pwd`.
- `test_add_docker_path` validates the helper but does not permanently change
  MATLAB's PATH. If Docker QA is skipped, select the directory containing the
  Docker CLI, call `add_docker_path`, verify `system('docker version')`, and
  rerun Docker and local QA.
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
