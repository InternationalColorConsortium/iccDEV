/**
 * @name Unsafe cast of profile header fields
 * @description Direct casts of icUInt32Number header fields (size, offset,
 *              count) to signed int without INT_MAX guards can produce
 *              negative values for malformed profiles, breaking comparisons
 *              and enabling integer overflow exploits.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/unsafe-header-cast
 * @tags security
 *       correctness
 *       external/cwe/cwe-681
 */

import cpp

/**
 * The cast is the bounded branch of a conditional expression that compares the
 * same field and substitutes INT_MAX on the other branch.
 */
predicate boundedByIntMax(CStyleCast cast, Field field) {
  exists(
    ConditionalExpr conditional, RelationalOperation comparison,
    Literal comparisonBound, Literal fallback
  |
    conditional.getCondition().getAChild*() = comparison and
    comparison.getAChild*().(FieldAccess).getTarget() = field and
    comparison.getAChild*() = comparisonBound and
    comparisonBound.getValue().regexpMatch("(?i)(2147483647|0x7fffffff)[lL]?") and
    (
      conditional.getThen().getAChild*() = cast.getExpr() and
      conditional.getElse().getAChild*() = fallback
      or
      conditional.getElse().getAChild*() = cast.getExpr() and
      conditional.getThen().getAChild*() = fallback
    ) and
    fallback.getValue().regexpMatch("(?i)(2147483647|0x7fffffff)[lL]?")
  )
}

from CStyleCast cast, FieldAccess fieldAccess
where
  // Cast target is signed int
  cast.getType().getName() = "int" and
  // Source is an unsigned 32-bit field access
  cast.getExpr().getType().getUnspecifiedType().(IntegralType).isUnsigned() and
  cast.getExpr().getType().getSize() = 4 and
  // The expression involves a member access (field read from a struct)
  fieldAccess = cast.getExpr().getAChild*() and
  fieldAccess.getTarget().getName() in [
    "size", "offset", "count",
    "nCount", "m_nCount", "nSize"
  ] and
  // Not already guarded by a ternary with INT_MAX
  not boundedByIntMax(cast, fieldAccess.getTarget()) and
  // Exclude header files (type definitions)
  not cast.getFile().getExtension() = "h"
select cast,
  "Unsafe cast of unsigned profile field to signed int without INT_MAX guard. " +
  "Malformed profiles with values > 0x7FFFFFFF will produce negative results (CWE-681)."
