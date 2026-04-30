---
applyTo: "Testing/**"
---

# Testing Instructions

## Test Infrastructure

| Script | Purpose | Prerequisite |
|--------|---------|--------------|
| `Testing/CreateAllProfiles.sh` | Generate ICC/iccMAX profiles from XML | `iccFromXml` on PATH |
| `Testing/RunTests.sh` | Validate profiles with round-trip and CMM apply | tools on PATH |
| `Testing/CreateAllProfiles.bat` | Windows equivalent | tools on PATH |
| `Testing/RunTests.bat` | Windows equivalent | tools on PATH |

`CreateAllProfiles.sh` and `RunTests.sh` auto-source `Testing/path.sh` when it
is present, which sets `PATH` and the platform library path to the build output
directory.

## CTest Gate

The standard local and CI entry point for tool testing is CTest, configured from
`Build/Cmake/Testing/CMakeLists.txt`. Build with `ENABLE_TESTS=ON` and
`ENABLE_TOOLS=ON`, then run:

```bash
ctest --test-dir build -N --no-tests=error
ctest --test-dir build --output-on-failure --no-tests=error
cmake --build build --target check
```

For Visual Studio generators, pass the configuration:

```cmd
ctest --test-dir out/vs2022-x64 -C Release -N --no-tests=error
ctest --test-dir out/vs2022-x64 -C Release --output-on-failure --no-tests=error
cmake --build out/vs2022-x64 --config Release --target check
```

Linux currently registers 14 CTest suites. Windows currently registers 2 CTest
suites through `Build/Cmake/Testing/RunWindowsBatchTest.cmake`. The Windows
wrapper runs the batch scripts from a disposable copy of `Testing/` under the
build tree and must not dirty the source `Testing/` directory.

See `docs/ctest.md` for the complete suite list, expected counts, fixtures, and
add-test process.

CTest registration and workflow count assertions are maintainer-owned. General
contributors should not edit `Build/Cmake/Testing/` or `.github/workflows/**`
unless a maintainer explicitly requests that change.

## Regression Tests

PoC profiles for automated regression testing live in `.github/ci/regression/`.
Use `.github/ci/regression/README.md` as the canonical catalog and naming guide.

Script-based gates live in `.github/scripts/`, including:

- `iccdev-json-parser-regression-tests.sh`
- `iccdev-json-cfg-tests.sh`
- `iccdev-stdobserver-regression-tests.sh`
- `iccdev-mluc-setter-regression-tests.sh`
- `iccdev-mluc-read-utf16-regression-tests.sh`
- `iccdev-namedcolor-apply-regression-tests.sh`

When adding a new regression input, add the matching script or workflow assertion
in the same change.

## Test Profile Directories

| Directory | Content | Used By |
|-----------|---------|---------|
| `Testing/Calc/` | Calculator MPE profiles | CalcElement operation tests |
| `Testing/CalcTest/` | Calculator operator test data | CalcElement validation |
| `Testing/Display/` | Display profiles | observer/display round-trip |
| `Testing/Encoding/` | 3-channel encoding profiles | encoding validation |
| `Testing/Named/` | Named color profiles | NamedColor CMM tests |
| `Testing/PCC/` | Profile Connection Conditions | multi-observer tests |
| `Testing/SpecRef/` | Spectral reflectance profiles | spectral round-trip |
| `Testing/HDR/` | HDR/HLG/PQ display profiles | HDR workflows |
| `Testing/ICS/` | ICS interoperability profiles | ICS validation |
| `Testing/Overprint/` | Overprint simulation profiles | overprint workflows |
| `Testing/CMYK-3DLUTs/` | CMYK 3D LUT profiles | CMYK workflows |
| `Testing/hybrid/` | Hybrid spectral/colorimetric profiles | hybrid workflows |
| `Testing/mcs/` | Material Color Space profiles | MCS workflows |
| `Testing/ApplyDataFiles/` | Input data for `iccApplyNamedCmm` | CMM input vectors |

## Adding a New Test Profile

1. Create `Testing/<Category>/<Name>.xml`.
2. Add the `iccFromXml <Name>.xml <Name>.icc` command to
   `CreateAllProfiles.sh` and `CreateAllProfiles.bat`.
3. Add validation commands to `RunTests.sh` and `RunTests.bat` as needed:
   `iccDumpProfile -v`, `iccRoundTrip`, or `iccApplyNamedCmm`.
4. Add regression PoCs under `.github/ci/regression/` only when they should run
   in automated CI.
5. If the profile affects automated coverage, update the relevant expected
   counts in `docs/ctest.md`, `Build/Cmake/Testing/CMakeLists.txt`, and the
   workflows that assert CTest or generated-profile totals.

## Maintainer CTest Suite Changes

This process is for iccDEV maintainers. General contributors should describe
the needed automated coverage in the issue or pull request instead of editing
CTest or workflow infrastructure directly.

1. Add the focused script under `.github/scripts/` or extend the existing
   `Testing/*.sh` and `Testing/*.bat` scripts.
2. Register the test in `Build/Cmake/Testing/CMakeLists.txt`.
3. Use `FIXTURES_REQUIRED iccdev_profiles` when the test needs generated
   profiles.
4. Update Linux CTest count assertions in `ci-tool-tests.yml` and
   `ci-iccdev-tool-tests.yml` when adding or removing Linux suites.
5. Update `docs/ctest.md` with the test name, source script, labels, and any
   count changes.

## Validation Output

`iccDumpProfile -v` validation messages use these prefixes:

- `Warning!`: non-critical issue
- `Error!`: critical issue
- `NonCompliant!`: ICC specification violation

Parse the overall status with:

```bash
iccDumpProfile -v profile.icc ALL 2>&1 | grep --text -A 3 "^Validation Report"
```
