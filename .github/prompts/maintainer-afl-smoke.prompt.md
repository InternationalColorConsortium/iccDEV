# Maintainer AFL++ Smoke Prompt

Use this prompt when adding or adjusting bounded AFL++ checks for iccDEV
command-line tools.

## Context

- Workflow: `.github/workflows/ci-afl-smoke.yml`
- Driver: `.github/scripts/iccdev-afl-smoke.sh`
- AFL++ commit: `441e2cdbd690c00174b1fa54d0b144bbc24a736c`
- Container: `ghcr.io/internationalcolorconsortium/iccdev`
- Container workflow: `.github/workflows/ci-docker.yml`
- Seeds: `.github/ci/test-data/` and `.github/ci/afl-seeds/`
- Local patch stacks: `.github/ci/fuzz-patches/afl` and
  `.github/ci/fuzz-patches/cfl`
- Patch stack validation: `.github/scripts/check-fuzz-patches.sh`
- Local CFL smoke: `.github/ci/cfl/build.sh`
- Official ClusterFuzzLite: `.github/workflows/ci-clusterfuzzlite.yml`
- Governance: `.github/instructions/workflow-governance.instructions.md`
- Docs: `docs/afl-fuzzing.md`

## Task

Update the AFL smoke target, seed set, or workflow inputs while keeping the job
manual, bounded, and governance-compliant.

Treat AFL smoke and the local CFL runner as bounded maintainer tools. The
official ClusterFuzzLite workflow is a separate manual/nightly in-process
lane. Do not present a fuzz patch stack as production source hardening or turn
fuzzing into a mandatory merge gate without maintainer direction.

Keep AFL++ bootstrap aligned with the regression container's LLVM major
version. For Clang/LLVM 22, build the pinned AFL++ commit with `CC=clang-22`,
`CXX=clang++-22`, and `LLVM_CONFIG=llvm-config-22`, then probe both
`afl-clang-fast` and `afl-clang-fast++` with `AFL_PATH`, `AFL_CC=clang-22`, and
`AFL_CXX=clang++-22`. The current narrow CI target set is `afl-fuzz`,
`afl-showmap`, `afl-cc`, `afl-compiler-rt.o`,
`SanitizerCoveragePCGUARD.so`, and `cmplog-routines-pass.so`.

Reject compiler pairs outside matching Clang 21 or Clang 22. The packaged
AFL++ wrapper is paired with 21, while the workflow rebuild is paired with 22;
do not silently fall back to an older system Clang.

Treat `AFL_TESTCACHE_SIZE` as inherited operator input. Keep `0` and values of
at least 128 MiB, reject non-numeric values, and normalize positive values
below the AFL++ 5.04a minimum to `0` with a warning so the smoke reaches the
forkserver without multiplying memory use across parallel targets.

The target sets differ by runner and are not interchangeable.

- **AFL** accepts `dump,toxml,fromxml,tojson,fromjson,roundtrip,fromcube` -- see
  the allow-list in `.github/scripts/iccdev-afl-smoke.sh`.
- **Local CFL** defaults to six command-line wrappers and eight in-process
  targets (14 total); see `.github/ci/cfl/README.md`.
- **Official ClusterFuzzLite** runs only the eight in-process targets.

`profilevisualize` and `writerserialize` are two of the in-process public-API
harnesses: compile the engine sources separately, include only public headers,
and do not include a CLI implementation or call
`processLuts()`. `writerserialize` additionally links `Mini{PDF,SVG,TIFF}.cpp`,
whose serialization entries are not reachable through `IccVizModel` alone.

Manual AFL dispatch accepts `target_ref`, optional exact 40-character
`target_sha`, and `patch_mode` (`none` by default, `all` for local patches).
The local CFL runner accepts `--targets`, seconds per target, and `--patches`;
it runs unpatched by default. Official ClusterFuzzLite uses `run_mode`,
`fuzz_minutes` per group/sanitizer pair, and `known_bug_patch_mode` (patched by
default).

The unified image packages the compiler runtime needed by its packaged
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
jactionlint --profile correctness --no-color .github/workflows/ci-afl-smoke.yml
yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
.github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 30000
```

Run the local CFL smoke when shared patch handling or CFL runner behavior
changes; use `.github/ci/cfl/README.md` for the target selection.

Run `.github/scripts/preflight-safety-checks.sh --require-tools` before pushing
workflow changes.

When changing AFL++ bootstrap behavior, also run the regression-container
bootstrap probe documented in `docs/afl-fuzzing.md`.

When changing fuzz patch stacks, the patch checker, the patch applicator, or
`.github/ci/cfl/` build behavior, manually dispatch
`.github/workflows/ci-docker.yml` from an approved publishing ref to rebuild
and test the unified image.

## Handoff

Report the exact workflow ref, target list, duration, build type, exec timeout,
selected `target_ref`, exact checkout SHA, patch mode, run URL, conclusion,
whether AFL reported crashes or warn-level hangs, and whether findings or log
artifacts were uploaded. Also report whether the AFL++ wrapper probe used the
regression container or a local toolchain.
