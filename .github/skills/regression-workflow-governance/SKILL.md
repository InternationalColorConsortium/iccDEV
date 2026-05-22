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

Use this skill when adding regression coverage or changing
`ci-iccdev-tool-tests.yml`.

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
   - `ci-iccdev-tool-tests.yml` for the main ASAN/UBSAN tool gate.
   - Other workflows only when they own the affected platform or feature.
4. Label the sub-test with the issue number and a short technical purpose.
5. Keep `run:` blocks compliant with workflow governance: `set -euo pipefail`,
   credential cleanup, no direct `${{ }}` in shell, trusted-base helpers for PR
   workflows, and sanitized summary output.
6. Update `.github/ci/regression/README.md` or the relevant docs so future
   maintainers can find the gate.
7. If a GitHub Actions run URL is involved, download the full run log archive
   and audit all jobs, including green jobs, for hidden diagnostics.
8. Run local script syntax checks and the focused regression before handoff.
   If the regression is added inside `iccdev-tool-coverage-baseline.sh`, run the
   direct script and the CTest wrapper because the CTest suite count should stay
   unchanged.

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

For edits to the existing tool coverage script:

```bash
ICCDEV_TOOLS_DIR=$PWD/build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-tool-output \
  .github/scripts/iccdev-tool-coverage-baseline.sh --asan --quick
ctest --test-dir build -R '^iccdev\.tool-coverage$' --output-on-failure
```

For workflow and packaging changes:

```bash
python3 -c "import yaml; [yaml.safe_load(open(p)) for p in ['.github/workflows/ci-pr-action.yml','.github/workflows/ci-iccdev-tool-tests.yml']]; print('YAML parse OK')"
actionlint -no-color .github/workflows/<workflow>.yml
python3 .github/scripts/audit-workflow-permissions.py --workflows-dir .github/workflows --format shell
```

If CMake or C++ changed, also run:

```bash
.github/scripts/run-codeql-local.sh --custom-only
```

## Review Checklist

- The test fails on the vulnerable or non-canonical behavior and passes on the
  patch.
- Output names include the issue number or regression name.
- Exit codes distinguish graceful tool failure from signal crashes.
- ASAN/UBSAN findings are treated as failures except for documented benign
  suppressions.
- Workflow changes follow `.github/instructions/workflow-governance.instructions.md`.
- PR workflow helper trust boundaries follow `docs/workflow-security-trust-boundaries.md`.
- Green workflow runs were checked for hidden log diagnostics, not only failed
  annotations.
- Any duplicate install manifest, missing-file uninstall, vcpkg CRT, vcpkg root,
  apt cache, Homebrew annotation, or skipped smoke coverage issue has either a
  hard gate or a documented reason for deferral.

## References

- `../../../docs/regression-workflow-governance.md`
- `../../../docs/workflow-security-trust-boundaries.md`
- `../../instructions/workflow-governance.instructions.md`
- `../../instructions/testing.instructions.md`
- `../../prompts/add-regression-workflow.prompt.md`
