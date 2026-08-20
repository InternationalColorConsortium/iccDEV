# AFL++ Smoke Fuzzing

iccDEV has a maintainer-owned AFL++ smoke workflow for short, manual checks of
command-line tool fuzzability. It is intentionally bounded and complements the
CTest, sanitizer, CodeQL, and regression-container gates.

## Review Scope

The AFL and CFL files in this area are experimental maintainer scaffolding.
They register manual/reusable workflows, helper scripts, seed handling, and
documentation so maintainers can run bounded fuzzing off `master` or an
integration branch such as `ci-afl-cfl`. They are not intended to make fuzzing a
required merge gate, replace CTest or sanitizer regression tests, or assert that
the current fuzz target set is complete.

The patch stacks under `.github/ci/fuzz-patches/afl` and
`.github/ci/fuzz-patches/cfl` are local validation aids. Reviewers should treat
them as disposable patches used to keep exploratory fuzz jobs moving while
maintainers sort and promote durable source fixes separately. A fuzz patch in
this directory is not a proposed upstream source fix by itself.

Run it from GitHub Actions with `ci-afl-smoke`, normally with:

```bash
gh workflow run ci-afl-smoke.yml \
  -f afl_targets=dump,toxml,fromxml,tojson,fromjson,roundtrip \
  -f duration_seconds=300 \
  -f exec_timeout_ms=30000 \
  -f cmake_build_type=Debug
```

The workflow runs inside the trusted regression container, then builds AFL++
from `https://github.com/AFLplusplus/AFLplusplus/tree/dev` against the
container's Clang/LLVM 22 toolchain. The job installs the matching LLVM
development headers when the published image does not already contain them,
puts the freshly built AFL++ checkout first in `PATH`, and builds iccDEV with
`afl-clang-fast`.

The regression image keeps the packaged `afl-clang-fast` wrapper usable for
short local smoke checks by installing its matching compiler runtime. For CI,
use the rebuilt wrapper path and explicitly probe it against the same LLVM
major version as `clang-22`. Known-good AFL wrapper checks compile both C and
C++ probes with:

```bash
AFL_PATH=/path/to/AFLplusplus AFL_CC=clang-22 AFL_CXX=clang++-22 \
  /path/to/AFLplusplus/afl-clang-fast -c test.c -o test.o
AFL_PATH=/path/to/AFLplusplus AFL_CC=clang-22 AFL_CXX=clang++-22 \
  /path/to/AFLplusplus/afl-clang-fast++ -c test.cpp -o testxx.o
```

The workflow intentionally builds only the AFL++ pieces needed by the smoke
job: `afl-fuzz`, `afl-showmap`, `afl-cc`, `afl-compiler-rt.o`,
`SanitizerCoveragePCGUARD.so`, and `cmplog-routines-pass.so`. The
`cmplog-routines-pass.so` file is required because current AFL++ uses it to
mark LLVM fast mode as available. Avoid broad `make source-only` or full
`GNUmakefile.llvm` builds in this workflow because they enter optional GCC
plugin or Nyx paths that are not required for iccDEV smoke fuzzing.

The selected allow-listed targets run in parallel for the requested duration.
Each `afl-fuzz` child uses AFL++ affinity fallback so three-target smoke runs
can share smaller runners instead of aborting when no free CPU core remains.
The job fails if AFL++ reports a saved crash, setup failure, missing
instrumentation, or zero executed test cases for any selected target. Generated
saved hangs are reported as `warn` rows only when Linux `core_pattern` is not
piped; otherwise saved hangs fail because AFL++ can misclassify crashes as
timeouts under piped core handlers. Hang artifacts still require separate replay
and triage before they are promoted to durable regression evidence.
Saved crashes and saved hangs are copied into an `afl-smoke-findings-<run-id>`
artifact and listed in the workflow summary whenever AFL writes testcase files.
Artifact filenames are sanitized for GitHub artifact portability; the
`manifest.tsv` file maps them back to the original AFL paths.
When findings are present, the smoke driver also runs
`.github/scripts/iccdev-fuzz-triage.sh` against the same sanitized tool build.
The triage output records one-line reproducers, logs, classifications, and a
Markdown report under the AFL work directory.

The core onboarding target names are:

