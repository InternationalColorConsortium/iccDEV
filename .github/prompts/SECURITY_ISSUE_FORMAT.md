# Security Issue Format

Use this reference for ASAN, UBSAN, IntegerSanitizer, fuzzer, and manual
memory-safety findings.

## Title

```text
{CrashType} in {Function}() at {File}:{Line}
```

Common crash types:

| Abbreviation | Finding |
|--------------|---------|
| `HBO` | heap-buffer-overflow |
| `SBO` | stack-buffer-overflow |
| `NPD` | null-pointer dereference |
| `UIO` | unsigned-integer-overflow |
| `UB` | undefined behavior |
| `UAF` | use-after-free |
| `SEGV` | segmentation fault |
| `OOM` | out-of-memory |
| `SO` | stack overflow |

For bisected regressions with a patch, use:

```text
Bisect: {sha7} {type}
```

## Body

### Maintainer Repro

Use the UTC timestamp when the reproduction was confirmed.

### Metadata

```markdown
- CWE: CWE-{N}
- File: {file}:{line}
- Function: {class}::{method}()
- Sanitizer: {ASAN|UBSAN|IntSan} ({error type})
- Bisect: {SHA} ({date})
- Severity: {Critical|High|Medium|Low}
- Affected tool(s): {tool list}
```

### Vuln

Use this compact block when the finding needs vulnerability triage metadata:

```markdown
CWE-{NNN} | {TOOL}:{finding-type} | {File.cpp}:{line} | {Class::Method()}
CVSS:3.1/AV:L/AC:L/PR:N/UI:R/S:U/C:{c}/I:{i}/A:{a} = {score}
CPE: cpe:2.3:a:icc:reficcmax:*:*:*:*:*:*:*:*
Introduced: {sha7} ({YYYY-MM-DD}) | Affected: <={version}
Vector: {1-line attack narrative}
```

### Git

Pin the exact commit:

```text
{commit_hash} (HEAD -> {branch}, origin/{branch})
```

### PoC Replay

Provide numbered, copy-pasteable steps. Host PoC files where maintainers can
download them with `wget` or `curl`.

### PoC Output

Include:

- sanitizer `SUMMARY:` line
- stack frames #0-#4
- `SCARINESS` when available
- shadow-byte legend only for UAF or double-free

### Patch and Patched Output

When a fix is available, include the smallest relevant diff and the clean
sanitizer output after the fix.
