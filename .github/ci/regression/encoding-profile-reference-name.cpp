// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       encoding-profile-reference-name.cpp

    Contains:   CTest helper for how a ColorEncodingSpace profile names its
                encoding (#1993).

    ICC.2:2023, ColorEncodingSpace profile (8.7), with referenceNameTag (9.2.108)
    and colorSpaceNameTag (9.2.50):

      - referenceNameTag is required.  "ISO 22028-1" means the profile defines
        the encoding itself: colorEncodingParamsTag and colorSpaceNameTag are
        both required.
      - Any other reference name is an encoding in the ICC 3-component colour
        encoding registry.  colorSpaceNameTag is optional there, and when present
        shall contain the same text as referenceNameTag; a mismatch is not
        allowed, so the converter refuses it and the validator reports it.
      - Paragraphs 3 and 4: colorEncodingParamsTag, when present, always
        overrides.  A known registry name supplies the values for elements the
        tag does not carry; an unknown name supplies none.

    icConvertEncodingProfile looked the registry encoding up by colorSpaceNameTag
    alone, so a conforming profile that omitted that optional tag could not be
    converted, and Testing/Encoding/sRgbEncodingOverrides.xml never reached the
    override path it was built for.  An unknown name was refused even when a
    colorEncodingParamsTag was present.  The validator checked only that
    referenceNameTag existed.

    The test is hermetic.  It installs a stand-in registry that records the name
    it is asked for, and a stand-in converter that captures the parameters it is
    handed, so each case checks exactly which values reach the conversion.
*/

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"

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
    std::fprintf(stderr, "[encoding-profile-reference-name] FAIL %s: %s\n", label, what);
  }
}

const icSignature kMbpl = icSigCeptMediumBlackPointLuminanceMbr;
const icSignature kMwpl = icSigCeptMediumWhitePointLuminanceMbr;
const icSignature kWlum = icSigCeptWhitePointLuminanceMbr;

// The registry's values for "sRGB" in this test.  All exact in float16.
const icFloatNumber kRegMbpl = 0.25f, kRegMwpl = 80.0f, kRegWlum = 100.0f;

bool attachValue(CIccTagStruct *pParams, icSignature sig, icFloatNumber v)
{
  CIccTagFloat16 *pTag = (CIccTagFloat16 *)CIccTag::Create(icSigFloat16ArrayType);
  if (!pTag)
    return false;
  if (!pTag->SetSize(1)) {
    delete pTag;
    return false;
  }
  (*pTag)[0] = v;
  if (!pParams->AttachElem(sig, pTag)) {
    delete pTag;
    return false;
  }
  return true;
}

CIccTagStruct *newParams()
{
  CIccTagStruct *p = (CIccTagStruct *)CIccTag::Create(icSigTagStructType);
  if (p && !p->SetTagStructType(icSigColorEncodingParamsSruct)) {
    delete p;
    return NULL;
  }
  return p;
}

bool attachText(CIccProfile *pIcc, icSignature sig, const char *szText)
{
  CIccTagUtf8Text *pTag = new CIccTagUtf8Text;
  pTag->SetText(szText);
  if (!pIcc->AttachTag(sig, pTag)) {
    delete pTag;
    return false;
  }
  return true;
}

// A ColorEncodingSpace profile.  szName and pParams are optional.
CIccProfile *newEncodingProfile(const char *szRef, const char *szName, CIccTagStruct *pParams)
{
  CIccProfile *pIcc = new CIccProfile;
  pIcc->InitHeader();
  pIcc->m_Header.version = icVersionNumberV5;
  pIcc->m_Header.deviceClass = icSigColorEncodingClass;
  pIcc->m_Header.colorSpace = icSigRgbData;
  attachText(pIcc, icSigReferenceNameTag, szRef);
  if (szName)
    attachText(pIcc, icSigColorSpaceNameTag, szName);
  if (pParams && !pIcc->AttachTag(icSigColorEncodingParamsTag, pParams))
    delete pParams;
  return pIcc;
}

// The stand-in registry: knows "sRGB" only, and records every name asked for.
class TestRegistry : public IIccEncProfileCacheHandler
{
public:
  std::string m_lastName;
  int m_nCalls = 0;

