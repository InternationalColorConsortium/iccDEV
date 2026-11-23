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
/*
 * embedded-profile-flags.cpp -- an ICC.2 profile embedded in an ICC.1
 * profile's embeddedV5ProfileTag is validated against the proposed ICC.2
 * annex on embedding.
 *
 * #2693 checked the embedded profile's header flags and device space against
 * ICC Technical Note 04-2018.  The annex that replaces the note changes:
 *   - header requirements: bit 0 of the embedded profile's flags "shall be
 *     set to 1".  The note said "should", which drew a warning; it is now
 *     non-compliance.  "Only iccMAX profiles containing 0 in bit position 1
 *     should be embedded" is unchanged, so bit 1 set still draws a warning.
 *   - header requirements: the device space "shall be either the same as or
 *     a superset of the device space of the encapsulating ICC profile".  The
 *     note required the same space.  Among the standard spaces, CMYK is a
 *     superset of CMY, and its channels are defined by the space.  A generic
 *     n-channel space (xCLR or ncXXXX) with more channels than the host's is
 *     a named color superset: it has channels the host's colorant tags do
 *     not define, so it needs a colorantInfoTag or colorantTableTag of its
 *     own, and the host's colorant tags stand in only for the same space.
 *     Its first channels shall be the host's, in the same order; that is
 *     checked by colorant name when both have a colorantTableTag.
 *   - embedding: "Redundant tags are not required in the embedded iccMAX
 *     profile as long as the redundant tags are not required for the correct
 *     processing of other tags in the embedded iccMAX profile".  A required
 *     tag the host holds now satisfies the embedded profile's requirement
 *     unless the embedded profile's own tags depend on it: its relative or
 *     absolute colorimetric transforms need its own mediaWhitePointTag, the
 *     annex's example.  The embedded profile used to be validated as its own
 *     parent, so a spectral Output child with only a DToB3Tag drew "Critical
 *     tag(s) missing" under a v4 host that holds every BToAx tag.
 *
 * This pins:
 *   1. bit 0 set and bit 1 clear draws no flag finding; bit 1 set draws a
 *      warning;
 *   2. bit 0 clear draws a finding, at non-compliant level;
 *   3. the same space, CMYK under CMY (with no colorant tag), and a wider
 *      generic space draw no device-space error; a different standard space,
 *      a generic space no wider than the host's, and a narrower space each
 *      draw a critical one; a named color superset without its own colorant
 *      tag is non-compliant even when the host has one, and the host's
 *      colorantTableTag satisfies the child's xCLR Output colorant
 *      recommendation only for the same space; a named color superset whose
 *      colorant table does not begin with the host's colorants in order is
 *      non-compliant, and with no host colorant table, order is not checked;
 *   4. an embedded spectral Output child with only a DToB3Tag draws no
 *      required-tag finding when the host holds the description, copyright,
 *      white point and BToAx tags; it draws the missing-transform error when
 *      the host holds no PCS-to-device tag either; and validated on its own
 *      it still draws every finding;
 *   5. under a host with a mediaWhitePointTag, a colorimetric-PCS child with
 *      only perceptual transforms needs no white point of its own, and one
 *      with a relative colorimetric transform does, and a child whose
 *      absolute DToB3Tag uses its spectral PCS does not.
 * The annex lets the child's PCS differ from the host's, so no PCS match is
 * asserted.
 */

