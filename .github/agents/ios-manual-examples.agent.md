---
description: Audit manual iOS example app helpers, CMake projects, docs, and local-device readiness
---

# Manual iOS Example Auditor

Audit `examples/ios-apply-preview`, `examples/ios-benchapply`,
`examples/ios-clut-editor`, and their docs using
`.github/skills/ios-manual-examples/SKILL.md`.

1. Verify each helper target, CMake project name, bundle variable, default
   bundle ID, README command, simulator launch, and device launch agree.
2. Verify helpers build matching static core archives and never mix device,
   simulator, or Mac Catalyst archives.
3. Verify device helpers require real local signing inputs and reject
   placeholder team or bundle IDs before building.
4. Verify simulator helpers remain unsigned and are not blocked by stale
   exported device placeholders.
5. Verify launch commands use the repeatable `simctl` and `devicectl` flags.
6. Verify persisted report names, PASS sentinels, and documented copy paths
   agree with the app code.
7. Verify Doxygen includes the example README and Objective-C++ sources without
   generated Xcode output.
8. Report changed-line findings only; do not request cloud review as a
   convergence mechanism.

Return evidence and recommendations only. Do not edit files.
