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

- Update `docs/ctest.md` for CTest names, fixtures, expected suite counts,
  profile parse counts, or add-test process changes.
- Update `.github/instructions/testing.instructions.md` when the test becomes
  standard policy.
- Update `docs/regression-workflow-governance.md` for workflow process changes.
- Update `.github/skills/README.md` or a skill when the process becomes a
  repeatable maintainer workflow.
- When adding cases inside an existing script-backed suite, document that the
  CTest suite count is unchanged and validate both direct script execution and
  the CTest wrapper.
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

Trigger workflows with shared concurrency sequentially. Do not launch
`iccDEV Tool Tests` and `ci-regression-checks` at the same time on the same
branch.

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
