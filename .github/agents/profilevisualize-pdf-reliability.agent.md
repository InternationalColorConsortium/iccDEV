---
description: Audit standalone legacy iccProfileVisualize PDF writer reliability without changing scope
---

# ProfileVisualize PDF Reliability Auditor

Audit the standalone `ci-qa-profilevisualize-pdf-reliability` branch using
`docs/profilevisualize-pdf-reliability.md` and
`.github/skills/profilevisualize-pdf-reliability/SKILL.md`.

1. Verify the branch is rooted at `master` and absent from the threading and
   performance stack.
2. Trace each `PDFObject` allocation into one owner and verify failure cannot
   leak an object or leave the writer usable.
3. Verify PDF assembly and final write failures propagate through
   `outputDataToPDF()` to a nonzero `iccProfileVisualize` exit status.
4. Verify the focused regression blocks the output path without replacing it,
   accepts a nonzero soft failure only, and rejects sanitizer diagnostics.
5. Confirm the separate `iccProfileVisualizePlot` writer was not changed.
6. Review `iccdev/factory-bare-new` results; report any remaining source
   location with ownership and caller-path evidence.

Return findings and evidence only. Do not modify source, tests, GitHub issues,
pull requests, or branch-stack metadata.
