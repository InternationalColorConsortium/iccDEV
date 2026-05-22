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
private predicate isProfileFieldDenominatorName(string name) {
  name.regexpMatch(
    "(?i)^(m_n.*|.*Steps|.*Count|.*Size|.*Channels|.*nInput.*|.*nOutput.*|" +
    ".*Denom.*|.*Divisor.*|.*Scale.*|.*Range.*|m_AWhite|AWhite|m_Fl|m_Nbb|" +
    "m_factor|m_c|m_cc|m_z|m_x0)$"
  )
}

bindingset[name]
private predicate isProfileLocalDenominatorName(string name) {
  name.regexpMatch(
    "(?i)^(denom.*|.*Denom.*|divisor.*|.*Divisor.*|stepSize|.*Steps|" +
    ".*Scale|.*Range|.*Channels|nInput.*|nOutput.*|n(Type|Vector|Device|Pcs|Spectral)Size)$"
  )
}

private predicate isProfileDerivedDenominator(Expr e) {
  exists(FieldAccess fa |
    fa = e.getAChild*() and
    isProfileFieldDenominatorName(fa.getTarget().getName())
  )
  or
  exists(VariableAccess va |
    va = e.getAChild*() and
    isProfileLocalDenominatorName(va.getTarget().getName())
  )
}

private predicate mentionsSameDenominator(Expr guard, Expr denom) {
  exists(FieldAccess guardAccess, FieldAccess denomAccess |
    guardAccess = guard.getAChild*() and
    denomAccess = denom.getAChild*() and
    guardAccess.getTarget() = denomAccess.getTarget()
  )
  or
  exists(VariableAccess guardAccess, VariableAccess denomAccess |
    guardAccess = guard.getAChild*() and
    denomAccess = denom.getAChild*() and
    guardAccess.getTarget() = denomAccess.getTarget()
  )
}

private predicate isZeroOrFiniteCheck(Expr guard) {
  guard.toString().regexpMatch("(?i).*(isfinite|isnan|isinf).*")
  or
  guard.toString().regexpMatch(
    "(?i).*((==|!=|<=|>=|<|>)\\s*(-?0(\\.0f?)?|1e-[0-9]+|1E-[0-9]+)([^0-9.]|$)).*"
  )
  or
  guard.toString().regexpMatch(
    "(?i).*(fabs|std::fabs|abs|std::abs).*((<|<=)\\s*(1e-[0-9]+|1E-[0-9]+|-?0(\\.0f?)?)([^0-9.]|$)).*"
  )
}

private predicate hasNearbyZeroOrFiniteCheckForDenominator(DivExpr div) {
  exists(IfStmt check |
    check.getEnclosingFunction() = div.getEnclosingFunction() and
    check.getLocation().getStartLine() <= div.getLocation().getStartLine() and
    check.getLocation().getStartLine() >= div.getLocation().getStartLine() - 12 and
    mentionsSameDenominator(check.getCondition(), div.getRightOperand()) and
    isZeroOrFiniteCheck(check.getCondition())
  )
}

from DivExpr div
where
  not div.getRightOperand() instanceof Literal and
  isProfileDerivedDenominator(div.getRightOperand()) and
  not hasNearbyZeroOrFiniteCheckForDenominator(div) and
  isIccSource(div.getFile())
select div,
  "Division by '" + div.getRightOperand().toString() +
  "' may use a zero or non-finite profile/CAM-derived denominator. Add explicit finite and non-zero validation (CWE-369)."
