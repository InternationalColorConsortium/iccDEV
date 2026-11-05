# Code Coverage and Timing Review

Use `docs/code-coverage.md` and
`.github/skills/code-coverage-analysis/SKILL.md` to review the requested branch
or workflow.

## Inputs

- Branch and source SHA:
- Workflow run URLs:
- Compiler and LLVM version:
- Build configurations:
- Fixed build job count:
- Local test iterations:
- Expected gate failures versus review-only exits:

## Required Review

1. Compare same-SHA run phase timings, profile counts, profile-data size,
   coverage totals, warnings, and test outcomes.
2. Prove that every raw profile participates exactly once. Reject shell globs
   and `find | xargs ... merge -o fixed.profdata`; use input files and
   module-matched reporting.
3. Audit all workflows for repeated fixed-output batching, without flagging
   safe streamed checksum or lint output.
4. Run the local configuration timing driver for Debug, Release,
   RelWithDebInfo, MinSizeRel, coverage, and ASan. Interleave repetitions and
   report n, mean, median, sample SD, CV, min, max, spread, and p95.
5. Identify duplicated work, masked failures, missing instrumentation, stale
   artifacts, coarse timing, unstable parallelism, and misleading comparisons.
6. Make authorized corrections, align canonical docs and repository guidance,
   and preserve partial failure evidence.
7. Validate the focused report helper and repository preflight. Record exact
   commands and distinguish local proof from hosted-CI proof.

Return findings ordered by impact, the implemented changes, the local timing
table, validation results, and remaining limits.
