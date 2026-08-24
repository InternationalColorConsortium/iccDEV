# Debug MCP Server Subprocess Issues

Use this prompt when MCP tool calls return errors, empty output, or unexpected results
from the iccdev-mcp server's subprocess-based tool execution.

## Diagnostic Steps

1. **Check runtime inventory**
   ```bash
   python3 -c 'from iccdev_mcp.cli_tools import TOOL_BINARIES; from iccdev_mcp.server import health_check; health = health_check(); assert set(health["cli_tools"]["available"]) == set(TOOL_BINARIES); assert not health["cli_tools"]["missing"]; print(health)'
   ```

2. **Check binary availability**
   ```bash
   python3 -c "from iccdev_mcp.cli_tools import _find_tool; print(_find_tool('iccDumpProfile'))"
   ```

3. **Test subprocess directly**
   ```bash
   # Run the same command the MCP server would execute
   iccDumpProfile /path/to/profile.icc ALL 2>&1
   echo "Exit code: $?"
   ```

4. **Check ASAN interference**
   ```bash
   ASAN_OPTIONS=detect_leaks=0,halt_on_error=0 iccDumpProfile profile.icc ALL
   ```

5. **Verify PATH includes tool directories**
   ```bash
   for d in Build/Tools/*/; do
     echo "$d: $(ls "$d"icc* 2>/dev/null | head -1)"
   done
   ```

6. **Test MCP tool in isolation**
   ```python
   import asyncio
   from iccdev_mcp.server import IccDevMcpServer
   server = IccDevMcpServer()
   result = asyncio.run(server.dump_profile("/path/to/profile.icc"))
   print(result[:500])
   ```

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Empty output | Binary not found | Rebuild with ENABLE_TOOLS=ON |
| Exit code 134 | ASAN abort | Set ASAN_OPTIONS=halt_on_error=0 |
| Timeout | Malformed profile loops | Add -timeout flag or skip profile |
| JSON parse error | stderr mixed in stdout | Check include_stderr parameter |
| Permission denied | Binary not executable | chmod +x on tool binaries |
| Different health totals | Optional native ABI differs by image | Check capability flags, not a fixed count |
