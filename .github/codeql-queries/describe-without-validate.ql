/**
 * @name Describe() called without prior Validate() check
 * @description CIccTag::Describe() can crash on partially-loaded or malformed
 *              tag data. Tools should call Validate() first and skip Describe()
 *              if critical errors are found. This pattern is the root cause of
 *              multiple fuzzer-discovered crashes.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/describe-without-validate
 * @tags security
 *       correctness
 *       external/cwe/cwe-476
 */

import cpp

from FunctionCall describeCall
where
  describeCall.getTarget().getName() = "Describe" and
  describeCall.getTarget().getDeclaringType().getName().matches("CIcc%") and
  // Not preceded by a Validate call on the same object within 10 lines
  not exists(FunctionCall validateCall |
    validateCall.getTarget().getName() = "Validate" and
    validateCall.getLocation().getFile() = describeCall.getLocation().getFile() and
    validateCall.getLocation().getStartLine() < describeCall.getLocation().getStartLine() and
    validateCall.getLocation().getStartLine() > describeCall.getLocation().getStartLine() - 15
  ) and
  // In tool source files (not the library itself which has internal guards)
  describeCall.getFile().getRelativePath().matches("%Tools/%")
select describeCall,
  "Describe() called without prior Validate() check. " +
  "Malformed tag data can crash in Describe(). Consider validating first (CWE-476)."
