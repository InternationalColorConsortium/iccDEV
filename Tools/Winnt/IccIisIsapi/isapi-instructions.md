# IIS `iccIisIsapi.dll` Instructions

This repository includes a Windows IIS ISAPI sample that proves `iccIisIsapi.dll`
can load `IccProfLib2.dll` and `IccXML2.dll` inside an IIS worker process.

## What to request

After the handler is mapped, the extension responds to four request forms:

- `GET /iccIisIsapi.dll?mode=health`
  - returns plain text `ok`
- `GET /iccIisIsapi.dll`
  - returns plain text with:
    - `IccProfLib` version
    - `IccLibXML` version
    - profile spec version
    - generated XML byte count
- `GET /iccIisIsapi.dll?format=xml`
  - returns a minimal ICC XML payload with `Content-Type: application/xml`
- `POST /iccIisIsapi.dll?mode=tools&input=icc` or `POST /iccIisIsapi.dll?mode=tools&input=xml`
  - runs `iccToXml`, `iccFromXml`, `iccDumpProfile`, and `iccRoundTrip`
  - returns JSON with exit codes, console output, generated artifact previews, and direct HTTP links for the uploaded input, logs, and generated files

Example local URLs from the current IIS sample site:

```text
http://localhost:18081/
http://localhost:18081/endpoints.html
http://localhost:18081/iccIisIsapi.dll
http://localhost:18081/iccIisIsapi.dll?mode=health
http://localhost:18081/iccIisIsapi.dll?format=xml
```

## Why `HTTP Error 403.14 - Forbidden` happens

`403.14` at the site root means IIS is serving a directory path, directory
browsing is disabled, and there is no default document to open.

For this sample, the fix is not to enable directory browsing. The better fix is:

1. Keep the site root pointed at the deployed `bin` folder.
2. Put an `index.html` file in that folder.
3. Put a `web.config` file in that folder that enables `index.html` as the
   default document.
4. Keep the ISAPI handler mapped to `iccIisIsapi.dll`.

This repository now installs `index.html` and `web.config` next to
`iccIisIsapi.dll`, so the root URL serves a landing page instead of the IIS
directory-listing error. Per-request uploads and generated files are written
under `_tool-work` and left accessible over HTTP so they can be fetched back
directly. The same `web.config` adds `.icc` and `.icm` MIME mappings so raw ICC
profiles download as `application/octet-stream` instead of failing with
`404.3 - Not Found`.

## Files deployed with the IIS sample

The sample install directory should contain at least:

- `iccIisIsapi.dll`
- `IccProfLib2.dll`
- `IccXML2.dll`
- `iconv-2.dll`
- `libxml2.dll`
- `zlib1.dll`
- `index.html`
- `web.config`

If the transitive runtime DLLs are missing, the IIS worker process can fail to
load the extension even when the handler mapping is correct.

## IIS setup checklist

1. Enable the IIS ISAPI extension feature.

```powershell
dism /online /Enable-Feature /FeatureName:IIS-ISAPIExtensions /All /NoRestart
```

2. Build and install the sample.

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

3. Point an IIS site at the installed `bin` directory.

```text
<repo>\out\iis-shared-install\bin
```

4. Use an x64 app pool if the sample was built for x64.

5. Create or update the IIS site with the helper script in the sample source
   directory. The script configures the app pool, site root, handler mapping,
   ISAPI restriction entry, and verification probes automatically.

```powershell
.\Tools\Winnt\IccIisIsapi\Install-IccIisIsapiSite.ps1 `
  -SiteName "Codex-iccIisIsapiInstall" `
  -Port 18081
```

The script defaults `-SiteRoot` to `<repo>\out\iis-shared-install\bin`. Use
`-SiteRoot` to point IIS at a different install bundle, or `-Layout Build` with
`-Configuration Release` for a build-tree proof.

## Smoke and browser checks

Direct endpoint checks:

```powershell
curl.exe http://localhost:18081/iccIisIsapi.dll?mode=health
curl.exe http://localhost:18081/iccIisIsapi.dll
curl.exe http://localhost:18081/iccIisIsapi.dll?format=xml
curl.exe -H "Accept: application/json" -H "Content-Type: application/octet-stream" `
  --data-binary "@sample.icc" `
  "http://localhost:18081/iccIisIsapi.dll?mode=tools&input=icc&filename=sample.icc"
```

Browser checks:

1. Open `http://localhost:18081/`
2. Confirm the landing page loads instead of `HTTP Error 403.14 - Forbidden`
3. Open `http://localhost:18081/endpoints.html` and upload an ICC profile or XML file
4. Open the returned `_tool-work/<job>/` workspace link to fetch the original upload, logs, and generated artifacts directly over HTTP

## Build-tree quick proof

The build target also copies `index.html` and `web.config` next to the built
DLL in the build output directory, so you can point IIS at the build tree for a
fast local proof before installing.

```powershell
$buildOut = (Resolve-Path '.\out\iis-shared-vcpkg-toolchain\Tools\IccIisIsapi\Release').Path
$oldPath = $env:PATH
try {
  $env:PATH = "$buildOut;$oldPath"
  & "$buildOut\iccIisIsapiSmoke.exe" "$buildOut\iccIisIsapi.dll"
}
finally {
  $env:PATH = $oldPath
}
```

## Relevant source files

- `Build/Cmake/Tools/IccIisIsapi/CMakeLists.txt`
- `Tools/Winnt/IccIisIsapi/iccIisIsapi.cpp`
- `Tools/Winnt/IccIisIsapi/index.html`
- `Tools/Winnt/IccIisIsapi/web.config`
