---
applyTo: "Build/**"
---

# Build System Instructions

## CMake Structure

Primary build file: `Build/Cmake/CMakeLists.txt`

- Project: RefIccMAX v2.3.2.0
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
| `ENABLE_SANITIZERS` | OFF | ASan + UBSan + IntegerSan combined |
| `ENABLE_ASAN` | OFF | AddressSanitizer only |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer only |
| `ENABLE_INTEGER_SANITIZER` | OFF | IntegerSanitizer for unsigned overflow |
| `ENABLE_TSAN` | OFF | ThreadSanitizer |
| `ENABLE_MSAN` | OFF | MemorySanitizer (Clang only) |
| `ENABLE_LSAN` | OFF | LeakSanitizer standalone |

`-fsanitize=undefined` does not catch unsigned overflow or float division by
zero. For numeric bug hunting, use IntegerSanitizer plus
`float-divide-by-zero,float-cast-overflow`.

Before changing sanitizer flags in an existing tree, delete
`Build/CMakeCache.txt` and `Build/CMakeFiles/`.

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
