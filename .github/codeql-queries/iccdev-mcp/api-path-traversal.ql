/**
 * @name REST API path traversal via query parameters
 * @description Detects REST API endpoints in rest_api.py that extract path
 *              parameters from HTTP requests and pass them to file or subprocess
 *              operations without validation. Several endpoints (tiff_dump,
 *              jpeg_dump, png_dump) accept path query params that bypass
 *              resolve_profile_path().
 * @kind path-problem
 * @problem.severity error
 * @id iccdev-mcp/api-path-traversal
 * @tags security path-traversal ssrf rest-api iccdev
 * @cwe CWE-022
 */

import python
import semmle.python.dataflow.new.DataFlow
import semmle.python.dataflow.new.TaintTracking

predicate inRestApi(Expr e) {
  e.getLocation().getFile().getRelativePath().matches("%iccdev_mcp/rest_api.py")
}

/**
 * HTTP request parameter extraction from Starlette/FastAPI.
 * Matches: request.query_params.get("path"), body.get("path"), etc.
 */
class ApiPathSource extends DataFlow::Node {
  ApiPathSource() {
    exists(Call c, StringLiteral s |
      c.getFunc().(Attribute).getName() = "get" and
      s.getText() in [
        "path", "path_a", "path_b", "directory",
        "xml_path", "json_path", "cube_path",
        "input_tiff", "output_tiff", "config"
      ] and
      c.getAnArg() = s and
      inRestApi(c) and
      this.asExpr() = c
    )
    or
    // Subscript access: query_params["path"]
    exists(Subscript sub, StringLiteral s |
      s.getText() in ["path", "path_a", "path_b", "directory"] and
      sub.getIndex() = s and
      inRestApi(sub) and
      this.asExpr() = sub
    )
  }
}

/**
 * File system and subprocess sinks.
 */
class FileOrProcessSink extends DataFlow::Node {
  FileOrProcessSink() {
    exists(Call c |
      (
        c.getFunc().(Name).getId() in ["Path", "open"] or
        c.getFunc().(Attribute).getName() in [
          "run", "Popen", "create_subprocess_exec",
          "read_text", "read_bytes", "write_bytes",
          "exists", "is_file", "resolve", "stat"
        ]
      ) and
      this.asExpr() = c.getAnArg()
    )
    or
    exists(BinaryExpr be |
      be.getOp() instanceof Div and
      this.asExpr() = be.getRight()
    )
  }
}

/**
 * Path validation functions that block traversal.
 */
class PathSanitizer extends DataFlow::Node {
  PathSanitizer() {
    exists(Call c |
      (
        c.getFunc().(Name).getId() = "resolve_profile_path" or
        c.getFunc().(Attribute).getName() = "resolve_profile_path" or
        c.getFunc().(Attribute).getName() = "is_relative_to"
      ) and
      this.asExpr() = c
    )
  }
}

module ApiPathConfig implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    source instanceof ApiPathSource
  }

  predicate isSink(DataFlow::Node sink) {
    sink instanceof FileOrProcessSink and
    // Exclude sinks inside approved path validation/listing helpers.
    not exists(Function f |
      f.getName() in [
        "resolve_profile_path", "_find_profile_dirs",
        "_find_testing_dir", "list_profiles", "list_files"
      ] and
      sink.asExpr().getScope+() = f
    )
  }

  predicate isBarrier(DataFlow::Node node) {
    node instanceof PathSanitizer
  }
}

module ApiPath = TaintTracking::Global<ApiPathConfig>;
import ApiPath::PathGraph

from ApiPath::PathNode source, ApiPath::PathNode sink
where ApiPath::flowPath(source, sink)
select sink.getNode(), source, sink,
  "User-controlled API parameter from $@ reaches file/process operation without path validation.",
  source.getNode(), "REST API parameter"
