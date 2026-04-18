/**
 * @name Fixed-point conversion function called with wrong type
 * @description icFtoD() expects icS15Fixed16Number (int32) but may be called
 *              with float/double arguments due to implicit conversion. This
 *              silently truncates the value (e.g., 2.4 -> 2) then divides by
 *              65536, producing garbage (0.0000305 instead of 2.4).
 *              Similarly, icDtoF() expects float but if the result is assigned
 *              to a float field that already holds the correct value, the
 *              round-trip through s15Fixed16 loses precision.
 *              Bug history: IccTagJson.cpp parametric curve ToJson/ParseJson
 *              used icFtoD(m_dParam[i]) where m_dParam is icFloatNumber[].
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id iccdev/fixedpoint-type-confusion
 * @tags security
 *       correctness
 *       external/cwe/cwe-681
 *       external/cwe/cwe-197
 */

import cpp

// icFtoD: s15Fixed16 -> float. Parameter is int32_t.
class IcFtoDCall extends FunctionCall {
  IcFtoDCall() {
    this.getTarget().getName() = "icFtoD"
  }
}

from IcFtoDCall call, Expr arg
where
  arg = call.getArgument(0) and
  arg.getType().getUnspecifiedType() instanceof FloatingPointType and
  // Exclude explicit casts that show developer intent
  not arg instanceof Cast and
  // In project source, not system headers
  call.getFile().getRelativePath().matches("%Icc%")
select call,
  "icFtoD() called with floating-point argument $@. " +
  "icFtoD expects icS15Fixed16Number (int32). Implicit float-to-int truncation " +
  "silently corrupts the value (CWE-681). Use the float value directly or " +
  "call the appropriate conversion function.",
  arg, arg.toString()
