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
    (denomAccess = denom or denomAccess = denom.getAChild*()) and
    guardAccess.getTarget() = denomAccess.getTarget()
  )
  or
  exists(VariableAccess guardAccess, VariableAccess denomAccess |
    guardAccess = guard.getAChild*() and
    (denomAccess = denom or denomAccess = denom.getAChild*()) and
    guardAccess.getTarget() = denomAccess.getTarget()
  )
}

private predicate isZeroOrFiniteCheck(Expr guard) {
  exists(FunctionCall call |
    (call = guard or call = guard.getAChild*()) and
    call.getTarget().getName().regexpMatch("(?i)^(isfinite|isnan|isinf)$")
  )
  or
  exists(ComparisonOperation cmp, Literal lit |
    (cmp = guard or cmp = guard.getAChild*()) and
    cmp.hasOperands(_, lit) and
    lit.toString().regexpMatch("(?i)^-?(0(\\.0f?)?|1e-[0-9]+|1E-[0-9]+)$")
  )
  or
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

private predicate isOneLiteral(Expr e) {
  e instanceof Literal and
  e.toString().regexpMatch("(?i)^1([uUlL]*)?$")
}

private predicate isZeroLiteral(Expr e) {
  e instanceof Literal and
  e.toString().regexpMatch("(?i)^-?0(\\.0f?)?$")
}

private predicate denominatorSubtractsOne(Expr denom) {
  exists(SubExpr sub |
    (sub = denom or sub = denom.getAChild*()) and
    isOneLiteral(sub.getRightOperand())
  )
}

private predicate isZeroOrFiniteCheckForDenominator(Expr guard, Expr denom) {
  isZeroOrFiniteCheck(guard)
  or
  (
    denominatorSubtractsOne(denom) and
    exists(ComparisonOperation cmp, Expr lit |
      (cmp = guard or cmp = guard.getAChild*()) and
      cmp.hasOperands(_, lit) and
      isOneLiteral(lit)
    )
  )
}

private predicate hasNearbyZeroOrFiniteCheckForDenominator(DivExpr div) {
  exists(IfStmt check |
    check.getEnclosingFunction() = div.getEnclosingFunction() and
    check.getLocation().getStartLine() <= div.getLocation().getStartLine() and
    check.getLocation().getStartLine() >= div.getLocation().getStartLine() - 12 and
    mentionsSameDenominator(check.getCondition(), div.getRightOperand()) and
    isZeroOrFiniteCheckForDenominator(check.getCondition(), div.getRightOperand())
  )
}

private predicate denominatorVariable(Expr denom, Variable v) {
  exists(VariableAccess access |
    (access = denom or access = denom.getAChild*()) and
    access.getTarget() = v
  )
}

private predicate returnFailsClosed(ReturnStmt ret) {
  ret.getExpr().toString().regexpMatch("(?i)^(false|0|null|nullptr)$")
}

private predicate failClosedBranch(IfStmt check) {
  exists(ReturnStmt ret |
    (ret = check.getThen() or ret = check.getThen().getAChild*()) and
    returnFailsClosed(ret)
  )
}

private predicate reassignedBetween(IfStmt check, DivExpr div, Variable v) {
  exists(AssignExpr assign, VariableAccess access |
    assign.getEnclosingFunction() = div.getEnclosingFunction() and
    access = assign.getLValue().getAChild*() and
    access.getTarget() = v and
    assign.getLocation().getStartLine() > check.getLocation().getStartLine() and
    assign.getLocation().getStartLine() < div.getLocation().getStartLine()
  )
}

private predicate hasPriorFailClosedZeroOrFiniteCheckForDenominator(DivExpr div) {
  exists(IfStmt check, Variable v |
    check.getEnclosingFunction() = div.getEnclosingFunction() and
    check.getLocation().getStartLine() < div.getLocation().getStartLine() and
    mentionsSameDenominator(check.getCondition(), div.getRightOperand()) and
    isZeroOrFiniteCheckForDenominator(check.getCondition(), div.getRightOperand()) and
    failClosedBranch(check) and
    denominatorVariable(div.getRightOperand(), v) and
    not reassignedBetween(check, div, v)
  )
}

private predicate conditionTrueMeansZero(Expr condition, Expr denom) {
  exists(EQExpr eq, Expr lit |
    (eq = condition or eq = condition.getAChild*()) and
    eq.hasOperands(_, lit) and
    isZeroLiteral(lit) and
    mentionsSameDenominator(eq, denom)
  )
}

private predicate conditionTrueMeansNonZero(Expr condition, Expr denom) {
  exists(NEExpr ne, Expr lit |
    (ne = condition or ne = condition.getAChild*()) and
    ne.hasOperands(_, lit) and
    isZeroLiteral(lit) and
    mentionsSameDenominator(ne, denom)
  )
}

private predicate hasConditionalZeroGuardForDenominator(DivExpr div) {
  exists(ConditionalExpr conditional |
    (
      (div = conditional.getElse() or div = conditional.getElse().getAChild*()) and
      isZeroLiteral(conditional.getThen()) and
      conditionTrueMeansZero(conditional.getCondition(), div.getRightOperand())
    )
    or
    (
      (div = conditional.getThen() or div = conditional.getThen().getAChild*()) and
      isZeroLiteral(conditional.getElse()) and
      conditionTrueMeansNonZero(conditional.getCondition(), div.getRightOperand())
    )
  )
}

from DivExpr div
where
  not div.getRightOperand() instanceof Literal and
  isProfileDerivedDenominator(div.getRightOperand()) and
  not hasNearbyZeroOrFiniteCheckForDenominator(div) and
  not hasPriorFailClosedZeroOrFiniteCheckForDenominator(div) and
  not hasConditionalZeroGuardForDenominator(div) and
  isIccSource(div.getFile())
select div,
  "Division by '" + div.getRightOperand().toString() +
  "' may use a zero or non-finite profile/CAM-derived denominator. Add explicit finite and non-zero validation (CWE-369)."
