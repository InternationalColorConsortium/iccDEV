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

| CTest | Issue | Bug | Check |
|-------|-------|-----|-------|
| `iccdev.iccconnect-config-parser` | #1662 | IccConnect CLI/JSON config parsing silently coerced malformed interpolation, intent, transform, and weight fields | Compiles a focused parser harness that verifies malformed legacy args and JSON fields fail closed |
| `iccdev.xform-abstorel-adjust` | #1662 | Relative-intent transforms that fell back to absolute AToB3 tags set `bAbsToRel` but did not apply absolute-to-relative PCS scaling | Compiles an in-memory AToB3-only profile and verifies the relative CMM output is media-white adjusted |
| `iccdev.parser-restore-calls` | Parser IO hardening | Parser call sites ignored failed cursor restore or read after failed seek | Compiles a focused `CIccIO` harness that verifies `ReadTags()` fails on restore failure and `ConnectSubProfile()` does not read after a failed seek |
| `iccdev.iccviz-gamut-roundtrip-metrics` | #1712 | iccviz `GamutVolume`/`RoundTripDE` scalar metrics and the degeneracy signal were untested on a real LUT profile | Drives `GamutVolume` + `RoundTripDE` on `sRGB_v4_ICC_preference.icc`, plus synthetic-profile checks: planar-gamut flags the `flat` predicate, a BToA signature is rejected, an overflowed (non-finite) volume fails, and neutral-axis inking yields 101 finite samples per channel |
| `iccdev.iccviz-degenerate-detection` | #1712 | `principalStdDevs` eigenvalues and the `s3 < 0.02*s1` flat predicate that flags a collapsed gamut cloud were unpinned | Header-only unit test over blob/plane/line and a rotated anisotropic cloud; calibrates the 0.02 constant and exercises the Smith-1961 trig branch (non-diagonal covariance) |
| `iccdev.iccviz-pdf-axis-labels` | #1712 | `DrawAxisPDF` axis-label and custom start/mid/end tick parameterization could regress silently | Runs `iccProfileVisualizePlot` on `sRGB_v4_ICC_preference.icc` in a freshly recreated scratch dir and asserts the emitted LUT PDF carries the axis titles plus the default (`50%`/`100%`) vs reversed (`100`/`50`) tick operators |

