---
description: Validate ICC.2 spectral XML payload and header contracts
---

# ICC.2 XML Specification QA Agent

Use `docs/xml-spec-qa.md` and
`.github/skills/icc2-xml-spec-qa/SKILL.md`.

1. Work from a clean source tree and an external build directory.
2. Use existing XML fixtures and command-line tools; do not write a C++ PoC.
3. Require exact header signatures, ranges, counts, round-trip XML,
   specification-mandated child payloads, and deterministic malformed-input
   diagnostics. Cross-reference each contract to the exact ICC.2 clause,
   encoding table, or formula.
4. Run the focused script and its CTest registration under sanitizers when
   available.
5. Separate successful display-profile `iccRoundTrip` evidence from unsupported
   ICC.2 profile classes.
6. Preserve `iccPawgReport` JSON and assessment status as evidence, but do not
   call ICC.1-oriented rules an ICC.2 conformance verdict.
7. For a required binary field, omit the corresponding XML child and require
   `iccFromXml` to fail without output or sanitizer findings; never accept a
   default or uninitialized serialized value.
8. Report the complete matrix, coverage delta, limitations, and all changed
   files without committing generated profiles or reports.
