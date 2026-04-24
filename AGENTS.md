# AGENTS.md -- iccDEV

## Project

RefIccMAX (iccDEV) -- ICC color profile libraries and tools.
Version 2.3.1.7, C++17, CMake 3.21+, BSD 3-Clause.

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
| IIS ISAPI DLL | `Tools/Winnt/IccIisIsapi/isapi-instructions.md` | IIS setup, deployment, security hardening |

## Prompts

| Task | Prompt |
|------|--------|
| Audit workflow governance | `.github/prompts/audit-workflow-governance.prompt.md` |
| Build, test, coverage | `.github/prompts/build-and-test.prompt.md` |
| Reproduce security issue | `.github/prompts/reproduce-security-issue.prompt.md` |
| Bisect and fix regressions | `.github/prompts/bisect-regression.prompt.md` |
| File a security issue | `.github/prompts/file-security-issue.prompt.md` |
| Code review bug hunting | `.github/prompts/code-review-hunting.prompt.md` |
| Add a new CLI tool | `.github/prompts/add-new-tool.prompt.md` |
| Debug WASM build | `.github/prompts/debug-wasm-build.prompt.md` |
| Cross-platform CI debug | `.github/prompts/cross-platform-ci.prompt.md` |
| New contributor onboarding | `.github/prompts/contributor-onboarding.prompt.md` |
| Version bump with ports | `.github/prompts/version-bump.prompt.md` |
| Debug vcpkg port CI | `.github/prompts/vcpkg-port-debug.prompt.md` |

## IIS ISAPI DLL (Windows)

The `Tools/Winnt/IccIisIsapi/` directory contains a Windows IIS ISAPI extension
that serves ICC profile tools over HTTP. Key components:

| Component | Path | Purpose |
|-----------|------|---------|
| ISAPI DLL | `Tools/Winnt/IccIisIsapi/iccIisIsapi.cpp` | Main entry point, tool orchestration |
| Sanitization | `Tools/Winnt/IccIisIsapi/IccIsapiSanitize.h/.cpp` | XSS prevention, URL decode, URI sanitize |
| HTTP layer | `Tools/Winnt/IccIisIsapi/IccIsapiHttp.h/.cpp` | Response helpers with 6 security headers |
| Client JS | `Tools/Winnt/IccIisIsapi/sanitize.js` | DOM-XSS prevention (mirrors server-side) |
| Fuzz test | `Tools/Winnt/IccIisIsapi/IccIsapiFuzzTest.cpp` | Tests sanitizer invariants against fuzz corpus |
| Stress test | `Tools/Winnt/IccIisIsapi/Stress-IccIisIsapi.ps1` | 10-phase concurrent load test |
| API docs | `Tools/Winnt/IccIisIsapi/api.md` | HTTP endpoint reference |
| OpenAPI | `Tools/Winnt/IccIisIsapi/iis-isapi.openapi.yaml` | Machine-readable API spec |

### HTTP Endpoints

- `GET /iccIisIsapi.dll`  --  version summary (JSON)
- `GET /iccIisIsapi.dll?mode=health`  --  health check
- `GET /iccIisIsapi.dll?format=xml`  --  minimal ICC XML
- `POST /iccIisIsapi.dll?mode=tools&input=icc|xml`  --  run tool suite, return JSON

### Security Hardening

- **CWE-79**: HtmlEscape all 5 entities, strip C0 control chars
- **CWE-116**: JsonEscape with solidus, SanitizeUri blocks dangerous schemes
- **CWE-170**: UrlDecode rejects null bytes (%00 and raw 0x00)
- **CWE-789**: Upload size checked before allocation (16 MB cap)
- **CWE-601**: SanitizeUri strips fragments, rejects javascript:/data:/vbscript:
- **6 security headers**: CSP, X-Content-Type-Options, X-Frame-Options, Referrer-Policy, Cache-Control, X-XSS-Protection

### Deployment

```powershell
# Build + install
cmake -S Build\Cmake -B out\build -G "Visual Studio 17 2022" -A x64 ...
cmake --build out\build --config Debug -- /m /maxcpucount

# Deploy to IIS
.\Tools\Winnt\IccIisIsapi\Install-IccIisIsapiSite.ps1 -SiteName "iccDLL Server" -Port 18081

# Package for deployment without build tools
.\Tools\Winnt\IccIisIsapi\Export-IccIisIsapiSite.ps1 -SourceRoot out\build\... -OutputZip pkg.zip
.\Tools\Winnt\IccIisIsapi\Import-IccIisIsapiSite.ps1 -PackageZip pkg.zip -SiteName "iccDLL Server"
```

### CRITICAL: Windows Build Flag

When overriding `CMAKE_CXX_FLAGS` on MSVC, always include `/DWIN32 /D_WINDOWS /EHsc`.
Without `WIN32` defined, `CIccFileIO::Open()` skips binary mode `'b'` in `fopen()`,
causing text-mode EOF (`0x1A`) in ICC binary data to truncate reads.

## Build

Compiler floor: GCC 11 / Clang 10 / MSVC 19.30 minimum; GCC 15 / Clang 14 / MSVC 19.40
recommended (matches CI and unlocks the strict warning tier).

```bash
# Standard (Clang)
cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++ && make -j$(nproc)

# Standard (GCC 15, matches ci-docker ubuntu:26.04)
cd Build && cmake Cmake -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15 && make -j$(nproc)

# Full sanitizer coverage (recommended for bug hunting)
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer,float-divide-by-zero,float-cast-overflow" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j$(nproc)
```

CRITICAL: `-fsanitize=undefined` does NOT catch unsigned integer overflow
(`-fsanitize=integer` required) or float division by zero
(`-fsanitize=float-divide-by-zero` required, IEEE 754 defines float/0 as NaN).
Always delete CMakeCache.txt before reconfigure.

## Test

```bash
Testing/CreateAllProfiles.sh   # Generate ~230 test profiles
Testing/RunTests.sh            # Validate all profiles
```

## CI Regression PoCs

Located in `.github/ci/regression/` with naming: `poc-{issue}-{component}-{type}.icc`

## Security

- Report via GitHub Security Advisory
- Validate ALL file-controlled sizes, offsets, loop bounds
- UIO fix pattern: `if (size > limit || offset > limit - size)`
- Reference: `.github/instructions/icc-library-code.instructions.md`
