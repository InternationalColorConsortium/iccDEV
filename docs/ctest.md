# CTest Tool Suites

The CMake build exposes the existing iccDEV profile and tool checks through
CTest when `ENABLE_TESTS=ON` and `ENABLE_TOOLS=ON`. The legacy shell and batch
scripts remain the source of truth for profile generation, but CTest owns
discovery, labels, fixtures, timeouts, logs, and the `check` target.

CTest registration is maintainer-owned infrastructure because it affects CI
coverage, workflow pass/fail behavior, and cross-platform release confidence.
General contributors should add or update test inputs under `Testing/` and
describe any needed CTest coverage in the issue or pull request. Changes to
`Build/Cmake/Testing/`, workflow count assertions, generated-profile counts, or
the Windows batch wrapper should be made by iccDEV maintainers unless a
maintainer explicitly asks otherwise.

## Local Commands

Windows examples include both `cmd.exe` and PowerShell forms where shell syntax
differs. If CMake reports `No such preset`, fetch and switch to a branch that
contains the matching `Build/Cmake/CMakePresets.json` update.

Linux and macOS single-config generators:

```bash
cmake -S Build/Cmake -B build \
  -DENABLE_TOOLS=ON \
  -DENABLE_TESTS=ON \
  -DENABLE_WXWIDGETS=OFF \
  -DENABLE_SHARED_LIBS=ON \
  -DENABLE_STATIC_LIBS=ON
cmake --build build --parallel "$(nproc)"
ctest --test-dir build -N --no-tests=error
ctest --test-dir build --output-on-failure --no-tests=error
cmake --build build --target check
```

Windows Visual Studio multi-config generators:

```cmd
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64 ^
  -DENABLE_TESTS=ON ^
  -DENABLE_TOOLS=ON
cmake --build out/vs2022-x64 --config Release -- /m /maxcpucount
ctest --test-dir out/vs2022-x64 -C Release -N --no-tests=error
ctest --test-dir out/vs2022-x64 -C Release --output-on-failure --no-tests=error
cmake --build out/vs2022-x64 --config Release --target check
```

Windows MinGW single-config generators, `cmd.exe`:

```cmd
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
cmake --preset mingw-x64 -S Build/Cmake -B out/mingw-x64 ^
  -DENABLE_TESTS=ON
cmake --build out/mingw-x64 --parallel
ctest --test-dir out/mingw-x64 -R "^iccdev\.(windows-icc-dump-profile-smoke|issue-987-shared-mpe-export)$" --output-on-failure --no-tests=error
```

Windows MinGW single-config generators, PowerShell:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --preset mingw-x64 -S Build/Cmake -B out/mingw-x64 `
  -DENABLE_TESTS=ON
cmake --build out/mingw-x64 --parallel
ctest --test-dir out/mingw-x64 -R "^iccdev\.(windows-icc-dump-profile-smoke|issue-987-shared-mpe-export)$" --output-on-failure --no-tests=error
```

When the local MSYS2 install only has the core compiler and nlohmann-json
packages, use the dependency-light static preset. `cmd.exe`:

```cmd
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
cmake --preset mingw-core-x64 -S Build/Cmake -B out/mingw-core-x64
cmake --build out/mingw-core-x64 --parallel
ctest --test-dir out/mingw-core-x64 -R "iccconnect|icc-dump-profile-smoke" --output-on-failure --no-tests=error
```

