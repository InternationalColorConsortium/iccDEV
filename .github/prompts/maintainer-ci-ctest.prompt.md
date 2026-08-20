# Maintainer CI and CTest Change Plan

Use this prompt for iccDEV maintainer-owned changes to CI, CTest, CPack,
sanitizer policy, release packaging, vcpkg verification, CodeQL/workflow
governance, or security automation.

## Inputs

- Branch:
- Issue or PR:
- Maintainer approval source:
- Infrastructure area:
- Behavior to prove:
- Platforms required:
- Expected pass signal:
- Expected failure signal:

## Scope Boundary

Confirm that the work is maintainer-owned before editing:

- `.github/**`
- `Dockerfile*`
- `Build/Cmake/Testing/**`
- workflow count assertions
- CPack, release packaging, installer, or artifact publishing logic
- sanitizer helper scripts and sanitizer policy
- CodeQL, workflow governance, or security automation
- vcpkg port release verification

If the request comes from a general contributor, ask them to describe the
needed coverage in the issue or PR and leave infrastructure edits to
maintainers unless an iccDEV maintainer explicitly approves the change.

## Decision Checklist

Choose the smallest gate that proves the behavior:

- `ci-pr-action` full: explicit long-cycle Unix GCC/Clang Release and Debug,
  exact GCC 15.2 strict Release LTO, GCC 15.2 ASAN+UBSAN tool tests, Windows,
  and Docker verification. Its tool-test caller excludes only the
  `pr-extended` CTests (`hybrid-pipeline`, `json-sort-regression`, and the
  issue-1781 `iccApplyToLink` QA matrix); they remain enabled in
  `ci-regression-checks`.
- `ci-pr-action` fast lane: exact GCC 15.2 strict Release LTO plus GCC 15.2
  ASAN+UBSAN Release tool validation, latest CTest by default, with Windows and
  Docker opt-in.
- `ci-pr-action` auto: default path-scoped selection. Source, build, test, and
  container changes select the full matrix; workflow-only changes use preflight
  and workflow-security gates.
- CTest suite: cross-platform tool/profile behavior that belongs in the normal
  local and CI test surface.
- Focused `.github/scripts/*.sh` regression: reusable Linux regression logic or
  parser/security invariant.
- Workflow inline step: short one-off CI assertion tied to a specific workflow.
- CPack or package smoke: install/export/uninstall, bundled consumers, or
  release artifact structure.
- Sanitizer gate: memory-safety, parser, or profile-controlled undefined
  behavior where sanitizer output is the pass/fail signal.
- vcpkg gate: overlay port, triplet, static CRT, or packaged consumer behavior.

## Required Updates

- Update `docs/ctest.md` for CTest names, fixtures, profile parse counts, or
  add-test process changes.
- Update `.github/instructions/testing.instructions.md` when the test becomes
  standard policy.
- Update `docs/regression-workflow-governance.md` for workflow process changes.
- Update `docs/build.md` and `docs/regression-workflow-governance.md` when
  changing maintainer Dockerfiles, container dependencies, GHCR publish flow, or
  pinned regression image digests.
- Update `.github/skills/README.md` or a skill when the process becomes a
  repeatable maintainer workflow.
- For registry QA workflow runs, preserve `summary.md`, `results.tsv`, and
  `findings.txt` as the review source of truth. The workflow may retain bounded
  log excerpts by default; rerun with `registry_qa_log_tail_lines=0` only when
  complete raw per-tool logs are required. Preserve downloaded registry profile
  payloads in developer reports so failing inputs remain available for review.
- On `ci-qa-pr-docker-testing`, Docker PR verification is advisory. If it fails,
  keep the orchestrator result successful when required non-Docker gates pass,
  add `bump-sha-pins`, update pinned action or container SHAs, and rerun Docker
  before claiming the container lane is verified.
- When adding cases inside an existing script-backed suite, document that the
  CTest suite count is unchanged and validate both direct script execution and
  the CTest wrapper.
- When changing generated-profile totals, update every assertion source. For
  WASM parity this includes `Build/Cmake/wasm-package/regression.js`,
  `.github/workflows/ci-pr-wasm.yml`, `.github/workflows/ci-pr-action.yml`, and
  `.github/workflows/ci-latest-release.yml`.
- For Windows executable tests, keep runtime DLL path handling centralized in
  `Build/Cmake/Testing/WindowsRuntimePaths.cmake`; update docs and skills when
  a test needs vcpkg, Visual Studio LLVM, or MinGW runtime DLLs.
- Keep root contributor docs focused on boundaries and routing, not internal
  workflow mechanics.

## Validation Commands

```bash
file <changed-files>
git diff --check
cmake -S Build/Cmake -B build -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -DENABLE_WXWIDGETS=OFF
cmake --build build --parallel "$(nproc)"
ctest --test-dir build -N --no-tests=error
ctest --test-dir build --output-on-failure --no-tests=error
```

For `.github/scripts/iccdev-tool-coverage-baseline.sh` updates:

```bash
ICCDEV_TOOLS_DIR=$PWD/build/Tools \
ICCDEV_TESTING_DIR=$PWD/Testing \
ICCDEV_TEST_OUTDIR=/tmp/iccdev-tool-output \
  .github/scripts/iccdev-tool-coverage-baseline.sh --asan --quick
ctest --test-dir build -R '^iccdev\.tool-coverage$' --output-on-failure
```

For `Dockerfile*` updates:

```bash
docker build -t iccdev-container-check -f <Dockerfile> .
docker run --rm iccdev-container-check <smoke-command>
```

For `Dockerfile.ci-regression`, use a no-cache build and smoke `clang`,
`clang++`, `gcc`, `g++`, `cmake`, `afl-fuzz`, and `/usr/bin/time`. If the image must be published,
publish through the maintainer-controlled container release path, then pass the
published branch or SHA tag to `ci-iccdev-tool-tests.yml`.

For PR or issue proof with the published maintainer image, follow
[Prove a Local PR Worktree](../../docs/regression-container.md#prove-a-local-pr-worktree).
For tool-specific behavior, build the affected target and
`build-test-binaries`, then run the focused registered CTest. Record the image
digest, image source revision, tested worktree commit, command, exit status,
warning count, and sanitizer result before handoff.

Workflow YAML:

```bash
python3 -c "import yaml; [yaml.safe_load(open(p)) for p in ['.github/workflows/<workflow>.yml']]; print('YAML parse OK')"
actionlint -no-color .github/workflows/<workflow>.yml
```

GitHub verification:

```bash
gh workflow run "<workflow>" --repo InternationalColorConsortium/iccDEV --ref <branch>
gh run watch <run-id> --repo InternationalColorConsortium/iccDEV --exit-status
```

Trigger workflows with shared concurrency sequentially. Use `ci-pr-action` for
normal maintainer validation; it defaults to `ci_scope=auto`. Use
`ci_scope=fast-lane` with an open same-repository PR number for the shortest
exact GCC 15.2 lane. Use `ci-regression-checks` through the orchestrator for
ASAN/UBSAN CTest coverage.

## Handoff Format

Report:

- Maintainer-owned area changed:
- Files changed:
- Expected counts:
- Local validation:
- GitHub runs:
- Artifacts:
- Annotations or deferred warnings:
- Windows or package validation still needed:
