# iccIisIsapi

`iccIisIsapi` is a Windows-only IIS ISAPI extension sample that proves
`IccProfLib2.dll` and `IccXML2.dll` can be loaded from an IIS-hosted native DLL.

## Request modes

- `?mode=health` returns `ok`
- `?format=xml` returns a minimal ICC XML payload
- no query string returns library and profile-version details
- `POST ?mode=tools&input=icc|xml` runs the wrapped tool suite and returns JSON

## Documentation

- repo-level API reference: [`docs/api.md`](../../../docs/api.md)
- machine-readable starter spec: [`docs/iis-isapi.openapi.yaml`](../../../docs/iis-isapi.openapi.yaml)

## Build

```powershell
cmake -B build -S . `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DENABLE_TOOLS=ON `
  -DENABLE_SHARED_LIBS=ON `
  -DENABLE_STATIC_LIBS=ON
cmake --build build --config Release --target iccIisIsapi iccIisIsapiSmoke
```

## Deployment notes

- Map `iccIisIsapi.dll` as an IIS ISAPI extension/handler.
- Ensure `IccProfLib2.dll`, `IccXML2.dll`, and their transitive runtime DLLs
  are resolvable by the IIS worker process.
- Use an x64 app pool for the x64 build.
- The IIS sample target now copies runtime DLL dependencies next to the build
  artifact and into the install `bin` directory on Windows.

## Local proof

`iccIisIsapiSmoke.exe` loads the extension with `LoadLibrary`, calls
`GetExtensionVersion`, and runs a mock `HttpExtensionProc` request without
requiring IIS to be installed.
