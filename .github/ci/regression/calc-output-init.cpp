/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// CTest contract for calculator output channels a program does not write
// (#2702, #2705).
//
// Only an out() operation writes a calculator's output. A main function can
// end before its out(), branch around it, or have none, and the channels it
// skips used to keep whatever the caller's buffer held: a CMM pixel buffer from
// malloc, or CIccApplyBPC's XYZbp on the stack, which MemorySanitizer reported
// further down the chain. CIccCalculatorFunc::Apply() now zeroes them at every
// invocation, the rule ICC.2-2023 11.2.1 states for temporary channels, except
// when the output buffer is the input buffer.
//
// Each program runs through its enclosing tag's apply object with a
// destination pre-filled with a sentinel, so an unwritten channel shows as the
// sentinel on an unfixed library. The reused-buffer case applies twice without
// refilling, as a CMM does across pixels.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeCalc.h"
#include "IccMpeBasic.h"
#include "IccTagMPE.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "calc-output-init: FAIL  %s\n", label);
    g_failures++;
  }
}

const icFloatNumber kSentinel = 12345.0f;

// A 3-in, 3-out multiProcessElementType tag holding one calculator, begun and
// ready to apply. Returns false if the program does not parse or begin.
struct CalcTag {
  CIccTagMultiProcessElement mpe;
  CIccApplyTagMpe *pApply;

  CalcTag() : mpe(3, 3), pApply(NULL) {}
  ~CalcTag() { delete pApply; }

  bool Init(const char *szProgram)
  {
    CIccMpeCalculator *pCalc = new CIccMpeCalculator(3, 3);
    std::string sReport;
    if (pCalc->SetCalcFunc(szProgram, sReport) != icFuncParseNoError) {
      std::fprintf(stderr, "calc-output-init: cannot parse \"%s\": %s\n", szProgram, sReport.c_str());
      delete pCalc;
      return false;
    }
    mpe.Attach(pCalc);
    if (!mpe.Begin())
      return false;
    pApply = mpe.GetNewApply();
    return pApply != NULL;
  }

  void Apply(icFloatNumber *dst, icFloatNumber a, icFloatNumber b, icFloatNumber c)
  {
    icFloatNumber src[3] = { a, b, c };
    mpe.Apply(pApply, dst, src);
  }
};

void fillSentinel(icFloatNumber *dst)
{
  dst[0] = dst[1] = dst[2] = kSentinel;
}

void testNoOut()
{
  // The #2705 shape: a main function with no out() at all.
  CalcTag t;
  bool ok = t.Init("{ in(0,3) pop(3) }");
  check(ok, "a program with no out() parses and begins");
  if (!ok)
    return;

  icFloatNumber dst[3];
  fillSentinel(dst);
  t.Apply(dst, 0.25f, 0.5f, 0.75f);
  check(dst[0] == 0 && dst[1] == 0 && dst[2] == 0,
        "a program with no out() leaves all three outputs zero");
}

void testPartialOut()
{
  CalcTag t;
  bool ok = t.Init("{ in(0) out(0) }");
  check(ok, "a program writing only out(0) parses and begins");
  if (!ok)
    return;

  icFloatNumber dst[3];
  fillSentinel(dst);
  t.Apply(dst, 0.25f, 0.5f, 0.75f);
  check(dst[0] == 0.25f, "out(0) carries the value written");
  check(dst[1] == 0 && dst[2] == 0, "outputs 1 and 2, never written, are zero");
}

void testReusedBuffer()
{
  // The branch writes all three outputs only when in(0) is non-zero. Applied
  // twice into the same buffer, the second pixel must not inherit the first.
  CalcTag t;
  bool ok = t.Init("{ in(0) if { 1 1 1 out(0,3) } }");
  check(ok, "the conditional-out program parses and begins");
  if (!ok)
    return;

  icFloatNumber dst[3];
  fillSentinel(dst);
  t.Apply(dst, 1, 0, 0);
  check(dst[0] == 1 && dst[1] == 1 && dst[2] == 1, "the branch taken writes ones");

  t.Apply(dst, 0, 0, 0);
  check(dst[0] == 0 && dst[1] == 0 && dst[2] == 0,
        "the branch skipped leaves zeros, not the previous pixel's ones");
}

