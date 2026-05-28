/**
 * @name Signed char used as unsigned byte in UTF-8 decoder
 * @description ICC UTF-8 decoders should make signed-to-unsigned byte
 *              conversion explicit before byte classification. Implicit
 *              conversion from signed char to unsigned char can trigger
 *              sanitizer findings and hide parser bugs.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/signed-char-byte-conversion
 * @tags security
 *       correctness
 *       external/cwe/cwe-704
 */

import cpp

predicate isSetTextUtf8Decoder(Function f) {
  f.getName() = "SetText" and
  f.getFile().getRelativePath().matches("%IccTagBasic.cpp") and
  exists(Parameter p | p.getFunction() = f and p.getName() = "szText")
}

predicate hasExplicitUnsignedCast(Expr e) {
  exists(StaticCast c |
    (c = e or c.getAChild*() = e) and
    c.getType().getName() = "unsigned char"
  ) or
  exists(CStyleCast c |
    (c = e or c.getAChild*() = e) and
    c.getType().getName() = "unsigned char"
  )
}

from Variable v, Expr init
where
  isSetTextUtf8Decoder(init.getEnclosingFunction()) and
  v.getType().getName() = "unsigned char" and
  init = v.getInitializer().getExpr() and
  init.toString().regexpMatch(".*szText\\[.*\\].*") and
  not hasExplicitUnsignedCast(init)
select init,
  "Signed char data from szText is assigned to an unsigned byte without an " +
  "explicit cast. Use static_cast<unsigned char>(...) before UTF-8 byte classification."
