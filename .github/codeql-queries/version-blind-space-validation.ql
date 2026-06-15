/**
 * @name Header colour space validated in a condition without a version test
 * @description Some data colour space signatures are version-specific: the zero
 *              "no data" space (0x00000000) and the iccMAX N-channel spaces
 *              (ncXXXX) are only valid for v5/iccMAX profiles (ICC.1 7.2.8 /
 *              Table 15); a v2/v4 data colour space must come from Table 19.
 *              CIccInfo::IsValidSpace() is version-blind and accepts the ncXXXX
 *              family for any version, so using it directly as the test of an
 *              `if` that gates a header colour-space (m_Header.colorSpace /
 *              m_Header.pcs) error -- without that same condition consulting
 *              m_Header.version -- lets an iccMAX-only space pass on a v2/v4
 *              profile. This is the class of bug fixed in #1359 (which moved the
 *              data-colour-space check behind a version gate; the DeviceLink PCS
 *              check has the same shape).
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/version-blind-space-validation
 * @tags correctness
 *       spec-conformance
 *       external/cwe/cwe-393
 */

import cpp

/** A version-blind colour-space validity predicate from CIccInfo. */
predicate isVersionBlindSpaceCheck(Function f) {
  f.getName() = ["IsValidSpace", "IsValidSpectralSpace"]
}

/** Holds if `e` (or a descendant) reads a header field named `name`. */
predicate refsHeaderField(Expr e, string name) {
  exists(FieldAccess fa | fa = e.getAChild*() and fa.getTarget().getName() = name)
}

from FunctionCall vc, IfStmt ifs
where
  isVersionBlindSpaceCheck(vc.getTarget()) and
  // Validates a profile *header* colour space (the version-dependent fields).
  exists(FieldAccess fa |
    (fa = vc.getArgument(0) or fa = vc.getArgument(0).(Cast).getExpr()) and
    fa.getTarget().getName() = ["colorSpace", "pcs"]
  ) and
  // The check is used directly as (part of) an `if` condition that gates the
  // error -- i.e. its result is the validity decision, not pre-combined with a
  // version-aware flag (as #1359 restructured the data-colour-space check).
  vc = ifs.getCondition().getAChild*() and
  // ... and that same condition never consults the profile version.
  not refsHeaderField(ifs.getCondition(), "version") and
  // Restrict to the profile-library validation surface.
  vc.getFile().getRelativePath().matches("%IccProfLib/%")
select vc,
  "Header colour space is validated by " + vc.getTarget().getName() +
    "() directly in an `if` condition that never consults m_Header.version. " +
    "IsValidSpace() is version-blind, so iccMAX-only spaces (ncXXXX) can pass on a v2/v4 " +
    "profile. Gate the check by major version, as #1359 did for the data colour space."
