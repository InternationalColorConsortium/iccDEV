# ICC.2 XML Specification QA

Measure and validate XML-to-ICC behavior for spectral PCS, bi-spectral PCS, and
MCS profiles using `.github/skills/icc2-xml-spec-qa/SKILL.md`.

Use the native inputs documented in `docs/xml-spec-qa.md`. Run
`iccFromXml`, `iccDumpProfile -v`, `iccToXml`, `iccRoundTrip`, and
`iccPawgReport --json`; include the negative header matrix and sanitizer scan.
Report exact signatures, ranges, channel counts, validation outcomes, tool
capability limits, PAWG interpretation, CTest evidence, and measured coverage.
Do not create a standalone C++ PoC or commit generated evidence artifacts.
