# iccdev-mcp

MCP server for ICC color profile tools from the International Color
Consortium's RefIccMAX (iccDEV) library.

Exposes 26 tools to AI assistants (Claude, Copilot, Cursor) via the
Model Context Protocol, covering profile inspection, color transforms,
and format conversion for ICC v2/v4 and iccMAX v5 profiles.

The static MCP inventory has 26 tools, but `health_check` reports tools
available in the running environment. Optional native capabilities can differ
between container variants, so clients and CI must inspect capability flags and
the discovered CLI inventory rather than assume a fixed health-tool total.
The response includes the available native, CLI, and service inventories so
clients can derive its dynamic `total_tools` value without numeric assumptions.


## Install

For the lowest-friction setup, use the unified iccDEV Docker image. It includes
the MCP server, REST dependencies, CLI tools, runtime libraries, maintainer
utilities, and `Testing/` profiles, so no local iccDEV build is required.

```bash
docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccdev-mcp-entrypoint mcp
```

Use `pip install` when integrating with a local source checkout or a locally
built iccDEV toolchain:

```bash
pip install iccdev-mcp
```

For full functionality with the pip-installed package, seven Python-native tools
are included in the `iccdev-mcp` wheel. The `validate_profile` tool dynamically
loads the public C validation ABI, so it also needs a shared IccProfLib build.
The 17 CLI-backed tools require iccDEV CLI binaries. Build iccDEV locally or
set `ICCDEV_TOOLS_DIR` and `ICCDEV_BUILD_DIR` to an existing build. The REST
health endpoint reports the bundled Python API and CLI discovery separately, so
incomplete local builds are visible at startup.

```bash
# Point to compiled iccDEV tools
export ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools
export ICCDEV_BUILD_DIR=/path/to/iccDEV/Build
```


## Usage

### Docker

The unified image uses a shell default. Start an MCP transport explicitly:

```bash
# MCP stdio mode
docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccdev-mcp-entrypoint mcp

# REST API mode on http://127.0.0.1:8080
docker run --rm -p 127.0.0.1:8080:8080 \
  ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccdev-mcp-entrypoint rest
```

Open <http://127.0.0.1:8080/> for the bundled REST dashboard. The dashboard
groups the 26 MCP-backed API routes, shows REST-only utility routes such as
file browsing and upload, and runs tools without requiring a separate client.

Supported container modes are `mcp`/`stdio`, `rest`/`api`, `sse`, and
`streamable-http`/`http`.

Useful runtime defaults are already configured:

| Variable | Default |
|----------|---------|
| `ICCDEV_TOOLS_DIR` | `/workspace/build/Tools` |
| `ICCDEV_TESTING_DIR` | `/workspace/iccDEV/Testing` |
| `ICCDEV_VALIDATION_LIBRARY` | `/opt/iccdev-validation/lib/libIccProfLib2.so` |

The unified image keeps sanitizer CLI tools, but loads the validation ABI from
an isolated non-sanitized shared build. An explicit validation library overrides
`ICCDEV_BUILD_DIR`, including the image default above. To select a custom build,
override this library path or unset it before starting the server. Empty,
missing, and unloadable explicit library paths fail closed without silently
falling back. Pip-only installations retain optional validation behavior.

Mount additional profiles read-only and list them through
`ICCDEV_PROFILE_DIRS`:

```bash
docker run --rm -p 127.0.0.1:8080:8080 \
  -v "$PWD/profiles:/profiles:ro" \
  -e ICCDEV_PROFILE_DIRS=/profiles \
  ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccdev-mcp-entrypoint rest
```

Local build and smoke test:

```bash
docker build -t iccdev:mcp-local -f Dockerfile .
docker run --rm -p 127.0.0.1:8080:8080 \
  iccdev:mcp-local iccdev-mcp-entrypoint rest
curl -fsS http://127.0.0.1:8080/api/health
curl -fsS http://127.0.0.1:8080/api/tools
python3 .github/scripts/iccdev-container-smoke.py iccdev:mcp-local
# Optional REST dashboard in a browser
xdg-open http://127.0.0.1:8080/  # or open the URL manually
```

### Claude Desktop

For a container-only local client, use this unified image configuration. Keep
`-i` (open stdin), omit `-t`, and do not close stdin until requested tool
responses arrive:

