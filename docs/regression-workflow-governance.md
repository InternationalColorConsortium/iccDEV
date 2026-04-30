# Regression Workflow Governance

Use this guide when adding or changing regression gates, tool-test workflows, or
focused validation scripts.

## Canonical Locations

| Item | Location | Purpose |
|------|----------|---------|
| Focused reusable regression scripts | `.github/scripts/` | Scripted checks shared by one or more workflows. |
| Regression PoC inventory | `.github/ci/regression/README.md` | Maps regression inputs and scripts to issues. |
| Main tool gate | `.github/workflows/ci-tool-tests.yml` | ASAN/UBSAN tool coverage, JSON gates, and regression scripts. |
| Comprehensive tool gate | `.github/workflows/ci-iccdev-tool-tests.yml` | Broad generated-profile and CLI coverage. |
| Workflow rules | `.github/instructions/workflow-governance.instructions.md` | Shell hardening, output sanitization, and injection prevention. |
| Testing rules | `.github/instructions/testing.instructions.md` | Test directories, script expectations, and regression flow. |

## When to Add a Script

Add a `.github/scripts/*.sh` regression when:

- The check is reused in more than one workflow.
- The logic parses binary/profile structure or generated output.
- The check needs a stable local command for maintainers.
- The workflow block would become hard to review inline.

Keep inline workflow checks only when they are short, single-purpose, and not
expected to be reused.

## Regression Gate Requirements

Every new regression gate should state:

1. The issue or PR number.
2. The affected component.
3. The reproducer or invariant.
4. The expected pass condition.
5. The expected failure signal.

Prefer deterministic invariants over broad diffs. For generated ICC profiles,
normalize only documented volatile fields when comparing whole files; otherwise
assert specific tag sizes, offsets, record lengths, or validation messages.

## Workflow Governance Requirements

Every edited workflow `run:` block must keep these properties:

- `shell: bash --noprofile --norc {0}` at the workflow or job default.
- `BASH_ENV: /dev/null` in the step or job environment.
- `set -euo pipefail` as the first shell command.
- `git config --global credential.helper ""`.
- `unset GITHUB_TOKEN || true`.
- No direct `${{ }}` expressions inside shell code; pass values through `env:`.
- Sanitized writes to `GITHUB_STEP_SUMMARY` and `GITHUB_OUTPUT`.
- Least-privilege permissions.

See `.github/instructions/workflow-governance.instructions.md` for the full
checklist.

## Full Log Audit Requirements

When reviewing a GitHub Actions run, inspect the complete log archive even if
the run conclusion is `success`. Green runs can still hide missing coverage,
cache failures, package-manager annotations, and non-fatal install/uninstall
diagnostics.

Use this pattern:

```bash
RUN_ID=<run-id>
REPO=InternationalColorConsortium/iccDEV
OUT="/tmp/iccdev-run-${RUN_ID}-logs.zip"
DIR="/tmp/iccdev-run-${RUN_ID}-logs"
rm -rf "$DIR" "$OUT"
gh api "/repos/${REPO}/actions/runs/${RUN_ID}/logs" > "$OUT"
mkdir -p "$DIR"
unzip -q "$OUT" -d "$DIR"
rg -n '##\[error\]|##\[warning\]|::error|::warning|CMake Warning|File ".+" does not exist|Cannot open: Permission denied|post-build check|DEP0005|digest-mismatch' "$DIR" | grep -v $'\033\[36;1m'
```

For install and packaging workflows, also check:

- Duplicate entries in `install_manifest.txt`.
- Install and uninstall logs containing `File ".+" does not exist`.
- Duplicate generated version-header installs.
- vcpkg root mismatch warnings.
- vcpkg CRT linkage warnings for static triplets.
- Build-type matrix entries whose smoke tests are skipped.

Fixes should add a gate for the defect class, not only silence the observed log
line.

## Script Pattern

Regression scripts should:

- Use `#!/bin/bash` and `set -uo pipefail`.
- Accept `ICCDEV_TOOLS_DIR`, `ICCDEV_TESTING_DIR`, and `ICCDEV_TEST_OUTDIR`.
- Write temporary files under `ICCDEV_TEST_OUTDIR`.
- Print `[PASS]`, `[FAIL]`, `[WARN]`, or `[SKIP]` labels.
- Exit non-zero if any required check fails.
- Treat ASAN/UBSAN findings as failures unless a documented benign suppression
  is part of the test envelope.

Minimal local invocation:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-regression \
  .github/scripts/<regression-script>.sh
```

## Documentation Updates

When adding a gate, update the smallest useful index:

- `.github/ci/regression/README.md` for PoC files or script-based gates.
- `.github/instructions/testing.instructions.md` when the gate becomes part of
  standard testing policy.
- `.github/copilot-instructions.md` only for cross-cutting agent routing.
- `docs/regression-workflow-governance.md` when the process itself changes.

Do not update root `README.md` or `CONTRIBUTING.md` for branch-specific
regression changes unless ICC approval explicitly covers those root documents.

## Local Validation

Before pushing:

```bash
bash -n .github/scripts/<regression-script>.sh
git diff --check
file .github/scripts/<regression-script>.sh
```

Run the focused regression and capture the command plus result in the handoff.
For workflow edits, also inspect the changed `run:` blocks for the governance
rules above.

For CI packaging edits, run the nearest local install/uninstall repro and grep
the captured logs for missing-file diagnostics. If the change touches CMake or
C++ code, run `.github/scripts/run-codeql-local.sh --custom-only`; CodeQL is not
a substitute for YAML linting.

## Handoff

Report:

- Branch and commit.
- Files changed.
- Focused local commands and exit status.
- Workflow names and sub-test labels to trigger.
- Any known skips, warnings, or sanitizer suppressions.
- Any green-run diagnostics that were converted into hard gates.
