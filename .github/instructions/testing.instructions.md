---
applyTo: "Testing/**"
---

# Testing — Auto-Loaded Instructions

## Test Infrastructure

Two shell scripts drive all iccDEV testing:

| Script | Purpose | Prereq |
|--------|---------|--------|
| `Testing/CreateAllProfiles.sh` | Generate ~80 ICC/iccMAX profiles from XML | `iccFromXml` on PATH |
| `Testing/RunTests.sh` | Validate profiles with round-trip and CMM apply | All tools on PATH |
| `Testing/CreateAllProfiles.bat` | Windows equivalent | tools in PATH |
| `Testing/RunTests.bat` | Windows equivalent | tools in PATH |

Both scripts auto-source `Testing/path.sh` if present (sets `PATH` and
`LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` to the build output directory).

## Test Profile Directories

| Directory | Content | Used By |
|-----------|---------|---------|
| `Testing/Calc/` | Calculator MPE profiles (srgbCalcTest, RGBWProjector, etc.) | RunTests: CalcElement operations |
| `Testing/CalcTest/` | Calculator operator test data | RunTests: CalcElement validation |
| `Testing/Display/` | Display profiles (sRGB, Rec2020, Rec2100 HLG, GSDF) | RunTests: observer/display round-trip |
| `Testing/Encoding/` | 3-channel encoding class profiles | RunTests: encoding validation |
| `Testing/Named/` | Named color profiles (tints, spectral, fluorescence) | RunTests: NamedColor CMM tests |
| `Testing/PCC/` | Profile Connection Conditions (observers, illuminants) | RunTests: multi-observer tests |
| `Testing/SpecRef/` | Spectral reflectance profiles (argbRef, srgbRef) | RunTests: spectral round-trip |
| `Testing/HDR/` | HDR/HLG/PQ display profiles | RunTests: HDR workflows |
| `Testing/ICS/` | ICS interoperability conformance profiles | RunTests: ICS validation |
| `Testing/Overprint/` | Overprint simulation profiles | RunTests: overprint workflows |
| `Testing/CMYK-3DLUTs/` | CMYK 3D LUT profiles | RunTests: CMYK workflows |
| `Testing/hybrid/` | Hybrid spectral+colorimetric profiles | RunTests: hybrid workflows |
| `Testing/mcs/` | Material Color Space profiles | RunTests: MCS workflows |
| `Testing/ApplyDataFiles/` | Input data files for iccApplyNamedCmm | RunTests: CMM input vectors |

## Test Execution Flow

```
CreateAllProfiles.sh:
  1. For each Testing/ subdirectory (Calc, Display, Encoding, Named, PCC, SpecRef, ...)
  2. Run: iccFromXml <name>.xml <name>.icc
  3. This converts XML profile definitions into binary ICC profiles

RunTests.sh:
  1. Run iccApplyNamedCmm with Calc/srgbCalcTest.txt → validates calculator operations
  2. Run iccApplyNamedCmm with Named/NamedColorTest.txt → validates named color CMM
     - Tests multiple observer/illuminant combinations (D93/D65/D50 × 2deg/10deg)
  3. Run iccApplyNamedCmm with various encoding/spectral test vectors
  4. Run iccRoundTrip on display profiles → validates A2B/B2A round-trip
  5. Run iccDumpProfile -v → validates profile structure
```

## Adding a New Test Profile

1. Create `Testing/<Category>/<Name>.xml` — XML representation of the profile
2. Add `iccFromXml <Name>.xml <Name>.icc` line to `CreateAllProfiles.sh` under
   the appropriate category section
3. If the profile needs validation, add test commands to `RunTests.sh`:
   - `iccDumpProfile -v <Name>.icc` for structural validation
   - `iccRoundTrip <Name>.icc` for round-trip validation
   - `iccApplyNamedCmm <data>.txt ... <Name>.icc ...` for CMM tests
4. Add matching entries to `CreateAllProfiles.bat` and `RunTests.bat`

## Validation Output Parsing

`iccDumpProfile -v` validation messages use these prefixes:
- `Warning!` — Non-critical issue (profile still usable)
- `Error!` — Critical issue (profile may not work correctly)
- `NonCompliant!` — Violates ICC specification

Parse the overall status:
```bash
iccDumpProfile -v profile.icc ALL 2>&1 | grep --text -A 3 "^Validation Report"
```

## CI Integration

Test profiles are generated and validated in these CI workflows:
- `ci-pr-action.yml` — CreateAllProfiles + RunTests on every PR
- `ci-pr-unix.yml` — Same on Ubuntu matrix
- `ci-comprehensive-build-test.yml` — Full matrix (GCC, Clang, sanitizers)
- `ci-sanitizer-tests.yml` — Runs tests under ASan/UBSan/TSan/MSan

## Common Pitfalls

- `iccFromXml` must be on PATH — scripts use bare command names, not paths
- Missing `path.sh` causes `command -v iccFromXml` to fail → scripts exit 1
- Windows `.bat` scripts use different path separators and tool names
- Some XML files are includes, not standalone — `calcImport.xml` and `calcVars.xml`
  cannot be compiled individually
- `set -x` / `set -` blocks in CreateAllProfiles.sh toggle verbose output per
  category — preserve this pattern when adding new sections
