/**
 * @name Incorrect u8Fixed8Number reconstruction from normalized float
 * @description ReadUInt16Float() normalizes u16 values to [0,1] by dividing
 *              by 65535. To reconstruct the original u8Fixed8Number value
 *              (which should be divided by 256), the correct formula is:
 *                value * 65535.0 / 256.0
 *              NOT:
 *                value * 256.0
 *              The ratio 65536/65535 = 1.0000153 introduces a systematic
 *              error of ~0.0015% that accumulates in gamma exponentiation.
 *              Bug history: CIccTagCurve::Describe() and DumpLut() used
 *              m_Curve[0] * 256.0 since the initial commit (2005), producing
 *              gamma 2.2071 instead of the correct 2.20703125.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/u8fixed8-reconstruction-error
 * @tags security
 *       correctness
 *       external/cwe/cwe-682
 */

import cpp

from MulExpr mul, Expr left, Expr right
where
  (
    // pattern: expr * 256.0
    left = mul.getLeftOperand() and
    right = mul.getRightOperand() and
    right instanceof Literal and
    right.getValue() = "256" and
    left.getType().getUnspecifiedType() instanceof FloatingPointType
    or
    // pattern: 256.0 * expr
    left = mul.getLeftOperand() and
    right = mul.getRightOperand() and
    left instanceof Literal and
    left.getValue() = "256" and
    right.getType().getUnspecifiedType() instanceof FloatingPointType
  ) and
  // In ICC source files
  mul.getFile().getRelativePath().matches("%Icc%") and
  // Not in test code
  not mul.getFile().getRelativePath().matches("%test%") and
  // Not in the SetGamma write path (where 256 is correct for encoding)
  not exists(Function f |
    f = mul.getEnclosingFunction() and
    f.getName().matches("%SetGamma%")
  )
select mul,
  "Multiplication by 256 on a normalized float value. " +
  "If this value came from ReadUInt16Float() (normalized by /65535), " +
  "the correct reconstruction for u8Fixed8 is: value * 65535.0 / 256.0, " +
  "not value * 256.0. The error ratio is 65536/65535 (CWE-682)."
