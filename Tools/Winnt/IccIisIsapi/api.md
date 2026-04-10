## API Reference

This document starts the API documentation process for the Windows IIS sample
added in the `ci-shared-exports` work. It covers the public HTTP surface,
the native ISAPI entry points, and the persisted artifact model exposed by
`iccIisIsapi.dll`.

This is intentionally narrower than the full C++ library surface of
`IccProfLib` and `IccLibXML`. It documents the externally callable API that
operators, reviewers, and web consumers can exercise today.

Machine-readable starter spec:

- [iis-isapi.openapi.yaml](iis-isapi.openapi.yaml)

## Scope

Documented here:

- `iccIisIsapi.dll` HTTP endpoints
- persisted `_tool-work` browsing endpoints
- native IIS ISAPI exports:
  - `GetExtensionVersion`
  - `HttpExtensionProc`
  - `TerminateExtension`

Not yet documented here:

- the complete public C++ class surface of `IccProfLib`
- the complete public C++ class surface of `IccLibXML`
- the complete public C++ class surface of `IccLibJSON`
- every CLI tool option outside the IIS-wrapped tool flow

## Base assumptions

- The sample is Windows-only.
- IIS maps `iccIisIsapi.dll` through `IsapiModule`.
- The handler allows `GET`, `HEAD`, and `POST`.
- The site root contains the runtime DLL set plus the tool executables used by
  the upload workflow.
- The site root also contains `_tool-work`, where uploaded inputs, generated
  files, and logs are persisted for later HTTP retrieval.

## HTTP API

### `GET /iccIisIsapi.dll`

Default summary response.

Response:

- status: `200 OK`
- content type: `text/plain; charset=utf-8`
- body includes:
  - `IccProfLib` version
  - `IccLibXML` version
  - profile spec version
  - generated XML byte count

Example:

```text
IccProfLib version: 2.3.1.7+0cbfc46
IccLibXML version: 2.3.1.7+0cbfc46
Profile spec ver: 4.00
XML payload bytes: 786
Hello from iccDEV IIS ISAPI!
```

### `GET /iccIisIsapi.dll?mode=health`

Health probe for smoke tests and monitoring.

Response:

- status: `200 OK`
- content type: `text/plain; charset=utf-8`
- body: `ok`

### `GET /iccIisIsapi.dll?format=xml`

Returns a minimal ICC XML payload generated in-process.

Response:

- status: `200 OK`
- content type: `application/xml; charset=utf-8`
- body: XML document

### `POST /iccIisIsapi.dll?mode=tools&input=icc|xml[&filename=...]`

Runs the browser tool suite against uploaded data.

Supported request bodies:

- `application/octet-stream` for ICC or ICM uploads
- `application/xml` for XML uploads
- `text/xml` for XML uploads

Query parameters:

- `mode`
  - required
  - must be `tools`
- `input`
  - required
  - allowed values: `icc`, `xml`
- `filename`
  - optional
  - used only for persisted file naming and response metadata

Upload limits:

- current handler limit: `16 MB`

Processing rules:

- `input=icc`
  - writes the uploaded ICC profile into a per-request workspace
  - runs:
    - `iccToXml`
    - `iccFromXml`
    - `iccDumpProfile`
    - `iccRoundTrip`
    - `iccToJson` (if IccJSON is built)
    - `iccFromJson` (if IccJSON is built and iccToJson produced output)
- `input=xml`
  - writes the uploaded XML into a per-request workspace
  - runs:
    - `iccFromXml`
    - `iccDumpProfile`
    - `iccToXml`
    - `iccRoundTrip`
    - `iccToJson` (if IccJSON is built and iccFromXml produced an ICC file)

Response:

- status: `200 OK` when the request itself is accepted and processed
- content type: `application/json; charset=utf-8`

Important behavior:

- A `200 OK` tool response does not mean every wrapped tool succeeded.
- Each tool reports its own `exit_code`, `ok`, `skipped`, `output`, and links to
  persisted logs or artifacts.
- `iccRoundTrip` can fail for a given profile while the overall HTTP request
  still succeeds.

## Tool-suite response shape

Top-level JSON fields:

- `mode`
  - always `tools`
- `input.kind`
  - `icc` or `xml`
- `input.filename`
  - sanitized filename stored in the workspace
- `input.bytes`
  - uploaded body size
- `input.url`
  - relative HTTP URL for the persisted upload
- `workspace_url`
  - relative HTTP URL for the per-request workspace
- `tools`
  - array of per-tool results

