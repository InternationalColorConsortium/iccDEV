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
5. Verify stale report cleanup happens before each JSON write and persistence
   failure changes the final status to FAIL.
6. Verify iPhone, iPad, and Mac Catalyst layouts expose every documented
   control and link; phone density changes must not regress the iPad/Mac wide
   dashboard layout.
7. Verify the light/dark/system appearance toggle remains available for visual
   comparison.
8. Verify reused assets do not carry stale sibling-app labels.
9. Verify Doxygen includes Objective-C++ files through `*.mm` FILE_PATTERNS
   without overriding `.mm` language mapping.
10. Report findings with changed-line evidence only; do not request cloud review
   as a convergence mechanism.

Return evidence and recommendations only. Do not edit files.
