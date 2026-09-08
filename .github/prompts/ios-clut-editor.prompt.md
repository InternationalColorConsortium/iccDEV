# iOS CLUT Editor Proof-of-Concept

Use this prompt when updating `examples/ios-clut-editor` or planning a related
iOS profile/CLUT editing proof of concept.

## Scope

- Keep this as a standalone Xcode iOS/iPadOS app under
  `examples/ios-clut-editor`.
- Consume the exported `apple-ios-*-core` `RefIccMAX` package.
- Reuse the existing iOS example visual shell and ICC assets where possible,
  but keep labels, bundle IDs, report names, and generated assets specific to
  the CLUT editor.
- Keep it manual-review only unless a maintainer explicitly asks for CTest or
  workflow integration.

## Contract Checklist

Before editing, compare against the existing iOS examples and Apple helpers:

1. `examples/ios-apply-preview`
2. `examples/ios-benchapply`
3. `Build/AppleMobile`
4. `.github/scripts/iccdev-apple-simulator-smoke.sh`
5. `docs/build.md`
6. `docs/governance/UPSTREAM_PR_READINESS.md`

Preserve these invariants:

- CMake, helper script, README, and launch commands use the same bundle-ID
  override.
- Core and app builds use matching SDK, architecture, configuration, and
  deployment target.
- Simulator launches use `--console-pty --terminate-running-process`.
- Device launches use `--console --terminate-existing --timeout`.
- The app removes stale JSON reports before writing new report output.
- Objective-C++ work that runs CMM or CLUT application stays off the main
  thread.
- iPhone and iPad UI both expose the documented controls and links.
- Doxygen INPUT and FILE_PATTERNS include new Objective-C++ sources without
  overriding `.mm` language handling.

## Validation

Use the smallest complete local gate for the changed surface:

```bash
git diff --check
cmake -S examples/ios-clut-editor -B out/ios-clut-editor-config-check -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-simulator-core"
examples/ios-clut-editor/build-ios.sh simulator --run-tests
doxygen .github/ci/doxygen/Doxyfile
test ! -s docs/generated/doxygen-warnings.log
```

If the core package does not exist, build it first:

```bash
cmake --preset apple-ios-simulator-core -S Build/Cmake \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-simulator-core --config Release --parallel
```

Do not use cloud review or CI as the implementation loop. If automated review
finds a blocker after a prior automated review cycle, stop and apply the
review-repair gate from `docs/governance/UPSTREAM_PR_READINESS.md`.
