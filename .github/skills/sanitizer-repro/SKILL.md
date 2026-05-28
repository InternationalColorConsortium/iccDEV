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
6. Inspect tool argument semantics before writing a one-liner. If a tool
   appends channel numbers, expands prefixes, or parses config files, preserve
   that behavior in the command instead of copying artifacts to synthetic names.
7. Minimize reproduction steps while keeping them copy-pasteable. When a
   maintainer asks for no substitutions, the command must start with the tool
   binary and use literal arguments only; do not use shell variables, loops,
   `mktemp`, or copy helpers.
8. File or update issues using the canonical security format.

## Build

```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON
make -j"$(nproc)"
nm Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
```

Use `CC=clang`, not `C=clang`; after any failed compiler configure, delete both
`CMakeCache.txt` and `CMakeFiles/` before retrying. Do not enable coverage for a
sanitizer reproduction because coverage instrumentation can mask findings.

## References

- `../../prompts/reproduce-security-issue.prompt.md`
- `../../prompts/file-security-issue.prompt.md`
- `../../prompts/SECURITY_ISSUE_FORMAT.md`
- `../../../docs/bisect.md`
- `../json-config-regression/SKILL.md`
