/**
 * @name Unchecked suffix length used for string-offset comparison
 * @description A suffix length greater than the supplied color-name length
 *              makes `name + (nameLength - suffixLength)` point before the
 *              buffer. Passing that pointer to strncmp() causes an
 *              out-of-bounds read.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/unterminated-suffix-comparison
 * @tags security
 *       external/cwe/cwe-125
 *       external/cwe/cwe-191
 */

import cpp
import semmle.code.cpp.controlflow.Guards

predicate isStrlenAssignment(Variable variable, AssignExpr assignment) {
  assignment.getLValue().(VariableAccess).getTarget() = variable and
  exists(FunctionCall strlen |
    (
      strlen = assignment.getRValue() or
      strlen = assignment.getRValue().getAChild*()
    ) and
    strlen.getTarget().getName() = "strlen"
  )
}

predicate hasPriorLengthGuard(FunctionCall comparison, Variable nameLength,
    Variable suffixLength) {
  exists(ComparisonOperation guard |
    guard.getEnclosingFunction() = comparison.getEnclosingFunction() and
    guard instanceof GuardCondition and
    guard.getOperator() = "<" and
    guard.getLeftOperand().getAChild*().(VariableAccess).getTarget() = nameLength and
    guard.getRightOperand().getAChild*().(VariableAccess).getTarget() = suffixLength and
    guard.(GuardCondition).controls(comparison.getBasicBlock(), false)
  )
}

from FunctionCall comparison, Expr firstArgument, SubExpr offset,
  Variable nameLength, Variable suffixLength, AssignExpr suffixAssignment
where
  comparison.getTarget().getName() = "strncmp" and
  firstArgument = comparison.getArgument(0) and
  offset = firstArgument.getAChild*() and
  offset.getLeftOperand().(VariableAccess).getTarget() = nameLength and
  offset.getRightOperand().(VariableAccess).getTarget() = suffixLength and
  comparison.getArgument(2).(VariableAccess).getTarget() = suffixLength and
  isStrlenAssignment(suffixLength, suffixAssignment) and
  not hasPriorLengthGuard(comparison, nameLength, suffixLength)
select comparison,
  "Suffix length $@ can exceed the color-name length before this offset is " +
  "passed to strncmp(), producing a pointer before the input buffer.",
  suffixLength, suffixLength.getName()
