# CodeQL Security Queries for iccdev-mcp

Custom CodeQL queries targeting security patterns specific to the iccdev-mcp
MCP server package wrapping iccDEV ICC profile analysis tools.

## Queries

| # | Query | ID | Severity | CWE | Description |
|---|-------|----|----------|-----|-------------|
| 1 | subprocess-command-injection.ql | iccdev-mcp/subprocess-command-injection | ERROR | CWE-078 | User input flowing to subprocess.run() without resolve_profile_path() |
| 2 | profile-path-traversal.ql | iccdev-mcp/profile-path-traversal | ERROR | CWE-022 | Profile paths reaching file ops without null byte and .. checks |
| 3 | api-path-traversal.ql | iccdev-mcp/api-path-traversal | ERROR | CWE-022 | REST API parameters reaching file/process ops without validation |
| 4 | unsafe-env-server-bind.ql | iccdev-mcp/unsafe-env-server-bind | WARNING | CWE-015 | Env vars (HOST/PORT) flowing to server bind unvalidated |
| 5 | unvalidated-file-upload.ql | iccdev-mcp/unvalidated-file-upload | WARNING | CWE-434 | Upload handlers missing size/type validation |
| 6 | missing-output-sanitization.ql | iccdev-mcp/missing-output-sanitization | WARNING | CWE-116 | Direct subprocess calls bypassing _run_tool() output sanitization |

## Coverage Gaps Filled

These queries detect patterns NOT covered by the 50 standard Python security queries:

- File upload size/type validation (no standard query)
- REST API path parameter validation (generic py/path-injection exists but is not API-centric)
- Server binding configuration validation (no standard query)
- Output sanitization for CLI results (no standard query)

## Running

```bash
# Install dependencies (first time only)
gh codeql pack install .github/codeql-queries/iccdev-mcp

# Create database
gh codeql database create /tmp/codeql-db --language=python \
  --source-root=iccdev-mcp --overwrite

# Run custom queries only
gh codeql database analyze /tmp/codeql-db --format=sarif-latest \
  --output=/tmp/results.sarif --threads=0 \
  .github/codeql-queries/iccdev-mcp/iccdev-mcp-security-suite.qls

# Run full suite (custom + standard Python security)
gh codeql database analyze /tmp/codeql-db --format=sarif-latest \
  --output=/tmp/results.sarif --threads=0 \
  codeql/python-queries:codeql-suites/python-security-and-quality.qls \
  .github/codeql-queries/iccdev-mcp/iccdev-mcp-security-suite.qls
```

## Security Model

The iccdev-mcp package processes untrusted ICC profiles via:

1. **MCP tool invocation** -- AI clients send file paths and config strings
2. **REST API endpoints** -- HTTP clients send query parameters and uploads
3. **Subprocess execution** -- All CLI tools run via subprocess.run() in list form

### Approved Sanitization Patterns

| Pattern | Module | Protection |
|---------|--------|------------|
| `resolve_profile_path()` | profiles.py | Null byte rejection, .. traversal blocking, directory search |
| `list_profiles()` | profiles.py | Directory null byte and .. rejection, Testing/ boundary check |
| `_run_tool()` | cli_tools.py | 10 MB output cap, timeout, list-form subprocess (no shell) |
| ASCII filename normalization | rest_api.py | Filename sanitization for uploads |
| Bounded upload reads | rest_api.py | Upload size limit enforcement |

### Expected Results

The checked-in server should have zero custom-query findings. Any non-zero
result means a path, subprocess, upload, output, or bind-surface regression
needs triage before release.

## Provenance

Adapted from existing MCP security query patterns and maintained under
`.github/codeql-queries/` so security automation has one repository-level query
location.
