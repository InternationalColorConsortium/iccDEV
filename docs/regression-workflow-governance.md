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
| MATLAB Windows gate | `.github/workflows/ci-matlab.yml` | PowerShell-native MSVC build, MATLAB MEX QA, native focused regressions, and Docker interoperability. |
| CTest registration | `Build/Cmake/Testing/CMakeLists.txt` | CTest names, labels, fixtures, timeouts, and check target. |
| CTest process guide | `docs/ctest.md` | Local commands, registered suites, and add-test workflow. |
| Maintainer CI skill | `.github/skills/maintainer-ci-ctest/SKILL.md` | Repeatable maintainer workflow for CI, CTest, CPack, sanitizer, and release gates. |
| Maintainer CI prompt | `.github/prompts/maintainer-ci-ctest.prompt.md` | Structured planning prompt for maintainer-owned infrastructure changes. |
| Workflow rules | `.github/instructions/workflow-governance.instructions.md` | Shell hardening, output sanitization, and injection prevention. |
| Workflow trust boundaries | `docs/workflow-security-trust-boundaries.md` | Trusted-base helper model, PR workflow canaries, and visual review aids. |
| Testing rules | `.github/instructions/testing.instructions.md` | Test directories, script expectations, and regression flow. |
| Unified Dockerfile | `Dockerfile` | Published runtime, MCP, and pinned CI dependency image. |

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
- `pull_request` workflows that build PR code must source `.github/scripts`
  helpers and sanitizers from a trusted base checkout, not from PR-controlled
  content, unless a test-only exception is explicitly marked for preflight.
- Runner-reduction changes must preserve trusted-base sanitizer sourcing,
  sanitize every `GITHUB_OUTPUT` value, and retain path-gated validation for
  container changes.

For reusable governance coverage, call
`.github/workflows/ci-pr-risk-security-analysis.yml` instead of duplicating the
scanner logic in a new CI workflow. The risk-analysis workflow is the PR
security canary for workflow and container changes, runs on every pull request
with read-only permissions, and fails blocking findings when PR-triggered.

Maintainer-owned workflow, release, packaging, Docker, MCP, and security
automation changes should use a conservative review loop. Run the applicable
local security tools, patch confirmed findings, retest, and document accepted
scanner noise with a clear rationale.

## Pull Request Review Convergence

Maintainer-owned infrastructure reviews must converge instead of repeatedly
expanding scope. Before requesting review, freeze the head and pass the
readiness gate in `docs/governance/UPSTREAM_PR_READINESS.md`. Review the
complete pull request and cumulative diff, not incremental slices. A requested
change returns the branch to branch-only grooming: repair the findings, renew
the readiness evidence, then request one complete re-review when a maintainer
needs it. If that re-review identifies a missed issue in unchanged code, stop
serial automated review and require maintainer direction before continuing.

Recent maintainer PRs show the same avoidable review findings recurring. Before
requesting review, check the PR against this list:

- Keep the PR scope narrow. Split unrelated registry QA, Docker, CodeQL, and
  C/C++ hardening work unless one failing gate requires the combination.
- For every new executable script, include the ICC copyright and BSD 3-Clause
  notice expected by `CONTRIBUTING.md`, run ShellCheck, and verify ASCII.
- Keep push, pull-request, reusable, and manual-dispatch validation paths
  equivalent for the changed surface. If a workflow tests a helper on push,
  the PR fast lane should test the same helper or document why it cannot.
- Keep branch triggers and publish conditions aligned. `ci-docker` publishes
  the canonical image only from `master` and release tags; do not add
  branch-specific or variant image tags.
- Keep Docker and regression-container docs reproducible from a fresh checkout
  or clean container. Fetch branch refs explicitly and avoid relying on local
  remote-tracking state, generated files, or preexisting host permissions.
- Validate trusted-base helper boundaries in PR workflows. PR-controlled
  checkouts must not provide sanitizer, summary, release, or package helpers
  unless the step is a reviewed test-only exception.
