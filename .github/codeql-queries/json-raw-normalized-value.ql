/**
 * @name JSON serialization of raw normalized ICC value
 * @description ICC profile values stored as normalized [0,1] floats (from
 *              ReadUInt16Float/ReadUInt8Float) must be converted back to their
 *              spec-defined representation before JSON serialization. Writing
 *              the raw normalized value produces meaningless numbers
 *              (e.g., 0.00862 instead of gamma 2.207).
 *              Bug history: CIccTagJsonCurve::ToJson() wrote m_Curve[0]
 *              directly as "gamma" instead of reconstructing the u8Fixed8
 *              value via m_Curve[0] * 65535.0 / 256.0.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/json-raw-normalized-value
 * @tags security
 *       correctness
 *       external/cwe/cwe-682
 */

import cpp

from FieldAccess access, AssignExpr assign
where
  // The field being accessed is a curve or parameter array
  (
    access.getTarget().getName() = "m_Curve" or
    access.getTarget().getName() = "m_pSamples"
  ) and
  // Used in a ToJson method
  access.getEnclosingFunction().getName().matches("%ToJson%") and
  // Directly assigned without arithmetic wrapper
  assign.getRValue().getAChild*() = access and
  not exists(MulExpr mul | mul.getAChild*() = access and assign.getRValue().getAChild*() = mul) and
  not exists(DivExpr div | div.getAChild*() = access and assign.getRValue().getAChild*() = div) and
  // In ICC source
  access.getFile().getRelativePath().matches("%Icc%")
select access,
  "Normalized ICC value $@ serialized to JSON without reconstruction. " +
  "Values from ReadUInt16Float are in [0,1] and must be converted to " +
  "spec-defined representation (e.g., u8Fixed8: val * 65535.0 / 256.0) " +
  "before JSON output (CWE-682).",
  access.getTarget(), access.getTarget().getName()
