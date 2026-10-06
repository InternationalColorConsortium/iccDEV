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
 * xml-mpe-reserved-roundtrip.cpp -- the reserved words of a tint array's
 * number array and of a calculator's sub-elements survive XML.
 *
 * The binary format keeps both.  Through XML:
 *
 *   - CIccMpeXmlTintArray::ToXml wrote the element's Reserved but not the
 *     reserved word of the number array tag it holds.  ParseXml has always
 *     read that word from a lower-case reserved attribute on the array's type
 *     node, the same spelling and decimal form the profile-level struct
 *     reader uses, but nothing wrote one, so ICC -> XML -> ICC set it to 0.
 *
 *   - CIccMpeXmlCalculator's SubElements loop parsed each sub-element and
 *     then took its Reserved from the enclosing CalculatorElement node rather
 *     than from the sub-element's own, so a sub-element's Reserved was lost and
 *     a calculator's own Reserved was copied onto every sub-element.  A
 *     top-level element is unaffected: CIccTagXmlMultiProcessElement reads
 *     each element's Reserved from the element node.
 *
 * Cases:
 *   1. A tint array whose number array has reserved 7 writes reserved="7" on
 *      the type node (float32ArrayType, the name the writer uses; the reader
 *      also takes float32NumberType), and that document parses back to 7.
 *   2. Control: a zero reserved word writes no attribute.
 *   3. A calculator carrying Reserved="9" whose sub-elements carry
 *      Reserved="5" (tint array), Reserved="6" (matrix) and none (curve set)
 *      parses to 5, 6 and 0, and writes the same three back.
 *
 * Cases 1 and 3 fail on an unfixed build.
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
    std::fprintf(stderr, "[xml-mpe-reserved-roundtrip] FAIL %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

template <class T>
bool parseInto(T &obj, const std::string &xml)
{
  xmlDoc *pDoc = xmlReadMemory(xml.c_str(), (int)xml.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  xmlNode *pNode = pDoc ? xmlDocGetRootElement(pDoc) : NULL;
  std::string parseStr;
  bool ok = pNode && obj.ParseXml(pNode, parseStr);
  if (!ok)
    std::fprintf(stderr, "%s", parseStr.c_str());
  if (pDoc)
    xmlFreeDoc(pDoc);
  return ok;
}

std::string tintXml(const char *szTypeAttrs)
{
  return std::string("<TintArrayElement InputChannels=\"1\" OutputChannels=\"1\">"
                     "<float32NumberType") + szTypeAttrs +
         "><Data>0 0.5 1</Data></float32NumberType></TintArrayElement>";
}

// Case 1.
void arrayReservedRoundTrips()
{
  CIccMpeXmlTintArray src;
  if (!parseInto(src, tintXml(" reserved=\"7\""))) {
    check(false, "1: the tint array element did not parse");
    return;
  }
  check(src.GetArray() && src.GetArray()->m_nReserved == 7,
        "1: reserved=\"7\" on the type node did not reach the number array");

  std::string xml;
  check(src.ToXml(xml, ""), "1: ToXml returned false");
  check(has(xml, "<float32ArrayType reserved=\"7\">"),
        "1: ToXml did not write the number array's reserved word");

  CIccMpeXmlTintArray dst;
  if (!parseInto(dst, xml)) {
    check(false, "1: ParseXml rejected the writer's output");
    return;
  }
  check(dst.GetArray() && dst.GetArray()->m_nReserved == 7,
        "1: the number array's reserved word did not survive ToXml -> ParseXml");
}

// Case 2.
void zeroArrayReservedWritesNothing()
{
  CIccMpeXmlTintArray src;
  if (!parseInto(src, tintXml(""))) {
    check(false, "2: the tint array element did not parse");
    return;
  }
  std::string xml;
  check(src.ToXml(xml, ""), "2: ToXml returned false");
  check(has(xml, "<float32ArrayType>") && !has(xml, "reserved="),
        "2: a zero reserved word was written");
}

// Case 3.
const char *kCalc =
  "<CalculatorElement InputChannels=\"1\" OutputChannels=\"1\" Reserved=\"9\">"
  "<SubElements>"
  "<TintArrayElement Name=\"t\" InputChannels=\"1\" OutputChannels=\"1\" Reserved=\"5\">"
  "<float32NumberType><Data>0 1</Data></float32NumberType>"
  "</TintArrayElement>"
  "<MatrixElement Name=\"m\" InputChannels=\"1\" OutputChannels=\"1\" Reserved=\"6\">"
  "<MatrixData>1</MatrixData>"
  "</MatrixElement>"
  "<CurveSetElement Name=\"c\" InputChannels=\"1\" OutputChannels=\"1\">"
  "<SegmentedCurve><FormulaSegment Start=\"-infinity\" End=\"+infinity\" FunctionType=\"0\">"
  "1 1 0 0</FormulaSegment></SegmentedCurve>"
  "</CurveSetElement>"
  "</SubElements>"
  "<MainFunction>{ in(0) tint{t} mtx{m} curv{c} out(0) }</MainFunction>"
  "</CalculatorElement>";

void subElementReservedRoundTrips()
{
  CIccMpeXmlCalculator calc;
  if (!parseInto(calc, kCalc)) {
    check(false, "3: the calculator did not parse");
    return;
  }

  struct Want { const char *szType; icElemTypeSignature sig; icUInt32Number nReserved; };
  const Want want[] = {
    { "tint", icSigTintArrayElemType, 5 },
    { "matrix", icSigMatrixElemType, 6 },
    { "curve set", icSigCurveSetElemType, 0 },
  };
  for (icUInt16Number i = 0; i < 3; i++) {
    CIccMultiProcessElement *pElem = calc.GetElem(icSigApplyElemOp, i);
    char szWhat[128];
    std::snprintf(szWhat, sizeof(szWhat), "3: sub-element %u is not the %s element",
                  (unsigned)i, want[i].szType);
    if (!pElem || pElem->GetType() != want[i].sig) {
      check(false, szWhat);
      continue;
    }
    std::snprintf(szWhat, sizeof(szWhat), "3: the %s sub-element's Reserved is %u, not %u",
                  want[i].szType, (unsigned)pElem->m_nReserved, (unsigned)want[i].nReserved);
    check(pElem->m_nReserved == want[i].nReserved, szWhat);
  }

  std::string xml;
  check(calc.ToXml(xml, ""), "3: ToXml returned false");
  check(has(xml, "<TintArrayElement InputChannels=\"1\" OutputChannels=\"1\" Reserved=\"5\">"),
        "3: the tint sub-element was not written with Reserved=\"5\"");
  check(has(xml, "<MatrixElement InputChannels=\"1\" OutputChannels=\"1\" Reserved=\"6\">"),
        "3: the matrix sub-element was not written with Reserved=\"6\"");
  // Checked on the sub-element's own start tag, not the whole document: the
  // calculator's own Reserved belongs to whatever parses the CalculatorElement.
  check(has(xml, "<CurveSetElement InputChannels=\"1\" OutputChannels=\"1\">"),
        "3: the curve set sub-element was written with a Reserved it did not carry");
}

} // namespace

int main()
{
  // The tint array reader creates its tag through CIccTag::Create and the
  // calculator creates its sub-elements through the MPE factory; both need
  // the XML classes back, as iccFromXml registers them.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  arrayReservedRoundTrips();
  zeroArrayReservedWritesNothing();
  subElementReservedRoundTrips();

  xmlCleanupParser();

  if (g_fail) {
    std::fprintf(stderr, "[xml-mpe-reserved-roundtrip] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[xml-mpe-reserved-roundtrip] all checks passed\n");
  return 0;
}
