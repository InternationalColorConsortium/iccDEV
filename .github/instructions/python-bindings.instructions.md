---
applyTo: "python/**"
---

# Python/Cython Bindings -- Path-Specific Instructions

## What This Is

Python package providing Pythonic access to IccProfLib via Cython.
The binding uses a C wrapper layer (`IccProfLib/IccWrapper.h/.cpp`) to
bridge from C++ to C, which Cython then wraps into Python classes.

## File Structure

```
python/
  iccdev/
    __init__.py          # Package init, re-exports from _iccdev
    _iccdev.pyx          # Cython binding (1204 lines)
    _iccdev.pyi          # PEP 561 type stubs
    cicc_wrapper.pxd     # Cython extern declarations for IccWrapper.h
  tests/
    test_iccdev.py       # 37 tests (enums, utilities, exceptions, profiles, CMM)
  setup.py               # Cython extension build
  pyproject.toml          # PEP 517 build config (setuptools)
  README.md               # Usage and API documentation
IccProfLib/
  IccWrapper.h           # C API declarations (thin wrapper over C++ classes)
  IccWrapper.cpp          # C API implementation
```

## Two Build Modes

### 1. Pre-built library (faster, recommended for development)

```bash
export ICCDEV_BUILD_DIR=$PWD/Build
python -m pip install -e "./python[dev]"
```

Links against `libIccProfLib2-static.a` (or `libIccProfLib2-staticd.a` for Debug builds)
found in `$ICCDEV_BUILD_DIR/IccProfLib/`.

### 2. Inline compilation (portable, no pre-built library needed)

```bash
python -m pip install -e "./python[dev]"
```

If `ICCDEV_BUILD_DIR` is unset or the static library is not found, `setup.py`
compiles the portable IccProfLib `.cpp` files inline into the extension module.
The AVX2 and AVX-512 translation units remain CMake-managed because they need
architecture-specific flags and cannot be compiled in universal or non-x86
source builds.

## Test

```bash
python -m pytest --rootdir . --import-mode=importlib python/tests -v
```

Tests find ICC profiles in `Testing/` from the repository root.
The `find_test_profile()` helper searches for `sRGB_D65_MAT.icc` in several
locations.

## Dependencies and Build Constraints

- Python 3.9+
- Cython >= 3.0,<3.2 for all source builds
- NumPy >= 1.23,<3 for array operations
- setuptools >= 64,<81
- Dev extras: pytest, numpy, build, wheel, twine, cibuildwheel

All GitHub Actions `pip install` lines for this package must use bounded
versions. Do not introduce unbounded build tools in workflows.

## API Overview

```python
import iccdev

# Profile operations
profile = iccdev.IccProfile("path/to/profile.icc")
header = profile.header

# CMM operations
cmm = iccdev.IccCmm()
cmm.attach("input.icc")
cmm.attach("output.icc")
cmm.begin()
```

## Key Implementation Details

- `_iccdev.pyx` wraps the C functions from `IccWrapper.h`, NOT the C++ classes directly
- Pointer lifecycle managed via Python `__dealloc__` (prevent double-free)
- Exception safety: C wrapper returns error codes; Cython translates to Python exceptions
- NumPy arrays used for pixel data in CMM transforms
- Source distributions require Cython and do not ship generated `_iccdev.cpp`
- `vendor_iccproflib.py` writes `vendor/IccProfLib/MANIFEST.sha256` for source evidence
- `setup.py` must keep pre-built library search paths in sync with test helpers
- Unix extensions linked against pre-built static IccProfLib must also link
  zlib when `ICC_USE_ZLIB` is enabled
- Run tests from the repository root with `--import-mode=importlib`; installed
  package validation must set `ICCDEV_REQUIRE_INSTALLED_PACKAGE=1` so tests fail
  if `iccdev` imports from the source checkout instead of site-packages.

## Common Pitfalls

- If `ICCDEV_BUILD_DIR` points to a stale build, the extension may link against
  an old library. Delete the build and rebuild both iccDEV and the extension.
- The inline compilation mode compiles the portable source set and takes
  several minutes.
- `undefined symbol: deflateInit_` means the extension found a zlib-enabled
  static IccProfLib but omitted zlib from the final extension link.
- Test profiles must exist in `../Testing/` -- run `CreateAllProfiles.sh` first.
- Current API is intentionally header/CMM focused. Tag-level reads, in-memory
  profile handles in the Cython object, CMM environment variables, named color
  lookup, and gamut diagnostics require explicit API additions with matching
  `.pyi` stubs and tests. XML/JSON/blob conversion, `iccDumpProfile`, and
  `iccRoundTrip` are exposed through CLI-backed helpers in `iccdev.cli`.

## PR, Merge, and Production Release

Use `docs/python-packaging-release.md` as the maintainer source for Python
package PR validation, merge criteria, and production PyPI release steps.
Python packaging PRs must pass `ci-json-python`, keep
`python/pyproject.toml` and `python/iccdev/__init__.py` versions synchronized,
and avoid committing generated `dist/`, `wheelhouse/`, build tree, virtual
environment, or vendored source artifacts unless a release manager explicitly
requests them.

Production release artifacts must be rebuilt from the maintainer-selected
release commit or tag. Do not publish from pull request branches, forked
branches, or CI scratch artifacts.
