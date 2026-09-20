/**
 * @name Fixed buffer indexed by a wider loop bound
 * @description A loop indexes a fixed-size aggregate position table with its loop
 *              counter, but the loop can iterate to or past the array's element
 *              count. This is the pattern behind issue #2608, where a 16-entry
 *              stack array was indexed by an attacker-controlled uInt16 output
 *              channel count.
 * @kind problem
 * @problem.severity error
 * @security-severity 9.3
 * @precision high
 * @id iccdev/fixed-buffer-loop-bound
 * @tags security
 *       correctness
 *       external/cwe/cwe-121
 *       external/cwe/cwe-787
 */

import cpp
import semmle.code.cpp.rangeanalysis.SimpleRangeAnalysis

private predicate isProductSource(File f) {
  (
    f.getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
    not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
  )
  or
  f.getBaseName() = "case.cpp"
}

private predicate loopCounterAndBound(ForStmt loop, LocalVariable counter, Expr bound) {
  exists(LTExpr condition, IncrementOperation update |
    condition = loop.getCondition() and
    condition.getLeftOperand() = counter.getAnAccess() and
    bound = condition.getRightOperand() and
    update = loop.getUpdate() and
    update.getAChild() = counter.getAnAccess()
  )
}

from LocalVariable buffer, ArrayType array, ForStmt loop,
     LocalVariable counter, Expr bound, Field boundField, int size
where
  array = buffer.getType() and
  size = array.getArraySize() and
  size > 0 and
  array.getBaseType().getUnspecifiedType() instanceof Class and
  loopCounterAndBound(loop, counter, bound) and
  bound.getAChild*() = boundField.getAnAccess() and
  boundField.getName().regexpMatch("(?i)^m_.*(size|count|channels|input|output|entries|elements)$") and
  exists(ArrayExpr access |
    access.getArrayBase().(VariableAccess).getTarget() = buffer and
    loop.getStmt().getAChild*() = access.getEnclosingStmt() and
    counter.getAnAccess() = access.getArrayOffset()
  ) and
  not exists(int constantBound |
    constantBound = bound.getValue().toInt() and
    constantBound <= size
  ) and
  upperBound(bound.getFullyConverted()) > size - 1 and
  isProductSource(loop.getFile())
select loop.getCondition(),
  "Loop counter '" + counter.getName() + "' can reach or exceed the " +
  size.toString() + " elements in fixed buffer '" + buffer.getName() +
  "'. Size the buffer from the loop bound or reject the larger count " +
  "(CWE-121/CWE-787; see #2608)."
