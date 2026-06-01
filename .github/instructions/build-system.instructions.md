---
applyTo: "Build/**"
---

# Build System Instructions

## CMake Structure

Primary build file: `Build/Cmake/CMakeLists.txt`

- Project: RefIccMAX v2.3.2.1
- Minimum CMake: 3.21
- C++ standard: C++17
- Compiler floor: GCC 11+, Clang 10+, MSVC 19.30+
- Recommended compilers for the strict-warning tier: GCC 15+, Clang 14+,
  MSVC 19.40+
- Dependencies: libpng, libjpeg-turbo, libtiff, libxml2, wxwidgets,
  nlohmann-json

## Platform Notes

| Platform | Configure entry point |
|----------|-----------------------|
| Linux | `cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++` |
| macOS Xcode | `cd Build && cmake -G "Xcode" Cmake` |
| Windows MSVC/vcpkg | `cmake --preset vs2022-x64 -B Build -S Build/Cmake` |
| Emscripten/WASM | `cd Build && emcmake cmake Cmake -DENABLE_TESTS=OFF -DENABLE_SHARED_LIBS=OFF` |

User-facing build details live in `docs/build.md`.

## Sanitizer Options

| Option | Default | Purpose |
|--------|---------|---------|
| `ENABLE_SANITIZERS` | OFF | ASan + UBSan + IntegerSan + float sanitizer combined |
| `ENABLE_ASAN` | OFF | AddressSanitizer only |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer only |
| `ENABLE_INTEGER_SANITIZER` | OFF | IntegerSanitizer for unsigned overflow |
| `ENABLE_FLOAT_SANITIZER` | OFF | `float-divide-by-zero,float-cast-overflow` |
| `ENABLE_TSAN` | OFF | ThreadSanitizer |
| `ENABLE_MSAN` | OFF | MemorySanitizer (Clang only) |
| `ENABLE_LSAN` | OFF | LeakSanitizer standalone |
| `ENABLE_COVERAGE` | OFF | Clang source coverage or GCC gcov |
| `ENABLE_PROFILING` | OFF | gprof/perf `-pg` profiling |

`-fsanitize=undefined` does not catch unsigned overflow or float division by
zero. For numeric bug hunting, use IntegerSanitizer plus
`float-divide-by-zero,float-cast-overflow`.

Before changing compiler, sanitizer, coverage, or profiling flags in an existing
tree, delete `Build/CMakeCache.txt` and `Build/CMakeFiles/`. Use `CC=clang`
and `CXX=clang++` for environment compiler selection; `C=clang` is ignored by
CMake and can leave a stale `/usr/bin/cc` cache entry after a failed configure.

## Canonical Linux Instrumentation Configure Lines

Run these from the repository root. They intentionally use CMake options instead
of hand-written sanitizer flags so compile and link flags stay synchronized.

```bash
# ASan only
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_ASAN=ON

# UBSan + IntSan + float checks, no ASan
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_UBSAN=ON -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON

# Security repro default: ASan + UBSan + IntSan + float checks, no coverage
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON

# Equivalent combined security build
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_SANITIZERS=ON

# ThreadSanitizer only; do not combine with ASan, LSan, fuzzing, or ENABLE_SANITIZERS
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TSAN=ON

# MemorySanitizer only; Clang-only and incompatible with other sanitizers here
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_MSAN=ON

# Source coverage; keep separate from sanitizer reproduction attempts
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_COVERAGE=ON

# gprof/perf profiling; keep separate from sanitizer reproduction attempts
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_PROFILING=ON
```

Build after configure with:

```bash
make -j"$(nproc)"
```

Preset equivalents live in `Build/Cmake/CMakePresets.json`:

| Preset | Purpose |
|--------|---------|
| `linux-clang-asan` | ASan-only Debug tool build |
| `linux-clang-ubsan` | UBSan-only Debug tool build |
| `linux-clang-ubsan-int-float` | UBSan + IntSan + float checks, no ASan |
| `linux-clang-sanitizers` | ASan + UBSan + IntSan + float checks |
| `linux-clang-tsan` | TSan-only Debug tool build |
| `linux-clang-msan` | MSan-only Debug tool build |
| `linux-clang-coverage` | Clang source coverage |
| `linux-clang-profiling` | gprof/perf `-pg` profiling |

Example:

```bash
cmake --preset linux-clang-sanitizers -S Build/Cmake -B out/linux-clang-sanitizers
cmake --build out/linux-clang-sanitizers -j"$(nproc)"
```

For sanitizer bug reproduction, do not add `-DENABLE_COVERAGE=ON` or
`-fprofile-instr-generate -fcoverage-mapping`; coverage instrumentation can
change optimizer and sanitizer behavior enough to mask a finding.

TSan conflicts with ASan, LSan, fuzzing, and `ENABLE_SANITIZERS`. MSan conflicts
with ASan, TSan, LSan, fuzzing, and `ENABLE_SANITIZERS`. CMake should reject
incompatible combinations.

## LTO Behavior

`CMAKE_INTERPROCEDURAL_OPTIMIZATION` is ON for Release builds when sanitizers and
coverage are both OFF. Consumers using non-LTO-aware linkers should pass
`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` or use `RelWithDebInfo`.

## Adding a New Tool

1. Create `Tools/CmdLine/NewTool/`.
2. Add a `CMakeLists.txt` with `add_executable()` and the required
   `target_link_libraries()` entries.
3. Add `add_subdirectory(NewTool)` to the parent `Tools/CmdLine/CMakeLists.txt`.
4. Link `IccXML2` only when XML features are required.
5. Guard GUI-only or unsupported tools with `if(EMSCRIPTEN)`.
6. If the tool should ship in vcpkg, update the `_core_tools` list in
   `ports/iccdev/portfile.cmake` and the `ci-vcpkg-ports.yml` verification step.

## CTest Integration

CTest registrations live in `Build/Cmake/Testing/CMakeLists.txt`. The `check`
target must exist on every platform and run CTest with `--no-tests=error`.
CTest, CPack, sanitizer, release packaging, and workflow integration are
maintainer-owned infrastructure. General contributors should not change these
areas unless an iccDEV maintainer explicitly requests it.

When adding a tool that should be covered by standard testing:

1. Add its build output directory to the CTest tool path list.
2. Add the target to the `check` dependency list when the test needs the tool.
3. Add or update a CTest suite for the tool.
4. Update `docs/ctest.md` and any workflow test-count assertions.

## vcpkg Port Integration

The `ports/iccdev/` overlay port builds static libraries and core CLI tools.
When changing CMake files, verify the exact-match patches in the portfile still
apply. See `.github/instructions/vcpkg-port.instructions.md` for the full port
maintenance workflow.
