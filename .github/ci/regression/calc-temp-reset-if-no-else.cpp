// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// #2707.  ICC.2-2023's calculatorElement clause: "At each invocation of a
// calculator (or sub-calculator) element it shall be assumed that all
// Temporary channel data are initialized to zero."
//
// CIccMpeCalculator::Apply() honours that with a memset, but only when
// SequenceNeedTempReset() finds a temporary that can be read before it is
// written; otherwise it skips the reset for speed.  That analysis walked an
// if's true branch from i+2 whatever followed the if.  The branch starts at
// i+2 only when an else op sits at i+1; without one it starts at i+1 -- the
// layout ParseFuncDef builds and ApplySequence runs.  So for an if with no
// else the branch's first op was never analysed.  When that op was a tget,
// the temporaries were not reset: the first invocation read uninitialised
// memory (the MemorySanitizer report) and every later one read whatever the
// previous invocation left behind.
//
// Zero-filling the buffer once at allocation (calloc in GetNewApply) would
// silence the first read and leave the second: the reset has to happen at
// every invocation, which is what fixing the analysis restores.  This test
// asserts the second invocation, so it catches that partial fix as well.
//
// The program under test:
//
//   in(0) if { tget(0) out(0) } 7 tput(0)
//
// With input 1 the branch runs and outputs temp 0, then the program stores 7
// in temp 0.  Each invocation must start with temp 0 at zero, so both outputs
// are 0.  Without the reset the second output is 7.
//
// Also covered, each able to go red on its own:
//   - a select.  The analysis walked a select's case and default bodies in a
//     line, as if all of them ran, so a write in one case masked a read after
//     the select took another branch.  It now answers "reset" for any select.
//     Invocation 1 runs case 0, which stores 7; invocation 2 takes the default
//     and must read 0, not that 7.
//   - an if-else whose true branch is only a tget.  With an else the true branch
//     starts at i+2; starting it at i+1 there would analyse the else op and miss
//     the branch's last op, which here is the tget.
//   - an if-else whose tget is in the else branch, which pins where the else
//     branch starts.
//
// Control: a branch that outputs a constant, so a 0 above cannot come from a
// branch that never ran.
//
// The first-invocation checks read memory the fix resets.  On an unfixed build
// that memory is uninitialised, so those checks can pass by luck there; the red
// signal is the second invocation.

#include <cstdio>
#include <string>

#include "IccMpeCalc.h"
#include "IccTagMPE.h"

int g_fail = 0;

static void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "  [FAIL] %s\n", what);
  }
  else {
    std::fprintf(stdout, "  [PASS] %s\n", what);
  }
}

// Apply a one-in, one-out calculator program twice through the enclosing tag's
// own apply object, as a transform does -- first to in1, then to in2.  Returns
// false if the program does not parse or begin.
static bool applyTwice(const char *szProgram, icFloatNumber in1,
                       icFloatNumber in2, icFloatNumber &first,
                       icFloatNumber &second)
{
  CIccTagMultiProcessElement mpe(1, 1);
  CIccMpeCalculator *pCalc = new CIccMpeCalculator(1, 1);
  std::string sReport;

  if (pCalc->SetCalcFunc(szProgram, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "  cannot parse \"%s\": %s\n", szProgram, sReport.c_str());
    delete pCalc;
    return false;
  }
  mpe.Attach(pCalc);                       // the tag owns it from here
  if (!mpe.Begin())
    return false;

  CIccApplyTagMpe *pApply = mpe.GetNewApply();
  if (!pApply)
    return false;

  icFloatNumber src[1] = { in1 };
  icFloatNumber dst[1] = { -1.0f };
  mpe.Apply(pApply, dst, src);
  first = dst[0];
  src[0] = in2;
  dst[0] = -1.0f;
  mpe.Apply(pApply, dst, src);
  second = dst[0];

  delete pApply;
  return true;
}

int main()
{
  std::fprintf(stdout, "=== #2707 calculator temporaries reset at each invocation ===\n");
  icFloatNumber a = 0, b = 0;

  // ---- the defect: if with no else, tget as the branch's first op ----
  bool ok = applyTwice("{ in(0) if { tget(0) out(0) } 7 tput(0) }", 1, 1, a, b);
  check(ok, "if-without-else program parses, begins and applies");
  if (ok) {
    check(a == 0, "first invocation reads temp 0 as zero");
    check(b == 0, "second invocation reads temp 0 as zero, not the 7 the first stored");
  }

  // ---- select: a write in case 0 must not mask the read after the default ----
  ok = applyTwice("{ in(0) sel case { 7 tput(0) } dflt { 1 tput(1) } tget(0) out(0) }", 0, 5, a, b);
  check(ok, "select program parses, begins and applies");
  if (ok) {
    check(a == 7, "select: invocation 1 runs case 0 and reads the 7 it stored");
    check(b == 0, "select: invocation 2 takes the default and reads temp 0 as zero, not 7");
  }

  // ---- if-else, true branch is only a tget: pins where the true branch starts ----
  ok = applyTwice("{ in(0) if { tget(0) } else { 0 } out(0) 7 tput(0) }", 1, 1, a, b);
  check(ok, "if-else (tget as the whole true branch) parses, begins and applies");
  if (ok)
    check(a == 0 && b == 0, "if-else true branch: temp 0 reads zero on both invocations");

  // ---- if-else, tget in the else branch: pins where the else branch starts ----
  ok = applyTwice("{ in(0) if { 0 } else { tget(0) } out(0) 7 tput(0) }", 0, 0, a, b);
  check(ok, "if-else (tget in the else branch) parses, begins and applies");
  if (ok)
    check(a == 0 && b == 0, "if-else else branch: temp 0 reads zero on both invocations");

  // ---- control: the branch really runs ----
  ok = applyTwice("{ in(0) if { 3 out(0) } 7 tput(0) }", 1, 1, a, b);
  check(ok, "control: constant-branch program parses, begins and applies");
  if (ok)
    check(a == 3 && b == 3, "control: the if branch runs and sets the output");

  if (g_fail) {
    std::fprintf(stderr, "[calc-temp-reset-if-no-else] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[calc-temp-reset-if-no-else] all checks passed\n");
  return 0;
}
