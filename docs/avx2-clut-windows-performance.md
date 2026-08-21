# Windows AVX2 CLUT Performance Report

## Scope

This report records the native Windows comparison for the opt-in 3D CLUT AVX2
path on branch `ci-qa-simd-issue-2142`, based on master `281ca19` and the
follow-up masked-tail optimization.

Environment:

- CPU: Intel Xeon w5-2465X, 16 cores / 32 logical processors
- MSVC: 19.44.35207
- ClangCL: 19.1.5
- CMake: 3.31.6
- Generator: Visual Studio 17 2022, x64
- Baseline: merge base `d529ece`

## Native Build and Correctness Matrix

The baseline and AVX2 builds used the same QA presets and the native
`RunClutEightOutputRegression.cmake` runner.

| Compiler | Mode | Baseline | AVX2 | Warnings |
|----------|------|----------|------|----------|
| MSVC | Release | PASS | PASS, SSE2 retained | 0 |
| MSVC | Debug | PASS | PASS, SSE2 retained | 0 |
| ClangCL | Release | PASS | PASS | 0 |
| ClangCL | Debug | PASS | PASS | 0 |
| MSVC ASan | Debug | PASS | PASS, SSE2 retained | 29 pre-existing deprecation warnings |
| ClangCL ASan | Debug | Runtime unsupported on this host | Runtime unsupported on this host | ASan interception failure before profile parsing |

ClangCL ASan failed with `interception_win: unhandled instruction` in the
runtime before `iccFromXml` parsed the fixture. This is a Windows sanitizer
runtime limitation on the tested host, not an iccDEV profile-processing
finding. Native MSVC ASan completed with no sanitizer finding.

## Release Throughput

The checked-in benchmark pins one thread to a configurable high-numbered
logical processor, alternates baseline and AVX2 process order, takes the median
of 11 samples, and verifies bit-exact parity for every output lane. Each sample
performs 20,000,000 in-process `CIccCLUT::Interp3d()` calls.

| Outputs | CPU 28 improvement | CPU 30 improvement | Dispatch |
|---------|-------------------:|-------------------:|----------|
| 8 | -20.27% | -20.32% | SSE2 fallback |
| 9 | -16.45% | -18.08% | SSE2 fallback |
| 10 | -16.69% | -11.88% | SSE2 fallback |
| 11 | -13.45% | -13.66% | SSE2 fallback |
| 12 | -15.50% | -20.26% | SSE2 fallback |
| 13 | -12.11% | -12.02% | SSE2 fallback |
| 14 | -10.89% | -13.45% | SSE2 fallback |
| 15 | 10.43% | 6.97% | AVX2 |
| 16 | -13.65% | -14.01% | SSE2 fallback |

Local revalidation at commit `94c0dc5` found that only 15 outputs improved on
both tested logical processors. Runtime AVX2 dispatch is therefore limited to
15 outputs. The opt-in
build still pays a per-call dispatch check on fallback counts; this is recorded
as a known cost rather than presenting those rows as AVX2 kernel results.

## Decision

- Use full AVX2 vectors plus masked AVX2 loads/stores for the final 1-7 outputs.
- Keep CPUID and XGETBV runtime checks.
- Keep AVX2 compiler flags scoped to `IccTagLutAvx2.cpp`.
- Dispatch AVX2 only for 15 outputs.
- Enable the Windows AVX2 implementation for ClangCL.
- Skip native MSVC AVX2 at configure time and retain its faster SSE2 path.
- Preserve the benchmark helper for future compiler, CPU, and kernel changes.
