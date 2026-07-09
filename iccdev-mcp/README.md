# iccdev-mcp

MCP server for ICC color profile tools from the International Color
Consortium's RefIccMAX (iccDEV) library.

Exposes 25 tools to AI assistants (Claude, Copilot, Cursor) via the
Model Context Protocol, covering profile inspection, color transforms,
and format conversion for ICC v2/v4 and iccMAX v5 profiles.


## Install

For the lowest-friction setup, use the iccDEV regression Docker image. It
includes the MCP server, REST dependencies, all required iccDEV CLI tools,
runtime libraries, maintainer utilities, and `Testing/` profiles, so no local
iccDEV build is required.

```bash
docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master \
  iccdev-mcp-entrypoint mcp
```

Use `pip install` when integrating with a local source checkout or a locally
built iccDEV toolchain:

```bash
pip install iccdev-mcp
```

For full functionality with the pip-installed package, the 6 Python-native
tools are included in the `iccdev-mcp` wheel and the 17 CLI-backed tools
require iccDEV CLI binaries. Build iccDEV locally or set `ICCDEV_TOOLS_DIR` to
an existing tool build. The REST health endpoint reports the bundled Python API
and CLI discovery separately, so incomplete local CLI builds are visible at
startup.

```bash
# Point to compiled iccDEV tools
export ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools
```


## Usage

### Docker

The regression image includes the Python package, REST dependencies, iccDEV
CLI tools, runtime libraries, maintainer utilities, and `Testing/` profiles.
It is the lowest-friction way to use CLI-backed MCP tools without building
iccDEV locally.

The `master` regression image is published from the repository `master`
branch. Feature branches can still run the Docker workflow manually; those
runs build and smoke test the image without publishing it to GHCR.

```bash
# MCP stdio mode
docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master \
  iccdev-mcp-entrypoint mcp

# REST API mode on http://127.0.0.1:8080
docker run --rm -p 127.0.0.1:8080:8080 \
  ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master \
  iccdev-mcp-entrypoint rest
```

Open <http://127.0.0.1:8080/> for the bundled REST dashboard. The dashboard
groups the 25 MCP-backed API routes, shows REST-only utility routes such as
file browsing and upload, and runs tools without requiring a separate client.

Supported container modes are `mcp`/`stdio`, `rest`/`api`, `sse`, and
`streamable-http`/`http`.

Useful runtime defaults are already configured:

| Variable | Default |
|----------|---------|
| `ICCDEV_TOOLS_DIR` | `/workspace/build/Tools` |
| `ICCDEV_TESTING_DIR` | `/workspace/iccDEV/Testing` |
| `LD_LIBRARY_PATH` | iccDEV build library directories |

Mount additional profiles read-only and list them through
`ICCDEV_PROFILE_DIRS`:

```bash
docker run --rm -p 127.0.0.1:8080:8080 \
  -v "$PWD/profiles:/profiles:ro" \
  -e ICCDEV_PROFILE_DIRS=/profiles \
  ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master \
  iccdev-mcp-entrypoint rest
```

Local build and smoke test:

```bash
docker build -t iccdev-ci-regression:mcp-local -f Dockerfile.ci-regression .
docker run --rm -p 127.0.0.1:8080:8080 \
  iccdev-ci-regression:mcp-local iccdev-mcp-entrypoint rest
curl -fsS http://127.0.0.1:8080/api/health
curl -fsS http://127.0.0.1:8080/api/tools
# Optional browser UI
xdg-open http://127.0.0.1:8080/  # or open the URL manually
```

### Claude Desktop

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

For PR review, verify that the repository MCP settings JSON keeps the
`mcpServers.iccdev.tools` allowlist synchronized with the 25-tool inventory from
`/api/tools`. Also check that `profile_summary` keeps raw ICC signature fields
unchanged while exposing printable `*_display` fields for UI readability; ICC
signature values may arrive as either 4-character strings or raw 32-bit
integers, so both forms need test coverage. Dashboard changes should preserve
the stale-output reset when switching tools.

### Remote (SSE transport)

```bash
iccdev-mcp --transport sse --port 8080
```


## Tools

### Python-Native (6) -- always available

| Tool | Description |
|------|-------------|
| `inspect_header` | ICC profile header (22 fields + 6 computed names) |
| `profile_summary` | Compact profile classification and routing metadata |
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

### Utility (2)

| Tool | Description |
|------|-------------|
| `health_check` | Server status and tool availability |
| `list_available_profiles` | Browse ICC test profiles |

The REST dashboard at `/` and `/ui` provides Testing/ directory and filename
selectors for ICC, XML, JSON, TIFF, JPEG, PNG, and cube inputs. `/api/tools`
returns the stable 25-tool MCP inventory and separately lists REST-only utility
routes (`/api/tools`, `/api/files`, and `/api/upload`). Core CLI-backed forms
expose documented options for `iccDumpProfile` validation/verbosity/tag
selection, `iccPawgReport` profile assessment, `iccRoundTrip` intent/MPE
selection, and `iccApplyProfiles` structured or raw `-cfg` argument mode.

## Environment Variables

| Variable | Description |
|----------|-------------|
| `ICCDEV_TOOLS_DIR` | Path to iccDEV CLI tool binaries |
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
