# MATLAB Bindings and QA

The `matlab/` tree provides a MATLAB/Octave MEX gateway and an object-oriented
package for profile inspection and color transforms.

## Windows Desktop Setup

Use a 64-bit Release build and the C++ compiler configured by MATLAB:

```matlab
cfg = mex.getCompilerConfigurations('C++', 'Installed');
disp({cfg.Name});
```

For Visual Studio 2022 with vcpkg dependencies:

```powershell
cmake -S Build\Cmake -B msvc -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build msvc --config Release --target IccProfLib2-static -- /m
```

Build the gateway from MATLAB:

```matlab
addpath('E:\opt\iccDEV\matlab');
build_mex('BuildDir', 'E:\opt\iccDEV\msvc');
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

To make one checkout the default for new MATLAB sessions, set the user
`MATLABPATH` environment variable to its `matlab` directory, then fully restart
MATLAB Desktop.

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

## Validation

Run the focused suite:

```matlab
addpath('E:\opt\iccDEV\matlab');
addpath('E:\opt\iccDEV\matlab\tests');
test_iccdev();
```

Run the extended local QA entry point:

```matlab
run_local_qa();
```

Run the demonstrations:

```matlab
run('E:\opt\iccDEV\matlab\examples\read_profile.m');
run('E:\opt\iccDEV\matlab\examples\color_transform.m');
run('E:\opt\iccDEV\matlab\examples\gamma_curve.m');
```

Reproduce the spectral-viewing luminance calculations from issue #1811
without a C++ build or MEX gateway:

```matlab
addpath('E:\opt\iccDEV\matlab');
addpath('E:\opt\iccDEV\matlab\tests');
test_luminance_normalization();
run('E:\opt\iccDEV\matlab\examples\luminance_normalization.m');
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
cmake --build msvc --config Release `
  --target iccLuminanceNormalizationTest
ctest --test-dir msvc -C Release `
  -R '^iccdev\.luminance-normalization$' `
  --output-on-failure --no-tests=error
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
build_mex('BuildDir', 'E:\opt\iccDEV\msvc');
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
run('E:\opt\iccDEV\matlab\examples\docker_interop.m');
```

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
