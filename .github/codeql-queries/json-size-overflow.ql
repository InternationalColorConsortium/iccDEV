/**
 * @name JSON array size cast to narrow integer without bounds check
 * @description nlohmann::json .size() returns size_t (64-bit on LP64). Casting
 *              directly to icUInt32Number, int, or icUInt16Number truncates
 *              the value when a malicious JSON array has more than 2^32 or
 *              2^16 elements, causing undersized allocations and buffer
 *              overflows. Use a bounds check or safe conversion helper.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/json-size-overflow
 * @tags security
 *       external/cwe/cwe-680
 *       external/cwe/cwe-190
 */

import cpp

/**
 * A call to .size() on a nlohmann::json object.
 */
class JsonSizeCall extends FunctionCall {
  JsonSizeCall() {
    this.getTarget().getName() = "size" and
    this.getFile().getRelativePath().matches("%IccJSON%")
  }
}

/**
 * A cast expression that narrows the result of .size().
 */
class NarrowingCast extends Cast {
  NarrowingCast() {
    this.getType().getSize() < 8 and
    exists(JsonSizeCall sizeCall |
      this.getExpr() = sizeCall or
      this.getExpr().(Expr).getAChild*() = sizeCall
    )
  }
}

from NarrowingCast cast, JsonSizeCall sizeCall
where
  cast.getExpr() = sizeCall or
  cast.getExpr().(Expr).getAChild*() = sizeCall
select cast,
  "JSON .size() result (size_t, 64-bit) is narrowed to a " +
  cast.getType().getName() +
  " without bounds checking. A malicious JSON array could overflow the " +
  "target type, causing undersized allocation (CWE-680, CWE-190). " +
  "Use icJsonSafeU32() or explicit range check."