| Script | Issue | Bug | Check |
|--------|-------|-----|-------|
| `.github/scripts/iccdev-mluc-setter-regression-tests.sh` | #928 | `multiLocalizedUnicodeType` setters included safety terminators in serialized `mluc` string lengths | Rebuilds `sRGB_D65_MAT.icc` and `NamedColor.icc` from XML and verifies canonical `desc`/`mluc` sizes |
| `.github/scripts/iccdev-mluc-read-utf16-regression-tests.sh` | #928 follow-up | `multiLocalizedUnicodeType` reader accepted malformed record lengths, string offsets, and UTF-16 surrogate data | Mutates `sRGB_D65_MAT.icc` in `/tmp` and verifies malformed `mluc` records are rejected without sanitizer findings |
| `.github/scripts/iccdev-pcc-zero-illuminant-regression-tests.sh` | #958 | Non-standard PCC viewing-condition illuminant XYZ with zero Y divided by zero or fell back to D50 | Compiles a small PCC helper and verifies zero-Y custom illuminants are rejected without sanitizer findings or D50 substitution |
| `.github/scripts/iccdev-cam-degenerate-regression-tests.sh` | `fix-dbz-fcam-cam` | Degenerate CAM/FCAM conversion state could divide by zero or produce non-finite appearance values | Compiles a small CAM helper and verifies degenerate forward and inverse conversions complete without sanitizer findings |
| `.github/scripts/iccdev-calculator-regression-tests.sh` | `bisect-ce59fa8-calculator` | Calculator round/truncate/select casts and if/else offset arithmetic accepted malformed profile data without sanitizer-safe guards | Rebuilds `srgbCalcTest`, exercises calculator debug apply, and verifies malformed CalcTest operator fixtures reject without sanitizer findings |
| `.github/scripts/iccdev-lut16-zero-curve-regression-tests.sh` | #955 | `lut16Type` write path took `&curve[0]` for zero-entry curves, binding a reference through a null curve buffer | Compiles a small `CIccTagLut16` writer and verifies invalid table counts are rejected without sanitizer findings |
| `.github/scripts/iccdev-namedcolor-apply-regression-tests.sh` | AFL apply namedColor2 | `CIccXformNamedColor::Apply` copied more than 16 device coordinates and accepted negative lookup results | Builds a small helper that verifies valid lookup, color-not-found, and too-many-device-coordinate paths without sanitizer findings |
| `.github/scripts/iccdev-tagcomposite-recursion-depth-regression-tests.sh` | #1437 | `CIccTagArray::Read` / `CIccTagStruct::Read` had no recursion-depth cap, so deeply-nested `tagArrayType`/`tagStructType` chains exhausted the stack (CWE-674) | Compiles a `CIccMemIO` harness that verifies a shallow composite chain still reads and that depth-200000 array and struct chains are rejected by the shared `CompositeDepthGuard` without sanitizer findings |
| `.github/scripts/iccdev-v5-namedcmm-regression-tests.sh` | v5 NamedCMM bring-up | v5 non-spectral DToB/BRDFDToB and matrix/TRC fallback paths were skipped by CMM selection | Recreates compact v5 profiles in the configured test output directory and verifies `iccApplyNamedCmm` plus `iccRoundTrip` complete without sanitizer findings |
| `.github/scripts/iccdev-version-bcd-regression-tests.sh` | `bisect-version-bcd-report` | ICC header version bytes with non-BCD nibbles were decoded as decimal-looking versions such as 141.91 | Mutates a known-good ICC profile version to `0xDB91BA7B` and verifies explicit invalid BCD diagnostics plus valid-version compatibility |
| `.github/scripts/iccdev-profile-visualize-tests.sh` | `ci-mods` iccProfileVisualize bring-up | LUT visualization output could regress silently, and ASAN exposed mismatched array deallocation in generated TIFF paths | Runs `iccProfileVisualize` on `sRGB_v4_ICC_preference.icc` in a disposable directory and verifies A2B/B2A TIFF plus LUT PDF artifacts without sanitizer findings |
| `.github/scripts/iccdev-issue-1729-spectral-data-info-sig.sh` | #1729 | `Testing/Display/LCDDisplay.xml` and `LaserProjector.xml` stored their `spectralDataInfoType` tag under the unregistered `smwi` (0x736D7769) signature, so a fromXml -> dump round-trip reported it as `Unknown 'smwi'` instead of `spectralDataInfoTag` (`sdin`) | Rebuilds both tracked fixtures with `iccFromXml`, dumps each with `iccDumpProfile`, and verifies the tag is recognized as `sdin` with no residual `smwi`/`0x736D7769` -- failing on a missing fixture or any sanitizer finding |
| `.github/scripts/iccdev-issue-1730-applytolink-pseq-desc.sh` | #1730 | `iccApplyToLink`'s V4 `deviceModelDesc` block copied the model text into `psd.m_deviceMfgDesc`, clobbering the `profileSequenceDesc` manufacturer description and leaving the model description at its default; the tool still exits 0, so exit-status tests cannot see it | Derives a source profile carrying distinct `dmnd`/`dmdd` (iccToXml -> inject -> iccFromXml), builds a V4 link, and asserts each description lands in its own `pseq` slot -- failing if the model text appears in the manufacturer slot, if the injection anchor breaks, or on any sanitizer finding |
| `.github/scripts/iccdev-applyprofiles-spectral-pcs-regression.sh` | #1671, #1675, #1677, sparse PCS follow-up | Spectral PCS transforms could read or write outside pixel, matrix, or illuminant buffers when processing invalid range/channel combinations or malformed sparse matrix headers | Reconstructs the embedded PoCs, verifies fixture digests, requires #1671 and the sparse source-matrix replay to complete successfully, and requires #1675/#1677 to fail gracefully under ASAN without sanitizer findings |

## Workflow-based external compatibility checks

| Workflow | Issue | Purpose | Check |
|----------|-------|---------|-------|
| `.github/workflows/ci-pr-win.yml` | MinGW toolchain | Keep normal Windows PR CI from regressing MinGW CMake/tool support | Runs a UCRT64 MinGW Release static build and the full registered MinGW CTest set, including `iccdev.windows-icc-dump-profile-smoke` and `iccdev.iccconnect-threaded-cmm` |
| `.github/workflows/ci-pr-win.yml` | #1025, #1036 | Keep Windows ClangCL warning output focused on source signal instead of known CRT/deprecation noise | Runs the ClangCL smoke build with ClangCL-only noise-control flags, classifies warning categories, and uploads sanitized warning logs |

## Adding a new PoC

1. Minimize the crash file (smallest reproducer)
2. Name: `poc-{issue}-{short-description}.icc`
3. Add a focused script or CTest gate through `ci-iccdev-tool-tests.yml`
4. Verify: the PoC must trigger the bug on unpatched code and pass cleanly on patched code
