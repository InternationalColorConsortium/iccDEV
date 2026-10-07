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

/*
 * json-mpe-reserved-roundtrip.cpp -- the reserved word of a tint array's
 * number array and of a calculator's sub-elements survives JSON.
 *
 * The binary format keeps both.  Through JSON:
 *
 *   - CIccMpeJsonTintArray wrote and read none of the number array tag's
 *     reserved word.  The element object's "Reserved" is the element's own,
 *     written by the caller, so the array's word is "arrayReserved", omitted
 *     when zero.
 *   - CIccMpeJsonCalculator::ToJson wrote each sub-element's "Reserved", but
 *     ParseJson never read it back.
 *
 * Cases:
 *   1. A tint array whose number array has reserved 7 writes arrayReserved 7
 *      and parses back to 7.
 *   2. Control: a zero reserved word writes no arrayReserved key.
 *   3. A calculator whose sub-elements carry "Reserved" 5 and none parses to 5
 *      and 0, and writes 5 back on the first sub-element only.
 *   4. A "Reserved" of 3000000000, past an int, round-trips on a sub-element
 *      and is written as an unsigned number.
 *   5. Both reads refuse 1.5, -1, "7" and 4294967296 rather than truncating,
 *      wrapping or dropping them, as the tone map reader does (#2621).
 *
 * Cases 1, 3, 4 and 5 fail on an unfixed build.
 */

#include "IccMpeJson.h"
#include "IccTagJson.h"
#include "IccMpeJsonFactory.h"
#include "IccTagJsonFactory.h"
#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[json-mpe-reserved-roundtrip] FAIL %s\n", what);
  }
}

IccJson tintJson()
{
  IccJson j = IccJson::object();
  j["inputChannels"] = 1;
  j["outputChannels"] = 1;
  j["values"] = IccJson::array({0.0, 0.5, 1.0});
  return j;
}

// Case 1.
void arrayReservedRoundTrips()
{
  CIccMpeJsonTintArray src;
  std::string parseStr;
  if (!src.ParseJson(tintJson(), parseStr) || !src.GetArray()) {
    std::fprintf(stderr, "%s", parseStr.c_str());
    check(false, "1: the tint array did not parse");
    return;
  }
  src.GetArray()->m_nReserved = 7;

  IccJson out = IccJson::object();
  check(src.ToJson(out), "1: ToJson returned false");
  check(out.contains("arrayReserved") && out["arrayReserved"] == 7,
        "1: ToJson did not write arrayReserved 7");

  CIccMpeJsonTintArray dst;
  if (!dst.ParseJson(out, parseStr) || !dst.GetArray()) {
    check(false, "1: ParseJson rejected the writer's output");
    return;
  }
  check(dst.GetArray()->m_nReserved == 7,
        "1: the number array's reserved word did not survive ToJson -> ParseJson");
}

// Case 2.
void zeroArrayReservedWritesNothing()
{
  CIccMpeJsonTintArray src;
  std::string parseStr;
  IccJson out = IccJson::object();
  check(src.ParseJson(tintJson(), parseStr) && src.ToJson(out) && !out.contains("arrayReserved"),
        "2: a zero reserved word was written");
}

IccJson calcWith(const IccJson &tintReserved)
{
  IccJson tint = tintJson();
  tint["type"] = "TintArrayElement";
  tint["name"] = "t";
  tint["Reserved"] = tintReserved;
  IccJson calc = IccJson::object();
  calc["inputChannels"] = 1;
  calc["outputChannels"] = 1;
  calc["subElements"] = IccJson::array({tint});
  calc["mainFunction"] = "{ in(0) tint{t} out(0) }";
  return calc;
}

// Case 4.
void wideSubElementReservedRoundTrips()
{
  CIccMpeJsonCalculator src;
  std::string parseStr;
  if (!src.ParseJson(calcWith(3000000000u), parseStr)) {
    std::fprintf(stderr, "%s", parseStr.c_str());
    check(false, "4: a sub-element Reserved of 3000000000 was refused");
    return;
  }
  CIccMultiProcessElement *pTint = src.GetElem(icSigApplyElemOp, 0);
  check(pTint && pTint->m_nReserved == 3000000000u,
        "4: a sub-element Reserved of 3000000000 did not read back");
  IccJson out = IccJson::object();
  check(src.ToJson(out) && out["subElements"][0]["Reserved"].is_number_unsigned() &&
        out["subElements"][0]["Reserved"] == 3000000000u,
        "4: a sub-element Reserved of 3000000000 was not written as that unsigned number");
}

