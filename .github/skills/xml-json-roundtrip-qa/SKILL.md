---
name: xml-json-roundtrip-qa
description: Measure ICC blob to XML and JSON round-trip fidelity, diagnostics, and abnormal process exits.
---

# ICC XML and JSON Round-Trip QA

Use this skill when investigating whether ICC serialization through XML or JSON
preserves bytes, validation diagnostics, and usable returned blobs.

1. Require Python 3.9+ and build `iccToXml`, `iccFromXml`, `iccToJson`, `iccFromJson`, and
   `iccDumpProfile` in a native external build directory.
2. Select an unused external evidence directory; let the runner create it.
   Keep it outside the input tree. Do not write generated blobs,
   logs, or corpus copies into the source tree.
3. Start with a bounded representative corpus:

   ```bash
   ICCDEV_TOOLS_DIR="$PWD/Build/Tools" \
     .github/scripts/iccdev-xml-json-roundtrip-qa.sh \
     --jobs 8 --max-profiles 32 ICC_PATH OUTPUT_DIR
   ```

4. Read `developer-summary.txt`, `summary.txt`, `diagnostics.tsv`, and
   `results.tsv` before individual logs. Treat normal nonzero conversion exits
   separately from timeouts, signal exits, and sanitizer findings.
5. For a changed run, pass `--compare-baseline PRIOR_OUTPUT_DIR` using completed
   schema-1 Python-runner evidence. Compare per-profile hashes as well as
   aggregate metrics. Baseline changes are informational, not a passing gate.
6. Reproduce every untriaged diagnostic, fidelity mask, or abnormal exit with
   the native tool, exact input, and exact arguments before filing or updating
   an issue.

Python directly observes POSIX signals as negative statuses. Positive native
255 is an ordinary rejection, not a signal. Failed source/returned validation
makes diagnostic fidelity `n/a`; exit zero does not establish conformance.
Read `run.json` provenance and `.log.command.json` exact replay arguments.
Use both `iccdev.xml-json-roundtrip-qa-contract` and
`iccdev.xml-json-roundtrip-qa` CTests after changing the runner. Windows can
invoke the Python implementation directly without Bash or GNU timeout.

Do not commit generated XML, JSON,
returned ICC blobs, logs, or external image corpora.
