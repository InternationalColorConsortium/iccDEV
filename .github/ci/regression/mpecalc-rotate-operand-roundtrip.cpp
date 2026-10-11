/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Regression test for the rotl/rotr operand that SIccCalcOp::Describe dropped
// (#2265), IccProfLib/IccMpeCalc.cpp.
//
// The icSigRotateLeftOp/icSigRotateRightOp arm formatted its selectors into the
// scratch buffer and then fell straight to `break` without the `desc += buf;`
// every neighbouring arm ends with, so rotl and rotr always described themselves
// bare.  Reading that text back gives CIccFuncTokenizer::GetIndex() no operand,
// so it keeps the (1,1) initV1/initV2 it is called with for these ops and then
// subtracts them off again -- landing both selectors on 0.  CIccOpDefRotateLeft
// ::Exec uses v1 as the rotate count (nCopy = v1+1) and v2 as the offset
// (nPos = v2+1), so an iccToXml -> iccFromXml round trip silently rewrote
// rotl(3,2) as rotl(1,1) and changed the transform.  Where the original count
// exceeded the available stack the round trip also erased the underflow, turning
// an invalid profile into a valid one: Testing/CalcTest/calcUnderStack_rotl.icc
// and _rotr.icc reconstructed clean while their 72 siblings still rejected.
//
// The fix emits both selectors biased by the same one GetIndex() removes, and
// keeps the copy/posdup convention of omitting what the defaults already supply
// so ops written without an index are unchanged.
//
// Describe() composed with SetFunction() must be the identity on the op text.
// The two unindexed cases are the control: they are stable both pre- and
// post-fix, so a run that reports only those as OK has not silently passed.
//
// Exit code 0 = pass, 1 = an operand was lost in the round trip.

#include "IccMpeCalc.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

// Collapse runs of whitespace so the comparison tracks the op text, not the
// blanks Describe() lays out.
static std::string squeeze(const std::string &s)
{
  std::string out;
  bool pending = false;
  for (std::string::const_iterator i = s.begin(); i != s.end(); ++i) {
    const char c = *i;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { pending = true; continue; }
    if (pending && !out.empty()) out += ' ';
    pending = false;
    out += c;
  }
  return out;
}

static void checkRoundTrip(const char *szFuncDef)
{
  CIccCalculatorFunc fn(NULL);
  std::string sReport;

  if (fn.SetFunction(szFuncDef, sReport) != icFuncParseNoError) {
    std::printf("FAIL: %s did not parse (%s)\n", szFuncDef, sReport.c_str());
    ++g_failures;
    return;
  }

  std::string sDescribed;
  fn.Describe(sDescribed, 100, 0);

  const std::string sWanted = squeeze(szFuncDef);
  const std::string sGot = squeeze(sDescribed);

  if (sWanted == sGot) {
    std::printf("ok:   %s\n", sWanted.c_str());
  }
  else {
    std::printf("FAIL: %s described back as %s\n", sWanted.c_str(), sGot.c_str());
    ++g_failures;
  }
}

int main()
{
  // Controls: no operand to lose, identical before and after the fix.
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotl }");
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotr }");

  // The regression: pre-fix every one of these described back bare.
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotl(3) }");
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotl(3,2) }");
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotr(4) }");
  checkRoundTrip("{ 1 2 3 4 5 6 7 8 rotr(4,3) }");

  if (g_failures) {
    std::printf("\n%d operand round trip(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
