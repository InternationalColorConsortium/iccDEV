---
name: avx2-clut-diagnostics
description: >
  Diagnose runtime-dispatched AVX2 3D CLUT interpolation, collect trace
  evidence, validate vector and masked-tail output, and prepare optimization
  handoff data.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# AVX2 CLUT Diagnostics

Use this skill for source-level AVX2 CLUT debugging and performance handoff.
Do not use diagnostic traces as throughput benchmarks.

## 1. Build the Diagnostic Configuration

On Windows:

```cmd
cmake --preset vs2022-clangcl-x64-avx2-diagnostics -S Build/Cmake -B out/vs2022-clangcl-x64-avx2-diagnostics
cmake --build out/vs2022-clangcl-x64-avx2-diagnostics --config Debug -- /m /maxcpucount
```

On Linux, configure with `-DICCDEV_ENABLE_AVX2=ON` and
`-DICC_AVX2_CLUT_DEBUG=ON`.

The default `ci-regression-checks` build does not enable
`ICCDEV_ENABLE_AVX2`; it validates the portable fallback rather than the AVX2
kernel. The Windows PR ClangCL lane enables AVX2, but use this explicit
diagnostic configuration when trace evidence is required.

## 2. Inspect the Dispatch

Set breakpoints in `IccTraceAvx2ClutDispatch()` and
`IccTraceAvx2ClutKernel()` in `IccProfLib/IccSignatureUtils.h`. Confirm AVX2
is selected only with CPU support and the measured winning output count: 15.
Record offsets, weights, vector outputs, masked outputs, and elapsed time.

## 3. Validate Behavior

```cmd
ctest --test-dir out/vs2022-clangcl-x64-avx2-diagnostics -C Debug --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
```

The eight-, nine-, eleven-, and fourteen-output fixtures pin SSE2 fallback
behavior. The fifteen-output fixture exercises a full AVX2 vector plus a
seven-output masked tail.

## 4. Measure Separately

Reconfigure with `ICC_AVX2_CLUT_DEBUG=OFF` before measuring. Preserve CPUID,
XGETBV, scalar/SSE fallback behavior, and bit-exact per-lane regression output.

Use the interleaved native benchmark for the performance decision:

```powershell
.github\scripts\iccdev-windows-clut-avx2-benchmark.ps1 `
  -BaselineBuildDir C:\path\to\baseline-build `
  -Avx2BuildDir C:\path\to\avx2-build `
  -Iterations 20000000 -Repetitions 11
```

Measure output counts 8 through 16. Windows AVX2 measurements use ClangCL;
native MSVC remains on SSE2 because its AVX2 implementation regressed.
Require `output_vector_match=True` for every result row.

## Handoff

Provide the branch and commit, target CPU, compiler, diagnostic trace summary,
debug and release validation results, measurement command, and any pending
Windows-specific investigation.

## Reference

`../../../docs/avx2-clut-diagnostics.md`
