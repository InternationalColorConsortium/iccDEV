/**
 * @name Format string with user-controlled argument
 * @description printf/snprintf/fprintf calls where a profile-derived string
 *              (tag name, description) is passed as a format argument rather
 *              than a data argument. If the string contains format specifiers,
 *              this causes undefined behavior.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/tainted-format-string
 * @tags security
 *       external/cwe/cwe-134
 */

import cpp

from FunctionCall printCall, int fmtIdx, VariableAccess fmtArg
where
  printCall.getTarget().getName() in ["printf", "fprintf", "sprintf", "snprintf"] and
  (
    (printCall.getTarget().getName() = "printf" and fmtIdx = 0) or
    (printCall.getTarget().getName() = "fprintf" and fmtIdx = 1) or
    (printCall.getTarget().getName() = "sprintf" and fmtIdx = 1) or
    (printCall.getTarget().getName() = "snprintf" and fmtIdx = 2)
  ) and
  fmtArg = printCall.getArgument(fmtIdx) and
  // Format arg is a variable (not a string literal) with a name suggesting profile data
  fmtArg.getTarget().getName().regexpMatch("(?i).*(desc|name|text|str|contents|buf).*") and
  // In ICC source files
  printCall.getFile().getRelativePath().regexpMatch(".*(?:Icc|icc).*")
select printCall,
  "Profile-derived string used as format argument. Use \"%s\" format with the string as a data argument (CWE-134)."