Per-tool JSON fields:

- `name`
  - tool name such as `iccToXml`
- `command`
  - command line summary used for the tool run
- `exit_code`
  - process exit code, or `-1` when launch failed or the tool was skipped
- `ok`
  - `true` only when the tool launched and exited `0`
- `skipped`
  - `true` when the handler intentionally skipped the tool
- `note`
  - human-readable note for timeouts or skip reasons
- `output`
  - captured stdout or stderr, truncated for browser display when needed
- `log_name`
  - persisted log filename
- `log_bytes`
  - persisted log size
- `log_url`
  - relative HTTP URL for the persisted log
- `artifact_name`
  - generated file name, if present
- `artifact_bytes`
  - generated file size
- `artifact_url`
  - relative HTTP URL for the generated file
- `artifact_preview`
  - text preview for generated XML when available

Representative response fragment:

```json
{
  "mode": "tools",
  "input": {
    "kind": "icc",
    "filename": "calcOverMem_tget.icc",
    "bytes": 3928,
    "url": "./_tool-work/iis1265.tmp/calcOverMem_tget.icc"
  },
  "workspace_url": "./_tool-work/iis1265.tmp/",
  "tools": [
    {
      "name": "iccToXml",
      "command": "iccToXml \"calcOverMem_tget.icc\" \"generated-from-upload.xml\"",
      "exit_code": 0,
      "ok": true,
      "skipped": false,
      "note": "",
      "log_url": "./_tool-work/iis1265.tmp/iccToXml.stdout.txt",
      "artifact_url": "./_tool-work/iis1265.tmp/generated-from-upload.xml"
    }
  ]
}
```

## Persisted artifact browsing

### `GET /_tool-work/`

Serves a stable landing page for the workspace root and avoids the default IIS
`403.14 - Forbidden` behavior at the directory level.

Response:

- status: `200 OK`
- content type: `text/html`

### `GET /_tool-work/{jobId}/`

Serves the per-request workspace manifest page.

Response:

- status: `200 OK`
- content type: `text/html`

The page links to:

- the original upload
- generated XML or ICC files
- persisted tool logs

### `GET /_tool-work/{jobId}/{artifactName}`

Serves a persisted upload, generated artifact, or log.

Observed content types:

- `.xml`: `text/xml`
- `.txt`: `text/plain`
- `.icc`, `.icm`: `application/octet-stream`

## Error model

Expected request-level errors:

- `400 Bad Request`
  - unsupported or malformed tool request
  - `mode=tools` called without `POST`
  - empty upload body
  - upload exceeds handler size limit
- `500 Internal Server Error`
  - unexpected unhandled failure in the extension

Representative `400` body:

```json
{
  "error": "Tool mode requires an HTTP POST request."
}
```

## Native ISAPI exports

### `BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO* versionInfo)`

Purpose:

- returns extension metadata to IIS

Behavior:

- fills `dwExtensionVersion`
- fills `lpszExtensionDesc` with the extension description string

### `DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK ecb)`

Purpose:

- main IIS entry point for request dispatch

Behavior:

- routes:
  - `mode=tools`
  - `mode=health`
  - `format=xml`
  - default summary
- writes the final response through the IIS ECB callbacks

### `BOOL WINAPI TerminateExtension(DWORD)`

Purpose:

- shutdown hook for IIS

Behavior:

- currently returns `TRUE` without custom teardown

## Local examples

Health:

```powershell
curl.exe http://localhost:18081/iccIisIsapi.dll?mode=health
```

Summary:

```powershell
curl.exe http://localhost:18081/iccIisIsapi.dll
```

XML:

```powershell
curl.exe http://localhost:18081/iccIisIsapi.dll?format=xml
```

ICC upload:

```powershell
curl.exe -H "Accept: application/json" -H "Content-Type: application/octet-stream" `
  --data-binary "@sample.icc" `
  "http://localhost:18081/iccIisIsapi.dll?mode=tools&input=icc&filename=sample.icc"
```

XML upload:

```powershell
curl.exe -H "Accept: application/json" -H "Content-Type: application/xml" `
  --data-binary "@sample.xml" `
  "http://localhost:18081/iccIisIsapi.dll?mode=tools&input=xml&filename=sample.xml"
```

## Next documentation steps

Recommended follow-up work:

- add generated docs or site navigation for the new OpenAPI file
- split the IIS API reference into stable and experimental sections if the
  sample grows
- document the public C++ library surface of `IccProfLib` and `IccLibXML`
- document each CLI tool in a dedicated command reference section
