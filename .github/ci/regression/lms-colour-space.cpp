// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       lms-colour-space.cpp

    Contains:   CTest helper for the 'LMS ' data colour space (ICC.2:2023 clause
                7.2.8, Table 15, signature 4C4D5320h).

    ICC.2 has listed 'LMS ' as a data colour space since 2019; ICC.1 never has.
    iccDEV had no signature for it, so icGetSpaceSamples() counted it as zero
    channels and IsValidSpace() refused it.  A v5 profile whose only change from
    a valid one was 'RGB ' to 'LMS ' drew a critical "Unknown colour space!" plus
    an "Incorrect number of input channels" error from every tag.  #2565.

    Case 1 pins the utility answers: three channels, a valid space, a name.

    Case 2 is the header verdict.  'LMS ' is accepted at v5 and refused below it,
    as the data colour space and as the PCS (B side) of a DeviceLink, the same way
    CheckHeader() already treats the other ICC.2-only spaces.

    Case 3 is the whole-profile verdict on a tracked document: srgbCalcTest with
    'LMS ' in place of 'RGB ' validates exactly as it does with 'RGB '.

    Case 4 is the CMM.  A lut-based tag on a 3-channel data colour space gets a
    3D transform; an unlisted space falls to the N-dimensional one, which refuses
    three inputs, so an 'LMS ' profile could not be applied at all.  v2RgbLut8 with
    'LMS ' must apply and give the same numbers as with 'RGB ', both when the CMM
    picks the tag and when the caller names it: those are two separate switches.

    Case 5 is the third switch, in CIccXformMpe::Create().  It has no caller in the
    tree, so it is asserted directly: the transform it returns for a lut-based tag
    on 'LMS ' is the 3D one, as it is for 'RGB '.

    Takes srgbCalcTest.xml and v2RgbLut8.xml as argv[1] and argv[2].
*/

#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccTagFactory.h"
#include "IccCmm.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <cmath>
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
    std::fprintf(stderr, "[lms-colour-space] FAIL %s: %s\n", label, what);
  }
}

const icColorSpaceSignature kLms = (icColorSpaceSignature)0x4C4D5320;  // 'LMS '
const icUInt32Number kV4 = 0x04400000;
const icUInt32Number kV5 = icVersionNumberV5;

class ScopedTagFactory {
public:
  explicit ScopedTagFactory(IIccTagFactory *pFactory) { CIccTagCreator::PushFactory(pFactory); }
  ~ScopedTagFactory() { delete CIccTagCreator::PopFactory(); }
  ScopedTagFactory(const ScopedTagFactory &) = delete;
  ScopedTagFactory &operator=(const ScopedTagFactory &) = delete;
};

CIccProfile *loadXml(const char *szXml)
{
  ScopedTagFactory xmlFactory(new CIccTagXmlFactory());
  CIccProfileXml *pProfile = new CIccProfileXml;
  std::string parseStr;
  if (!pProfile->LoadXml(szXml, NULL, &parseStr)) {
    std::fprintf(stderr, "[lms-colour-space] cannot parse %s: %s\n", szXml, parseStr.c_str());
    delete pProfile;
    return NULL;
  }
  return pProfile;
}

// The report lines that mention a colour space, and nothing else.
std::string spaceLines(const std::string &report)
{
  std::string lines;
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (line.find("colour space") != std::string::npos ||
        line.find("number of input channels") != std::string::npos ||
        line.find("number of output channels") != std::string::npos)
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

std::string headerLines(icUInt32Number version, icProfileClassSignature cls,
                        icColorSpaceSignature data, icColorSpaceSignature pcs)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = version;
  prof.m_Header.deviceClass = cls;
  prof.m_Header.colorSpace = data;
  prof.m_Header.pcs = pcs;
  std::string report;
  prof.Validate(report);
  return spaceLines(report);
}

void expectHeader(const char *label, icUInt32Number version, icProfileClassSignature cls,
                  icColorSpaceSignature data, icColorSpaceSignature pcs, const std::string &want)
{
  std::string got = headerLines(version, cls, data, pcs);
  if (got != want) {
    std::string what = "got [" + got + "] want [" + want + "]";
    check(false, label, what.c_str());
  }
}

void caseUtil()
{
  CIccInfo info;
  check(icGetSpaceSamples(kLms) == 3, "util", "icGetSpaceSamples('LMS ') != 3");
  check(info.IsValidSpace(kLms), "util", "IsValidSpace('LMS ') is false");
  check(!std::strcmp(info.GetColorSpaceSigName(kLms), "LmsData"), "util", "GetColorSpaceSigName('LMS ') != LmsData");
}

