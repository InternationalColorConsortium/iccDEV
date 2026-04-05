# Copilot Instructions for iccDEV

## Path-Specific Instructions (auto-loaded by file pattern)

| Pattern | File | Content |
|---------|------|---------|
| `.github/workflows/**` | `instructions/workflow-governance.instructions.md` | Shell hardening, sanitizer, injection prevention |
| `.github/scripts/**` | `instructions/sanitizer-scripts.instructions.md` | sanitize-sed.sh and sanitize.ps1 API reference |
| `Build/**` | `instructions/build-system.instructions.md` | CMake options, platform notes, WASM/LTO |
| `IccProfLib/**`, `IccXML/**`, `Tools/**` | `instructions/icc-library-code.instructions.md` | Input validation, sanitizer compat |
| `Testing/**` | `instructions/testing.instructions.md` | Test scripts, profile dirs, validation flow |
| `IccProfLib/icProfileHeader.h` | `instructions/icc-specification.instructions.md` | ICC header, tags, color spaces, version encoding |
| `ports/**` | `instructions/vcpkg-port.instructions.md` | vcpkg portfile, features, local source mode, CI |

## Reusable Prompts

| Task | Prompt |
|------|--------|
| Audit workflow governance | `prompts/audit-workflow-governance.prompt.md` |
| Debug WASM build failures | `prompts/debug-wasm-build.prompt.md` |
| Build, test, coverage | `prompts/build-and-test.prompt.md` |
| Reproduce security advisory | `prompts/reproduce-security-issue.prompt.md` |
| Add a new CLI tool | `prompts/add-new-tool.prompt.md` |
| Debug cross-platform CI | `prompts/cross-platform-ci.prompt.md` |
| New contributor onboarding | `prompts/contributor-onboarding.prompt.md` |
| File a security issue | `prompts/file-security-issue.prompt.md` |
| Version bump with ports | `prompts/version-bump.prompt.md` |
| Debug vcpkg port CI | `prompts/vcpkg-port-debug.prompt.md` |
| Bisect and fix regressions | `prompts/bisect-regression.prompt.md` |

## Project Overview

RefIccMAX (iccDEV) — ICC color profile libraries and tools.
**Version**: 2.3.1.6 · **C++17** · **CMake ≥ 3.21** · **BSD 3-Clause**

## Code Style

- **2-space indent**, no tabs, K&R braces
- Member prefix `m_` (e.g., `m_variableName`)
- Error handling via return values — **no exceptions**
- Match existing patterns; consistency over perfection
- All new files must include the ICC copyright + BSD 3-Clause header
- Aim for zero compiler warnings across all platforms

## Project Structure

| Directory | Purpose |
|-----------|---------|
| `IccProfLib/` | Core ICC profile library (C++) |
| `IccXML/` | XML serialization (libxml2) |
| `Tools/CmdLine/` | 13 CLI tools (DumpProfile, RoundTrip, ApplyProfiles, …) |
| `Build/Cmake/` | CMake build system |
| `Testing/` | Test scripts and profile generators |
| `ports/iccdev/` | vcpkg overlay port (portfile.cmake, vcpkg.json) |
| `.github/workflows/` | 18 CI workflows (PR, sanitizer, WASM, Docker, vcpkg, release) |
| `.github/scripts/` | `sanitize-sed.sh` (bash) and `sanitize.ps1` (PowerShell) |

## Build

### Ubuntu
```bash
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libxml2-dev \
  libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers \
  nlohmann-json3-dev cmake clang clang-tools build-essential
cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++ && make -j"$(nproc)"
```

### macOS
```bash
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg
cmake -G "Xcode" Build/Cmake && xcodebuild -project RefIccMAX.xcodeproj
```

### Windows (MSVC + vcpkg)
```cmd
vcpkg integrate install && vcpkg install
cmake --preset vs2022-x64 -B . -S Build/Cmake
cmake --build . -- /m /maxcpucount
```

### CMake Options

| Option | Default | Purpose |
|--------|---------|---------|
| `ENABLE_TESTS` | ON | Build test targets (requires static libs) |
| `ENABLE_TOOLS` | ON | Build CLI + GUI tools |
| `ENABLE_SANITIZERS` | OFF | ASan + UBSan + IntegerSan combined |
| `ENABLE_ASAN` | OFF | AddressSanitizer only |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer only |
| `ENABLE_INTEGER_SANITIZER` | OFF | IntegerSanitizer (unsigned overflow, Clang only) |
| `ENABLE_TSAN` | OFF | ThreadSanitizer (conflicts with ASan) |
| `ENABLE_MSAN` | OFF | MemorySanitizer (Clang only) |
| `ENABLE_LSAN` | OFF | LeakSanitizer standalone |
| `ENABLE_COVERAGE` | OFF | LLVM source-based coverage |
| `ENABLE_FUZZING` | OFF | LibFuzzer harnesses (Clang only) |
| `ENABLE_ICCXML` | ON | IccXML library support |

