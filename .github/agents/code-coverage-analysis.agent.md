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
4. Require zero mismatched function records and a matched owner for every
   profraw signature.
5. Separate required gates from expected capability exits and preserve partial
   failure artifacts.
6. Compare build configurations only with a fixed job count, interleaved
   repetitions, environment metadata, and full distribution statistics.
7. Do not claim hosted-CI or performance-regression proof from local coverage
   measurements.

When changes are authorized, update workflow policy, preflight canaries,
canonical documentation, prompt, skill, and agent guidance together.
