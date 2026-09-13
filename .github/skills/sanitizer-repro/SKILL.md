---
name: sanitizer-repro
description: >
  Reproduce and triage sanitizer and Valgrind findings against iccDEV tools with
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

1. For a CodeQL alert, establish profile-to-tool reachability before creating a
   PoC. Trace parser, `Begin()`, and `Apply()` return propagation. If setup
   rejects the field and every tool caller propagates failure, classify the
   alert as a query false positive and add a guarded plus unguarded query test;
   do not force reachability with a direct API call that violates the contract.
2. Build from a clean CMake cache with sanitizer flags.
3. Verify sanitizer linkage before claiming coverage.
4. Run the exact reproduction command and capture exit code plus stderr.
5. Classify exit codes: `0` success, `1-127` graceful failure, `128+` signal.
6. Attribute root cause from sanitizer stack frames, not PoC filenames.
7. Inspect tool argument semantics before writing a one-liner. If a tool
   appends channel numbers, expands prefixes, or parses config files, preserve
   that behavior in the command instead of copying artifacts to synthetic names.
8. For sanitizer noise triage, distinguish runtime suppressions from compile-time
   ignorelists:
   - Use `Testing/silence.txt` only for recoverable runtime UBSAN suppressions.
   - Use `.github/ci/ubsan-ignorelist.txt` plus a rebuild for fatal
     Clang IntegerSanitizer noise from known-benign sites.
   - Verify normal GCC and Clang builds still configure and compile; GCC ignores
     the Clang-only compile-time ignorelist path.
   - Keep patterns narrow. Standard-library implementation paths such as
     `*/include/c++/*/bits/...` can be noise; project-owned `Icc*`, `Tools`,
     `IccConnect`, AFL, and CFL paths stay actionable unless a separate source
     fix or issue proves otherwise.
9. Minimize reproduction steps while keeping them copy-pasteable. When a
   maintainer asks for no substitutions, the command must start with the tool
   binary and use literal arguments only; do not use shell variables, loops,
   `mktemp`, or copy helpers.
10. File or update issues using the canonical security format.

## Uninitialized-memory and race matrix

Do not treat ASAN or UBSAN as coverage for uninitialized reads. Use each lane
for its own signal:

- Run Memcheck and Helgrind against a non-sanitized build. Do not stack
  Valgrind on an ASAN build.
- Run TSan separately for data races; it does not replace MSan or Memcheck.
- Run MSan only when dependent C++ runtime code is also instrumented. A normal
  distro `libstdc++` or `libc++` can stop origin tracking at an STL boundary and
  produce misleading reports.
- Do not suppress a sequence of STL frames to make MSan advance. Build the
  pinned instrumented libc++ runtime, then rerun the same input and controls.

Build the repository runtime and run the JSON plus threaded controls with:

```bash
.github/scripts/iccdev-build-msan-libcxx.sh --prefix /tmp/iccdev-msan-libcxx
.github/scripts/iccdev-msan-taint-qa.sh \
  --source-dir "$PWD" \
  --build-dir /tmp/iccdev-msan-build \
  --runtime-dir /tmp/iccdev-msan-libcxx
```

For Valgrind-assisted taint tracing, configure with
`-DCMAKE_BUILD_TYPE=Debug -DICCDEV_ENABLE_TAINT_TRACE=ON`, set
`ICC_TAINT_TRACE=1`, and use
`.github/scripts/iccdev-taint-trace-qa.sh`. The trace helpers inspect shadow
state before formatting values, so logging must never dereference poisoned or
unaddressable storage. Release, RelWithDebInfo, and MinSizeRel builds always
compile these diagnostics out, even if the option is explicitly requested.
After #2543, malformed parametric-curve arrays are rejection controls; the
nonnumeric colorant PCS fixture remains the positive uninitialized-read proof.

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

For fatal Clang IntegerSanitizer noise that requires the compile-time
ignorelist:

```bash
CC=clang CXX=clang++ cmake -S Build/Cmake -B build-intsan \
  -DENABLE_TOOLS=ON \
  -DENABLE_INTEGER_SANITIZER=ON \
  -DUBSAN_IGNORELIST=.github/ci/ubsan-ignorelist.txt
cmake --build build-intsan --target iccApplyNamedCmm -j"$(nproc)"
```

Verify CMake prints `-fsanitize-ignorelist=` in the final sanitizer flags before
claiming that an ignorelist entry was tested.

## References

- `../../prompts/reproduce-security-issue.prompt.md`
- `../../prompts/file-security-issue.prompt.md`
- `../../prompts/SECURITY_ISSUE_FORMAT.md`
- `../../../docs/bisect.md`
- `../json-config-regression/SKILL.md`
