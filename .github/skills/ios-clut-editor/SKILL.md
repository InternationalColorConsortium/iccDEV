---
name: ios-clut-editor
description: >
  Build, review, and maintain the ios-clut-editor profile and 3D CLUT editing
  proof-of-concept app without repeating prior iOS review-loop failures.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(gh:*)
---

# iOS CLUT Editor Workflow

Use this skill when changing `examples/ios-clut-editor/**`, its documentation,
or its local build helper.

## Start Gate

1. Confirm the branch is intended for branch-local grooming, not PR publication.
2. Read `AGENTS.md`, `.github/copilot-instructions.md`,
   `.github/instructions/build-system.instructions.md`, and
   `docs/governance/UPSTREAM_PR_READINESS.md`.
3. Review prior iOS examples before editing:
   `examples/ios-apply-preview`, `examples/ios-benchapply`,
   `Build/AppleMobile`, and `.github/scripts/iccdev-apple-simulator-smoke.sh`.
4. Inventory active and suppressed review findings if this branch already has a
   PR. Do not repair comment-by-comment.

## Required Invariants

- `IccClutEditorPOC`, `ICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER`, README commands,
  and `build-ios.sh` must refer to the same app and bundle ID.
- The app consumes `apple-ios-*-core`; it must not depend on desktop tools,
  CTest executables, libtiff, wxWidgets, signing files, or generated Xcode
  output.
- Keep `CMAKE_OSX_DEPLOYMENT_TARGET`, CMake Xcode attributes, helper defaults,
  and README examples aligned.
- Keep CMM and CLUT application on a background queue and update UIKit only on
  the main queue.
- Remove stale `Documents/clut-editor-report.json` before writing the current
  run's report.
- Keep simulator launch repeatable with
  `simctl launch --console-pty --terminate-running-process`.
- Keep device launch repeatable with
  `devicectl device process launch --console --terminate-existing --timeout`.
- Reused assets must not carry stale sibling-app text such as `BENCH` or
  `APPLY` into CLUT editor source art or generated icons.
- Doxygen should include the README, `.h`, and `.mm` files through INPUT and
  `*.mm` in FILE_PATTERNS; do not add `EXTENSION_MAPPING = mm=C++`.

## Validation

Use the smallest complete validation for the changed surface:

```bash
git diff --check
cmake --preset apple-ios-simulator-core -S Build/Cmake \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-simulator-core --config Release --parallel
examples/ios-clut-editor/build-ios.sh simulator --run-tests
doxygen .github/ci/doxygen/Doxyfile
test ! -s docs/generated/doxygen-warnings.log
```

For documentation-only edits, keep the Doxygen and diff checks. For code or
helper edits, run the simulator smoke unless unavailable and record the exact
skip reason.

## Stop Rules

- Do not push until the local gate covering the current diff passes.
- Do not request repeated automated reviews for convergence. If a second review
  cycle finds a new blocker, report `review-stop: FAIL - maintainer direction
  required` and stop.
- Do not add workflow or CTest registration unless a maintainer explicitly
  requests it.
