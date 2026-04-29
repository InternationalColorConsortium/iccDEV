# Debug vcpkg Port CI

Use this prompt when the `ci-vcpkg-ports.yml` workflow fails.

## Quick Triage

### 1. Identify Which Platform Failed

The workflow runs on 3 platforms. Check which job(s) failed:
- `windows-latest` (x64-windows, MSVC + Ninja)
- `ubuntu-24.04` (x64-linux, GCC + Ninja)
- `macos-14` (arm64-osx, Apple Clang + Ninja)

### 2. Common Failure Categories

#### Optional Component Regressions
```
Could NOT find TIFF / PNG / JPEG / LibXml2
```
**Cause**: A component option was enabled without its dependency, or the
vcpkg feature mapping no longer matches the upstream CMake option.
**Fix**: Check `ENABLE_ICCXML`, `ENABLE_ICCJSON`, `ENABLE_TOOLS`,
`ENABLE_IMAGE_TOOLS`, `ENABLE_CMM_TOOLS`, and `ENABLE_IIS_TOOLS` in the
configure log.

#### GitHub API 403 (non-local mode)
```
error: curl operation failed with response code 403
```
**Cause**: `vcpkg_from_github` hits GitHub API rate limits when resolving
`--head` refs. This was the reason local source mode was created.
**Fix**: Ensure `VCPKG_ICCDEV_SOURCE` and `VCPKG_KEEP_ENV_VARS` are set
in the workflow step's `env:` block.

#### Missing Transitive Dependencies
```
undefined reference to `deflate` / `lzma_code` / `jpeg_read_header`
```
**Cause**: A library (e.g., libtiff) was re-enabled but its transitive
static dependencies aren't linked.
**Fix**: Either add the missing deps to `vcpkg.json` or disable the
feature that requires them.

#### Windows LNK1104 (cannot open .lib)
```
LINK : fatal error LNK1104: cannot open file 'IccProfLib2d.lib'
```
**Cause**: Shared DLL build attempted on Windows — no `__declspec(dllexport)`.
**Fix**: Ensure `ENABLE_SHARED_LIBS=OFF` and `ENABLE_STATIC_LIBS=ON` in
the portfile's `vcpkg_cmake_configure` call.

#### PCH / Precompiled Header Errors
```
error: ... precompiled header ...
```
**Cause**: IccDEVCmm uses PCH that fails under Ninja generator.
**Fix**: Ensure `ENABLE_CMM_TOOLS=OFF` in the portfile configure options.

### 3. Reproduce Locally

```bash
# Set local source mode
export VCPKG_ICCDEV_SOURCE=/path/to/iccDEV
export VCPKG_KEEP_ENV_VARS=VCPKG_ICCDEV_SOURCE

# Bootstrap vcpkg
git clone --depth 1 https://github.com/microsoft/vcpkg.git /tmp/vcpkg
/tmp/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Install with verbose output
/tmp/vcpkg/vcpkg install iccdev \
  --overlay-ports=/path/to/iccDEV/ports \
  --triplet x64-linux \
  --classic \
  2>&1 | tee vcpkg-install.log
```

Windows equivalent:
```powershell
$env:VCPKG_ICCDEV_SOURCE = "C:\path\to\iccDEV"
$env:VCPKG_KEEP_ENV_VARS = "VCPKG_ICCDEV_SOURCE"

git clone --depth 1 https://github.com/microsoft/vcpkg.git "$env:TEMP\vcpkg"
& "$env:TEMP\vcpkg\bootstrap-vcpkg.bat" -disableMetrics

& "$env:TEMP\vcpkg\vcpkg.exe" install iccdev `
  --overlay-ports="C:\path\to\iccDEV\ports" `
  --triplet x64-windows `
  --classic
```

### 4. Verify Installed Artifacts

After a successful install, check:
```bash
TRIPLET=x64-linux  # adjust for platform
PREFIX=/tmp/vcpkg/installed/$TRIPLET

# Headers
ls $PREFIX/include/RefIccMAX/IccProfLib2/*.h | wc -l  # expect ~52

# Libraries
ls $PREFIX/lib/libIccProfLib2*   # expect libIccProfLib2-static.a
ls $PREFIX/lib/libIccXML2*       # present only with the xml feature
ls $PREFIX/lib/libIccJSON2*      # present only with the json feature

# CMake config
cat $PREFIX/share/RefIccMAX/RefIccMAXConfig.cmake

# Tools
$PREFIX/tools/iccdev/iccDumpProfile --help
$PREFIX/tools/iccdev/iccRoundTrip --help
```

## Key Files

| File | Purpose |
|------|---------|
| `ports/iccdev/portfile.cmake` | Static-only vcpkg build script |
| `ports/iccdev/vcpkg.json` | Port manifest (deps, features) |
| `.github/workflows/ci-vcpkg-ports.yml` | Cross-platform CI workflow |
| `Build/Cmake/CMakeLists.txt` | Upstream CMake options used by the port |

## Environment Variable Gotcha

vcpkg **strips all non-standard environment variables** before running
portfile CMake. To pass custom variables through:

1. Set `VCPKG_KEEP_ENV_VARS=VAR_NAME` as an environment variable
2. Set `VAR_NAME=value` as an environment variable
3. In the portfile, access via `$ENV{VAR_NAME}`

Both must be set in the same workflow step's `env:` block.
