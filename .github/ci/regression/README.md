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
| `iccdev.reflectance-observer-illum-range` | #1671 | Reflectance observer setup allocated temporary observer matrices from the element reflectance range instead of the applied illuminant range | Compiles a focused reflectance observer and CLUT harness that verifies mismatched illuminant ranges initialize safely and malformed one-step, zero-span, or reversed ranges fail closed |
| `iccdev.pcs-edge-metadata` | #1748 | `CIccPcsXform::ConnectFirst()` and `ConnectLast()` left the edge source/destination signatures and sample counts at their defaults, so `CIccCmm::Begin()` rejected any chain whose PCS edge space differed from the profile's own PCS | Pins the four fields on both edge builders against a Lab-PCS fixture and drives whole CMMs over the XYZ and Lab edges, checking the two PCS routes agree numerically |
| `iccdev.parser-restore-calls` | Parser IO hardening | Parser call sites ignored failed cursor restore or read after failed seek | Compiles a focused `CIccIO` harness that verifies `ReadTags()` fails on restore failure and `ConnectSubProfile()` does not read after a failed seek |
| `iccdev.iccviz-gamut-roundtrip-metrics` | #1712 | iccviz `GamutVolume`/`RoundTripDE` scalar metrics and the degeneracy signal were untested on a real LUT profile | Drives `GamutVolume` + `RoundTripDE` on `sRGB_v4_ICC_preference.icc`, plus synthetic-profile checks: planar-gamut flags the `flat` predicate, a BToA signature is rejected, an overflowed (non-finite) volume fails, and neutral-axis inking yields 101 finite samples per channel |
| `iccdev.iccviz-degenerate-detection` | #1712 | `principalStdDevs` eigenvalues and the `s3 < 0.02*s1` flat predicate that flags a collapsed gamut cloud were unpinned | Header-only unit test over blob/plane/line and a rotated anisotropic cloud; calibrates the 0.02 constant and exercises the Smith-1961 trig branch (non-diagonal covariance) |
| `iccdev.mpe-empty-identity` | #1809 | `CIccTagMultiProcessElement::Validate()` raised `icValidateWarning` for a zero-element tag with equal input and output channel counts, although `Read()`/`Write()`/`Begin()`/`Apply()` all define that shape as the identity transform; 79 reference profiles under `Testing/` (148 messages) sat in the warning cohort as a result | Validates a zero-element tag through a Lab/Lab profile and asserts the equal-channel case is `icValidateOK` reported at information level, the unequal-channel case stays a critical error, and the runtime behaviour that justifies the split (`Begin()` true / `Apply()` identity for equal channels, `Begin()` false for unequal) |
| `iccdev.mpexml-unknown-reserved` | #1886 | `CIccMpeXmlUnknown::ToXml()` formatted the `Reserved` attribute into `line` but appended `buf`, which still held the element's type signature, so the value was dropped and the signature was injected into the attribute list before the start tag closed — the emitted document did not parse; `ParseXml()` had never read `Reserved` back either | Drives the writer on an element carrying a non-zero `Reserved`, parsing the output with libxml2 rather than grepping it (the defect produced text a substring check would accept), and asserts the value survives a parse/write round-trip, an out-of-range value is refused, and a document without the attribute still reads as zero |
| `iccdev.proflib-exported-data-linkage` | #1888 | Exported IccProfLib global *variables* could not be linked from a regression executable on Windows shared builds — `WINDOWS_EXPORT_ALL_SYMBOLS` exports functions but not data, so the first test to reference `icMsgValidate*` failed with LNK2019 on the Windows leg alone | Links the exported globals through `${ICCDEV_TEST_LIB_ICCPROFLIB}` so the Windows leg fails here if that fallback is dropped, and pins eight of the nine exported globals — the four report prefixes, both D50 arrays and both solver pointers — so a change to any of them fails here rather than leaving the tests that keep hard-coded copies (`mpe-empty-identity.cpp`, `pawg-q4-xyz-pcs-decode.cpp`) quietly asserting stale text. `icInfo` is excluded: it has no definition anywhere in the tree, so referencing it would break the link on every platform, not just on Windows |
| `iccdev.pcc-reflectance-observer-range` | #1853 | `IIccProfileConnectionConditions::getReflectanceObserver()` applied the illuminant to the combined observer/range-map matrix, which is `rangeRef.steps` wide, rather than to the observer matrix, which is as wide as the illuminant SPD is long — so a reflectance range finer than the illuminant read past the end of the SPD (CWE-125) and every mapped range weighted the reflectance axis instead of the wavelength axis; `Mult()` returns a new matrix without consuming its operands, so the observer matrix also leaked on every mapped range, and `CIccProfile::calcMediaWhiteXYZ()` dereferenced the result without a NULL check | Pins the physical invariant that resampling a flat unit reflectance over the same wavelength span must not move the white point, comparing a matched grid against coarser and finer grids and against hand-computed XYZ; the finer grid runs first so a sanitizer build reports the overflow rather than exiting on the value mismatch, and the no-viewing-conditions case exercises the NULL fall-through |
| `iccdev.iccviz-pdf-axis-labels` | #1712, #1777 | `DrawAxisPDF` axis-label and custom start/mid/end tick parameterization could regress silently; the CLI also confirmed nothing on a successful run | Runs `iccProfileVisualizePlot` on `sRGB_v4_ICC_preference.icc` in a freshly recreated scratch dir and asserts the emitted LUT PDF carries the axis titles plus the default (`50%`/`100%`) vs reversed (`100`/`50`) tick operators, and that the run prints a success line naming the written PDF (#1777) |
| `iccdev.v2-legacy-pcs` | #1883 | ICC v2 encodes 16-bit PCS Lab over 0..0xff00 rather than 0..0xffff, and `CIccXform::AdjustPCS` plus the PCS step `CIccCmm::Begin()` splices onto a legacy transform select that encoding on `UseLegacyPCS()`. A clean checkout's `Testing/` corpus is 210 profiles — 208 v5 and 2 v4 — with no v2 profile at all and 0 `mft2`, 0 `mft1` tags, so every `UseLegacyPCS()==true` branch was unreachable from CI | Builds a v2 `mft2` and a v4 `mAB ` profile over identical CLUT content in memory and drives both through `CIccCmm`, asserting the neutral chroma channels come back scaled by exactly 65535/65280 and paper-white L\* expands past 1.0. Also pins the predicate per tag class (`mft2`/`ncl2` true, `mft1`/`mAB `/`mBA ` false) and that `Lab2ToLab4`/`Lab4ToLab2` are exact inverses. Needs no fixture, so a corpus change cannot silently disable it |
| `iccdev.v2-xml-fixtures` | #1883 | `Testing/**/*.icc` is gitignored, so the tracked artifact for the new v2 corpus entries is `Testing/V2/*.xml` and the profiles are generated from them by `CreateAllProfiles.sh`. Nothing pinned that those sources stay v2: a fixture that drifted to v4 would keep parsing and validating while quietly restoring the original coverage gap | Parses each fixture through the same `CIccProfileXml::LoadXml` path `iccFromXml` uses and asserts the header is still 2.x, the `AToB0Tag` still carries the type the fixture exists to provide (`mft2`, `mft1`, or none for matrix/TRC), and the profile still validates with no critical error |

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
| `.github/scripts/iccdev-issue-1781-applytolink-qa-matrix.sh` | #1781 | `CIccCLUT::Init` limits both the node count and the node count x output channels, but `iccApplyToLink`'s own cap omitted the output-channel factor, so a CMYK source at the documented maximum `lut_size` 255 passed the cap and was refused by `Init`; `Init` leaves `m_pData` NULL with `m_nNumPoints` already committed and `CIccMBB::NewCLUT` discards its result, so the writer got a NULL data pointer with a non-zero countdown and the first node wrote to address zero (SIGSEGV on both the v4 and v5 paths). Also: a zero-width or reversed input range wrote a file and exited 0 (QA cases 19/20), and a v4 link accepted a restricted range it cannot record, declaring [0,1] while sampling something else (QA case 04) | Runs the #1781 QA matrix against a CMYK source built from the tracked `CMYK-3DLUTs.xml` at `lut_size` 5. Fails on any run killed by a signal (exit 129-192, chosen so the tool's own `return -1` / 255 is not misread as a crash), on the capacity/range/v4-domain cases not being refused with their expected diagnostic, on a 3-channel source at `lut_size` 255 no longer succeeding, on `.cube` output losing `LUT_3D_INPUT_RANGE`, or on any of QA cases 01-18 changing behaviour |

