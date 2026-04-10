/**
 * @name Recursive JSON structure without depth limit
 * @description ICC profiles serialized to JSON can contain nested structures
 *              (MultiProcessElements containing sub-elements, CalculatorOps
 *              containing nested ops). When parsing such JSON without a depth
 *              limit, a malicious file can cause stack overflow via deep
 *              nesting. Recursive Parse/ToJson/FromJson functions should
 *              track and limit recursion depth.
 * @kind problem
 * @problem.severity warning
 * @precision low
 * @id iccdev/json-recursive-parse
 * @tags security
 *       external/cwe/cwe-674
 *       external/cwe/cwe-400
 */

import cpp

/**
 * A function that parses JSON and calls itself recursively.
 */
class RecursiveJsonParser extends Function {
  RecursiveJsonParser() {
    (
      this.getName().matches("%ParseJson%") or
      this.getName().matches("%FromJson%") or
      this.getName().matches("%LoadJson%") or
      this.getName().matches("%ToJson%")
    ) and
    exists(FunctionCall selfCall |
      selfCall.getTarget() = this and
      selfCall.getEnclosingFunction() = this
    )
  }
}

/**
 * Holds when the function has a depth or level parameter.
 */
predicate hasDepthParam(Function f) {
  exists(Parameter p |
    p = f.getAParameter() and
    (
      p.getName().toLowerCase().matches("%depth%") or
      p.getName().toLowerCase().matches("%level%") or
      p.getName().toLowerCase().matches("%recursi%")
    )
  )
}

from RecursiveJsonParser parser
where not hasDepthParam(parser)
select parser,
  "Recursive JSON parser '" + parser.getName() +
  "' has no depth-limiting parameter. A maliciously nested JSON structure " +
  "can cause stack overflow (CWE-674). Add a depth parameter with a " +
  "reasonable limit (e.g., 64 levels)."
