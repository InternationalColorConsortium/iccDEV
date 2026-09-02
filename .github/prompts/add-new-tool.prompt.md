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
 * Copy the complete ICC Software License block from an adjacent existing
 * C++ source file. Do not use an abbreviated or placeholder license banner.
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

## Step 3: Create Build/Cmake Tool Registration

Create `Build/Cmake/Tools/IccNewTool/CMakeLists.txt`:

```cmake
SET( SRC_PATH ../../../.. )
SET( SOURCES ${SRC_PATH}/Tools/CmdLine/IccNewTool/iccNewTool.cpp )
SET( TARGET_NAME iccNewTool )

ADD_EXECUTABLE( ${TARGET_NAME} ${SOURCES} )
TARGET_LINK_LIBRARIES( ${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB} )

IF(ENABLE_INSTALL_RIM)
  INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
ENDIF()
```

## Step 4: Register in Top-Level CMakeLists.txt

Edit `Build/Cmake/CMakeLists.txt` near the other command-line tool
registrations:

```cmake
ADD_SUBDIRECTORY(Tools/IccNewTool)
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
rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_SANITIZERS=ON
make -j"$(nproc)"
ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 \
LD_LIBRARY_PATH=IccProfLib:IccXML \
  Tools/IccNewTool/iccNewTool ../Testing/Display/sRGB_D65_MAT.icc
```

## Step 6: Add to Testing

Prefer a focused CTest registration in
`Build/Cmake/Testing/CMakeLists.txt`, add the target to the relevant build-test
dependency list, and document the selector in `docs/ctest.md`. CTest and
workflow changes are maintainer-owned, so coordinate those edits with an
iccDEV maintainer.

If the tool also belongs in the legacy profile sweep, add equivalent validation
to both `Testing/RunTests.sh` and `Testing/RunTests.bat`:
```bash
echo "==========================================================================="
echo "Test NewTool with sRGB"
iccNewTool Display/sRGB_D65_MAT.icc
```

Add the same to `Testing/RunTests.bat` for Windows.

## Step 7: Update Documentation

- Add `Tools/CmdLine/IccNewTool/Readme.md` with purpose, usage, and output notes
- Add tool description and example to `docs/tools-cli-reference.md`
- Confirm `.github/ci/doxygen/Doxyfile` includes Markdown so `Tools/CmdLine/*/Readme.md` files are generated
- Keep tool and reference links Doxygen-safe: if a link points outside the Doxygen INPUT tree, use an explicit HTML anchor (`<a href="...">...</a>`) instead of a Markdown link that will trigger warning-only docs failures
- Run `doxygen .github/ci/doxygen/Doxyfile` and require an empty `docs/generated/doxygen-warnings.log`; do not add generated Doxygen output or warning logs
- Add tool description to `docs/index.md` under "Tools based upon these libraries"
- Add to `docs/install.md` Docker examples if applicable
- Update `.github/prompts/contributor-onboarding.prompt.md` when the repository map changes
- Update `Build/Cmake/wasm-package/` staging, exports, tests, package metadata, and release workflow smoke text if WASM-compatible
- Update `iccdev-mcp/` only when the tool has a safe, bounded MCP use case; CLI-only tools must be documented as such
- Add or update an agent prompt/skill only for a repeatable maintainer workflow, not merely because a binary was added
- Do not add the tool to `IccIisIsapi` by default. That sample intentionally
  exposes a bounded ICC/XML pipeline; a new IIS wrapper needs an explicit input,
  timeout, artifact, and deployment design.

## Checklist

- [ ] Source file with ICC copyright header
- [ ] `Build/Cmake/Tools/IccNewTool/CMakeLists.txt` with the correct library link
- [ ] Registered in `Build/Cmake/CMakeLists.txt`
- [ ] Emscripten `if(EMSCRIPTEN)` guard for WASM
- [ ] Builds clean with `clang++ -Wall -Wextra`
- [ ] 0 ASan/UBSan findings on test profiles
- [ ] Added to focused CTest coverage and, if applicable, both legacy RunTests scripts
- [ ] Tool `Readme.md`, `docs/tools-cli-reference.md`, and `docs/index.md` updated
- [ ] If core tool: added to `_core_tools` in `ports/iccdev/portfile.cmake`
- [ ] If core tool: added to verify step in `.github/workflows/ci-vcpkg-ports.yml`
- [ ] Packaging/install and runtime dependency behavior verified
- [ ] MCP, WASM, IIS, prompts, and skills explicitly classified as integrated or not applicable
- [ ] PR description includes: purpose, build/test commands, sample output
