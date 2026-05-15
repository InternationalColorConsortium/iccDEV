---
applyTo: ".github/workflows/**"
---

# Workflow Governance Instructions

These rules apply to all GitHub Actions workflow files in this repository.
Reference implementations: `ci-pr-action.yml` for Bash and `ci-pr-win.yml` for
PowerShell.

Workflow files, sanitizer helpers, CodeQL configuration, CTest gates, CPack and
release packaging, and security automation are maintainer-owned
infrastructure. General contributors should not edit these areas unless an
iccDEV maintainer explicitly requests that work.

## Maintainer Security Posture

Maintainer-owned infrastructure should favor conservative review over speed.
For CI, release, packaging, Docker, and security-automation changes,
maintainers should run the smallest complete local security battery that matches
the changed surface, patch confirmed findings, and retest before pushing.

Do not treat scanner output as automatically authoritative. Classify each
finding as fixed, accepted with a documented rationale, or deferred with an
owner-visible follow-up. Prefer precise evidence over broad success claims.

## Shell Hardening

Every Bash `run:` block should use:

```yaml
shell: bash --noprofile --norc {0}
env:
  BASH_ENV: /dev/null
run: |
  set -euo pipefail
  git config --global credential.helper ""
  unset GITHUB_TOKEN || true
  source .github/scripts/sanitize-sed.sh
```

PowerShell-only workflows and jobs do not need `BASH_ENV`. Use an explicit
PowerShell shell on each step or `defaults.run.shell` at job/workflow scope.

Every PowerShell `run:` block should use:

```yaml
shell: pwsh -NoProfile -NoLogo -NonInteractive -Command {0}
env:
  POWERSHELL_TELEMETRY_OPTOUT: 1
  POWERSHELL_UPDATECHECK: Off
run: |
  $ErrorActionPreference = 'Stop'
  $PSDefaultParameterValues['*:ErrorAction'] = 'Stop'
  git config --global credential.helper ""
  if (Test-Path env:GITHUB_TOKEN) { Remove-Item env:GITHUB_TOKEN }
  . .github/scripts/sanitize.ps1
```

## Injection Prevention

Never place `${{ }}` expressions directly in shell code. Pass expression values
through `env:` and read the environment variable inside the shell.

```yaml
env:
  BUILD_TYPE: ${{ matrix.build_type }}
run: echo "Building ${BUILD_TYPE}"
```

This applies to branch names, PR titles, issue titles, comments, workflow inputs,
and matrix values. Expressions in YAML-level `name:`, `with:`, `key:`, and `if:`
fields are evaluated by GitHub Actions, not by the shell.

For privileged `pull_request_target` or `workflow_run` automation, never check
out or execute PR-controlled content. Use trusted workflow metadata only, pass
IDs through `env:`, and mutate PR state with least-privilege `issues: write` or
`pull-requests: write` permissions.

## SIGPIPE Avoidance

Under `set -o pipefail`, avoid `echo "$value" | grep -q pattern`. Prefer:

```bash
grep -qE 'pattern' <<< "$value"
grep -q 'pattern' "$filepath"
[[ "$value" =~ pattern ]]
```

## Output Sanitization

Sanitize every write to `GITHUB_STEP_SUMMARY` or `GITHUB_OUTPUT`.

| Bash | PowerShell | Purpose |
|------|------------|---------|
| `sanitize_line "$var"` | `Sanitize-Line $var` | One safe line |
| `sanitize_print "$var"` | `Sanitize-Print $var` | Safe block output |
| `sanitize_ref "$var"` | `Sanitize-Ref $var` | Git ref |
| `sanitize_filename "$var"` | `Sanitize-Filename $var` | Path component |
| `escape_html "$var"` | `Escape-Html $var` | HTML entity encoding |

For multiline content, sanitize one line at a time.

## Permissions and Credentials

- Use least-privilege `permissions:` at job level when possible.
- Clear git credentials before user-controllable logic:
  `git config --global credential.helper ""`.
- Remove `GITHUB_TOKEN` from the environment unless the step truly needs it.
- PR workflows should use concurrency groups with `cancel-in-progress: true`.

## Automated Scanner

`ci-pr-risk-security-analysis.yml` checks workflow governance, including action
pinning, dangerous triggers, credential hygiene, shell hardening, matrix
expression injection, output sanitization, permissions, Dockerfile security, and
supply-chain risk.
It is the required PR security canary for workflow and container risk, and runs
with read-only permissions against every pull request through the
`ci-pr-action.yml` risk-gate job. It also supports manual `workflow_dispatch`
runs and reusable `workflow_call` invocation from other CI workflows. Prefer
calling it over copying scanner logic, and avoid adding a second direct PR
trigger unless the caller gate is removed to prevent duplicate checks.

