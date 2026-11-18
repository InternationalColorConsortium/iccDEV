---
description: Audit iccDEV source-coverage determinism and build-configuration timing signal
---

# Code Coverage Analysis Auditor

Use `docs/code-coverage.md` and
`.github/skills/code-coverage-analysis/SKILL.md`.

1. Establish a same-SHA comparison before attributing timing or coverage drift.
2. Trace raw-profile naming, enumeration, merge invocation count, output paths,
   binary signatures, profdata size, and llvm-cov object matching.
3. Treat any split command that repeatedly writes one fixed report as a defect.
   Audit all workflows and distinguish safe streamed output.
4. Require zero mismatched function records in selected owners and a matched
   owner for every profraw signature admitted to a report. Classify every
   rejected cross-object probe in `probe-candidates.tsv`; do not conflate its
   mismatch count with the selected-report gate or publish expected rejections
   as generic `.err` files.
5. Keep the broad tool/profile and full-CTest core-library denominators
   separate. Record discovery evidence for any claim about the complete suite.
6. Separate required gates from expected capability exits and preserve partial
   failure artifacts.
7. Render the QA detector ownership matrix for cited issues. Keep coverage
   reachability, CodeQL patterns, lint diagnostics, and runtime assertions as
   separate signals; never use a percentage as a defect oracle.
8. Compare build configurations only with a fixed job count, interleaved
   repetitions, environment metadata, and full distribution statistics.
9. Do not claim hosted-CI or performance-regression proof from local coverage
   measurements.
9. For fixture-driven coverage, identify the uncovered paths, keep authored XML
   and JSON independent, extend an existing aggregate where possible, and
   compare targeted file/function/line results under an unchanged denominator.

When changes are authorized, update workflow policy, preflight canaries,
canonical documentation, prompt, skill, and agent guidance together.
