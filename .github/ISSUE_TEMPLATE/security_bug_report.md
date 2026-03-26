---
name: Security bug report
about: Report a memory safety bug, sanitizer finding, or crash (ASAN/UBSAN/fuzzer)
title: '{CrashType} in {Function}() at {File}:{Line}'
labels: 'Bug'
assignees: ''

---

## Maintainer Repro

<!-- UTC timestamp of when you confirmed reproduction -->
YYYY-MM-DD HH:MM:SS UTC

## Git

<!-- Exact commit hash — get from: git log --oneline -1 -->
```
<commit_hash> (HEAD -> <branch>)
```

## PoC Replay

<!-- Numbered steps — must be copy-paste reproducible -->
<!-- Host PoC files on GitHub or CDN with wget/curl URLs -->

Step 1. wget <PoC_file_URL>

Step 2. ASAN_OPTIONS=print_scariness=1:halt_on_error=0:detect_leaks=0 <tool> <poc_file> <args>

## PoC Output

<!-- Paste COMPLETE sanitizer output — do NOT summarize or truncate -->
<!-- Must include SUMMARY line, shadow bytes, and SCARINESS score -->
```

```

## Patch

<!-- Optional — include unified diff if you have a fix -->
<!-- Reference: Gold Standard issue #700 -->
```diff

```

## Patched Output

<!-- Optional — prove the fix works (clean exit, no ASAN/UBSAN) -->
```

```

## Classification

- **Crash type**: <!-- HBO / SBO / NPD / UB / UAF / SEGV / OOM / SO -->
- **CWE**: <!-- e.g., CWE-122 (heap-buffer-overflow) -->
- **SCARINESS**: <!-- Score from ASAN print_scariness=1, or N/A for UBSAN -->
- **Severity**: <!-- Critical (≥40) / Moderate (20-39) / Low (<20) -->
- **Affected tool(s)**: <!-- e.g., iccDumpProfile, iccToXml -->

## Notes

<!-- Any additional context: related issues, upstream references, spec citations -->
<!-- For Won't Fix: cite the specification (e.g., RFC 1321) under a ## Won't Fix heading -->
