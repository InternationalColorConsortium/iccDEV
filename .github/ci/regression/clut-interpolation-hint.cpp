// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       clut-interpolation-hint.cpp

    Contains:   CTest helper for the interpolation hint in byte 7 of the four
                CLUT processing elements (ICC.2:2023 clause 4.2.5 Table 1, and
                Tables 113, 114, 117 and 122).

    In the CLUT, emission CLUT, extended CLUT and reflectance CLUT elements,
    bytes 4-6 are reserved and byte 7 is an interpolationHintType: 0 none,
    1 trilinear, 2 tetrahedral.  iccDEV reads bytes 4-7 as one reserved word,
    and CIccMultiProcessElement::Validate() required all of it to be zero, so a
    tetrahedral hint was reported NonCompliant ("Reserved Value must be zero").
    #2565.

    The binary, XML and JSON paths already carry the whole word, so the hint
    round-trips; only the verdict changes.  Whether the CMM honours the hint is
    not part of this: ICC.2 says it "should", and iccDEV picks its
    interpolation from the caller.

    Case 1 is each element on its own: hints 0-2 are clean, 3 and FFh are an
    invalid hint, and a bit in bytes 4-6 is still a reserved-value error, alone
    and alongside a valid hint.  A matrix element with the same word is the
    control: outside the four CLUT elements, byte 7 stays reserved.

    Case 2 is the whole profile the issue measured, Testing/CLUT/
    avx2-3d-8-output.xml, with the hint set in its CLUT element.  The same
    profile relabelled v4.4 must still report a hint: ICC.1 Table 63 reserves
    all of bytes 4-7 of the CLUT element.  The element is not given the
    profile, so CIccTagMultiProcessElement::Validate() makes that check.

    Takes avx2-3d-8-output.xml as argv[1].
*/

#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagFactory.h"
#include "IccMpeFactory.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccMpeSpectral.h"
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
    std::fprintf(stderr, "[clut-interpolation-hint] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

// The report lines about the reserved word, and nothing else: a default-built
// element has no table and draws other findings that are not under test.
std::string reservedLines(const std::string &report)
{
  std::string lines;
  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (has(line, "interpolation hint") || has(line, "Reserved Value"))
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

std::string elementLines(CIccMultiProcessElement &elem, icUInt32Number reserved)
{
  elem.m_nReserved = reserved;
  std::string report;
  elem.Validate("", report);
  return reservedLines(report);
}

enum Verdict { kClean, kBadHint, kReserved, kBoth };

void expect(CIccMultiProcessElement &elem, const char *name, icUInt32Number reserved, Verdict want)
{
  std::string lines = elementLines(elem, reserved);
  bool hint = has(lines, "Invalid interpolation hint");
  bool resv = has(lines, "Reserved Value must be zero");

  char label[96];
  std::snprintf(label, sizeof(label), "%s reserved %08X", name, (unsigned int)reserved);
  bool ok = false;
  switch (want) {
    case kClean:    ok = !hint && !resv; break;
    case kBadHint:  ok = hint && !resv;  break;
    case kReserved: ok = !hint && resv;  break;
    case kBoth:     ok = hint && resv;   break;
  }
  check(ok, label, ("got: [" + lines + "]").c_str());
}

void caseElements()
{
  CIccMpeCLUT clut;
  CIccMpeExtCLUT xclt;
  CIccMpeEmissionCLUT eclt;
  CIccMpeReflectanceCLUT rclt;
  struct { CIccMultiProcessElement *p; const char *name; } elems[] = {
    { &clut, "clut" }, { &xclt, "xclt" }, { &eclt, "eclt" }, { &rclt, "rclt" },
  };

  for (auto &e : elems) {
    expect(*e.p, e.name, 0x00000000, kClean);
    expect(*e.p, e.name, 0x00000001, kClean);
    expect(*e.p, e.name, 0x00000002, kClean);
    expect(*e.p, e.name, 0x00000003, kBadHint);
    expect(*e.p, e.name, 0x000000FF, kBadHint);
    expect(*e.p, e.name, 0x00000100, kReserved);
    expect(*e.p, e.name, 0x01000000, kReserved);
    expect(*e.p, e.name, 0x00000102, kReserved);
    expect(*e.p, e.name, 0x00010003, kBoth);
  }

  // Control: byte 7 is reserved in every other element.
  CIccMpeMatrix matrix;
  expect(matrix, "matx", 0x00000002, kReserved);
  expect(matrix, "matx", 0x00000000, kClean);
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

void caseProfile(const char *szXml)
{
  ScopedFactories factories;
  CIccProfileXml profile;
  std::string parseStr;
  if (!profile.LoadXml(szXml, NULL, &parseStr)) {
    check(false, "profile", ("fixture did not load: " + parseStr).c_str());
    return;
  }

  CIccTag *pTag = profile.FindTag(icSigAToB0Tag);
  CIccTagMultiProcessElement *pMpe =
    (pTag && pTag->GetType() == icSigMultiProcessElementType) ? (CIccTagMultiProcessElement*)pTag : NULL;
  CIccMultiProcessElement *pClut = NULL;
  for (int i = 0; pMpe && i < (int)pMpe->NumElements(); i++) {
    CIccMultiProcessElement *pElem = pMpe->GetElement(i);
    if (pElem && pElem->GetType() == icSigCLutElemType)
      pClut = pElem;
  }
  check(pClut != NULL, "profile", "the fixture's AToB0 no longer has a CLUT element");
  if (!pClut)
    return;

  std::string before;
  icValidateStatus rvBefore = profile.Validate(before);
  check(reservedLines(before).empty(), "profile", "the fixture already reports a reserved value");

  const icUInt32Number hints[] = { 1, 2 };
  for (icUInt32Number hint : hints) {
    pClut->m_nReserved = hint;
    std::string report;
    icValidateStatus rv = profile.Validate(report);
    char label[32];
    std::snprintf(label, sizeof(label), "profile hint %u", (unsigned int)hint);
    check(rv == rvBefore, label, ("verdict changed: " + reservedLines(report)).c_str());
    check(reservedLines(report).empty(), label, ("reported: " + reservedLines(report)).c_str());
  }

  pClut->m_nReserved = 3;
  std::string report;
  profile.Validate(report);
  check(has(report, "Invalid interpolation hint"), "profile hint 3", "not reported");

  // v4: byte 7 is reserved.
  profile.m_Header.version = 0x04400000;
  pClut->m_nReserved = 0;
  std::string v4clean;
  profile.Validate(v4clean);
  check(!has(v4clean, "interpolation hint"), "profile v4 hint 0", ("reported: " + reservedLines(v4clean)).c_str());

  pClut->m_nReserved = 2;
  std::string v4hint;
  profile.Validate(v4hint);
  check(has(v4hint, "Reserved Value must be zero; the interpolation hint is defined by ICC.2 only"),
        "profile v4 hint 2", ("not reported: " + reservedLines(v4hint)).c_str());
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s avx2-3d-8-output.xml\n", argv[0]);
    return 2;
  }

  caseElements();
  caseProfile(argv[1]);

  if (g_fail) {
    std::fprintf(stderr, "[clut-interpolation-hint] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[clut-interpolation-hint] all checks passed\n");
  return 0;
}
