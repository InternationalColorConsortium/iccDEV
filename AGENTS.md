# AGENTS.md -- iccDEV

## Project

RefIccMAX (iccDEV) -- ICC color profile libraries and tools.
Version 2.3.1.6, C++17, CMake 3.21+, BSD 3-Clause.

## Documentation Index

| Document | Path | Purpose |
|----------|------|---------|
| Copilot instructions | `.github/copilot-instructions.md` | Build, test, style, CI overview |
| Build system | `.github/instructions/build-system.instructions.md` | CMake options, platform notes |
| Library code | `.github/instructions/icc-library-code.instructions.md` | Input validation, sanitizer rules, UIO fix pattern |
| Testing | `.github/instructions/testing.instructions.md` | Test scripts, regression PoCs, profile dirs |
| ICC spec | `.github/instructions/icc-specification.instructions.md` | Header, tags, color spaces, version encoding |
| Workflow governance | `.github/instructions/workflow-governance.instructions.md` | Shell hardening, sanitizer, injection prevention |
| Sanitizer scripts | `.github/instructions/sanitizer-scripts.instructions.md` | sanitize-sed.sh and sanitize.ps1 API |
| vcpkg port | `.github/instructions/vcpkg-port.instructions.md` | Portfile, features, local source mode |

## Prompts

| Task | Prompt |
|------|--------|
| Audit workflow governance | `.github/prompts/audit-workflow-governance.prompt.md` |
| Build, test, coverage | `.github/prompts/build-and-test.prompt.md` |
| Reproduce security issue | `.github/prompts/reproduce-security-issue.prompt.md` |
| Bisect and fix regressions | `.github/prompts/bisect-regression.prompt.md` |
| File a security issue | `.github/prompts/file-security-issue.prompt.md` |
| Add a new CLI tool | `.github/prompts/add-new-tool.prompt.md` |
| Debug WASM build | `.github/prompts/debug-wasm-build.prompt.md` |
| Cross-platform CI debug | `.github/prompts/cross-platform-ci.prompt.md` |
| New contributor onboarding | `.github/prompts/contributor-onboarding.prompt.md` |
| Version bump with ports | `.github/prompts/version-bump.prompt.md` |
| Debug vcpkg port CI | `.github/prompts/vcpkg-port-debug.prompt.md` |

## Build

```bash
# Standard
cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++ && make -j$(nproc)

# Full sanitizer coverage (recommended for bug hunting)
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j$(nproc)
```

CRITICAL: `-fsanitize=undefined` does NOT catch unsigned integer overflow.
`-fsanitize=integer` is required. Always delete CMakeCache.txt before reconfigure.

## Test

```bash
Testing/CreateAllProfiles.sh   # Generate ~210 test profiles
Testing/RunTests.sh            # Validate all profiles
```

## CI Regression PoCs

Located in `.github/ci/regression/` with naming: `poc-{issue}-{component}-{type}.icc`

## Security

- Report via GitHub Security Advisory
- Validate ALL file-controlled sizes, offsets, loop bounds
- UIO fix pattern: `if (size > limit || offset > limit - size)`
- Reference: `.github/instructions/icc-library-code.instructions.md`