void caseHeader()
{
  const char *kBadData = "Error! -  - Invalid data colour space (0x4C4D5320) for a v2/v4 profile; only iccMAX (v5) permits this!\n";
  const char *kBadPcs  = "Error! -  - Invalid pcs colour space (0x4C4D5320) for a v2/v4 profile; only iccMAX (v5) permits this!\n";

  expectHeader("v5 display data", kV5, icSigDisplayClass, kLms, icSigXYZData, "");
  expectHeader("v4 display data", kV4, icSigDisplayClass, kLms, icSigXYZData, kBadData);
  expectHeader("v5 link pcs", kV5, icSigLinkClass, icSigRgbData, kLms, "");
  expectHeader("v4 link pcs", kV4, icSigLinkClass, icSigRgbData, kLms, kBadPcs);
  expectHeader("v5 link both", kV5, icSigLinkClass, kLms, kLms, "");

  // Controls: RGB is unchanged at both versions.
  expectHeader("v4 display rgb", kV4, icSigDisplayClass, icSigRgbData, icSigXYZData, "");
  expectHeader("v5 display rgb", kV5, icSigDisplayClass, icSigRgbData, icSigXYZData, "");
}

void caseProfile(const char *szXml)
{
  CIccProfile *pProfile = loadXml(szXml);
  if (!pProfile) {
    check(false, "profile", "fixture did not load");
    return;
  }
  check(pProfile->m_Header.colorSpace == icSigRgbData, "profile", "fixture is no longer an RGB profile");

  std::string rgbReport, lmsReport;
  icValidateStatus rgb = pProfile->Validate(rgbReport);
  pProfile->m_Header.colorSpace = kLms;
  icValidateStatus lms = pProfile->Validate(lmsReport);

  check(rgb <= icValidateWarning, "profile", "the RGB fixture no longer validates");
  if (lms != rgb) {
    std::string what = "LMS verdict differs from RGB: " + spaceLines(lmsReport);
    check(false, "profile", what.c_str());
  }
  check(spaceLines(lmsReport).empty(), "profile", "LMS report mentions the colour space or channel count");
  delete pProfile;
}

// Returns false on any CMM failure.  pOut receives the PCS value for pIn.
bool applyOne(const char *szXml, icColorSpaceSignature space, bool bExplicitTag,
              const icFloatNumber *pIn, icFloatNumber *pOut, icStatusCMM &stat)
{
  CIccProfile *pProfile = loadXml(szXml);
  if (!pProfile)
    return false;
  pProfile->m_Header.version = kV5;
  pProfile->m_Header.colorSpace = space;

  CIccCmm cmm(space, icSigUnknownData, true);
  if (bExplicitTag) {
    CIccTag *pTag = pProfile->FindTag(icSigAToB0Tag);
    if (!pTag) {
      delete pProfile;
      return false;
    }
    stat = cmm.AddXform(pProfile, pTag, icPerceptual);  // takes ownership
  }
  else
    stat = cmm.AddXform(pProfile, icPerceptual);  // takes ownership
  if (stat != icCmmStatOk)
    return false;
  stat = cmm.Begin();
  if (stat != icCmmStatOk)
    return false;
  stat = cmm.Apply(pOut, pIn);
  return stat == icCmmStatOk;
}

void caseCmm(const char *szXml)
{
  const icFloatNumber in[][3] = { {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.25f, 0.5f, 0.75f}, {0.9f, 0.1f, 0.4f} };

  for (int explicitTag = 0; explicitTag < 2; explicitTag++)
  for (const auto &px : in) {
    icFloatNumber rgbOut[3] = {0}, lmsOut[3] = {0};
    icStatusCMM stat = icCmmStatOk;
    char label[64];
    std::snprintf(label, sizeof(label), "cmm%s %.2f %.2f %.2f", explicitTag ? " explicit tag" : "",
                  px[0], px[1], px[2]);

    check(applyOne(szXml, icSigRgbData, explicitTag != 0, px, rgbOut, stat), label, "the RGB control does not apply");
    if (!applyOne(szXml, kLms, explicitTag != 0, px, lmsOut, stat)) {
      char what[96];
      std::snprintf(what, sizeof(what), "the LMS profile does not apply (status %d)", (int)stat);
      check(false, label, what);
      continue;
    }
    for (int i = 0; i < 3; i++)
      check(std::fabs(rgbOut[i] - lmsOut[i]) < 1e-6, label, "LMS output differs from RGB output");
  }
}

void caseMpeCreate(const char *szXml)
{
  const icColorSpaceSignature spaces[] = { icSigRgbData, kLms };
  for (icColorSpaceSignature space : spaces) {
    const char *label = space == kLms ? "mpe create LMS" : "mpe create RGB";
    CIccProfile *pProfile = loadXml(szXml);
    if (!pProfile) {
      check(false, label, "fixture did not load");
      continue;
    }
    pProfile->m_Header.version = kV5;
    pProfile->m_Header.colorSpace = space;
    CIccXform *pXform = CIccXformMpe::Create(pProfile, true, icPerceptual);  // owns pProfile
    check(pXform != NULL, label, "no transform");
    if (pXform)
      check(pXform->GetXformType() == icXformType3DLut, label, "not the 3D lut transform");
    delete pXform;
  }
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s srgbCalcTest.xml v2RgbLut8.xml\n", argv[0]);
    return 2;
  }

  caseUtil();
  caseHeader();
  caseProfile(argv[1]);
  caseCmm(argv[2]);
  caseMpeCreate(argv[2]);

  if (g_fail) {
    std::fprintf(stderr, "[lms-colour-space] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[lms-colour-space] all checks passed\n");
  return 0;
}
