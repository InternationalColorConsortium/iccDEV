# Debug Python/Cython Bindings

Use this prompt when the iccdev Python/Cython package fails to build, import, or
produces incorrect results from ICC profile operations.

For the compiler-free install, use `python -m pip install iccdev` on a
supported CPython 3.9-3.13 platform. Install `iccdev[numpy]` when diagnosing
array operations. The rolling Latest Build remains the source for exact-tag
wheel URLs; check CPython ABI, OS, and architecture before diagnosing a
rejected wheel. PyPI currently publishes wheels, not a source distribution, so
use the documented Git source install if no compatible wheel exists. For
release failures, run the workflow regression suites from the repository root:

```bash
python -m pytest --rootdir . --import-mode=importlib .github/tests/test_python_release_wheels.py .github/tests/test_python_test_collection.py -q
```

Validate each lane with `python .github/scripts/validate-python-release-wheels.py
<wheel-dir> --identifiers <wheel-dir>/build-identifiers.txt --lane <lane-id>`.
The lane selects the complete expected inventory independently of the artifact.
For all downloaded release wheels, use `python
.github/scripts/validate-python-release-wheels.py <release-assets> --release`.
This needs no CI identifier files and rejects missing targets and inconsistent
RECORD file coverage, hashes, or sizes. Verify both published checksum manifests
and artifact provenance separately; record the release tag and source SHA.
See `docs/python-packaging-release.md` for the artifact and source-build paths.

For wheel CI failures, inspect the shared `_build-python-wheels.yml` used by
both `ci-json-python` and `ci-latest-release`. Python 3.13 drives cibuildwheel
3.4.0 for the full seven-lane CPython 3.9-3.13 matrix. Windows ARM64 Python
3.9/3.10 omit optional NumPy; other targets require binary NumPy wheels. Both
callers run the installed-wheel `not parity` suite. Primary CI keeps its
native parity and sdist checks, rejects empty test collection, and disables
shared wheel artifact upload (`upload-wheels: false`); the release caller uploads
explicitly named artifacts (`upload-wheels: true`). Keep wheel build jobs
unprivileged and release credentials confined to their publishing steps.

## Build Diagnostics

1. **Check Cython compilation**
   ```bash
   python -m pip install -e "./python[dev]" 2>&1 | tail -20
   ```

2. **Verify shared library linkage**
   ```bash
   python3 -c "import iccdev._iccdev as m; print(m.__file__)"
   ldd $(python3 -c "import iccdev._iccdev as m; print(m.__file__)")
   ```

3. **Test basic import**
   ```python
   import iccdev
   print(iccdev.__version__)
   profile = iccdev.IccProfile("test.icc")
   print(profile.header)
   ```

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| `ImportError: undefined symbol: deflateInit_` | Static IccProfLib linked without zlib | Ensure the extension link includes zlib |
| Other undefined IccProfLib symbol | Missing or stale libIccProfLib2 | Set `ICCDEV_BUILD_DIR`, rebuild, and reinstall |
| macOS arm64 error in `IccTagLutAvx*.cpp` | Standalone build compiled CMake-managed x86 SIMD sources | Keep AVX2/AVX-512 sources out of inline or universal builds |
| Cython compilation error | Missing headers | Install libxml2-dev, libtiff-dev |
| Segfault on profile load | ASAN mismatch | Build with matching sanitizer flags |
| Version mismatch | Stale extension | python -m pip install -e "./python[dev]" --force-reinstall |

For isolated sdist validation, do not assume runtime environments contain
setuptools. Packaging-configuration tests should skip when build dependencies
are intentionally absent; importing `iccdev` must not require setuptools.

## JSON Parity Testing

```bash
# Run full native parity suite from the repository root
ICCDEV_BUILD_DIR=$PWD/Build python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m parity
```
