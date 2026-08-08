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
- [ ] All three examples complete.
- [ ] `run_gamma_qa` decodes all three TRC tags as gamma 2.20703125.
- [ ] `run_docker_qa` passes when Docker is available.
- [ ] Rebuild works after `clear classes` and `clear mex`.
- [ ] Modified `.m` and Markdown files remain ASCII.

## Hosted Workflow

`.github/workflows/ci-matlab.yml` runs for MATLAB-related pull requests to
`master`, selected MATLAB QA branches, and manual dispatch. It uses read-only
permissions, trusted-base sanitizer helpers, SHA-pinned actions, a focused
dependency-free Release build, checked-in profiles, a digest-pinned container
interoperability job, and no cache or artifact publication.
