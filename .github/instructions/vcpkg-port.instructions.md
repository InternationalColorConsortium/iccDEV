---
applyTo: "ports/**"
---

# vcpkg Port — Auto-Loaded Instructions

## Port Structure

```
ports/iccdev/
├── portfile.cmake   — Build logic and feature flags
├── usage            — CMake consumption hint printed by vcpkg
└── vcpkg.json       — Port manifest (version, port-version, deps, features)
```

## Architecture Decisions

### Static-Only Build
The port builds static libraries only (`ENABLE_SHARED_LIBS=OFF`).
IccProfLib has incomplete `__declspec(dllexport)` annotations (308 partial
uses across 43 headers), and IccXML has zero export annotations.
`CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON` crashes the MSVC linker with access
violation (0xC0000005) due to the massive symbol table (~15,000 symbols).

### Core-Only Tools
Image-dependent tools (IccTiffDump, IccJpegDump, IccPngDump,
IccSpecSepToTiff, IccApplyProfiles) are excluded. Static linking of
libtiff/libpng/libjpeg causes transitive dependency failures across
platforms (e.g., deflate, lzma, jpeg symbols unresolved on Linux).

### wxWidgets Disabled
wxProfileDump GUI is excluded (`ENABLE_WXWIDGETS=OFF`) — the port
targets library consumers and headless CLI usage.

## Upstream CMake Options

The port no longer applies source-time string patches. Upstream CMake exposes
the package switches used by vcpkg:

- `ENABLE_ICCXML`
- `ENABLE_ICCJSON`
- `ENABLE_TOOLS`
- `ENABLE_IMAGE_TOOLS`
- `ENABLE_CMM_TOOLS`
- `ENABLE_IIS_TOOLS`
- `ENABLE_WXWIDGETS`

## Local Source Mode

For CI and development, the portfile supports a local source mode:

```bash
export VCPKG_ICCDEV_SOURCE=/path/to/iccDEV
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE
```

**Critical**: vcpkg sanitizes environment variables before running portfile
CMake. The `VCPKG_KEEP_ENV_VARS` setting is mandatory to whitelist custom
env vars through the sandbox.

Without local source mode, the portfile falls back to `vcpkg_from_github` with
an immutable `REF` commit and matching `SHA512`. Refresh both together.

## Features

| Feature | Default | CMake Option | Dependencies |
|---------|---------|-------------|-------------|
| `tools` | OFF | `ENABLE_TOOLS` | nlohmann-json |
| `xml` | OFF | `ENABLE_ICCXML` | libxml2 |
| `json` | OFF | `ENABLE_ICCJSON` | nlohmann-json; also builds IccConnect |

The core port always enables `ICC_USE_ZLIB=ON` and declares `zlib` as a direct
dependency. Keep zlib synchronized in the root `vcpkg.json`,
`Build/Cmake/vcpkg.json`, and `ports/iccdev/vcpkg.json`. The
`examples/hello-iccdev/vcpkg.json` manifest consumes the `iccdev` port and must
not duplicate its transitive dependencies.

## Installed Artifacts

- **Headers**: `include/RefIccMAX/IccProfLib2/*.h` (52 files)
- **Libraries**: `lib/IccProfLib2-static.lib` plus feature libraries, including `IccConnect2-static` when JSON/tools are enabled
- **CMake config**: `share/RefIccMAX/RefIccMAXConfig.cmake`
- **Tools**: installed only with the `tools` feature; XML/JSON conversion
  tools are copied only when their matching feature is enabled

## CI Workflow

`ci-vcpkg-ports.yml` validates the port on 3 platforms:
- windows-latest (x64-windows, MSVC)
- ubuntu-24.04 (x64-linux, GCC)
- macos-14 (arm64-osx, Apple Clang)

Verification checks: headers present, static libs exist, CMake config
resolves, and core/XML/JSON/connect tools execute: iccDumpProfile,
iccRoundTrip, iccFromXml, iccToXml, iccFromJson, iccToJson, and
iccApplyNamedCmm.

## Version Bumps

When updating the port:
1. Update `"version"` in `ports/iccdev/vcpkg.json`
2. For source-only refreshes without an upstream version bump, increment
   `"port-version"` instead of changing `"version"`.
3. For dependency changes, synchronize the root, `Build/Cmake`, and overlay
   port manifests, then increment `"port-version"`.
4. Update `REF` and `SHA512` in `portfile.cmake` for non-local consumers when
   the pinned source changes.
5. Test locally before pushing: set `VCPKG_ICCDEV_SOURCE` and run
   `vcpkg install iccdev --overlay-ports=ports --classic`
