# iccdev-mcp Documentation

## Contents

| Document | Description |
|----------|-------------|
| [cli-tool-reference.md](cli-tool-reference.md) | MCP tool to CLI tool mapping with arguments, examples, and exit codes |
| [icc-format-guide.md](icc-format-guide.md) | ICC binary format quick reference for MCP tool users |
| [security-guide.md](security-guide.md) | Vulnerability taxonomy and security analysis guide |
| [build-and-test.md](build-and-test.md) | Building iccDEV, running tests, environment setup |

## See Also

- [../README.md](../README.md) -- Package overview, install, MCP client configuration
- [../../.github/codeql-queries/iccdev-mcp/README.md](../../.github/codeql-queries/iccdev-mcp/README.md) -- Custom MCP CodeQL security queries
- [ICC.1-2022-05](https://www.color.org/specification/ICC.1-2022-05.pdf) -- ICC v4.4 specification
- [ICC.2-2023](https://www.color.org/specification/ICC.2-2023.pdf) -- ICC v5/iccMAX specification

## REST Inventory

`GET /api/tools` returns the stable 25-tool MCP inventory and a separate
`rest_utility_routes` list for REST-only helpers such as `/api/files` and
`/api/upload`.
