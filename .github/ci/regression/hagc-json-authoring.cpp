/*
 * The ICC Software License, Version 0.2
 *
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
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

// The JSON half of the HAGC authoring invariants. See
// .github/ci/regression/hagc-authoring-invariants.cpp for the XML half and for
// why these invariants have to be established on the authoring paths at all:
// in this tag nothing is redundant, so it is the decoder's clamps and its
// mixing-type-driven rebuild of the coefficient array that *define* the domain
// the evaluator is written against, and a model built in memory has been
// through neither.
//
// Two things are pinned here, both of which the JSON parser got the same way
// the XML parser did - by open-coding the fixed coefficient cases in a switch
// that only writes the arms each type needs:
//
// 1. A "componentMixingType": 3 alternate whose "coefficients" object names
//    fewer than six entries must leave the rest at zero. The constructor builds
//    an alternate as type 0, which sets kMax = 1, so a switch that does not
//    zero first leaves that behind - and Pack() derives the coefficient
//    presence flags from the values, so the phantom entry is written to disk as
//    if the document had asked for it. The coefficient sum is then 2.0 rather
//    than 1.0, which halves every mixed value the gain curve sees.
//
// 2. "componentMixingType": 1 must leave kComponent as the only non-zero
//    coefficient. Types 0 to 2 serialize no coefficients at all, so a stale
//    kMax there never reaches the file - it changes only what the in-process
//    tag computes. That assertion is red only when CIccTagHagc::SetMetadata()'s
//    Pack-then-Unpack round trip has also been reverted, since the round trip
//    scrubs a coefficient the bytes never carried; it is kept because it states
//    the invariant the parser is meant to hold to on its own.
//
// It also pins that an "applicationVersion" outside the three bits the encoding
// gives it is refused rather than masked down to something the document never
// asked for.
//
// The document is built here rather than read from Testing/ because it is the
// parser being tested, not a fixture: the XML side owns the corpus fixture
// (Testing/HDR/HagcMixingTypes.xml) and this is its counterpart.
//
// Returns 0 on success; the number of failed assertions otherwise.

#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccProfile.h"
#include "IccTagHagc.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char *szWhat)
{
  if (!cond) {
    printf("FAIL: %s\n", szWhat);
    g_failures++;
  }
}

void checkClose(double got, double want, double tol, const char *szWhat)
{
  if (!(fabs(got - want) <= tol)) {
    printf("FAIL: %s (got %.9g, want %.9g)\n", szWhat, got, want);
    g_failures++;
  }
}

// A minimal RGB display profile carrying one HAGC tag. %s is the application
// version, so the same document drives both the positive and the out of range
// case; everything else is fixed.
const char *kProfileTemplate =
"{"
"  \"IccProfile\": {"
"    \"Header\": {"
"      \"ProfileVersion\": \"4.50\","
"      \"ProfileDeviceClass\": \"mntr\","
"      \"DataColourSpace\": \"RGB \","
"      \"PCS\": \"XYZ \","
"      \"RenderingIntent\": \"Relative Colorimetric\""
"    },"
"    \"Tags\": ["
"      {"
"        \"headroomAdaptiveGainCurveTag\": {"
"          \"data\": {"
"            \"type\": \"headroomAdaptiveGainCurveType\","
"            \"applicationVersion\": %s,"
"            \"minApplicationVersion\": 0,"
"            \"baselineHeadroom\": 3.0,"
"            \"headroomAdaptiveToneMap\": true,"
"            \"chromaticitiesMode\": 0,"
"            \"commonComponentMixing\": false,"
"            \"commonCurveParameters\": false,"
"            \"alternateImages\": ["
"              {"
"                \"headroom\": 0.0,"
"                \"componentMixingType\": 3,"
"                \"pchipSlope\": false,"
"                \"coefficients\": { \"kRed\": 1.0 },"
"                \"controlPoints\": ["
"                  { \"x\": 0.25, \"y\": -0.1, \"m\": -0.4 },"
"                  { \"x\": 0.50, \"y\": -0.2, \"m\": -0.3 },"
"                  { \"x\": 0.75, \"y\": -0.3, \"m\": -0.2 },"
"                  { \"x\": 1.00, \"y\": -0.4, \"m\": -0.1 }"
"                ]"
"              },"
"              {"
"                \"headroom\": 5.0,"
"                \"componentMixingType\": 1,"
"                \"pchipSlope\": true,"
"                \"controlPoints\": ["
"                  { \"x\": 0.5, \"y\": 0.2 },"
"                  { \"x\": 1.0, \"y\": 0.4 },"
"                  { \"x\": 1.5, \"y\": 0.6 }"
"                ]"
"              }"
"            ]"
"          }"
"        }"
"      }"
"    ]"
"  }"
"}";

std::string document(const char *szApplicationVersion)
{
  std::string s = kProfileTemplate;
  size_t pos = s.find("%s");

  if (pos != std::string::npos)
    s.replace(pos, 2, szApplicationVersion);

  return s;
}

bool parseDocument(CIccProfileJson &profile, const char *szApplicationVersion,
                   std::string &parseStr)
{
  IccJson j;
  std::string text = document(szApplicationVersion);

  try {
    j = IccJson::parse(text);
  }
  catch (...) {
    parseStr = "the test's own document is not valid JSON";
    return false;
  }

  return profile.ParseJson(j, parseStr);
}

const CIccTagHagc *findHagc(CIccProfile *pProfile)
{
  CIccTag *pTag = pProfile ? pProfile->FindTag(icSigHeadroomAdaptiveGainCurveTag) : NULL;

  return pTag && pTag->GetType() == icSigHeadroomAdaptiveGainCurveType
       ? (const CIccTagHagc *)pTag : NULL;
}

void testJsonMixingTypeCoefficients()
{
  CIccProfileJson profile;
  std::string parseStr;

  if (!parseDocument(profile, "1", parseStr)) {
    check(false, "the HAGC document parses");
    printf("       %s\n", parseStr.c_str());
    return;
  }

  const CIccTagHagc *pTag = findHagc(&profile);

  if (!pTag) {
    check(false, "the parsed profile carries a headroomAdaptiveGainCurveTag");
    return;
  }

  const icHagcMetadata &m = pTag->GetMetadata();

  check(m.GetNumAlternates() == 2, "the document's two alternate images are present");

  const icHagcAlternateImage *pAlt0 = m.GetAlternate(0);
  const icHagcAlternateImage *pAlt1 = m.GetAlternate(1);

  if (!pAlt0 || !pAlt1) {
    check(false, "both alternates are readable");
    return;
  }

  check(pAlt0->m_nMixingType == icHagcMixingCustom, "alternate 0 is mixing type 3");
  checkClose(pAlt0->m_coef[icHagcCoefRed], 1.0, 1e-4, "alternate 0 kRed is what the document says");
  checkClose(pAlt0->m_coef[icHagcCoefMax], 0.0, 1e-12,
             "alternate 0 kMax is zero, not the constructor's type 0 default");

  double sum = 0.0;
  for (int i = 0; i < icHagcNumCoefficients; i++)
    sum += pAlt0->m_coef[i];
  checkClose(sum, 1.0, 1e-4, "alternate 0 coefficients sum to what the document says");

  check(pAlt1->m_nMixingType == icHagcMixingComponent, "alternate 1 is mixing type 1");
  checkClose(pAlt1->m_coef[icHagcCoefComponent], 1.0, 1e-4, "alternate 1 kComponent is 1");
  checkClose(pAlt1->m_coef[icHagcCoefMax], 0.0, 1e-12,
             "alternate 1 kMax is zero, not the constructor's type 0 default");
}

void testJsonApplicationVersionRange()
{
  CIccProfileJson profile;
  std::string parseStr;

  // Three bits, so 8 is not representable.  Masking it would produce a profile
  // saying 0 from a document saying 8, with nothing reporting the change.
  check(!parseDocument(profile, "8", parseStr),
        "an application version past the three bit field is refused, not masked");

  CIccProfileJson negative;
  parseStr.clear();
  check(!parseDocument(negative, "-1", parseStr),
        "a negative application version is refused");
}

// ---------------------------------------------------------------------------
// The JSON parser fails closed on wrong-typed and missing values, and the
// writer never re-encodes away bytes the model does not keep.
// ---------------------------------------------------------------------------

const char *kJsonHagc =
  "{\"applicationVersion\":1,\"minApplicationVersion\":0,\"hdrReferenceWhite\":300.0,"
  "\"baselineHeadroom\":3.0,\"headroomAdaptiveToneMap\":true,\"chromaticitiesMode\":0,"
  "\"commonComponentMixing\":false,\"commonCurveParameters\":false,"
  "\"alternateImages\":["
  "{\"headroom\":0.0,\"componentMixingType\":3,\"pchipSlope\":false,\"coefficients\":{\"kRed\":1.0},"
  "\"controlPoints\":[{\"x\":0.5,\"y\":-0.2,\"m\":-0.3},{\"x\":1.0,\"y\":-0.4,\"m\":-0.1}]},"
  "{\"headroom\":5.0,\"componentMixingType\":1,\"pchipSlope\":true,"
  "\"controlPoints\":[{\"x\":0.5,\"y\":0.2},{\"x\":1.0,\"y\":0.4}]}"
  "]}";

std::string replacedJson(const char *szDoc, const char *szFrom, const char *szTo)
{
  std::string s = szDoc;
  size_t pos = s.find(szFrom);
  if (pos != std::string::npos)
    s.replace(pos, strlen(szFrom), szTo);
  else
    printf("TEST BUG: '%s' not found in the document\n", szFrom);
  return s;
}

bool parseJsonTag(const std::string &text, CIccTagJsonHagc &tag, std::string &parseStr)
{
  IccJson j;
  try {
    j = IccJson::parse(text);
  }
  catch (...) {
    parseStr = "not valid JSON";
    return false;
  }
  return tag.ParseJson(j, parseStr);
}

void expectJsonRefused(const std::string &text, const char *szWhat)
{
  CIccTagJsonHagc tag;
  std::string parseStr;
  if (parseJsonTag(text, tag, parseStr)) {
    printf("FAIL: %s (the tag parsed)\n", szWhat);
    g_failures++;
  }
}

const icUInt8Number kJsonOnePointBlock[31] = {
  0x00, 0x00, 0x00, 0x00, 0x10,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x27, 0x10, 0x40, 0x00, 0x03, 0xE8, 0x27, 0x10, 0x46, 0x50
};

void checkJsonByteRoundTrip(const std::vector<icUInt8Number> &raw, const char *szWhat)
{
  CIccTagJsonHagc src;
  src.SetRawMetadata(raw.empty() ? NULL : &raw[0], (icUInt32Number)raw.size());

  IccJson j = IccJson::object();
  if (!src.ToJson(j)) {
    printf("FAIL: %s - ToJson failed\n", szWhat);
    g_failures++;
    return;
  }
  check(j.contains("rawMetadata"), szWhat);

  CIccTagJsonHagc back;
  std::string parseStr;
  if (!parseJsonTag(j.dump(), back, parseStr)) {
    printf("FAIL: %s - the exported JSON does not parse back (%s)\n", szWhat, parseStr.c_str());
    g_failures++;
    return;
  }
  check(back.GetRawMetadataSize() == raw.size() &&
        (raw.empty() || memcmp(back.GetRawMetadata(), &raw[0], raw.size()) == 0),
        "and the bytes survive the JSON round trip exactly");
}

void testJsonStrictAndByteExact()
{
  {
    CIccTagJsonHagc tag;
    std::string parseStr;
    check(parseJsonTag(kJsonHagc, tag, parseStr), "positive control: the strict-parse document parses");
    const icHagcAlternateImage *pAlt1 = tag.GetMetadata().GetAlternate(1);
    check(pAlt1 && pAlt1->m_headroom == 5.0f, "and alternate 1 keeps its headroom");
  }

  expectJsonRefused(replacedJson(kJsonHagc, "\"headroom\":5.0", "\"headroom\":\"5.0\""),
                    "a quoted alternate headroom is refused, not read as 0");
  expectJsonRefused(replacedJson(kJsonHagc, "\"hdrReferenceWhite\":300.0", "\"hdrReferenceWhite\":\"300\""),
                    "a quoted hdrReferenceWhite is refused, not dropped to the default");
  expectJsonRefused(replacedJson(kJsonHagc, "\"applicationVersion\":1", "\"applicationVersion\":\"1\""),
                    "a quoted applicationVersion is refused, not read as 0");
  expectJsonRefused(replacedJson(kJsonHagc, "\"kRed\":1.0", "\"kRed\":\"1.0\""),
                    "a quoted coefficient is refused");
  expectJsonRefused(replacedJson(kJsonHagc, "{\"headroom\":0.0,", "{"),
                    "an alternate image without headroom is refused");
  expectJsonRefused(replacedJson(kJsonHagc, "{\"x\":0.5,\"y\":-0.2", "{\"y\":-0.2"),
                    "a control point without x is refused");
  expectJsonRefused(replacedJson(kJsonHagc, ",\"m\":-0.3", ""),
                    "a control point without m is refused when slopes are carried");
  expectJsonRefused(replacedJson(kJsonHagc, "\"pchipSlope\":false", "\"pchipSlope\":\"no\""),
                    "a non-boolean pchipSlope is refused");
  expectJsonRefused(replacedJson(kJsonHagc, "\"chromaticitiesMode\":0,",
                                 "\"chromaticitiesMode\":3,\"chromaticities\":[0,0,0,0,0,0,0,\"x\"],"),
                    "a non-numeric chromaticity is refused");
  expectJsonRefused("{\"rawMetadata\":5}", "a non-string rawMetadata is refused");

  std::vector<icUInt8Number> canonical(kJsonOnePointBlock, kJsonOnePointBlock + sizeof(kJsonOnePointBlock));
  {
    CIccTagJsonHagc exact;
    exact.SetRawMetadata(&canonical[0], (icUInt32Number)canonical.size());
    IccJson j = IccJson::object();
    check(exact.ToJson(j) && !j.contains("rawMetadata") && j.contains("alternateImages"),
          "control: an exact tag still exports the structured form");
  }

  std::vector<icUInt8Number> trailing = canonical;
  trailing.push_back(0xAB);
  checkJsonByteRoundTrip(trailing, "a block with a trailing byte exports as rawMetadata");

  std::vector<icUInt8Number> reserved = canonical;
  reserved[0] |= 0x03;
  checkJsonByteRoundTrip(reserved, "a block with reserved bits set exports as rawMetadata");

  checkJsonByteRoundTrip(std::vector<icUInt8Number>(), "an empty block exports as rawMetadata");
}

}  // namespace

int main()
{
  // The JSON tag handlers have to be on the stack before ParseJson(), or the
  // HAGC element parses as an unknown tag and there is nothing to look at.
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  testJsonMixingTypeCoefficients();
  testJsonApplicationVersionRange();
  testJsonStrictAndByteExact();

  if (g_failures)
    printf("%d assertion(s) failed\n", g_failures);
  else
    printf("all HAGC JSON authoring invariants hold\n");

  /* Not the raw count: a count that is a multiple of 256 would read as a pass. */
  return g_failures ? 1 : 0;
}
