---
name: code-coverage-analysis
description: Review or update iccDEV Clang source coverage, deterministic profraw merging, coverage workflow signal, and local build-configuration timing variance.
---

# Code Coverage Analysis

Use this skill for `ci-code-coverage.yml`, LLVM profraw/profdata handling,
coverage drift, or local timing comparisons across build configurations.

## Workflow

1. Read `docs/code-coverage.md` and keep coverage separate from sanitizer
   reproduction and throughput benchmarking.
2. Record the source SHA, compiler, runner or host, logical CPUs, fixed job
   count, memory, profile count, profile-signature count, and phase timings.
3. Require pooled `LLVM_PROFILE_FILE` names containing `%4m` or another
   justified bounded pool. Preserve the base module signature when grouping.
4. Merge with `llvm-profdata --input-files`; never feed an unbounded list or
   wildcard to a fixed `-o` merge. Generate module-matched reports with
   `Build/Cmake/Testing/iccdev-matched-coverage-report.sh` and require zero
   mismatches.
5. Separate gates from review-only capability exits. Required CTest,
   instrumentation, profile generation, and report construction fail the job.
   Preserve partial artifacts with `always()`.
6. For timing analysis, run
   `.github/scripts/iccdev-build-config-timing.py` with a fixed job count and
   interleaved repetitions. Use at least ten test iterations for comparison;
   label shorter runs as smoke evidence.
7. Audit every workflow with
   `.github/scripts/check-workflow-fixed-output-batching.py`; distinguish unsafe
   repeated fixed-output commands from safe streamed output.
8. Validate YAML, jactionlint, yamllint, zizmor, ShellCheck, Python syntax and
   checker fixtures, fast-lane preflight, documentation rendering, and focused
   local report generation. State whether hosted CI ran.

Report raw sample counts and distributions, not only averages. A same-input
coverage percentage change is a correctness issue until deterministic merge and
module matching are proven.
