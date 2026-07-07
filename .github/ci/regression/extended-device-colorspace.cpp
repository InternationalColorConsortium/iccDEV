// Regression for #626: iccMAX "Extended Device Colour Space" amendment support.
//
// The amendment (ICC.2:2023 minor revision, 30 Oct 2025) adds:
//   * spectralRangeType   ('srng', 10.2.w)  -- a 20-byte tag type carrying a
//                                              spectral + bi-spectral range.
//   * deviceSpectralRangeTag ('dsrn', 9.2.x) -- holds an srng.
//   * devicePccTag        ('dpcc', 9.2.x+1) -- a tagStructType of ...
//   * profileConnectionConditionsStructure ('pcc ', 12.2.y) -- 6 member sub-tags.
//
// This test pins the library-level contract that those signatures/types are
// registered, round-trip losslessly, and validate their required members. It
// links IccProfLib only (mirroring the other regression executables) and uses
// CIccMemIO for the binary round-trip; the XML round-trip is exercised by the
// iccToXml/iccFromXml tools build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccTagFactory.h"
#include "IccStructFactory.h"
#include "IccStructBasic.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[extended-device-colorspace] FAIL: %s\n", what);
  }
}

// --- 1. spectralRangeType ('srng') is registered and round-trips losslessly. ---
void testSpectralRangeType()
{
  // The factory must build a CIccTagSpectralRange for the new type signature.
  CIccTag *pTag = CIccTagCreator::CreateTag(icSigSpectralRangeType);
  check(pTag != NULL, "srng: factory creates a tag");
  if (!pTag)
    return;
  check(pTag->GetType() == icSigSpectralRangeType, "srng: GetType() == icSigSpectralRangeType");

  CIccTagSpectralRange *pSrc = dynamic_cast<CIccTagSpectralRange*>(pTag);
  check(pSrc != NULL, "srng: factory tag is a CIccTagSpectralRange");
  if (!pSrc) {
    delete pTag;
    return;
  }

  // Populate a 380..730nm @ 10nm visible range plus a bi-spectral range so both
  // halves of the 20-byte structure are exercised.
  pSrc->m_nReserved = 0;
  pSrc->m_spectralRange.start = icFtoF16(380.0f);
  pSrc->m_spectralRange.end   = icFtoF16(730.0f);
  pSrc->m_spectralRange.steps = 36;
  pSrc->m_biSpectralRange.start = icFtoF16(400.0f);
  pSrc->m_biSpectralRange.end   = icFtoF16(700.0f);
  pSrc->m_biSpectralRange.steps = 31;

  CIccMemIO io;
  check(io.Alloc(64, true), "srng: allocate write buffer");
  check(pSrc->Write(&io), "srng: Write() succeeds");

  // srng is a fixed 20-byte structure (Table W).
  check(io.GetLength() == 20, "srng: serialized length is 20 bytes");

  io.Seek(0, icSeekSet);
  CIccTagSpectralRange dst;
  check(dst.Read(io.GetLength(), &io), "srng: Read() succeeds");

  check(dst.m_spectralRange.start == pSrc->m_spectralRange.start &&
        dst.m_spectralRange.end   == pSrc->m_spectralRange.end   &&
        dst.m_spectralRange.steps == pSrc->m_spectralRange.steps,
        "srng: spectral range round-trips");
  check(dst.m_biSpectralRange.start == pSrc->m_biSpectralRange.start &&
        dst.m_biSpectralRange.end   == pSrc->m_biSpectralRange.end   &&
        dst.m_biSpectralRange.steps == pSrc->m_biSpectralRange.steps,
        "srng: bi-spectral range round-trips");

  // A buffer shorter than the fixed structure must be rejected, not over-read.
  io.Seek(0, icSeekSet);
  CIccTagSpectralRange tooShort;
  check(!tooShort.Read(8, &io), "srng: short read rejected");

  delete pTag;
}

// --- 2. profileConnectionConditionsStructure ('pcc ') handler + required members. ---
void testProfileConnectionConditionsStruct()
{
  // The struct factory must build the dedicated handler for the new struct sig.
  std::string sName;
  check(CIccStructCreator::GetStructSigName(sName, icSigProfileConnectionConditionsStruct),
        "pcc: struct signature has a registered name");
  check(sName == "profileConnectionConditionsStructure",
        "pcc: struct name is profileConnectionConditionsStructure");

  CIccTagStruct ts;
  check(ts.SetTagStructType(icSigProfileConnectionConditionsStruct),
        "pcc: SetTagStructType succeeds");
  IIccStruct *pHandler = ts.GetStructHandler();
  check(pHandler != NULL, "pcc: struct handler created");
  if (pHandler)
    check(std::string(pHandler->GetDisplayName()) == "profileConnectionConditionsStructure",
          "pcc: handler display name");

  // Attach the four unconditionally-required members (iXYZ/svcn/c2sp/s2cp) plus
  // mwpt. Validate() checks member *presence* by signature, so lightweight XYZ
  // tags stand in for the members here. The struct takes ownership.
  ts.AttachElem(icSigPccPcsIlluminantXYZMbr, new CIccTagXYZ());
  ts.AttachElem(icSigPccSpectralViewingConditionsMbr, new CIccTagXYZ());
  ts.AttachElem(icSigPccCustomToStandardPccMbr, new CIccTagXYZ());
  ts.AttachElem(icSigPccStandardToCustomPccMbr, new CIccTagXYZ());
  ts.AttachElem(icSigPccMediaWhitePointMbr, new CIccTagXYZ());

  std::string sReport;
  ts.Validate("", sReport, NULL);
  check(sReport.find("Missing required profileConnectionConditions member") == std::string::npos,
        "pcc: complete structure reports no missing-member error");

  // Drop a required member (s2cp) and confirm the missing-member error fires.
  check(ts.DeleteElem(icSigPccStandardToCustomPccMbr), "pcc: delete s2cp member");
  std::string sReport2;
  ts.Validate("", sReport2, NULL);
  check(sReport2.find("Missing required profileConnectionConditions member") != std::string::npos,
        "pcc: missing required member is flagged");
}

// --- 3. The new tag and type signatures resolve to their amendment names. ---
void testSignatureNames()
{
  check(std::string(CIccTagCreator::GetTagSigName(icSigDeviceSpectralRangeTag)) == "deviceSpectralRangeTag",
        "dsrn: tag signature name");
  check(std::string(CIccTagCreator::GetTagSigName(icSigDevicePccTag)) == "devicePccTag",
        "dpcc: tag signature name");
  check(std::string(CIccTagCreator::GetTagTypeSigName(icSigSpectralRangeType)) == "spectralRangeType",
        "srng: tag type name");
}

} // namespace

int main()
{
  testSpectralRangeType();
  testProfileConnectionConditionsStruct();
  testSignatureNames();

  if (g_fail)
    std::fprintf(stderr, "[extended-device-colorspace] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stderr, "[extended-device-colorspace] all assertions passed\n");

  return g_fail;
}
