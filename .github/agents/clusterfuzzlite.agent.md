---
description: Build and validate iccDEV ClusterFuzzLite targets with ASan, UBSan, and MSan
---

# ClusterFuzzLite Agent

Use `.github/skills/clusterfuzzlite/SKILL.md` and
`.github/ci/cfl/README.md`.

1. Keep libFuzzer as the only engine and test `address`, `undefined`, and
   `memory` as separate builds.
2. Build the `core`, `formats`, and `assessment` in-process target groups.
   Keep fork/exec CLI wrappers in the local smoke lane.
3. Preserve OSS-Fuzz compiler and linker flags without mixing sanitizers.
   Reject compiler pairs outside matching Clang 21 or Clang 22.
4. Package only tracked ICC/XML/JSON seed fixtures by matching target family,
   and preserve target options and dictionaries. Keep a schema-shaped
   IccConnect seed and dedicated configuration dictionary.
5. Keep actions and the builder image immutable, permissions read-only, and
   triggers limited to manual dispatch plus `ci-qa-clusterfuzz` pushes.
6. Run configuration CTest, workflow governance checks, and local OSS-Fuzz
   build/check/run validation before reporting success.
7. Build the pinned instrumented libc++/libc++abi for MSan and pinned
   instrumented libxml2 for the XML target. Reject unexpected runtime
   boundaries in every fuzzer and replay the #2687 artifact before fuzzing.
8. Preserve separate per-group fuzz budgets, fail-independent corpus pruning,
   and the optional manual coverage-report artifact.
9. Record exact sanitizer results and treat MSan dependency-instrumentation
   failures as blockers rather than suppressing them.
10. Keep temporary MSan workarounds in individual issue patches. Default fuzz
    builds warn and continue past drifted or integrated patches; use strict mode
    only when maintaining the patch inventory.
11. Keep CMM controls outside the ICC profile-size header, exercise top-level
    and nested config round trips, and require complete ICC license blocks in
    every C/C++ harness source.
12. Strengthen existing targets before adding multi-profile CMM, serializer,
    or V5 display-observer targets. Require an instrumented MSan boundary before
    adding image or carrier dependencies.
