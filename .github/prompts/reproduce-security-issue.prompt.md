# Reproduce a Security Advisory

Use this prompt when triaging an iccDEV security advisory (GHSA/CVE) or
investigating a crash reported against an ICC profile or image file.

## Examples
- https://github.com/InternationalColorConsortium/iccDEV/issues/696 https://github.com/InternationalColorConsortium/iccDEV/issues/700 https://github.com/InternationalColorConsortium/iccDEV/issues/734 https://github.com/InternationalColorConsortium/iccDEV/issues/753

## Step 0: Establish Tool Reachability

For a CodeQL report, trace the untrusted field from its parser through setup and
the affected operation before synthesizing an artifact. Check whether `Init()`,
`Read()`, or `Begin()` bounds the field and whether every command-line tool
propagates a failed setup result. A direct library call that skips a required
setup method does not prove profile-to-tool reachability.

If the path is blocked, record the guard and call-chain evidence, add a CodeQL
query test with a guarded case and an unguarded control, and report a false
positive instead of creating a PoC. Alerts #999, #1000, and #1647 are the
reference case for a `Begin()`-established Apply invariant.

## Step 1: Identify the Advisory

```bash
# List all advisories
gh api --paginate "repos/InternationalColorConsortium/iccDEV/security-advisories" \
  --jq '.[] | "\(.ghsa_id) \(.cve_id // "no-CVE") \(.summary)"'

# Get details for a specific advisory
gh api "repos/InternationalColorConsortium/iccDEV/security-advisories/GHSA-xxxx-xxxx-xxxx"
```

## Step 2: Build with Sanitizers

The preset file is `Build/Cmake/CMakePresets.json`; run preset commands from
that directory. From the repository root, the canonical ASan, TSan, and MSan
copy/paste one-liners are:

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

Verify the selected runtime before claiming coverage:

```bash
nm -D out/linux-clang-asan/Tools/IccDumpProfile/iccDumpProfile | grep __asan_init
nm -D out/linux-clang-tsan/Tools/IccApplyProfiles/iccApplyProfiles | grep __tsan_init
ldd out/linux-clang-msan/Tools/IccFromXml/iccFromXml && nm -D out/linux-clang-msan/Tools/IccFromXml/iccFromXml | grep __msan_init
```

**Note**: `-fsanitize=integer` detects unsigned integer overflow (wrapping),
which is well-defined per C/C++ standard and NOT caught by `undefined` alone.
Issue #769 required this flag. MSVC does not support it. Do not add coverage
flags or `-DENABLE_COVERAGE=ON` when reproducing sanitizer findings; coverage
instrumentation can mask a bug.

For the historical combined ASan, UBSan, IntSan, and float configuration, use
a separate out-of-tree build:

```bash
cmake -S Build/Cmake -B out/linux-clang-sanitizers -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON && cmake --build out/linux-clang-sanitizers --parallel "$(nproc)"
```

For uninitialized-memory findings, do not keep suppressing frames when a JSON
or threaded run stops in `libstdc++` or an ordinary distro `libc++`. That is an
uninstrumented runtime boundary, not proof that the application frame is safe
or unsafe. Build the pinned MSan libc++ runtime and run the paired controls:

```bash
.github/scripts/iccdev-build-msan-libcxx.sh --prefix /tmp/iccdev-msan-libcxx
.github/scripts/iccdev-msan-taint-qa.sh \
  --source-dir "$PWD" \
  --build-dir /tmp/iccdev-msan-build \
  --runtime-dir /tmp/iccdev-msan-libcxx
```

The taint-trace option is diagnostic-only. Configure it in Debug; Release,
RelWithDebInfo, and MinSizeRel compile it out even when explicitly requested.
Treat malformed parametric-curve arrays as fail-closed controls after #2543;
use the nonnumeric colorant PCS fixture as the positive uninitialized-read case.

Keep the matrix distinct: ASAN/UBSAN for memory safety and undefined behavior,
MSan or Memcheck for uninitialized reads, and TSan or Helgrind for races. Run
Valgrind only on a non-sanitized binary.

## Step 3: Reproduce

