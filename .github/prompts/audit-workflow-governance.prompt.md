# Audit Workflow Governance Compliance

Use this prompt to audit any GitHub Actions workflow for compliance.

## Audit Checklist

For EVERY `run:` step in the workflow, verify:

### 1. Shell Hardening
- [ ] `shell: bash --noprofile --norc {0}` (bash) or `pwsh -NoProfile -NoLogo -NonInteractive -Command {0}` (PowerShell)
- [ ] `BASH_ENV: /dev/null` in env (bash steps)
- [ ] `set -euo pipefail` as first command (bash)
- [ ] `$ErrorActionPreference = 'Stop'` (PowerShell)

### 2. Credential Reduction
- [ ] `git config --global credential.helper ""`
- [ ] `unset GITHUB_TOKEN || true` (bash) or `Remove-Item env:GITHUB_TOKEN` (PowerShell)

### 3. Sanitizer Loaded
- [ ] `source .github/scripts/sanitize-sed.sh` (bash) with inline fallback
- [ ] `. .github/scripts/sanitize.ps1` (PowerShell)

### 4. Expression Injection
- [ ] No `${{ matrix.* }}` directly in `run:` blocks — must use `env:` passthrough
- [ ] No `${{ github.event.*.title }}` or other user-controllable contexts in `run:`
- [ ] No `${{ github.head_ref }}` in `run:` blocks

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

## Running the Audit

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

## Severity Classification

| Severity | Finding | Example |
|----------|---------|---------|
| **CRITICAL** | Expression injection in `run:` | `${{ matrix.build_type }}` in shell |
| **CRITICAL** | Missing sanitizer for summary | Raw `>> $GITHUB_STEP_SUMMARY` |
| **HIGH** | No shell hardening | Missing `--noprofile --norc` |
| **HIGH** | No credential reduction | Missing `unset GITHUB_TOKEN` |
| **MEDIUM** | Excessive permissions | `contents: write` when read suffices |
| **LOW** | Missing concurrency group | No `concurrency:` block |

## Reference Workflows

- **Bash gold standard**: `.github/workflows/ci-pr-action.yml`
- **PowerShell gold standard**: `.github/workflows/ci-pr-win.yml`
- **Cross-platform release**: `.github/workflows/ci-latest-release.yml`
