# vcpkg Port — Cross-Platform Package Install

A vcpkg overlay port is provided in `ports/iccdev/` for consuming iccDEV
(RefIccMAX) as a dependency in your own projects. The port builds the core
ICC profile libraries and command-line tools as **static libraries** on
Windows, Linux, and macOS.

> **CI Status**: The port is validated on every push by
> [`ci-vcpkg-ports.yml`](../.github/workflows/ci-vcpkg-ports.yml) across
> `windows-latest`, `ubuntu-24.04`, and `macos-14`.

## Installed Artifacts

| Component | Description |
|-----------|-------------|
| **IccProfLib2-static** | Core ICC profile library (C++17, ~47 MB static archive) |
| **IccXML2-static** | XML serialization library (~15 MB, optional via `xml` feature) |
| **52 headers** | Installed to `include/RefIccMAX/IccProfLib2/` and `include/RefIccMAX/IccXML2/` |
| **9 CLI tools** | See [Tools Included](#tools-included) below |
| **CMake config** | `find_package(RefIccMAX CONFIG REQUIRED)` with namespace targets |

### Tools Included

| Tool | Description |
|------|-------------|
| `iccDumpProfile` | Dump and validate ICC profile structure |
| `iccRoundTrip` | Evaluate A2B/B2A round-trip colorimetric accuracy |
| `iccApplyNamedCmm` | Apply named color CMM sequences |
| `iccApplyToLink` | Build DeviceLink profiles or .cube LUTs |
| `iccApplySearch` | Profile search with forward transform |
| `iccFromCube` | Convert .cube 3D LUT to ICC DeviceLink |
| `iccV5DspObsToV4Dsp` | Convert v5 Display+Observer to v4 Display profile |
| `iccFromXml` | Create ICC profiles from XML (requires `xml` feature) |
| `iccToXml` | Convert ICC profiles to XML (requires `xml` feature) |

### What Is Excluded

The port focuses on core libraries and CLI tools. The following are **not**
included:

- **Image tools**: IccTiffDump, IccJpegDump, IccPngDump, IccSpecSepToTiff,
  IccApplyProfiles (require libtiff/libpng/libjpeg — causes static link
  issues across platforms)
- **GUI**: wxProfileDump (requires wxWidgets)
- **Shared/DLL libraries**: See [Known Limitations](#known-limitations)

## Quick Start

### 1. Bootstrap vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh          # Linux / macOS
# or: .\vcpkg\bootstrap-vcpkg.bat   # Windows
```

Or use the vcpkg bundled with Visual Studio (Windows only — see
[Manifest Mode](#manifest-mode-vs-bundled-vcpkg) below).

### 2. Install the Port

#### Classic Mode (Standalone vcpkg)

```bash
./vcpkg/vcpkg install iccdev \
  --overlay-ports=path/to/iccDEV/ports \
  --triplet x64-linux \
  --classic
```

Replace the triplet for your platform:

| Platform | Triplet |
|----------|---------|
| Linux x64 | `x64-linux` |
| Windows x64 | `x64-windows` |
| macOS ARM (Apple Silicon) | `arm64-osx` |
| macOS x64 (Intel) | `x64-osx` |

### 3. Verify the Install

```bash
# Check installed components
vcpkg list | grep iccdev

# Run a tool
./vcpkg/installed/x64-linux/tools/iccdev/iccDumpProfile --help
```

## Features

| Feature | Default | CMake Flag | Dependencies | Description |
|---------|---------|------------|-------------|-------------|
| `tools` | ON | `ENABLE_TOOLS` | — | 7 core CLI tools |
| `xml` | ON | `ENABLE_ICCXML` | libxml2 | IccXML2 library + iccFromXml/iccToXml |

### Install with Specific Features

```bash
# All defaults (tools + xml)
vcpkg install iccdev --overlay-ports=ports --classic

# Core tools only, no XML
vcpkg install "iccdev[core,tools]" --overlay-ports=ports --classic

# Libraries only, no tools, no XML
vcpkg install "iccdev[core]" --overlay-ports=ports --classic
```

## CMake Integration

### Using vcpkg Toolchain (Recommended)

If your project already uses vcpkg, add the overlay to your CMake configure:

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=path/to/iccDEV/ports
```

Then in your `CMakeLists.txt`:

```cmake
find_package(RefIccMAX CONFIG REQUIRED)

# Core profile library
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)

# XML serialization (optional)
target_link_libraries(my_target PRIVATE RefIccMAX::IccXML2-static)
```

### Using vcpkg Install + CMAKE_PREFIX_PATH

```bash
# Install the port
vcpkg install iccdev --overlay-ports=ports --triplet x64-linux --classic

# Configure your project
cmake -B build -DCMAKE_PREFIX_PATH=path/to/vcpkg/installed/x64-linux
```

### Minimal Example

See [`examples/hello-iccdev/`](../examples/hello-iccdev/) for a complete
standalone example that links IccProfLib2 and IccXML2. The example supports
three discovery paths including vcpkg-installed packages (Path 1).

### Git Submodule Integration

If you consume iccDEV as a **git submodule** instead of a vcpkg port, use
`add_subdirectory()` to build IccProfLib directly:

```cmake
# Set required variables before including IccProfLib
set(PROJECT_UP_NAME "REFICCMAX")
set(REFICCMAX_VERSION "2.3.1.6")
set(REFICCMAX_MAJOR_VERSION "2")
set(ENABLE_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(ENABLE_INSTALL_RIM OFF CACHE BOOL "" FORCE)

add_subdirectory(
  "${CMAKE_CURRENT_SOURCE_DIR}/submodules/iccDEV/Build/Cmake/IccProfLib"
  "${CMAKE_CURRENT_BINARY_DIR}/IccProfLib"
)

# IccProfLib2 target is available (ALIAS to IccProfLib2-static)
# Include dirs propagate automatically via PUBLIC properties:
#   - #include "IccProfile.h"            (flat)
#   - #include <IccProfLib/IccProfile.h>  (prefixed)
target_link_libraries(my_target PRIVATE IccProfLib2)
```

> **Important notes for submodule consumers:**
>
> - Use **static builds** on Windows (`ENABLE_SHARED_LIBS OFF`) — shared
>   builds require `__declspec(dllexport)` decorations not yet present.
> - Do **not** call `target_include_directories()` on the `IccProfLib2`
>   target when static-only — it is a CMake ALIAS. Include directories
>   propagate automatically via `target_link_libraries()`.
> - If your project uses vcpkg's toolchain, use `include(FindIccDEV)` or
>   `add_subdirectory()` instead of `find_package(iccDEV)` — the vcpkg
>   toolchain intercepts `find_package()` and won't find custom modules.

## Manifest Mode (VS-Bundled vcpkg)

The vcpkg that ships with Visual Studio **does not support classic mode**. It
requires a `vcpkg.json` manifest in your project directory. This is the
recommended approach for Visual Studio projects.

### 1. Create vcpkg.json in Your Project

```json
{
  "name": "my-project",
  "version-string": "1.0.0",
  "builtin-baseline": "3af1d1e60af2b2abf55760538cd607829029b07a",
  "dependencies": [
    "iccdev"
  ],
  "overrides": [
    { "name": "iccdev", "version": "2.3.1.6" }
  ]
}
```

> **Finding the baseline**: The `builtin-baseline` is the vcpkg registry commit
> hash. Find it in `vcpkg-bundle.json` (`embeddedsha` field) in your VS vcpkg
> directory, typically at:
> `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\`

### 2. Configure with CMake

```powershell
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_OVERLAY_PORTS="path\to\iccDEV\ports" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -G "Visual Studio 17 2022" -A x64
```

vcpkg automatically resolves iccdev and its transitive dependencies
(nlohmann-json, libxml2, zlib, libiconv) during the configure step.

### 3. Build

```powershell
cmake --build build --config Release
```

### Manifest Mode with Local Source

To use a local iccDEV checkout in manifest mode, set the environment variables
**before** running cmake:

```powershell
$env:VCPKG_ICCDEV_SOURCE = "E:\path\to\iccDEV"
$env:VCPKG_KEEP_ENV_VARS = "VCPKG_ICCDEV_SOURCE"

cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="...\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_OVERLAY_PORTS="E:\path\to\iccDEV\ports" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

## Local Source Mode (CI / Development)

By default, the portfile downloads source from GitHub using
`vcpkg_from_github`. In CI or when developing against a local checkout,
set environment variables to use the local source tree instead:

```bash
export VCPKG_ICCDEV_SOURCE=/path/to/iccDEV
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE

vcpkg install iccdev \
  --overlay-ports=/path/to/iccDEV/ports \
  --triplet x64-linux \
  --classic
```

> **Important**: `VCPKG_KEEP_ENV_VARS` is **required**. vcpkg sanitizes
> (strips) all non-standard environment variables before running portfile
> CMake scripts. Without this setting, the portfile cannot read
> `VCPKG_ICCDEV_SOURCE`.

### Windows (PowerShell)

```powershell
$env:VCPKG_ICCDEV_SOURCE = "C:\path\to\iccDEV"
$env:VCPKG_KEEP_ENV_VARS = "VCPKG_ICCDEV_SOURCE"

vcpkg install iccdev `
  --overlay-ports="C:\path\to\iccDEV\ports" `
  --triplet x64-windows `
  --classic
```

## Platform-Specific Notes

### Windows

- Uses MSVC + Ninja generator internally
- Produces `.lib` static archives (`IccProfLib2-static.lib`, `IccXML2-static.lib`)
- Tools installed as `.exe` files
- The VS-bundled vcpkg (`C:\Program Files\Microsoft Visual Studio\...`) does
  **not** support `--classic` mode — use a standalone vcpkg clone

### Linux

- Uses GCC + Ninja generator
- Produces `.a` static archives (`libIccProfLib2-static.a`, `libIccXML2-static.a`)
- nlohmann-json and libxml2 are pulled from the vcpkg registry

### macOS

- Uses Apple Clang + Ninja generator
- ARM64 (Apple Silicon) builds use `arm64-osx` triplet
- Intel builds use `x64-osx` triplet

## CI Workflow

The [`ci-vcpkg-ports.yml`](../.github/workflows/ci-vcpkg-ports.yml) workflow
runs on every push to `ci-vcpkg-ports` and on PRs that modify `ports/**`.

### Matrix

| Runner | Triplet | Compiler |
|--------|---------|----------|
| `windows-latest` | `x64-windows` | MSVC |
| `ubuntu-24.04` | `x64-linux` | GCC |
| `macos-14` | `arm64-osx` | Apple Clang |

### Verification Checks

Each platform verifies:
1. ✅ IccProfLib2 headers exist (`include/RefIccMAX/IccProfLib2/*.h`)
2. ✅ Static libraries built (`IccProfLib2-static`, `IccXML2-static`)
3. ✅ CMake config resolves (`RefIccMAXConfig.cmake`)
4. ✅ Core tools executable (`iccDumpProfile`, `iccRoundTrip`, `iccFromXml`, `iccToXml`)

## Source Patches

The portfile applies 9 patches to the upstream `Build/Cmake/CMakeLists.txt`
via `vcpkg_replace_string` (exact string matching). These patches:

1. Guard `add_subdirectory(IccXML)` behind `ENABLE_ICCXML` option
2. Disable IccDEVCmm (Windows CMM DLL — PCH failures under Ninja)
3. Make `find_package(TIFF)` optional (`REQUIRED` → `QUIET`)
4. Make `find_package(PNG)` optional (`REQUIRED` → `QUIET`)
5. Make `find_package(JPEG)` optional (`REQUIRED` → `QUIET`)
6. Disable IccJpegDump subdirectory
7. Convert TIFF `FATAL_ERROR` to `STATUS` message
8. Convert PNG `FATAL_ERROR` to `STATUS` message (2 locations)
9. Disable IccPngDump subdirectory

> **Warning for upstream maintainers**: Changing the exact text of any
> patched line in CMakeLists.txt will break the vcpkg port build. Run
> `ci-vcpkg-ports.yml` after modifying the CMake build system.

## Known Limitations

### Static-Only Build

The port produces static libraries only. Shared/DLL builds are not supported
because:

- **IccProfLib**: Has partial `ICCPROFLIB_API` macro annotations (308 uses
  across 43 headers) but coverage is incomplete — many classes and functions
  lack export decorations
- **IccXML**: Has zero `__declspec(dllexport)` annotations
- **`CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON`**: Crashes the MSVC linker with
  access violation (0xC0000005) due to the massive symbol table

Proper DLL support requires systematic annotation of all public API surfaces
across both libraries (estimated 3–5 day effort). See the full assessment in
the project's `vcpkg-integration-scope.md`.

### No Image Tools

TIFF, PNG, and JPEG-dependent tools are excluded because static linking of
these image libraries causes transitive dependency issues (missing `deflate`,
`lzma`, `jpeg` symbols) that differ across platforms.

### No GUI

wxProfileDump is excluded — the port targets library consumers and headless
CLI usage.

## Troubleshooting

### "This vcpkg distribution does not have a classic mode instance"

You're using the VS-bundled vcpkg which only supports manifest mode. Either:
- **Option A**: Use manifest mode — create a `vcpkg.json` in your project
  (see [Manifest Mode](#manifest-mode-vs-bundled-vcpkg) above)
- **Option B**: Clone a standalone vcpkg for classic mode:
  ```bash
  git clone https://github.com/microsoft/vcpkg.git
  ./vcpkg/bootstrap-vcpkg.sh
  ```

### "requires a manifest with a specified baseline"

Add `builtin-baseline` to your project's `vcpkg.json`. Find the hash in
`vcpkg-bundle.json` in your VS vcpkg directory (`embeddedsha` field), or
use the latest commit SHA from `https://github.com/microsoft/vcpkg`.

### `vcpkg_replace_string: Could not find the string to replace`

The upstream CMakeLists.txt changed and a portfile patch no longer matches.
Compare the patch string in `portfile.cmake` with the current CMakeLists.txt.

### `VCPKG_ICCDEV_SOURCE` not detected in portfile

You forgot `VCPKG_KEEP_ENV_VARS`. Set both:
```bash
export VCPKG_ICCDEV_SOURCE=/path/to/iccDEV
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE
```

### GitHub API 403 when using `--head`

The `vcpkg_from_github` `--head` mode hits GitHub API rate limits. Use local
source mode instead (see [Local Source Mode](#local-source-mode-ci--development)).

## File Reference

| File | Purpose |
|------|---------|
| [`ports/iccdev/vcpkg.json`](../ports/iccdev/vcpkg.json) | Port manifest (name, version, dependencies, features) |
| [`ports/iccdev/portfile.cmake`](../ports/iccdev/portfile.cmake) | Build script with local source mode + 9 source patches |
| [`.github/workflows/ci-vcpkg-ports.yml`](../.github/workflows/ci-vcpkg-ports.yml) | Cross-platform CI workflow |
| [`examples/hello-iccdev/`](../examples/hello-iccdev/) | Standalone example consuming the libraries |
