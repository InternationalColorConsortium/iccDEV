# AVX2 CLUT Diagnostics and Optimization Handoff

## Purpose

The AVX2 3D CLUT implementation is runtime-dispatched from
`CIccCLUT::Interp3d()`. `IccSignatureUtils.h` provides opt-in tracepoints for
examining that decision and the kernel inputs without affecting normal builds.

## Diagnostic Configuration

Use the Windows ClangCL diagnostics preset:

```cmd
cmake --preset vs2022-clangcl-x64-avx2-diagnostics -S Build/Cmake -B out/vs2022-clangcl-x64-avx2-diagnostics
cmake --build out/vs2022-clangcl-x64-avx2-diagnostics --config Debug -- /m /maxcpucount
ctest --test-dir out/vs2022-clangcl-x64-avx2-diagnostics -C Debug --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
```

The preset enables `ICCDEV_ENABLE_AVX2=ON` and
`ICC_AVX2_CLUT_DEBUG=ON`. The latter is compiled into IccProfLib only and must
remain disabled for release builds and throughput measurements.

## CI Coverage

`ci-regression-checks` does not set `ICCDEV_ENABLE_AVX2`; its default build
therefore validates the scalar/SSE fallback and the portable high-output
fixtures, not the AVX2 kernel. The Windows PR workflow enables AVX2 in its
ClangCL lane. Run an explicit AVX2-enabled diagnostic build and the focused
CTest when source-level dispatch evidence is required.

## Local ISA Coverage and Timing Matrix

Build portable, AVX2, and (where the host supports it) AVX-512 variants in
separate directories. The Linux Clang presets keep the compiler, Release
configuration, QA flags, dependencies, and disabled diagnostics identical
across the comparison.

```bash
cmake --preset linux-clang-clut-baseline-qa-flags \
  -S Build/Cmake -B out/clut-portable
cmake --preset linux-clang-clut-avx2-qa-flags \
  -S Build/Cmake -B out/clut-avx2
cmake --preset linux-clang-clut-avx512-qa-flags \
  -S Build/Cmake -B out/clut-avx512
cmake --build out/clut-portable --parallel "$(nproc)"
cmake --build out/clut-avx2 --parallel "$(nproc)"
cmake --build out/clut-avx512 --parallel "$(nproc)"
ICCDEV_CLUT_ISA_AFFINITY=0-3 \
  ./.github/scripts/iccdev-clut-isa-local-report.sh out/clut-report \
  out/clut-portable out/clut-avx2 out/clut-avx512
```

The generated `report.md` records build options, host information, focused test
coverage, individual samples, and median end-to-end timings. It is not a
kernel-only benchmark: profile conversion, validation, CMM setup, and process
startup are included. Use a diagnostic build to prove dispatch, and establish
an isolated kernel benchmark before claiming an ISA throughput improvement.

## Performance Telemetry and Profiler Capture

Configure a separate release build with
`-DICCDEV_ENABLE_PERF_MONITORING=ON` to collect aggregate, opt-in library
telemetry. The library remains silent unless `ICC_PERF_STATS_FILE` names a
report file. The report records CLUT calls by scalar/SSE2/AVX2/AVX-512 path,
output-channel counts, aggregate CLUT elapsed nanoseconds, and threaded-CMM
call, pixel, strip, and active-worker totals. Each process appends one report
block, so a CTest run preserves its child-process telemetry in one file.

On Linux, capture the focused regression in a fresh output directory:

```bash
ICCDEV_CLUT_PROFILE_AFFINITY=0 \
ICCDEV_CLUT_PROFILE_RUNS=21 \
ICCDEV_CLUT_PROFILE_FLAMEGRAPH=1 \
FLAMEGRAPH_DIR=/path/to/FlameGraph \
./.github/scripts/iccdev-clut-profile.sh out/clut-avx2 out/clut-profile
```

The report includes per-run wall/user/system time and RSS, median and p95
elapsed time, raw `perf stat` counters when permitted by the host, and derived
IPC, instruction-rate, branch-miss-rate, and cache-miss-rate values in
`perf-derived.tsv`. A ratio describes this end-to-end CTest envelope; it is not
a kernel-only or per-pixel value. The report also includes a `strace -f -c`
summary when available, and optional `perf.data`, folded stacks, and an SVG
flamegraph. `perf` hardware counters may be unavailable when
`kernel.perf_event_paranoid` disallows them; this is reported rather than
treated as a correctness failure. Do not use `strace`, source coverage, or
debug logging when collecting cycle or throughput comparisons.

For source coverage, configure an independent
`linux-clang-coverage` build and run the same focused CTest. Export
`llvm-profdata` and `llvm-cov` reports separately; coverage instrumentation
changes generated code and must never be compared with release timing data.
On macOS, use DTrace only for scheduler/syscall evidence on hosts where policy
permits it; use Instruments for CPU samples. On Windows, use ETW/WPA rather
than DTrace.

### Cascade Lake Linux Validation

The Linux matrix was run on an Intel Xeon Silver 4216 (Cascade Lake), which
provides AVX2 and AVX-512F/BW/DQ/VL, with Clang 21.1.8 and CMake 4.2.3. All
three Release variants passed `iccdev.clut-eight-output-regression`:

| Variant | Median end-to-end time (s) |
|---|---:|
| Portable | 0.18 |
| AVX2 | 0.15 |
| AVX-512 | 0.17 |

