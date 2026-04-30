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

## Adding a new PoC

1. Minimize the crash file (smallest reproducer)
2. Name: `poc-{issue}-{short-description}.icc`
3. Add a regression sub-test in `ci-iccdev-tool-tests.yml` Test 18
4. Verify: the PoC must trigger the bug on unpatched code and pass cleanly on patched code