#include "IccProfile.h"
#include "IccTagEmbedIcc.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
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
    std::fprintf(stderr, "[embedded-profile-flags] FAIL %s\n", what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

const char *kNotEmbedded = "do not mark it as embedded in another file";
const char *kDataOnly = "cannot be used independently of embedded colour data";
const char *kNotSuperset = "is neither the same as nor a superset of the parent profile's";
const char *kNoChannels = "has neither a colorantInfoTag nor a colorantTableTag to define its channels";

/* Any tag object will do where only the tag's presence matters. */
void attachText(CIccProfile *pProfile, icTagSignature sig)
{
  CIccTagUtf8Text *pText = new CIccTagUtf8Text;
  pText->SetText("x");
  pProfile->AttachTag(sig, pText);
}

/* A v5 display child with the given header flags and device space, owned by
   the tag, with a colorantTableTag if bColorants. */
CIccTagEmbeddedProfile *newEmbedded(icUInt32Number flags, icColorSpaceSignature space = icSigRgbData,
                                    bool bColorants = false)
{
  CIccProfile *pChild = new CIccProfile;
  pChild->InitHeader();
  pChild->m_Header.version = icVersionNumberV5;
  pChild->m_Header.deviceClass = icSigDisplayClass;
  pChild->m_Header.colorSpace = space;
  pChild->m_Header.pcs = icSigXYZData;
  pChild->m_Header.flags = flags;
  if (bColorants)
    attachText(pChild, icSigColorantTableTag);

  CIccTagEmbeddedProfile *pTag = new CIccTagEmbeddedProfile;
  pTag->SetProfile(pChild);
  return pTag;
}

icValidateStatus validate(icUInt32Number flags, std::string &report,
                          icColorSpaceSignature childSpace = icSigRgbData,
                          icColorSpaceSignature hostSpace = icSigRgbData,
                          bool bChildColorants = false, bool bHostColorants = false)
{
  CIccProfile host;
  host.InitHeader();
  host.m_Header.version = icVersionNumberV4_3;
  host.m_Header.deviceClass = icSigDisplayClass;
  host.m_Header.colorSpace = hostSpace;
  host.m_Header.pcs = icSigXYZData;
  if (bHostColorants)
    attachText(&host, icSigColorantTableTag);

  CIccTagEmbeddedProfile *pTag = newEmbedded(flags, childSpace, bChildColorants);
  icValidateStatus rv = pTag->Validate("", report, &host);
  delete pTag;
  return rv;
}

void testFlags()
{
  std::string report;

  validate(icEmbeddedProfileTrue, report);
  check(!has(report, kNotEmbedded) && !has(report, kDataOnly),
        "bit 0 set and bit 1 clear: neither flag finding");

  report.clear();
  icValidateStatus rv = validate(0, report);
  check(has(report, kNotEmbedded), "bit 0 clear: the not-marked-embedded finding is drawn");
  check(!has(report, kDataOnly), "bit 0 clear: the bit 1 finding is not drawn");
  check(rv >= icValidateNonCompliant, "bit 0 clear: the status is at least non-compliant");

  /* "Only iccMAX profiles containing 0 in bit position 1 should be
     embedded": a warning. */
  report.clear();
  validate(icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly, report);
  check(has(report, kDataOnly), "bit 1 set: the cannot-be-used-independently finding is drawn");
  check(!has(report, kNotEmbedded), "bit 1 set: the bit 0 finding is not drawn");

  report.clear();
  validate(icUseWithEmbeddedDataOnly, report);
  check(has(report, kNotEmbedded) && has(report, kDataOnly), "both wrong: both findings are drawn");
  check(has(report, (std::string(icMsgValidateNonCompliant) + " - Embedded profile header flags do not").c_str()),
        "the bit 0 finding carries the non-compliant prefix");
  check(has(report, (std::string(icMsgValidateWarning) + " - Embedded profile header flags say").c_str()),
        "the bit 1 finding carries the warning prefix");
  check(!has(report, "device class does not match"), "a matching device class draws no class error");
  check(!has(report, "device space"), "a matching device space draws no space error");
}

/* "the same as or superset of the device space of the encapsulating
   profile". */
void testDeviceSpace()
{
  CIccInfo info;
  std::string rgb = info.GetColorSpaceSigName(icSigRgbData);
  std::string cmyk = info.GetColorSpaceSigName(icSigCmykData);
  std::string clr3 = info.GetColorSpaceSigName(icSig3colorData);
  std::string gray = info.GetColorSpaceSigName(icSigGrayData);

  std::string report;
  validate(icEmbeddedProfileTrue, report, icSigRgbData);
  check(!has(report, "device space") && !has(report, "fewer device channels"),
        "the same device space draws no device-space error");

  /* CMYK is a standard superset of CMY: its channels are defined by the
     space, so it needs no colorant tag of its own. */
  report.clear();
  validate(icEmbeddedProfileTrue, report, icSigCmykData, icSigCmyData);
  check(!has(report, "device space") && !has(report, "fewer device channels"),
        "a CMYK child with no colorant tag under a CMY host is a superset: no device-space finding");

  /* A named color superset has channels the host's colorant tags do not
     define, so it needs a colorant tag of its own; the host's does not
     count. */
  report.clear();
  validate(icEmbeddedProfileTrue, report, icSig4colorData, icSigRgbData, true);
  check(!has(report, "device space") && !has(report, "fewer device channels"),
        "a 4CLR child with its own colorants under an RGB host is a superset: no device-space finding");

  report.clear();
  validate(icEmbeddedProfileTrue, report, icSig7colorData, icSigCmykData, true);
  check(!has(report, "device space") && !has(report, "fewer device channels"),
        "a 7CLR child with its own colorants under a CMYK host is a superset: no device-space finding");

  std::string clr7 = info.GetColorSpaceSigName(icSig7colorData);
  report.clear();
  icValidateStatus rv = validate(icEmbeddedProfileTrue, report, icSig7colorData, icSigCmykData, false, true);
  check(has(report, ("device space " + clr7 + " is a named color superset of the parent profile's " + cmyk).c_str()) &&
        has(report, kNoChannels),
        "a 7CLR child with no colorant tag under a CMYK host: the missing channel definition is drawn, "
        "though the host has a colorantTableTag");
  check(has(report, (std::string(icMsgValidateNonCompliant) + " - Embedded profile device space").c_str()),
        "the missing channel definition is non-compliant");
  check(!has(report, kNotSuperset), "a superset without colorants draws no not-a-superset error");
  check(rv >= icValidateNonCompliant, "a superset without its own colorants is at least non-compliant");

  report.clear();
  validate(icEmbeddedProfileTrue, report, icSigRgbData);
  check(!has(report, kNoChannels), "the same device space needs no colorant tag of its own");

  report.clear();
  rv = validate(icEmbeddedProfileTrue, report, icSigCmykData);
  check(has(report, ("device space " + cmyk + " " + kNotSuperset + " " + rgb).c_str()),
        "a CMYK child under an RGB host: the device-space error names both spaces");
  check(!has(report, "fewer device channels"), "a wider child draws no channel-count error");
  check(!has(report, kNoChannels), "a space that is not a superset draws no channel-definition finding");
  check(rv >= icValidateCriticalError, "a named space that is not the host's is a critical error");

  report.clear();
  rv = validate(icEmbeddedProfileTrue, report, icSig3colorData);
  check(has(report, ("device space " + clr3 + " " + kNotSuperset + " " + rgb).c_str()),
        "a 3CLR child under an RGB host is no wider: the device-space error is drawn");
  check(rv >= icValidateCriticalError, "a generic space no wider than the host's is a critical error");

  report.clear();
  rv = validate(icEmbeddedProfileTrue, report, icSigGrayData);
  check(has(report, ("device space " + gray + " " + kNotSuperset + " " + rgb).c_str()),
        "a GRAY child under an RGB host: the device-space error is drawn");
  check(has(report, "fewer device channels than parent profile"),
        "a narrower child also draws the channel-count error");
  check(rv >= icValidateCriticalError, "a narrower child is a critical error");
}

/* A v4 CMYK Output host.  With bPcsToDevice it holds the full ICC.1 tag set;
   without it, no BToAx tag. */
void initOutputHost(CIccProfile &host, bool bPcsToDevice)
{
  host.InitHeader();
  host.m_Header.version = icVersionNumberV4_3;
  host.m_Header.deviceClass = icSigOutputClass;
  host.m_Header.colorSpace = icSigCmykData;
  host.m_Header.pcs = icSigLabData;
  const icTagSignature hostTags[] = {
    icSigProfileDescriptionTag, icSigCopyrightTag, icSigMediaWhitePointTag,
    icSigAToB0Tag, icSigAToB1Tag, icSigAToB2Tag, icSigGamutTag,
  };
  for (size_t i = 0; i < sizeof(hostTags) / sizeof(hostTags[0]); ++i)
    attachText(&host, hostTags[i]);
  if (bPcsToDevice) {
    attachText(&host, icSigBToA0Tag);
    attachText(&host, icSigBToA1Tag);
    attachText(&host, icSigBToA2Tag);
  }
}

/* The case that exposed the rule: a v4 CMYK Output host holding a v5
   spectral-only Output child with a DToB3Tag and no PCS-to-device tag,
   description, copyright or white point. */
void testRequiredTags()
{
  CIccProfile host;
  initOutputHost(host, true);

  CIccProfile *pChild = new CIccProfile;
  pChild->InitHeader();
  pChild->m_Header.version = icVersionNumberV5;
  pChild->m_Header.deviceClass = icSigOutputClass;
  pChild->m_Header.colorSpace = icSigCmykData;
  pChild->m_Header.pcs = (icColorSpaceSignature)0;
  pChild->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | 36);
  pChild->m_Header.flags = icEmbeddedProfileTrue;
  attachText(pChild, icSigDToB3Tag);

  CIccTagEmbeddedProfile tag;
  tag.SetProfile(pChild);

  std::string report;
  tag.Validate("", report, &host);
  check(!has(report, "Critical tag(s) missing"),
        "embedded: no transform-pair requirement when the host holds the BToAx tags");
  check(!has(report, "Required tags missing"), "embedded: no description or copyright requirement");
  check(!has(report, "Media white point tag missing"), "embedded: no white point requirement");

  /* A required tag the host does not hold is still required of the child. */
  CIccProfile bareHost;
  initOutputHost(bareHost, false);
  report.clear();
  icValidateStatus rv = tag.Validate("", report, &bareHost);
  check(has(report, "Critical tag(s) missing"),
        "embedded: with no PCS-to-device tag in the host either, the child's is still required");
  check(!has(report, "Required tags missing"),
        "embedded: the host's description and copyright still count");
  check(rv >= icValidateCriticalError, "embedded: the missing transform is a critical error");

  /* The same child on its own is an incomplete ICC.2 Output profile. */
  report.clear();
  rv = pChild->Validate(report);
  check(has(report, "Critical tag(s) missing"),
        "stand-alone: the missing PCS-to-device transform is still reported");
  check(has(report, "Required tags missing"),
        "stand-alone: the missing description and copyright are still reported");
  check(rv >= icValidateCriticalError, "stand-alone: the incomplete child is a critical error");
}

