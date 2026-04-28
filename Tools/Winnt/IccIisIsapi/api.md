# iccIisIsapi API Reference

This document covers the externally callable HTTP surface and native IIS ISAPI
exports for `iccIisIsapi.dll`. Setup and deployment instructions are in
[isapi-instructions.md](isapi-instructions.md).

Machine-readable starter spec: [iis-isapi.openapi.yaml](iis-isapi.openapi.yaml).

## Scope

Documented here:

- `iccIisIsapi.dll` HTTP endpoints
- persisted `_tool-work` browsing endpoints
- native IIS ISAPI exports: `GetExtensionVersion`, `HttpExtensionProc`,
  `TerminateExtension`

Not documented here:

- full C++ API surface of `IccProfLib`, `IccLibXML`, or `IccLibJSON`
- every CLI option outside the IIS-wrapped tool flow

## Base Assumptions

- The sample is Windows-only.
- IIS maps `iccIisIsapi.dll` through `IsapiModule`.
- The handler allows `GET`, `HEAD`, and `POST`.
- The site root contains required runtime DLLs and wrapped tool executables.
- `_tool-work` persists uploads, logs, and generated artifacts for HTTP retrieval.

## HTTP API

### `GET /iccIisIsapi.dll`

Default summary response.

- Status: `200 OK`
- Content type: `text/plain; charset=utf-8`
- Body includes `IccProfLib` version, `IccLibXML` version, profile spec version,
  XML payload byte count, and greeting text.

### `GET /iccIisIsapi.dll?mode=health`

Health probe for smoke tests and monitoring.

- Status: `200 OK`
- Content type: `text/plain; charset=utf-8`
- Body: `ok`

### `GET /iccIisIsapi.dll?format=xml`

Returns a minimal ICC XML payload generated in-process.

- Status: `200 OK`
- Content type: `application/xml; charset=utf-8`

### `POST /iccIisIsapi.dll?mode=tools&input=icc|xml[&filename=...]`

Runs the browser tool suite against uploaded ICC or XML data.

Supported request bodies:

- `application/octet-stream` for ICC or ICM uploads
- `application/xml` or `text/xml` for XML uploads

Query parameters:

| Parameter | Required | Values | Purpose |
|-----------|----------|--------|---------|
| `mode` | yes | `tools` | Select wrapped tool pipeline |
| `input` | yes | `icc`, `xml` | Uploaded data type |
| `filename` | no | sanitized file name | Persisted file naming and response metadata |

Current upload limit: `16 MB`.

For `input=icc`, the handler writes the upload to a per-request workspace and
runs `iccToXml`, `iccFromXml`, `iccDumpProfile`, `iccRoundTrip`, and JSON tools
when available.

For `input=xml`, the handler writes the upload as XML and runs `iccFromXml`,
`iccDumpProfile`, `iccToXml`, `iccRoundTrip`, and JSON tools when available.

The HTTP response is `200 OK` when the request is accepted and processed. Each
wrapped tool reports its own `exit_code`, `ok`, `skipped`, `output`, and artifact
links.

## Tool-Suite Response Shape

Top-level fields:

| Field | Meaning |
|-------|---------|
| `mode` | Always `tools` |
| `input.kind` | `icc` or `xml` |
| `input.filename` | sanitized workspace filename |
| `input.bytes` | uploaded byte count |
| `input.url` | relative URL for persisted upload |
| `workspace_url` | relative URL for request workspace |
| `tools` | per-tool result array |

Per-tool fields:

| Field | Meaning |
|-------|---------|
| `name` | wrapped tool name |
| `command` | command-line summary |
| `exit_code` | process exit code, or `-1` when skipped or launch failed |
| `ok` | true only for launched process exit `0` |
| `skipped` | true when intentionally skipped |
| `note` | timeout or skip reason |
| `output` | captured output, truncated for browser display when needed |
| `log_url` | relative URL for persisted log |
| `artifact_url` | relative URL for generated artifact |
| `artifact_preview` | text preview when available |

## Persisted Artifacts

Per-request workspaces live under `_tool-work/<job>/`. A workspace can contain:

- uploaded input
- stdout/stderr logs
- generated ICC, XML, or JSON artifacts
- `index.html` for browser navigation

## Native ISAPI Exports

| Export | Purpose |
|--------|---------|
| `GetExtensionVersion` | Reports extension version and description to IIS |
| `HttpExtensionProc` | Handles each request |
| `TerminateExtension` | Performs extension shutdown |

## Examples

```powershell
curl.exe http://localhost:18081/iccIisIsapi.dll?mode=health
curl.exe http://localhost:18081/iccIisIsapi.dll?format=xml
curl.exe -H "Content-Type: application/octet-stream" `
  --data-binary "@sample.icc" `
  "http://localhost:18081/iccIisIsapi.dll?mode=tools&input=icc&filename=sample.icc"
```