  virtual CIccProfile *GetEncodingProfile(const icUChar *szName)
  {
    m_nCalls++;
    m_lastName = (const char *)szName;
    if (m_lastName != "sRGB")
      return NULL;
    CIccTagStruct *p = newParams();
    if (!p || !attachValue(p, kMbpl, kRegMbpl) || !attachValue(p, kMwpl, kRegMwpl) ||
        !attachValue(p, kWlum, kRegWlum)) {
      delete p;
      return NULL;
    }
    return newEncodingProfile("ISO 22028-1", "sRGB", p);
  }
};

// The stand-in converter: keeps a copy of the parameters it is handed.
class CaptureConverter : public IIccEncProfileConverter
{
public:
  CIccTagStruct *m_pParams = NULL;
  int m_nCalls = 0;

  virtual ~CaptureConverter() { delete m_pParams; }

  virtual icStatusEncConvert ConvertFromParams(CIccProfilePtr &newIcc, CIccTagStruct *pParams,
                                               icHeader * /*pHeader*/)
  {
    m_nCalls++;
    delete m_pParams;
    m_pParams = pParams ? (CIccTagStruct *)pParams->NewCopy() : NULL;
    newIcc = NULL;
    return icEncConvertOk;
  }
};

// The captured value of one member, or -1 if it is absent.
icFloatNumber member(CIccTagStruct *pParams, icSignature sig)
{
  if (!pParams)
    return -1;
  CIccTagFloat16 *pTag = (CIccTagFloat16 *)pParams->FindElemOfType(sig, icSigFloat16ArrayType);
  if (!pTag || !pTag->GetSize())
    return -1;
  return (*pTag)[0];
}

TestRegistry *g_pRegistry = NULL;
CaptureConverter *g_pConverter = NULL;

// Convert one profile through the stand-ins, resetting what they record first.
icStatusEncConvert convert(CIccProfile *pIcc)
{
  g_pRegistry->m_lastName.clear();
  g_pRegistry->m_nCalls = 0;
  g_pConverter->m_nCalls = 0;
  delete g_pConverter->m_pParams;
  g_pConverter->m_pParams = NULL;
  CIccProfilePtr pNew = NULL;
  icStatusEncConvert stat = icConvertEncodingProfile(pNew, pIcc);
  delete pNew;
  return stat;
}

bool reportHas(CIccProfile *pIcc, const char *szNeedle)
{
  std::string report;
  pIcc->Validate(report);
  return report.find(szNeedle) != std::string::npos;
}

} // namespace

