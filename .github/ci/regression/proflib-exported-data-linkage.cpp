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
// IccProfLib carried no dllexport/dllimport on its data, so a consumer emitted a
// direct data reference with no __imp_ indirection and the link failed:
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
// #2219 fixed that: the eight globals carry ICCPROFLIB_DATA_API, real
// dllexport/dllimport kept separate from the incomplete ICCPROFLIB_API. This
// test was written before the fix and linked IccProfLib2-static on Windows to
// sidestep it, exactly as three tools did; #2154 retired that fallback, so it
// now links IccProfLib2.dll like every other regression executable and its LINK
// is once again an assertion. Note where the failure lands: an in-tree target
// that cannot link takes the whole Windows BUILD down, which is why
// iccdev.proflib-exported-data-dll-linkage exists to compile the same
// references out of tree and report the loss as one red test instead.
//
// It covers all eight exported globals -- the four icMsgValidate* prefixes,
// both icD50XYZ arrays, and both solver pointers. A ninth declaration, icInfo,
// was excluded here because it had no definition at all; #1897 removed it, and
// iccdev.proflib-exported-global-definitions now audits the headers against the
// built library so a replacement cannot appear unnoticed. See the note in main().
//
// It also has a job on every platform. Several tests assert on hard-coded copies
// of these values rather than reading the globals: mpe-empty-identity.cpp spells
// out three of the icMsgValidate* prefixes and pawg-q4-xyz-pcs-decode.cpp spells
// out the icD50XYZ triple. They hard-code by history rather than by necessity --
// both were written while reading these globals still failed to link on Windows.
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

  // There is no ninth check. IccUtil.h used to declare
  // `extern ICCPROFLIB_API CIccInfo icInfo;` with no definition anywhere in the
  // tree, so referencing it failed to link on every platform, not just Windows
  // -- `nm -DC libIccProfLib2.so` listed the eight globals above and not that
  // one. It could not be covered here for that exact reason: adding it would
  // have broken this test's own link rather than failing an assertion. #1897
  // removed the declaration; the audit that generalizes it,
  // iccdev.proflib-exported-global-definitions, reads the symbol table instead
  // of referencing the symbols, which is why it can report the case this test
  // structurally cannot.

  if (!g_fail)
    std::printf("[proflib-exported-data-linkage] all assertions passed\n");

  return g_fail;
}
