---
applyTo: "Build/**"
---

# Build System — Auto-Loaded Instructions

## CMake Structure

Primary build file: `Build/Cmake/CMakeLists.txt`
- **Project**: RefIccMAX v2.3.1.5
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

## Sanitizer Mutual Exclusivity

TSan conflicts with: ASan, LSan, Fuzzing, ENABLE_SANITIZERS
MSan conflicts with: ASan, TSan, LSan, Fuzzing, ENABLE_SANITIZERS
CMake will `FATAL_ERROR` if incompatible sanitizers are combined.

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
