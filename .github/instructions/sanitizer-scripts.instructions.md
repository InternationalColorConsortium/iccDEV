---
applyTo: ".github/scripts/**"
---

# Sanitizer Script Instructions

Two sanitizer scripts protect CI output from injection and rendering attacks:

- `.github/scripts/sanitize-sed.sh` for Bash
- `.github/scripts/sanitize.ps1` for PowerShell

Every workflow `run:` step should source the appropriate script before writing
to `GITHUB_STEP_SUMMARY` or `GITHUB_OUTPUT`.

## Bash Functions

| Function | Purpose |
|----------|---------|
| `escape_html STRING` | Encode `& < > " '` as HTML entities |
| `sanitize_line STRING` | Strip control chars and escape HTML for one line |
| `sanitize_print STRING` | Sanitize a block while preserving newlines |
| `sanitize_ref STRING` | Allow only safe git ref characters |
| `sanitize_filename STRING` | Allow only safe path-component characters |

`sanitize_line` emits no trailing newline. For multiline content, iterate line by
line and call `sanitize_line` for each line.

## PowerShell Functions

| Function | Purpose |
|----------|---------|
| `Escape-Html` | Encode HTML entities |
| `Strip-AnsiSequences` | Strip terminal escape sequences |
| `Strip-UnicodeControl` | Strip Unicode formatting controls |
| `Strip-CtrlKeepNewlines` | Remove C0 controls except LF/CR/TAB |
| `Strip-CtrlRemoveNewlines` | Remove all C0 controls |
| `Trim-Whitespace` | Collapse whitespace |
| `Truncate-String` | Apply a length limit |
| `Sanitize-Line` | Single-line safe output |
| `Sanitize-Print` | Block output |
| `Sanitize-Ref` | Git ref sanitization |
| `Sanitize-Filename` | Path component sanitization |
| `Safe-EchoForSummary` | Direct summary write helper |
| `Sanitizer-Version` | Return version string |

## Adding Functions

1. Keep Bash and PowerShell parity when a function is part of workflow policy.
2. Follow existing naming: `snake_case` for Bash and `Verb-Noun` for PowerShell.
3. Handle empty/null input gracefully.
4. Update workflow fallbacks only for functions required before checkout.
