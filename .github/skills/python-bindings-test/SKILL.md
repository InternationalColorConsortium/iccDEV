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

6. **Run CI workflow locally**

   ```bash
   gh workflow run ci-json-python.yml
   ```

## Validation Checklist

- [ ] `iccToXml`, `iccFromXml`, `iccToJson`, `iccFromJson`, `iccDumpProfile`, and `iccRoundTrip` binaries built
- [ ] CreateAllProfiles completes without errors
- [ ] RunTests passes all tests
- [ ] Python package installs cleanly
- [ ] Tests run from repository root with `--import-mode=importlib`
- [ ] XML/JSON/blob helper parity tests pass
- [ ] Profile dump and round-trip parity tests pass
- [ ] No ASAN/UBSAN findings during test execution
