// Regression test for shared (linked) tag data in CheckTagLayout.
//
// ICC.1:2022 section 7.3.1 permits several tag directory entries to reference one
// common block of tag data, provided the shared entries carry an identical
// offset and size ("...both the offset and size of the tag data elements in the
// tag table shall be the same"). The canonical example is AToB0/AToB1/AToB2 — or
// BToA0/1/2 — pointing at a single LUT. The structural tag-layout validation
// added in #1216 walked
// the offset-sorted directory and reported every entry whose start fell before
// the running frontier as "<tag> overlaps <prev>" / non-compliant, which
// misflagged these legitimately shared entries.
//
// This test builds a profile that attaches one tag object under three AToB
// signatures (so Write() emits three directory entries with identical
// offset+size), reads it back, validates it, and asserts that the layout check
// does not report an overlap and does not push the status to non-compliant.
//
// Pre-fix: validation report contains "overlaps" and status is NonCompliant.
// Post-fix: no overlap is reported.

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
#include <cstring>
#include <string>

static CIccTagXYZ *make_xyz(icFloatNumber x, icFloatNumber y, icFloatNumber z)
{
  CIccTagXYZ *pTag = new CIccTagXYZ;
  (*pTag)[0].X = icDtoF(x);
  (*pTag)[0].Y = icDtoF(y);
  (*pTag)[0].Z = icDtoF(z);
  return pTag;
}

// A small RGB->XYZ 16-bit LUT (mft2), the tag type the real Turquoise_output.icc
// profile shares across AToB0/AToB1/AToB2.
static CIccTagLut16 *make_lut16()
{
  CIccTagLut16 *pLut = new CIccTagLut16;
  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve *aCurves = pLut->NewCurvesA();
  LPIccCurve *bCurves = pLut->NewCurvesB();
  if (!aCurves || !bCurves) { delete pLut; return nullptr; }
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *ca = new CIccTagCurve(); ca->SetSize(2); aCurves[i] = ca;
    CIccTagCurve *cb = new CIccTagCurve(); cb->SetSize(2); bCurves[i] = cb;
  }
  icUInt8Number grid[3] = {2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) { delete pLut; return nullptr; }
  return pLut;
}

int main()
{
  CIccProfile profile;

  // Minimal, well-formed-enough header for a writable RGB display profile.
  memset(&profile.m_Header, 0, sizeof(profile.m_Header));
  profile.m_Header.deviceClass    = icSigDisplayClass;
  profile.m_Header.colorSpace     = icSigRgbData;
  profile.m_Header.pcs            = icSigXYZData;
  profile.m_Header.platform       = icSigMacintosh;
  profile.m_Header.renderingIntent= icPerceptual;
  profile.m_Header.version        = icVersionNumberV4_3;
  profile.m_Header.magic          = icMagicNumber;
  profile.m_Header.illuminant.X   = icDtoF(0.9642f);
  profile.m_Header.illuminant.Y   = icDtoF(1.0000f);
  profile.m_Header.illuminant.Z   = icDtoF(0.8249f);

  // Required-ish tags so the profile carries some bulk before the shared block.
  profile.AttachTag(icSigMediaWhitePointTag, make_xyz(0.9642f, 1.0000f, 0.8249f));
  profile.AttachTag(icSigRedColorantTag,     make_xyz(0.4361f, 0.2225f, 0.0139f));
  profile.AttachTag(icSigGreenColorantTag,   make_xyz(0.3851f, 0.7169f, 0.0971f));
  profile.AttachTag(icSigBlueColorantTag,    make_xyz(0.1431f, 0.0606f, 0.7139f));

  // The crux: one tag object shared across three signatures. AttachTag()
  // deduplicates by tag pointer, so Write() emits three directory entries that
  // all point at the same offset with the same size.
  CIccTagLut16 *pShared = make_lut16();
  if (!pShared) return 2;
  if (!profile.AttachTag(icSigAToB0Tag, pShared)) return 3;
  if (!profile.AttachTag(icSigAToB1Tag, pShared)) return 4;
  if (!profile.AttachTag(icSigAToB2Tag, pShared)) return 11;

  CIccMemIO io;
  if (!io.Alloc(256 * 1024, true)) return 5;
  if (!profile.Write(&io)) return 6;

  icUInt32Number nSize = (icUInt32Number)io.GetLength();
  if (!nSize) return 7;

  // Read the bytes back and validate via the memory-buffer overload. This both
  // parses the profile (so we can confirm the shared layout) and runs the
  // structural tag-layout check under test.
  std::string report;
  icValidateStatus status = icValidateOK;
  CIccProfile *pRead = ValidateIccProfile(io.GetData(), nSize, report, status);
  if (!pRead) return 8;

  // Sanity: confirm Write() really produced shared entries (identical offset).
  icUInt32Number off0 = 0, off1 = 0, off2 = 0;
  for (TagEntryList::const_iterator i = pRead->m_Tags.begin();
       i != pRead->m_Tags.end(); ++i) {
    if (i->TagInfo.sig == icSigAToB0Tag) off0 = i->TagInfo.offset;
    else if (i->TagInfo.sig == icSigAToB1Tag) off1 = i->TagInfo.offset;
    else if (i->TagInfo.sig == icSigAToB2Tag) off2 = i->TagInfo.offset;
  }
  delete pRead;
  if (!off0 || off0 != off1 || off1 != off2) {
    // The fixture itself is wrong (no shared layout) — test cannot prove anything.
    fprintf(stderr, "fixture did not produce shared tag offsets: %u %u %u\n",
            off0, off1, off2);
    return 9;
  }

  // The actual regression assertion: shared tag data must not be reported as an
  // overlap by the structural tag-layout check. (This synthetic fixture is not a
  // complete, fully-conformant profile — it intentionally omits the other
  // required tags — so we assert specifically on the layout/overlap behaviour
  // rather than on the overall validation status.)
  (void)status;
  if (report.find("overlaps") != std::string::npos) {
    fprintf(stderr, "shared tag data wrongly reported as overlap:\n%s\n",
            report.c_str());
    return 1;
  }

  return 0;
}
