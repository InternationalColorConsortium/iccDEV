/**
 * @name Signed integer overflow in offset arithmetic
 * @description Casting untrusted icUInt32Number values to int and adding them
 *              can overflow when processing malformed ICC profiles. Use
 *              icUInt64Number or unsigned arithmetic for offset+size sums.
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/signed-offset-overflow
 * @tags security
 *       correctness
 *       external/cwe/cwe-190
 */

import cpp

from AddExpr add, Cast lhs, Cast rhs
where
  // Both operands cast to int (signed 32-bit)
  lhs = add.getLeftOperand() and
  rhs = add.getRightOperand() and
  lhs.getType().getName() = "int" and
  rhs.getType().getName() = "int" and
  // The original type is unsigned 32-bit (icUInt32Number or unsigned int)
  (
    lhs.getExpr().getType().getUnspecifiedType().(IntegralType).isUnsigned() or
    rhs.getExpr().getType().getUnspecifiedType().(IntegralType).isUnsigned()
  ) and
  // In a comparison or array context (where overflow matters)
  (
    exists(ComparisonOperation cmp | cmp.getAnOperand() = add) or
    exists(ArrayExpr arr | arr.getArrayOffset() = add)
  ) and
  // The expression involves offset or size fields (ICC profile structure)
  (
    lhs.getExpr().toString().regexpMatch(".*(?i)(offset|size|length).*") or
    rhs.getExpr().toString().regexpMatch(".*(?i)(offset|size|length).*")
  )
select add,
  "Signed integer overflow risk: casting unsigned offset/size fields to int before addition. Use icUInt64Number or unsigned arithmetic (CWE-190)."
