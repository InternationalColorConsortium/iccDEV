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

## Metadata

<!-- Machine-parseable block -- agents extract CWE, file:line, sanitizer type -->
- **CWE**: <!-- e.g., CWE-122 (heap-buffer-overflow) -->
- **File**: <!-- e.g., IccCmmSearch.cpp:112 -->
- **Function**: <!-- e.g., CIccApplyCmmSearch::costFunc() -->
- **Sanitizer**: <!-- ASAN (heap-buffer-overflow READ 8) | UBSAN | IntSan (unsigned-integer-overflow) -->
- **Bisect**: <!-- SHA (YYYY-MM-DD) -->
- **SCARINESS**: <!-- Score from ASAN print_scariness=1, or N/A -->
- **Severity**: <!-- Critical (>=40) / High (20-39) / Medium (<20) / Info -->
- **Affected tool(s)**: <!-- e.g., iccDumpProfile, iccToXml -->

## Git

<!-- Exact commit hash -- get from: git log --oneline -1 -->
```
<commit_hash> (HEAD -> <branch>)
```

## Build

<!-- Standard sanitizer build -- include -fsanitize=integer for UIO bugs -->
```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j$(nproc)
```

## PoC Replay

<!-- Numbered steps -- must be copy-paste reproducible -->
<!-- Host PoC files on GitHub or CDN with wget/curl URLs -->

Step 1. wget <PoC_file_URL>

Step 2. ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1 <tool> <poc_file> <args>

## PoC Output

<!-- Paste sanitizer output: SUMMARY line + stack frames #0-#4 -->
<!-- CUT shadow byte legend (17 lines of noise) unless UAF/double-free -->
<!-- KEEP SCARINESS score line when present -->
```

```

## Patch

<!-- Optional -- include unified diff if you have a fix -->
<!-- Reference: Gold Standard issues #753, #769 -->
```diff

```

## Patched Output

<!-- Optional -- prove the fix works (clean exit, no ASAN/UBSAN) -->
```

```

## Notes

<!-- Any additional context: related issues, upstream references, spec citations -->
<!-- For Won't Fix: cite the specification (e.g., RFC 1321) under a ## Won't Fix heading -->
<!-- One bug per issue -- do NOT bundle multiple independent findings -->
