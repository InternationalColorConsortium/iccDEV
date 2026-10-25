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
// CTest contract: the XML reader refuses malformed numeric attributes it used to
// read with sscanf() (#2836, #2839, #2840).
//
// sscanf("%u") and sscanf("%x") stop at the first character they cannot use
// and report success, so a malformed value loaded as its leading digits and
// the profile "parsed and saved correctly":
//   - ProfileFlags VendorFlags="0g1" loaded as 0 (#2836);
//   - a tint array number tag's reserved="7x" loaded as 7 (#2839);
//   - a calculator sub-element's Reserved="0x" loaded as 0 (#2840).
// The same reads stood at the other tag-type reserved sites (a profile tag's
// reserved, written "%08x"; a top-level MPE element's Reserved; tag array and
// struct members' reserved) and at namedColor2's VendorFlag. All of them now
// go through icXmlParseU32 or icXmlParseHex32 and refuse the document.
//
// Each case parses a complete profile twice: once with a well-formed value,
// which must parse and keep the value, and once with the same value plus a
// trailing character, which must be refused. On an unfixed library every
// malformed case parses, so the refusal assertions fail in any build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagMPE.h"
#include "IccMpeCalc.h"
#include "IccMpeBasic.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool ok, const std::string &label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label.c_str());
  if (!ok)
    ++g_failures;
}

// A v5 scanner profile whose AToB0 is a calculator with one tint array
// sub-element. Each @NAME@ is replaced by the attribute text under test.
const char *kTemplate =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<IccProfile>\n"
  " <Header>\n"
  "  <ProfileVersion>5.0</ProfileVersion>\n"
  "  <ProfileDeviceClass>scnr</ProfileDeviceClass>\n"
  "  <DataColourSpace>RGB </DataColourSpace>\n"
  "  <PCS>XYZ </PCS>\n"
  "  <CreationDateTime>2026-10-13T15:05:00</CreationDateTime>\n"
  "  <ProfileFlags EmbeddedInFile=\"false\" UseWithEmbeddedDataOnly=\"false\" @FLAGS@/>\n"
  "  <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/></PCSIlluminant>\n"
  " </Header>\n"
  " <Tags>\n"
  "  <copyrightTag><multiLocalizedUnicodeType @TAGRES@><LocalizedText LanguageCountry=\"enUS\">c</LocalizedText></multiLocalizedUnicodeType></copyrightTag>\n"
  "  <multiProcessElementType>\n"
  "   <TagSignature>A2B0</TagSignature>\n"
  "   <MultiProcessElements InputChannels=\"3\" OutputChannels=\"3\">\n"
  "    <CalculatorElement InputChannels=\"3\" OutputChannels=\"3\" @ELEMRES@>\n"
  "     <SubElements>\n"
  "      <TintArrayElement Name=\"tint\" InputChannels=\"1\" OutputChannels=\"3\" @SUBRES@>\n"
  "       <float32NumberType @TINTRES@><Data>0 0 0 1 1 1</Data></float32NumberType>\n"
  "      </TintArrayElement>\n"
  "     </SubElements>\n"
  "     <MainFunction>{ 0.5 tint{tint} out(0,3) }</MainFunction>\n"
  "    </CalculatorElement>\n"
  "   </MultiProcessElements>\n"
  "  </multiProcessElementType>\n"
  " </Tags>\n"
  "</IccProfile>\n";

std::string fill(const std::string &key, const std::string &text)
{
  std::string s = kTemplate;
  const char *keys[] = { "@FLAGS@", "@TAGRES@", "@ELEMRES@", "@SUBRES@", "@TINTRES@" };
  for (const char *k : keys) {
    size_t at = s.find(k);
    s.replace(at, std::string(k).size(), key == k ? text : std::string());
  }
  return s;
}

bool parse(const std::string &xml, CIccProfileXml &profile, std::string &err)
{
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "profile.xml", NULL, 0);
  if (!pDoc) {
    err = "libxml2 could not read the fixture";
    return false;
  }
  bool ok = profile.ParseXml(xmlDocGetRootElement(pDoc), err);
  xmlFreeDoc(pDoc);
  return ok;
}

CIccMpeCalculator *calculator(CIccProfile &profile)
{
  CIccTagMultiProcessElement *pTag =
      dynamic_cast<CIccTagMultiProcessElement *>(profile.FindTag(icSigAToB0Tag));
  if (!pTag)
    return NULL;
  return dynamic_cast<CIccMpeCalculator *>(pTag->GetElement(0));
}

// Parses the well-formed and the malformed spelling of one attribute.
// 'value' reads back what the well-formed parse stored, or -1 if it cannot.
template <typename Reader>
void pair(const char *what, const std::string &key, const std::string &attr,
          const std::string &good, const std::string &bad, long long expect, Reader value)
{
  {
    CIccProfileXml profile;
    std::string err;
    bool ok = parse(fill(key, attr + "=\"" + good + "\""), profile, err);
    check(ok, std::string(what) + " \"" + good + "\": parses");
    if (ok)
      check(value(profile) == expect, std::string(what) + " \"" + good + "\": value kept");
    else
      std::printf("      %s", err.c_str());
  }
  {
    CIccProfileXml profile;
    std::string err;
    check(!parse(fill(key, attr + "=\"" + bad + "\""), profile, err),
          std::string(what) + " \"" + bad + "\": refused");
  }
}

} // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  pair("ProfileFlags VendorFlags", "@FLAGS@", "VendorFlags", "80000000", "0g1",
       0x80000000LL, [](CIccProfile &p) { return (long long)(p.m_Header.flags & 0xFFFF0000u); });

  pair("profile tag reserved", "@TAGRES@", "reserved", "0000000a", "0000000ax", 0xa,
       [](CIccProfile &p) {
         CIccTag *pTag = p.FindTag(icSigCopyrightTag);
         return pTag ? (long long)pTag->m_nReserved : -1LL;
       });

  pair("MPE element Reserved", "@ELEMRES@", "Reserved", "9", "9x", 9,
       [](CIccProfile &p) {
         CIccMpeCalculator *pCalc = calculator(p);
         return pCalc ? (long long)pCalc->m_nReserved : -1LL;
       });

  pair("calculator sub-element Reserved", "@SUBRES@", "Reserved", "9", "0x", 9,
       [](CIccProfile &p) {
         CIccMpeCalculator *pCalc = calculator(p);
         CIccMultiProcessElement *pSub = pCalc ? pCalc->GetElem(icSigApplyElemOp, 0) : NULL;
         return pSub ? (long long)pSub->m_nReserved : -1LL;
       });

  pair("tint array reserved", "@TINTRES@", "reserved", "7", "7x", 7,
       [](CIccProfile &p) {
         CIccMpeCalculator *pCalc = calculator(p);
         CIccMpeTintArray *pTint =
             pCalc ? dynamic_cast<CIccMpeTintArray *>(pCalc->GetElem(icSigApplyElemOp, 0)) : NULL;
         CIccTagNumArray *pArray = pTint ? pTint->GetArray() : NULL;
         return pArray ? (long long)pArray->m_nReserved : -1LL;
       });

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
