/**
 * @name Unchecked Tell() position subtraction in ICC writers
 * @description CIccIO::Tell() returns a signed position and can fail or fail to
 *              advance on special outputs. Subtracting Tell-derived positions
 *              and narrowing directly to icUInt32Number can underflow or
 *              truncate offset/size fields.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/tell-subtraction-underflow
 * @tags security
 *       correctness
 *       external/cwe/cwe-191
 *       external/cwe/cwe-681
 */

import cpp

predicate isAffectedWriter(Function f) {
  f.getName() = "Write" and
  (
    f.getFile().getRelativePath().matches("%IccMpeBasic.cpp") or
    f.getFile().getRelativePath().matches("%IccMpeCalc.cpp") or
    f.getFile().getRelativePath().matches("%IccTagMPE.cpp")
  )
}

from Cast c, SubExpr sub
where
  isAffectedWriter(c.getEnclosingFunction()) and
  c.getType().getName() = "icUInt32Number" and
  c.getExpr().getAChild*() = sub and
  sub.toString().regexpMatch(".*(Tell|start|end|elemStart|tagStart).*")
select c,
  "Tell-derived offset/size subtraction is narrowed to icUInt32Number inside " +
  "an ICC writer. Check signed Tell() results and range before subtracting."
