/**
 * @name Unchecked allocation in profile processing
 * @description new, malloc, calloc, and realloc return values must be checked
 *              before use. Malformed ICC profiles can trigger large allocations
 *              that fail, and unchecked NULL dereference crashes the tool.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/unchecked-allocation
 * @tags security
 *       correctness
 *       external/cwe/cwe-252
 *       external/cwe/cwe-476
 */

import cpp

/**
 * A call to a memory allocation function.
 */
class AllocationCall extends FunctionCall {
  AllocationCall() {
    this.getTarget().getName() in [
      "malloc", "calloc", "realloc",
      "icRealloc", "icMalloc"
    ]
  }
}

from AllocationCall call
where
  // The return value is assigned to a variable
  exists(AssignExpr assign |
    assign.getRValue() = call
  ) and
  // But the variable is not checked for NULL before first use
  exists(AssignExpr assign, Variable v |
    assign.getRValue() = call and
    assign.getLValue().(VariableAccess).getTarget() = v and
    exists(VariableAccess use |
      use.getTarget() = v and
      use.getLocation().getStartLine() > assign.getLocation().getStartLine() and
      use.getLocation().getStartLine() < assign.getLocation().getStartLine() + 5 and
      not exists(IfStmt check |
        check.getCondition().getAChild*().(VariableAccess).getTarget() = v and
        check.getLocation().getStartLine() > assign.getLocation().getStartLine() and
        check.getLocation().getStartLine() <= use.getLocation().getStartLine()
      )
    )
  ) and
  // Exclude test files
  not call.getFile().getRelativePath().matches("%test%")
select call,
  "Allocation result from " + call.getTarget().getName() +
  "() used without NULL check. Malformed profiles can trigger allocation failure (CWE-252/CWE-476)."