PowerShell:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --preset mingw-core-x64 -S Build/Cmake -B out/mingw-core-x64
cmake --build out/mingw-core-x64 --parallel
ctest --test-dir out/mingw-core-x64 -R "iccconnect|icc-dump-profile-smoke" --output-on-failure --no-tests=error
```

Use `--no-tests=error` for discovery and execution so a registration regression
cannot pass as a green no-op.

## Registered Suites

Linux currently registers 23 tests:

| Test | Source |
|------|--------|
| `iccdev.create-profiles` | `Testing/CreateAllProfiles.sh` |
| `iccdev.iccconnect-threaded-cmm` | `.github/ci/regression/iccconnect-threaded-cmm.cpp` |
| `iccdev.legacy-run-tests` | `Testing/RunTests.sh` |
| `iccdev.tool-coverage` | `.github/scripts/iccdev-tool-coverage-baseline.sh --asan --skip-hybrid` |
| `iccdev.hybrid-pipeline` | `.github/scripts/iccdev-hybrid-pipeline-tests.sh` |
| `iccdev.specsep-tiff-geometry-regression` | `.github/scripts/iccdev-specsep-tiff-geometry-regression-tests.sh` |
| `iccdev.dump-profile-header-regression` | `.github/scripts/iccdev-dump-profile-header-regression-tests.sh` |
| `iccdev.pawg-report-regressions` | `.github/scripts/iccdev-pawg-report-regression-tests.sh` |
| `iccdev.json-cfg` | `.github/scripts/iccdev-json-cfg-tests.sh` |
| `iccdev.json-cli-exercise` | `.github/scripts/json-cli-exercise.sh` |
| `iccdev.json-parser-regressions` | `.github/scripts/iccdev-json-parser-regression-tests.sh` |
| `iccdev.stdobserver-regressions` | `.github/scripts/iccdev-stdobserver-regression-tests.sh` |
| `iccdev.mluc-setter-regressions` | `.github/scripts/iccdev-mluc-setter-regression-tests.sh` |
| `iccdev.mluc-read-utf16-regressions` | `.github/scripts/iccdev-mluc-read-utf16-regression-tests.sh` |
| `iccdev.mluc-iso-code-regressions` | `.github/scripts/iccdev-mluc-iso-code-regression-tests.sh` |
| `iccdev.pcc-zero-illuminant-regressions` | `.github/scripts/iccdev-pcc-zero-illuminant-regression-tests.sh` |
| `iccdev.calculator-regressions` | `.github/scripts/iccdev-calculator-regression-tests.sh` |
| `iccdev.lut16-zero-curve-regressions` | `.github/scripts/iccdev-lut16-zero-curve-regression-tests.sh` |
| `iccdev.namedcolor-apply-regressions` | `.github/scripts/iccdev-namedcolor-apply-regression-tests.sh` |
| `iccdev.v5-namedcmm-regressions` | `.github/scripts/iccdev-v5-namedcmm-regression-tests.sh` |
| `iccdev.version-bcd-regressions` | `.github/scripts/iccdev-version-bcd-regression-tests.sh` |
| `iccdev.profile-visualize-regressions` | `.github/scripts/iccdev-profile-visualize-tests.sh` |
| `iccdev.describe-sink-api` | `.github/ci/regression/iccDescribeSinkTest.cpp` |

`iccdev.legacy-run-tests` requires `iccToJson` and `iccFromJson` under CTest.
The JSON round-trip uses a temporary directory for generated `.json` and
round-trip `.icc` files so a passing Unix run does not remove or modify tracked
files in `Testing/`.

`iccdev.tool-coverage` may add focused command-line regressions inside the
existing script without changing the CTest suite count. When a bug is tied to an
AFL-minimized crash or hang, embed the smallest stable reproducer in the script
or generate it under `ICCDEV_TEST_OUTDIR`; do not require local AFL output
directories or commit generated crash artifacts. Validate both the direct script
path and the CTest wrapper when changing this suite. The JSON parser suite
includes malformed curve gamma and out-of-range numeric narrowing coverage, and
must reject invalid numeric fields before conversion without sanitizer findings.

`iccdev.pawg-report-regressions` builds the standalone `iccPawgReport` tool,
checks the 31-item PAWG report structure, verifies summary counts against the
rendered item lines, runs malformed and malware-signature dynamic inputs, and
fails on sanitizer findings.

`iccdev.hybrid-pipeline` preserves the full six-phase hybrid spectral/colorimetric
integration test as a separate `slow` CTest label. Routine CI tool sweeps run
with `--label-exclude slow`; run the hybrid gate explicitly with
`ctest --test-dir build -R '^iccdev\.hybrid-pipeline$' --output-on-failure`.

Use a standalone CTest row for focused crash regressions that need clear
maintainer visibility in CTest output. For example,
`iccdev.specsep-tiff-geometry-regression` generates a malformed TIFF under
`ICCDEV_TEST_OUTDIR`, invokes `iccSpecSepToTiff` using its prefix-plus-channel
input semantics, and verifies graceful rejection without sanitizer findings.
`iccdev.dump-profile-header-regression` mutates the ICC header size field to
`0xffffffff` and verifies `iccDumpProfile -v 100` handles validation reporting
without signed-conversion sanitizer findings.

Windows full tool builds currently register 5 tests:

| Test | Source |
|------|--------|
| `iccdev.iccconnect-threaded-cmm` | `.github/ci/regression/iccconnect-threaded-cmm.cpp` |
| `iccdev.windows-create-profiles` | `Testing/CreateAllProfiles.bat` |
| `iccdev.windows-legacy-run-tests` | `Testing/RunTests.bat` |
| `iccdev.windows-icc-dump-profile-smoke` | `Build/Cmake/Testing/RunWindowsDumpProfileSmokeTest.cmake` |
| `iccdev.issue-987-shared-mpe-export` | `Build/Cmake/Testing/RunWindowsSharedExportTest.cmake` |

The batch-backed Windows tests run through
`Build/Cmake/Testing/RunWindowsBatchTest.cmake`. The wrapper copies `Testing/`
into `build/Testing/ctest-output/windows-testing`, runs the batch scripts from
that disposable directory, verifies key output, and fails if the source
`Testing/` tree is changed.

Windows CTest wrappers collect build-tree DLL directories plus runtime
dependency directories from `CMakeCache.txt`, including `CMAKE_PREFIX_PATH`,
vcpkg installed triplets, compiler `bin` directories, and common dependency
library prefixes. This keeps CTest execution independent of a developer's
interactive `PATH` for tools such as `libxml2.dll` or `libwinpthread-1.dll`.
MinGW builds still need the UCRT64 `bin` directory on the invoking shell `PATH`
because GCC launches runtime-dependent compiler subprocesses during the build.

Feature-disabled Windows builds register the tests whose targets are available.
For example, `mingw-core-x64` does not build XML conversion tools, so it skips
the batch-backed generated-profile suites and can still run the dump-profile
smoke test plus the IccConnect threaded CMM regression.

`iccdev.windows-icc-dump-profile-smoke` runs `iccDumpProfile --read --diag`
against the checked-in `Testing/CalcTest/calcUnderStack_add.icc` profile and
verifies the eager `ReadIccProfile()` path, expected profile size, tag count,
and header/file-size diagnostic. `iccdev.issue-987-shared-mpe-export` is a
focused Windows shared-library regression for MSVC and MinGW. It checks that
`IccProfLib2.dll` exports `CIccTagMultiProcessElement::NumElements` with
`dumpbin` or `objdump`, then builds and runs a small consumer against the
build-tree DLL and import library.

## Fixtures and Logs

`iccdev.create-profiles` and `iccdev.windows-create-profiles` set the
`iccdev_profiles` fixture. Tests that require generated profiles must declare
`FIXTURES_REQUIRED iccdev_profiles`.

CTest logs and per-suite output are written under:

```text
<build>/Testing/Temporary/
<build>/Testing/ctest-output/
```

The CI workflows upload those paths with `ctest-results.xml` and
`ctest-list.txt`.

## Maintainer Add-Test Process

This process is for iccDEV maintainers adding or changing the CTest gate.
For repeatable agent-assisted work, use
`.github/skills/maintainer-ci-ctest/SKILL.md` or
`.github/prompts/maintainer-ci-ctest.prompt.md`.

1. Add or update the legacy test input:
   - Profile XML: update `Testing/CreateAllProfiles.sh` and
     `Testing/CreateAllProfiles.bat`.
   - Profile validation: update `Testing/RunTests.sh` and
     `Testing/RunTests.bat`.
   - Focused Linux regression: add or update a `.github/scripts/*.sh` script.
2. Register the test in `Build/Cmake/Testing/CMakeLists.txt`.
   - Use `iccdev_add_script_test()` for Linux shell tests.
   - Use `iccdev_add_windows_batch_test()` for Windows batch-backed tests.
   - Set labels and a timeout.
   - Add `FIXTURES_REQUIRED iccdev_profiles` when the test needs generated
     profiles.
3. Update hard-coded coverage counts when the suite count or generated profile
   count changes:
   - Linux CTest count in `.github/workflows/ci-tool-tests.yml`.
   - Linux CTest count in `.github/workflows/ci-iccdev-tool-tests.yml`.
   - Windows profile parse count in `Build/Cmake/Testing/CMakeLists.txt`.
   - JSON profile parse count in `.github/workflows/ci-json-roundtrip.yml`.
     Keep these in sync with `Testing/CreateAllProfiles.sh` and
     `Testing/CreateAllProfiles.bat`.
   - WASM expected ICC count in `Build/Cmake/wasm-package/regression.js`,
     `.github/workflows/ci-pr-wasm.yml`, `.github/workflows/ci-pr-action.yml`,
     and `.github/workflows/ci-latest-release.yml` when
     `Testing/CreateAllProfiles.sh` changes the generated-profile set.
4. Validate locally with CMake configure, build, `ctest -N --no-tests=error`,
   `ctest --output-on-failure --no-tests=error`, and `git diff --check`.
   For changes inside `iccdev.tool-coverage`, also run the direct script with
   explicit `ICCDEV_TOOLS_DIR`, `ICCDEV_TESTING_DIR`, and
   `ICCDEV_TEST_OUTDIR`, plus `ctest -R '^iccdev\.tool-coverage$'`.

Do not let a workflow keep `|| true` around profile generation, CTest
discovery, or regression execution. Expected skips should be explicit in the
script output and still leave a deterministic pass/fail condition.