The separate Debug AVX2 trace confirmed that the fifteen-output fixture
selects AVX2 with eight vector lanes and a seven-output masked tail. Keep the
current runtime dispatch: these process-level timings are not an isolated
kernel benchmark and do not justify preferring AVX2 over AVX-512 for this CPU.

### Capture Dispatch Evidence

Timing builds must leave diagnostics disabled. Build a separate Debug AVX2
configuration with `-DICC_AVX2_CLUT_DEBUG=ON`, then use the checked-in fixtures
to capture dispatch and lane evidence:

```bash
iccFromXml Testing/CLUT/avx2-3d-15-output.xml /tmp/clut-fifteen.icc
iccApplyNamedCmm Testing/CLUT/avx2-3d-8-output.txt 3 0 /tmp/clut-fifteen.icc 0 \
  > /tmp/clut-fifteen.log 2>&1
grep 'AVX2 CLUT' /tmp/clut-fifteen.log
```

An AVX2-capable host reports `selected=1`, `vector_outputs=8`, and
`masked_outputs=7` for the fifteen-output fixture. The eight-, nine-, eleven-,
and fourteen-output fixtures confirm SSE2 fallback without entering the AVX2
trace helper. This demonstrates dispatch selection and lane coverage; the
logged nanoseconds are diagnostic evidence only and must not be presented as
throughput.
ICC profile fixtures support up to 15 output channels, so a 16-lane AVX-512
case requires an in-memory test surface rather than a serializable profile.

## Tracepoints

Set source breakpoints in these functions:

| Function | Evidence |
|----------|----------|
| `IccTraceAvx2ClutDispatch()` | CPU AVX2 availability, output count, selected path, CLUT data pointer, and corner offsets |
| `IccTraceAvx2ClutKernel()` | Full-vector and masked-tail output counts, elapsed nanoseconds, and interpolation weights |

The fifteen-output fixture exercises eight full-vector outputs and seven masked
outputs. All other serializable output counts retain SSE2 after sustained-load
measurements failed to show a repeatable AVX2 benefit.

## Optimization Protocol

1. Use diagnostics to establish the selected path and inputs.
2. Rebuild with `ICC_AVX2_CLUT_DEBUG=OFF` before collecting measurements.
3. Keep the AVX2 compiler flag scoped to `IccTagLutAvx2.cpp`.
4. Preserve the CPUID and XGETBV runtime checks in `CIccCLUT::Interp3d()`.
5. Run `iccdev.clut-eight-output-regression` after every implementation change.
6. Compare output values against the scalar/SSE fallback before accepting any optimization.
7. Run the interleaved native benchmark across all output counts from 8 to 16.
8. Require bit-exact output-vector parity for every lane; a rounded aggregate
   checksum can hide lane swaps and canceling errors.

```powershell
.github\scripts\iccdev-windows-clut-avx2-benchmark.ps1 `
  -BaselineBuildDir C:\path\to\baseline-build `
  -Avx2BuildDir C:\path\to\avx2-build `
  -Iterations 20000000 -Repetitions 11 `
  -AffinityCpu 30 `
  -OutputPath out\clut-avx2-benchmark.tsv
```

Every result row must report `output_vector_match=True`.

Configure the Windows comparison with the same ClangCL QA preset family so
the optional AVX2 implementation is the only intentional build difference:

```powershell
$env:VCPKG_ROOT = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg'
cmake --preset vs2022-clangcl-x64-clut-baseline-qa-flags `
  -S Build\Cmake -B out\clut-baseline
cmake --preset vs2022-clangcl-x64-avx2-qa-flags `
  -S Build\Cmake -B out\clut-avx2
cmake --build out\clut-baseline --config Release `
  --target IccProfLib2 -- /m /maxcpucount
cmake --build out\clut-avx2 --config Release `
  --target IccProfLib2 -- /m /maxcpucount
```

The Visual Studio presets run vcpkg manifest installation during CMake
configuration. A separate `vcpkg integrate install` or `vcpkg install` step is
not required. Do not compare the native MSVC preset against the ClangCL AVX2
preset when attributing a performance difference to AVX2; that also changes
the compiler. The dedicated baseline and AVX presets suppress only the
repository's existing Microsoft CRT compatibility deprecations so benchmark
build output remains actionable; normal build presets retain those warnings.

The AVX2 kernel uses full vectors plus masked loads and stores for the remaining
one through seven outputs. Dispatch is limited to 15 outputs.
Native MSVC is intentionally left on the SSE2 path:
the August 2026 Windows measurements found its AVX2 code slower. Use ClangCL
for the Windows AVX2 preset. Do not introduce FMA or a wider ISA requirement
without separate compiler flags and corresponding runtime capability checks.

## Handoff Record

Record the following with each optimization attempt:

| Item | Record |
|------|--------|
| Revision | Branch and commit SHA |
| Environment | Windows version, CPU model, compiler, and preset |
| Diagnostics | Dispatch decision, output count, vector/tail split, and trace summary |
| Correctness | Focused CTest command and scalar/SSE comparison |
| Measurement | Release command, workload, iteration count, and results |
| Decision | Accepted change, rejected experiment, or pending investigation |

The reference Windows investigation is recorded in
`avx2-clut-windows-performance.md`.
The initial Linux profiling baseline is recorded in
`avx2-clut-linux-performance.md`.

Use `.github/prompts/avx2-clut-diagnostics.prompt.md` for one-off work and
`.github/skills/avx2-clut-diagnostics/SKILL.md` for the repeatable workflow.
