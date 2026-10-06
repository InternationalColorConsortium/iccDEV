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
 * xml-mpe-reader-setters.cpp -- the XML reader installs a sampled calculator
 * curve's calculator and a tint array element's array through their setters.
 *
 * CIccSampledCalculatorCurveXml::ParseXml and CIccMpeXmlTintArray::ParseXml
 * assigned m_pCalc and m_Array directly (#2645).  The JSON reader installs
 * both through SetCalculator() and SetArray(), which link the installed object
 * to its owner and release the one it replaces, and the binary reader does the
 * same (through SetCalculator(), and inline for the array), so an XML-parsed
 * calculator or tint array was the only one with no parent, and a second
 * ParseXml on the same element leaked the first array.  The calculator
 * reader also leaked the calculator it had allocated when that calculator's
 * own parse failed, where the JSON reader deletes it.
 *
 * Cases:
 *   1. A tint array parsed from XML has its element as parent.
 *   2. A second ParseXml on the same tint array element installs the second
 *      array, still parented; the first is released (LeakSanitizer only).
 *   3. A sampled calculator curve parsed from XML holds a calculator whose
 *      parent is the curve.
 *   4. A sampled calculator curve whose calculator fails to parse fails the
 *      curve set, and frees the calculator (LeakSanitizer only).
 *
 * Cases 1, 2 and 3 fail on an unfixed build anywhere, on the parent checks.
 * The leaks in cases 2 and 4 are seen only by LeakSanitizer, which this test's
 * registration turns on with ASAN_OPTIONS=detect_leaks=1 where it exists.
 */

#include "IccMpeXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccTagBasic.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

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
    std::fprintf(stderr, "[xml-mpe-reader-setters] FAIL %s\n", what);
  }
}

// Both members are protected and the XML curve class is private to
// IccMpeXml.cpp, so read them through a pointer to member named via a derived
// class.  Neither probe is ever instantiated.
struct CurveSetAccess : public CIccMpeCurveSet {
  static icCurveSetCurvePtr curve(const CIccMpeCurveSet &curveSet, int i)
  {
    return (curveSet.*(&CurveSetAccess::m_curve))[i];
  }
};

struct CalcCurveAccess : public CIccSampledCalculatorCurve {
  static CIccMpeCalculator *calc(const CIccSampledCalculatorCurve &curve)
  {
    return curve.*(&CalcCurveAccess::m_pCalc);
  }
};

