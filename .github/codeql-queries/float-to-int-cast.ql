/**
 * @name Float-to-integer cast without range check
 * @description Casting float or double to integer types without checking for
 *              NaN, infinity, or out-of-range values is undefined behavior.
 *              ICC profiles contain s15Fixed16 and float data that can be NaN
 *              after malformed profile processing.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/float-to-int-cast
 * @tags security
 *       correctness
 *       external/cwe/cwe-681
 */

import cpp

from CStyleCast cast
where
  // Cast from floating point to integer
  cast.getExpr().getType().getUnspecifiedType() instanceof FloatingPointType and
  cast.getType().getUnspecifiedType() instanceof IntegralType and
  // Not in a context that is already guarded by isnan/isinf/isfinite
  not exists(FunctionCall guard |
    guard.getTarget().getName().regexpMatch("(?i)(isnan|isinf|isfinite|fpclassify|__builtin_isnan)") and
    guard.getLocation().getStartLine() >= cast.getLocation().getStartLine() - 5 and
    guard.getLocation().getStartLine() <= cast.getLocation().getStartLine()
  ) and
  // Not a trivial constant cast (e.g., (int)0.0)
  not cast.getExpr() instanceof Literal and
  // In source files (not system headers)
  cast.getFile().getRelativePath().matches("%Icc%") and
  // Exclude test code
  not cast.getFile().getRelativePath().matches("%test%")
select cast,
  "Float-to-integer cast without NaN/range check. " +
  "Malformed ICC profiles can produce NaN values that cause undefined behavior on integer cast (CWE-681)."
