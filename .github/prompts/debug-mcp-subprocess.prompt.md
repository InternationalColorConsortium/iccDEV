# Debug MCP Server Subprocess Issues

Use this prompt when MCP tool calls return errors, empty output, or unexpected results
from the iccdev-mcp server's subprocess-based tool execution.

For rolling-release package failures, validate the universal wheel and sdist
with `.github/scripts/validate-mcp-release-package.py <distribution-directory>`.
Run `.github/scripts/iccdev-mcp-package-smoke.py --source-tree` in a source
environment and omit the flag in separate installed wheel/sdist environments.
The smoke proves parent/child imports, console startup, stdio discovery and
profile inspection, REST health, and explicit missing-transform-backend errors.
Keep Windows and Linux installation evidence separate; verify the exact release
tag, both checksum manifests, provenance, and source commit before consuming
assets. Release asset URLs are replaceable and must not be called immutable.
See `docs/python-packaging-release.md` for the GitHub wheel/uvx route; a GitHub
release or branch push does not establish PyPI publication.

## Diagnostic Steps

1. **Check runtime inventory**
   ```bash
   python3 - <<'PY'
   from iccdev_mcp.server import health_check
   health = health_check()
   assert health["status"] == "ok"
   print(health)
   # Check that the specific backend needed by the failing call is available.
   assert "iccDumpProfile" in health["cli_tools"]["available"]
   PY
   ```

   Change `iccDumpProfile` to the tool under investigation. Missing optional
   backends are expected in a package-only install; an advertised tool name or
   fixed `total_tools` value is not an availability check.

2. **Check binary availability**
   ```bash
   python3 -c "from iccdev_mcp.cli_tools import _find_tool; print(_find_tool('iccDumpProfile'))"
   ```

3. **Test subprocess directly**
   ```bash
   # Run the same command the MCP server would execute
   iccDumpProfile /path/to/profile.icc ALL
   echo "Exit code: $?"
   ```

4. **Inspect sanitizer failures**
   ```bash
   ASAN_OPTIONS=halt_on_error=1 iccDumpProfile /path/to/profile.icc ALL
   ```

   Preserve the sanitizer report and exit status. A sanitizer abort is a
   failure to investigate, not a successful MCP response.

5. **Verify PATH includes tool directories**
   ```bash
   for d in Build/Tools/*/; do
     echo "$d: $(ls "$d"icc* 2>/dev/null | head -1)"
   done
   ```

6. **Test MCP tool in isolation**
   ```python
   from iccdev_mcp.server import dump_profile
   result = dump_profile("/path/to/profile.icc")
   print(result)
   assert result["returncode"] == 0, result
   ```

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Tool unavailable | Required binary not found | Check `ICCDEV_TOOLS_DIR`, build target, and runtime library path |
| Exit code 134 | Sanitizer or process abort | Save the report and reproduce the exact argv outside MCP |
| Timeout | Tool exceeds the MCP subprocess bound | Save the input and exact argv; reproduce with an external timeout |
| Unexpected response | CLI output differs from the MCP result | Inspect stdout, stderr, and structured MCP content separately |
| Permission denied | Binary not executable | chmod +x on tool binaries |
| Different health totals | Optional native ABI differs by image | Check capability flags, not a fixed count |
