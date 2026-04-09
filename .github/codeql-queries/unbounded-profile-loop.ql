/**
 * @name Unbounded loop driven by profile field
 * @description Loops whose iteration count is directly controlled by an
 *              untrusted ICC profile field (tag count, element count, size)
 *              without an upper bound check can cause denial of service via
 *              excessive iteration or memory exhaustion.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/unbounded-profile-loop
 * @tags security
 *       correctness
 *       external/cwe/cwe-400
 *       external/cwe/cwe-834
 */

import cpp

from ForStmt loop, FieldAccess fa
where
  // The loop condition compares against a field access
  fa = loop.getCondition().getAChild*() and
  // The field name suggests a count/size from profile data
  fa.getTarget().getName().regexpMatch("(?i).*(count|num|size|length|steps|nInput|nOutput|m_n).*") and
  // The field is from a class/struct (not a local)
  exists(fa.getTarget().getDeclaringType()) and
  // No bounds check before the loop (simple heuristic: no if/min in the 3 lines before)
  not exists(IfStmt guard |
    guard.getLocation().getStartLine() >= loop.getLocation().getStartLine() - 5 and
    guard.getLocation().getStartLine() < loop.getLocation().getStartLine() and
    guard.getCondition().getAChild*().toString().regexpMatch(".*(?i)(max|limit|bound|INT_MAX).*")
  ) and
  not exists(FunctionCall minCall |
    minCall.getTarget().getName().matches("%min%") and
    minCall.getLocation().getStartLine() >= loop.getLocation().getStartLine() - 3 and
    minCall.getLocation().getStartLine() <= loop.getLocation().getStartLine()
  ) and
  // Exclude test code
  not loop.getFile().getRelativePath().matches("%test%")
select loop,
  "Loop iteration count controlled by profile field '" +
  fa.getTarget().getName() +
  "' without upper bound validation. Malformed profiles can cause DoS (CWE-400/CWE-834)."
