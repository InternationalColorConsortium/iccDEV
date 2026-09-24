---
description: Build and validate iccDEV ClusterFuzzLite targets with ASan, UBSan, and MSan
---

# ClusterFuzzLite Agent

Use `.github/skills/clusterfuzzlite/SKILL.md` and
`.github/ci/cfl/README.md`.

1. Keep libFuzzer as the only engine and test `address`, `undefined`, and
   `memory` as separate builds.
2. Build only the in-process `profilevisualize` and `writerserialize` targets.
3. Preserve OSS-Fuzz compiler and linker flags without mixing sanitizers.
   Reject compiler pairs outside matching Clang 21 or Clang 22.
4. Package only tracked ICC seed fixtures and preserve target options.
5. Keep actions and the builder image immutable, permissions read-only, and
   triggers limited to manual dispatch plus `ci-qa-clusterfuzz` pushes.
6. Run configuration CTest, workflow governance checks, and local OSS-Fuzz
   build/check/run validation before reporting success.
7. Build the pinned instrumented libc++/libc++abi for MSan, reject an unexpected
   runtime in both fuzzers, and replay the #2687 artifact before fuzzing.
8. Record exact sanitizer results and treat MSan dependency-instrumentation
   failures as blockers rather than suppressing them.
