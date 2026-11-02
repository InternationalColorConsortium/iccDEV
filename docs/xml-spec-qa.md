<!--
SPDX-FileCopyrightText: 2026 International Color Consortium
SPDX-License-Identifier: BSD-3-Clause
-->

# ICC.2 XML specification quality assurance

The focused XML specification gate converts native repository XML inputs to
ICC profiles, validates and dumps them, serializes them back to XML, and checks
ICC.2 header relationships for spectral PCS, bi-spectral PCS, and multiplex
connection space (MCS) data. It also requires specification-mandated payloads
inside spectral multi-processing elements to fail closed when omitted.

Run it against an existing tools build:

```bash
ICCDEV_TOOLS_DIR="$PWD/Build/Tools" \
  .github/scripts/iccdev-xml-spec-qa.sh
```

Python 3 is required to parse and validate the complete `iccPawgReport` JSON
document. CTest reports the focused gate as skipped with return code 77 when
the interpreter is unavailable.

Generated ICC files, PAWG JSON, and logs are transient and go to
`/tmp/iccdev-xml-spec-qa` by default. Set `ICCDEV_TEST_OUTDIR` to preserve an
isolated evidence directory. The native XML inputs are checked in under
`.github/ci/test-data/xml-spec-qa`; the script never changes them.
Each tool invocation has a 60-second default bound; set
`ICCDEV_COMMAND_TIMEOUT` to a different positive integer when needed.

## Specification and implementation cross-reference

The normative reference is
<a href="https://www.color.org/specification/ICC.2-2023.pdf">ICC.2:2023</a>.
The preceding ICC.2:2019 edition defines the same white-vector and matrix-array
fields at the same clause, table, and formula numbers. Its September 2021
cumulative errata does not amend clauses 11.2.4 through 11.2.13. The current
public CICP and extended Device Colour Space amendments do not change these
processing-element encodings, so the fail-closed contract applies throughout.

| Surface | ICC.2:2023 relationship | Native input | Required evidence |
|---------|-------------------------|-----------------|-------------------|
| Spectral PCS | Header spectral PCS and range fields, clauses 7.2.21 and 7.2.22; the signature channel count must agree with the range steps | `.github/ci/test-data/xml-spec-qa/spectral-reflectance-5.xml` | `rs0005`, 410-690 nm, five steps, no bi-spectral range |
| Bi-spectral PCS | Header bi-spectral range, clause 7.2.23; the signature represents the product of both axes | `.github/ci/test-data/xml-spec-qa/bispectral-reflectance-2x3.xml` | `bs0006`, two by three samples, reflected 420-680 nm and incident 300-500 nm |
| MCS | Header multiplex connection space, clause 7.2.24, and the Multiplex Identification profile-class requirements | `.github/ci/test-data/xml-spec-qa/mcs-3-channel-mid.xml` | `mc0003`, three multiplex channel names, `multiplexTypeArrayTag`, valid v5 profile |
| Emission CLUT white | `emissionCLUTElement`, clause 11.2.4, Table 114 and Formula (12); the reference-white emission vector is an `S`-element field following the CLUT | `.github/ci/test-data/xml-spec-qa/emission-clut-missing-white.xml` | `iccFromXml` rejects the omitted `WhiteData` with `Missing White Data` and writes no profile |
| Reflectance CLUT white | `reflectanceCLUTElement`, clause 11.2.12, Table 122 and Formula (40); the reference-white reflectance vector is an `S`-element field following the CLUT | `.github/ci/test-data/xml-spec-qa/reflectance-clut-missing-white.xml` | `iccFromXml` rejects the omitted `WhiteData` with `Missing White Data` and writes no profile |
| Emission matrix white | `emissionMatrixElement`, clause 11.2.5, Table 115 and Formula (16); the reference-white emission vector is an `S`-element field | `.github/ci/test-data/xml-spec-qa/emission-matrix-missing-white.xml` | `iccFromXml` rejects the omitted `WhiteData` with `Missing White Data` and writes no profile |
| Inverse emission matrix data | `inverseEmissionMatrixElement`, clause 11.2.8, Table 118 and Formula (29); the spectral matrix is an `S × Q`-element field | `.github/ci/test-data/xml-spec-qa/inverse-emission-matrix-missing-matrix.xml` | `iccFromXml` rejects the omitted `MatrixData` with `Missing Matrix Data` and writes no profile |
| Emission observer white | `emissionObserverElement`, clause 11.2.6, Table 116 and Formula (23); the reference-white emission vector is an `S`-element field | `.github/ci/test-data/xml-spec-qa/emission-observer-missing-white.xml` | `iccFromXml` rejects the omitted `WhiteData` with `Missing White Data` and writes no profile |
| Reflectance observer white | `reflectanceObserverElement`, clause 11.2.13, Table 123 and Formula (47); the reference-white reflectance vector is an `S`-element field | `.github/ci/test-data/xml-spec-qa/reflectance-observer-missing-white.xml` | `iccFromXml` rejects the omitted `WhiteData` with `Missing White Data` and writes no profile |
| Spectral MPE range minimum | Clauses 11.2.4-11.2.6 and 11.2.12-11.2.13 require the spectral range step count to be two or greater | `.github/ci/test-data/xml-spec-qa/emission-clut-one-spectral-step.xml` | `iccFromXml` rejects `steps="1"` with `Invalid Spectral Range` and writes no profile; the direct regression covers both CLUT and both observer readers |
| Encoded spectral range ordering | The same clauses encode range endpoints as ICC float16 values; profile validation requires the encoded start to remain below the encoded end | `.github/ci/test-data/xml-spec-qa/emission-clut-collapsed-range.xml` | `iccFromXml` rejects decimal endpoints that collapse during float16 encoding and writes no profile; the direct regression covers both CLUT, both matrix and both observer readers |