## vcpkg Port

A vcpkg overlay port is provided in `ports/iccdev/` for consuming iccDEV as a
static library dependency. See `docs/build.md` for full usage instructions.

### Quick Install
```bash
vcpkg install iccdev --overlay-ports=ports --triplet x64-linux --classic
```

### CMake Integration
```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)
```

### Port Features
- `tools` (default): 7 core CLI tools + 2 XML tools
- `xml` (default): IccXML2 library (adds libxml2 dependency)

### Local Source Mode (CI)
Set `VCPKG_ICCDEV_SOURCE` + `VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE` to build
from a local checkout instead of downloading from GitHub. Required in CI to
avoid GitHub API rate limits.

### Known Limitations
- Static-only (no `__declspec(dllexport)` in IccProfLib/IccXML)
- Image tools excluded (TIFF/PNG/JPEG deps cause static link issues)
- wxProfileDump GUI excluded

CI workflow: `ci-vcpkg-ports.yml` validates the port on Windows, Ubuntu, macOS.

## Testing

```bash
Testing/CreateAllProfiles.sh   # Generate ~80 test profiles
Testing/RunTests.sh            # Validate all profiles (Unix)
Testing/RunTests.bat           # Validate all profiles (Windows)
```

## CI Workflows (21)

| Category | Workflows |
|----------|-----------|
| PR gates | `ci-pr-action.yml`, `ci-pr-lint.yml`, `ci-pr-unix.yml`, `ci-pr-unix-sb.yml`, `ci-pr-win.yml` |
| Build/test | `ci-comprehensive-build-test.yml`, `ci-sanitizer-tests.yml`, `ci-code-coverage.yml` |
| WASM | `ci-wasm-build-test.yml`, `wasm-latest-matrix.yml` |
| Docker | `ci-docker-latest.yml`, `ci-docker-nixos.yml` |
| Release | `ci-latest-release.yml` |
| Analysis | `ci-pr-risk-security-analysis.yml` |
| Shared | `ci-shared-exports.yml` |
| vcpkg | `ci-vcpkg-ports.yml` (Windows, Ubuntu, macOS overlay port test) |
| Ops | `label.yml`, `update-labels.yml` |

### Workflow Governance

All `run:` steps MUST follow the shell-hardening prologue per
[xsscx/governance](https://github.com/xsscx/governance/tree/main/actions):

**Bash steps** — every `run:` block:
```yaml
shell: bash --noprofile --norc {0}
env:
  BASH_ENV: /dev/null
run: |
  set -euo pipefail
  git config --global credential.helper ""
  unset GITHUB_TOKEN || true
  source .github/scripts/sanitize-sed.sh  # or inline fallback
```

**PowerShell steps** — every `run:` block:
```yaml
shell: pwsh -NoProfile -NoLogo -NonInteractive -Command {0}
env:
  POWERSHELL_TELEMETRY_OPTOUT: 1
  POWERSHELL_UPDATECHECK: Off
run: |
  $ErrorActionPreference = 'Stop'
  git config --global credential.helper ""
  if (Test-Path env:GITHUB_TOKEN) { Remove-Item env:GITHUB_TOKEN }
  . .github/scripts/sanitize.ps1
```

**Mandatory**: All `GITHUB_STEP_SUMMARY` writes must use `sanitize_line`/`Sanitize-Line`.
Never place `${{ matrix.* }}` or other expressions directly in `run:` blocks — pass
through `env:` to prevent shell injection.

## Security

- Report vulnerabilities via [GitHub Security Advisory](https://github.com/InternationalColorConsortium/iccDEV/security/advisories)
- Validate all inputs — especially file-controlled sizes, offsets, and loop bounds
- Two sanitizer scripts for CI output: `sanitize-sed.sh` (bash V3) and `sanitize.ps1` (PowerShell V1)
- Reference workflow: `ci-pr-action.yml` (bash) · `ci-pr-win.yml` (PowerShell)

## Pull Request Process

1. Branch from `master`: `feature/<name>` or `bugfix/<name>`
2. Ensure builds + tests pass on all platforms
3. Follow existing code style
4. Clear PR description; address review feedback
5. Requires Committer approval; CLA must be signed