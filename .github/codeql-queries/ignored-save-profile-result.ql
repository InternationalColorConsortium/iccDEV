/**
 * @name Ignored SaveIccProfile result
 * @description ICC command-line tools must check SaveIccProfile's return value
 *              before reporting that an output profile was created.
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id iccdev/ignored-save-profile-result
 * @tags security
 *       correctness
 *       external/cwe/cwe-252
 */

import cpp

class SaveIccProfileCall extends FunctionCall {
  SaveIccProfileCall() {
    this.getTarget().getName() = "SaveIccProfile"
  }
}

from SaveIccProfileCall call
where
  exists(ExprStmt es | es.getExpr() = call) and
  not call.getFile().getRelativePath().matches("%test%")
select call,
  "The result of SaveIccProfile is ignored. Check it before reporting output success."
