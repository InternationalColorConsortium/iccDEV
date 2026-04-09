/**
 * @name Division without zero check on profile data
 * @description Division operations where the divisor comes from ICC profile
 *              fields (steps, count, channel count) without a zero check.
 *              Malformed profiles can have zero values in these fields.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/division-by-zero-profile
 * @tags security
 *       correctness
 *       external/cwe/cwe-369
 */

import cpp

from DivExpr div
where
  // The divisor is not a constant
  not div.getRightOperand() instanceof Literal and
  // The divisor involves a field access or variable that could be profile-derived
  (
    exists(FieldAccess fa |
      fa = div.getRightOperand().getAChild*() and
      fa.getTarget().getName().regexpMatch("(?i).*(steps|count|num|size|channels|nInput|nOutput|m_n|denom).*")
    )
    or
    exists(VariableAccess va |
      va = div.getRightOperand() and
      va.getTarget().getName().regexpMatch("(?i).*(steps|count|num|size|channels|nInput|nOutput|denom|divisor).*")
    )
  ) and
  // No zero check in the preceding 5 lines
  not exists(IfStmt check |
    check.getLocation().getStartLine() >= div.getLocation().getStartLine() - 5 and
    check.getLocation().getStartLine() <= div.getLocation().getStartLine() and
    (
      check.getCondition().toString().matches("%== 0%") or
      check.getCondition().toString().matches("%!= 0%") or
      check.getCondition().toString().matches("%> 0%") or
      check.getCondition().toString().matches("%< 1%")
    )
  ) and
  // In ICC source files
  div.getFile().getRelativePath().regexpMatch(".*(?:Icc|icc).*") and
  not div.getFile().getRelativePath().matches("%test%")
select div,
  "Division where divisor may be zero from malformed profile data. Add a zero check (CWE-369)."
