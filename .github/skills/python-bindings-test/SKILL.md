# Python Bindings Build and Test

Build and validate the iccdev Python/Cython bindings, CLI-backed XML/JSON/blob
helpers, profile dump, and round-trip parity tests.

## Steps

1. **Build iccDEV with tools enabled**

   ```bash
   cmake -S Build/Cmake -B Build -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON
   cmake --build Build --parallel
   ```

2. **Generate test profiles**

   ```bash
   cd Testing
   sh CreateAllProfiles.sh
   sh RunTests.sh
   cd ..
   ```

3. **Install Python package**

   ```bash
   python -m pip install -e "./python[dev]"
   ```

4. **Run installed-import unit tests from the repository root**

   ```bash
   python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m "not parity"
   ```

5. **Run native parity tests**

   ```bash
   ICCDEV_BUILD_DIR=$PWD/Build python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m parity
   ```

   Expected: XML/JSON/blob helper conversion, `iccDumpProfile`, `iccRoundTrip`,
   and PAWG smoke tests pass or skip only when the native tool build is absent.

6. **Validate both package build modes**

   - With `ICCDEV_BUILD_DIR`, verify the extension imports and its Unix linkage
     includes zlib when the static library was built with `ICC_USE_ZLIB`.
   - Without `ICCDEV_BUILD_DIR`, build and install the sdist in an isolated
     environment. The portable inline source set must exclude the AVX2 and
     AVX-512 translation units.
   - Run installed-package tests without assuming setuptools is present at
     runtime.

7. **Validate shared wheel testing and release intake**

   From the repository root, use Python 3.13 with cibuildwheel 3.4.0, pytest,
   PyYAML, and packaging installed so the wheel inventory test is collected:

   ```bash
   python -m pytest --rootdir . --import-mode=importlib .github/tests/test_python_release_wheels.py .github/tests/test_python_test_collection.py -q
   python .github/scripts/validate-python-release-wheels.py <wheel-dir> --identifiers <wheel-dir>/build-identifiers.txt --lane <lane-id>
   ```

   Use the matrix lane ID so a truncated identifier file cannot reduce the
   expected wheel inventory. Both `ci-json-python` and
   `ci-latest-release` call `_build-python-wheels.yml`: Python 3.13 drives
   cibuildwheel 3.4.0 across seven lanes and 45 CPython 3.9-3.13 wheels. Run
   the same installed-wheel `not parity` suite in both callers. Windows ARM64
   Python 3.9/3.10 omit optional NumPy; all other targets require binary NumPy.
   Primary CI sets `upload-wheels: false` and `run-wheel-smoke: false` for its
   native jobs; other native callers keep the default smoke check. The release
   caller sets `upload-wheels: true`,
   downloads each `iccdev-python-*` artifact by name, and includes raw wheels
   in checksums and provenance. Do not reuse PR artifacts for publication.
   Require consistent wheel metadata/tags, native extension and type stubs,
   safe unique archive members, and the canonical BSD-3-Clause license.
   Require RECORD coverage, secure file hashes, and sizes. For raw downloaded
   release assets without CI identifier files, run the validator with
   `--release` to require all 45 wheels. Verify SHA256SUMS, SHA512SUMS, and
   provenance separately before installation. Record the immutable release
   tag and source SHA; branch artifacts are local QA evidence, not a release.
   Follow `docs/python-packaging-release.md` for the interim GitHub route;
   do not infer PyPI publication from a branch push or successful local build.

8. **Run CI manually when requested**

   ```bash
   gh workflow run ci-json-python.yml
   ```

## Validation Checklist

- [ ] `iccToXml`, `iccFromXml`, `iccToJson`, `iccFromJson`, `iccDumpProfile`, and `iccRoundTrip` binaries built
- [ ] CreateAllProfiles completes without errors
- [ ] RunTests passes all tests
- [ ] Python package installs cleanly
- [ ] Pre-built Unix extension resolves zlib symbols
- [ ] Standalone sdist builds on non-x86 or universal targets without x86 SIMD sources
- [ ] Installed-package smoke does not require setuptools
- [ ] Primary and release callers use the same full installed-wheel matrix
- [ ] No tests collected (pytest exit code 5) fails primary CI
- [ ] Release wheel intake rejects missing, duplicate, mismatched, or incomplete wheels and identifier inventories
- [ ] Wheel metadata, archive paths, and complete license contents pass validation
- [ ] Raw release wheels participate in SHA256/SHA512 checksums and provenance
- [ ] Tests run from repository root with `--import-mode=importlib`
- [ ] XML/JSON/blob helper parity tests pass
- [ ] Profile dump and round-trip parity tests pass
- [ ] For native bug fixes, report separate ASAN/UBSAN results; release-wheel tests alone do not establish sanitizer coverage