## Workflow-based external compatibility checks

| Workflow | Issue | Purpose | Check |
|----------|-------|---------|-------|
| `.github/workflows/ci-pr-win.yml` | MinGW toolchain | Keep normal Windows PR CI from regressing MinGW CMake/tool support | Runs a UCRT64 MinGW Release static build and the full registered MinGW CTest set, including `iccdev.windows-icc-dump-profile-smoke` and `iccdev.iccconnect-threaded-cmm` |
| `.github/workflows/ci-pr-win.yml` | #1025, #1036 | Keep Windows ClangCL warning output focused on source signal instead of known CRT/deprecation noise | Runs the ClangCL smoke build with ClangCL-only noise-control flags, classifies warning categories, and uploads sanitized warning logs |

## Referencing exported IccProfLib globals (#1888)

A test may call any exported IccProfLib **function** freely. Referencing an
exported global **variable** is different, and gets it wrong on Windows only.

On a Windows shared build the library is `IccProfLib2.dll`, exported by CMake's
`WINDOWS_EXPORT_ALL_SYMBOLS` rather than by `__declspec` annotations — MSVC is
deliberately excluded from the `ICCPROFLIBDLL_EXPORTS` definition, a decision
taken in #764. That mechanism auto-exports functions but **not global data**, and
IccProfLib carries no `dllexport`/`dllimport` on its variables. A test compiling
with `ICCPROFLIB_API` empty therefore emits a direct data reference with no
`__imp_` indirection, and the link fails:

