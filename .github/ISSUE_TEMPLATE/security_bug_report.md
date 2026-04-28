---
name: Security bug report
about: Report a memory-safety bug, sanitizer finding, or fuzzer crash
title: '{CrashType} in {Function}() at {File}:{Line}'
labels: 'Bug'
assignees: ''
---

Use one issue per independent finding. For detailed formatting guidance, see
`.github/prompts/SECURITY_ISSUE_FORMAT.md`.

## Maintainer Repro

<!-- UTC timestamp when you confirmed reproduction -->

## Metadata

- **CWE**:
- **File**:
- **Function**:
- **Sanitizer**:
- **Bisect**:
- **Severity**:
- **Affected tool(s)**:

## Git

```text
<commit_hash> (HEAD -> <branch>)
```

## Build

<!-- Link to the build used, or paste only the non-standard flags. -->

## PoC Replay

1. Download or create the PoC input:
2. Run the affected tool with exact arguments:

## PoC Output

```text
<!-- Paste the SUMMARY line and stack frames #0-#4. Omit shadow-byte legend unless needed. -->
```

## Patch or Mitigation

```diff
<!-- Optional -->
```

## Patched Output

```text
<!-- Optional: clean output after fix -->
```
