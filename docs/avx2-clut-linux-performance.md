# Linux CLUT Profiling Report

## Scope

This report records the initial Linux profiling baseline for the
runtime-dispatched CLUT work on `cl-qa-timing-stats`. It is an end-to-end
CTest envelope, not an isolated AVX2 kernel benchmark. The result must not be
used to claim an ISA speedup.

## Configuration

| Field | Value |
|---|---|
| Date | 2026-08-22 |
| Stack base | `4f59ba6f` (`perf: synchronize AVX2 CLUT and threaded CMM paths`) |
| Host | WSL2, Linux 6.18.33.2 |
| Compiler | Clang 22.1.2 |
| Build | Release, `ICCDEV_ENABLE_AVX2=ON`, `ICCDEV_ENABLE_PERF_MONITORING=ON` |
| Workload | `iccdev.clut-eight-output-regression` |
| Affinity | CPU 0 |
| Repetitions | 21 |

The invocation was:

```bash
ICCDEV_CLUT_PROFILE_AFFINITY=0 \
ICCDEV_CLUT_PROFILE_RUNS=21 \
./.github/scripts/iccdev-clut-profile.sh \
  /tmp/iccdev-clut-perf-build /tmp/iccdev-clut-profile-21
```

## Results

| Metric | Value |
|---|---:|
| Median elapsed time | 0.09 s |
| P95 elapsed time | 0.11 s |
| Sample cycles | 185,147,225 |
| Sample instructions | 196,855,868 |
| Sample IPC | 1.06 |
| Sample context switches | 0 |
| Sample CPU migrations | 0 |

The focused CLUT and threaded-CMM CTests both passed. The telemetry report
also confirmed AVX2 selection for the fifteen-output fixture:

```text
clut_calls_sse2=0
clut_calls_avx2=1
clut_calls_outputs_15=1
```

## Interpretation

The syscall summary attributes 84% of traced time to `wait4`, with process
startup and fixture conversion also visible. These samples primarily measure
CTest orchestration, XML conversion, validation, and tool startup. They do
not isolate `CIccCLUT::Interp3d()` enough to evaluate AVX2 throughput.

Use the report only as a reproducibility baseline. Future optimization claims
require an interleaved scalar/SSE2 versus AVX2 kernel workload, the same CPU
affinity and compiler, bit-exact output vectors, and distribution plus hardware
counter artifacts from the profiling helper.
