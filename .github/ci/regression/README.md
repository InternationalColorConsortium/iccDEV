# CI Regression PoC Profiles

Minimal ICC profile proof-of-concept files used by CI regression tests.
These trigger specific fixed bugs and verify they stay fixed.

Each file is referenced by `.github/workflows/ci-iccdev-tool-tests.yml`
Test 18 (Regression Bisect).

## Profiles

| File | Issue | Bug | CWE | Tool |
|------|-------|-----|-----|------|
| `poc-599-gbd-sio.icc` | #599 | GBD signed-integer-overflow in vertex/triangle count | CWE-190 | iccDumpProfile |
| `poc-744-tonemap-hbo.icc` | #744 | ToneMapFunc Describe heap-buffer-overflow (m_params OOB) | CWE-122 | iccDumpProfile ALL |
| `poc-763-cenc-huaf.icc` | #763 | cenc profile use-after-free in AddXform | CWE-416 | iccApplyNamedCmm |
| `poc-769-*.icc` | #769 | Unsigned integer overflow in offset+size bounds checks | CWE-190 | iccDumpProfile ALL |

## Script-based regressions

| Script | Issue | Bug | Check |
|--------|-------|-----|-------|
| `.github/scripts/iccdev-mluc-setter-regression-tests.sh` | #928 | `multiLocalizedUnicodeType` setters included safety terminators in serialized `mluc` string lengths | Rebuilds `sRGB_D65_MAT.icc` and `NamedColor.icc` from XML and verifies canonical `desc`/`mluc` sizes |
| `.github/scripts/iccdev-mluc-read-utf16-regression-tests.sh` | #928 follow-up | `multiLocalizedUnicodeType` reader accepted malformed record lengths, string offsets, and UTF-16 surrogate data | Mutates `sRGB_D65_MAT.icc` in `/tmp` and verifies malformed `mluc` records are rejected without sanitizer findings |
| `.github/scripts/iccdev-pcc-zero-illuminant-regression-tests.sh` | #958 | Non-standard PCC viewing-condition illuminant XYZ with zero Y divided by zero or fell back to D50 | Compiles a small PCC helper and verifies zero-Y custom illuminants are rejected without sanitizer findings or D50 substitution |
| `.github/scripts/iccdev-calculator-regression-tests.sh` | `bisect-ce59fa8-calculator` | Calculator round/truncate/select casts and if/else offset arithmetic accepted malformed profile data without sanitizer-safe guards | Rebuilds `srgbCalcTest`, exercises calculator debug apply, and verifies malformed CalcTest operator fixtures reject without sanitizer findings |
| `.github/scripts/iccdev-lut16-zero-curve-regression-tests.sh` | #955 | `lut16Type` write path took `&curve[0]` for zero-entry curves, binding a reference through a null curve buffer | Compiles a small `CIccTagLut16` writer and verifies invalid table counts are rejected without sanitizer findings |
| `.github/scripts/iccdev-namedcolor-apply-regression-tests.sh` | AFL apply namedColor2 | `CIccXformNamedColor::Apply` copied more than 16 device coordinates and accepted negative lookup results | Builds a small helper that verifies valid lookup, color-not-found, and too-many-device-coordinate paths without sanitizer findings |
| `.github/scripts/iccdev-v5-namedcmm-regression-tests.sh` | v5 NamedCMM bring-up | v5 non-spectral DToB/BRDFDToB and matrix/TRC fallback paths were skipped by CMM selection | Recreates compact v5 profiles in the configured test output directory and verifies `iccApplyNamedCmm` plus `iccRoundTrip` complete without sanitizer findings |
| `.github/scripts/iccdev-version-bcd-regression-tests.sh` | `bisect-version-bcd-report` | ICC header version bytes with non-BCD nibbles were decoded as decimal-looking versions such as 141.91 | Mutates a known-good ICC profile version to `0xDB91BA7B` and verifies explicit invalid BCD diagnostics plus valid-version compatibility |

## Workflow-based external compatibility checks

| Workflow | Issue | Purpose | Check |
|----------|-------|---------|-------|
| `.github/workflows/ci-issue-987-unicolour.yml` | #987 | Compare iccDEV issue #987 FOGRA39 handling with the reporter's likely Unicolour context and MinGW native toolchain | Runs the fixed `waacton/Unicolour` `IccProfileTests.Fogra39` test on Windows, builds `IccProfLib2` and `iccDumpProfile` with UCRT64 MinGW across Debug/Release/RelWithDebInfo and shared/static linkage, runs `iccdev.windows-icc-dump-profile-smoke` for every matrix row, runs `iccdev.issue-987-shared-mpe-export` for shared builds, and captures `iccDumpProfile --read --diag` output for the attached FOGRA39 profile |
| `.github/workflows/ci-pr-win.yml` | MinGW toolchain | Keep normal Windows PR CI from regressing MinGW CMake/tool support | Runs a UCRT64 MinGW Release static build and the full registered MinGW CTest set, including `iccdev.windows-icc-dump-profile-smoke` and `iccdev.iccconnect-threaded-cmm` |

## Adding a new PoC

1. Minimize the crash file (smallest reproducer)
2. Name: `poc-{issue}-{short-description}.icc`
3. Add a regression sub-test in `ci-iccdev-tool-tests.yml` Test 18
4. Verify: the PoC must trigger the bug on unpatched code and pass cleanly on patched code
