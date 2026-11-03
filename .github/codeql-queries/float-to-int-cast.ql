/**
 * @name Historical ICC float-to-integer conversion regression
 * @description Detects reintroductions of float-to-integer undefined behavior
 *              fixed in ICC visualization and spectral resampling. The query
 *              requires a preceding finiteness check and range validation on
 *              the same value, rather than relying on a source-line window.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/float-to-int-cast
 * @tags security
 *       correctness
 *       external/cwe/cwe-681
 */

import cpp

private predicate isProductSource(File f) {
  (
    f.getRelativePath().regexpMatch(".*(?:IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
    not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
  )
  or
  f.getBaseName() = "case.cpp"
}

private predicate convertedVariable(CStyleCast cast, Variable variable) {
  exists(VariableAccess access |
    (access = cast.getExpr() or access = cast.getExpr().getAChild*()) and
    access.getTarget() = variable
  )
}

private predicate isHistoricalConversion(CStyleCast cast, Variable variable, string issue) {
  convertedVariable(cast, variable) and
  (
    cast.getEnclosingFunction().getName() = ["ClipU8", "ClipU16"] and
    variable.getName() = "input" and
    issue = "#1396"
    or
    exists(FunctionCall roundCall |
      (roundCall = cast.getExpr() or roundCall = cast.getExpr().getAChild*()) and
      roundCall.getTarget().getName().regexpMatch("(?i)^(round|lround)$") and
      variable.getName() = "valueTemp" and
      issue = "#1396"
    )
    or
    cast.getEnclosingFunction().getName() = "resampleCore" and
    variable.getName() = "t" and
    issue = "#2230"
  )
}

private predicate callChecksVariable(FunctionCall call, Variable variable) {
  call.getTarget().getName().regexpMatch(
    "(?i)^(isfinite|isnan|isinf|fpclassify|__builtin_isnan|__builtin_isfinite)$"
  ) and
  exists(VariableAccess access |
    access = call.getArgument(0).getAChild*() and
    access.getTarget() = variable
  )
}

private predicate comparisonChecksVariable(ComparisonOperation comparison, Variable variable) {
  exists(VariableAccess access |
    access = comparison.getAChild*() and
    access.getTarget() = variable
  )
}

/**
 * The checks may return, clamp, or continue. Requiring them to precede the cast
 * keeps the rule compatible with all three established ICC hardening idioms.
 * Two distinct comparisons establish both sides of the permitted range.
 */
private predicate hasFinitenessAndRangeChecks(CStyleCast cast, Variable variable) {
  exists(FunctionCall finiteCheck |
    finiteCheck.getEnclosingFunction() = cast.getEnclosingFunction() and
    finiteCheck.getLocation().getStartLine() < cast.getLocation().getStartLine() and
    callChecksVariable(finiteCheck, variable)
  ) and
  count(ComparisonOperation rangeCheck |
    rangeCheck.getEnclosingFunction() = cast.getEnclosingFunction() and
    rangeCheck.getLocation().getStartLine() < cast.getLocation().getStartLine() and
    comparisonChecksVariable(rangeCheck, variable)
  |
    rangeCheck
  ) >= 2
}

from CStyleCast cast, Variable variable, string issue
where
  cast.getExpr().getType().getUnspecifiedType() instanceof FloatingPointType and
  cast.getType().getUnspecifiedType() instanceof IntegralType and
  isHistoricalConversion(cast, variable, issue) and
  not hasFinitenessAndRangeChecks(cast, variable) and
  isProductSource(cast.getFile())
select cast,
  "Float-to-integer conversion matches the ICC defect fixed in " + issue +
  " but lacks preceding finiteness and two-sided range checks (CWE-681)."
