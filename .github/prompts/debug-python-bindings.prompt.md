# Debug Python/Cython Bindings

Use this prompt when the iccdev Python/Cython package fails to build, import, or
produces incorrect results from ICC profile operations.

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
