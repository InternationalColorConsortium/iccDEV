/**
 * @name Missing output sanitization on subprocess results
 * @description Detects subprocess calls in tool wrapper functions where the
 *              output (stdout/stderr) is returned without sanitization. The
 *              cli_tools._run_tool() function enforces a 10 MB output cap and
 *              uses errors="replace" decoding, but direct subprocess calls
 *              bypass this protection. Unsanitized output may contain ANSI
 *              escape sequences or control characters enabling terminal
 *              injection attacks on MCP clients.
 * @kind problem
 * @problem.severity warning
 * @id iccdev-mcp/missing-output-sanitization
 * @tags security sanitization output mcp iccdev
 * @cwe CWE-116
 */

import python

/**
 * Direct subprocess calls (not through _run_tool wrapper).
 */
class DirectSubprocessCall extends Call {
  DirectSubprocessCall() {
    this.getFunc().(Attribute).getName() in [
      "run", "Popen", "call", "check_output", "communicate"
    ]
  }
}

/**
 * The approved subprocess wrapper: _run_tool().
 * Any subprocess call inside _run_tool is already sanitized.
 */
class ApprovedWrapper extends Function {
  ApprovedWrapper() {
    this.getName() = "_run_tool"
  }
}

/**
 * Tool functions that should use _run_tool() instead of direct subprocess.
 */
class ToolFunction extends Function {
  ToolFunction() {
    this.getName() in [
      "run_dump_profile", "run_to_xml", "run_from_xml",
      "run_to_json", "run_from_json", "run_round_trip",
      "run_tiff_dump", "run_jpeg_dump", "run_png_dump",
      "run_from_cube", "run_apply_profiles",
      "run_apply_named_cmm", "run_apply_to_link",
      "run_v5_to_v4", "run_spec_sep_to_tiff",
      "run_apply_search",
      // REST API handlers
      "api_dump_profile", "api_inspect_header",
      "api_to_xml", "api_from_xml", "api_round_trip",
      "api_tiff_dump", "api_jpeg_dump", "api_png_dump",
      "api_from_cube", "api_apply_profiles",
      "api_apply_named_cmm", "api_apply_search",
      "api_create_link", "api_v5_to_v4",
      "api_spec_sep_to_tiff"
    ]
  }
}

from DirectSubprocessCall subproc, ToolFunction tool
where
  tool = subproc.getScope() and
  not tool instanceof ApprovedWrapper
select subproc,
  "Direct subprocess call in " + tool.getName() +
  " bypasses _run_tool() output sanitization. " +
  "Use _run_tool() for consistent output size limiting and error handling."
