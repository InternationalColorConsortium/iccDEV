# Add a Regression Workflow Gate

Use this prompt when adding a regression test or wiring a focused script into
GitHub Actions.

## Goal

Add a minimal, reproducible regression gate for:

- Issue or PR:
- Component path:
- Reproducer or invariant:
- Expected pass signal:
- Expected failure signal:

## Constraints

- Keep the fix and regression scoped to the affected code path.
- Prefer a reusable `.github/scripts/*.sh` gate when the check is shared by more
  than one workflow.
- Follow `.github/instructions/workflow-governance.instructions.md` for every
  workflow `run:` block.
- Do not add generated profiles, logs, crash artifacts, or local absolute paths.
- Keep output labels clear enough for a maintainer to map CI failures back to
  the issue.

## Implementation Steps

1. Read `docs/regression-workflow-governance.md`.
2. Inspect existing regression blocks in `ci-iccdev-tool-tests.yml`.
3. Add the smallest script or inline test that proves the behavior.
   If the case belongs inside `iccdev-tool-coverage-baseline.sh`, keep it
   self-contained under `ICCDEV_TEST_OUTDIR` and do not change the CTest suite
   count.
4. Name the test with the issue number and technical subject.
5. Update `.github/ci/regression/README.md` or related docs.
6. Run:

```bash
bash -n .github/scripts/<new-regression>.sh
git diff --check
file .github/scripts/<new-regression>.sh
```

7. Run the focused regression locally with explicit `ICCDEV_TOOLS_DIR`,
   `ICCDEV_TESTING_DIR`, and `ICCDEV_TEST_OUTDIR`.
   For `iccdev-tool-coverage-baseline.sh`, also run
   `ctest --test-dir build -R '^iccdev\.tool-coverage$' --output-on-failure`.

## Handoff Format

Report:

- Files changed.
- Exact local validation commands and exit status.
- Sentinel outputs or invariants verified.
- Workflow names and sub-test labels to trigger manually.
