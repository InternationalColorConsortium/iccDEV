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
expression injection, output sanitization, permissions, and supply-chain risk.

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

Use YAML parsing, `actionlint`, shellcheck, `yamllint`, and the workflow
permission audit for workflow files. Run CodeQL for related C/C++ or CMake
changes, not as the workflow YAML checker.

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
