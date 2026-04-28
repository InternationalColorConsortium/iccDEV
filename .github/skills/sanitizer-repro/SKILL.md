---
name: sanitizer-repro
description: >
  Reproduce and triage ASAN/UBSAN findings against iccDEV tools with
  authoritative exit-code and stack-frame handling.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Sanitizer Reproduction

Use this skill for security advisories, crash reports, fuzzing artifacts, or
manual findings involving iccDEV command-line tools.

## Workflow

1. Build from a clean CMake cache with sanitizer flags.
2. Verify sanitizer linkage before claiming coverage.
3. Run the exact reproduction command and capture exit code plus stderr.
4. Classify exit codes: `0` success, `1-127` graceful failure, `128+` signal.
5. Attribute root cause from sanitizer stack frames, not PoC filenames.
6. Minimize reproduction steps while keeping them copy-pasteable.
7. File or update issues using the canonical security format.

## Build

```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ CXXFLAGS="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -g -O1" LDFLAGS="-fsanitize=address,undefined,integer" cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j"$(nproc)"
nm Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
```

Add `float-divide-by-zero,float-cast-overflow` when investigating floating-point
semantic bugs; those are not covered by `undefined`.

## References

- `../../prompts/reproduce-security-issue.prompt.md`
- `../../prompts/file-security-issue.prompt.md`
- `../../prompts/SECURITY_ISSUE_FORMAT.md`
- `../../../docs/bisect.md`
- `../json-config-regression/SKILL.md`
