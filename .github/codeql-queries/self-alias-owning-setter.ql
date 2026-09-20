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

predicate deletesField(DeleteExpr deleteExpr, Field field) {
  deleteExpr.getExpr().(FieldAccess).getTarget() = field
}

predicate assignsParameterToField(AssignExpr assignment, Field field,
    Parameter parameter) {
  assignment.getLValue().(FieldAccess).getTarget() = field and
  assignment.getRValue().(VariableAccess).getTarget() = parameter
}

predicate rejectsSelfAlias(MemberFunction setter, Field field,
    Parameter parameter, DeleteExpr deleteExpr) {
  exists(IfStmt guard, Expr condition, ReturnStmt returnStmt |
    guard.getEnclosingFunction() = setter and
    condition = guard.getCondition() and
    condition.getAChild*().(FieldAccess).getTarget() = field and
    condition.getAChild*().(VariableAccess).getTarget() = parameter and
    condition.toString().matches("%==%") and
    returnStmt = guard.getThen().getAChild*() and
    guard.getLocation().getStartLine() < deleteExpr.getLocation().getStartLine()
  )
}

from MemberFunction setter, Field field, Parameter parameter, DeleteExpr deleteExpr,
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
  not rejectsSelfAlias(setter, field, parameter, deleteExpr)
select assignment,
  "Owning setter deletes member $@ before storing parameter $@. SetX(GetX()) " +
  "would store a dangling pointer; reject self-aliasing before delete.",
  field, field.getName(),
  parameter, parameter.getName()
