# Audit Workflow Governance Compliance

Use this prompt to audit any GitHub Actions workflow for compliance.

## Audit Checklist

For EVERY `run:` step in the workflow, verify:

### 1. Shell Hardening
- [ ] `shell: bash --noprofile --norc {0}` for bash or `pwsh -NoProfile -NoLogo -NonInteractive -Command {0}` for PowerShell
- [ ] `BASH_ENV: /dev/null` in env for bash steps only
- [ ] `set -euo pipefail` as first command (bash)
- [ ] `$ErrorActionPreference = 'Stop'` (PowerShell)

### 2. Credential Reduction
- [ ] `git config --global credential.helper ""`
- [ ] `unset GITHUB_TOKEN || true` (bash) or `Remove-Item env:GITHUB_TOKEN` (PowerShell)

### 3. Sanitizer Loaded
- [ ] PR workflows source sanitizers from a trusted base checkout, not PR content
- [ ] `source "$TRUSTED_SCRIPTS/sanitize-sed.sh"` (bash) with inline fallback
- [ ] `. "$env:TRUSTED_SCRIPTS\sanitize.ps1"` (PowerShell)

### 4. Expression Injection
- [ ] No `${{ matrix.* }}` directly in `run:` blocks -- must use `env:` passthrough
- [ ] No `${{ github.event.*.title }}` or other user-controllable contexts in `run:`
- [ ] No `${{ github.head_ref }}` in `run:` blocks
- [ ] `pull_request_target` and `workflow_run` jobs do not checkout or execute
      PR-controlled code before mutating labels, comments, releases, or checks
- [ ] `pull_request` jobs that execute PR code do not source `.github/scripts`
      or `.github/tests` from the PR checkout unless the step is a reviewed
      test-only exception with `preflight: allow-pr-script-execution`

### 5. Output Sanitization
- [ ] All `GITHUB_STEP_SUMMARY` writes use `sanitize_line`/`Sanitize-Line`
- [ ] All `GITHUB_OUTPUT` writes use `sanitize_ref`/`Sanitize-Ref` for refs
- [ ] Multiline content iterated line-by-line (not passed as single argument)

### 6. Permissions
- [ ] Least-privilege `permissions:` declared (prefer job-level)
- [ ] No `write` permissions unless genuinely needed

### 7. Concurrency
- [ ] `concurrency:` group defined for PR/push workflows
- [ ] `cancel-in-progress: true` for PR workflows

### 8. Full Run Log Audit
- [ ] Full log archive downloaded with `gh api /repos/OWNER/REPO/actions/runs/RUN_ID/logs`
- [ ] Runtime diagnostics grepped across every job, not only failed jobs
- [ ] Echoed shell source filtered out before classifying findings
- [ ] Green-run `##[error]`, `##[warning]`, and `CMake Warning` lines reviewed
- [ ] Install/uninstall logs checked for duplicate manifest paths and missing files
- [ ] Package-manager and cache warnings classified as fixed, deferred, or baseline
- [ ] Matrix smoke-test skips reviewed for missing coverage

## Running the Audit

### Automated (preferred)

Trigger the 10-check governance scanner:

```bash
gh workflow run ci-pr-risk-security-analysis.yml \
  --ref ci-governance-audit \
  -f analysis_target='Current branch workflows'
```

The scanner checks: SHA pinning, dangerous triggers, credential hygiene,
shell hardening, matrix injection, output sanitization, permissions,
trusted-base helper boundaries, supply-chain score, trivy indicators, and
workflow inventory.
PowerShell-only workflows with `defaults.run.shell: pwsh ...` are exempt from
the bash-specific BASH_ENV check.

### Manual Grep

```bash
# Quick grep for common violations
FILE=".github/workflows/target.yml"

# Direct expression injection in run: blocks
grep -n 'run:' -A 50 "$FILE" | grep -E '\$\{\{[^}]*(matrix|github\.event|github\.head_ref)'

# Missing shell hardening
grep -n 'run:' "$FILE" | head -5  # then check if shell: is set

# Unsanitized summary writes
grep -n 'GITHUB_STEP_SUMMARY' "$FILE" | grep -v 'sanitize_line\|Sanitize-Line\|sanitize_print\|Sanitize-Print\|Safe-EchoForSummary'

# Missing credential cleanup
grep -c 'credential.helper' "$FILE"  # should equal number of run: blocks
```

### Local Tooling

```bash
python3 -c "import yaml; yaml.safe_load(open('$FILE')); print('YAML OK')"
actionlint -no-color "$FILE"
yamllint -d '{extends: default, rules: {document-start: disable, truthy: disable, line-length: {max: 120}}}' "$FILE"
gh codeql resolve queries .github/codeql-queries/iccdev-security-suite.qls
```

Use CodeQL to validate query packs and C/C++ or CMake-relevant security changes;
do not treat CodeQL as the workflow YAML parser.

For Dockerfile or container-policy changes, also run:

```bash
hadolint Dockerfile Dockerfile.*
trivy config --severity LOW,MEDIUM,HIGH,CRITICAL --exit-code 1 .
docker build -f Dockerfile -t iccdev-local:ubuntu .
trivy image --scanners vuln,secret --severity HIGH,CRITICAL --exit-code 1 iccdev-local:ubuntu
docker build -f Dockerfile.nixos -t iccdev-local:nixos .
trivy image --scanners vuln,secret --severity HIGH,CRITICAL --exit-code 1 iccdev-local:nixos
```

### Full Run Log Grep

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

## Severity Classification

| Severity | Finding | Example |
|----------|---------|---------|
| **CRITICAL** | Expression injection in `run:` | `${{ matrix.build_type }}` in shell |
| **CRITICAL** | Missing sanitizer for summary | Raw `>> $GITHUB_STEP_SUMMARY` |
| **HIGH** | No shell hardening | Missing `--noprofile --norc` |
| **HIGH** | No credential reduction | Missing `unset GITHUB_TOKEN` |
| **HIGH** | Green-run missing-file diagnostic | `-- File "... " does not exist.` |
| **HIGH** | Duplicate install manifest path | Same installed path removed twice |
| **HIGH** | Skipped matrix smoke coverage | Release variants do not run consumer smoke |
| **MEDIUM** | Package/cache warning in green job | apt cache permission or Homebrew annotation |
| **MEDIUM** | Excessive permissions | `contents: write` when read suffices |
| **LOW** | Missing concurrency group | No `concurrency:` block |

## CodeQL Boundary

CodeQL Actions analysis is required for workflow changes. CodeQL Python
analysis is required for changed Python scripts. Full CodeQL C/C++ remains
required for related C/C++ or CMake changes. Pair CodeQL with YAML parsing,
`actionlint`, ShellCheck, `yamllint`, and the workflow permission audit because
no single scanner covers the full CI threat model.

## Reference Workflows

- **Bash gold standard**: `.github/workflows/ci-pr-action.yml`
- **PowerShell gold standard**: `.github/workflows/ci-pr-win.yml`
- **Cross-platform release**: `.github/workflows/ci-latest-release.yml`
- **Trust-boundary visual aid**: `../../docs/workflow-security-trust-boundaries.md`
