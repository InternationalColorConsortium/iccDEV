/**
 * @name Rule of Three violation: dtor frees member with no copy ctor / assignment
 * @description A class whose destructor frees a raw pointer member but does
 *              not declare a copy constructor or copy-assignment operator
 *              risks double-free and use-after-free if instances are copied.
 *              C++ Core Guideline C.20-C.22 / CWE-415 / CWE-416.
 * @kind problem
 * @problem.severity warning
 * @security-severity 5.5
 * @precision medium
 * @id iccdev/rule-of-three-violation
 * @tags security
 *       external/cwe/cwe-415
 *       external/cwe/cwe-416
 *       reliability
 */

import cpp

predicate dtorFreesField(Destructor d, Field f) {
  exists(DeleteExpr del |
    del.getEnclosingFunction() = d and
    del.getExpr().(FieldAccess).getTarget() = f
  )
  or
  exists(DeleteArrayExpr del |
    del.getEnclosingFunction() = d and
    del.getExpr().(FieldAccess).getTarget() = f
  )
}

predicate hasCopyCtor(Class c) {
  exists(CopyConstructor cc | cc.getDeclaringType() = c)
}

predicate hasCopyAssign(Class c) {
  exists(CopyAssignmentOperator op | op.getDeclaringType() = c)
}

predicate hasDeletedCopy(Class c) {
  exists(MemberFunction mf |
    mf.getDeclaringType() = c and
    mf.isDeleted() and
    (mf instanceof CopyConstructor or mf instanceof CopyAssignmentOperator)
  )
  or
  // Template instantiations inherit deleted copy ops from the primary template.
  exists(Class tmpl, MemberFunction mf |
    tmpl = c.(ClassTemplateInstantiation).getTemplate() and
    mf.getDeclaringType() = tmpl and
    mf.isDeleted() and
    (mf instanceof CopyConstructor or mf instanceof CopyAssignmentOperator)
  )
}

from Class c, Destructor d, Field f
where
  d.getDeclaringType() = c and
  dtorFreesField(d, f) and
  not hasCopyCtor(c) and
  not hasCopyAssign(c) and
  not hasDeletedCopy(c) and
  // exclude POD wrappers that intentionally manage themselves
  c.fromSource()
select d,
  "Class '" + c.getName() + "' destructor frees member '" + f.getName() +
  "' but has no copy ctor or copy-assignment operator. Implement Rule of Three or mark copies =delete to prevent double-free."