// Case 5.
void badReservedIsRefused()
{
  const IccJson bad[] = { 1.5, -1, "7", 4294967296ull };
  const char *names[] = { "1.5", "-1", "\"7\"", "4294967296" };
  for (int i = 0; i < 4; i++) {
    std::string parseStr, what;

    IccJson j = tintJson();
    j["arrayReserved"] = bad[i];
    CIccMpeJsonTintArray tint;
    what = std::string("5: arrayReserved ") + names[i] + " was accepted";
    check(!tint.ParseJson(j, parseStr), what.c_str());

    CIccMpeJsonCalculator calc;
    what = std::string("5: a sub-element Reserved of ") + names[i] + " was accepted";
    check(!calc.ParseJson(calcWith(bad[i]), parseStr), what.c_str());
  }
}

// Case 3.
void subElementReservedRoundTrips()
{
  IccJson tint = tintJson();
  tint["type"] = "TintArrayElement";
  tint["name"] = "t";
  tint["Reserved"] = 5u;  // unsigned, as a parsed document holds it

  IccJson curve = IccJson::object();
  curve["type"] = "CurveSetElement";
  curve["name"] = "c";
  curve["inputChannels"] = 1;
  curve["outputChannels"] = 1;
  IccJson seg = IccJson::object();
  seg["type"] = "FormulaSegment";
  seg["start"] = "-infinity";
  seg["end"] = "+infinity";
  seg["functionType"] = 0;
  seg["parameters"] = IccJson::array({1.0, 1.0, 0.0, 0.0});
  IccJson segCurve = IccJson::object();
  segCurve["type"] = "SegmentedCurve";
  segCurve["segments"] = IccJson::array({seg});
  curve["curves"] = IccJson::array({segCurve});

  IccJson calc = IccJson::object();
  calc["inputChannels"] = 1;
  calc["outputChannels"] = 1;
  calc["subElements"] = IccJson::array({tint, curve});
  calc["mainFunction"] = "{ in(0) tint{t} curv{c} out(0) }";

  CIccMpeJsonCalculator src;
  std::string parseStr;
  if (!src.ParseJson(calc, parseStr)) {
    std::fprintf(stderr, "%s", parseStr.c_str());
    check(false, "3: the calculator did not parse");
    return;
  }

  CIccMultiProcessElement *pTint = src.GetElem(icSigApplyElemOp, 0);
  CIccMultiProcessElement *pCurve = src.GetElem(icSigApplyElemOp, 1);
  check(pTint && pTint->GetType() == icSigTintArrayElemType && pTint->m_nReserved == 5,
        "3: the tint sub-element did not read back Reserved 5");
  check(pCurve && pCurve->GetType() == icSigCurveSetElemType && pCurve->m_nReserved == 0,
        "3: the curve set sub-element did not read back Reserved 0");

  IccJson out = IccJson::object();
  check(src.ToJson(out), "3: ToJson returned false");
  bool bWritten = out.contains("subElements") && out["subElements"].is_array() &&
                  out["subElements"].size() == 2 &&
                  out["subElements"][0].contains("Reserved") &&
                  out["subElements"][0]["Reserved"] == 5u &&
                  !out["subElements"][1].contains("Reserved");
  check(bWritten, "3: the sub-elements were not written with Reserved 5 and none");
}

} // namespace

int main()
{
  // The tint array creates its tag through the tag factory and the calculator
  // its sub-elements through the MPE factory; both need the JSON classes back,
  // as iccFromJson registers them.
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  arrayReservedRoundTrips();
  zeroArrayReservedWritesNothing();
  subElementReservedRoundTrips();
  wideSubElementReservedRoundTrips();
  badReservedIsRefused();

  if (g_fail) {
    std::fprintf(stderr, "[json-mpe-reserved-roundtrip] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[json-mpe-reserved-roundtrip] all checks passed\n");
  return 0;
}