`ci-preflight-safety.yml` runs the local pre-flight script on workflow and hook
changes. It installs and runs actionlint, yamllint, shellcheck, and zizmor, then
delegates repository-specific checks to `.github/scripts/preflight-safety-checks.sh`.
Use that script locally before pushing maintainer-owned infrastructure changes.
The local script intentionally runs only the deterministic subset of
`ci-pr-risk-security-analysis.yml`: changed-workflow YAML parsing, actionlint,
yamllint, zizmor, SHA pin checks, dangerous-trigger checks, run-block expression
injection checks, checkout credential checks, shellcheck for changed scripts,
CodeQL query resolution, and the workflow permission audit. Leave the full
owner-visible report, broad repository scan, and any GitHub-context-specific
classification to the Actions workflow.

Use tiered CodeQL coverage. CI-only, script-only, and documentation-only changes
should use the pre-flight workflow's fast CodeQL query resolution. Full CodeQL
database builds are reserved for C/C++/CMake/query/config changes, scheduled
runs, the `codeql-ready` PR label, or explicit maintainer `workflow_dispatch`.

## Full Run Log Audit

A workflow conclusion of `success` is not proof that the workflow is clean.
When a run URL is part of the task, download and inspect the complete log
archive.

Search runtime output for:

- `##[error]`, `##[warning]`, `::error`, and `::warning`
- `CMake Warning`
- `File ".+" does not exist`
- `Cannot open: Permission denied`
- `post-build check`
- `DEP0005`
- `digest-mismatch`

Filter out echoed shell source before classifying diagnostics. Bash source lines
printed by Actions commonly include ANSI cyan `\033[36;1m`; those lines often
show failure-handling code, not an actual failure.

Treat these green-run signals as actionable:

- Duplicate `install_manifest.txt` paths.
- Install or uninstall logs with missing-file diagnostics.
- Duplicate generated header install lines.
- Cache paths that include apt lock or `partial` directories.
- Homebrew already-installed annotations from unconditional `brew install`.
- vcpkg root mismatch warnings.
- Static vcpkg triplets linked against the wrong CRT.
- Matrix configurations that skip smoke tests for non-Debug variants.

Use YAML parsing, `actionlint`, shellcheck, `yamllint`, `zizmor`, and the
workflow permission audit for workflow files. Use `hadolint`, Trivy config, or
equivalent Dockerfile checks when container files are in scope. Run CodeQL for
related C/C++ or CMake changes, not as the workflow YAML checker.

The pre-flight gate must include Dockerfile paths when container files change.
Dockerfile checks are blocking for changed container files: avoid advisory-only
suppression for package pinning, `RUN cd` patterns, missing healthchecks, or
pipefail findings unless a maintainer records the explicit exception.

Before merging workflow changes, run:

```bash
python3 -c "import yaml; [yaml.safe_load(open(p)) for p in ['.github/workflows/<workflow>.yml']]; print('YAML OK')"
actionlint -no-color .github/workflows/<workflow>.yml
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' .github/workflows/<workflow>.yml
zizmor .github/workflows/<workflow>.yml
gh codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
.github/scripts/preflight-safety-checks.sh
```

Also scan changed `run:` blocks for direct `${{ }}` interpolation. Any value
from `github.event`, `github.head_ref`, `matrix`, or workflow inputs must be
passed through `env:` and sanitized before output.

## Test Workflow Guardrails

CTest discovery and execution steps must use `--no-tests=error`. Workflows that
own the Linux CTest gate must assert the expected `Total Tests` line after
`ctest -N`.

Do not mask profile generation, CTest discovery, or regression execution with
`|| true`. Expected skips must be represented as explicit script output and the
workflow must still have a deterministic pass/fail condition.

When profile generation counts change, update the assertions in
`docs/ctest.md`, `Build/Cmake/Testing/CMakeLists.txt`, and the workflows that
validate generated-profile totals.

## Optional Local Hooks

Maintainers can install the optional local hooks with:

```bash
.github/scripts/install-git-hooks.sh
```

The `pre-commit` hook blocks CI, hook, and Dockerfile changes that fail the fast
preflight gate. The `pre-push` hook warns before pushing multiple commits,
detached HEAD state, branches behind upstream, protected branch targets, or
maintainer-owned infrastructure changes. It is advisory for non-interactive
pushes and prompts only when stdin and stderr are attached to a terminal.