xmlNode *parseRoot(xmlDoc *&pDoc, const char *szXml)
{
  pDoc = xmlReadMemory(szXml, (int)std::char_traits<char>::length(szXml),
                       "frag.xml", NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  return pDoc ? xmlDocGetRootElement(pDoc) : NULL;
}

bool parseTint(CIccMpeXmlTintArray &tint, const char *szXml)
{
  xmlDoc *pDoc = NULL;
  xmlNode *pNode = parseRoot(pDoc, szXml);
  std::string parseStr;
  bool ok = pNode && tint.ParseXml(pNode, parseStr);
  if (pDoc)
    xmlFreeDoc(pDoc);
  return ok;
}

bool parseCurveSet(CIccMpeXmlCurveSet &curveSet, const char *szXml)
{
  xmlDoc *pDoc = NULL;
  xmlNode *pNode = parseRoot(pDoc, szXml);
  std::string parseStr;
  bool ok = pNode && curveSet.ParseXml(pNode, parseStr);
  if (pDoc)
    xmlFreeDoc(pDoc);
  return ok;
}

const char *kTintA =
  "<TintArrayElement InputChannels=\"1\" OutputChannels=\"1\">"
  "<float32NumberType><Data>0 0.5 1</Data></float32NumberType>"
  "</TintArrayElement>";

const char *kTintB =
  "<TintArrayElement InputChannels=\"1\" OutputChannels=\"1\">"
  "<float32NumberType><Data>0.25 0.75</Data></float32NumberType>"
  "</TintArrayElement>";

const char *kCalcCurve =
  "<CurveSetElement InputChannels=\"1\" OutputChannels=\"1\">"
  "<SampledCalculatorCurve FirstEntry=\"0.0\" LastEntry=\"1.0\" DesiredSize=\"16\">"
  "<CalculatorElement InputChannels=\"1\" OutputChannels=\"1\">"
  "<MainFunction>{ in(0) 2 mul out(0) }</MainFunction>"
  "</CalculatorElement>"
  "</SampledCalculatorCurve>"
  "</CurveSetElement>";

const char *kBadCalcCurve =
  "<CurveSetElement InputChannels=\"1\" OutputChannels=\"1\">"
  "<SampledCalculatorCurve FirstEntry=\"0.0\" LastEntry=\"1.0\" DesiredSize=\"16\">"
  "<CalculatorElement InputChannels=\"x\" OutputChannels=\"1\">"
  "<MainFunction>{ in(0) out(0) }</MainFunction>"
  "</CalculatorElement>"
  "</SampledCalculatorCurve>"
  "</CurveSetElement>";

const IIccObject *asObject(CIccMpeTintArray &tint)
{
  return &tint;
}

void tintArrayIsParented()
{
  CIccMpeXmlTintArray tint;
  if (!parseTint(tint, kTintA)) {
    check(false, "1: the tint array element did not parse");
    return;
  }
  CIccTagNumArray *pArray = tint.GetArray();
  check(pArray != NULL, "1: the parsed tint array element holds no array");
  if (pArray)
    check(pArray->GetParentObject() == asObject(tint),
          "1: the parsed tint array's parent is not its element");
}

void tintArrayReparseReplaces()
{
  CIccMpeXmlTintArray tint;
  if (!parseTint(tint, kTintA) || !parseTint(tint, kTintB)) {
    check(false, "2: the tint array element did not parse twice");
    return;
  }
  CIccTagNumArray *pArray = tint.GetArray();
  check(pArray && pArray->GetNumValues() == 2,
        "2: the second parse did not install the second array");
  if (pArray) {
    icFloatNumber v[2] = { 0, 0 };
    check(pArray->GetValues(v, 0, 2) && v[0] == 0.25f && v[1] == 0.75f,
          "2: the installed array does not hold the second document's values");
    check(pArray->GetParentObject() == asObject(tint),
          "2: the re-parsed tint array's parent is not its element");
  }
}

void calculatorIsParented()
{
  CIccMpeXmlCurveSet curveSet;
  if (!parseCurveSet(curveSet, kCalcCurve)) {
    check(false, "3: the curve set with a sampled calculator curve did not parse");
    return;
  }
  icCurveSetCurvePtr pCurve = CurveSetAccess::curve(curveSet, 0);
  if (!pCurve || pCurve->GetType() != icSigSampledCalculatorCurve) {
    check(false, "3: channel 0 is not a sampled calculator curve");
    return;
  }
  CIccSampledCalculatorCurve *pCalcCurve = static_cast<CIccSampledCalculatorCurve*>(pCurve);
  CIccMpeCalculator *pCalc = CalcCurveAccess::calc(*pCalcCurve);
  check(pCalc != NULL, "3: the parsed sampled calculator curve holds no calculator");
  if (pCalc)
    check(pCalc->GetParentObject() == static_cast<const IIccObject*>(pCalcCurve),
          "3: the parsed calculator's parent is not its curve");
}

void failedCalculatorIsReleased()
{
  CIccMpeXmlCurveSet curveSet;
  check(!parseCurveSet(curveSet, kBadCalcCurve),
        "4: a curve set whose calculator does not parse was accepted");
}

} // namespace

int main()
{
  // The tint array reader creates its tag through CIccTag::Create and needs
  // the XML class back, as iccFromXml registers it.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  tintArrayIsParented();
  tintArrayReparseReplaces();
  calculatorIsParented();
  failedCalculatorIsReleased();

  xmlCleanupParser();

  if (g_fail) {
    std::fprintf(stderr, "[xml-mpe-reader-setters] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[xml-mpe-reader-setters] all checks passed\n");
  return 0;
}
