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

Use `--no-tests=error` for discovery and execution so a registration regression
cannot pass as a green no-op.

## Registered Suites

Linux currently registers 16 tests:

| Test | Source |
|------|--------|
| `iccdev.create-profiles` | `Testing/CreateAllProfiles.sh` |
| `iccdev.legacy-run-tests` | `Testing/RunTests.sh` |
| `iccdev.tool-coverage` | `.github/scripts/iccdev-tool-coverage-baseline.sh --asan` |
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

`iccdev.legacy-run-tests` requires `iccToJson` and `iccFromJson` under CTest.
The JSON round-trip uses a temporary directory for generated `.json` and
round-trip `.icc` files so a passing Unix run does not remove or modify tracked
files in `Testing/`.

Windows currently registers 2 tests:

| Test | Source |
|------|--------|
| `iccdev.windows-create-profiles` | `Testing/CreateAllProfiles.bat` |
| `iccdev.windows-legacy-run-tests` | `Testing/RunTests.bat` |

The Windows tests run through
`Build/Cmake/Testing/RunWindowsBatchTest.cmake`. The wrapper copies
`Testing/` into `build/Testing/ctest-output/windows-testing`, runs the batch
scripts from that disposable directory, verifies key output, and fails if the
source `Testing/` tree is changed.

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
   - WASM expected ICC count in `.github/workflows/ci-tool-tests.yml` when
     `Testing/CreateAllProfiles.sh` changes the generated-profile set.
4. Validate locally with CMake configure, build, `ctest -N --no-tests=error`,
   `ctest --output-on-failure --no-tests=error`, and `git diff --check`.

Do not let a workflow keep `|| true` around profile generation, CTest
discovery, or regression execution. Expected skips should be explicit in the
script output and still leave a deterministic pass/fail condition.
