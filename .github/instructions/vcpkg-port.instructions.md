---
applyTo: "ports/**"
---

# vcpkg Port — Auto-Loaded Instructions

## Port Structure

```
ports/iccdev/
├── portfile.cmake   — Build logic, source patches, feature flags
└── vcpkg.json       — Port manifest (name, version, deps, features)
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

## Source Patches (9 total)

The portfile applies 9 `vcpkg_replace_string` patches to the upstream
`Build/Cmake/CMakeLists.txt`:

1. **IccXML guard** — wrap `add_subdirectory(IccXML)` in `if(ENABLE_ICCXML)`
2. **IccDEVCmm disable** — Windows CMM DLL has PCH issues under Ninja
3. **TIFF REQUIRED → QUIET** — make libtiff optional
4. **PNG REQUIRED → QUIET** — make libpng optional
5. **JPEG REQUIRED → QUIET** — make libjpeg optional
6. **IccJpegDump disable** — image tool mixed in with core tools
7. **TIFF FATAL_ERROR → STATUS** — skip instead of abort
8. **PNG FATAL_ERROR → STATUS** (2 locations) — skip instead of abort
9. **IccPngDump disable** — image tool unconditionally added

When updating upstream CMakeLists.txt, check whether these patches still
apply. The `vcpkg_replace_string` calls do exact string matching and will
fail if the upstream text changes.

## Local Source Mode

For CI and development, the portfile supports a local source mode:

```bash
export VCPKG_ICCDEV_SOURCE=/path/to/iccDEV
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE
```

**Critical**: vcpkg sanitizes environment variables before running portfile
CMake. The `VCPKG_KEEP_ENV_VARS` setting is mandatory to whitelist custom
env vars through the sandbox.

Without local source mode, the portfile falls back to `vcpkg_from_github`
with `REF "v${VERSION}"` — requires a tagged release.

## Features

| Feature | Default | CMake Option | Dependencies |
|---------|---------|-------------|-------------|
| `tools` | ON | `ENABLE_TOOLS` | — |
| `xml` | ON | `ENABLE_ICCXML` | libxml2 |

## Installed Artifacts

- **Headers**: `include/RefIccMAX/IccProfLib2/*.h` (52 files)
- **Libraries**: `lib/IccProfLib2-static.lib`, `lib/IccXML2-static.lib`
- **CMake config**: `share/RefIccMAX/RefIccMAXConfig.cmake`
- **Tools** (9): iccDumpProfile, iccApplyNamedCmm, iccRoundTrip, iccFromCube,
  iccApplyToLink, iccApplySearch, iccV5DspObsToV4Dsp, iccFromXml, iccToXml

## CI Workflow

`ci-vcpkg-ports.yml` validates the port on 3 platforms:
- windows-latest (x64-windows, MSVC)
- ubuntu-24.04 (x64-linux, GCC)
- macos-14 (arm64-osx, Apple Clang)

Verification checks: headers present, static libs exist, CMake config
resolves, 4 tools executable (iccDumpProfile, iccRoundTrip, iccFromXml,
iccToXml).

## Version Bumps

When bumping the port version:
1. Update `"version"` in `ports/iccdev/vcpkg.json`
2. If using tagged releases (non-local mode), update `REF` and `SHA512`
   in `portfile.cmake`
3. Verify patches still apply against the new upstream CMakeLists.txt
4. Test locally before pushing: set `VCPKG_ICCDEV_SOURCE` and run
   `vcpkg install iccdev --overlay-ports=ports --classic`
