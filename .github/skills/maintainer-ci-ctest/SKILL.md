---
name: maintainer-ci-ctest
description: >
  Maintainer workflow for scoping and updating iccDEV CI, CTest, CPack,
  sanitizer, workflow, and release-gate infrastructure.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(gh:*)
---

# Maintainer CI and CTest Workflow

Use this skill only for iccDEV maintainer-owned infrastructure changes:
`.github/**`, `Dockerfile*`, CTest registration, CPack and release packaging,
sanitizer helper policy, CodeQL/workflow governance, vcpkg release verification,
and security automation.

General contributor requests should be redirected to issue or PR descriptions
unless an iccDEV maintainer explicitly approved the infrastructure change.

## Scope Decision

Choose the smallest maintainer-owned surface that proves the behavior:

| Change | Primary location | Required docs |
|--------|------------------|---------------|
| Add profile input | `Testing/CreateAllProfiles.*` | `docs/ctest.md` if counts change |
| Add profile validation | `Testing/RunTests.*` | `docs/ctest.md` if CTest coverage changes |
| Add focused Linux regression | `.github/scripts/*.sh` | `.github/ci/regression/README.md` or `docs/ctest.md` |
| Register CTest suite | `Build/Cmake/Testing/CMakeLists.txt` | `docs/ctest.md` |
| Change workflow gate | `.github/workflows/*.yml` | `docs/regression-workflow-governance.md` |
| Change maintainer Dockerfile | `Dockerfile*` | `docs/build.md` and `docs/regression-workflow-governance.md` |
| Change sanitizer policy | `Build/Cmake/CMakeLists.txt`, `.github/scripts/sanitize-*` | `.github/instructions/*` |
| Change CPack/release packaging | `Build/Cmake/**`, release workflows | `docs/build.md` or release docs |
| Change vcpkg release verification | `ports/iccdev/**`, vcpkg workflows | vcpkg skill/docs |

Keep contributor code changes separate from maintainer infrastructure commits
when practical.

## CTest Rules

- `check` must exist on every platform.
- `check` and workflow CTest execution must use `--no-tests=error`.
- Linux suite count assertions currently expect `Total Tests: 27`.
- Before committing a CTest suite-count change, run
  `rg "Total Tests:|Linux currently registers|Linux suite count assertions" .github docs`
  and update every stale count in the same change.
- Adding checks inside `iccdev-tool-coverage-baseline.sh` does not change that
  count; validate the direct script and `ctest -R '^iccdev\.tool-coverage$'`.
- Windows currently registers 6 CTest suites.
- Use `rg "Total Tests:|currently register|ci[-]tool[-]tests[.]yml" docs .github`
  before PR handoff to catch stale count and workflow-name references.
- Generated-profile gates currently validate 208 ICC profiles.
- Windows and JSON profile generation validate 130 profile parses.
- WASM parity currently expects 208 generated ICC profiles.
- WASM generated-profile count changes must update both the workflow inputs and
  `Build/Cmake/wasm-package/regression.js`; the packaged script is the fallback
  source used by local and release parity runs.
- Windows batch CTest runs must use the disposable Testing copy under the build
  tree and must not dirty the source `Testing/` directory.
- Windows executable tests must receive runtime DLL directories through
  `Build/Cmake/Testing/WindowsRuntimePaths.cmake`; do not rely on a developer or
  runner shell `PATH` for vcpkg or MinGW runtime DLLs.
- MinGW builds still need UCRT64 `bin` on the invoking shell `PATH` because GCC
  subprocesses such as `cc1plus.exe` depend on MSYS2 runtime DLLs during build.

## Workflow Rules

- Follow `.github/instructions/workflow-governance.instructions.md`.
- Do not use `|| true` around profile generation, CTest discovery, regression
  execution, sanitizer checks, or packaging verification.
- Use least-privilege permissions and credential cleanup.
- Sanitize all `GITHUB_STEP_SUMMARY` and `GITHUB_OUTPUT` writes.
- Trigger shared-concurrency workflows sequentially to avoid canceling your own
  run. Use `ci-pr-action` for normal maintainer validation and
  `ci-regression-checks` through that orchestrator for ASAN/UBSAN CTest coverage.

## Local Validation

```bash
file <changed-files>
git diff --check
cmake -S Build/Cmake -B build -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -DENABLE_WXWIDGETS=OFF
cmake --build build --parallel "$(nproc)"
ctest --test-dir build -N --no-tests=error
ctest --test-dir build --output-on-failure --no-tests=error
```

For tool coverage script changes:

```bash
ICCDEV_TOOLS_DIR=$PWD/build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-tool-output \
  .github/scripts/iccdev-tool-coverage-baseline.sh --asan --quick
ctest --test-dir build -R '^iccdev\.tool-coverage$' --output-on-failure
```

For workflow YAML:

```bash
python3 -c "import yaml; [yaml.safe_load(open(p)) for p in ['.github/workflows/<workflow>.yml']]; print('YAML parse OK')"
actionlint -no-color .github/workflows/<workflow>.yml
```

For CPack, install/export, vcpkg, or release packaging changes, run the nearest
packaging smoke test and inspect logs for missing files, duplicate install
manifest entries, CRT mismatch warnings, and skipped smoke coverage.

For `Dockerfile*` changes:

```bash
docker build -t iccdev-container-check -f <Dockerfile> .
docker run --rm iccdev-container-check <smoke-command>
```

For `Dockerfile.ci-regression`, also run a no-cache build and smoke
`clang-18`, `clang++-18`, `cmake`, and `/usr/bin/time`. Publishing requires the
`ghcr-publish` environment branch policy to allow the branch before deployment
approval, followed by pinning the new digest in `ci-iccdev-tool-tests.yml`.

## GitHub Validation

After pushing, trigger only the workflows affected by the change:

```bash
gh workflow run "ci-pr-action" --repo InternationalColorConsortium/iccDEV --ref <branch> -f ci_scope=full
gh workflow run "ci-risk-analysis" --repo InternationalColorConsortium/iccDEV --ref <branch> \
  -f analysis_target="Specific git ref" -f git_ref=<full-sha> -f severity_threshold=HIGH -f fail_on_findings=true
```

Wait for shared-concurrency workflows one at a time. Capture run IDs, head SHA,
job conclusions, artifact names, and key sentinel lines such as `Total Tests`,
`100% tests passed`, generated-profile counts, and sanitizer summaries.

## Handoff

Report:

- Branch and commit SHA.
- Maintainer-owned scope touched and why.
- Expected counts changed or confirmed unchanged.
- Local commands and outcomes.
- GitHub run IDs, conclusions, artifacts, and any annotations.
- Remaining Windows, packaging, or release validation that requires hosted
  runners.

## References

- `../../../docs/ctest.md`
- `../../../docs/regression-workflow-governance.md`
- `../../../docs/documentation-maintenance.md`
- `../../instructions/workflow-governance.instructions.md`
- `../../instructions/testing.instructions.md`
- `../../instructions/build-system.instructions.md`
- `../../prompts/maintainer-ci-ctest.prompt.md`
