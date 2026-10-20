/**
 * @name Fixed buffer used as transform output
 * @description A fixed-size local buffer is passed as the output of a wrapper
 *              around CIccCmm::Apply() without a matching destination-width
 *              guard. This is the stack-buffer-overflow shape from issue
 *              #2868.
 * @kind problem
 * @problem.severity error
 * @security-severity 9.1
 * @precision high
 * @id iccdev/fixed-buffer-transform-output
 * @tags security
 *       correctness
 *       external/cwe/cwe-121
 *       external/cwe/cwe-787
 */

import cpp
import semmle.code.cpp.controlflow.Guards

private predicate isProductSource(File f) {
  (
    f.getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
    not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
  )
  or
  f.getBaseName() = "case.cpp"
}

private predicate isCmmApplyCall(FunctionCall call) {
  call.getTarget().getName() = "Apply" and
  call.getTarget().getDeclaringType().getName() = "CIccCmm"
}

private predicate hasPriorCapacityGuard(FunctionCall apply, ArrayType array) {
  exists(ComparisonOperation guard, FunctionCall destSamples |
    guard.getEnclosingFunction() = apply.getEnclosingFunction() and
    guard instanceof GuardCondition and
    guard.getOperator() = ">" and
    destSamples.getTarget().getName() = "GetDestSamples" and
    destSamples.getTarget().getDeclaringType().getName() = "CIccCmm" and
    destSamples.getQualifier().(VariableAccess).getTarget() =
      apply.getQualifier().(VariableAccess).getTarget() and
    guard.getLeftOperand() = destSamples and
    guard.getRightOperand().getValue() = array.getArraySize().toString() and
    guard.(GuardCondition).controls(apply.getBasicBlock(), false)
  )
}

private predicate isTransformOutputWrapper(Function wrapper, Parameter output,
    ArrayType array) {
  output = wrapper.getParameter(0) and
  exists(FunctionCall apply |
    apply.getEnclosingFunction() = wrapper and
    isCmmApplyCall(apply) and
    apply.getArgument(0).getAChild*() = output.getAnAccess() and
    not hasPriorCapacityGuard(apply, array)
  )
}

from LocalVariable buffer, ArrayType array, FunctionCall wrapperCall,
     Function wrapper, Parameter output, int size
where
  array = buffer.getType() and
  size = array.getArraySize() and
  size > 0 and
  wrapperCall.getTarget() = wrapper and
  isTransformOutputWrapper(wrapper, output, array) and
  wrapperCall.getArgument(0).getAChild*() = buffer.getAnAccess() and
  isProductSource(wrapperCall.getFile())
select wrapperCall.getArgument(0),
  "Fixed buffer '" + buffer.getName() + "' has " + size.toString() +
  " elements but receives output from a CIccCmm transform whose channel count is profile-defined. " +
  "Allocate the output buffer from the transform's destination channel count or reject oversized output " +
  "(CWE-121/CWE-787; see #2868)."
