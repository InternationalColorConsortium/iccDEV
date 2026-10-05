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

// #2751.  A calculator's function finds its sub-elements through a pointer
// back to the calculator that owns it (CIccCalculatorFunc::m_pCalc): the
// calc(), curv(), mtx() and similar operations call m_pCalc->GetElem().
// CIccMpeCalculator's copy constructor and operator= copied the function with
// NewCopy(), which keeps the source's pointer, so the copy's function went on
// reading the SOURCE calculator's sub-elements.  Once the source was destroyed
// that was a use after free.  iccApplySearch reaches it: CIccCmm::AddXform()
// copies the destination profile, CIccCmmSearch::Begin() deletes the original,
// and the copy's Begin() then validates its calculator through the dangling
// pointer.
//
// Covered, each able to go red on its own:
//   - the copy constructor points the copied function at the copy;
//   - a calculator inside a copied multiProcessElementType tag still applies
//     after the original tag is deleted (the iccApplySearch path);
//   - operator= points the copied function at the target, and keeps the
//     sub-elements: it sized the copy by the target's own count, zero after
//     SetSize(0,0), so every sub-element was dropped;
//   - the assigned calculator still applies after the source is deleted;
//   - operator= copies the temporary-channel state the copy constructor copies;
//   - SetCalcFunc(icCalculatorFuncPtr) points a function built for another
//     calculator at the one it is installed in;
//   - self-assignment of a calculator keeps its function and sub-elements
//     (SetSize(0,0) released them first);
//   - self-assignment of a function keeps its operations.  func.m_Op is m_Op
//     there, so after freeing it the copy filled the new buffer from itself,
//     leaving the operations uninitialised.
//
// On an unfixed build the copied-tag check reads freed memory and the
// function self-assignment check reads uninitialised memory, so without a
// sanitizer either can pass by luck.  The owner and count checks are red on
// any unfixed build.

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

// Reads the protected members under test.  A pointer to member formed through
// the derived class may be applied to any object of the base class.
struct FuncProbe : public CIccCalculatorFunc
{
  static CIccMpeCalculator *Owner(const CIccCalculatorFunc *pFunc)
  {
    return pFunc->*(&FuncProbe::m_pCalc);
  }
};

struct CalcProbe : public CIccMpeCalculator
{
  static CIccCalculatorFunc *Func(const CIccMpeCalculator *pCalc)
  {
    return pCalc->*(&CalcProbe::m_calcFunc);
  }
  static icUInt32Number NumSubElem(const CIccMpeCalculator *pCalc)
  {
    return pCalc->*(&CalcProbe::m_nSubElem);
  }
  static icUInt32Number NumTempChannels(const CIccMpeCalculator *pCalc)
  {
    return pCalc->*(&CalcProbe::m_nTempChannels);
  }
};

static const char *kSub = "{ in(0) 2 mul out(0) }";        // doubles its input
static const char *kMain = "{ in(0) calc(0) 1 tput(3) out(0) }";

// A one-in, one-out calculator whose function calls sub-element 0, a
// sub-calculator that doubles its input.  tput(3) gives it four temporaries.
static CIccMpeCalculator *makeCalc()
{
  CIccMpeCalculator *pSub = new CIccMpeCalculator(1, 1);
  std::string sReport;
  if (pSub->SetCalcFunc(kSub, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "  cannot parse \"%s\": %s\n", kSub, sReport.c_str());
    delete pSub;
    return NULL;
  }

  CIccMpeCalculator *pCalc = new CIccMpeCalculator(1, 1);
  pCalc->SetSubElem(0, pSub);
  if (pCalc->SetCalcFunc(kMain, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "  cannot parse \"%s\": %s\n", kMain, sReport.c_str());
    delete pCalc;
    return NULL;
  }
  return pCalc;
}

// Begin and apply a tag once; false if it does not begin.
static bool applyTag(CIccTagMultiProcessElement &mpe, icFloatNumber in, icFloatNumber &out)
{
  if (!mpe.Begin())
    return false;

  CIccApplyTagMpe *pApply = mpe.GetNewApply();
  if (!pApply)
    return false;

  icFloatNumber src[1] = { in };
  icFloatNumber dst[1] = { -1.0f };
  mpe.Apply(pApply, dst, src);
  out = dst[0];

  delete pApply;
  return true;
}

