# AFL++ Smoke Fuzzing

iccDEV has a maintainer-owned AFL++ smoke workflow for short, manual checks of
command-line tool fuzzability. It is intentionally bounded and complements the
CTest, sanitizer, CodeQL, and regression-container gates.

Run it from GitHub Actions with `ci-afl-smoke`, normally with:

```bash
gh workflow run ci-afl-smoke.yml --ref ci-afl-cfl \
  -f afl_targets=dump \
  -f duration_seconds=300 \
  -f exec_timeout_ms=5000 \
  -f cmake_build_type=Debug
```

The workflow first builds AFL++ from
`https://github.com/AFLplusplus/AFLplusplus/tree/dev`, puts that checkout first
in `PATH`, then builds iccDEV with `afl-clang-fast` and runs the selected
allow-listed target for the requested duration. It fails if AFL++ reports a
crash, hang, setup failure, or missing instrumentation.

Supported target names are:

- `dump`: `iccDumpProfile @@ ALL`
- `toxml`: `iccToXml @@ generated.xml`
- `fromcube`: `iccFromCube @@ generated.icc`

Local maintainer smoke:

```bash
.github/scripts/iccdev-afl-smoke.sh --seconds 60 --targets dump --exec-timeout-ms 5000
```

Manual options:

- `duration_seconds`: seconds per selected target, validated as `1` through
  `3600`
- `afl_targets`: comma-separated allow-list entries
- `exec_timeout_ms`: AFL per-exec timeout, validated as `20` through `30000`
- `cmake_build_type`: `Debug`, `Release`, `RelWithDebInfo`, or `MinSizeRel`
- `target_sha`: optional exact commit to check out
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
- AFL++ sourced from the upstream `dev` branch
- 240-minute job timeout covering the accepted maximum of three 3600-second
  target runs plus build overhead
- hardened bash steps
- workflow inputs passed through environment variables
- sanitized `GITHUB_STEP_SUMMARY` writes
