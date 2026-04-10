/**
 * @name Multiplication overflow before allocation
 * @description Two integer values multiplied together before being passed to
 *              new[], malloc, calloc, or SetSize() can silently overflow,
 *              causing an undersized allocation and subsequent buffer overflow.
 *              Common pattern in ICC: nInputChannels * nOutputChannels for
 *              matrix sizes, nCurves * nEntries for LUT data.
 * @kind problem
 * @problem.severity error
 * @security-severity 8.0
 * @precision medium
 * @id iccdev/multiplication-overflow-alloc
 * @tags security
 *       external/cwe/cwe-190
 *       external/cwe/cwe-680
 *       external/cwe/cwe-787
 */

import cpp

/**
 * An allocation expression: new[], malloc, calloc, realloc.
 */
class AllocationExpr extends Expr {
  AllocationExpr() {
    this instanceof NewArrayExpr
    or
    exists(FunctionCall fc | fc = this |
      fc.getTarget().getName() = "malloc" or
      fc.getTarget().getName() = "calloc" or
      fc.getTarget().getName() = "realloc" or
      fc.getTarget().getName() = "icRealloc"
    )
  }
}

/**
 * A call to SetSize, resize, or reserve with a multiplication argument.
 */
class SizeSettingCall extends FunctionCall {
  SizeSettingCall() {
    this.getTarget().getName() = "SetSize" or
    this.getTarget().getName() = "resize" or
    this.getTarget().getName() = "reserve"
  }
}

/**
 * A multiplication expression involving two non-constant operands.
 */
class UncheckedMultiply extends MulExpr {
  UncheckedMultiply() {
    not this.getLeftOperand().isConstant() and
    not this.getRightOperand().isConstant()
  }
}

from UncheckedMultiply mul
where
  (
    exists(AllocationExpr alloc |
      alloc.getAChild*() = mul
    )
    or
    exists(SizeSettingCall sizeCall |
      sizeCall.getAnArgument().getAChild*() = mul
    )
  ) and
  mul.getType().getSize() <= 4 and
  mul.getFile().getRelativePath().matches("%Icc%")
select mul,
  "Two non-constant values are multiplied in a 32-bit context before " +
  "allocation or SetSize(). If both values come from untrusted input " +
  "(e.g., nInputChannels * nOutputChannels), the product can silently " +
  "wrap, causing undersized allocation (CWE-190, CWE-680, CWE-787). " +
  "Widen to icUInt64Number before multiplying, then check against UINT32_MAX."
