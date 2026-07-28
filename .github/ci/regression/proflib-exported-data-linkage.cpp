// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: a test that references an exported IccProfLib global *variable*
// could not be linked on a Windows shared build (#1888).
//
// The library is IccProfLib2.dll there, and its exports come from CMake's
// WINDOWS_EXPORT_ALL_SYMBOLS rather than from __declspec annotations -- MSVC is
// deliberately excluded from the ICCPROFLIBDLL_EXPORTS definition, a decision
// taken in #764. That mechanism auto-exports functions but not global data, and
// IccProfLib carries no dllexport/dllimport on its data, so a consumer emits a
// direct data reference with no __imp_ indirection and the link fails:
//
//   mpe-empty-identity.obj : error LNK2019: unresolved external symbol
//     "char const * const icMsgValidateWarning" (?icMsgValidateWarning@@3PEBDEB)
//   fatal error LNK1120: 3 unresolved externals
//
// Functions in the same headers thunk normally, which is why this went
// unnoticed until #1887 became the first regression test to touch library data.
// Linux and macOS have no import-library model and are unaffected, so a green
// local pre-flight proves nothing here -- only the Windows CI leg does.
//
// THIS TEST IS THE PROOF. Its value is that it links at all on Windows: it is
// built against ${ICCDEV_TEST_LIB_ICCPROFLIB}, which falls back to the static
// library on Windows shared builds exactly as the three affected tools already
// do. If that fallback is removed or the export model changes, this stops
// linking and the Windows leg goes red instead of the next author discovering it.
//
// It covers eight of the nine exported globals -- the four icMsgValidate*
// prefixes, both icD50XYZ arrays, and both solver pointers. The ninth, icInfo,
// is excluded because it has no definition at all; see the note in main().
//
// It also has a job on every platform. Because exported data cannot be linked
// from a test that also links IccXML (that would load IccProfLib twice, once
// inside the DLL and once in the executable), those tests assert on hard-coded
// literals instead -- see mpe-empty-identity.cpp and pawg-q4-xyz-pcs-decode.cpp.
// Hard-coded copies drift silently. Pinning the real globals here means a change
// to icMsgValidateWarning or icD50XYZ fails HERE, naming the literals that must
// be updated, rather than leaving those tests quietly asserting stale text.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccUtil.h"
#include "IccSolve.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[proflib-exported-data-linkage] FAIL: %s\n", what);
  }
}

bool near(icFloatNumber a, double b)
{
  return std::fabs((double)a - b) < 1e-4;
}

} // namespace

int main()
{
  // -----------------------------------------------------------------
  // 1. Validation report prefixes. These exact strings are what a log
  //    scan greps for and what the IccXML-linking tests hard-code.
  // -----------------------------------------------------------------
  check(icMsgValidateWarning != NULL && !std::strcmp(icMsgValidateWarning, "Warning! - "),
        "1: icMsgValidateWarning changed -- update the literals in "
        "mpe-empty-identity.cpp");
  check(icMsgValidateNonCompliant != NULL &&
          !std::strcmp(icMsgValidateNonCompliant, "NonCompliant! - "),
        "1: icMsgValidateNonCompliant changed");
  check(icMsgValidateCriticalError != NULL &&
          !std::strcmp(icMsgValidateCriticalError, "Error! - "),
        "1: icMsgValidateCriticalError changed -- update the literals in "
        "mpe-empty-identity.cpp");
  check(icMsgValidateInformation != NULL &&
          !std::strcmp(icMsgValidateInformation, "Information - "),
        "1: icMsgValidateInformation changed -- update the literals in "
        "mpe-empty-identity.cpp");

  // -----------------------------------------------------------------
  // 2. D50 constants. pawg-q4-xyz-pcs-decode.cpp hard-codes the actual
  //    triple because it cannot link this symbol; keep them in step.
  // -----------------------------------------------------------------
  check(near(icD50XYZ[0], 0.9642) && near(icD50XYZ[1], 1.0000) &&
          near(icD50XYZ[2], 0.8249),
        "2: icD50XYZ changed -- update the literal in pawg-q4-xyz-pcs-decode.cpp");
  check(near(icD50XYZxx[0], 96.42) && near(icD50XYZxx[1], 100.00) &&
          near(icD50XYZxx[2], 82.49),
        "2: icD50XYZxx changed");

  // -----------------------------------------------------------------
  // 3. Solver extension points. These are the documented way a caller
  //    installs its own matrix solver, so they are the exported data a
  //    downstream consumer is most likely to reach for.
  // -----------------------------------------------------------------
  check(g_pIccMatrixSolver != NULL, "3: g_pIccMatrixSolver is null");
  check(g_pIccMatrixInverter != NULL, "3: g_pIccMatrixInverter is null");

  // icInfo (IccUtil.h) is deliberately NOT covered. It is declared
  // `extern ICCPROFLIB_API CIccInfo icInfo;` but has no definition anywhere in
  // the tree, so referencing it fails to link on every platform, not just
  // Windows -- `nm -DC libIccProfLib2.so` lists the other exported globals and
  // not this one. That is a dangling public declaration rather than the export
  // problem this test covers, and nothing in-tree uses it. Adding it here would
  // break the Linux build too.

  if (!g_fail)
    std::printf("[proflib-exported-data-linkage] all assertions passed\n");

  return g_fail;
}
