/**
 * @name Fixed buffer used as transform output
 * @description A fixed-size local buffer is passed as the output of a wrapper
 *              around CIccCmm::Apply(). The transform output width is profile
 *              defined, so the wrapper cannot establish that the buffer is
 *              large enough. This is the stack-buffer-overflow shape from
 *              issue #2868.
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

private predicate isTransformOutputWrapper(Function wrapper, Parameter output) {
  output = wrapper.getParameter(0) and
  exists(FunctionCall apply |
    apply.getEnclosingFunction() = wrapper and
    isCmmApplyCall(apply) and
    apply.getArgument(0).getAChild*() = output.getAnAccess()
  )
}

from LocalVariable buffer, ArrayType array, FunctionCall wrapperCall,
     Function wrapper, Parameter output, int size
where
  array = buffer.getType() and
  size = array.getArraySize() and
  size > 0 and
  wrapperCall.getTarget() = wrapper and
  isTransformOutputWrapper(wrapper, output) and
  wrapperCall.getArgument(0).getAChild*() = buffer.getAnAccess() and
  isProductSource(wrapperCall.getFile())
select wrapperCall.getArgument(0),
  "Fixed buffer '" + buffer.getName() + "' has " + size.toString() +
  " elements but receives output from a CIccCmm transform whose channel count is profile-defined. " +
  "Allocate the output buffer from the transform's destination channel count or reject oversized output " +
  "(CWE-121/CWE-787; see #2868)."
