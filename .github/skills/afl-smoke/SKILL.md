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
   ```

4. When changing AFL++ bootstrap behavior, validate the regression-container
   path with root inside the image:

   ```bash
   docker run --rm --user 0 ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master bash -lc '
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

- Keep workflow triggers manual or reusable unless maintainers explicitly widen
  them.
- Keep CI AFL++ tooling sourced from
  `https://github.com/AFLplusplus/AFLplusplus/tree/dev` and rebuilt against
  the regression container's Clang/LLVM major version.
- Do not rely on packaged `afl-clang-fast` unless the regression image rebuilds
  and probes it against the selected LLVM version.
- Keep the workflow bootstrap narrow: build `afl-fuzz`, `afl-showmap`,
  `afl-cc`, `afl-compiler-rt.o`, `SanitizerCoveragePCGUARD.so`, and
  `cmplog-routines-pass.so`. Avoid broad AFL++ targets that enter optional GCC
  plugin or Nyx paths.
- Keep the target allow-list inside `.github/scripts/iccdev-afl-smoke.sh`.
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
- Keep numeric workflow options validated by the script, not only by the
  Actions input UI.
- Pass workflow inputs through `env:` before shell use, but do not use unknown
  `AFL_*` variable names for workflow inputs because AFL++ warns on mistyped
  AFL environment variables.
- Sanitize all summary output.
- Track only tiny deterministic seeds. Do not commit AFL queues, crashes,
  hangs, coverage reports, profiler data, or generated build trees.