int main()
{
  std::fprintf(stdout, "=== #2751 a copied calculator's function uses the copy ===\n");

  // ---- copy constructor ----
  {
    CIccMpeCalculator *pCalc = makeCalc();
    check(pCalc != NULL, "calculator with a sub-calculator builds");
    if (pCalc) {
      CIccMpeCalculator *pCopy = pCalc->NewCopy();
      check(FuncProbe::Owner(CalcProbe::Func(pCalc)) == pCalc,
            "control: the source's function points at the source");
      check(FuncProbe::Owner(CalcProbe::Func(pCopy)) == pCopy,
            "copy constructor: the copied function points at the copy");
      delete pCopy;
      delete pCalc;
    }
  }

  // ---- the iccApplySearch path: copy the tag, delete the original, apply ----
  {
    CIccMpeCalculator *pCalc = makeCalc();
    if (pCalc) {
      CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(1, 1);
      pTag->Attach(pCalc);                   // the tag owns it from here
      CIccTagMultiProcessElement copy(*pTag);
      delete pTag;

      icFloatNumber out = -1.0f;
      bool ok = applyTag(copy, 0.25f, out);
      check(ok, "copied tag begins after the original is deleted");
      check(ok && out == 0.5f, "copied tag applies its own sub-calculator (0.25 -> 0.5)");
    }
  }

  // ---- operator= ----
  {
    CIccMpeCalculator *pCalc = makeCalc();
    if (pCalc) {
      CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(1, 1);
      pTag->Attach(pCalc);
      icFloatNumber out = -1.0f;
      check(applyTag(*pTag, 0.25f, out) && out == 0.5f,
            "control: the source calculator begins and applies (0.25 -> 0.5)");

      CIccMpeCalculator *pDst = new CIccMpeCalculator(1, 1);
      *pDst = *pCalc;
      check(FuncProbe::Owner(CalcProbe::Func(pDst)) == pDst,
            "operator=: the copied function points at the target");
      check(CalcProbe::NumSubElem(pDst) == 1,
            "operator=: the target keeps the source's one sub-element");
      check(CalcProbe::NumTempChannels(pDst) == CalcProbe::NumTempChannels(pCalc) &&
            CalcProbe::NumTempChannels(pCalc) == 4,
            "operator=: the target copies the source's four temporaries");

      delete pTag;                           // and with it the source
      CIccTagMultiProcessElement dstTag(1, 1);
      dstTag.Attach(pDst);
      out = -1.0f;
      bool ok = applyTag(dstTag, 0.25f, out);
      check(ok, "operator=: the target begins after the source is deleted");
      check(ok && out == 0.5f, "operator=: the target applies its own sub-calculator (0.25 -> 0.5)");
    }
  }

  // ---- SetCalcFunc(icCalculatorFuncPtr) with a function built for another calculator ----
  {
    CIccMpeCalculator *pOther = makeCalc();
    CIccMpeCalculator *pCalc = makeCalc();
    if (pOther && pCalc) {
      CIccCalculatorFunc *pFunc = CalcProbe::Func(pOther)->NewCopy();
      check(FuncProbe::Owner(pFunc) == pOther,
            "control: a copied function still points at the calculator it came from");
      pCalc->SetCalcFunc(pFunc);
      check(FuncProbe::Owner(CalcProbe::Func(pCalc)) == pCalc,
            "SetCalcFunc: the installed function points at its new calculator");

      delete pOther;
      pOther = NULL;
      CIccTagMultiProcessElement tag(1, 1);
      tag.Attach(pCalc);
      pCalc = NULL;
      icFloatNumber out = -1.0f;
      bool ok = applyTag(tag, 0.25f, out);
      check(ok, "SetCalcFunc: the calculator begins after the function's builder is deleted");
      check(ok && out == 0.5f, "SetCalcFunc: the calculator applies its own sub-calculator (0.25 -> 0.5)");
    }
    delete pOther;
    delete pCalc;
  }

  // ---- self-assignment of a calculator ----
  {
    CIccMpeCalculator *pCalc = makeCalc();
    if (pCalc) {
      CIccMpeCalculator &alias = *pCalc;
      *pCalc = alias;
      check(CalcProbe::Func(pCalc) != NULL,
            "calculator self-assignment keeps the function");
      check(CalcProbe::NumSubElem(pCalc) == 1,
            "calculator self-assignment keeps the sub-element");
      delete pCalc;
    }
  }

  // ---- self-assignment of a function ----
  {
    CIccMpeCalculator *pCalc = makeCalc();
    if (pCalc) {
      CIccCalculatorFunc *pFunc = CalcProbe::Func(pCalc);
      icUInt32Number nOps = pFunc->GetNumOps();
      std::string before, after;
      pFunc->Describe(before);
      CIccCalculatorFunc &alias = *pFunc;
      *pFunc = alias;
      pFunc->Describe(after);
      check(nOps > 0 && pFunc->GetNumOps() == nOps,
            "function self-assignment keeps the operation count");
      check(!before.empty() && after == before,
            "function self-assignment keeps the operations");
      delete pCalc;
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[calc-copy-owner] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[calc-copy-owner] all checks passed\n");
  return 0;
}