/* A colorantTableTag naming the given colorants. */
void attachColorants(CIccProfile *pProfile, const char *const *names, icUInt16Number n)
{
  CIccTagColorantTable *pTable = new CIccTagColorantTable;
  pTable->SetSize(n);
  for (icUInt16Number i = 0; i < n; ++i)
    std::strncpy((char*)pTable->GetEntry(i)->name, names[i], sizeof(pTable->GetEntry(i)->name) - 1);
  pProfile->AttachTag(icSigColorantTableTag, pTable);
}

/* A superset's first channels shall be the parent's in the same order;
   checked by colorant name when both profiles have a colorantTableTag. */
void testColorantOrder()
{
  static const char *const kCmyk[] = { "Cyan", "Magenta", "Yellow", "Black" };
  static const char *const kOrdered[] = { "Cyan", "Magenta", "Yellow", "Black", "Orange", "Green", "Violet" };
  static const char *const kSwapped[] = { "Magenta", "Cyan", "Yellow", "Black", "Orange", "Green", "Violet" };
  const char *kOrder = "a superset device space shall begin with the parent's channels in the same order";

  struct Case {
    const char *const *childNames;
    icUInt16Number nChild;
    bool bHostTable;
    const char *expect;   /* NULL: no order finding */
    const char *what;
  } cases[] = {
    { kOrdered, 7, true, NULL, "parent's colorants first, in order: no order finding" },
    { kSwapped, 7, true, "channel 1 is colorant \"Magenta\" but the parent profile's channel 1 is \"Cyan\"",
      "first two channels swapped: the first mismatch is reported" },
    { kOrdered, 3, true, "channel 4 has no colorant table entry but the parent profile's channel 4 is \"Black\"",
      "a child table shorter than the parent's: the missing entry is reported" },
    { kSwapped, 7, false, NULL, "no colorantTableTag in the host: order is not checked" },
  };

  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
    CIccProfile host;
    host.InitHeader();
    host.m_Header.version = icVersionNumberV4_3;
    host.m_Header.deviceClass = icSigOutputClass;
    host.m_Header.colorSpace = icSigCmykData;
    host.m_Header.pcs = icSigLabData;
    if (cases[c].bHostTable)
      attachColorants(&host, kCmyk, 4);

    CIccProfile *pChild = new CIccProfile;
    pChild->InitHeader();
    pChild->m_Header.version = icVersionNumberV5;
    pChild->m_Header.deviceClass = icSigOutputClass;
    pChild->m_Header.colorSpace = icSig7colorData;
    pChild->m_Header.pcs = icSigLabData;
    pChild->m_Header.flags = icEmbeddedProfileTrue;
    attachColorants(pChild, cases[c].childNames, cases[c].nChild);

    CIccTagEmbeddedProfile tag;
    tag.SetProfile(pChild);

    std::string report;
    icValidateStatus rv = tag.Validate("", report, &host);
    if (cases[c].expect) {
      check(has(report, cases[c].expect) && has(report, kOrder), cases[c].what);
      check(rv >= icValidateNonCompliant, "a channel-order mismatch is at least non-compliant");
    }
    else {
      check(!has(report, kOrder), cases[c].what);
    }
    check(!has(report, kNoChannels), "a child with its own colorantTableTag draws no channel-definition finding");
  }
}

