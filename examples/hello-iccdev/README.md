# hello-iccdev

Minimal standalone example that links **IccProfLib2** and **IccXML2** from the
[iccDEV](https://github.com/InternationalColorConsortium/iccDEV) project
(RefIccMAX — the ICC reference implementation).

## What It Does

1. Prints the IccProfLib and IccLibXML library version strings
2. Creates a minimal ICC display profile (sRGB, Lab PCS)
3. Round-trips the profile header to XML via `CIccProfileXml::ToXml()`
4. Prints a greeting: **Hello, iccDEV!**

## Prerequisites

| Requirement | Minimum Version |
|-------------|-----------------|
| CMake       | 3.16            |
| C++ compiler | C++17 support  |
| libxml2     | any recent      |

On Debian/Ubuntu:

```bash
sudo apt-get install cmake g++ libxml2-dev
```

On macOS (Homebrew):

```bash
brew install cmake libxml2
```

On Windows, libxml2 is fetched automatically via vcpkg when building iccDEV.

## Build iccDEV First

Before building the example, build the iccDEV libraries:

```bash
cd <iccDEV-root>/Build/Cmake
cmake -B build
cmake --build build
```

## Build the Example

The CMakeLists.txt tries three discovery paths in order:

### Path 1 — Installed Package

If iccDEV has been installed (via `cmake --install`), the example finds it
automatically through `find_package(RefIccMAX CONFIG)`:

```bash
cd examples/hello-iccdev
cmake -B build -DCMAKE_PREFIX_PATH="/usr/local"
cmake --build build
```

### Path 2 — Build-Tree Export (Recommended for Development)

Point at the iccDEV build directory. The CMake config is loaded directly from
the build tree — no install step required:

```bash
cd examples/hello-iccdev
cmake -B build -DICCDEV_BUILD_DIR=../../Build/Cmake/build
cmake --build build
```

### Path 3 — Manual Discovery (Legacy / Custom Layouts)

For non-standard directory layouts, specify both the source root and build
directory:

```bash
cd examples/hello-iccdev
cmake -B build \
  -DICCDEV_ROOT=/path/to/iccDEV \
  -DICCDEV_BUILD_DIR=/path/to/iccDEV/Build/Cmake/build
cmake --build build
```

## Run

```bash
./build/hello-iccdev
```

Expected output:

```
IccProfLib version: 2.3.1
IccLibXML  version: 2.3.1
Profile spec ver:  V5.1.0

XML round-trip OK (282 bytes)
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  ... (truncated)

Hello, iccDEV!
```

## Platform Notes

### Linux / macOS

Shared libraries (`libIccProfLib2.so` / `.dylib`) must be on the library search
path. When using Path 2, set `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH`
(macOS) to include the build directory:

```bash
export LD_LIBRARY_PATH=../../Build/Cmake/build/IccProfLib:../../Build/Cmake/build/IccXML:$LD_LIBRARY_PATH
./build/hello-iccdev
```

### Windows (MSVC)

The CMakeLists.txt automatically copies DLLs from the build directory next to
the executable via a post-build step — no manual `PATH` modification needed.

## CI Integration

This example is tested automatically in the
[ci-comprehensive-build-test](../../.github/workflows/ci-comprehensive-build-test.yml)
workflow. The CI job:

1. Builds iccDEV with CMake
2. Configures hello-iccdev with `-DICCDEV_BUILD_DIR` (Path 2)
3. Builds and runs the example
4. Verifies the output contains `IccProfLib version:`, `IccLibXML version:`,
   and `Hello, iccDEV!`

## Files

| File | Description |
|------|-------------|
| `CMakeLists.txt` | Build configuration with 3-path library discovery |
| `hello-iccdev.cpp` | Example source — versions, profile creation, XML round-trip |
| `README.md` | This file |

## License

Copyright © 2026 The International Color Consortium. All rights reserved.
