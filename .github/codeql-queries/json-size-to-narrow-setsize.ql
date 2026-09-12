/**
 * @name JSON-derived count passed to narrow SetSize parameter
 * @description A JSON array count safely converted to icUInt32Number still
 *              overflows when implicitly converted to a narrower SetSize()
 *              parameter. The undersized allocation is then indexed using
 *              the original count, causing a buffer overflow.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/json-size-to-narrow-setsize
 * @tags security
 *       external/cwe/cwe-680
 *       external/cwe/cwe-190
 */

import cpp

class JsonSizeConversion extends FunctionCall {
  JsonSizeConversion() {
    this.getTarget().getName() = "icJsonSafeU32" and
    exists(FunctionCall sizeCall |
      sizeCall = this.getArgument(0) and
      sizeCall.getTarget().getName() = "size"
    )
  }
}

predicate hasPriorNarrowingGuard(FunctionCall setSize, Variable sizeValue) {
  exists(IfStmt guard, Expr limit |
    guard.getEnclosingFunction() = setSize.getEnclosingFunction() and
    guard.getLocation().getStartLine() < setSize.getLocation().getStartLine() and
    guard.getCondition().getAChild*().(VariableAccess).getTarget() = sizeValue and
    limit = guard.getCondition().getAChild*() and
    limit.getValue() in ["65535", "65535U", "0xffff", "0xFFFF", "UINT16_MAX"]
  )
}

from JsonSizeConversion conversion, Variable sizeValue, FunctionCall setSize
where
  sizeValue.getInitializer().getExpr() = conversion and
  setSize.getTarget().getName() = "SetSize" and
  setSize.getNumberOfArguments() > 0 and
  setSize.getArgument(0).(VariableAccess).getTarget() = sizeValue and
  setSize.getArgument(0).getType().getSize() >
    setSize.getTarget().getParameter(0).getType().getSize() and
  not hasPriorNarrowingGuard(setSize, sizeValue)
select setSize,
  "JSON array count $@ is implicitly narrowed by SetSize() from " +
  sizeValue.getType().getName() + " to " +
  setSize.getTarget().getParameter(0).getType().getName() +
  ". Reject values above the SetSize() parameter maximum before allocation.",
  conversion, "derived from JSON .size()"