The binary field layout is represented by `icHeader` in
`IccProfLib/icProfileHeader.h`. XML parsing and serialization are implemented
in `IccXML/IccLibXML/IccProfileXml.cpp`; signature channel decoding is in
`IccProfLib/IccUtil.cpp`; cross-field validation is in
`IccProfLib/IccProfile.cpp`. These implementation references complement the
normative ICC.2 specification; they do not replace it.

The fifteen checked-in negative XML files reject non-finite wavelength endpoints,
spectral and bi-spectral step counts that do not fit the 16-bit header field,
signature/range channel mismatches, a bi-spectral range attached to a normal
spectral PCS, an MCS/channel-name mismatch, and emission or reflectance CLUTs
without their mandatory white vectors. They also reject spectral matrix and
observer elements missing a required white or matrix array, spectral MPE ranges
containing fewer than two steps, and ordered decimal endpoints that collapse
during ICC float16 encoding. This pins syntax conversion, semantic validation,
and required element payloads.

## Tool interpretation

`iccDumpProfile -v` is the conformance check used for the generated profiles.
Its normalized spectral viewing-condition warning is retained in the evidence;
the gate requires the final v5 validation result and exact header values.

`iccRoundTrip` is exercised with a generated ICC.1 RGB display control for all
four rendering intents. Its current rejection of the NamedColor and
MultiplexIdentification fixture classes is asserted as an explicit capability
boundary, not reported as a spectral or MCS transformation success.

`iccPawgReport --json` must parse every generated profile and produce a summary.
Its current local rule set applies ICC.1 assumptions to some valid ICC.2
classes and color spaces, so a report assessment exit of 1 is preserved for
review but is not used as an ICC.2 conformance verdict. A generated ICC.1
display control must still report zero PAWG failures.

## Expected ICC.1 conversion result

The reported generated `Testing/hybrid/Results/LCDDisplayCat8Obs.icc` is a
v4.30 display profile. Its expected dump therefore has `NoSpectralData`,
undefined spectral and bi-spectral ranges, and an undefined MCS. Those values
are correct for that ICC.1 conversion output; the three v5 fixtures above are
the controls that exercise the ICC.2-only header fields.

## Measured coverage

