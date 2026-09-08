---
description: Audit the ios-clut-editor app, helper, assets, docs, and review-readiness evidence
---

# iOS CLUT Editor Auditor

Audit `examples/ios-clut-editor`, its docs, and its helper scripts using
`.github/skills/ios-clut-editor/SKILL.md`.

1. Compare app code, CMake, README commands, helper script, Info.plist,
   Doxygen inputs, and asset labels as one cumulative surface.
2. Verify the profile chain, CLUT edit controls, report fields, persisted JSON
   filename, and terminal PASS/FAIL output agree.
3. Verify bundle-ID overrides propagate through CMake, helper script, README,
   simulator launch, device launch, and Mac Catalyst launch.
4. Verify simulator, device, and Mac Catalyst launch commands match existing
   fresh-launch helper behavior for their platform.
5. Verify stale report cleanup happens only for the latest accepted render,
   older background renders cannot overwrite JSON, and persistence failure
   changes the final status to FAIL.
6. Verify report and edited-image share actions are disabled while a render is
   pending, then re-enabled only for the accepted result.
7. Verify iPhone, iPad, and Mac Catalyst layouts expose every documented
   control and link; phone density changes must not regress the iPad/Mac wide
   dashboard layout.
8. Verify the profile-chain CMM interpolation is fixed unless the UI, report,
   and docs explicitly describe a combined CMM plus CLUT interpolation mode.
9. Verify the light/dark/system appearance toggle remains available for visual
   comparison.
10. Verify reused assets do not carry stale sibling-app labels.
11. Verify Doxygen includes Objective-C++ files through `*.mm` FILE_PATTERNS
   without overriding `.mm` language mapping.
12. Report findings with changed-line evidence only; do not request cloud review
   as a convergence mechanism.

Return evidence and recommendations only. Do not edit files.
