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

/** A call whose result is immediately dereferenced (no intervening compare to 0). */
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
    not exists(ComparisonOperation cmp |
      cmp.getEnclosingFunction() = fc.getEnclosingFunction() and
      cmp.getAnOperand().(VariableAccess).getTarget() = lv and
      cmp.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
      cmp.getLocation().getStartLine() <= deref.getLocation().getStartLine()
    ) and
    not exists(FunctionCall guard |
      guard.getEnclosingFunction() = fc.getEnclosingFunction() and
      guard.getAnArgument().(VariableAccess).getTarget() = lv and
      guard.getTarget().getName().regexpMatch("(?i)(assert|abort|throw|require)") and
      guard.getLocation().getStartLine() >= fc.getLocation().getStartLine() and
      guard.getLocation().getStartLine() <= deref.getLocation().getStartLine()
    )
  )
}

from FunctionCall fc, Expr deref
where dereferencedWithoutCheck(fc, deref)
select deref,
  "Dereference of result from $@ without a NULL check; can return NULL on malformed input.",
  fc, fc.getTarget().getName() + "()"
