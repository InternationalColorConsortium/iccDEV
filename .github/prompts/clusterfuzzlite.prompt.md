# ClusterFuzzLite Maintainer Prompt

Use `.github/skills/clusterfuzzlite/SKILL.md` and
`.github/ci/cfl/README.md` to build, test, or update the official iccDEV
ClusterFuzzLite lane.

Keep libFuzzer as the engine and validate distinct `address`, `undefined`, and
`memory` builds. Preserve the in-process target boundary, tracked seed corpus,
immutable action and image pins, manual plus `ci-qa-clusterfuzz` push triggers,
matching Clang 21 or 22 compilers, and repository workflow-governance rules.

Before handoff, run the configuration CTest, workflow linters, preflight safety
checks, and the OSS-Fuzz `build_fuzzers`, `check_build`, and bounded
`run_fuzzer` commands for both targets under every sanitizer. Report exact
commands, results, skips, image digest, branch, and commit.
