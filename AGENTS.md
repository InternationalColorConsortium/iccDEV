# AGENTS.md -- iccDEV

High-signal handoff for agents. Deeper details live in `.github/copilot-instructions.md`
and `.github/instructions/*.instructions.md`.

## Project

RefIccMAX (iccDEV): ICC profile libraries and CLI tools. C++17, CMake, BSD
3-Clause. Prefer small fixes with exact repros and regression coverage.

## Latest Bisect Hunting

Active JSON/config branch: `bisect-60bbb8c-json`

Local worktree used for fixes:
`~/bisect/iccDEV-bisect-60bbb8c-json`

Latest pushed fixes:
- `4ffcba5 fix: restore JSON config round-trips`
- `0eca71b fix: harden JSON parser regressions`

Reports in `~/bisect/`:
- `iccdev-json-it8-srcType-report.txt`
- `iccdev-json-parser-regression-report.txt`

Before changing this branch again, run:

```bash
cd ~/bisect/iccDEV-bisect-60bbb8c-json
cd Build && cmake --build . --target iccFromJson iccToJson \
  iccApplySearch iccApplyNamedCmm iccApplyProfiles -j"$(nproc)"
cd ..
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing \
  LD_LIBRARY_PATH=$PWD/Build/IccProfLib:$PWD/Build/IccXML:$PWD/Build/IccJSON \
  .github/scripts/iccdev-json-parser-regression-tests.sh
```

JSON/config parser rule: fail closed. Do not silently truncate arrays, skip bad
struct members, attach failed nested MPE elements, accept malformed fixed-size
arrays, or retain stale state across reset/fromJson calls.

## Build/Test Essentials

```bash
cd Build && cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build . -j"$(nproc)"
Testing/CreateAllProfiles.sh
Testing/RunTests.sh
```

Full sanitizer bug-hunt builds need `address,undefined,integer`; add
`float-divide-by-zero,float-cast-overflow` when investigating numeric bugs.
Delete `Build/CMakeCache.txt` before changing sanitizer flags.

## Agent Rules

- Use `git --no-pager status --short --branch` before edits.
- Keep files ASCII; verify generated/edited files with `file`.
- Use 2-space C++ indent, K&R braces, `m_` members, return-value errors.
- Exit 1-127 is graceful failure, not a crash. Exit 128+ is signal/crash.
- Add the nearest regression test for every behavior fix.
- Keep PoC reports in `~/bisect/` unless asked to commit artifacts.

## Fast Navigation

| Need | File |
|------|------|
| Build, test, style, CI | `.github/copilot-instructions.md` |
| Regression bisect workflow | `.github/prompts/bisect-regression.prompt.md` |
| Security repro | `.github/prompts/reproduce-security-issue.prompt.md` |
| Issue filing format | `.github/prompts/file-security-issue.prompt.md` |
| Library hardening | `.github/instructions/icc-library-code.instructions.md` |
| Workflow hardening | `.github/instructions/workflow-governance.instructions.md` |
| Testing details | `.github/instructions/testing.instructions.md` |
