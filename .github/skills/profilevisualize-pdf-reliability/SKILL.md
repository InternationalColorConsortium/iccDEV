---
name: profilevisualize-pdf-reliability
description: >
  Validate ownership and output-failure propagation in the standalone legacy
  iccProfileVisualize PDF writer.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# ProfileVisualize PDF Reliability

Use this skill only for `Tools/CmdLine/IccProfileVisualize`. Read
`../../../docs/profilevisualize-pdf-reliability.md` before changing source.

1. Confirm the branch is rooted at `master` and is not a threading/performance
   stack layer.
2. Trace allocations through `PDFWriter` and require single ownership before
   insertion into the object container.
3. Require output assembly and `icWriteAndClose()` failures to reach
   `outputDataToPDF()` and a nonzero CLI status.
4. Keep rejected output targets regular-file safe and classify exit `1-127` as
   a soft failure.
5. Build the focused tool with ASAN, UBSAN, integer, and float sanitizers.
6. Run `.github/scripts/iccdev-profile-visualize-tests.sh` and the
   `iccdev/factory-bare-new` CodeQL query.
7. Keep `iccProfileVisualizePlot` out of scope unless expressly authorized.

The durable test artifact is documented in
`docs/profilevisualize-pdf-reliability.md`. Do not commit generated PDFs,
TIFFs, logs, or CodeQL databases.
