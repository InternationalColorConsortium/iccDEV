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

predicate accessesField(Expr e, Field field) {
  e.getAChild*().(FieldAccess).getTarget() = field
}

/**
 * Holds if `condition` compares `field` directly against the size of a
 * container - `field > v.size()`, `s.length() < field`, and so on.
 *
 * Clamping a loop bound to the size of the container the loop indexes is the
 * strongest form of the check available, and strictly stronger than comparing
 * against a hard-coded ceiling: it cannot drift when the container changes.
 * It carries none of the tokens the text regex below looks for, though, so
 * without this predicate such a clamp is invisible and the loop is reported as
 * unbounded. That produced alerts #2344-#2348 on IccProfLib/IccCmmSearch.cpp,
 * where `m_nCostApply` is clamped against three `std::vector::size()` values
 * before use and every one of the five reports was a false positive.
 *
 * Matched structurally - the field on one side of a comparison, the size call
 * on the other - rather than by widening the text regex again. A regex that
 * merely looked for "size" anywhere in the guard would also accept a guard
 * about some unrelated container, and text-matching is already what has made
 * the predicate below brittle (see its macro-expansion note).
 */
predicate comparesFieldAgainstContainerSize(Expr condition, Field field) {
  // RelationalOperation, deliberately not ComparisonOperation: the latter is
  // also the superclass of EqualityOperation, so `if (m_nCount == v.size())`
  // - a test, not a clamp, and no bound in either direction - would satisfy
  // this and silence every loop over the field in the whole declaring type.
  exists(RelationalOperation cmp, FunctionCall sizeCall |
    cmp = condition.getAChild*() and
    sizeCall.getTarget() instanceof MemberFunction and
    sizeCall.getTarget().getName().regexpMatch("(?i)^(size|length)$")
  |
    accessesField(cmp.getLeftOperand(), field) and
    sizeCall = cmp.getRightOperand().getAChild*()
    or
    accessesField(cmp.getRightOperand(), field) and
    sizeCall = cmp.getLeftOperand().getAChild*()
  )
}

predicate conditionMentionsFieldAndBound(Expr condition, Field field) {
  accessesField(condition, field) and
  (
    // Match a guard that names a recognised upper bound. NOTE: object-like macros
    // (e.g. #define MAX_CALC_ELEMENTS 65536) are expanded before CodeQL builds the
    // AST, so a guard written `field >= MAX_CALC_ELEMENTS` presents here as the
    // literal `65536`, never the macro spelling. The bare value must therefore be
    // listed alongside the macro name. Includes the concrete caps used across the
    // library: 65536 (MAX_CALC_ELEMENTS) and 16777215/0xffffff (the 24-bit array
    // serialization cap) in addition to the previously-listed 4096/65535.
    condition.getAChild*().toString().regexpMatch(
      "(?i).*(max|limit|bound|INT_MAX|icMaxEnum|4096|65535|65536|16777215|0xffffff|MAX_CALC_ELEMENTS|kMax).*"
    )
    or
    // ...or a clamp against the size of the container being walked, which names
    // no bound at all yet bounds the loop more tightly than any constant can.
    comparesFieldAgainstContainerSize(condition, field)
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
    (
      // A constructor is the commonest place in this codebase to establish a
      // field invariant, and it is the one function whose name can never match
      // the list below - getName() returns the class name. Excluding it defeats
      // this predicate's own purpose, and did: the clamp behind alerts
      // #2345-#2348 sits in CIccApplyCmmSearch's constructor, so every loop
      // reading the field from another method was reported as unguarded.
      setup instanceof Constructor
      or
      setup.getName().regexpMatch("(?i).*(open|create|init|read|validate|load|parse|set).*")
    ) and
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
    // Scope the min() exemption to the loop's own function. Without this it matched
    // any *min*-named call within three lines of ANY loop in ANY file, suppressing
    // unrelated loops through a cross-function line-number match. The
    // sibling guards above - hasPriorBoundGuardInFunction and
    // hasSetupBoundGuardInSameType - are already function-scoped; only this
    // exemption was left comparing bare line numbers. Mirror them.
    minCall.getEnclosingFunction() = loop.getEnclosingFunction() and
    minCall.getLocation().getStartLine() >= loop.getLocation().getStartLine() - 3 and
    minCall.getLocation().getStartLine() <= loop.getLocation().getStartLine()
  ) and
  // Exclude test code
  not loop.getFile().getRelativePath().matches("%test%")
select loop,
  "Loop iteration count controlled by profile field '" +
  fa.getTarget().getName() +
  "' without upper bound validation. Malformed profiles can cause DoS (CWE-400/CWE-834)."
