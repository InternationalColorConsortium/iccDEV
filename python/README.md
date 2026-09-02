# iccdev - Python Bindings for RefIccMAX

Python bindings for the [RefIccMAX (iccDEV)](https://github.com/InternationalColorConsortium/iccDEV)
ICC color profile library, built with [Cython](https://cython.org/).

## Features

- **Read ICC profiles** - open and inspect ICC v2/v4/v5 (iccMAX) profile headers
- **In-memory validation** - validate bytes or files with native status and report
- **Color transforms** - build multi-profile CMM pipelines and apply pixel transforms
- **Thread-safe apply** - create per-thread apply handles for concurrent use
- **NumPy zero-copy** - `apply_ndarray()` for high-performance bulk transforms
- **CLI-backed conversion helpers** - XML/JSON/blob conversion, profile dump,
  and round-trip diagnostics aligned with the native iccDEV tools
- **Pythonic API** - context managers, enums, exceptions, type hints (PEP 561)
- **Exception hierarchy** - `IccError` -> `IccProfileError`, `IccCmmError`
- **Immutable headers** - `IccProfileHeader` as hashable NamedTuple with computed properties

## Installation

## Current Status

The `iccdev` package is in alpha development.  Until release automation
publishes signed wheels to PyPI, install from this branch or from a local
checkout for validation:

```bash
pip install "git+https://github.com/InternationalColorConsortium/iccDEV.git@pip-install-iccdev#subdirectory=python"
```

### From PyPI (after production release)

```bash
pip install iccdev
```

After the first production package release, this installs a pre-built wheel (no
compiler needed) or builds from source if no wheel matches your platform.
IccProfLib is bundled in source distributions; no external ICC library install
is required.

### From source (development)

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV/python
pip install -e ".[dev]"
```

When installing from the git repo, the build system auto-detects whether
IccProfLib2 is pre-built (via `ICCDEV_BUILD_DIR`) or compiles all 36
source files inline.
On Windows, normal release Python builds only link against release
`IccProfLib2-static.lib`; Debug CRT libraries such as
`IccProfLib2-staticd.lib` are rejected unless using a debug Python build or
setting `ICCDEV_ALLOW_DEBUG_PYTHON_LIB=1`. When `ICCDEV_BUILD_DIR` is set, that
build tree is authoritative; setup does not fall back to Release libraries from
other checkout-local build directories.

The core `IccProfile` and `IccCmm` APIs do not require iccDEV command-line
tools at runtime. The XML, JSON, dump, and round-trip helper functions are
CLI-backed; set `ICCDEV_TOOLS_DIR`, `ICCDEV_BUILD_DIR`, or `PATH` so Python can
find `iccToXml`, `iccFromXml`, `iccToJson`, `iccFromJson`, `iccDumpProfile`,
and `iccRoundTrip`.

Source distributions require Cython at build time.  The project intentionally
does not ship a generated `_iccdev.cpp`; reproducible builds regenerate the C++
extension source from `iccdev/_iccdev.pyx` using the Cython version declared in
`pyproject.toml`.  The `sdist` command vendors `IccProfLib` automatically so a
source distribution can build outside a repository checkout even when the local
checkout also has a pre-built `ICCDEV_BUILD_DIR`.

## Requirements

- Python >= 3.9
- C++17 compiler (MSVC, GCC, or Clang) - only for source builds
- NumPy >= 1.23,<3 (optional, for `apply_ndarray()`)
- Cython >= 3.0,<3.2 - only for source distribution builds

## Quick Start

```python
import iccdev
import numpy as np

# Read a profile
with iccdev.IccProfile("sRGB.icc") as profile:
    hdr = profile.header
    print(f"Version: {hdr.version_string}")
    print(f"Color space: {hdr.color_space_name}")
    print(f"PCS: {hdr.pcs_name}")
    print(f"Device class: {hdr.device_class_name}")
    print(f"Platform: {hdr.platform_name}")

# Convenience functions
p = iccdev.open_profile("sRGB.icc")      # lazy open
p = iccdev.read_profile("sRGB.icc")      # full read
validation = iccdev.validate_profile_file("sRGB.icc")
print(validation.status.name, validation.report)

# Apply a color transform
with iccdev.IccCmm() as cmm:
    cmm.attach("input.icc", intent=iccdev.Intent.Perceptual)
    cmm.attach("output.icc")
    cmm.begin()

    # Single pixel
    result = cmm.apply([0.5, 0.3, 0.1])

    # Multiple pixels (Python lists)
    results = cmm.apply_multi([[0.5, 0.3, 0.1], [1.0, 0.0, 0.0]])

    # NumPy zero-copy (fastest path)
    pixels = np.array([[0.5, 0.3, 0.1]], dtype=np.float32)
    out = cmm.apply_ndarray(pixels)  # returns np.ndarray

    # Thread-safe apply handle
    apply_handle = cmm.get_apply()
    result = apply_handle.apply([0.5, 0.3, 0.1])

# CLI-backed ICC XML/JSON/blob conversion and diagnostics
xml_text = iccdev.icc_to_xml("sRGB.icc")
xml_blob = iccdev.icc_from_xml(xml_text)
json_text = iccdev.icc_to_json(xml_blob)
json_blob = iccdev.icc_from_json(json_text)
print(iccdev.dump_profile(json_blob))
print(iccdev.round_trip("sRGB.icc"))
```

## Exception Handling

Treat ICC profiles as untrusted input and handle library errors explicitly:

```python
import iccdev

try:
    with iccdev.IccProfile("input.icc") as profile:
        print(profile.header.version_string)
except iccdev.IccProfileError as exc:
    print(f"profile load failed: {exc}")

try:
    with iccdev.IccCmm() as cmm:
        cmm.attach("input.icc")
        cmm.attach("output.icc")
        cmm.begin()
        pixel = cmm.apply([0.5, 0.5, 0.5])
except iccdev.IccCmmError as exc:
    print(f"CMM failed: {exc.status_name} ({exc.status})")
```

## API Reference

### Classes

| Class | Description |
|-------|-------------|
| `IccProfile` | Read/open ICC profiles, inspect header |
| `IccCmm` | Color Management Module - build & apply transforms |
| `IccApply` | Thread-safe per-thread apply handle |
| `IccProfileHeader` | Immutable NamedTuple snapshot of profile header |

### Exceptions

| Exception | Description |
|-----------|-------------|
| `IccError` | Base exception for all ICC errors |
| `IccProfileError` | Profile open/read/header errors |
| `IccCmmError` | CMM pipeline errors (attach, begin, apply) |

### Enums

| Enum | Description |
|------|-------------|
| `ColorSpace` | ICC color space signatures (RGB, CMYK, Lab, XYZ, Gray, CLR1-CLR15, ...) |
| `ProfileClass` | Profile classes (Input, Display, Output, Link, Abstract, ...) |
| `RenderingIntent` | Perceptual, RelativeColorimetric, Saturation, AbsoluteColorimetric |
| `Intent` | Short alias for `RenderingIntent` |
| `Interpolation` | Linear, Tetrahedral |
| `LutType` | Color, NamedColor, Preview, Gamut, BPC, ... |
| `CmmStatus` | CMM operation status codes |

### Functions

| Function | Description |
|----------|-------------|
| `sig_to_str(sig)` | Convert 4-byte ICC signature to ASCII string |
| `open_profile(path)` | Open profile lazily (shortcut for `IccProfile(path)`) |
| `read_profile(path)` | Read profile fully into memory |
| `validate_profile(bytes)` | Validate ICC bytes and return status plus report |
| `validate_profile_file(path)` | Validate an ICC file and return status plus report |
| `icc_to_xml(profile)` | Convert ICC path or bytes to XML text using `iccToXml` |
| `icc_from_xml(xml)` | Convert XML text/path/bytes to ICC profile bytes using `iccFromXml` |
| `icc_to_json(profile)` | Convert ICC path or bytes to JSON text using `iccToJson` |
| `icc_from_json(json)` | Convert JSON text/path/object to ICC profile bytes using `iccFromJson` |
| `dump_profile(profile, mode="ALL")` | Run `iccDumpProfile` for an ICC path or bytes |
| `round_trip(profile)` | Run `iccRoundTrip` for an ICC path or bytes |
| `find_tool(tool)` | Resolve a supported iccDEV command-line tool |
| `available_tools()` | Return discovered CLI-backed helper tools |

### IccProfile

```python
profile = iccdev.IccProfile("profile.icc", lazy=True)

# Header (immutable NamedTuple, cached after first access)
hdr = profile.header
hdr.version_string       # e.g., "4.3.0"
hdr.color_space_name     # e.g., "RGB"
hdr.pcs_name             # e.g., "Lab"
hdr.device_class_name    # e.g., "Display"
hdr.rendering_intent_name
hdr.platform_name

# Enum properties
profile.color_space      # ColorSpace enum
profile.pcs              # Profile Connection Space
profile.device_class     # ProfileClass enum
profile.version_string   # shortcut
profile.is_v5            # True if iccMAX (v5.x)
```

Current limitation: `IccProfile` exposes profile lifecycle and header metadata.
Tag-level structured access through the Cython profile object, CMM environment
variables, named color lookup, and gamut diagnostics are planned API parity
extensions. Use the CLI-backed helpers for XML/JSON conversion, profile bytes,
profile dump output, and round-trip diagnostics.

### CLI-backed XML/JSON/blob helpers

```python
import iccdev

profile_bytes = open("sRGB.icc", "rb").read()

xml_text = iccdev.icc_to_xml(profile_bytes)
xml_roundtrip_bytes = iccdev.icc_from_xml(xml_text)

json_text = iccdev.icc_to_json(xml_roundtrip_bytes)
json_roundtrip_bytes = iccdev.icc_from_json(json_text)

with iccdev.IccProfile("sRGB.icc") as original:
    print(original.header.version_string)

with open("roundtrip.icc", "wb") as out:
    out.write(json_roundtrip_bytes)

print(iccdev.dump_profile(json_roundtrip_bytes, "ALL"))
print(iccdev.round_trip("sRGB.icc"))
```

These helpers intentionally preserve native tool behavior instead of
re-implementing ICC XML/JSON serialization in Python. They run subprocesses
without a shell, raise `IccToolError` with command, exit code, stdout, and
stderr on failure, and discover tools in this order:

1. `ICCDEV_TOOLS_DIR`
2. `ICCDEV_BUILD_DIR`
3. `PATH`

## Build Layouts

Set `ICCDEV_BUILD_DIR` to link against a pre-built static IccProfLib.  The build
system searches common CMake layouts under `Build/` and `out/`, including:

- `Build/IccProfLib`
- `Build/IccProfLib/Release`
- `Build/IccProfLib/Debug`
- `out/vs2022-vcpkg/IccProfLib/Release`
- `out/ninja-vcpkg-debug/IccProfLib`
- `out/ninja-vcpkg-release/IccProfLib`

If no pre-built static library is found, the extension compiles the portable
vendored IccProfLib sources inline. The optional AVX2 and AVX-512 translation
units remain CMake-managed because they require architecture-specific compiler
flags and are not valid in universal or non-x86 source builds.

## Wheel Compatibility Policy

CI validates Linux, macOS, and Windows wheels with cibuildwheel for Python
3.9, 3.12, and 3.13.  Linux wheels should follow the active manylinux policy
selected by cibuildwheel.  `musllinux_i686` and `win32` are intentionally
skipped; Windows builds require an MSVC C++17 toolchain.

## Maintainer release workflow

For Python packaging pull requests, merge criteria, and production PyPI release
steps, see [Python packaging PR, merge, and production release](../docs/python-packaging-release.md).
Do not publish artifacts from a PR branch; production artifacts must be rebuilt
from the maintainer-selected release commit or tag.

### IccCmm

```python
cmm = iccdev.IccCmm()

# Attach profiles
cmm.attach("profile.icc",
    intent=iccdev.Intent.Perceptual,
    interp=iccdev.Interpolation.Linear,
    lut_type=iccdev.LutType.Color,
    use_d2b=True,
    use_bpc=False,
)
cmm.attach_profile(...)  # backward-compatible alias

cmm.begin()

# Properties (available after begin())
cmm.src_space       # Source ColorSpace
cmm.dst_space       # Destination ColorSpace
cmm.src_channels    # Number of source channels
cmm.dst_channels    # Number of destination channels
cmm.is_ready        # True if initialized

# Transform methods
cmm.apply(pixel)              # single pixel -> list[float]
cmm.apply_multi(pixels)       # list of pixels -> list[list[float]]
cmm.apply_ndarray(np_array)   # np.float32 array -> np.ndarray (zero-copy)

# Thread-safe handle
apply_handle = cmm.get_apply()
```

## Build Configuration

The `setup.py` searches for the IccProfLib2 static library in common build
output locations. Override the search path:

```bash
export ICCDEV_BUILD_DIR=/path/to/build/output
```

### Windows (MSVC)

```cmd
set ICCDEV_BUILD_DIR=C:\path\to\Build
pip install -e ".[dev]"
```

### macOS

```bash
# Requires Xcode CLT or full Xcode
export ICCDEV_BUILD_DIR=/path/to/Build
pip install -e ".[dev]"
```

### Linux (WSL2)

```bash
export ICCDEV_BUILD_DIR=/tmp/iccdev-build
pip install -e ".[dev]" --break-system-packages
```

## Testing

```bash
python -m pytest --rootdir . --import-mode=importlib python/tests -v -m "not parity"
```

Native-backed parity tests mirror the CTest/CI tool workflow for ProfileLib,
XML, JSON, display-profile round-trip, PAWG report, profile dump, and named-CMM
configuration smoke operations. Run them after building iccDEV tools:

```bash
export ICCDEV_BUILD_DIR=/path/to/Build
python -m pytest --rootdir . --import-mode=importlib python/tests -v -m parity
```

The parity marker discovers built tools from `ICCDEV_BUILD_DIR`,
`ICCDEV_TOOLS_DIR`, or `PATH`, and skips when native tools or generated profiles
are not available. Wheel smoke tests intentionally run the `not parity` subset.
Run tests from the repository root with `--import-mode=importlib` so installed
package validation does not accidentally import directly from `python/iccdev`.

## Type Checking

The package ships PEP 561 type stubs (`py.typed` + `_iccdev.pyi`).
Works with mypy, pyright, and IDE autocompletion.

## License

BSD 3-Clause - same as the parent RefIccMAX project. See [LICENSE.md](../LICENSE.md).