- `dump`: `iccDumpProfile @@ ALL`
- `toxml`: `iccToXml @@ generated.xml`
- `fromxml`: `iccFromXml @@ generated.icc`
- `tojson`: `iccToJson @@ generated.json`
- `fromjson`: `iccFromJson @@ generated.icc`
- `roundtrip`: `iccRoundTrip @@ 1 0`
- `fromcube`: `iccFromCube @@ generated.icc`

`fromcube` remains available as a non-core compatibility target. Stabilize the
six core targets before adding more command-line tools.

Local maintainer smoke:

```bash
.github/scripts/iccdev-afl-smoke.sh --seconds 60 --targets dump --exec-timeout-ms 30000
```

The AFL and CFL smoke workflows default to applying their maintainer-local
patch stacks before building. This branch therefore tests local fuzzing fixes
before a developer promotes them to normal source PRs. Apply maintainer-local
AFL patches locally before configuring iccDEV with:

```bash
.github/scripts/iccdev-afl-smoke.sh --patches --seconds 60 --targets dump
```

The default AFL patch directory is `.github/ci/fuzz-patches/afl`. CFL uses the
parallel `.github/ci/fuzz-patches/cfl` stack through `cfl/build.sh --patches`.
Run `.github/scripts/check-fuzz-patches.sh` before committing patch-stack
edits; it validates both stacks with `git apply --check` from a fresh temporary
clone.
Both stacks are local validation aids; stable source fixes should still land as
normal source changes.

Manual AFL and CFL workflow dispatches accept `target_ref`, optional
`target_sha`, and `patch_mode`. Use `target_ref` for branch or tag validation,
`target_sha` when a replay must pin a full 40-character exact commit,
`patch_mode=all` for the maintainer patch stack, and `patch_mode=none` for raw
branch comparison.

When AFL saves crashes or hangs, the smoke workflow replays them with iccDEV
command-line tools. The uploaded findings artifact includes raw inputs,
per-finding `.cmd` replay files, replay logs, and
`triage/reproducer-one-liners.txt` / `.md` bundles for copy-paste review.

## Sanitizer Noise Suppression

AFL and CFL smoke jobs can expose sanitizer reports from three places:

- iccDEV project code, such as `IccProfLib`, `IccConnect`, `Tools`, `cfl`, or
  workflow helper code. Treat these as actionable until triage proves otherwise.
- Maintainer-local fuzz patches under `.github/ci/fuzz-patches`. Fix or retire
  the patch if the patch introduced the sanitizer report.
- Standard-library implementation code, usually from paths such as
  `*/include/c++/*/bits/...`. These can be IntegerSanitizer noise when the stack
  does not enter project-owned code before the report.

Use two different mechanisms:

- `Testing/silence.txt` is a runtime UBSAN suppression list. It is useful for
  recoverable runs where `UBSAN_OPTIONS=suppressions=$PWD/Testing/silence.txt`
  is honored by the runtime.
- `.github/ci/ubsan-ignorelist.txt` is a compile-time ignorelist. It is the
  right mechanism for fatal Clang IntegerSanitizer noise because the offending
  instrumentation must be omitted during build. GCC builds ignore this setting;
  use the normal GCC build as a regression check that suppression config does
  not leak Clang-only flags into non-Clang compilers.

When adding a suppression:

1. Reproduce the finding without a new suppression and record the exact tool
   command, exit code, and sanitizer summary.
2. Attribute the first meaningful project-owned stack frame. Do not suppress a
   project-owned frame in this noise file.
3. If the report is standard-library implementation noise, add the narrowest
   path pattern under the matching sanitizer kind.
4. For fatal Clang IntegerSanitizer builds, rebuild with:

   ```bash
   CC=clang CXX=clang++ cmake -S Build/Cmake -B build-intsan \
     -DENABLE_TOOLS=ON \
     -DENABLE_INTEGER_SANITIZER=ON \
     -DUBSAN_IGNORELIST=.github/ci/ubsan-ignorelist.txt
   cmake --build build-intsan --target iccApplyNamedCmm -j"$(nproc)"
   ```

5. Replay the same finding against the rebuilt binary. A successful noise
   suppression changes the result to normal tool behavior without hiding new
   sanitizer reports from project code.
6. Document additions and removals in the issue or PR. Include why the pattern
   is safe, the command that failed before, and the command that passed after
   rebuild.

Before opening or updating a PR, verify the suppression config under the local
compiler matrix:

