# iccIisIsapi

`iccIisIsapi` is a Windows-only IIS ISAPI extension sample. It demonstrates
loading `IccProfLib2.dll`, `IccXML2.dll`, and optionally `IccJSON2.dll` from an
IIS-hosted native DLL.

## Request Modes

- `GET ?mode=health`: health probe, returns `ok`
- `GET ?format=xml`: minimal ICC XML payload
- `GET ?format=json|fromjson`: documented structured response modes
- `GET` with no query string: service/profile summary without build metadata
- `POST ?mode=tools&input=icc|xml`: upload ICC/XML data, run wrapped tools, and
  return JSON

Tool POSTs require `X-ICCDEV-Request: 1` and an exact ICC/XML media type. The
installer binds to `127.0.0.1` by default so the sample is local-only unless an
operator deliberately changes the binding.

The upload endpoint intentionally wraps a bounded conversion, validation, and
assessment subset: `iccToXml`, `iccFromXml`, `iccDumpProfile`,
`iccPawgReport`, `iccRoundTrip`, and the JSON converters when available. The
browser presents `iccPawgReport` as a Profile Assessment Report and provides a
downloadable text report. Other iccDEV binaries need different input contracts
or are unsuitable for a request-time IIS pipeline; they remain command-line
tools rather than implicit HTTP endpoints.

Security boundaries include forced `.icc`/`.xml` upload extensions,
cryptographically random 128-bit workspace identifiers, a denied workspace
root, four concurrent analysis jobs, 24-hour/100-workspace retention, DOCTYPE
rejection, static-response security headers, and relative child-process
arguments that do not disclose server paths. Non-loopback deployments require
IIS authentication and authorization. IIS 403 and 404 responses use a generic
error page without physical paths or diagnostic details.

The runtime site publishes only the end-user pages and required static assets.
Deployment guides, scripts, OpenAPI files, and alternate operator pages remain
in the install documentation tree and are denied by the website.

## Documentation

- [IIS setup and deployment](isapi-instructions.md)
- [HTTP and native API reference](api.md)
- [OpenAPI starter spec](iis-isapi.openapi.yaml)

## Local Smoke Test

`iccIisIsapiSmoke.exe` loads the extension with `LoadLibrary`, calls
`GetExtensionVersion`, and runs a mock `HttpExtensionProc` request without
requiring IIS.

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
