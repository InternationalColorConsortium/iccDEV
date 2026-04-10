/**
 * @name Unterminated UCS-2 buffer passed to SetText
 * @description SetText scans for null terminator; passing data() from a vector
 *              without a trailing zero causes heap-buffer-overflow
 * @kind problem
 * @problem.severity error
 * @security-severity 8.0
 * @precision medium
 * @id icc/unterminated-ucs2-settext
 * @tags security
 *       external/cwe/cwe-125
 *       external/cwe/cwe-170
 */

import cpp

/**
 * A call to `std::vector::data()` that produces a pointer used as
 * the first argument to `CIccLocalizedUnicode::SetText()`.
 */
class VectorDataCall extends FunctionCall {
  VectorDataCall() {
    this.getTarget().getName() = "data" and
    this.getTarget().getDeclaringType().getSimpleName() = "vector"
  }
}

/**
 * A call to `SetText` on a `CIccLocalizedUnicode` object (or pointer)
 * whose first argument is a `std::vector::data()` return value.
 */
class SetTextCall extends FunctionCall {
  SetTextCall() {
    this.getTarget().getName() = "SetText" and
    this.getTarget().getDeclaringType().getName() = "CIccLocalizedUnicode"
  }
}

/**
 * Holds when `vectorVar` is the qualifier (the vector object) on which
 * `.data()` is called in `dataCall`.
 */
predicate vectorOfDataCall(VectorDataCall dataCall, Variable vectorVar) {
  dataCall.getQualifier().(VariableAccess).getTarget() = vectorVar
}

/**
 * Holds when the vector `v` has a trailing zero element assigned after
 * construction, e.g. `ucs2[wtext.size()] = 0;` or `v.push_back(0)`.
 */
predicate hasNullTerminator(Variable v) {
  // Pattern 1: direct subscript assignment of 0, e.g. ucs2[N] = 0
  exists(AssignExpr assign |
    assign.getRValue().getValue() = "0" and
    exists(ArrayExpr ae |
      ae = assign.getLValue() and
      ae.getArrayBase().(VariableAccess).getTarget() = v
    )
  )
  or
  // Pattern 2: push_back(0) on the vector
  exists(FunctionCall pushBack |
    pushBack.getTarget().getName() = "push_back" and
    pushBack.getQualifier().(VariableAccess).getTarget() = v and
    pushBack.getArgument(0).getValue() = "0"
  )
  or
  // Pattern 3: emplace_back(0) on the vector
  exists(FunctionCall emplace |
    emplace.getTarget().getName() = "emplace_back" and
    emplace.getQualifier().(VariableAccess).getTarget() = v and
    emplace.getArgument(0).getValue() = "0"
  )
}

/**
 * Holds when the vector was constructed with +1 extra capacity, suggesting
 * room was reserved for a null terminator. Matches patterns like
 * `vector<icUInt16Number> ucs2(wtext.size() + 1)`.
 */
predicate hasPlusOneSize(Variable v) {
  exists(ConstructorCall ctor, AddExpr add |
    ctor = v.getInitializer().getExpr() and
    add = ctor.getAnArgument() and
    add.getAnOperand().getValue() = "1"
  )
}

from SetTextCall setTextCall, VectorDataCall dataCall, Variable vectorVar
where
  // The first argument to SetText is the data() return
  setTextCall.getArgument(0) = dataCall and
  // Identify the underlying vector variable
  vectorOfDataCall(dataCall, vectorVar) and
  // No evidence of null termination
  not hasNullTerminator(vectorVar) and
  // No +1 in the constructor size (which alone does not guarantee termination,
  // but combined with the missing-assignment check reduces false positives)
  not hasPlusOneSize(vectorVar)
select setTextCall,
  "SetText() receives buffer from $@ without a trailing null terminator. " +
  "SetText scans for a null using `for (len=0; *pBuf; len++, pBuf++)` -- " +
  "an unterminated buffer causes heap-buffer-overflow (CWE-125/CWE-170).",
  dataCall, vectorVar.getName() + ".data()"