On the branch baseline, a Clang 18 source-coverage build measured the native
matrix cumulatively after the equivalent four-profile XML conversion baseline:

| Scope | Region change | Line change | Branch change |
|-------|--------------:|------------:|--------------:|
| `IccProfileXml.cpp` | 60.40% to 61.23% | 51.17% to 52.86% | 44.56% to 45.61% |
| Four IccXML implementation files | 28.24% to 28.39% | 22.16% to 22.36% | 21.30% to 21.55% |

The added matrix reaches ten new regions, twenty new lines, and twelve new
branches across `IccProfileXml.cpp`, `IccTagXml.cpp`, `IccMpeXml.cpp`, and
`IccUtilXml.cpp`. In particular, both unsigned-16 overflow diagnostics are now
executed by durable XML inputs.

The four-file totals are calculated from those named source rows. LLVM reports
duplicate inline-function records when profiles from multiple tool objects are
merged; those mismatched records are excluded and are not included in the
implementation-file deltas above.

## Corpus review and bug-hunt notes

The ICC ICS repository was reviewed for inclusion. It currently has no
bi-spectral or MCS XML profile, and its spectral profiles overlap the existing
`Testing/` corpus. Duplicating the approximately 17.8 MB hybrid CMYK profile
would not be proportionate for this focused gate; details are recorded in the
fixture README.

Sanitizer probes confirmed that non-finite spectral endpoints are rejected and
missing range children produce invalid-profile exits without a crash. A manual
reader/writer symmetry audit found that both spectral CLUT readers diagnosed a
missing `WhiteData` child but continued with a `malloc()`-allocated white
spectrum; `CIccMpeSpectralCLUT::Write()` then serialized indeterminate values.
The specification cross-check above establishes that the field is mandatory,
so the parser now rejects the omission and both native fixtures pin the CLI
diagnostic. The direct `iccdev.spectral-clut-xml-channel-mismatch` regression
also checks both reader classes under sanitizers.

The #2960 follow-up audit found that the adjacent spectral matrix and observer
readers had the same failure-
propagation defect with a different memory consequence. Their `SetSize()`
methods use `calloc()`, so an omitted required `WhiteData` or `MatrixData`
child was reported in the parser log but returned success and became an
authored all-zero array. Unpatched `iccFromXml` reported all four minimal
profiles as successfully parsed and saved, and `iccToXml` then emitted the
invented arrays. The parser now rejects both matrix siblings and both observer
siblings, with complete-element controls retained by the direct regression.
The combined ASan, UBSan, integer, and float sanitizer build stayed clean, as
expected for this specification-invalid but memory-safe zero-substitution.

The #2964 sampled-curve audit also checked the non-normative XML file-import
paths. Every `Filename=` text and binary branch in both
`CIccSampledCurveSegmentXml` and `CIccSingleSampledCurveXml` must check the
result of `SetSize()` before copying samples. The single-curve implementation
has a 65,535-sample storage limit, so a 65,536-byte UInt8 input is a compact,
deterministic refusal case. The segment class has no comparable count cap; its
allocation-failure contract is exercised on Linux by a validated allocator DSO
that refuses only `calloc(257, 4)`. CTest registers these boundary, injected,
and non-injected controls as `iccdev.xml-sampled-curve-setsize`. Sanitizer builds
still run the deterministic case and report the DSO case as skipped when their
allocator interceptors prevent `LD_PRELOAD` interposition.

A valid MCS profile carrying malformed `ProfileSubClassVersion` text (`1.bad`)
is still accepted, normalized to `0.00`, and reported valid. This is the
already-known #2387/#2384 parse-diagnostic suppression identified in
`IccProfileXml.cpp`; it is not encoded as an accepted regression by this suite.

The coverage workflow reruns the focused matrix before its broad corpus loops,
and CTest registers it as `iccdev.xml-spec-qa` on Unix-like full-tool builds.
Windows does not register bash-backed tests; its existing profile-generation,
dump, and PAWG smoke tests remain the platform-native coverage boundary.
