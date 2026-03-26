---
applyTo: ".github/scripts/**"
---

# Sanitizer Scripts — Auto-Loaded Instructions

Two sanitizer scripts protect CI output from injection and rendering attacks.
Both scripts are sourced at the top of every `run:` step per governance rules.

## Bash: `sanitize-sed.sh` (V3, 279 lines)

### Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `escape_html` | `escape_html STRING` | `& < > " '` → HTML entities |
| `sanitize_line` | `sanitize_line STRING` | Strip control chars + escape HTML (single line) |
| `sanitize_print` | `sanitize_print STRING` | Strip control chars + escape HTML (block, preserves newlines) |
| `sanitize_ref` | `sanitize_ref STRING` | Git ref sanitization (alphanumeric + `._-/` only) |
| `sanitize_filename` | `sanitize_filename STRING` | Path component (alphanumeric + `._-` only) |

### Known Behavior

- `sanitize_line` uses `printf '%s'` — **NO trailing newline**
- Passing multiline content to `sanitize_line` collapses all lines to one
- For multiline: iterate with `while IFS= read -r line`
- ANSI escape sequences are stripped via sed
- C0 control characters (0x00-0x1F except LF/TAB) are removed

### Inline Fallback

When the script file may not be checked out (e.g., shallow clone), use:
```bash
if [ -f .github/scripts/sanitize-sed.sh ]; then
  source .github/scripts/sanitize-sed.sh
else
  escape_html() { local s="$1"; s="${s//&/&amp;}"; s="${s//</&lt;}"; s="${s//>/&gt;}"; printf '%s' "$s"; }
  sanitize_line() { escape_html "$1"; }
  sanitize_print() { escape_html "$1"; }
fi
```

## PowerShell: `sanitize.ps1` (V1, 396 lines)

### Functions

| Function | Signature | Purpose |
|----------|-----------|---------|
| `Escape-Html` | `Escape-Html [string]$Text` | `& < > " '` → HTML entities |
| `Strip-CtrlKeepNewlines` | `Strip-CtrlKeepNewlines [string]$Text` | Remove C0 except LF/CR/TAB |
| `Strip-CtrlRemoveNewlines` | `Strip-CtrlRemoveNewlines [string]$Text` | Remove ALL C0 including LF |
| `Trim-Whitespace` | `Trim-Whitespace [string]$Text` | Collapse whitespace |
| `Truncate-String` | `Truncate-String [string]$Text -MaxLen N` | Length limit with `…` suffix |
| `Sanitize-Line` | `Sanitize-Line [string]$Text` | Single-line safe output |
| `Sanitize-Print` | `Sanitize-Print [string]$Text` | Block output (keeps newlines) |
| `Sanitize-Ref` | `Sanitize-Ref [string]$Text` | Git ref sanitization |
| `Sanitize-Filename` | `Sanitize-Filename [string]$Text` | Path component sanitization |
| `Safe-EchoForSummary` | `Safe-EchoForSummary [string]$Text` | Direct GITHUB_STEP_SUMMARY write |
| `Sanitizer-Version` | `Sanitizer-Version` | Returns version string |

### Usage

```powershell
. .github/scripts/sanitize.ps1
$safe = Sanitize-Line $env:MATRIX_BUILD_TYPE
Add-Content $env:GITHUB_STEP_SUMMARY "Build: $safe"
```

## Adding New Functions

1. Add to BOTH `sanitize-sed.sh` AND `sanitize.ps1` (keep parity)
2. Follow existing naming: `snake_case` (bash) / `Verb-Noun` (PowerShell)
3. All functions must handle empty/null input gracefully
4. Test with: `echo '<script>alert(1)</script>' | sanitize_line`
5. Update the inline fallback if the function is required in shallow-clone scenarios
