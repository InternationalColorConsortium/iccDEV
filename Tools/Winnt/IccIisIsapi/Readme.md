# iccIisIsapi

`iccIisIsapi` is a Windows-only IIS ISAPI extension sample that proves
`IccProfLib2.dll` and `IccXML2.dll` can be loaded from an IIS-hosted native DLL.
When IccJSON is built (`ENABLE_ICCJSON=ON`), the extension also loads
`IccJSON2.dll` and runs `iccToJson`/`iccFromJson` as part of the tool pipeline.

## Request modes

- `?mode=health` returns `ok`
- `?format=xml` returns a minimal ICC XML payload
- no query string returns library and profile-version details
- `POST ?mode=tools&input=icc|xml` runs the wrapped tool suite and returns JSON

## Documentation

- API reference: [`api.md`](api.md)
- machine-readable starter spec: [`iis-isapi.openapi.yaml`](iis-isapi.openapi.yaml)

## Build

```powershell
if (-not $env:VCPKG_ROOT) {
  throw "Set VCPKG_ROOT to your vcpkg checkout first."
}

$prefix = Join-Path (Resolve-Path .) 'out\iis-shared-install'
$toolchain = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'

cmake -S Build\Cmake -B out\iis-shared-vcpkg-toolchain `
  -G "Visual Studio 17 2022" `
  -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  "-DCMAKE_INSTALL_PREFIX=$prefix" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DENABLE_TESTS=OFF `
  -DENABLE_TOOLS=ON `
  -DENABLE_WXWIDGETS=OFF `
  -DENABLE_SHARED_LIBS=ON `
  -DENABLE_STATIC_LIBS=ON `
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF `
  -Wno-dev

cmake --build out\iis-shared-vcpkg-toolchain --config Release `
  --target iccIisIsapi iccIisIsapiSmoke -- /m /maxcpucount
cmake --install out\iis-shared-vcpkg-toolchain --config Release
```

## Deployment notes

- Map `iccIisIsapi.dll` as an IIS ISAPI extension/handler.
- Ensure `IccProfLib2.dll`, `IccXML2.dll`, and their transitive runtime DLLs
  are resolvable by the IIS worker process.
- Use an x64 app pool for the x64 build.
- The IIS sample target now copies runtime DLL dependencies next to the build
  artifact and into the install `bin` directory on Windows.
- `Tools\Winnt\IccIisIsapi\Install-IccIisIsapiSite.ps1` can create or update
  an IIS site against the install bundle automatically.

## Local proof

`iccIisIsapiSmoke.exe` loads the extension with `LoadLibrary`, calls
`GetExtensionVersion`, and runs a mock `HttpExtensionProc` request without
requiring IIS to be installed.

```powershell
$prefix = (Resolve-Path '.\out\iis-shared-install').Path
$oldPath = $env:PATH
try {
  $env:PATH = "$prefix\bin;$oldPath"
  & "$prefix\bin\iccIisIsapiSmoke.exe" "$prefix\bin\iccIisIsapi.dll"
}
finally {
  $env:PATH = $oldPath
}
```

## IIS setup

```powershell
.\Tools\Winnt\IccIisIsapi\Install-IccIisIsapiSite.ps1 `
  -SiteName "iccDLL Server" `
  -Port 18081
```

## IIS export / import (deploy without rebuilding)

```powershell
# Export a ready-to-deploy package from a build tree or install prefix
.\Tools\Winnt\IccIisIsapi\Export-IccIisIsapiSite.ps1 `
  -SourceRoot out\debug\Tools\IccIisIsapi\Debug `
  -OutputZip  iccDLL-Server-package.zip

# Import on a target machine (no build tools required)
.\Tools\Winnt\IccIisIsapi\Import-IccIisIsapiSite.ps1 `
  -PackageZip iccDLL-Server-package.zip `
  -SiteName   "iccDLL Server" `
  -Port       18081
```
