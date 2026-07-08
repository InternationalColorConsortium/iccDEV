/**
 * @name Profile path traversal via user-supplied ICC path
 * @description Detects user-controlled ICC profile path values reaching file
 *              system operations without passing through resolve_profile_path().
 *              The profiles.py module provides null byte rejection, path traversal
 *              (.. component) blocking, and controlled directory search. Bypassing
 *              this allows reading/analyzing arbitrary files.
 * @kind path-problem
 * @problem.severity error
 * @id iccdev-mcp/profile-path-traversal
 * @tags security path-traversal file-access mcp iccdev
 * @cwe CWE-022
 */

import python
import semmle.python.dataflow.new.DataFlow
import semmle.python.dataflow.new.TaintTracking

/**
 * User-supplied profile path in MCP tool functions.
 */
class ProfilePathSource extends DataFlow::Node {
  ProfilePathSource() {
    exists(Function f, Name n |
      f.getName() in [
        "dump_profile", "profile_to_xml", "profile_to_json",
        "round_trip_test", "tiff_dump", "jpeg_dump", "png_dump",
        "from_cube", "inspect_header", "color_transform",
        "roundtrip_delta", "v5_to_v4", "create_link",
        "apply_profiles", "spec_sep_to_tiff"
      ] and
      n.getId() in [
        "path", "profile", "src_profile", "dst_profile",
        "cube_path", "xml_path", "json_path",
        "display_profile", "observer_profile",
        "src_image", "dst_image", "output_path"
      ] and
      n.getScope() = f and
      this.asExpr() = n
    )
    or
    // REST API query parameter extraction
    exists(Call c, StringLiteral s |
      c.getFunc().(Attribute).getName() = "get" and
      s.getText() in ["path", "path_a", "path_b", "directory"] and
      c.getAnArg() = s and
      this.asExpr() = c
    )
  }
}

/**
 * File system sinks: Path(), open(), read operations.
 */
class FileSystemSink extends DataFlow::Node {
  FileSystemSink() {
    exists(Call c |
      (
        c.getFunc().(Name).getId() in ["Path", "open"] or
        c.getFunc().(Attribute).getName() in [
          "read_bytes", "read_text", "write_bytes", "write_text",
          "exists", "is_file", "is_dir", "stat",
          "mkdir", "unlink", "rename"
        ]
      ) and
      this.asExpr() = c.getAnArg()
    )
    or
    // Path / operator (pathlib division for path joining)
    exists(BinaryExpr be |
      be.getOp() instanceof Div and
      this.asExpr() = be.getRight()
    )
  }
}

/**
 * The resolve_profile_path() function and Path.resolve() + is_file()
 * are the approved sanitizers.
 */
class ProfilePathSanitizer extends DataFlow::Node {
  ProfilePathSanitizer() {
    exists(Call c |
      (
        c.getFunc().(Name).getId() = "resolve_profile_path" or
        c.getFunc().(Attribute).getName() = "resolve_profile_path"
      ) and
      this.asExpr() = c
    )
  }
}

module ProfilePathConfig implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    source instanceof ProfilePathSource
  }

  predicate isSink(DataFlow::Node sink) {
    sink instanceof FileSystemSink and
    // Exclude sinks inside approved path validation/listing helpers.
    not exists(Function f |
      f.getName() in ["resolve_profile_path", "list_profiles", "list_files"] and
      sink.asExpr().getScope+() = f
    )
  }

  predicate isBarrier(DataFlow::Node node) {
    node instanceof ProfilePathSanitizer
  }
}

module ProfilePath = TaintTracking::Global<ProfilePathConfig>;
import ProfilePath::PathGraph

from ProfilePath::PathNode source, ProfilePath::PathNode sink
where ProfilePath::flowPath(source, sink)
select sink.getNode(), source, sink,
  "User-controlled profile path from $@ reaches file operation without resolve_profile_path().",
  source.getNode(), "profile path parameter"