```bash
CC=gcc CXX=g++ cmake -S Build/Cmake -B build-gcc-normal -DENABLE_TOOLS=ON
cmake --build build-gcc-normal --target iccApplyNamedCmm -j"$(nproc)"

CC=clang CXX=clang++ cmake -S Build/Cmake -B build-clang-normal -DENABLE_TOOLS=ON
cmake --build build-clang-normal --target iccApplyNamedCmm -j"$(nproc)"
```

For Docker parity with CI, mount the checkout and run the same smoke checks in
the regression image:

```bash
docker run --rm -v "$PWD":/work -w /work \
  ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master \
  bash -lc '
    CC=gcc CXX=g++ cmake -S Build/Cmake -B /tmp/iccdev-gcc-normal \
      -DENABLE_TOOLS=ON -DENABLE_TESTS=OFF -DENABLE_IMAGE_TOOLS=OFF \
      -DENABLE_CMM_TOOLS=ON -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=ON &&
    cmake --build /tmp/iccdev-gcc-normal --target iccApplyNamedCmm -j2 &&
    CC=clang CXX=clang++ cmake -S Build/Cmake -B /tmp/iccdev-intsan \
      -DENABLE_TOOLS=ON -DENABLE_TESTS=OFF -DENABLE_IMAGE_TOOLS=OFF \
      -DENABLE_CMM_TOOLS=ON -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=ON \
      -DENABLE_INTEGER_SANITIZER=ON \
      -DUBSAN_IGNORELIST=.github/ci/ubsan-ignorelist.txt &&
    cmake --build /tmp/iccdev-intsan --target iccApplyNamedCmm -j2
  '
```

To remove a suppression, delete the narrow pattern, rebuild the sanitizer
target, and replay a representative input. Keep the removal only when the report
does not return or when the returned report now has a project-owned fix.

Local container bootstrap check, useful before changing the workflow or the
regression image:

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

Manual options:

- `duration_seconds`: seconds per selected target, validated as `1` through
  `3600`. CFL workflow dispatch also requires `duration_seconds` multiplied by
  selected target count to stay at or below `5400` seconds so the job can finish
  inside its hosted runner timeout.
- `afl_targets`: comma-separated allow-list entries
- `exec_timeout_ms`: AFL per-exec timeout, validated as `20` through `30000`
  and defaulted to `30000` to avoid false saved-hang artifacts in longer smoke
  runs.
- `cmake_build_type`: `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`
- `target_sha`: optional full 40-character exact commit to check out
- `regression_image_tag`: trusted maintainer container tag

Use the local script for instrumentation health and seed sanity only. Long AFL
campaigns, corpus minimization, crash triage, and coverage reports remain
operator workflows outside this CI job. Promote only durable regression inputs
with exact replay commands; never commit AFL queues, crashes, hangs, generated
coverage, or profiling output.

The workflow follows the repository workflow-governance model:

- manual or reusable trigger only
- least-privilege read permissions
- SHA-pinned checkout actions
- AFL++ sourced from the upstream `dev` branch and rebuilt against the
  container LLVM major version
- 240-minute job timeout covering the accepted maximum of three 3600-second
  target runs plus build overhead; selected targets run in parallel
- hardened bash steps
- workflow inputs passed through environment variables
- sanitized `GITHUB_STEP_SUMMARY` writes
- crash and hang testcase upload through the `afl-smoke-findings-<run-id>`
  artifact

## CFL Core Smoke

The matching CFL smoke entry point is `cfl/build.sh` and the manual
`ci-cfl-smoke` workflow:

```bash
cfl/build.sh --seconds 30

gh workflow run ci-cfl-smoke.yml \
  -f cfl_targets=dump,toxml,fromxml,tojson,fromjson,roundtrip,profilevisualize,writerserialize \
  -f duration_seconds=30
```

The six command-line CFL harnesses are conservative CLI-fidelity wrappers:
libFuzzer writes each input to a temporary file and replays the matching
sanitized iccDEV tool. `profilevisualize` and `writerserialize` are in-process
harnesses. Both parse the input from memory and consume only public headers,
with the engine sources compiled as separate translation units:
`profilevisualize` stops at the `IccVizModel.hpp` data API, and
`writerserialize` carries the rendered result on into `Mini{PDF,SVG,TIFF}`,
which is where the byte-level layout and offset arithmetic live (#2116).

Both need a CLUT-bearing seed to reach the raster path at all, and the seed
caps remove the only one at their defaults — see #2120.

CFL smoke duration is wall-clock seconds per selected target, implemented with
LibFuzzer `-max_total_time`. It is not an iteration or execution count.
