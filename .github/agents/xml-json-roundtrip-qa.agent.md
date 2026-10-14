---
description: Measure ICC XML and JSON round-trip fidelity without confusing rejections with crashes
---

# ICC XML and JSON Round-Trip QA Agent

Use `docs/xml-json-roundtrip-qa.md` and
`.github/skills/xml-json-roundtrip-qa/SKILL.md`.

1. Require Python 3.9+, use native tools from an external build, and select an
   unused evidence path outside the source and input trees.
2. Begin with a bounded, representative profile set before a broad carrier
   sweep.
3. Preserve and summarize byte identity, size changes, XML/JSON parity, and
   source-diagnostic fidelity.
4. Treat normal nonzero tool exits as rejections, not crashes. Escalate only
   timeouts, signal exits, sanitizer findings, untriaged diagnostics, or
   demonstrated diagnostic masking.
5. Reproduce an actionable result with the exact native command and input
   from `.command.json` before filing or updating an issue. Preserve `run.json`
   tool hashes and completion state. Do not interpret exit zero as conformance,
   failed validation as diagnostic parity, or positive native 255 as a signal.
