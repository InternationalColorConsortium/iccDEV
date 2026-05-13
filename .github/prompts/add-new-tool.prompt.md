# Add a New CLI Tool

Use this prompt when adding a new command-line tool to iccDEV.

## Step 1: Create the Tool Directory

```bash
mkdir -p Tools/CmdLine/IccNewTool
```

## Step 2: Write the Source File

Create `Tools/CmdLine/IccNewTool/iccNewTool.cpp`:

```cpp
/** @file
 *  @brief iccDEV - IccNewTool
 */

/*
 * Copyright (c) 2024-2026 The International Color Consortium.
 *                           All rights reserved.
 *
 * [BSD 3-Clause license text]
 */

#include <cstdio>
#include <cstring>
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("Usage: iccNewTool <profile_path>\n");
    printf("Built with IccProfLib version " ICCPROFLIBVER "\n");
    return 1;
  }

  CIccProfile profile;
  if (!profile.ReadFile(argv[1])) {
    printf("Error: Unable to read '%s'\n", argv[1]);
    return 1;
  }

  // Tool logic here...
  printf("Profile: '%s'\n", argv[1]);
  printf("Version: %s\n", profile.GetVersionString().c_str());

  return 0;
}
```

## Step 3: Create CMakeLists.txt

Create `Tools/CmdLine/IccNewTool/CMakeLists.txt`:

```cmake
add_executable(iccNewTool iccNewTool.cpp)

target_link_libraries(iccNewTool
  PRIVATE
    IccProfLib2-static
    $<$<BOOL:${ENABLE_ICCXML}>:IccXML2-static>
)

target_include_directories(iccNewTool
  PRIVATE
    ${PROJECT_SOURCE_DIR}/IccProfLib
    $<$<BOOL:${ENABLE_ICCXML}>:${PROJECT_SOURCE_DIR}/IccXML/IccLibXML>
)

install(TARGETS iccNewTool DESTINATION bin)

# Emscripten WASM support
if(EMSCRIPTEN)
  set_target_properties(iccNewTool PROPERTIES
    SUFFIX ".js"
    LINK_FLAGS "-sMODULARIZE=1 -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain,FS -sALLOW_MEMORY_GROWTH=1"
  )
endif()
```

## Step 4: Register in Parent CMakeLists.txt

Edit `Tools/CmdLine/CMakeLists.txt`:
```cmake
add_subdirectory(IccNewTool)
```

## Step 5: Build and Test

```bash
cd Build
cmake Cmake -DCMAKE_CXX_COMPILER=clang++
make -j"$(nproc)"

# Test
LD_LIBRARY_PATH=IccProfLib:IccXML \
  Tools/IccNewTool/iccNewTool ../Testing/Display/sRGB_D65_MAT.icc

# Test with sanitizers
cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DENABLE_SANITIZERS=ON
make -j"$(nproc)"
ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 \
LD_LIBRARY_PATH=IccProfLib:IccXML \
  Tools/IccNewTool/iccNewTool ../Testing/Display/sRGB_D65_MAT.icc
```

## Step 6: Add to Testing

If the tool accepts ICC profiles, add validation to `Testing/RunTests.sh`:
```bash
echo "==========================================================================="
echo "Test NewTool with sRGB"
iccNewTool Display/sRGB_D65_MAT.icc
```

Add the same to `Testing/RunTests.bat` for Windows.

## Step 7: Update Documentation

- Add `Tools/CmdLine/IccNewTool/Readme.md` with purpose, usage, and output notes
- Add tool description and example to `docs/tools-cli-reference.md`
- Confirm `Doxyfile.full` includes Markdown so `Tools/CmdLine/*/Readme.md` files are generated
- Add tool description to `docs/index.md` under "Tools based upon these libraries"
- Add to `docs/install.md` Docker examples if applicable
- Update `.github/copilot-instructions.md` project structure table
- Update `Build/Cmake/wasm-package/` staging, exports, tests, package metadata, and release workflow smoke text if WASM-compatible

## Checklist

- [ ] Source file with ICC copyright header
- [ ] CMakeLists.txt with IccProfLib2-static link
- [ ] Registered in parent CMakeLists.txt
- [ ] Emscripten `if(EMSCRIPTEN)` guard for WASM
- [ ] Builds clean with `clang++ -Wall -Wextra`
- [ ] 0 ASan/UBSan findings on test profiles
- [ ] Added to Testing/RunTests.sh and .bat
- [ ] Tool `Readme.md`, `docs/tools-cli-reference.md`, and `docs/index.md` updated
- [ ] If core tool: added to `_core_tools` in `ports/iccdev/portfile.cmake`
- [ ] If core tool: added to verify step in `.github/workflows/ci-vcpkg-ports.yml`
- [ ] PR description includes: purpose, build/test commands, sample output
