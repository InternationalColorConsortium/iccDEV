/**
 * @name Static cast to derived type without dynamic_cast
 * @description Using static_cast to convert CIccTag* to derived types without
 *              checking GetType() first can cause type confusion when malformed
 *              profiles report incorrect tag type signatures. Prefer
 *              dynamic_cast or verify GetType() matches before static_cast.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/unsafe-tag-downcast
 * @tags security
 *       correctness
 *       external/cwe/cwe-843
 */

import cpp

from StaticCast cast
where
  // Casting from CIccTag* (or CIccTag&)
  cast.getExpr().getType().stripType().getName().matches("CIccTag") and
  // To a derived type
  cast.getType().stripType().getName().matches("CIcc%") and
  cast.getType().stripType().getName() != "CIccTag" and
  // Not preceded by a GetType() check within 5 lines
  not exists(FunctionCall typeCheck |
    typeCheck.getTarget().getName() = "GetType" and
    typeCheck.getLocation().getFile() = cast.getLocation().getFile() and
    typeCheck.getLocation().getStartLine() >= cast.getLocation().getStartLine() - 8 and
    typeCheck.getLocation().getStartLine() <= cast.getLocation().getStartLine()
  ) and
  // In tool files (library has its own type dispatch)
  cast.getFile().getRelativePath().matches("%Tools/%")
select cast,
  "static_cast from CIccTag* to " + cast.getType().stripType().getName() +
  " without GetType() check. Malformed profiles may have mismatched type signatures (CWE-843)."
