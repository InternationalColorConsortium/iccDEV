# ClusterFuzzLite Maintainer Prompt

Use `.github/skills/clusterfuzzlite/SKILL.md` and
`.github/ci/cfl/README.md` to build, test, or update the official iccDEV
ClusterFuzzLite lane.

Keep libFuzzer as the engine and validate distinct `address`, `undefined`, and
`memory` builds. Preserve the in-process target boundary, tracked seed corpus,
immutable action and image pins, manual plus `ci-qa-clusterfuzz` push triggers,
matching Clang 21 or 22 compilers, and repository workflow-governance rules.
Preserve the validated total manual duration input from 2 through 45 whole
minutes, pass it to the runner as the total budget, and retain a 2-minute total
budget for branch-push runs.
For the memory build, require the pinned instrumented libc++/libc++abi, prove
both fuzzers resolve that runtime rather than libstdc++, and replay the pinned
#2687 artifact before accepting the build.

Before handoff, run the configuration CTest, workflow linters, preflight safety
checks, and the OSS-Fuzz `build_fuzzers`, `check_build`, and bounded
`run_fuzzer` commands for both targets under every sanitizer. Report exact
commands, results, skips, image digest, resolved MSan runtime, #2687 replay,
branch, and commit.
