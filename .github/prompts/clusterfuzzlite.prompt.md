# ClusterFuzzLite Maintainer Prompt

Use `.github/skills/clusterfuzzlite/SKILL.md` and
`.github/ci/cfl/README.md` to build, test, or update the official iccDEV
ClusterFuzzLite lane.

Keep libFuzzer as the engine and validate distinct `address`, `undefined`, and
`memory` builds. Preserve the `core`, `formats`, and `assessment` in-process
target groups, matching tracked seed families, target dictionaries,
immutable action and image pins, manual plus `ci-qa-clusterfuzz` push triggers,
matching Clang 21 or 22 compilers, and repository workflow-governance rules.
Preserve the validated manual duration input from 2 through 45 whole minutes,
pass it as a separate budget for each target group, and retain a 2-minute
per-group budget for branch-push runs. Keep pruning runnable after a fuzz
finding and retain the optional manual coverage-report artifact.
For the memory build, require the pinned instrumented libc++/libc++abi and the
pinned instrumented libxml2 for the XML target. Prove every fuzzer resolves the
expected runtime rather than libstdc++, prove the XML target resolves bundled
libxml2, and replay the pinned #2687 artifact before accepting the build.
Keep temporary open-issue MSan fixes as independent patches. The default fuzz
path must attempt each patch, warn on drift or prior integration, and continue
to later patches and the build. Use strict application only for maintaining the
patch stack, and retire patches one issue at a time after normal fixes land.

Before handoff, run the configuration CTest, workflow linters, preflight safety
checks, and the OSS-Fuzz `build_fuzzers`, `check_build`, and bounded
`run_fuzzer` commands for every target under every sanitizer. Report exact
commands, results, skips, image digest, resolved MSan runtime, #2687 replay,
branch, and commit.
