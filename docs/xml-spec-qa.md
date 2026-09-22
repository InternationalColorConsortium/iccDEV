# ICC.2 XML header quality assurance

The focused XML specification gate converts native repository XML inputs to
ICC profiles, validates and dumps them, serializes them back to XML, and checks
the ICC.2 header relationships for spectral PCS, bi-spectral PCS, and multiplex
connection space (MCS) data.

Run it against an existing tools build:

```bash
ICCDEV_TOOLS_DIR="$PWD/Build/Tools" \
  .github/scripts/iccdev-xml-spec-qa.sh
```

Generated ICC files, PAWG JSON, and logs are transient and go to
`/tmp/iccdev-xml-spec-qa` by default. Set `ICCDEV_TEST_OUTDIR` to preserve an
isolated evidence directory. The native XML inputs are checked in under
`.github/ci/test-data/xml-spec-qa`; the script never changes them.

## Specification and implementation cross-reference

The normative reference is
<a href="https://www.color.org/specification/ICC.2-2023.pdf">ICC.2:2023</a>.

| Surface | ICC.2:2023 relationship | Native input | Required evidence |
|---------|-------------------------|-----------------|-------------------|
| Spectral PCS | Header spectral PCS and range fields, clauses 7.2.21 and 7.2.22; the signature channel count must agree with the range steps | `.github/ci/test-data/xml-spec-qa/spectral-reflectance-5.xml` | `rs0005`, 410-690 nm, five steps, no bi-spectral range |
| Bi-spectral PCS | Header bi-spectral range, clause 7.2.23; the signature represents the product of both axes | `.github/ci/test-data/xml-spec-qa/bispectral-reflectance-2x3.xml` | `bs0006`, two by three samples, reflected 420-680 nm and incident 300-500 nm |
| MCS | Header multiplex connection space, clause 7.2.24, and the Multiplex Identification profile-class requirements | `.github/ci/test-data/xml-spec-qa/mcs-3-channel-mid.xml` | `mc0003`, three multiplex channel names, `multiplexTypeArrayTag`, valid v5 profile |

The binary field layout is represented by `icHeader` in
`IccProfLib/icProfileHeader.h`. XML parsing and serialization are implemented
in `IccXML/IccLibXML/IccProfileXml.cpp`; signature channel decoding is in
`IccProfLib/IccUtil.cpp`; cross-field validation is in
`IccProfLib/IccProfile.cpp`. These implementation references complement the
normative ICC.2 specification; they do not replace it.

The seven checked-in negative XML files reject non-finite wavelength endpoints,
spectral and bi-spectral step counts that do not fit the 16-bit header field,
signature/range channel mismatches, a bi-spectral range attached to a normal
spectral PCS, and an MCS/channel-name mismatch. This pins both syntax conversion
and semantic validation.

## Tool interpretation

`iccDumpProfile -v` is the conformance check used for the generated profiles.
Its normalized spectral viewing-condition warning is retained in the evidence;
the gate requires the final v5 validation result and exact header values.

`iccRoundTrip` is exercised with a generated ICC.1 RGB display control for all
four rendering intents. Its current rejection of Abstract, NamedColor, and
MultiplexIdentification profile classes is asserted as an explicit capability
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
missing range children produce invalid-profile exits without a crash. A valid
MCS profile carrying malformed `ProfileSubClassVersion` text (`1.bad`) is still
accepted, normalized to `0.00`, and reported valid. This is the already-known
#2387/#2384 parse-diagnostic suppression identified in `IccProfileXml.cpp`; it
is not encoded as an accepted regression by this suite.

The coverage workflow reruns the focused matrix before its broad corpus loops,
and CTest registers it as `iccdev.xml-spec-qa` on Unix-like full-tool builds.
Windows does not register bash-backed tests; its existing profile-generation,
dump, and PAWG smoke tests remain the platform-native coverage boundary.
