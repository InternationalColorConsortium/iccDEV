# Build & Test iccDEV

Use this prompt to build, test, and validate iccDEV across platforms.

## Quick Build (Ubuntu)

```bash
cd Build
cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
make -j"$(nproc)"
```

## Sanitizer Build

```bash
cd Build
cmake Cmake \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON \
  -DENABLE_COVERAGE=ON
make -j"$(nproc)"
```

Run with sanitizers:
```bash
ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 \
UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1 \
  ./Tools/IccDumpProfile/iccDumpProfile profile.icc ALL
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
| `-DENABLE_SANITIZERS=ON` | ASan + UBSan |
| `-DENABLE_ASAN=ON` | AddressSanitizer only |
| `-DENABLE_UBSAN=ON` | UBSan only |
| `-DENABLE_TSAN=ON` | ThreadSanitizer (conflicts with ASan) |
| `-DENABLE_COVERAGE=ON` | LLVM source-based coverage |
| `-DENABLE_FUZZING=ON` | LibFuzzer harnesses |
| `-DENABLE_ICCXML=ON` | IccXML library |

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

This builds: IccProfLib2-static, IccXML2-static, 9 CLI tools, CMake config.
See `ports/iccdev/portfile.cmake` for the full build configuration.

## Coverage Report

```bash
cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DENABLE_COVERAGE=ON
make -j"$(nproc)"
LLVM_PROFILE_FILE=iccdev_%m.profraw Testing/RunTests.sh
llvm-profdata merge -sparse *.profraw -o merged.profdata
llvm-cov report ./Tools/IccDumpProfile/iccDumpProfile -instr-profile=merged.profdata
```
