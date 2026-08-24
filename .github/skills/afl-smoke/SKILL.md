---
name: afl-smoke
description: Run or update the iccDEV AFL++ manual smoke workflow, seeds, and maintainer documentation.
---

# AFL++ Smoke Workflow

Use this skill when changing `.github/workflows/ci-afl-smoke.yml`,
`.github/scripts/iccdev-afl-smoke.sh`, or AFL smoke seed documentation.

## Local Checks

1. Validate shell syntax and style:

   ```bash
   bash -n .github/scripts/iccdev-afl-smoke.sh
   shellcheck .github/scripts/iccdev-afl-smoke.sh
   ```

2. Validate workflow syntax and governance:

   ```bash
   actionlint .github/workflows/ci-afl-smoke.yml
   yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
   .github/scripts/preflight-safety-checks.sh --require-tools
   ```

3. Run a short local AFL smoke before pushing when AFL++ is installed:

   ```bash
   .github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 30000
   cfl/build.sh --targets dump,toxml,fromxml,tojson,fromjson,roundtrip,profilevisualize,writerserialize --seconds 30
   ```

4. When changing AFL++ bootstrap behavior, validate the regression-container
   path with root inside the image:

   ```bash
   docker run --rm --user 0 ghcr.io/internationalcolorconsortium/iccdev:latest bash -lc '
   set -euo pipefail
   apt-get -o Acquire::Retries=3 -o Dpkg::Use-Pty=0 update -qq
   apt-get install -y -qq --no-install-recommends llvm-22-dev zlib1g-dev >/tmp/apt-install.log
   afl_src=$(mktemp -d)
   git clone --depth 1 --branch dev https://github.com/AFLplusplus/AFLplusplus.git "$afl_src"
   make -C "$afl_src" -j"$(nproc)" CC=clang-22 CXX=clang++-22 afl-fuzz afl-showmap
   make -C "$afl_src" -j"$(nproc)" CC=clang-22 CXX=clang++-22 LLVM_CONFIG=llvm-config-22 -f GNUmakefile.llvm \
     ./afl-cc ./afl-compiler-rt.o ./SanitizerCoveragePCGUARD.so ./cmplog-routines-pass.so
   probe=$(mktemp -d)
   printf "int main(void) { return 0; }\n" > "$probe/test.c"
   printf "int main(void) { return 0; }\n" > "$probe/test.cpp"
   AFL_PATH="$afl_src" AFL_CC=clang-22 AFL_CXX=clang++-22 "$afl_src/afl-clang-fast" -c "$probe/test.c" -o "$probe/test.o"
   AFL_PATH="$afl_src" AFL_CC=clang-22 AFL_CXX=clang++-22 "$afl_src/afl-clang-fast++" -c "$probe/test.cpp" -o "$probe/testxx.o"
   '
   ```

## Update Rules

- Treat this area as experimental maintainer scaffolding. The workflows should
  remain manual/reusable entry points for bounded fuzz checks from `master` or
  integration branches, not required merge gates.
- Do not review files under `.github/ci/fuzz-patches/afl` or
  `.github/ci/fuzz-patches/cfl` as final upstream hardening changes. They are
  local validation patches; durable fixes must be promoted separately as normal
  source changes.
- Keep workflow triggers manual or reusable unless maintainers explicitly widen
  them.
- Keep CI AFL++ tooling sourced from
  `https://github.com/AFLplusplus/AFLplusplus/tree/dev` and rebuilt against
  the unified image's Clang/LLVM major version.
- The unified image packages the compiler runtime needed by its packaged
  `afl-clang-fast` for short local smoke checks. The AFL workflow must still
  rebuild and probe AFL++ wrappers against the selected LLVM version.
- Keep the workflow bootstrap narrow: build `afl-fuzz`, `afl-showmap`,
  `afl-cc`, `afl-compiler-rt.o`, `SanitizerCoveragePCGUARD.so`, and
  `cmplog-routines-pass.so`. Avoid broad AFL++ targets that enter optional GCC
  plugin or Nyx paths.
- Keep the target allow-list inside `.github/scripts/iccdev-afl-smoke.sh`.
- Keep the two lanes' target sets distinct. AFL onboarding stays
  `dump,toxml,fromxml,tojson,fromjson,roundtrip,fromcube` (the allow-list
  above); CFL adds the two in-process targets,
  `dump,toxml,fromxml,tojson,fromjson,roundtrip,profilevisualize,writerserialize`.
  `profilevisualize` and `writerserialize` are CFL-only and are rejected by the
  AFL runner. The first six CFL
  targets remain CLI-fidelity wrappers. Build `profilevisualize` and
  `writerserialize` in-process through public headers, with the engine sources
  in separate translation units; never include a CLI implementation or call
  `processLuts()` from either harness. `writerserialize` also links
  `Mini{PDF,SVG,TIFF}.cpp` to reach the serialization entries (#2116), and
  needs a corpus that still contains a CLUT seed (#2120).
- Use `.github/ci/fuzz-patches/afl` and `.github/ci/fuzz-patches/cfl` for
  maintainer-local patch stacks when `--patches` is requested.
- Keep `ci-docker.yml` push paths and regression-image verification in sync
  with AFL/CFL patch-stack helpers so container rebuilds happen when the
  checker, applicator, smoke script, `cfl/`, or fuzz patches change.
- Run `.github/scripts/check-fuzz-patches.sh` after editing either patch stack
  so malformed hunks or stale context are caught before workflow dispatch.
- Keep manual AFL and CFL workflow inputs aligned: `target_ref` selects the
  branch, tag, or ref to check out, optional `target_sha` pins a full
  40-character exact commit, and `patch_mode` selects `all` or `none`. CFL
  duration must be expressed as seconds per target, not LibFuzzer iteration or
  execution counts.
- Keep selected AFL targets running in parallel with AFL++ affinity fallback
  enabled, and merge per-target summaries in a deterministic order.
- Treat saved crashes as smoke failures. Report generated saved hangs as
  `warn` summary rows only after verifying Linux `core_pattern` is not piped;
  otherwise fail on saved hangs because AFL++ can misclassify crashes as
  timeouts under piped core handlers. Replay and triage hang artifacts before
  promoting them to regression evidence.
- Upload any saved crash or hang testcase files as the
  `afl-smoke-findings-<run-id>` artifact and list them in the workflow summary.
  Sanitize uploaded artifact filenames and keep a manifest that records the
  original AFL paths.
- Upload smoke logs and summary TSVs as separate artifacts so maintainers can
  debug failed builds even when no crash or hang artifact was generated.
- Replay saved findings with `.github/scripts/iccdev-fuzz-triage.sh` and report
  the generated summary, logs, and one-line reproducers.
- Keep numeric workflow options validated by the script, not only by the
  Actions input UI.
- Pass workflow inputs through `env:` before shell use, but do not use unknown
  `AFL_*` variable names for workflow inputs because AFL++ warns on mistyped
  AFL environment variables.
- Sanitize all summary output.
- Track only tiny deterministic seeds. Do not commit AFL queues, crashes,
  hangs, coverage reports, profiler data, or generated build trees.
