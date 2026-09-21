// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       json-calculator-newcopy.cpp

    Contains:   CTest helper for copying a JSON calculator element (#2622).

    CIccMpeJsonCalculator did not override NewCopy(), so the virtual call
    resolved to CIccMpeCalculator::NewCopy() and returned a plain
    CIccMpeCalculator: a copied JSON calculator silently stopped being one.

    The copy that exposed it is on the way to JSON.  CIccMpeJsonCurveSet
    serializes a SampledCalculatorCurve through CIccSampledCalculatorCurveJson,
    which copies the curve, and the curve's copy constructor copies its
    calculator through NewCopy().  The copied calculator had no JSON extension,
    so the curve's ToJson() returned false, the curve set's ToJson() returned
    false, and the tag writer ignored that and wrote the element with no
    "curves" array.  iccToJson exited 0 on a document iccFromJson then refused.
    Reproduced on the tracked Testing/Display/Rec2100HlgFullCC.xml, which
    CreateAllProfiles.sh does not build, so no corpus run ever reached it.

    CIccMpeXmlCalculator already overrides NewCopy() for exactly this case,
    which is why the XML path worked on the same profile.

    The checks below are, in order: the copy keeps its JSON class; the copy
    writes the same JSON as the original, so copying only the calculator and
    not the parse-time scratch state loses nothing; and the real failing path,
    a curve set holding a SampledCalculatorCurve, now serializes and reads back.
*/

#include "IccProfile.h"
#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccMpeJson.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-calculator-newcopy] FAIL %s: %s\n", label, what);
  }
}

// A one-channel calculator parsed from JSON, so it is a CIccMpeJsonCalculator
// built the way iccFromJson builds one.  One channel in and out is what a
// SampledCalculatorCurve requires of its calculator.
CIccMpeJsonCalculator *makeCalc()
{
  CIccMpeJsonCalculator *p = new CIccMpeJsonCalculator;
  IccJson j = IccJson::object();
  j["inputChannels"] = 1;
  j["outputChannels"] = 1;
  j["mainFunction"] = "{ in(0,1) 2 mul out(0,1) }";
  std::string parseStr;
  if (!p->ParseJson(j, parseStr)) {
    std::fprintf(stderr, "[json-calculator-newcopy] setup: %s\n", parseStr.c_str());
    delete p;
    return NULL;
  }
  return p;
}

// True only for a calculator that the JSON writers will serialize: the
// SampledCalculatorCurve writer tests exactly this extension name.
bool isJsonCalc(CIccMpeCalculator *pCalc)
{
  if (!pCalc || std::strcmp(pCalc->GetClassName(), "CIccMpeJsonCalculator") != 0)
    return false;
  IIccExtensionMpe *pExt = pCalc->GetExtension();
  return pExt && !std::strcmp(pExt->GetExtClassName(), "CIccMpeJson");
}

} // namespace

int main()
{
  // ---- NewCopy keeps the JSON class --------------------------------------
  CIccMpeJsonCalculator *pCalc = makeCalc();
  check(pCalc != NULL, "setup", "could not parse the calculator");
  if (!pCalc)
    return 1;

  CIccMpeCalculator *pCopy = pCalc->NewCopy();
  check(isJsonCalc(pCopy), "newcopy class",
        "NewCopy() returned a plain CIccMpeCalculator, not a CIccMpeJsonCalculator");

  // ---- ... and writes the same JSON --------------------------------------
  // The copy deliberately leaves the parse-time maps empty; ToJson() must not
  // depend on them, or a copied calculator would serialize differently.
  CIccMpeJsonCalculator *pJsonCopy = dynamic_cast<CIccMpeJsonCalculator*>(pCopy);
  if (pJsonCopy) {
    IccJson orig, copied;
    check(pCalc->ToJson(orig), "original writes", "the original calculator did not serialize");
    check(pJsonCopy->ToJson(copied), "copy writes", "the copied calculator did not serialize");
    check(orig == copied, "copy is faithful", "the copy serializes differently from the original");
  }
  delete pCopy;
  delete pCalc;

  // ---- the #2622 path: a curve set holding a SampledCalculatorCurve -------
  CIccMpeJsonCurveSet curveSet;
  check(curveSet.SetSize(1), "curve set", "SetSize(1) failed");

  CIccSampledCalculatorCurve *pCurve = new CIccSampledCalculatorCurve(0.0f, 1.0f);
  CIccMpeJsonCalculator *pCurveCalc = makeCalc();
  check(pCurveCalc && pCurve->SetCalculator(pCurveCalc), "curve", "SetCalculator failed");
  check(curveSet.SetCurve(0, pCurve), "curve set", "SetCurve failed");

  IccJson j = IccJson::object();
  bool bWritten = curveSet.ToJson(j);
  check(bWritten, "curve set writes",
        "CurveSetElement ToJson() returned false for a SampledCalculatorCurve");
  check(j.contains("curves") && j["curves"].is_array() && j["curves"].size() == 1,
        "curves array", "the element was written without its curves array");
  if (j.contains("curves") && j["curves"].is_array() && j["curves"].size() == 1) {
    const IccJson &c = j["curves"][0];
    check(c.contains("type") && c["type"] == "SampledCalculatorCurve", "curve type",
          "the curve was not written as a SampledCalculatorCurve");
    check(c.contains("calculator"), "curve calculator", "the curve was written without its calculator");
  }

  // ---- ... and what it writes, its own reader accepts ---------------------
  if (bWritten) {
    CIccMpeJsonCurveSet readBack;
    std::string parseStr;
    check(readBack.ParseJson(j, parseStr), "reads back",
          ("iccFromJson's reader refused the writer's own output: " + parseStr).c_str());
  }

  if (g_fail) {
    std::fprintf(stderr, "[json-calculator-newcopy] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[json-calculator-newcopy] all checks passed\n");
  return 0;
}
