/**
 * @name Subprocess command injection via unsanitized MCP tool input
 * @description Detects user-controlled input flowing to subprocess.run() calls
 *              in cli_tools.py without passing through resolve_profile_path()
 *              or _run_tool() sanitization. The iccdev-mcp server accepts file
 *              paths and config strings from untrusted MCP/REST clients that
 *              reach iccDEV CLI tool invocations.
 * @kind path-problem
 * @problem.severity error
 * @id iccdev-mcp/subprocess-command-injection
 * @tags security command-injection subprocess mcp iccdev
 * @cwe CWE-078
 */

import python
import semmle.python.dataflow.new.DataFlow
import semmle.python.dataflow.new.TaintTracking

/**
 * MCP tool function parameters that accept user input.
 * These async def functions in server.py receive untrusted strings.
 */
class McpToolParameter extends DataFlow::Node {
  McpToolParameter() {
    exists(Function f, Name n |
      f.getName() in [
        // Python-native tools
        "inspect_header", "color_transform", "roundtrip_delta",
        // CLI-backed tools
        "dump_profile", "profile_to_xml", "xml_to_profile",
        "profile_to_json", "json_to_profile", "round_trip_test",
        "tiff_dump", "jpeg_dump", "png_dump", "from_cube",
        "apply_profiles", "apply_named_cmm", "create_link",
        "v5_to_v4", "spec_sep_to_tiff", "apply_search"
      ] and
      n.getId() in [
        "path", "profile", "src_profile", "dst_profile",
        "xml_path", "json_path", "cube_path", "config",
        "src_image", "dst_image", "output_path",
        "display_profile", "observer_profile"
      ] and
      n.getScope() = f and
      this.asExpr() = n
    )
  }
}

/**
 * REST API request parameter extraction.
 */
class RestApiInput extends DataFlow::Node {
  RestApiInput() {
    exists(Call c, StringLiteral s |
      c.getFunc().(Attribute).getName() = "get" and
      s.getText() in [
        "path", "path_a", "path_b", "config",
        "xml_path", "json_path", "cube_path",
        "input_tiff", "output_tiff"
      ] and
      c.getAnArg() = s and
      this.asExpr() = c
    )
  }
}

/**
 * Subprocess execution sinks in cli_tools.py.
 */
class SubprocessSink extends DataFlow::Node {
  SubprocessSink() {
    exists(Call c |
      c.getFunc().(Attribute).getName() in [
        "run", "Popen", "call", "check_output",
        "create_subprocess_exec"
      ] and
      this.asExpr() = c.getAnArg()
    )
  }
}

/**
 * Sanitizers that validate input before subprocess use.
 */
class InputSanitizer extends DataFlow::Node {
  InputSanitizer() {
    exists(Call c |
      (
        // Profile path resolution (null byte + traversal checks)
        c.getFunc().(Name).getId() = "resolve_profile_path" or
        c.getFunc().(Attribute).getName() = "resolve_profile_path" or
        // Path validation
        c.getFunc().(Attribute).getName() = "resolve" or
        c.getFunc().(Attribute).getName() = "is_file"
      ) and
      this.asExpr() = c
    )
  }
}

module SubprocessInjectionConfig implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    source instanceof McpToolParameter or
    source instanceof RestApiInput
  }

  predicate isSink(DataFlow::Node sink) {
    sink instanceof SubprocessSink
  }

  predicate isBarrier(DataFlow::Node node) {
    node instanceof InputSanitizer
  }
}

module SubprocessInjection = TaintTracking::Global<SubprocessInjectionConfig>;
import SubprocessInjection::PathGraph

from SubprocessInjection::PathNode source, SubprocessInjection::PathNode sink
where SubprocessInjection::flowPath(source, sink)
select sink.getNode(), source, sink,
  "Unsanitized user input from $@ flows to subprocess call without resolve_profile_path() validation.",
  source.getNode(), "MCP/REST parameter"
