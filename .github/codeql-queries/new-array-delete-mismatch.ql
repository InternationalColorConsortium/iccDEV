/**
 * @name Mismatched new[] / delete (CWE-762)
 * @description Memory allocated with operator new[] must be released with
 *              operator delete[]. Mixing array new with scalar delete is
 *              undefined behavior per C++ [expr.delete]/2 and triggers
 *              ASAN alloc-dealloc-mismatch.
 * @kind problem
 * @problem.severity error
 * @security-severity 6.5
 * @precision high
 * @id iccdev/new-array-delete-mismatch
 * @tags security
 *       external/cwe/cwe-762
 *       reliability
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow

/**
 * A scalar `delete` expression whose operand is the result of a `new[]`
 * expression flowing through a single field assignment.
 *
 * Targets the iccDEV pattern:
 *   ctor: m_vals = new icFloatNumber[nCols];
 *   dtor: delete m_vals;            // BUG: should be delete[]
 */
predicate fieldAllocatedAsArray(Field f, NewArrayExpr nae) {
  exists(Assignment a |
    a.getLValue().(FieldAccess).getTarget() = f and
    a.getRValue() = nae
  )
}

predicate fieldDeletedAsScalar(Field f, DeleteExpr del) {
  del.getExpr().(FieldAccess).getTarget() = f and
  not del instanceof DeleteArrayExpr
}

from Field f, NewArrayExpr nae, DeleteExpr del
where
  fieldAllocatedAsArray(f, nae) and
  fieldDeletedAsScalar(f, del) and
  // Restrict to fields that are ONLY ever array-allocated (no scalar new sites)
  not exists(NewExpr ne |
    ne.(Assignment).getLValue().(FieldAccess).getTarget() = f and
    not ne instanceof NewArrayExpr
  )
select del,
  "Field '" + f.getName() + "' is allocated with new[] (at $@) but freed with scalar delete here. Use delete[] to avoid undefined behavior.",
  nae, "this new[] expression"
