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

from CStyleCast cast
where
  // Cast target is signed int
  cast.getType().getName() = "int" and
  // Source is an unsigned 32-bit field access
  cast.getExpr().getType().getUnspecifiedType().(IntegralType).isUnsigned() and
  cast.getExpr().getType().getSize() = 4 and
  // The expression involves a member access (field read from a struct)
  exists(FieldAccess fa |
    fa = cast.getExpr().getAChild*() and
    fa.getTarget().getName() in [
      "size", "offset", "count",
      "nCount", "m_nCount", "nSize"
    ]
  ) and
  // Not already guarded by a ternary with INT_MAX
  not exists(ConditionalExpr cond |
    cond.getAChild*() = cast and
    cond.toString().matches("%INT_MAX%")
  ) and
  // Exclude header files (type definitions)
  not cast.getFile().getExtension() = "h"
select cast,
  "Unsafe cast of unsigned profile field to signed int without INT_MAX guard. " +
  "Malformed profiles with values > 0x7FFFFFFF will produce negative results (CWE-681)."
