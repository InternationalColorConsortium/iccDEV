# File a Security Issue on iccDEV

Use this prompt when sanitizer output, fuzzing, or manual testing identifies a
memory-safety bug or parser robustness issue that should be filed upstream.

Follow `.github/prompts/SECURITY_ISSUE_FORMAT.md` for the canonical issue
structure. Keep the filed issue concise, reproducible, and machine-readable.

## Required Inputs

- Exact iccDEV commit
- Affected tool and command-line arguments
- PoC file or command that creates the PoC
- Sanitizer output with the `SUMMARY:` line and stack frames #0-#4
- CWE and severity assessment when known
- Patch diff and clean output when a fix is available

## Issue Rules

- File one independent bug per issue.
- Derive title location from sanitizer frames, not from profile filenames.
- Use the shortest command sequence that reproduces the finding.
- Cut ASAN shadow-byte legends unless the issue is UAF or double-free.
- Include `SCARINESS` when ASAN was run with `print_scariness=1`.
- For bisected regressions with a fix, use title format `Bisect: {sha7} {type}`.

## Gold Standard References

- https://github.com/InternationalColorConsortium/iccDEV/issues/700
- https://github.com/InternationalColorConsortium/iccDEV/issues/753
- https://github.com/InternationalColorConsortium/iccDEV/issues/794
