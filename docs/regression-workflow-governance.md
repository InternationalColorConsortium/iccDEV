# Regression Workflow Governance

Use this guide when adding or changing regression gates, tool-test workflows, or
focused validation scripts.

These workflows and gates are maintainer-owned infrastructure. General
contributors should not edit `.github/**`, CTest registration, sanitizer helper
scripts, CPack or release packaging, workflow policy, or security automation
unless an iccDEV maintainer explicitly requests that work. Contributors should
describe needed coverage in the issue or pull request so maintainers can decide
where it belongs.

## Canonical Locations

| Item | Location | Purpose |
|------|----------|---------|
| Focused reusable regression scripts | `.github/scripts/` | Scripted checks shared by one or more workflows. |
| Regression PoC inventory | `.github/ci/regression/README.md` | Maps regression inputs and scripts to issues. |
| Tool test gate | `.github/workflows/ci-iccdev-tool-tests.yml` | ASAN/UBSAN tool coverage, JSON gates, regression scripts, and broad generated-profile CLI coverage. |
| CTest registration | `Build/Cmake/Testing/CMakeLists.txt` | CTest names, labels, fixtures, timeouts, and check target. |
| CTest process guide | `docs/ctest.md` | Local commands, registered suites, and add-test workflow. |
| Maintainer CI skill | `.github/skills/maintainer-ci-ctest/SKILL.md` | Repeatable maintainer workflow for CI, CTest, CPack, sanitizer, and release gates. |
| Maintainer CI prompt | `.github/prompts/maintainer-ci-ctest.prompt.md` | Structured planning prompt for maintainer-owned infrastructure changes. |
| Workflow rules | `.github/instructions/workflow-governance.instructions.md` | Shell hardening, output sanitization, and injection prevention. |
| Testing rules | `.github/instructions/testing.instructions.md` | Test directories, script expectations, and regression flow. |
| Maintainer Dockerfiles | `Dockerfile`, `Dockerfile.nixos`, `Dockerfile.ci-regression` | Release/runtime images and pinned CI dependency images. |
| Regression container publisher | `.github/workflows/ci-regression-container.yml` | Builds and publishes `Dockerfile.ci-regression` through `ghcr-publish`. |

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

- `shell: bash --noprofile --norc {0}` for bash steps.
- `BASH_ENV: /dev/null` for bash steps only.
- `set -euo pipefail` as the first bash command.
- `pwsh -NoProfile -NoLogo -NonInteractive -Command {0}` and
  `$ErrorActionPreference = 'Stop'` for PowerShell steps.
- `git config --global credential.helper ""`.
- `unset GITHUB_TOKEN || true`.
- No direct `${{ }}` expressions inside shell code; pass values through `env:`.
- Sanitized writes to `GITHUB_STEP_SUMMARY` and `GITHUB_OUTPUT`.
- Least-privilege permissions.
- `pull_request_target` and `workflow_run` automation must not checkout or run
  PR-controlled code before mutating trusted state such as labels or checks.

For reusable governance coverage, call
`.github/workflows/ci-pr-risk-security-analysis.yml` instead of duplicating the
scanner logic in a new CI workflow.

Local review should include YAML parsing, `actionlint`, `yamllint`, direct
`${{ }}` interpolation scans for `run:` blocks, and CodeQL query-pack resolution
when CodeQL workflows or queries are touched.

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

- `docs/ctest.md` for CTest registrations, expected suite counts, fixtures, or
  generated-profile count changes.
- `docs/ctest.md` or the relevant tool README when adding cases inside an
  existing CTest-backed script without changing the CTest suite count.
- `.github/ci/regression/README.md` for PoC files or script-based gates.
- `.github/instructions/testing.instructions.md` when the gate becomes part of
  standard testing policy.
- `.github/copilot-instructions.md` only for cross-cutting agent routing.
- `docs/regression-workflow-governance.md` when the process itself changes.

Do not update root `README.md` or `CONTRIBUTING.md` for branch-specific
regression changes unless ICC approval explicitly covers those root documents.

## Maintainer Dockerfile Changes

All `Dockerfile*` changes are maintainer-owned because they affect release
artifacts, runner trust, or pinned dependency envelopes. Keep container changes
separate from general source changes when practical.

| File | Owner intent | Required local checks |
|------|--------------|-----------------------|
| `Dockerfile` | Ubuntu release/runtime image for the published `iccdev` container. | Build locally, run at least one installed tool, and check the healthcheck target. |
| `Dockerfile.nixos` | NixOS/scratch runtime image and closure minimization. | Build locally, run one tool, and confirm closure/secret checks remain active. |
| `Dockerfile.ci-regression` | Dependency image for ASAN/UBSAN CTest and hybrid timing gates. | Run a no-cache build and smoke `clang-18`, `clang++-18`, `cmake`, and `/usr/bin/time`. |

For `Dockerfile.ci-regression` publishing:

1. Add the branch to the `ghcr-publish` environment deployment branch policy if
   the branch is not already allowed.
2. Trigger `ci-regression-container` and wait for maintainer deployment approval.
3. Read the pushed GHCR digest from the run log or summary.
4. Pin `ci-iccdev-tool-tests.yml` to that digest and rerun the regression gate.
5. Remove temporary branch policy entries after the branch is no longer needed.

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
For updates to `.github/scripts/iccdev-tool-coverage-baseline.sh`, run both the
direct script invocation and `ctest -R '^iccdev\.tool-coverage$'` so CI wrapper
behavior is covered.

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
