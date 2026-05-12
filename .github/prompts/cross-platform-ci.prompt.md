# Cross-Platform CI Debugging

Use this prompt when a CI workflow fails on one platform but passes on others.

## Quick Triage

### 1. Identify the Failing Step

```bash
# List recent runs with status
gh run list --repo InternationalColorConsortium/iccDEV --limit 10

# Get failed job logs
gh run view <RUN_ID> --repo InternationalColorConsortium/iccDEV --log-failed
```

### 2. Platform Matrix

| Workflow | Platform | Compiler | Key Differences |
|----------|----------|----------|-----------------|
| `ci-pr-action.yml` | ubuntu-24.04 | clang | Gold-standard bash governance |
| `ci-pr-unix.yml` | ubuntu-24.04 | gcc + clang | Matrix: {gcc, clang} x {Debug, Release} |
| `ci-pr-win.yml` | windows-latest | MSVC | vcpkg deps, PowerShell governance |
| `ci-sanitizer-tests.yml` | ubuntu-24.04 | clang | ASan, UBSan, TSan, MSan matrix |
| `ci-pr-wasm.yml` | ubuntu-24.04 | emcc | Release WASM PR parity gate |
| `ci-docker.yml` | ubuntu-24.04 | gcc | Docker build + test (Ubuntu + NixOS matrix) |
| `ci-vcpkg-ports.yml` | win/ubuntu/macos | varies | vcpkg overlay port install + verify |

### 3. Common Failure Categories

#### Compiler-Specific
```
# GCC vs Clang differences:
- GCC: stricter -Wmaybe-uninitialized (false positives in GCC, not Clang)
- Clang: stricter -Wunused-private-field
- MSVC: /W4 warnings differ significantly from -Wall -Wextra
- emcc: no POSIX signals, no threads, no filesystem access (virtual FS only)

# Fix: use compiler-specific pragmas or CMake generator expressions:
target_compile_options(target PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
)
```

#### Sanitizer-Specific
```
# ASan: shadow memory requires 20TB virtual address space
#   - Docker/QEMU may not support this
#   - Use halt_on_error=0 for catch-and-continue
# TSan: conflicts with ASan (CMake enforces this)
# MSan: requires ALL linked libraries to be MSan-instrumented
#   - System libxml2/libtiff are NOT instrumented -> false positives
#   - May need to build deps from source with MSan
# UBSan: most portable, fewest false positives
```

#### WASM-Specific
```
# Emscripten quirks:
- No POSIX signals (alarm, sigsetjmp, sigaltstack)
- No shared libraries (-DENABLE_SHARED_LIBS=OFF required)
- No pkg-config -> must build libxml2 with CMake
- emsdk git tags != Emscripten SDK versions
- ASan and SAFE_HEAP are mutually exclusive
- -sINVOKE_RUN=0 is MANDATORY with -sMODULARIZE=1
```

#### Windows-Specific
```
# MSVC differences:
- vcpkg handles all dependencies (manifest mode)
- No LD_LIBRARY_PATH -> DLLs must be in PATH or same directory
- Windows CTest wrappers add build-tree, vcpkg, compiler, and MinGW runtime
  directories from CMakeCache; raw executable runs still need an explicit PATH
- PowerShell sanitizer: .github/scripts/sanitize.ps1
- Line endings: CRLF in bat scripts, LF in sh scripts
- Path separators: backslash in build output
```

#### vcpkg Port-Specific
```
# vcpkg overlay port issues:
- VCPKG_ICCDEV_SOURCE env var stripped by vcpkg sandbox - need VCPKG_KEEP_ENV_VARS
- Non-local port source uses pinned REF + SHA512; refresh both with port-version
- VS-bundled vcpkg doesn't support --classic mode - use standalone clone
- Static-only build (no __declspec(dllexport)) - ENABLE_SHARED_LIBS=OFF
- IccDEVCmm PCH fails under Ninja - keep ENABLE_CMM_TOOLS=OFF in vcpkg
- Image tool dependencies stay out of vcpkg - keep ENABLE_IMAGE_TOOLS=OFF
```

#### Dependency Failures
```
# GitLab transient failures (libtiff, libxml2):
- Add retry logic: 3 attempts, 10s backoff
- Check: curl -sI https://gitlab.gnome.org | head -1

# Homebrew failures (macOS):
- brew update && brew install --overwrite <pkg>
- Check: brew doctor

# vcpkg failures (Windows):
- vcpkg update && vcpkg install
- Check: vcpkg list
```

## Reproduce Locally

### Ubuntu (matches ci-pr-action, ci-pr-unix, ci-sanitizer-tests)
```bash
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libxml2-dev \
  nlohmann-json3-dev cmake clang clang-tools build-essential
cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
make -j"$(nproc)"
cd ../Testing && ./CreateAllProfiles.sh && ./RunTests.sh
```

### Windows (matches ci-pr-win)
```powershell
vcpkg integrate install
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64
cmake --build out/vs2022-x64 --config Release --target check -- /m /maxcpucount

$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --preset mingw-core-x64 -S Build/Cmake -B out/mingw-core-x64
cmake --build out/mingw-core-x64 --target check --parallel
```

### WASM (matches ci-pr-wasm)
```bash
source /path/to/emsdk/emsdk_env.sh
# Build libxml2 for WASM first (see debug-wasm-build.prompt.md)
cd Build
emcmake cmake Cmake -DENABLE_TESTS=OFF -DENABLE_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Release
emmake make -j"$(nproc)"
```

## CI Log Patterns to Search

```bash
# Download and search logs
gh run view <RUN_ID> --repo InternationalColorConsortium/iccDEV --log > /tmp/ci.log

# Common error patterns
grep -n "error:" /tmp/ci.log | head -20          # Compiler errors
grep -n "FAILED" /tmp/ci.log | head -20           # CMake/CTest failures
grep -n "AddressSanitizer" /tmp/ci.log | head -5  # ASan findings
grep -n "runtime error:" /tmp/ci.log | head -5    # UBSan findings
grep -n "fatal:" /tmp/ci.log | head -5            # Git/linker fatal errors
grep -n "HTTP.*[45][0-9][0-9]" /tmp/ci.log        # Network failures
```
