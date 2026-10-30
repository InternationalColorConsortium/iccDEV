# Debug vcpkg Port CI

Use this prompt when the `ci-vcpkg-ports.yml` workflow fails.

## Quick Triage

### 0. Verify the Registry Baseline Set

For a registry baseline update, keep `vcpkg.json`, `Build/Cmake/vcpkg.json`,
`examples/hello-iccdev/vcpkg-configuration.json`,
`.github/workflows/ci-vcpkg-ports.yml`, and `.github/workflows/ci.yml` at the
same commit. The overlay port manifest intentionally has no registry baseline.

### 1. Identify the Windows Failure

The vcpkg workflows run on `windows-2022` with the `x64-windows` triplet.
The overlay port uses MSVC and Ninja; the manifest consumer uses the Visual
Studio generator. Do not route vcpkg installation through Linux or macOS.

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

#### Source Download or Hash Failures (non-local mode)
```
error: curl operation failed with response code 403
error: SHA512 mismatch
```
**Cause**: `vcpkg_from_github` is using the pinned commit/hash path. API 403
usually means local source mode was not enabled in CI; SHA512 mismatch means
`REF` and `SHA512` were not refreshed together.
**Fix**: In CI, set both `VCPKG_ICCDEV_SOURCE` and `VCPKG_KEEP_ENV_VARS` in the
same step. For external consumers, update `REF`, `SHA512`, and `port-version`.

#### Missing Transitive Dependencies
```
undefined reference to `deflate` / `lzma_code` / `jpeg_read_header`
```
**Cause**: A library (e.g., libtiff) was re-enabled but its transitive
static dependencies aren't linked.
**Fix**: Either add the missing deps to `vcpkg.json` or disable the
feature that requires them.

#### Missing zlib for Compressed Tags
```
Could NOT find ZLIB
```
**Cause**: `ICC_USE_ZLIB=ON` is active, but zlib is missing from one of the
first-party manifests or from the overlay port dependency list.
**Fix**: Keep `zlib` in the root `vcpkg.json`, `Build/Cmake/vcpkg.json`, and
`ports/iccdev/vcpkg.json`; keep `-DICC_USE_ZLIB=ON` explicit in
`ports/iccdev/portfile.cmake`; increment the overlay `port-version`.

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

### 3. Reproduce Locally on Windows

```powershell
$iccdevRoot = 'C:\path\to\iccDEV'
$vcpkgRoot = Join-Path $env:TEMP 'iccdev-vcpkg-qa'
$env:VCPKG_ICCDEV_SOURCE = $iccdevRoot
$env:VCPKG_KEEP_ENV_VARS = "VCPKG_ICCDEV_SOURCE"

git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
git -C $vcpkgRoot checkout 04a9d8e5212d01ee1dd9478eadd9caade4f8b0d4
& "$vcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics

& "$vcpkgRoot\vcpkg.exe" install 'iccdev[tools,xml,json]' `
  "--overlay-ports=$iccdevRoot\ports" `
  --triplet x64-windows `
  --classic
if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed: $LASTEXITCODE" }
```

For a source-pin refresh, repeat the install in a separate installed tree
with `VCPKG_ICCDEV_SOURCE` unset and binary cache restoration disabled for
`iccdev`, so vcpkg downloads and verifies the pinned archive. An already
installed package is reused even after the source environment variable is
unset; use a fresh tree or remove the installed `iccdev` package first.
Follow the Port Pin Check in
<a href="../skills/vcpkg-export-consumer-debug/SKILL.md">vcpkg Export Consumer Debug</a>;
local-source mode alone does not validate `REF` or `SHA512`.

### 4. Verify Installed Artifacts and Usage

After a successful install, check the static libraries, CMake package, CLI
tools, and the installed `usage` text:

```powershell
$prefix = Join-Path $vcpkgRoot 'installed\x64-windows'
Get-ChildItem "$prefix\include\RefIccMAX\IccProfLib2\*.h" | Measure-Object
@('IccProfLib2-static.lib', 'IccXML2-static.lib',
  'IccJSON2-static.lib', 'IccConnect2-static.lib') | ForEach-Object {
  if (-not (Test-Path "$prefix\lib\$_")) { throw "Missing library: $_" }
}
if (-not (Test-Path "$prefix\share\reficcmax\RefIccMAXConfig.cmake")) {
  throw 'Missing RefIccMAX CMake package'
}
Get-Content "$prefix\share\iccdev\usage"
@('iccDumpProfile', 'iccRoundTrip', 'iccFromXml', 'iccToXml',
  'iccFromJson', 'iccToJson', 'iccApplyNamedCmm', 'iccBenchApply') | ForEach-Object {
  if (-not (Test-Path "$prefix\tools\iccdev\$_.exe")) { throw "Missing tool: $_" }
}
```

Build `examples/hello-iccdev` in manifest mode against the overlay and run
the resulting executable. The overlay is static-only and does not install
`IccDEVCmm.dll`; validate the CMM DLL through the Windows CMake runtime
package and its separate host smoke test.

## Key Files

| File | Purpose |
|------|---------|
| `vcpkg.json` | Repository-root manifest used by Windows and manifest-mode builds |
| `Build/Cmake/vcpkg.json` | CMake-source manifest used when configuring from `Build/Cmake` |
| `ports/iccdev/portfile.cmake` | Static-only vcpkg build script |
| `ports/iccdev/vcpkg.json` | Port manifest (deps, features) |
| `examples/hello-iccdev/vcpkg.json` | Consumer manifest; relies on `iccdev` transitive dependencies |
| `.github/workflows/ci-vcpkg-ports.yml` | Windows overlay and manifest consumer CI workflow |
| `.github/workflows/vcpkg-packages.yml` | Manual Windows dependency and CPack bundle workflow |
| `Build/Cmake/CMakeLists.txt` | Upstream CMake options used by the port |

## CI Policy

The vcpkg workflows validate `x64-windows` only. The manual `Windows vcpkg
packages` workflow preserves the portable vcpkg layout in its artifact:
`vcpkg/scripts/buildsystems/vcpkg.cmake` and
`vcpkg/installed/x64-windows/`.

## Environment Variable Gotcha

vcpkg **strips all non-standard environment variables** before running
portfile CMake. To pass custom variables through:

1. Set `VCPKG_KEEP_ENV_VARS=VAR_NAME` as an environment variable
2. Set `VAR_NAME=value` as an environment variable
3. In the portfile, access via `$ENV{VAR_NAME}`

Both must be set in the same workflow step's `env:` block.
