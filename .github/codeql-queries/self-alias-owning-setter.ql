/**
 * @name Owning setter permits self-alias use-after-free
 * @description An owning setter that deletes a pointer member before assigning
 *              its pointer parameter permits SetX(GetX()). That frees the
 *              argument, stores the dangling pointer, and can cause an
 *              immediate use-after-free or a later double-free.
 * @kind problem
 * @problem.severity error
 * @security-severity 7.5
 * @precision high
 * @id iccdev/self-alias-owning-setter
 * @tags security
 *       external/cwe/cwe-415
 *       external/cwe/cwe-416
 *       reliability
 */

import cpp
import semmle.code.cpp.controlflow.Guards

predicate deletesField(Expr deleteExpr, Field field) {
  exists(DeleteExpr del |
    deleteExpr = del and
    del.getExpr().(FieldAccess).getTarget() = field
  )
  or
  exists(DeleteArrayExpr del |
    deleteExpr = del and
    del.getExpr().(FieldAccess).getTarget() = field
  )
}

predicate assignsParameterToField(AssignExpr assignment, Field field,
    Parameter parameter) {
  assignment.getLValue().(FieldAccess).getTarget() = field and
  assignment.getRValue().(VariableAccess).getTarget() = parameter
}

predicate nullCheckedSelfAliasControlsDelete(IfStmt guard,
    EqualityOperation comparison, Parameter parameter, Expr deleteExpr) {
  exists(LogicalAndExpr condition |
    condition = guard.getCondition() and
    comparison = condition.getAnOperand() and
    condition.getAnOperand().(VariableAccess).getTarget() = parameter and
    guard.getThen() instanceof ReturnStmt and
    guard.getLocation().getStartLine() < deleteExpr.getLocation().getStartLine()
  )
}

predicate rejectsSelfAlias(MemberFunction setter, Field field,
    Parameter parameter, Expr deleteExpr) {
  exists(IfStmt guard, EqualityOperation comparison |
    guard.getEnclosingFunction() = setter and
    comparison = guard.getCondition().getAChild*() and
    comparison.getAnOperand().(FieldAccess).getTarget() = field and
    comparison.getAnOperand().(VariableAccess).getTarget() = parameter and
    (
      // The false continuation of an equality check has excluded the alias.
      comparison.getOperator() = "==" and
      (
        comparison instanceof GuardCondition and
        comparison.(GuardCondition).controls(deleteExpr.getBasicBlock(), false)
        or
        nullCheckedSelfAliasControlsDelete(guard, comparison, parameter, deleteExpr)
      )
      or
      // The true continuation of an inequality check has excluded the alias.
      comparison.getOperator() = "!=" and
      comparison instanceof GuardCondition and
      comparison.(GuardCondition).controls(deleteExpr.getBasicBlock(), true)
    )
  )
}

from MemberFunction setter, Field field, Parameter parameter, Expr deleteExpr,
  AssignExpr assignment
where
  deletesField(deleteExpr, field) and
  assignsParameterToField(assignment, field, parameter) and
  deleteExpr.getEnclosingFunction() = setter and
  assignment.getEnclosingFunction() = setter and
  field.getDeclaringType() = setter.getDeclaringType() and
  field.getType().getUnspecifiedType() instanceof PointerType and
  parameter.getType().getUnspecifiedType() instanceof PointerType and
  deleteExpr.getLocation().getStartLine() < assignment.getLocation().getStartLine() and
  (
    deleteExpr.getBasicBlock() = assignment.getBasicBlock() or
    deleteExpr.getBasicBlock().getASuccessor*() = assignment.getBasicBlock()
  ) and
  not rejectsSelfAlias(setter, field, parameter, deleteExpr)
select assignment,
  "Owning setter deletes member $@ before storing parameter $@. SetX(GetX()) " +
  "would store a dangling pointer; reject self-aliasing before delete.",
  field, field.getName(),
  parameter, parameter.getName()
