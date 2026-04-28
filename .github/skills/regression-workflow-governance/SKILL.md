---
name: regression-workflow-governance
description: >
  Add or update iccDEV regression gates and tool-test workflow coverage while
  preserving GitHub Actions governance, sanitizer reporting, and issue
  traceability.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Regression Workflow Governance

Use this skill when adding regression coverage, updating `ci-tool-tests.yml`,
or changing `ci-iccdev-tool-tests.yml`.

## Required Inputs

1. Issue or PR reference, for example `#928`.
2. The exact affected component path.
3. The smallest reproducible input or generated-profile invariant.
4. The expected pass/fail signal.

## Workflow

1. Read `../../../docs/regression-workflow-governance.md`.
2. Put reusable checks in `.github/scripts/` when the logic is more than a few
   shell lines or should run in more than one workflow.
3. Wire the check into the nearest existing regression block:
   - `ci-tool-tests.yml` for the main ASAN/UBSAN tool gate.
   - `ci-iccdev-tool-tests.yml` for comprehensive tool coverage.
   - Other workflows only when they own the affected platform or feature.
4. Label the sub-test with the issue number and a short technical purpose.
5. Keep `run:` blocks compliant with workflow governance: `set -euo pipefail`,
   credential cleanup, no direct `${{ }}` in shell, and sanitized summary output.
6. Update `.github/ci/regression/README.md` or the relevant docs so future
   maintainers can find the gate.
7. Run local script syntax checks and the focused regression before handoff.

## Validation

```bash
bash -n .github/scripts/<new-regression>.sh
git diff --check
file .github/scripts/<new-regression>.sh
```

For local tool gates, set paths explicitly:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-regression \
  .github/scripts/<new-regression>.sh
```

## Review Checklist

- The test fails on the vulnerable or non-canonical behavior and passes on the
  patch.
- Output names include the issue number or regression name.
- Exit codes distinguish graceful tool failure from signal crashes.
- ASAN/UBSAN findings are treated as failures except for documented benign
  suppressions.
- Workflow changes follow `.github/instructions/workflow-governance.instructions.md`.

## References

- `../../../docs/regression-workflow-governance.md`
- `../../instructions/workflow-governance.instructions.md`
- `../../instructions/testing.instructions.md`
- `../../prompts/add-regression-workflow.prompt.md`
