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
- Run MSan only when dependent runtime code is also instrumented. A normal
  distro `libstdc++`, `libc++`, or `libxml2` can leave destination shadow bytes
  poisoned after initializing them and produce misleading reports.
- Do not suppress a sequence of STL frames to make MSan advance. Build the
  pinned instrumented libc++ runtime, then rerun the same input and controls.

Build the repository runtime and run the JSON, XML, and threaded controls with:

```bash
.github/scripts/iccdev-build-msan-libcxx.sh --prefix /tmp/iccdev-msan-libcxx
.github/scripts/iccdev-msan-taint-qa.sh \
  --source-dir "$PWD" \
  --build-dir /tmp/iccdev-msan-build \
  --runtime-dir /tmp/iccdev-msan-libcxx
```

Despite its historical name, `iccdev-build-msan-libcxx.sh` installs pinned,
instrumented libc++, libc++abi, and libxml2. A plain `-DENABLE_MSAN=ON` build
instruments iccDEV itself but not distribution dependencies; do not classify
reports from those libraries as iccDEV bugs.

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

Run preset commands from `Build/Cmake`, because that is where
`CMakePresets.json` resides. `-S Build/Cmake` does not make a preset visible
when the command starts at the repository root. These are the canonical
copy/paste one-liners:

```bash
(cd Build/Cmake && cmake --preset linux-clang-asan) && cmake --build out/linux-clang-asan --parallel "$(nproc)"
(cd Build/Cmake && cmake --preset linux-clang-tsan) && cmake --build out/linux-clang-tsan --parallel "$(nproc)"
.github/scripts/iccdev-build-msan-libcxx.sh --prefix "$PWD/out/msan-runtime" && export ICCDEV_MSAN_LIBCXX_DIR="$PWD/out/msan-runtime" && (cd Build/Cmake && cmake --preset linux-clang-msan) && cmake --build out/linux-clang-msan --parallel "$(nproc)"
```

The direct CMake equivalents are:

```bash
cmake -S Build/Cmake -B out/linux-clang-asan-manual -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_TESTS=ON -DENABLE_TOOLS=ON -DENABLE_WXWIDGETS=OFF -DENABLE_LTO=OFF && cmake --build out/linux-clang-asan-manual --parallel "$(nproc)"
cmake -S Build/Cmake -B out/linux-clang-tsan-manual -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -DENABLE_TESTS=ON -DENABLE_TOOLS=ON -DENABLE_WXWIDGETS=OFF -DENABLE_LTO=OFF && cmake --build out/linux-clang-tsan-manual --parallel "$(nproc)"
export ICCDEV_MSAN_LIBCXX_DIR="$PWD/out/msan-runtime" && cmake -S Build/Cmake -B out/linux-clang-msan-manual -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DENABLE_MSAN=ON -DENABLE_TESTS=ON -DENABLE_TOOLS=ON -DENABLE_WXWIDGETS=OFF -DENABLE_IMAGE_TOOLS=OFF -DICC_USE_ZLIB=OFF -DENABLE_LTO=OFF -DCMAKE_C_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins" -DCMAKE_CXX_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins -nostdinc++ -isystem $ICCDEV_MSAN_LIBCXX_DIR/include/c++/v1" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins -nostdlib++ -L$ICCDEV_MSAN_LIBCXX_DIR/lib -Wl,-rpath,$ICCDEV_MSAN_LIBCXX_DIR/lib -lc++ -lc++abi" -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=memory -fsanitize-memory-track-origins -nostdlib++ -L$ICCDEV_MSAN_LIBCXX_DIR/lib -Wl,-rpath,$ICCDEV_MSAN_LIBCXX_DIR/lib -lc++ -lc++abi" -DLIBXML2_INCLUDE_DIR="$ICCDEV_MSAN_LIBCXX_DIR/include/libxml2" -DLIBXML2_LIBRARY="$ICCDEV_MSAN_LIBCXX_DIR/lib/libxml2.so" && cmake --build out/linux-clang-msan-manual --parallel "$(nproc)"
```

Verify each runtime before claiming coverage:

```bash
nm -D out/linux-clang-asan/Tools/IccDumpProfile/iccDumpProfile | grep __asan_init
nm -D out/linux-clang-tsan/Tools/IccApplyProfiles/iccApplyProfiles | grep __tsan_init
ldd out/linux-clang-msan/Tools/IccFromXml/iccFromXml && nm -D out/linux-clang-msan/Tools/IccFromXml/iccFromXml | grep __msan_init
```

Use `CC=clang`, not `C=clang`; after any failed compiler configure, delete both
`CMakeCache.txt` and `CMakeFiles/` before retrying. Do not enable coverage for a
sanitizer reproduction because coverage instrumentation can mask findings.
Resolve the selected build directory before changing into `Testing`, prepend
all of its immediate `Tools` subdirectories to `PATH`, and assert that
`command -v` names the selected tree. A command that merely leaves an unmatched
`Tools/*` glob in `PATH` is not evidence that the tool was found. Run every
documented test command independently, record its exit status, and inspect its
complete log for sanitizer diagnostics; a wrapper exit code alone is
insufficient.

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
