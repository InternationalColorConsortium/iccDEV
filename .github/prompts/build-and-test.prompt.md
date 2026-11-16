# Build & Test iccDEV

Use this prompt to build, test, and validate iccDEV across platforms.

## Quick Build (Ubuntu)

```bash
cd Build
cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
make -j"$(nproc)"
```

## Sanitizer Builds

`CMakePresets.json` is under `Build/Cmake`, so run preset commands from that
directory. These are independent copy/paste one-liners from the repository
root:

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

Verify the selected runtime before testing:

```bash
nm -D out/linux-clang-asan/Tools/IccDumpProfile/iccDumpProfile | grep __asan_init
nm -D out/linux-clang-tsan/Tools/IccApplyProfiles/iccApplyProfiles | grep __tsan_init
ldd out/linux-clang-msan/Tools/IccFromXml/iccFromXml && nm -D out/linux-clang-msan/Tools/IccFromXml/iccFromXml | grep __msan_init
```

**Why `-fsanitize=integer`**: Unsigned integer overflow is well-defined
(wrapping) per C/C++ standard. UBSAN's `undefined` group does NOT detect it.
The `integer` group adds: `unsigned-integer-overflow`,
`implicit-unsigned-integer-truncation`, `implicit-signed-integer-truncation`,
`implicit-integer-sign-change`. Issue #769 is ONLY detectable with this flag.
MSVC does not support `-fsanitize=integer` -- Clang only. Do not enable coverage
for sanitizer reproduction because coverage instrumentation can mask findings.

Run with sanitizers:
```bash
ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 \
UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1 \
  out/linux-clang-asan/Tools/IccDumpProfile/iccDumpProfile profile.icc ALL
```

## WASM Build

```bash
# Install emsdk (one-time)
git clone https://github.com/nicholasgasior/emsdk.git /tmp/emsdk
cd /tmp/emsdk && ./emsdk install latest && ./emsdk activate latest
source /tmp/emsdk/emsdk_env.sh

# Build libxml2 for WASM
emcmake cmake -S /tmp/libxml2-src -B /tmp/libxml2-build \
  -DBUILD_SHARED_LIBS=OFF -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_LZMA=OFF
cmake --build /tmp/libxml2-build -j"$(nproc)"

# Build iccDEV for WASM
cd Build
emcmake cmake Cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_TESTS=OFF -DENABLE_TOOLS=ON \
  -DENABLE_SHARED_LIBS=OFF -DENABLE_STATIC_LIBS=ON \
  -DLIBXML2_ROOT=/tmp/libxml2-build
emmake make -j"$(nproc)"
```

## Full Test Cycle

```bash
# 1. Generate test profiles
Testing/CreateAllProfiles.sh

# 2. Run validation
Testing/RunTests.sh

# 3. Spot-check a profile
./Tools/IccDumpProfile/iccDumpProfile Testing/Display/sRGBD65.icc ALL

# 4. Round-trip test
./Tools/IccRoundTrip/iccRoundTrip Testing/Display/sRGBD65.icc
```

## CMake Options Reference

| Option | Purpose |
|--------|---------|
| `-DENABLE_TESTS=ON` | Build test targets |
| `-DENABLE_SANITIZERS=ON` | ASan + UBSan + IntSan + float checks |
| `-DENABLE_ASAN=ON` | AddressSanitizer only |
| `-DENABLE_UBSAN=ON` | UBSan only |
| `-DENABLE_INTEGER_SANITIZER=ON` | IntegerSanitizer (unsigned overflow, Clang only) |
| `-DENABLE_FLOAT_SANITIZER=ON` | `float-divide-by-zero,float-cast-overflow` |
| `-DENABLE_TSAN=ON` | ThreadSanitizer (conflicts with ASan) |
| `-DENABLE_MSAN=ON` | Low-level MemorySanitizer instrumentation; reportable runs require the instrumented runtime helper |
| `-DENABLE_COVERAGE=ON` | LLVM source-based coverage |
| `-DENABLE_PROFILING=ON` | gprof/perf `-pg` profiling |
| `-DENABLE_FUZZING=ON` | LibFuzzer harnesses |
| `-DENABLE_ICCXML=ON` | IccXML library |

For actionable MemorySanitizer results, build the pinned instrumented libc++,
libc++abi, and libxml2 runtime and run the focused controls:

```bash
.github/scripts/iccdev-build-msan-libcxx.sh --prefix "$PWD/out/msan-runtime" && .github/scripts/iccdev-msan-taint-qa.sh --source-dir "$PWD" --build-dir "$PWD/out/linux-clang-msan-taint" --runtime-dir "$PWD/out/msan-runtime" --out-dir "$PWD/out/msan-taint-evidence"
```

Equivalent standalone analysis presets are `linux-clang-tsan`,
`linux-clang-msan` (requires `ICCDEV_MSAN_LIBCXX_DIR`), and
`linux-clang-valgrind`. The Valgrind preset is deliberately non-sanitized.
Do not run `cmake --preset ... -S Build/Cmake` from the repository root: preset
discovery happens before `-S` is applied. Run the subshell forms above.

## vcpkg Port Build

Build and install the core libraries + tools via vcpkg overlay:

```bash
# Set local source mode (avoids GitHub API download)
export VCPKG_ICCDEV_SOURCE=$(pwd)
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE

# Install
vcpkg install iccdev \
  --overlay-ports=ports \
  --triplet x64-linux \
  --classic

# Verify
vcpkg list | grep iccdev
```

This builds static libraries, feature-selected tools, and the CMake config.
See `ports/iccdev/portfile.cmake` for the full build configuration.

## Coverage Report

```bash
CC=clang-22 CXX=clang++-22 cmake -S Build/Cmake -B out/coverage \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TESTS=ON \
  -DENABLE_COVERAGE=ON
cmake --build out/coverage --parallel
mkdir -p out/coverage-profraw
LLVM_PROFILE_FILE="$PWD/out/coverage-profraw/ctest-%4m.profraw" \
  ctest --test-dir out/coverage -R '^iccdev\.hybrid-pipeline$' \
    --output-on-failure --no-tests=error
LLVM_COV=llvm-cov-22 LLVM_PROFDATA=llvm-profdata-22 \
  Build/Cmake/Testing/iccdev-matched-coverage-report.sh \
    out/coverage out/coverage-profraw out/coverage-report
```

For the complete workflow and interpretation rules, use
`docs/code-coverage.md`.