- Validate exact SHAs where documentation promises exact SHAs. Do not let
  branch or tag names pass through a field described as an exact commit.
- Reject invalid workflow inputs instead of silently coercing them. This is
  especially important for `patch_mode`, timeout, duration, target lists, and
  numeric strings with leading zeroes.
- Keep bounded jobs actually bounded. Runtime maxima must fit the job timeout
  after multiplying by target count and build/setup time.
- End PowerShell expected-failure probes with `exit 0` after all assertions
  pass. A handled native failure leaves `$LASTEXITCODE` nonzero and otherwise
  makes the workflow step fail after printing a success message.
- Do not leave temporary clones, generated profiles, crash artifacts, or other
  local proof material behind unless the PR intentionally adds test fixtures.
- For AFL/CFL and Docker changes, probe both the advertised user path and the
  CI path: wrapper compile probes, patch-stack checks, dry-run patch
  application, `iccdev-fuzz-env`, and container healthcheck semantics.

For every `Dockerfile` change, validate the one canonical image locally and
through `ci-docker` on `master`. Keep its `latest`, immutable full-SHA, and
release-tag behavior consistent; do not restore variant Dockerfiles or
branch-specific image publication.

Branch protection should require stable aggregate contexts, not conditional
lane job names. `PR Summary` must aggregate orchestration prerequisites and all
selected full, fast-lane, auto, governance, docs, and path-gated jobs. Do not
require removed lane names or initialization jobs as branch contexts. Require
WASM parity separately on `master`, where that workflow runs outside the
orchestrator. See `docs/label-system.md` for the current context list.

When `container_changed` is true, `ci-pr-action` selects the read-only Docker
PR verification lane and `PR Summary` requires its result. The lane is skipped
for other changes to conserve runners. Do not treat a skipped lane as container
verification; rerun it after any Dockerfile, container image, or container
workflow update.

The Docker PR lane builds the exact checked-out PR Dockerfile without a workflow
cache. The resulting image is local to the job: it must bind the checked-out PR
tree read-only, copy it to container-local scratch space, and build and run the
fast CTest envelope there. Only `ci-docker` publishes images.

Local review should include YAML parsing, `actionlint`, `yamllint`, direct
`${{ }}` interpolation scans for `run:` blocks, Dockerfile base/remote-exec
checks when container files are in scope, CodeQL Actions analysis, and CodeQL
query-pack resolution when CodeQL workflows or queries are touched.

See `.github/instructions/workflow-governance.instructions.md` for the full
checklist and `docs/workflow-security-trust-boundaries.md` for the visual trust
boundary model.

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
| `Dockerfile` | Unified image for runtime tools, MCP, ASAN/UBSAN CTest, fuzzing, review, and hybrid timing gates. Clang 22 is the default toolchain; the packaged AFL++ LLVM plugin is paired with Clang 21. | Run a no-cache build and smoke `git`, `gh`, `curl`, `clang`, `clang++`, `gcc`, `g++`, `lldb`, `gdb`, `cmake`, `afl-fuzz`, `afl-showmap`, `iccdev-fuzz-env`, MCP initialization, libFuzzer compilation, and `/usr/bin/time`; AFL wrapper changes also need the container bootstrap probe in `docs/afl-fuzzing.md`. |

For unified `Dockerfile` publishing:

1. Build and smoke the target image locally with no cache.
2. Publish through the maintainer-controlled container release path.
3. Record the published immutable SHA tag, digest, and source revision from the
   release output.
4. Publish `latest` only from `master` after all image smoke tests and
   regression CTest checks succeed, then confirm it resolves to the immutable
   digest.
5. Pass the immutable SHA tag to `ci-iccdev-tool-tests.yml` and rerun the
   regression gate.
6. Do not create branch-specific or run-specific image tags.

For day-to-day image use, PR checkout, issue reproduction, evidence handling,
and CI dispatch commands, use
[Maintainer regression container](regression-container.md).

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
