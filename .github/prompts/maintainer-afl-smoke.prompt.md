# Maintainer AFL++ Smoke Prompt

Use this prompt when adding or adjusting bounded AFL++ checks for iccDEV
command-line tools.

## Context

- Workflow: `.github/workflows/ci-afl-smoke.yml`
- Driver: `.github/scripts/iccdev-afl-smoke.sh`
- AFL++ source: `https://github.com/AFLplusplus/AFLplusplus/tree/dev`
- Container: `ghcr.io/internationalcolorconsortium/iccdev-ci-regression`
- Container workflow: `.github/workflows/ci-docker.yml`
- Seeds: `.github/ci/test-data/` and `.github/ci/afl-seeds/`
- Local patch stacks: `.github/ci/fuzz-patches/afl` and
  `.github/ci/fuzz-patches/cfl`
- CFL smoke: `cfl/build.sh` and `.github/workflows/ci-cfl-smoke.yml`
- Governance: `.github/instructions/workflow-governance.instructions.md`
- Docs: `docs/afl-fuzzing.md`

## Task

Update the AFL smoke target, seed set, or workflow inputs while keeping the job
manual, bounded, and governance-compliant.

Treat AFL/CFL work in this branch as experimental maintainer scaffolding. The
goal is to register manual/reusable workflows, docs, helper scripts, and local
validation patch stacks for future runs off `master` or integration branches.
Do not present the fuzz patch stack as production source hardening, and do not
turn these workflows into mandatory merge gates unless a maintainer explicitly
asks for that policy change.

Keep AFL++ bootstrap aligned with the regression container's LLVM major
version. For Clang/LLVM 22, build AFL++ `dev` with `CC=clang-22`,
`CXX=clang++-22`, and `LLVM_CONFIG=llvm-config-22`, then probe both
`afl-clang-fast` and `afl-clang-fast++` with `AFL_PATH`, `AFL_CC=clang-22`, and
`AFL_CXX=clang++-22`. The current narrow CI target set is `afl-fuzz`,
`afl-showmap`, `afl-cc`, `afl-compiler-rt.o`,
`SanitizerCoveragePCGUARD.so`, and `cmplog-routines-pass.so`.

The current core onboarding target set is
`dump,toxml,fromxml,tojson,fromjson,roundtrip`. Stabilize those targets before
adding more command-line tools.

Manual dispatches should expose the same operator controls on AFL and CFL:
`target_ref` for a branch, tag, or ref, optional `target_sha` for a full
40-character exact commit pin, and `patch_mode` with `all` or `none`. Use `all`
when validating maintainer-local patch stacks before promoting them to source
PRs, and `none` when comparing raw branch behavior.
Expose CFL runtime as seconds per target, not LibFuzzer iteration or execution
counts, so the manual UI matches AFL's duration model.

The regression image packages the compiler runtime needed by its packaged
AFL++ wrapper for short local smoke checks. The AFL workflow should still
rebuild and probe AFL++ wrappers against the selected LLVM major version. Avoid
broad AFL++ `source-only` or full LLVM builds in the smoke workflow when they
enter optional GCC plugin or Nyx paths. Selected AFL targets should run in
parallel with AFL++ affinity fallback enabled and report deterministic
per-target summaries.

Saved crashes should fail the smoke job. Generated saved hangs should be
reported as `warn` summary rows only after the job verifies Linux
`core_pattern` is not piped; otherwise saved hangs must fail because AFL++ can
misclassify crashes as timeouts under piped core handlers. Replay and triage
hang artifacts before promoting them to regression evidence. Any saved crash or
hang testcase files should be uploaded as the `afl-smoke-findings-<run-id>`
artifact and listed in the workflow summary. Uploaded filenames should be
artifact-safe, with `manifest.tsv` mapping back to the original AFL paths.
When findings exist, replay them with `.github/scripts/iccdev-fuzz-triage.sh`
and include the generated triage summary and one-line reproducers in the
handoff.

## Required Validation

```bash
bash -n .github/scripts/iccdev-afl-smoke.sh
shellcheck .github/scripts/iccdev-afl-smoke.sh
actionlint .github/workflows/ci-afl-smoke.yml
yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
.github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 30000
cfl/build.sh --targets dump,toxml,fromxml,tojson,fromjson,roundtrip --seconds 30
```

Run `.github/scripts/preflight-safety-checks.sh --require-tools` before pushing
workflow changes.

When changing AFL++ bootstrap behavior, also run the regression-container
bootstrap probe documented in `docs/afl-fuzzing.md`.

When changing fuzz patch stacks, the patch checker, the patch applicator, or
`cfl/` build behavior, verify that `.github/workflows/ci-docker.yml` still
rebuilds and tests the regression image for those paths.

## Handoff

Report the exact workflow ref, target list, duration, build type, exec timeout,
selected `target_ref`, exact checkout SHA, patch mode, run URL, conclusion,
whether AFL reported crashes or warn-level hangs, and whether findings or log
artifacts were uploaded. Also report whether the AFL++ wrapper probe used the
regression container or a local toolchain.
