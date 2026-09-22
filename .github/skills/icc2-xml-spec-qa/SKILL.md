---
name: icc2-xml-spec-qa
description: Validate ICC.2 XML conversion and header relationships for spectral PCS, bi-spectral PCS, and MCS profiles with iccDumpProfile, iccRoundTrip, and iccPawgReport.
---

# ICC.2 XML Specification QA

Use this skill when changing ICC.2 header XML parsing, spectral or bi-spectral
signatures and ranges, MCS handling, or the related command-line QA surface.

## Workflow

1. Keep builds and generated evidence outside the source worktree. Do not add a
   standalone C++ PoC; use the native checked-in XML inputs and existing tools.
2. Read `docs/xml-spec-qa.md` and inspect the applicable ICC.2 clauses before
   changing an expected value.
3. Build `iccFromXml`, `iccToXml`, `iccDumpProfile`, `iccRoundTrip`, and
   `iccPawgReport` with tests enabled.
4. Run `.github/scripts/iccdev-xml-spec-qa.sh` with `ICCDEV_TOOLS_DIR` and an
   isolated `ICCDEV_TEST_OUTDIR`.
5. Require exact signatures, wavelength ranges, channel counts, XML fidelity,
   expected negative diagnostics, and no sanitizer finding.
6. Treat `iccRoundTrip` unsupported-profile results as capability evidence, not
   successful transforms. Treat ICC.2 PAWG assessment failures as review data
   until its rule set is ICC.2-aware; require JSON parse evidence regardless.
7. Discover and run `iccdev.xml-spec-qa` with CTest, then report the compiler,
   sanitizer envelope, profile inputs, complete pass count, and output path.

Do not commit generated profiles, round-trip XML, JSON reports, logs, profraw,
or coverage HTML. If a header contract changes, update the script, CTest,
coverage workflow, and this document together.
