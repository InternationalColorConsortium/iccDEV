/**
 * @name Division without zero check on profile data
 * @description Division operations where the divisor comes from ICC profile
 *              fields or derived color-appearance state without a zero or
 *              finite check. Malformed profiles can drive these values to
 *              zero, NaN, or infinity.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/division-by-zero-profile
 * @tags security
 *       correctness
 *       external/cwe/cwe-369
 */

import cpp

private predicate isIccSource(File f) {
  f.getRelativePath().regexpMatch(".*(?:IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
  not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
}

bindingset[name]
private predicate isProfileDenominatorName(string name) {
  name.regexpMatch(
    "(?i).*(steps|count|num|size|channels|nInput|nOutput|denom|divisor|scale|range|" +
    "m_n|m_AWhite|AWhite|m_Fl|m_Nbb|m_factor|m_c|m_z).*"
  )
}

private predicate isProfileDerivedDenominator(Expr e) {
  exists(FieldAccess fa |
    fa = e.getAChild*() and
    isProfileDenominatorName(fa.getTarget().getName())
  )
  or
  exists(VariableAccess va |
    va = e.getAChild*() and
    isProfileDenominatorName(va.getTarget().getName())
  )
}

private predicate isGuardCondition(Expr e) {
  e.toString().regexpMatch("(?i).*(validCam|safe|checked).*")
  or
  exists(VariableAccess va |
    va = e.getAChild*() and
    va.getTarget().getName().regexpMatch("(?i).*(validCam|safe|checked).*")
  )
}

private predicate hasNearbyZeroOrFiniteCheck(DivExpr div) {
  exists(IfStmt check |
    check.getEnclosingFunction() = div.getEnclosingFunction() and
    check.getLocation().getStartLine() <= div.getLocation().getStartLine() and
    (
      isGuardCondition(check.getCondition()) or
      (
        check.getLocation().getStartLine() >= div.getLocation().getStartLine() - 8 and
        (
          check.getCondition().toString().regexpMatch("(?i).*((==|!=|<=|>=|<|>).*(0|0\\.0|0\\.0f)).*") or
          check.getCondition().toString().regexpMatch("(?i).*(isfinite|isnan|isinf).*")
        ) and
        isProfileDerivedDenominator(check.getCondition())
      )
    )
  )
}

from DivExpr div
where
  not div.getRightOperand() instanceof Literal and
  isProfileDerivedDenominator(div.getRightOperand()) and
  not hasNearbyZeroOrFiniteCheck(div) and
  isIccSource(div.getFile())
select div,
  "Division by '" + div.getRightOperand().toString() +
  "' may use a zero or non-finite profile/CAM-derived denominator. Add explicit finite and non-zero validation (CWE-369)."
