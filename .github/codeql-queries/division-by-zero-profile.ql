/**
 * @name Historical ICC divide-by-zero regression
 * @description Detects reintroductions of profile/CAM-derived divide-by-zero
 *              defects fixed in issues #1406, #1410, #1679, #1782, and #1950.
 *              A finding is suppressed only when a zero/range/finiteness check
 *              on the denominator controls the division.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/division-by-zero-profile
 * @tags security
 *       correctness
 *       external/cwe/cwe-369
 */

import cpp
import semmle.code.cpp.controlflow.Guards

private predicate isIccSource(File f) {
  f.getRelativePath().regexpMatch(".*(?:IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
  not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
}

private predicate sameDenominatorInput(Expr guard, Expr denominator) {
  exists(VariableAccess guardAccess, VariableAccess denominatorAccess |
    guardAccess = guard.getAChild*() and
    (denominatorAccess = denominator or denominatorAccess = denominator.getAChild*()) and
    guardAccess.getTarget() = denominatorAccess.getTarget()
  )
  or
  exists(FieldAccess guardAccess, FieldAccess denominatorAccess |
    guardAccess = guard.getAChild*() and
    (denominatorAccess = denominator or denominatorAccess = denominator.getAChild*()) and
    guardAccess.getTarget() = denominatorAccess.getTarget()
  )
}

private predicate isNumericSafetyCheck(Expr guard) {
  exists(FunctionCall call |
    call = guard.getAChild*() and
    call.getTarget().getName().regexpMatch("(?i)^(isfinite|isnan|isinf|fabs|fabsf|abs)$")
  )
  or
  exists(ComparisonOperation comparison |
    comparison = guard or comparison = guard.getAChild*()
  )
}

/**
 * A guard must dominate the division on one branch. This recognizes both an
 * enclosing positive guard and an early-return rejection. Unlike the retired
 * query, it has no source-line window and therefore recognizes the #1837
 * range check even when cleanup or diagnostics separate it from the division.
 */
private predicate hasControllingDenominatorGuard(DivExpr division) {
  exists(Expr guard |
    guard instanceof GuardCondition and
    guard.getEnclosingFunction() = division.getEnclosingFunction() and
    sameDenominatorInput(guard, division.getRightOperand()) and
    isNumericSafetyCheck(guard) and
    guard.(GuardCondition).controls(division.getBasicBlock(), _)
  )
}

bindingset[namePattern]
private predicate denominatorMentions(Expr denominator, string namePattern) {
  exists(VariableAccess access |
    (access = denominator or access = denominator.getAChild*()) and
    access.getTarget().getName().regexpMatch(namePattern)
  )
  or
  exists(FieldAccess access |
    (access = denominator or access = denominator.getAChild*()) and
    access.getTarget().getName().regexpMatch(namePattern)
  )
}

private predicate historicalDivision(DivExpr division, string issue) {
  division.getEnclosingFunction().getName() = "pushXyzLumToXyz" and
  denominatorMentions(division.getRightOperand(), "XYZLum") and
  issue = "#1406"
  or
  division.getEnclosingFunction().getName() = "icYxy2XYZVector" and
  denominatorMentions(division.getRightOperand(), "xy") and
  issue = "#1410"
  or
  division.getEnclosingFunction().getName() = "calcQuadraticVertex" and
  denominatorMentions(division.getRightOperand(), "denom|[tT]") and
  issue = "#1679"
  or
  division.getEnclosingFunction().getName() = "calcDstBlackPoint" and
  denominatorMentions(division.getRightOperand(), "range|MaxL|MinL") and
  issue = "#1782"
  or
  division.getEnclosingFunction().getName() = "CalcCoefficients" and
  denominatorMentions(division.getRightOperand(), "m_La") and
  issue = "#1950"
}

from DivExpr division, string issue
where
  historicalDivision(division, issue) and
  not hasControllingDenominatorGuard(division) and
  (isIccSource(division.getFile()) or division.getFile().getBaseName() = "case.cpp")
select division,
  "Division matches the ICC defect fixed in " + issue +
  " but is not controlled by a denominator zero/range/finiteness guard (CWE-369)."
