# ICC.2 XML Specification QA

Measure and validate XML-to-ICC behavior for spectral PCS, bi-spectral PCS, and
MCS profiles, including specification-mandated spectral element payloads, using
`.github/skills/icc2-xml-spec-qa/SKILL.md`.

Use the native inputs documented in `docs/xml-spec-qa.md`. Run
`iccFromXml`, `iccDumpProfile -v`, `iccToXml`, `iccRoundTrip`, and
`iccPawgReport --json`; include the negative header matrix, required-child
omission fixtures, and sanitizer scan. Cross-reference expected values and
mandatory fields to the exact ICC.2 clause, table, or formula. Report exact
signatures, ranges, channel counts, validation outcomes, deterministic
diagnostics, tool capability limits, PAWG interpretation, CTest evidence, and
measured coverage. Do not create a standalone C++ PoC or commit generated
evidence artifacts.