int main()
{
  // The library owns handlers once installed; keep pointers to read them back.
  g_pRegistry = new TestRegistry;
  g_pConverter = new CaptureConverter;
  IIccEncProfileCacheHandler::SetEncCacheHandler(g_pRegistry);
  IIccEncProfileConverter::SetEncProfileConverter(g_pConverter);

  // ---- conversion ---------------------------------------------------------
  {
    // Registry name, no colorSpaceNameTag, no parameters: the registry values.
    CIccProfile *p = newEncodingProfile("sRGB", NULL, NULL);
    check(convert(p) == icEncConvertOk, "registry only", "not converted");
    check(g_pRegistry->m_lastName == "sRGB", "registry only",
          "the registry was not asked for the referenceNameTag's name");
    check(member(g_pConverter->m_pParams, kMbpl) == kRegMbpl &&
          member(g_pConverter->m_pParams, kMwpl) == kRegMwpl,
          "registry only", "the registry values did not reach the conversion");
    delete p;
  }
  {
    // Registry name with parameters: they override, the rest keep registry values.
    CIccTagStruct *pOver = newParams();
    attachValue(pOver, kMbpl, 0.5f);
    attachValue(pOver, kMwpl, 200.0f);
    CIccProfile *p = newEncodingProfile("sRGB", NULL, pOver);
    check(convert(p) == icEncConvertOk, "overrides", "not converted");
    check(member(g_pConverter->m_pParams, kMbpl) == 0.5f &&
          member(g_pConverter->m_pParams, kMwpl) == 200.0f,
          "overrides", "the profile's own parameters did not override the registry's");
    check(member(g_pConverter->m_pParams, kWlum) == kRegWlum, "overrides",
          "an element the profile does not carry lost its registry value");
    delete p;
  }
  {
    // colorSpaceNameTag that differs from referenceNameTag: not allowed, so refused
    // before the registry is consulted.
    CIccProfile *p = newEncodingProfile("sRGB", "bg-sRGB", NULL);
    check(convert(p) == icEncConvertBadProfile, "mismatched name",
          "a colorSpaceNameTag that differs from referenceNameTag was not refused");
    check(g_pRegistry->m_nCalls == 0 && g_pConverter->m_nCalls == 0, "mismatched name",
          "the registry or the converter was reached anyway");
    delete p;
  }
  {
    // colorSpaceNameTag that matches: allowed, and the encoding is looked up.
    CIccProfile *p = newEncodingProfile("sRGB", "sRGB", NULL);
    check(convert(p) == icEncConvertOk && g_pRegistry->m_lastName == "sRGB", "matching name",
          "a colorSpaceNameTag equal to referenceNameTag was not accepted");
    delete p;
  }
  {
    // A name the registry does not know, with parameters: they are the values used.
    CIccTagStruct *pOwn = newParams();
    attachValue(pOwn, kMbpl, 0.5f);
    CIccProfile *p = newEncodingProfile("unregistered-encoding", NULL, pOwn);
    check(convert(p) == icEncConvertOk, "unknown name with parameters",
          "refused although colorEncodingParamsTag was present");
    check(member(g_pConverter->m_pParams, kMbpl) == 0.5f, "unknown name with parameters",
          "the profile's own parameters did not reach the conversion");
    delete p;
  }
  {
    // A name the registry does not know, and nothing else: nothing to interpret with.
    CIccProfile *p = newEncodingProfile("unregistered-encoding", NULL, NULL);
    check(convert(p) == icEncConvertNoBaseProfile, "unknown name alone",
          "expected icEncConvertNoBaseProfile");
    check(g_pConverter->m_nCalls == 0, "unknown name alone", "the converter was still called");
    delete p;
  }
  {
    // "ISO 22028-1": the profile's own parameters, and the registry is not consulted.
    CIccTagStruct *pOwn = newParams();
    attachValue(pOwn, kMbpl, 0.5f);
    CIccProfile *p = newEncodingProfile("ISO 22028-1", "my-encoding", pOwn);
    check(convert(p) == icEncConvertOk, "ISO 22028-1", "not converted");
    check(g_pRegistry->m_nCalls == 0, "ISO 22028-1", "the registry was consulted");
    check(member(g_pConverter->m_pParams, kMbpl) == 0.5f, "ISO 22028-1",
          "the profile's own parameters did not reach the conversion");
    delete p;
  }

  // ---- validation ---------------------------------------------------------
  const char *kNoParams = "is \"ISO 22028-1\" but colorEncodingParamsTag is missing";
  const char *kNoName = "is \"ISO 22028-1\" but colorSpaceNameTag is missing";
  const char *kMismatch = "colorSpaceNameTag does not contain the same text as referenceNameTag";
  {
    CIccTagStruct *pOwn = newParams();
    attachValue(pOwn, kMbpl, 0.5f);
    CIccProfile *p = newEncodingProfile("ISO 22028-1", "my-encoding", pOwn);
    check(!reportHas(p, kNoParams) && !reportHas(p, kNoName), "validate complete",
          "a complete \"ISO 22028-1\" profile drew a missing-tag finding");
    delete p;
  }
  {
    CIccProfile *p = newEncodingProfile("ISO 22028-1", "my-encoding", NULL);
    check(reportHas(p, kNoParams), "validate no params",
          "an \"ISO 22028-1\" profile without colorEncodingParamsTag was not reported");
    delete p;
  }
  {
    CIccTagStruct *pOwn = newParams();
    attachValue(pOwn, kMbpl, 0.5f);
    CIccProfile *p = newEncodingProfile("ISO 22028-1", NULL, pOwn);
    check(reportHas(p, kNoName), "validate no name",
          "an \"ISO 22028-1\" profile without colorSpaceNameTag was not reported");
    delete p;
  }
  {
    CIccProfile *p = newEncodingProfile("sRGB", "bg-sRGB", NULL);
    check(reportHas(p, kMismatch), "validate mismatch",
          "a colorSpaceNameTag that differs from referenceNameTag was not reported");
    delete p;
  }
  {
    CIccProfile *pMatch = newEncodingProfile("sRGB", "sRGB", NULL);
    CIccProfile *pAbsent = newEncodingProfile("sRGB", NULL, NULL);
    check(!reportHas(pMatch, kMismatch) && !reportHas(pAbsent, kMismatch) &&
          !reportHas(pAbsent, kNoName),
          "validate registry name", "a matching or absent colorSpaceNameTag was reported");
    delete pMatch;
    delete pAbsent;
  }

  if (g_fail) {
    std::fprintf(stderr, "[encoding-profile-reference-name] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[encoding-profile-reference-name] all checks passed\n");
  return 0;
}
