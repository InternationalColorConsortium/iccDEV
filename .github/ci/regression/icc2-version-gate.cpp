// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       icc2-version-gate.cpp

    Contains:   CTest helper for the profile version at which ICC.2:2023 content
                is accepted (ICC.2:2023 clause 7.2.6).

    ICC.2:2023 gives 5.0.0.0 as the version "consistent with this
    specification", but CIccProfile::Validate() accepted several of its
    additions only from 5.1.0.0, which no edition of ICC.2 defines.  #2561.

      - Flag bit 3, extended-range PCS (7.2.13 Table 18): NonCompliant at
        exactly 5.0.0.0 ("bits 3-15"), accepted from 5.1.
      - cicpTag (9.2.48): "Invalid tag type" at exactly 5.0.0.0.
      - A spectralViewingConditionsType with zero observer or illuminant steps
        (10.2.22): NonCompliant below 5.1.
      - The 'not ' calculator operator (Table 102), with 'neq ', and the
        sampled calculator curve segment 'clcf' (11.2.2.3): a warning below
        5.1.

    The last two never ran at all: CIccTagMultiProcessElement::Validate() did
    not pass the profile to its elements, so every element saw NULL.  #2603.
    With the profile passed, a 5.1 gate would have warned on eight tracked
    5.0 profiles that use 'not ', so both move to 5.0 with the others.

    Each gate is pinned at 4.40, 5.00 and 5.10: 5.00 and 5.10 must agree, and
    4.40 must still report, since ICC.1 has none of this content.  The 4.40
    rows of the two element gates are the check that the profile now reaches
    the elements.  Takes Testing/Calc/srgbCalcTest.xml (a 'not ' calculator)
    and Testing/Display/Rec2100HlgFullCC.xml (zero-step observer and
    illuminant, and a 'clcf' segment) as argv[1] and argv[2].
*/

#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagFactory.h"
#include "IccMpeFactory.h"
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

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[icc2-version-gate] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

// The report lines containing both needles, and nothing else: relabelling a
// profile draws findings that are not under test.
std::string lines(const std::string &report, const char *what, const char *also = NULL)
{
  std::string out;
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (has(line, what) && (!also || has(line, also)))
      out += line + "\n";
    pos = end + 1;
  }
  return out;
}

class ScopedFactories {
public:
  ScopedFactories()
  {
    CIccTagCreator::PushFactory(new CIccTagXmlFactory());
    CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());
  }
  ~ScopedFactories()
  {
    delete CIccMpeCreator::PopFactory();
    delete CIccTagCreator::PopFactory();
  }
  ScopedFactories(const ScopedFactories &) = delete;
  ScopedFactories &operator=(const ScopedFactories &) = delete;
};

const icUInt32Number kV4_3 = 0x04300000;
const icUInt32Number kV4_4 = 0x04400000;
const icUInt32Number kV5   = icVersionNumberV5;
const icUInt32Number kV5_1 = 0x05100000;

std::string validateAt(CIccProfile &profile, icUInt32Number version)
{
  profile.m_Header.version = version;
  std::string report;
  profile.Validate(report);
  return report;
}

// Report found (want) or absent (!want) at the version, with the lines as evidence.
void expect(CIccProfile &profile, icUInt32Number version, const char *what, const char *also,
            bool want, const char *name)
{
  std::string got = lines(validateAt(profile, version), what, also);
  char label[96];
  std::snprintf(label, sizeof(label), "%s at %08X", name, (unsigned int)version);
  check(got.empty() != want, label,
        want ? "not reported" : ("reported: " + got).c_str());
}

bool load(CIccProfileXml &profile, const char *szXml)
{
  std::string parseStr;
  if (!profile.LoadXml(szXml, NULL, &parseStr)) {
    check(false, szXml, ("fixture did not load: " + parseStr).c_str());
    return false;
  }
  return true;
}

void caseCalcProfile(const char *szXml)
{
  CIccProfileXml profile;
  if (!load(profile, szXml))
    return;

  // Flag bit 3: ICC.2 Table 18 defines bits 0-3; ICC.1 v4 reserves bits 2-15.
  const char *kFlags = "Reserved profile flags";
  profile.m_Header.flags = icExtendedRangePCS;
  expect(profile, kV4_4, kFlags, NULL, true,  "flag bit 3");
  expect(profile, kV5,   kFlags, NULL, false, "flag bit 3");
  expect(profile, kV5_1, kFlags, NULL, false, "flag bit 3");

  // Control: bit 4 stays reserved in v5.
  profile.m_Header.flags = 0x00000010;
  expect(profile, kV5,   "bits 4-15", NULL, true, "flag bit 4");
  expect(profile, kV5_1, "bits 4-15", NULL, true, "flag bit 4");
  profile.m_Header.flags = 0;

  // The fixture's calculator uses 'not ' at 5.0.
  const char *kNot = "Calculator operator(s) not supported by profile version: (not)";
  expect(profile, kV4_4, kNot, NULL, true,  "'not ' operator");
  expect(profile, kV5,   kNot, NULL, false, "'not ' operator");
  expect(profile, kV5_1, kNot, NULL, false, "'not ' operator");

  // cicpTag: ICC.1 from v4.4, and ICC.2.
  CIccTagCicp *pCicp = new CIccTagCicp;
  pCicp->SetFields(9, 16, 0, 1);
  if (!profile.AttachTag(icSigCicpTag, pCicp)) {
    delete pCicp;
    check(false, "cicpTag", "the fixture already has a cicpTag");
    return;
  }
  const char *kCicp = "cicpTag";
  const char *kInvalid = "Invalid tag type";
  expect(profile, kV4_3, kCicp, kInvalid, true,  "cicpTag");
  expect(profile, kV4_4, kCicp, kInvalid, false, "cicpTag");
  expect(profile, kV5,   kCicp, kInvalid, false, "cicpTag");
  expect(profile, kV5_1, kCicp, kInvalid, false, "cicpTag");
}

void caseSpectralProfile(const char *szXml)
{
  CIccProfileXml profile;
  if (!load(profile, szXml))
    return;

  const char *kVersion[] = {
    "Missing Observer CMF not supported by profile version",
    "Missing illuminant SPD not supported by profile version",
    "sampled calculator curve is not supported by version of profile",
  };
  const char *kName[] = { "zero-step observer", "zero-step illuminant", "'clcf' segment" };

  for (int i = 0; i < 3; i++) {
    expect(profile, kV4_4, kVersion[i], NULL, true,  kName[i]);
    expect(profile, kV5,   kVersion[i], NULL, false, kName[i]);
    expect(profile, kV5_1, kVersion[i], NULL, false, kName[i]);
  }
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s srgbCalcTest.xml Rec2100HlgFullCC.xml\n", argv[0]);
    return 2;
  }

  ScopedFactories factories;
  caseCalcProfile(argv[1]);
  caseSpectralProfile(argv[2]);

  if (g_fail) {
    std::fprintf(stderr, "[icc2-version-gate] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[icc2-version-gate] all checks passed\n");
  return 0;
}
