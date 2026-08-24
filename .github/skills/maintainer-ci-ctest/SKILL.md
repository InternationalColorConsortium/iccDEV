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
- Do not add hard-coded CTest suite totals to workflows, docs, or maintainer
  instructions; keep suite lists descriptive and let CTest discovery report the
  current total.
- Adding checks inside `iccdev-tool-coverage-baseline.sh` does not change that
  count; validate the direct script and `ctest -R '^iccdev\.tool-coverage$'`.
- Windows full builds include focused executable regressions, batch-backed
  suites, dump/profile smoke coverage, shared-export coverage, and PAWG report
  coverage.
- The comprehensive build matrix must retain MSVC, ClangCL, and MinGW UCRT64
  coverage, plus a separate MSVC full CTest gate with warnings treated as
  errors. Unix must likewise retain the full strict CTest gate.
- Use `rg "Total Tests:|currently register|ci[-]tool[-]tests[.]yml" docs .github`
  before PR handoff to catch stale count and workflow-name references.
- Generated-profile count changes must update every explicit assertion source,
  including `Build/Cmake/Testing/CMakeLists.txt`, generated-profile workflows,
  and packaged WASM regression scripts.
- Do not duplicate generated-profile totals in this skill; use the assertion
  sources as the current truth.
- Windows batch CTest runs must use the disposable Testing copy under the build
  tree and must not dirty the source `Testing/` directory.
- Windows executable tests must receive runtime DLL directories through
  `Build/Cmake/Testing/WindowsRuntimePaths.cmake`; do not rely on a developer or
  runner shell `PATH` for vcpkg or MinGW runtime DLLs.
- MinGW builds still need UCRT64 `bin` on the invoking shell `PATH` because GCC
  subprocesses such as `cc1plus.exe` depend on MSYS2 runtime DLLs during build.

## Workflow Rules

- Follow `.github/instructions/workflow-governance.instructions.md`.
- Treat `ci-pr-action` `full` as the explicit long-cycle maintainer gate. It runs
  Unix GCC/Clang Release and Debug builds, exact GCC 15.2 strict Release LTO in
  the regression container, GCC 15.2 ASAN+UBSAN tool tests, Windows, and Docker.
  Its tool-test caller excludes the `pr-extended` CTest label to stay within
  the PR runtime budget; `ci-regression-checks` continues to run the labelled
  tests.
- `ci_scope=auto` is the default. It selects the full matrix for source, build,
  test, and container changes; workflow-only changes run the preflight and
  workflow-security gates.
- Use `ci_scope=fast-lane` for the exact GCC 15.2 Release LTO and ASAN+UBSAN
  Release tool lanes. Fast lane defaults to the latest CTest with Windows
  disabled; Docker runs when the PR changes the container surface.
- Container changes require the Docker PR verification lane. Do not claim the
  container surface is verified until its local image build and hosted lane pass.
- Do not use `|| true` around profile generation, CTest discovery, regression
  execution, sanitizer checks, or packaging verification.
- Use least-privilege permissions and credential cleanup.
- Sanitize all `GITHUB_STEP_SUMMARY` and `GITHUB_OUTPUT` writes.
- Check out the base ref's `.github/scripts` sparsely and source its sanitizer
  helpers for every workflow that executes PR-controlled source.
- Include `github.event_name` in concurrency keys for workflows that accept
  both PR and manual-dispatch events, so a manual lane cannot cancel its PR
  counterpart.
- Trigger shared-concurrency workflows sequentially to avoid canceling your own
  run. Use `ci-pr-action` for normal maintainer validation and
  `ci-regression-checks` through that orchestrator for ASAN/UBSAN CTest coverage.

## Local Validation

For focused workflow iteration, start with:

```bash
.github/scripts/preflight-safety-checks.sh --fast-lane
```

This scans changed workflow/script surfaces without running CTest or local
CodeQL databases. Run only the nearest feature test during iteration, then use
`--fast-lane=matlab` for MATLAB-only work. Use the full preflight or hosted
gates before final handoff.

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

For the unified `Dockerfile`, also run a no-cache build and smoke
`clang`, `clang++`, `gcc`, `g++`, `cmake`, `afl-fuzz`, and `/usr/bin/time`. If the image is
published, pass the published branch or SHA tag to `ci-iccdev-tool-tests.yml`.

## GitHub Validation

After pushing, trigger only the workflows affected by the change:

```bash
gh workflow run "ci-pr-action" --repo InternationalColorConsortium/iccDEV --ref <branch> -f ci_scope=full
gh workflow run "ci-pr-action" --repo InternationalColorConsortium/iccDEV --ref <branch> \
  -f ci_scope=fast-lane -f pr_number=<open-pr-number>
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
- For registry QA runs, report `summary.md`, `results.tsv`, and `findings.txt`
  as authoritative evidence. Note whether per-run logs were bounded by
  `registry_qa_log_tail_lines`; use `0` only when full raw logs are needed.
  Developer reports must preserve downloaded profile payloads so reviewers can
  inspect and rerun failing inputs without a second download step.
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