Before writing the one-liner, inspect the affected tool's argument handling.
Some tools transform path arguments before opening files. For example,
`iccSpecSepToTiff` treats `infile_fmt` as a prefix and appends the channel
number, so a file ending in `spec_3` is reproduced with prefix `spec_` and
`start=end=3`.

```bash
# Set sanitizer options for catch-and-continue (full crash chain)
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1
export LD_LIBRARY_PATH=IccProfLib:IccXML

# Test with DumpProfile (most common crash surface)
Tools/IccDumpProfile/iccDumpProfile poc-file.icc ALL

# Test with ToXml (XML serialization crashes)
Tools/IccToXml/iccToXml poc-file.icc /dev/null

# Test with RoundTrip (CMM Apply crashes)
Tools/IccRoundTrip/iccRoundTrip poc-file.icc

# For TIFF-embedded ICC crashes
Tools/IccTiffDump/iccTiffDump poc-file.tif

# For multi-profile workflows
Tools/IccApplyNamedCmm/iccApplyNamedCmm -cfg config.json
```

## Step 4: Classify the Finding

Read the ASAN/UBSAN stack trace -- frames #2-#3 identify the bug location:

| ASAN Error Type | CWE | Severity |
|-----------------|-----|----------|
| heap-buffer-overflow | CWE-122 | CRITICAL |
| stack-buffer-overflow | CWE-121 | CRITICAL |
| use-after-free | CWE-416 | CRITICAL |
| alloc-dealloc-mismatch | CWE-762 | HIGH |
| null-pointer-deref (SEGV) | CWE-476 | HIGH |
| runtime error: signed integer overflow | CWE-190 | HIGH |
| runtime error: unsigned integer overflow | CWE-190 | HIGH |
| runtime error: outside range of representable values | CWE-681 | HIGH |
| runtime error: member access within null pointer | CWE-476 | HIGH |

## Step 5: Determine Root Cause

Key questions:
1. Which `Read()` function loaded the malformed data without validation?
2. Which field from the file controlled the crash (size, offset, count, enum)?
3. Is the fix in `Read()` (input validation) or `Describe()`/`Apply()` (defensive check)?

Common patterns:
- **Uncapped allocation**: `new T[fileControlledCount]` without size check
- **Missing null termination**: Fixed-size string fields read without `[N-1]='\0'`
- **NaN bypass**: `if(v<0)...if(v>1)` doesn't catch NaN (IEEE 754)
- **Enum out-of-range**: `(icSomeEnum)rawUint32` without range validation
- **Offset overflow**: `offset + size` wraps past `profileSize`

## Step 6: Create Minimal PoC

```bash
# If the crash file is large, minimize it while preserving the crash.
# Copy the file, remove non-essential tags or payload bytes, then verify the
# same tool still produces the same sanitizer finding.
```

## Step 7: File Upstream

Create an issue with:
1. Clear title: `[CWE-NNN] Type: Location in File.cpp:LineN`
2. ASAN/UBSAN output (verbatim)
3. Minimal PoC file attached (rename `.icc` -> `.icc.txt` for GitHub)
4. One-liner reproduction command
5. Suggested fix (if known)

For maintainer-facing repros, prefer a direct program invocation with absolute
paths and literal arguments. Avoid wrapper harnesses, shell substitutions, temp
copies, or filtered output unless the report explicitly requires them.

```bash
# Example upstream report format:
# Title: [CWE-122] Heap-buffer-overflow in CIccTagColorantTable::Describe()
# Body: Steps + ASAN output + PoC file + fix suggestion
gh issue create --repo InternationalColorConsortium/iccDEV \
  --title "[CWE-122] HBO in CIccTagColorantTable::Describe()" \
  --body-file /tmp/issue-body.md
```

## Exit Code Reference

| Exit Code | Meaning | Is it a crash? |
|-----------|---------|----------------|
| 0 | Success | No |
| 1-127 | Graceful rejection | **No** |
| 134 | SIGABRT (ASan abort) | Yes |
| 136 | SIGFPE | Yes |
| 137 | SIGKILL (OOM) | Yes (resource exhaustion) |
| 139 | SIGSEGV | Yes |
