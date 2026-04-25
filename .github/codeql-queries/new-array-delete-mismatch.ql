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
  del.getExpr().(FieldAccess).getTarget() = f
}

from Field f, NewArrayExpr nae, DeleteExpr del
where
  fieldAllocatedAsArray(f, nae) and
  fieldDeletedAsScalar(f, del) and
  // Exclude fields that are ALSO scalar-allocated somewhere (mixed use)
  not exists(NewExpr ne, Assignment a |
    a.getRValue() = ne and
    a.getLValue().(FieldAccess).getTarget() = f
  )
select del,
  "Field '" + f.getName() + "' is allocated with new[] (at $@) but freed with scalar delete here. Use delete[] to avoid undefined behavior.",
  nae, "this new[] expression"
