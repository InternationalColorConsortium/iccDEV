# IIS iccIisIsapi.dll Instructions

This repository includes a Windows IIS ISAPI sample that proves `iccIisIsapi.dll`
can load `IccProfLib2.dll` and `IccXML2.dll` inside an IIS worker process.

The browser console intentionally exposes only the bounded ICC/XML conversion
and validation pipeline documented below. It is not an HTTP wrapper for every
iccDEV CLI tool.

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
  - runs conversion, `iccDumpProfile`, `iccPawgReport`, and `iccRoundTrip`
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
directory-listing error. Per-request uploads and generated files are written under `_tool-work` behind
non-enumerated 128-bit capability URLs returned to the requesting client.
The same `web.config` adds `.icc` and `.icm` MIME mappings so raw ICC
profiles download as `application/octet-stream` instead of failing with
`404.3 - Not Found`.

## Files deployed with the IIS sample

The sample install directory should contain at least:

- `iccIisIsapi.dll`
- `IccProfLib2.dll` (or `IccProfLib2d.dll` for Debug)
- `IccXML2.dll` (or `IccXML2d.dll` for Debug)
- `iconv-2.dll`
- `libxml2.dll`
- `zlib1.dll` (or `zlibd1.dll` for Debug)
- `index.html`, `index2.html`, `endpoints.html`, `integration.html`
- `site.css`, `site.js`, `sanitize.js`
- `web.config`
- `assets/` -- ICC branding images
- `_tool-work/` -- runtime workspace directory; root browsing is denied
- Tool executables: `iccToXml.exe`, `iccFromXml.exe`, `iccDumpProfile.exe`, `iccRoundTrip.exe`
- Assessment executable: `iccPawgReport.exe`
- JSON tool executables (optional, if IccJSON is built): `iccToJson.exe`, `iccFromJson.exe`
- JSON support DLL (optional): `IccJSON2.dll` (or `IccJSON2d.dll` for Debug)

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
  --target install -- /m /maxcpucount
```

Building the `install` target is intentional: a later standalone
`cmake --install` does not build missing targets first and can fail when only
the IIS DLL and smoke executable were built.

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
  -SiteName "iccDLL Server" `
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
2. Confirm the site landing page loads and `/_tool-work/` returns 403 or 404
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
- `Tools/Winnt/IccIisIsapi/iccIisIsapi.cpp` -- ISAPI DLL entry point
- `Tools/Winnt/IccIisIsapi/IccIsapiSanitize.h/.cpp` -- Sanitization primitives (CWE-79, CWE-116, CWE-170, CWE-20)
- `Tools/Winnt/IccIisIsapi/IccIsapiHttp.h/.cpp` -- HTTP response helpers with security headers
- `Tools/Winnt/IccIisIsapi/sanitize.js` -- Client-side DOM-XSS prevention
- `Tools/Winnt/IccIisIsapi/IccIsapiFuzzTest.cpp` -- Fuzz test harness
- `Tools/Winnt/IccIisIsapi/Stress-IccIisIsapi.ps1` -- PowerShell 7+ 10-phase concurrent stress test
- `Tools/Winnt/IccIisIsapi/Test-IccIisIsapiEndpoints.ps1` -- live HTTP endpoint and PAWG report QA
- `Tools/Winnt/IccIisIsapi/Install-IccIisIsapiSite.ps1` -- IIS site installer
- `Tools/Winnt/IccIisIsapi/Uninstall-IccIisIsapiSite.ps1` -- IIS site uninstaller
- `Tools/Winnt/IccIisIsapi/Export-IccIisIsapiSite.ps1` -- Package site for deployment
- `Tools/Winnt/IccIisIsapi/Import-IccIisIsapiSite.ps1` -- Deploy package to IIS
- `Tools/Winnt/IccIisIsapi/index.html` -- Landing page
- `Tools/Winnt/IccIisIsapi/endpoints.html` -- Browser tool console
- `Tools/Winnt/IccIisIsapi/web.config` -- IIS configuration
- `Tools/Winnt/IccIisIsapi/api.md` -- HTTP API reference
- `Tools/Winnt/IccIisIsapi/iis-isapi.openapi.yaml` -- OpenAPI 3.1 spec

## Live endpoint QA

After installing the site, run the repository-owned endpoint test:

```powershell
.\Tools\Winnt\IccIisIsapi\Test-IccIisIsapiEndpoints.ps1 `
  -BaseUrl http://localhost:18081 `
  -SampleIccPath .\Testing\sRGB_v4_ICC_preference.icc
```

The test verifies the landing page security headers, including the absence of
`unsafe-inline` and the exact stylesheet hash allowlist, health, summary, XML
response, invalid `input` rejection, required request header, media-type
enforcement, cross-site rejection, forced upload extensions, ICC and XML PAWG
pipelines, DOCTYPE rejection, non-enumerated workspace identifiers, path-safe
artifact links, downloadable report content, and persisted workspace page.
Browser QA should also confirm that `endpoints.html` renders the report card,
download link, and no console errors.

The installer binds to `127.0.0.1` by default. Use `-BindAddress` only when a
reviewed deployment intentionally needs a non-loopback interface, and add TLS
before sending profiles over an untrusted network.

Run the concurrent stress test with PowerShell 7 or later:

```powershell
pwsh -File .\Tools\Winnt\IccIisIsapi\Stress-IccIisIsapi.ps1 `
  -BaseUrl http://localhost:18081 `
  -Concurrency 20 `
  -Duration 10
```

Package imports are validated in a clean staging directory before deployment
and fail closed when any documented runtime tool is absent, including
`iccPawgReport.exe`; a partially functional tool endpoint deployment is not
accepted.
