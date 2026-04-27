# Copilot Instructions for iccDEV

Use this as the short cross-cutting guide. Path-specific details auto-load from
`.github/instructions/*.instructions.md`; prefer those over duplicating content
here.

## Path-Specific Instructions

| Pattern | File | Purpose |
|---------|------|---------|
| `.github/workflows/**` | `instructions/workflow-governance.instructions.md` | Shell hardening, sanitizer, injection prevention |
| `.github/scripts/**` | `instructions/sanitizer-scripts.instructions.md` | sanitize-sed.sh / sanitize.ps1 API |
| `Build/**` | `instructions/build-system.instructions.md` | CMake, platform notes, WASM/LTO |
| `IccProfLib/**`, `IccXML/**`, `Tools/**`, `IccJSON/**` | `instructions/icc-library-code.instructions.md` | Parser hardening, sanitizer compatibility |
| `Testing/**` | `instructions/testing.instructions.md` | Test scripts, profile dirs, regression flow |
| `IccProfLib/icProfileHeader.h` | `instructions/icc-specification.instructions.md` | ICC header/tags/color-space rules |
| `ports/**` | `instructions/vcpkg-port.instructions.md` | vcpkg overlay port and CI |
| `Tools/Winnt/IccIisIsapi/**` | `Tools/Winnt/IccIisIsapi/isapi-instructions.md` | IIS ISAPI setup and hardening |

## Current JSON/Config Bisect

Branch: `bisect-60bbb8c-json`

Reports in `~/bisect/`:
- `iccdev-json-it8-srcType-report.txt`
- `iccdev-json-parser-regression-report.txt`

Latest pushed commits:
- `4ffcba5 fix: restore JSON config round-trips`
- `0eca71b fix: harden JSON parser regressions`

Regression gate:

```bash
cd ~/bisect/iccDEV-bisect-60bbb8c-json
cd Build && cmake --build . --target iccFromJson iccToJson \
  iccApplySearch iccApplyNamedCmm iccApplyProfiles -j"$(nproc)"
cd ..
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing \
  LD_LIBRARY_PATH=$PWD/Build/IccProfLib:$PWD/Build/IccXML:$PWD/Build/IccJSON \
  .github/scripts/iccdev-json-parser-regression-tests.sh
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing \
  LD_LIBRARY_PATH=$PWD/Build/IccProfLib:$PWD/Build/IccXML:$PWD/Build/IccJSON \
  .github/scripts/iccdev-json-cfg-tests.sh
```

Parser rule: fail closed. Reject short arrays, non-numeric required values,
failed nested `ParseJson()`, bad struct members, malformed fixed-size arrays, and
stale reset/fromJson state.

## Build and Test

```bash
# Standard Linux build
cd Build && cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build . -j"$(nproc)"

# Generate and validate profiles
Testing/CreateAllProfiles.sh
Testing/RunTests.sh
```

Sanitizer bug-hunt build:

```bash
cd Build
rm -f CMakeCache.txt
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer,float-divide-by-zero,float-cast-overflow" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build . -j"$(nproc)"
```

`-fsanitize=undefined` does not catch unsigned overflow or float divide-by-zero;
use `integer,float-divide-by-zero,float-cast-overflow` for bug hunting.

## Code Style and Safety

- 2-space C++ indent, K&R braces, no tabs.
- Member prefix `m_`; match nearby naming and ownership patterns.
- Error handling uses return values, not exceptions.
- New files need ICC copyright + BSD 3-Clause header unless generated.
- Validate all file-controlled sizes, offsets, counts, and loop bounds.
- Bounds check pattern: `if (size > limit || offset > limit - size)`.
- Exit 1-127 is graceful failure, not a crash; exit 128+ is signal/crash.
- Keep all generated agent files ASCII and verify with `file`.

## CI and Regression Rules

- Every workflow `run:` block starts with `set -euo pipefail`, clears git
  credentials, unsets `GITHUB_TOKEN`, and sources `sanitize-sed.sh` or fallback.
- Do not put `${{ }}` expressions directly in shell code; pass through `env:`.
- Sanitize all `GITHUB_STEP_SUMMARY` writes.
- Add regression coverage in the nearest script/workflow before claiming a fix.
- For JSON/config bugs, prefer `.github/scripts/iccdev-json-parser-regression-tests.sh`
  and `.github/scripts/iccdev-json-cfg-tests.sh`.

## Prompts

| Task | Prompt |
|------|--------|
| Bisect regression | `.github/prompts/bisect-regression.prompt.md` |
| Reproduce security issue | `.github/prompts/reproduce-security-issue.prompt.md` |
| File issue | `.github/prompts/file-security-issue.prompt.md` |
| Code review hunting | `.github/prompts/code-review-hunting.prompt.md` |
| Build/test/coverage | `.github/prompts/build-and-test.prompt.md` |
| Workflow governance audit | `.github/prompts/audit-workflow-governance.prompt.md` |
| vcpkg debug | `.github/prompts/vcpkg-port-debug.prompt.md` |