/* The xCLR Output colorant recommendation (ICC.2-2023 8.5) in the child's
   own validation: a host colorantTableTag stands in only for the same device
   space. */
void testChannelTags()
{
  const char *kClrRecommended =
    "xCLR output profile has neither the recommended colorantInfoTag nor a colorantTableTag";

  for (int bSameSpace = 0; bSameSpace < 2; ++bSameSpace) {
    CIccProfile host;
    host.InitHeader();
    host.m_Header.version = icVersionNumberV4_3;
    host.m_Header.deviceClass = icSigOutputClass;
    host.m_Header.colorSpace = bSameSpace ? icSig7colorData : icSig4colorData;
    host.m_Header.pcs = icSigLabData;
    attachText(&host, icSigColorantTableTag);

    CIccProfile *pChild = new CIccProfile;
    pChild->InitHeader();
    pChild->m_Header.version = icVersionNumberV5;
    pChild->m_Header.deviceClass = icSigOutputClass;
    pChild->m_Header.colorSpace = icSig7colorData;
    pChild->m_Header.pcs = icSigLabData;
    pChild->m_Header.flags = icEmbeddedProfileTrue;
    attachText(pChild, icSigAToB0Tag);

    CIccTagEmbeddedProfile tag;
    tag.SetProfile(pChild);

    std::string report;
    tag.Validate("", report, &host);
    if (bSameSpace)
      check(!has(report, kClrRecommended),
            "7CLR child under a 7CLR host: the host's colorantTableTag satisfies the recommendation");
    else
      check(has(report, kClrRecommended),
            "7CLR child under a 4CLR host: the host's colorantTableTag does not define the child's channels");
  }
}

