\page iccapply_overview iccApply Tool Lanes

This developer reference documents the command-line contracts and execution
paths for the four `iccApply*` tools. It is the Doxygen HTML landing page so
the package opens with the command-line behavior most often needed when
implementing, debugging, or testing an application path.

The generated package provides:

- Interactive SVG diagrams for each tool's argument and transform lanes.
- Source browsing with implementation comments retained.
- Call, caller, include, class, and directory graphs for the surrounding
  library code.
- Direct links between shared configuration parsers and their tool-specific
  callers.

| Tool | Primary input lane | Output |
|---|---|---|
| \ref profiles_lane "iccApplyProfiles" | TIFF and standard profile sequence | TIFF |
| \ref namedcmm_lane "iccApplyNamedCmm" | Named or numeric color data and named CMM | Legacy, JSON, or IT8 data |
| \ref search_lane "iccApplySearch" | Color data and Search CMM sequence | Legacy, JSON, or IT8 data |
| \ref tolink_lane "iccApplyToLink" | Positional link parameters and profile sequence | ICC DeviceLink or `.cube` LUT |

## Navigation

- \ref profiles_lane "iccApplyProfiles: TIFF standard-CMM lane"
- \ref namedcmm_lane "iccApplyNamedCmm: named-color and numeric-data lane"
- \ref search_lane "iccApplySearch: constrained Search-CMM lane"
- \ref tolink_lane "iccApplyToLink: sampled DeviceLink and Cube lane"

The diagrams describe control flow and argument ownership. The executable
`Usage()` output and the linked source pages remain authoritative for accepted
arguments and diagnostics.

## QA examples

The checked-in QA sources are included in the generated source browser at
`.github/ci/quality-assurance/scripts/`. For a cross-tool smoke run after
building the hybrid fixtures:

```sh
(cd Testing/hybrid && ../../../.github/ci/quality-assurance/scripts/icc_apply_qa_suite.sh --mutations 12)
```

Focused checks can run from the repository root after building the tools:

```sh
.github/ci/quality-assurance/scripts/iccApplyProfiles-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplyNamedCmm-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplySearch-quick-check.sh
.github/ci/quality-assurance/scripts/iccApplyToLink-quick-check.sh
```