// A calculator in the middle of a chain writes into the tag's own pixel
// buffers, which the elements take turns using. With four elements the
// calculator's destination is the buffer the first element filled with 7s, so
// an unwritten channel kept a 7 from two elements earlier.
void testMiddleOfChain()
{
  CIccTagMultiProcessElement mpe(3, 3);

  CIccMpeMatrix *pSevens = new CIccMpeMatrix();
  bool ok = pSevens->SetSize(3, 3);
  if (ok)
    for (int k = 0; k < 3; k++)
      pSevens->GetConstants()[k] = 7.0f;          // zero matrix plus 7s
  mpe.Attach(pSevens);

  CIccMpeMatrix *pIdentity1 = new CIccMpeMatrix();
  ok = ok && pIdentity1->SetSize(3, 3);
  if (ok)
    for (int k = 0; k < 3; k++)
      pIdentity1->GetMatrix()[k * 3 + k] = 1.0f;
  mpe.Attach(pIdentity1);

  CIccMpeCalculator *pCalc = new CIccMpeCalculator(3, 3);
  std::string sReport;
  ok = ok && pCalc->SetCalcFunc("{ in(0) out(0) }", sReport) == icFuncParseNoError;
  mpe.Attach(pCalc);

  CIccMpeMatrix *pIdentity2 = new CIccMpeMatrix();
  ok = ok && pIdentity2->SetSize(3, 3);
  if (ok)
    for (int k = 0; k < 3; k++)
      pIdentity2->GetMatrix()[k * 3 + k] = 1.0f;
  mpe.Attach(pIdentity2);

  check(ok, "the four-element chain is built");
  if (!ok || !mpe.Begin())
    return;
  CIccApplyTagMpe *pApply = mpe.GetNewApply();
  check(pApply != NULL, "the four-element chain gives an apply object");
  if (!pApply)
    return;

  icFloatNumber src[3] = { 0.25f, 0.5f, 0.75f };
  icFloatNumber dst[3];
  fillSentinel(dst);
  mpe.Apply(pApply, dst, src);
  check(dst[0] == 7.0f, "the calculator passes channel 0 through the chain");
  check(dst[1] == 0 && dst[2] == 0,
        "mid-chain, the channels the calculator skipped are zero, not the first element's 7s");
  delete pApply;
}

// A direct caller of the element's apply object may pass the same buffer for
// input and output. The reset is skipped then, so in() still reads the input.
void testInPlace()
{
  CalcTag t;
  bool ok = t.Init("{ in(0,3) out(0,3) }");
  check(ok, "the in-place identity program parses and begins");
  if (!ok)
    return;

  CIccApplyMpe *pElemApply = t.pApply->begin()->ptr;
  icFloatNumber buf[3] = { 0.25f, 0.5f, 0.75f };
  pElemApply->Apply(buf, buf);
  check(buf[0] == 0.25f && buf[1] == 0.5f && buf[2] == 0.75f,
        "an in-place call still reads its inputs");
}

void testFullOut()
{
  // Positive control: a program that writes every output is unaffected.
  CalcTag t;
  bool ok = t.Init("{ in(0,3) out(0,3) }");
  check(ok, "the identity program parses and begins");
  if (!ok)
    return;

  icFloatNumber dst[3];
  fillSentinel(dst);
  t.Apply(dst, 0.25f, 0.5f, 0.75f);
  check(dst[0] == 0.25f && dst[1] == 0.5f && dst[2] == 0.75f,
        "the identity program passes all three inputs through");
}

} // namespace

int main()
{
  testNoOut();
  testPartialOut();
  testReusedBuffer();
  testMiddleOfChain();
  testInPlace();
  testFullOut();

  if (g_failures)
    std::fprintf(stderr, "calc-output-init: %d failure(s)\n", g_failures);
  else
    std::printf("calc-output-init: all checks passed\n");
  return g_failures;
}
