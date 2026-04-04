---
applyTo: "Build/**"
---

# Build System — Auto-Loaded Instructions

## CMake Structure

Primary build file: `Build/Cmake/CMakeLists.txt`
- **Project**: RefIccMAX v2.3.1.6
- **Minimum CMake**: 3.21
- **C++ Standard**: C++17 (required)
- **Dependencies**: libpng, libjpeg-turbo, libtiff, libxml2, wxwidgets, nlohmann-json

## Platform-Specific Notes

### Linux (Clang preferred)
```bash
cmake Cmake -DCMAKE_CXX_COMPILER=clang++
```

### macOS (Xcode generator)
```bash
cmake -G "Xcode" Cmake
```

### Windows (MSVC + vcpkg)
```cmd
cmake --preset vs2022-x64 -B . -S Cmake
```
vcpkg manifest at repo root (`vcpkg.json`) handles dependency installation.

### Emscripten (WASM)
```bash
emcmake cmake Cmake -DENABLE_TESTS=OFF -DENABLE_SHARED_LIBS=OFF
```
CMakeLists.txt has `if(EMSCRIPTEN)` guards that:
- Skip ELF-only linker flags (`-Wl,-z,relro`, `-fstack-protector-strong`)
- Add Emscripten-specific flags (`-sMODULARIZE=1`, `-sINVOKE_RUN=0`)
- Disable GUI tools (IccDumpProfileGui)

## Sanitizer Options

| Option | Default | Purpose |
|--------|---------|---------|
| `ENABLE_SANITIZERS` | OFF | ASan + UBSan + IntegerSan combined |
| `ENABLE_ASAN` | OFF | AddressSanitizer only |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer only |
| `ENABLE_INTEGER_SANITIZER` | OFF | IntegerSanitizer (unsigned overflow) |
| `ENABLE_TSAN` | OFF | ThreadSanitizer |
| `ENABLE_MSAN` | OFF | MemorySanitizer (Clang only) |
| `ENABLE_LSAN` | OFF | LeakSanitizer standalone |

**CRITICAL**: `-fsanitize=undefined` does NOT catch unsigned integer overflow.
Use `ENABLE_INTEGER_SANITIZER=ON` or `ENABLE_SANITIZERS=ON` (which now includes
integer). Issue #769 was only detectable with `-fsanitize=integer`.

### Full-coverage build (recommended for bug hunting)
```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j$(nproc)
```

### Sanitizer Mutual Exclusivity

TSan conflicts with: ASan, LSan, Fuzzing, ENABLE_SANITIZERS
MSan conflicts with: ASan, TSan, LSan, Fuzzing, ENABLE_SANITIZERS
CMake will `FATAL_ERROR` if incompatible sanitizers are combined.
IntegerSanitizer is Clang-only; MSVC builds skip it automatically.

## LTO Behavior

`CMAKE_INTERPROCEDURAL_OPTIMIZATION` is set to ON for Release builds when
sanitizers and coverage are both OFF. Static archives (.a) will contain LLVM
bitcode — consumers linking with non-LTO-aware linkers must pass
`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` or use `RelWithDebInfo`.

## Adding a New Tool

1. Create directory under `Tools/CmdLine/NewTool/`
2. Add `CMakeLists.txt` with `add_executable()` and `target_link_libraries(IccProfLib2)`
3. Add to parent `Tools/CmdLine/CMakeLists.txt` via `add_subdirectory(NewTool)`
4. Tool should link `IccXML2` if XML features are needed
5. Ensure `if(EMSCRIPTEN)` guard skips GUI-only tools
6. If the tool should be in the vcpkg port, add it to the `_core_tools` list
   in `ports/iccdev/portfile.cmake` and update the verify step in
   `ci-vcpkg-ports.yml`

## vcpkg Port Integration

The `ports/iccdev/` overlay port builds a subset of the project (static
libraries + core CLI tools). When modifying `Build/Cmake/CMakeLists.txt`:

- The portfile applies 9 `vcpkg_replace_string` patches that do exact
  string matching. Changing patched lines will break the port build.
- Patched areas: `find_package(TIFF/PNG/JPEG)`, `add_subdirectory(IccXML)`,
  IccDEVCmm, IccJpegDump, IccPngDump, and FATAL_ERROR messages.
- After modifying CMakeLists.txt, run `ci-vcpkg-ports.yml` to verify.
- See `.github/instructions/vcpkg-port.instructions.md` for full details.
