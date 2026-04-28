# iccIisIsapi

`iccIisIsapi` is a Windows-only IIS ISAPI extension sample. It demonstrates
loading `IccProfLib2.dll`, `IccXML2.dll`, and optionally `IccJSON2.dll` from an
IIS-hosted native DLL.

## Request Modes

- `GET ?mode=health`: health probe, returns `ok`
- `GET ?format=xml`: minimal ICC XML payload
- `GET` with no query string: library/profile summary
- `POST ?mode=tools&input=icc|xml`: upload ICC/XML data, run wrapped tools, and
  return JSON

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
