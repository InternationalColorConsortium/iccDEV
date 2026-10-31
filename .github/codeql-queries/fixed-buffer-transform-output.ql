/**
 * @name Fixed buffer used as transform output
 * @description A fixed-size local buffer is forwarded through one or more
 *              wrappers to CIccCmm::Apply() without a recognized literal
 *              destination-capacity guard. Forwarded capacity parameters
 *              require manual review. This is the stack-buffer-overflow
 *              shape from issues #2868 and #2886.
 * @kind problem
 * @problem.severity error
 * @security-severity 9.1
 * @precision medium
 * @id iccdev/fixed-buffer-transform-output
 * @tags security
 *       correctness
 *       external/cwe/cwe-121
 *       external/cwe/cwe-787
 */

import cpp
import semmle.code.cpp.controlflow.Guards

private predicate isProductSource(File f) {
  f.getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
  not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
  or
  f.getBaseName() = "case.cpp"
}

private predicate isCmmApplyCall(FunctionCall call) {
  call.getTarget().getName() = "Apply" and
  call.getTarget().getDeclaringType().getName() = "CIccCmm"
}

private predicate isMatchingDestSamplesCall(FunctionCall destSamples, FunctionCall apply) {
  destSamples.getTarget().getName() = "GetDestSamples" and
  destSamples.getTarget().getDeclaringType().getName() = "CIccCmm" and
  destSamples.getQualifier().(VariableAccess).getTarget() =
    apply.getQualifier().(VariableAccess).getTarget()
}

private predicate hasPriorCapacityGuard(FunctionCall apply, FunctionCall incoming, ArrayType array) {
  exists(ComparisonOperation guard, FunctionCall destSamples |
    guard.getEnclosingFunction() = apply.getEnclosingFunction() and
    guard instanceof GuardCondition and
    guard.getOperator() = ">" and
    isMatchingDestSamplesCall(destSamples, apply) and
    guard.getLeftOperand() = destSamples and
    (
      guard.getRightOperand().getValue() = array.getArraySize().toString()
      or
      exists(Parameter sizeParameter |
        sizeParameter.getFunction() = apply.getEnclosingFunction() and
        guard.getRightOperand() = sizeParameter.getAnAccess() and
        incoming.getTarget() = sizeParameter.getFunction() and
        incoming.getArgument(sizeParameter.getIndex()).getValue() = array.getArraySize().toString()
      )
    ) and
    guard.(GuardCondition).controls(apply.getBasicBlock(), false)
  )
}

private predicate reachesUnguardedCmmApply(FunctionCall incoming, Parameter output, ArrayType array) {
  incoming.getTarget() = output.getFunction() and
  (
    exists(FunctionCall apply |
      apply.getEnclosingFunction() = output.getFunction() and
      isCmmApplyCall(apply) and
      apply.getArgument(0).getAChild*() = output.getAnAccess() and
      not hasPriorCapacityGuard(apply, incoming, array)
    )
    or
    exists(FunctionCall forwardedCall, Parameter forwardedOutput |
      forwardedCall.getEnclosingFunction() = output.getFunction() and
      forwardedCall.getTarget() = forwardedOutput.getFunction() and
      forwardedCall.getArgument(forwardedOutput.getIndex()).getAChild*() = output.getAnAccess() and
      reachesUnguardedCmmApply(forwardedCall, forwardedOutput, array)
    )
  )
}

private predicate fixedBufferFlowsToUnguardedApply(
  FunctionCall firstCall, LocalVariable buffer, ArrayType array, VariableAccess bufferUse
) {
  exists(Parameter firstOutput |
    firstCall.getTarget() = firstOutput.getFunction() and
    bufferUse.getTarget() = buffer and
    firstCall.getArgument(firstOutput.getIndex()).getAChild*() = bufferUse and
    reachesUnguardedCmmApply(firstCall, firstOutput, array)
  )
}

from
  LocalVariable buffer, ArrayType array, FunctionCall firstCall, VariableAccess bufferUse, int size
where
  array = buffer.getType() and
  size = array.getArraySize() and
  size > 0 and
  fixedBufferFlowsToUnguardedApply(firstCall, buffer, array, bufferUse) and
  isProductSource(firstCall.getFile())
select bufferUse,
  "Fixed buffer '" + buffer.getName() + "' has " + size.toString() +
    " elements but is forwarded to a CIccCmm transform whose channel count is profile-defined. " +
    "No matching literal capacity guard was recognized. Pass the capacity directly to the guarded " +
    "Apply() wrapper, or manually verify capacity parameters forwarded through other helpers " +
    "(CWE-121/CWE-787; see #2868 and #2886)."