```
mpe-empty-identity.obj : error LNK2019: unresolved external symbol
  "char const * const icMsgValidateWarning" (?icMsgValidateWarning@@3PEBDEB)
fatal error LNK1120: 3 unresolved externals
```

Linux and macOS have no import-library model, so **a green local pre-flight will
not catch this** — only the Windows CI leg will.

The eight affected symbols:

| Symbol | Header |
|---|---|
| `g_pIccMatrixSolver`, `g_pIccMatrixInverter` | `IccProfLib/IccSolve.h` |
| `icD50XYZ[3]`, `icD50XYZxx[3]` | `IccProfLib/IccUtil.h` |
| `icMsgValidateWarning`, `icMsgValidateNonCompliant`, `icMsgValidateCriticalError`, `icMsgValidateInformation` | `IccProfLib/IccUtil.h` |

`IccUtil.h` declares a ninth, `icInfo`, but it is a **dangling declaration** —
there is no definition anywhere in the tree, so referencing it fails to link on
every platform rather than only on Windows. Nothing in-tree uses it. That is a
separate defect from the export problem described here.

Choose by what else the test links:

- **IccProfLib only** — link `${ICCDEV_TEST_LIB_ICCPROFLIB}` instead of
  `${TARGET_LIB_ICCPROFLIB}`. It resolves to the static library on Windows shared
  builds, the same fallback `iccDumpProfile`, `iccProfilePlot` and
  `wxProfileDump` already use.
- **Also links IccXML or IccJson** — you cannot use that fallback. Those
  libraries link `${TARGET_LIB_ICCPROFLIB}` `PUBLIC`, so pulling in the static
  IccProfLib as well would load the library twice in one process, once inside
  the DLL and once in the executable, giving two copies of its globals. Assert on
  literal values instead. The tests in that position today are
  `json-sparsematrix-huaf.cpp`, `xml-writer-graceful-degrade.cpp` and
  `json-bcd-version-parse.cpp`.

`mpe-empty-identity.cpp` and `pawg-q4-xyz-pcs-decode.cpp` also assert on
literals, but **not** for that reason — both link IccProfLib alone, so either
could take the fallback. They were written before `${ICCDEV_TEST_LIB_ICCPROFLIB}`
existed (#1887 landed hours ahead of #1894), which left hard-coding as the only
way past the LNK2019 at the time. `mpe-empty-identity.cpp` records a second
reason that still holds: the literal text is what a log scan greps for, so
pinning it catches a rename that reading the global would hide.

Literals copied out of the library drift silently, so
`iccdev.proflib-exported-data-linkage` pins the real globals. If it fails after a
library change, update the hard-coded copies it names.

## Adding a new PoC

1. Minimize the crash file (smallest reproducer)
2. Name: `poc-{issue}-{short-description}.icc`
3. Add a focused script or CTest gate through `ci-iccdev-tool-tests.yml`
4. Verify: the PoC must trigger the bug on unpatched code and pass cleanly on patched code
