# Maintainer AFL++ Smoke Prompt

Use this prompt when adding or adjusting bounded AFL++ checks for iccDEV
command-line tools.

## Context

- Workflow: `.github/workflows/ci-afl-smoke.yml`
- Driver: `.github/scripts/iccdev-afl-smoke.sh`
- AFL++ source: `https://github.com/AFLplusplus/AFLplusplus/tree/dev`
- Container: `ghcr.io/internationalcolorconsortium/iccdev-ci-regression`
- Seeds: `.github/ci/test-data/` and `.github/ci/afl-seeds/`
- Governance: `.github/instructions/workflow-governance.instructions.md`
- Docs: `docs/afl-fuzzing.md`

## Task

Update the AFL smoke target, seed set, or workflow inputs while keeping the job
manual, bounded, and governance-compliant.

Keep AFL++ bootstrap aligned with the regression container's LLVM major
version. For Clang/LLVM 22, build AFL++ `dev` with `CC=clang-22`,
`CXX=clang++-22`, and `LLVM_CONFIG=llvm-config-22`, then probe both
`afl-clang-fast` and `afl-clang-fast++` with `AFL_PATH`, `AFL_CC=clang-22`, and
`AFL_CXX=clang++-22`. The current narrow CI target set is `afl-fuzz`,
`afl-showmap`, `afl-cc`, `afl-compiler-rt.o`,
`SanitizerCoveragePCGUARD.so`, and `cmplog-routines-pass.so`.

Do not use the packaged AFL++ wrapper from the regression image unless the
image has rebuilt and probed that wrapper against the same LLVM major version.
Avoid broad AFL++ `source-only` or full LLVM builds in the smoke workflow when
they enter optional GCC plugin or Nyx paths. Selected AFL targets should run in
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

## Required Validation

```bash
bash -n .github/scripts/iccdev-afl-smoke.sh
shellcheck .github/scripts/iccdev-afl-smoke.sh
actionlint .github/workflows/ci-afl-smoke.yml
yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
.github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 30000
```

Run `.github/scripts/preflight-safety-checks.sh --require-tools` before pushing
workflow changes.

When changing AFL++ bootstrap behavior, also run the regression-container
bootstrap probe documented in `docs/afl-fuzzing.md`.

## Handoff

Report the exact workflow ref, target list, duration, build type, exec timeout,
run URL, conclusion, whether AFL reported crashes or warn-level hangs, and
whether a findings artifact was uploaded. Also report whether the AFL++ wrapper
probe used the regression container or a local toolchain.
