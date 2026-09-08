---
name: ios-manual-examples
description: >
  Maintain the manual iOS example app CMake projects, local Xcode build
  helpers, device signing guard rails, and documentation.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Manual iOS Example Apps

Use this skill when changing `examples/ios-apply-preview/**`,
`examples/ios-benchapply/**`, or shared documentation for manual iOS example
apps.

## Required Invariants

- Keep each helper, CMake bundle variable, README command, simulator launch,
  and device launch aligned:
  - apply preview: `IccApplyPreviewPOC`,
    `ICCDEV_APPLYPREVIEW_BUNDLE_IDENTIFIER`,
    `org.color.iccdev.ApplyPreviewPOC`.
  - bench apply: `IccBenchApplyPOC`,
    `ICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER`,
    `org.color.iccdev.BenchApplyPOC`.
  - CLUT editor: `IccClutEditorPOC`,
    `ICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER`,
    `org.color.iccdev.ClutEditorPOC`.
- Build helpers must provide `simulator` and `device` targets, `--open`,
  `--run-tests`, and `--launch`.
- Simulator runs must use `simctl launch --console-pty
  --terminate-running-process` with code signing disabled.
- Device runs must use `devicectl device process launch --console
  --terminate-existing --timeout`, require `TEAM_ID` for command-line builds,
  require `DEVICE_ID` for launch, and reject placeholder team or bundle IDs
  before building.
- Unsigned simulator helpers should ignore a globally exported placeholder
  `BUNDLE_ID` and use the app's default bundle ID.
- Keep generated Xcode projects and local signing data out of the repository.
- Do not add CTest or workflow registration unless a maintainer explicitly
  requests it.

## Validation

Use the smallest complete local gate for the changed example:

```bash
git diff --check
examples/ios-apply-preview/build-ios.sh simulator --run-tests
examples/ios-benchapply/build-ios.sh simulator --run-tests
examples/ios-clut-editor/build-ios.sh simulator --run-tests
doxygen .github/ci/doxygen/Doxyfile
test ! -s docs/generated/doxygen-warnings.log
```

For helper-only signing changes, also verify placeholder rejection before any
device build starts:

```bash
TEAM_ID=ABCDE12345 examples/ios-apply-preview/build-ios.sh device
TEAM_ID=ABCDE12345 examples/ios-benchapply/build-ios.sh device
TEAM_ID=ABCDE12345 examples/ios-clut-editor/build-ios.sh device
```

Run physical-device checks with an unlocked paired device and real local
signing values when hardware is available. Do not commit or print signing
credentials beyond the non-secret team ID and bundle ID needed for the run.
