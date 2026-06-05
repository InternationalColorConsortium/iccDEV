/**
 * @name Unbounded loop driven by profile field
 * @description Loops whose iteration count is directly controlled by an
 *              untrusted ICC profile field (tag count, element count, size)
 *              without an upper bound check can cause denial of service via
 *              excessive iteration or memory exhaustion.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/unbounded-profile-loop
 * @tags security
 *       correctness
 *       external/cwe/cwe-400
 *       external/cwe/cwe-834
 */

import cpp

predicate isNarrowCountType(Type t) {
  exists(IntegralType it |
    it = t and
    it.getSize() <= 2
  )
  or
  t.getName().regexpMatch("(?i).*(char|short|byte|u?int(8|16)|icUInt(8|16)Number|icInt(8|16)Number).*")
}

predicate conditionMentionsFieldAndBound(Expr condition, Field field) {
  condition.getAChild*().(FieldAccess).getTarget() = field and
  condition.getAChild*().toString().regexpMatch(
    "(?i).*(max|limit|bound|INT_MAX|icMaxEnum|4096|65535|MAX_CALC_ELEMENTS|kMax).*"
  )
}

predicate hasPriorBoundGuardInFunction(ForStmt loop, Field field) {
  exists(IfStmt guard |
    guard.getEnclosingFunction() = loop.getEnclosingFunction() and
    guard.getLocation().getStartLine() < loop.getLocation().getStartLine() and
    conditionMentionsFieldAndBound(guard.getCondition(), field)
  )
}

predicate hasSetupBoundGuardInSameType(Field field) {
  exists(IfStmt guard, Function setup |
    setup = guard.getEnclosingFunction() and
    setup.getDeclaringType() = field.getDeclaringType() and
    setup.getName().regexpMatch("(?i).*(open|create|init|read|validate|load|parse|set).*") and
    conditionMentionsFieldAndBound(guard.getCondition(), field)
  )
}

from ForStmt loop, FieldAccess fa
where
  // The loop condition compares against a field access
  fa = loop.getCondition().getAChild*() and
  // The field name suggests a count/size from profile data
  fa.getTarget().getName().regexpMatch("(?i).*(count|num|size|length|steps|nInput|nOutput|m_n).*") and
  // The field is from a class/struct (not a local)
  exists(fa.getTarget().getDeclaringType()) and
  // Exempt loops with a cooperative-cancellation guard in the condition
  // (e.g., && sink.ShouldContinue()) - bounded by IDescribeSink contract.
  not loop.getCondition().getAChild*().toString().matches("%ShouldContinue%") and
  // Exempt loops driven by u8/u16-narrowed fields - already capped at
  // 255/65535 by the field type itself.
  not isNarrowCountType(fa.getTarget().getType()) and
  // No bounds check earlier in the same function, or in setup/validation
  // methods that establish same-object field invariants before loop use.
  not hasPriorBoundGuardInFunction(loop, fa.getTarget()) and
  not hasSetupBoundGuardInSameType(fa.getTarget()) and
  not exists(FunctionCall minCall |
    minCall.getTarget().getName().matches("%min%") and
    minCall.getLocation().getStartLine() >= loop.getLocation().getStartLine() - 3 and
    minCall.getLocation().getStartLine() <= loop.getLocation().getStartLine()
  ) and
  // Exclude test code
  not loop.getFile().getRelativePath().matches("%test%")
select loop,
  "Loop iteration count controlled by profile field '" +
  fa.getTarget().getName() +
  "' without upper bound validation. Malformed profiles can cause DoS (CWE-400/CWE-834)."