```json
{
  "mcpServers": {
    "iccdev": {
      "command": "docker",
      "args": ["run", "--rm", "-i",
               "ghcr.io/internationalcolorconsortium/iccdev:latest",
               "iccdev-mcp-entrypoint", "mcp"]
    }
  }
}
```

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "iccdev": {
      "command": "iccdev-mcp",
      "env": {
        "ICCDEV_TOOLS_DIR": "/path/to/iccDEV/Build/Tools"
      }
    }
  }
}
```

### VS Code (Copilot)

Add to `.vscode/mcp.json`:

```json
{
  "servers": {
    "iccdev": {
      "command": "iccdev-mcp"
    }
  }
}
```

### GitHub Copilot coding agent

Add this repository MCP configuration under
`Settings` -> `Copilot` -> `MCP servers`. GitHub requires explicit `type`,
`args`, and `tools` fields for repository MCP settings.

```json
{
  "mcpServers": {
    "iccdev": {
      "type": "local",
      "command": "iccdev-mcp",
      "args": [],
      "env": {
        "ICCDEV_TOOLS_DIR": "Build/Tools"
      },
      "tools": [
        "inspect_header",
        "profile_summary",
        "validate_profile",
        "color_transform",
        "roundtrip_delta",
        "icc_sig_to_str",
        "enum_spaces",
        "list_available_profiles",
        "health_check",
        "dump_profile",
        "profile_to_xml",
        "xml_to_profile",
        "profile_to_json",
        "json_to_profile",
        "tiff_dump",
        "jpeg_dump",
        "png_dump",
        "from_cube",
        "apply_profiles",
        "apply_named_cmm",
        "create_link",
        "v5_to_v4",
        "spec_sep_to_tiff",
        "apply_search",
        "round_trip_test",
        "pawg_report"
      ]
    }
  }
}
```

### Remote (SSE transport)

```bash
iccdev-mcp --transport sse --port 8080
```


## Tools

### Python-Native (7)

| Tool | Description |
|------|-------------|
| `inspect_header` | ICC profile header (22 fields + 6 computed names) |
| `profile_summary` | Compact profile classification and routing metadata |
| `validate_profile` | In-memory validation status and report from the native C API; requires a shared IccProfLib build |
| `color_transform` | Multi-profile color transform via CMM |
| `roundtrip_delta` | Round-trip fidelity measurement (delta-E) |
| `icc_sig_to_str` | 4-byte ICC signature to string |
| `enum_spaces` | List all 32 ICC color space identifiers |

### CLI-Backed (17) -- require iccDEV tools

| Tool | CLI Binary | Description |
|------|-----------|-------------|
| `dump_profile` | iccDumpProfile | Full profile text dump |
| `pawg_report` | iccPawgReport | PAWG security, conformance, and quality checklist |
| `profile_to_xml` | iccToXml | ICC to XML conversion |
| `xml_to_profile` | iccFromXml | XML to ICC conversion |
| `profile_to_json` | iccToJson | ICC to JSON conversion |
| `json_to_profile` | iccFromJson | JSON to ICC conversion |
| `round_trip_test` | iccRoundTrip | Round-trip transform fidelity |
| `tiff_dump` | iccTiffDump | TIFF metadata + embedded ICC |
| `jpeg_dump` | iccJpegDump | JPEG metadata + embedded ICC |
| `png_dump` | iccPngDump | PNG metadata + embedded ICC |
| `from_cube` | iccFromCube | .cube LUT to ICC profile |
| `apply_profiles` | iccApplyProfiles | Multi-profile TIFF transform |
| `apply_named_cmm` | iccApplyNamedCmm | Named color CMM |
| `create_link` | iccApplyToLink | Device link creation |
| `v5_to_v4` | iccV5DspObsToV4Dsp | v5 display to v4 conversion |
| `spec_sep_to_tiff` | iccSpecSepToTiff | Spectral separation |
| `apply_search` | iccApplySearch | Search-based transform |

### CLI-Only Tools (Not MCP-Exposed)

The following built iccDEV executables do not have MCP wrappers and must not
be added to an MCP server `tools` allowlist: `iccBenchApply`, `iccProfilePlot`,
`iccProfileVisualize`, and `iccProfileVisualizePlot`. Use them directly from
the command line; see the repository
[CLI tool reference](../docs/tools-cli-reference.md) for their interfaces.

### Utility (2)

| Tool | Description |
|------|-------------|
| `health_check` | Server status and tool availability |
| `list_available_profiles` | Browse ICC test profiles |

The REST dashboard at `/` and `/ui` provides Testing/ directory and filename
selectors for ICC, XML, JSON, TIFF, JPEG, PNG, and cube inputs. `/api/tools`
returns the stable 26-tool MCP inventory and separately lists REST-only utility
routes (`/api/tools`, `/api/files`, and `/api/upload`). Core CLI-backed forms
expose documented options for `iccDumpProfile` validation/verbosity/tag
selection, `iccPawgReport` profile assessment, `iccRoundTrip` intent/MPE
selection, and `iccApplyProfiles` structured or raw `-cfg` argument mode.

## Environment Variables

| Variable | Description |
|----------|-------------|
| `ICCDEV_TOOLS_DIR` | Path to iccDEV CLI tool binaries |
| `ICCDEV_BUILD_DIR` | CMake build root containing the shared IccProfLib validation ABI |
| `ICCDEV_VALIDATION_LIBRARY` | Explicit shared IccProfLib path; overrides `ICCDEV_BUILD_DIR` |
| `ICCDEV_TESTING_DIR` | Path to iccDEV Testing/ profiles directory |
| `ICCDEV_PROFILE_DIRS` | Additional profile search directories (colon-separated) |


## Development

```bash
# Install for development
cd iccdev-mcp
pip install -e ".[dev,rest]"

# Run tests
pytest tests/ -v

# Run server (stdio)
python -m iccdev_mcp.server
```


## License

BSD 3-Clause. See [LICENSE.md](../LICENSE.md).