/* The annex's example of a tag the embedded profile's own tags depend on:
   relative and absolute colorimetric transforms need the profile's own
   mediaWhitePointTag, even when the host has one. */
void testWhitePoint()
{
  const char *kNoWhite = "Media white point tag missing";

  struct Case {
    icTagSignature deviceToPcs;
    bool bSpectralPcs;
    bool bNeedsOwn;
    const char *what;
  } cases[] = {
    { icSigAToB0Tag, false, false, "a child with only perceptual transforms uses the host's white point" },
    { icSigAToB1Tag, false, true, "a child with a relative colorimetric transform needs its own white point" },
    { icSigDToB3Tag, true, false,
      "a child whose absolute DToB3Tag uses its spectral PCS needs no colorimetric white point of its own" },
  };

  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
    CIccProfile host;
    initOutputHost(host, true);

    CIccProfile *pChild = new CIccProfile;
    pChild->InitHeader();
    pChild->m_Header.version = icVersionNumberV5;
    pChild->m_Header.deviceClass = icSigOutputClass;
    pChild->m_Header.colorSpace = icSigCmykData;
    pChild->m_Header.pcs = icSigLabData;
    if (cases[c].bSpectralPcs)
      pChild->m_Header.spectralPCS = (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | 36);
    pChild->m_Header.flags = icEmbeddedProfileTrue;
    attachText(pChild, cases[c].deviceToPcs);
    attachText(pChild, icSigBToA0Tag);

    CIccTagEmbeddedProfile tag;
    tag.SetProfile(pChild);

    std::string report;
    tag.Validate("", report, &host);
    check(has(report, kNoWhite) == cases[c].bNeedsOwn, cases[c].what);
  }
}

}  // namespace

int main()
{
  testFlags();
  testDeviceSpace();
  testChannelTags();
  testColorantOrder();
  testRequiredTags();
  testWhitePoint();

  if (g_fail) {
    std::fprintf(stderr, "[embedded-profile-flags] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[embedded-profile-flags] PASS\n");
  return 0;
}
