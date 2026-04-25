/**
 * @name Pointer dereferenced after Read/ParseXml without NULL check
 * @description CIcc*::Read(), ParseXml(), ParseBasic(), ParseTag(), and
 *              GetTagOfType()-style functions can return NULL on malformed
 *              input. Dereferencing the returned pointer without a NULL
 *              check causes SIGSEGV. This pattern recurs across 20+ NPD
 *              fixes (PRs #404, #405, #407, #418, #515, #639, #685, #728,
 *              #75f124f, #485fd34).
 * @kind problem
 * @problem.severity warning
 * @security-severity 7.0
 * @precision medium
 * @id iccdev/null-after-read
 * @tags security
 *       external/cwe/cwe-476
 *       reliability
 */

import cpp

/** Functions whose return value is commonly NULL on bad input. */
predicate canReturnNull(Function f) {
  f.getName().regexpMatch("(?i)(Read|ParseXml|ParseTag|ParseBasic|GetTag|GetTagOfType|FindTag|NewElement|NewTag|GetMpe|GetXform|FindMpe|GetTagFor|GetView|pccIcc)") and
  f.fromSource() and
  f.getType().getUnspecifiedType() instanceof PointerType
}

/** A null-check on local variable lv (between fc and deref lines). */
predicate hasNullCheck(LocalVariable lv, FunctionCall fc, Expr deref) {
  // Explicit comparison: ptr == 0, ptr != NULL, etc.
  exists(ComparisonOperation cmp |
    cmp.getEnclosingFunction() = fc.getEnclosingFunction() and
    cmp.getAnOperand().(VariableAccess).getTarget() = lv and
    cmp.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
    cmp.getLocation().getStartLine() <= deref.getLocation().getStartLine()
  )
  or
  // Logical-not style: if (!ptr) ...
  exists(NotExpr ne |
    ne.getEnclosingFunction() = fc.getEnclosingFunction() and
    ne.getOperand().(VariableAccess).getTarget() = lv and
    ne.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
    ne.getLocation().getStartLine() <= deref.getLocation().getStartLine()
  )
  or
  // Truth test: if (ptr), while (ptr), ptr && x, ptr || x, ptr ? a : b.
  // The variable access (or its immediate ImplicitCastExpr-to-bool wrapper)
  // must be the entire condition / operand -- NOT a sub-expression of
  // something like `if (ptr->method())` which is a USE, not a check.
  exists(VariableAccess va, Expr cond |
    va.getTarget() = lv and
    va.getEnclosingFunction() = fc.getEnclosingFunction() and
    va.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
    va.getLocation().getStartLine() <= deref.getLocation().getStartLine() and
    (cond = va or cond.(Conversion).getExpr() = va) and
    (
      exists(IfStmt is | is.getCondition() = cond) or
      exists(WhileStmt ws | ws.getCondition() = cond) or
      exists(ForStmt fs | fs.getCondition() = cond) or
      exists(ConditionalExpr ce | ce.getCondition() = cond) or
      exists(LogicalAndExpr la | la.getAnOperand() = cond) or
      exists(LogicalOrExpr lo | lo.getAnOperand() = cond)
    )
  )
  or
  // Guard call: assert(ptr), abort_if_null(ptr), etc.
  exists(FunctionCall guard |
    guard.getEnclosingFunction() = fc.getEnclosingFunction() and
    guard.getAnArgument().(VariableAccess).getTarget() = lv and
    guard.getTarget().getName().regexpMatch("(?i)(assert|abort|throw|require)") and
    guard.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
    guard.getLocation().getStartLine() <= deref.getLocation().getStartLine()
  )
  or
  // Reassignment: ptr = ...; (resets the chain — handled by the next call's check)
  exists(Assignment reasn |
    reasn.getEnclosingFunction() = fc.getEnclosingFunction() and
    reasn.getLValue().(VariableAccess).getTarget() = lv and
    reasn.getLocation().getStartLine() > fc.getLocation().getStartLine() and
    reasn.getLocation().getStartLine() < deref.getLocation().getStartLine()
  )
}

/** A call whose result is immediately dereferenced (no intervening NULL check). */
predicate dereferencedWithoutCheck(FunctionCall fc, Expr deref) {
  canReturnNull(fc.getTarget()) and
  exists(VariableAccess va, LocalVariable lv, Assignment asn |
    asn.getRValue() = fc and
    asn.getLValue() = va and
    va.getTarget() = lv and
    deref.getEnclosingFunction() = fc.getEnclosingFunction() and
    deref.getLocation().getStartLine() > fc.getLocation().getStartLine() and
    (
      deref.(PointerDereferenceExpr).getOperand().(VariableAccess).getTarget() = lv or
      deref.(PointerFieldAccess).getQualifier().(VariableAccess).getTarget() = lv or
      deref.(FunctionCall).getQualifier().(VariableAccess).getTarget() = lv
    ) and
    not hasNullCheck(lv, fc, deref)
  )
}

from FunctionCall fc, Expr deref
where dereferencedWithoutCheck(fc, deref)
select deref,
  "Dereference of result from $@ without a NULL check; can return NULL on malformed input.",
  fc, fc.getTarget().getName() + "()"
