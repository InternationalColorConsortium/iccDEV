---
mode: agent
description: Review or complete standalone ProfileVisualize PDF writer reliability work
---

# ProfileVisualize PDF Reliability Handoff

Use `docs/profilevisualize-pdf-reliability.md` as the source of truth.

## Required Behavior

1. Keep this work on a standalone branch rooted at `master`; do not add it to
   the threading or performance stack.
2. Trace every `PDFObject` allocation through `PDFWriter` ownership and every
   output failure through `CloseFile()`, `outputDataToPDF()`, and the CLI exit
   status.
3. Preserve the existing regular-file output validation.
4. Update `.github/scripts/iccdev-profile-visualize-tests.sh` for any new
   failure path. A rejected output target must be a nonzero soft failure, never
   a crash or a success-shaped result.
5. Build `iccProfileVisualize` with ASAN, UBSAN, integer, and float
   sanitizers; run the focused regression and the `iccdev/factory-bare-new`
   CodeQL query.
6. Do not modify the separate `iccProfileVisualizePlot` writer unless its
   source is independently audited and explicitly in scope.

Report the branch, changed ownership/failure contracts, exact regression
artifact, CodeQL disposition, sanitizer result, and deferred work.
