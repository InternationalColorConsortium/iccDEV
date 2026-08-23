# AVX2 CLUT Diagnostics and Optimization

Use this prompt to inspect the runtime-dispatched AVX2 3D CLUT path without
changing scalar, SSE, or AVX-512 behavior.

## Windows Debug Build

```cmd
cmake --preset vs2022-clangcl-x64-avx2-diagnostics -S Build/Cmake -B out/vs2022-clangcl-x64-avx2-diagnostics
cmake --build out/vs2022-clangcl-x64-avx2-diagnostics --config Debug -- /m /maxcpucount
ctest --test-dir out/vs2022-clangcl-x64-avx2-diagnostics -C Debug --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
```

Set source breakpoints in `IccTraceAvx2ClutDispatch()` and
`IccTraceAvx2ClutKernel()` in `IccProfLib/IccSignatureUtils.h`. Capture:

- CPU AVX2 availability and selected dispatch path.
- Output channel count, corner offsets, and interpolation weights.
- Eight-lane vector work and the seven-output masked tail for fifteen outputs.

## Optimization Rules

1. Preserve CPUID and XGETBV runtime dispatch in `CIccCLUT::Interp3d()`.
2. Keep ISA flags scoped to `IccTagLutAvx2.cpp`; never apply `/arch:AVX2`
   globally.
3. Validate the eight-, nine-, eleven-, fourteen-, and fifteen-output fixtures
   before and after each change; fifteen outputs is the focused AVX2
   masked-tail case.
4. Measure throughput only with `ICC_AVX2_CLUT_DEBUG=OFF`; trace timing is
   diagnostic evidence, not a benchmark.
5. Retain scalar/SSE fallback behavior for unsupported CPUs and output counts.
6. Do not treat the default `ci-regression-checks` result as AVX2 execution:
   it does not enable `ICCDEV_ENABLE_AVX2`. The Windows PR ClangCL lane does;
   use an explicit diagnostics build when trace evidence is required.
7. Compare all output counts from 8 through 16 with
   `.github/scripts/iccdev-windows-clut-avx2-benchmark.ps1`.
   Require `output_vector_match=True` for every row; aggregate checksums are
   not sufficient to prove per-lane parity.
8. Treat native MSVC as SSE-only unless new measurements demonstrate a
   repeatable improvement; the supported Windows AVX2 measurement compiler is
   ClangCL.
9. On Linux, configure a separate Release build with
   `-DICCDEV_ENABLE_PERF_MONITORING=ON`, then collect at least 21 pinned
   samples with `.github/scripts/iccdev-clut-profile.sh`. Preserve its timing,
   `perf stat`, syscall, and optional FlameGraph artifacts; never compare
   coverage- or trace-instrumented results with release timings.

## Handoff

Record the branch, commit, compiler, CPU model, build preset, test command,
trace summary, benchmark command, p50/p95 timing, counter artifacts, and the
bit-exact output-vector comparison in the handoff. See
`docs/avx2-clut-diagnostics.md` for the complete checklist.
