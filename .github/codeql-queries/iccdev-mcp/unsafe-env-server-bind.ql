/**
 * @name Unsafe environment variable use in server binding
 * @description Detects environment variables (HOST, PORT, BIND_ADDRESS) flowing
 *              to network bind operations without validation. The iccdev-mcp
 *              REST API server (rest_api.py) may use env vars for host/port
 *              configuration. Unvalidated values could expose the service on
 *              unintended interfaces or cause crashes from non-numeric ports.
 * @kind path-problem
 * @problem.severity warning
 * @id iccdev-mcp/unsafe-env-server-bind
 * @tags security environment-variable server-configuration iccdev
 * @cwe CWE-15
 */

import python
import semmle.python.dataflow.new.DataFlow
import semmle.python.dataflow.new.TaintTracking

/**
 * os.environ.get() or os.getenv() for server config variables.
 */
class EnvVarSource extends DataFlow::Node {
  EnvVarSource() {
    exists(Call c, StringLiteral s |
      (
        // os.environ.get("HOST")
        (c.getFunc().(Attribute).getName() = "get" and
         c.getFunc().(Attribute).getObject().(Attribute).getName() = "environ")
        or
        // os.getenv("HOST")
        c.getFunc().(Attribute).getName() = "getenv"
      ) and
      s.getText() in [
        "HOST", "PORT", "BIND_ADDRESS",
        "ICCDEV_MCP_HOST", "ICCDEV_MCP_PORT"
      ] and
      c.getAnArg() = s and
      this.asExpr() = c
    )
  }
}

/**
 * Server binding sinks: uvicorn.run(), socket.bind(), etc.
 */
class ServerBindSink extends DataFlow::Node {
  ServerBindSink() {
    exists(Call c |
      c.getFunc().(Attribute).getName() in [
        "run", "bind", "listen", "serve"
      ] and
      this.asExpr() = c.getAnArg()
    )
  }
}

/**
 * Validation barriers: int() cast, explicit validation functions.
 */
class EnvValidation extends DataFlow::Node {
  EnvValidation() {
    exists(Call c |
      c.getFunc().(Name).getId() in [
        "int", "validate_port", "validate_host"
      ] and
      this.asExpr() = c
    )
  }
}

module EnvBindConfig implements DataFlow::ConfigSig {
  predicate isSource(DataFlow::Node source) {
    source instanceof EnvVarSource
  }

  predicate isSink(DataFlow::Node sink) {
    sink instanceof ServerBindSink
  }

  predicate isBarrier(DataFlow::Node node) {
    node instanceof EnvValidation
  }
}

module EnvBind = TaintTracking::Global<EnvBindConfig>;
import EnvBind::PathGraph

from EnvBind::PathNode source, EnvBind::PathNode sink
where EnvBind::flowPath(source, sink)
select sink.getNode(), source, sink,
  "Environment variable $@ flows to server bind without explicit validation.",
  source.getNode(), "env var"
