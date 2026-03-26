---
applyTo: ".github/workflows/**"
---

# Workflow Governance — Auto-Loaded Instructions

These rules apply to ALL GitHub Actions workflow files in this repository.
Reference implementation: `ci-pr-action.yml` (bash) · `ci-pr-win.yml` (PowerShell).

## Shell Hardening (MANDATORY — every `run:` block)

### Bash Steps

```yaml
shell: bash --noprofile --norc {0}
env:
  BASH_ENV: /dev/null
run: |
  set -euo pipefail
  git config --global credential.helper ""
  unset GITHUB_TOKEN || true
  if [ -f .github/scripts/sanitize-sed.sh ]; then
    source .github/scripts/sanitize-sed.sh
  else
    escape_html() { local s="$1"; s="${s//&/&amp;}"; s="${s//</&lt;}"; s="${s//>/&gt;}"; printf '%s' "$s"; }
    sanitize_line() { escape_html "$1"; }
    sanitize_print() { escape_html "$1"; }
  fi
```

### PowerShell Steps

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

## Expression Injection Prevention

**CRITICAL**: Never place `${{ }}` expressions directly in `run:` blocks.
GitHub Actions evaluates expressions BEFORE the shell — an attacker-controlled
value (branch name, PR title, matrix variable) becomes arbitrary shell code.

```yaml
# ❌ WRONG — injectable
run: echo "Building ${{ matrix.build_type }}"

# ✅ CORRECT — pass through env
env:
  BUILD_TYPE: ${{ matrix.build_type }}
run: echo "Building ${BUILD_TYPE}"
```

**Also applies to**: `github.event.pull_request.title`, `github.event.issue.title`,
`github.head_ref`, `github.event.comment.body`, and ALL user-controllable contexts.

## Output Sanitization

Every write to `GITHUB_STEP_SUMMARY` or `GITHUB_OUTPUT` MUST use sanitizer functions:

| Bash | PowerShell | Purpose |
|------|-----------|---------|
| `sanitize_line "$var"` | `Sanitize-Line $var` | Single-line HTML-safe output |
| `sanitize_print "$var"` | `Sanitize-Print $var` | Block output (preserves structure) |
| `sanitize_ref "$var"` | `Sanitize-Ref $var` | Git ref sanitization |
| `sanitize_filename "$var"` | `Sanitize-Filename $var` | Path component sanitization |
| `escape_html "$var"` | `Escape-Html $var` | Raw HTML entity encoding |

### Multiline Variables

`sanitize_line` outputs NO trailing newline. For multiline content, iterate:

```bash
# ❌ WRONG — collapses to single line
sanitize_line "$MULTILINE_VAR"

# ✅ CORRECT — preserves line-per-line formatting
echo "$MULTILINE_VAR" | while IFS= read -r line; do
  sanitize_line "$line"
  echo ""
done
```

PowerShell equivalent:
```powershell
$multiline -split "`n" | ForEach-Object { Sanitize-Line $_ }
```

## Permissions

Use least-privilege `permissions:` at job level (not workflow level when possible):

```yaml
permissions:
  contents: read
  pull-requests: read
  # Add write only when genuinely needed (e.g., release uploads)
```

## Credential Hygiene

Every step must clear credentials before running user-controllable logic:
- `git config --global credential.helper ""`
- `unset GITHUB_TOKEN || true` (bash) / `Remove-Item env:GITHUB_TOKEN` (PowerShell)

## Concurrency

PR workflows must use concurrency groups to cancel stale runs:
```yaml
concurrency:
  group: "ci-${{ github.workflow }}-${{ github.event.pull_request.number || github.sha }}"
  cancel-in-progress: true
```

## GitHub Actions Expression Parser Quirks

- `${{ }}` with empty braces is a **parse error** even inside bash `#` comments
- GitHub evaluates ALL `${{ }}` in YAML string values before bash sees them
- YAML-level comments (outside `run:` blocks) are stripped first and are safe
- `defaults.run.env` does NOT exist — use per-step or job-level `env:`
