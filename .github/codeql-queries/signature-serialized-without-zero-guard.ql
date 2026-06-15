/**
 * @name ICC signature serialized without a zero round-trip guard
 * @description An ICC four-byte signature serialized through the icGetSig*
 *              text helpers does not survive a round-trip when it is zero:
 *              icGetSigStr(0) / icGetColorSigStr(0) emit the literal text
 *              "NULL", and the inverse icGetSigVal("NULL") returns 0x4E554C4C
 *              (the ASCII bytes 'N','U','L','L'), not 0. The JSON serializer
 *              guards every such header field with `field ? helper(...) : ""`;
 *              a header field that the JSON serializer guards but another
 *              serializer (e.g. XML) emits unconditionally will silently
 *              corrupt a zero/NoData signature on round-trip. This is the
 *              class of bug fixed in #1356 (data colour space / PCS).
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id iccdev/signature-serialized-without-zero-guard
 * @tags correctness
 *       reliability
 *       round-trip
 *       external/cwe/cwe-704
 */

import cpp

/**
 * The icGetSig*-family text helpers that render a zero signature as the literal
 * string "NULL" (see IccUtil.cpp). icGetSigVal() cannot invert that "NULL" back
 * to 0, so a zero signature passed to one of these for serialization is lossy
 * unless the zero case is guarded out.
 */
predicate isSigToTextHelper(Function f) {
  f.getName() = ["icGetSigStr", "icGetColorSigStr", "icGetColorSig", "icGetSig"]
}

/**
 * Holds if `fc` renders header field `f` to text via a sig-to-text helper. The
 * signature value is the third positional argument (buf, bufSize, sig, ...);
 * accept a direct field access or one wrapped in an explicit cast.
 */
predicate sigHelperOnField(FunctionCall fc, Field f) {
  isSigToTextHelper(fc.getTarget()) and
  exists(FieldAccess fa |
    (fa = fc.getArgument(2) or fa = fc.getArgument(2).(Cast).getExpr()) and
    fa.getTarget() = f
  )
}

/**
 * Holds if the call `fc` is control-dependent on a condition that tests field
 * `f` -- covering both the ternary idiom `f ? helper(...) : ""` and an
 * enclosing `if (f != 0) { ... helper(...) }` block. Such a call never renders
 * a zero `f`, so it cannot corrupt the round-trip.
 */
predicate zeroGuardedOnField(FunctionCall fc, Field f) {
  // Ternary idiom: `f ? helper(...) : ""` -- the call lives in the then branch
  // of a ConditionalExpr whose condition references field f.
  exists(ConditionalExpr ce, FieldAccess fa |
    fc = ce.getThen().getAChild*() and
    fa = ce.getCondition().getAChild*() and
    fa.getTarget() = f
  )
  or
  // Enclosing `if (f != 0) { ... helper(...) }` block guarding the same field.
  exists(IfStmt ifs, FieldAccess fa |
    fc.getEnclosingStmt().getParentStmt*() = ifs.getThen() and
    fa = ifs.getCondition().getAChild*() and
    fa.getTarget() = f
  )
}

/** Holds if `fc` serializes a signature inside a `*ToXml*` function. */
predicate inXmlSerializer(FunctionCall fc) {
  fc.getEnclosingFunction().getName().matches("%ToXml%")
}

/**
 * Holds if header field `f` is serialized in a `*ToJson*` function with the
 * zero case guarded -- proof that the field can legitimately be zero and that
 * the project's own convention is to guard it.
 */
predicate jsonGuardsField(Field f) {
  exists(FunctionCall jc |
    sigHelperOnField(jc, f) and
    jc.getEnclosingFunction().getName().matches("%ToJson%") and
    zeroGuardedOnField(jc, f)
  )
}

from FunctionCall xmlCall, Field f
where
  sigHelperOnField(xmlCall, f) and
  inXmlSerializer(xmlCall) and
  not zeroGuardedOnField(xmlCall, f) and
  jsonGuardsField(f)
select xmlCall,
  "Header signature field '" + f.getName() + "' is serialized unconditionally here, but the " +
    "JSON serializer guards its zero case. A zero/NoData signature will not round-trip: " +
    "icGetSig*(0) emits \"NULL\" and icGetSigVal(\"NULL\") returns 0x4E554C4C. Guard with " +
    "`" + f.getName() + " ? helper(...) : \"\"` to match the JSON serializer (see #1356)."
