// Regression for #1976: the gamt transform reported the wrong destination, and
// CIccCmm::IsInGamut() classified 16-bit gamut codes as though the tag were 8-bit.
//
// Two independent defects, both pinned here.
//
// 1. Connection metadata.  ICC.1:2022 9.2.29 lets gamutTag be lut8Type, lut16Type
//    or lutBToAType -- all B-to-A shaped -- so CIccXform::Create()'s icXformLutGamut
//    case forces bInput false to walk the tag in its stored direction.  m_bInput,
//    though, also drives GetDstSpace()/GetNumDstSamples(), whose !m_bInput branches
//    return the profile's *device* space.  CIccCmm::AddXform() had correctly recorded
//    PCS -> icSigGamutData, so the two disagreed and Begin()'s trailing output-count
//    guard rejected the chain with icCmmStatBadSpaceLink.  Every gamt-bearing profile
//    in Testing/ failed this way; the path had no working caller.  The fix marks the
//    xform via SetGamutXform() so the reported destination no longer follows m_bInput.
//
// 2. Classification.  The tag defines zero as in gamut and every non-zero value as
//    out of gamut.  IsInGamut() instead returned true for anything below 1/255 -- a
//    faithful float rewrite of the original (unsigned)(v*255.0) truncation, so the
//    8-bit assumption predates the rewrite.  For a lut16Type gamt that swallows every
//    code below 258, since 1/255 is exactly 257/65535.  All 7 corpus gamt tables store
//    8-bit values replicated x257, so their nodes sit on or above that cutoff and it
//    is interpolation between a zero node and a non-zero one that lands underneath:
//    3444 Lab coordinates swept through CMYK-3DLUTs.icc give 92 such results, the
//    smallest ~7.1e-15.  The fix compares against zero.
//
// The profile is built in memory rather than loaded, because every gamt-bearing
// profile under Testing/ is generated from XML by the suite rather than tracked, so a
// fixture-loading test would depend on generation order.  Its CLUT is flat at 1/65535
// so the applied value is independent of interpolation and of which grid cell the
// input lands in.
//
// Each assertion is red-green against one half of the fix: the Begin()/space
// assertions fail with icCmmStatBadSpaceLink without (1), and the sub-1/255
// assertions fail without (2).  Header + IccProfLib only.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagLut.h"
#include "IccTagBasic.h"
#include "IccDefs.h"

#include <cstdio>
#include <limits>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[gamut-xform] FAIL: %s\n", what);
  }
}

// Smallest non-zero code a lut16Type gamt tag can carry.  This is the value the
// 1/255 threshold used to swallow, and it is what a real gamt profile produces at
// its least-out-of-gamut nodes.
const icFloatNumber kSmallest16BitCode = 1.0f / 65535.0f;

// Build a curve of nEntries evenly-spaced points spanning 0..1, i.e. an identity
// ramp, so the CLUT value passes through the transform unchanged.
CIccTagCurve *identityCurve(icUInt32Number nEntries)
{
  // Guards the division below; a single-entry curve is a gamma curve, not a ramp.
  if (nEntries < 2)
    return NULL;

  CIccTagCurve *pCurve = new CIccTagCurve();
  if (!pCurve || !pCurve->SetSize(nEntries))
    return NULL;

  for (icUInt32Number i = 0; i < nEntries; i++)
    (*pCurve)[i] = (icFloatNumber)i / (icFloatNumber)(nEntries - 1);

  return pCurve;
}

// An output-class CMYK profile with a Lab PCS carrying a lut16Type gamt tag that is
// flat at kSmallest16BitCode.  Only the header fields and the tag the gamut xform
// path consults are populated -- the profile is never written or validated.
CIccProfile *buildGamutProfile()
{
  CIccProfile *pProfile = new CIccProfile();
  if (!pProfile)
    return NULL;

  pProfile->InitHeader();
  pProfile->m_Header.deviceClass = icSigOutputClass;
  pProfile->m_Header.colorSpace = icSigCmykData;
  pProfile->m_Header.pcs = icSigLabData;
  pProfile->m_Header.renderingIntent = icPerceptual;

  // 3 PCS channels in, 1 gamut channel out -- the shape 9.2.29 requires.
  CIccTagLut16 *pLut = new CIccTagLut16();
  if (!pLut) {
    delete pProfile;
    return NULL;
  }
  pLut->Init(3, 1);
  pLut->SetColorSpaces(icSigLabData, icSigGamutData);

  // CIccMBB sizes the A and B curve arrays from IsInputB(), so ask it which side
  // carries how many rather than assuming the lut16 convention.
  LPIccCurve *pCurvesA = pLut->NewCurvesA();
  LPIccCurve *pCurvesB = pLut->NewCurvesB();
  if (!pCurvesA || !pCurvesB) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  const int nA = pLut->IsInputB() ? 1 : 3;
  const int nB = pLut->IsInputB() ? 3 : 1;
  for (int i = 0; i < nA; i++)
    pCurvesA[i] = identityCurve(2);
  for (int i = 0; i < nB; i++)
    pCurvesB[i] = identityCurve(2);

  icUInt8Number grid[3] = { 2, 2, 2 };
  CIccCLUT *pCLUT = pLut->NewCLUT(grid, 2);
  if (!pCLUT) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  // Flat table: 2*2*2 nodes, one output channel each.  A constant table makes the
  // applied result independent of interpolation, so the assertion below pins the
  // classification rather than the interpolator.
  for (icUInt32Number i = 0; i < pCLUT->NumPoints(); i++)
    (*pCLUT)[i] = kSmallest16BitCode;

  if (!pProfile->AttachTag(icSigGamutTag, pLut)) {
    delete pLut;
    delete pProfile;
    return NULL;
  }

  return pProfile;
}

// Defect 2: the tag defines in-gamut as exactly zero.
void testIsInGamutThreshold()
{
  icFloatNumber v;

  v = 0.0f;
  check(CIccCmm::IsInGamut(&v), "zero must be in gamut");

  // Was reported in gamut by the 1/255 threshold; this is the assertion the
  // defect report measured at ~1.5e-05.
  v = kSmallest16BitCode;
  check(!CIccCmm::IsInGamut(&v), "smallest 16-bit code (1/65535) must be out of gamut");

  v = 1.0f / 512.0f;
  check(!CIccCmm::IsInGamut(&v), "1/512 must be out of gamut");

  // The largest 16-bit code the old cutoff still swallowed.  1/255 is exactly
  // 257/65535, so code 256 is the last one below it -- an exact binary value on
  // every platform, unlike a nextafter() step, and it names the real boundary.
  v = 256.0f / 65535.0f;
  check(!CIccCmm::IsInGamut(&v), "largest code below 1/255 (256/65535) must be out of gamut");

  // At and above the old cutoff both behaviours agree; pinned so a future rewrite
  // cannot regress the side that was already correct.
  v = 1.0f / 255.0f;
  check(!CIccCmm::IsInGamut(&v), "1/255 must be out of gamut");

  v = 1.0f;
  check(!CIccCmm::IsInGamut(&v), "one must be out of gamut");

  // Neither compares equal to zero, so both classify as out of gamut -- the safe
  // verdict for a value the transform could not produce meaningfully.
  v = std::numeric_limits<icFloatNumber>::quiet_NaN();
  check(!CIccCmm::IsInGamut(&v), "NaN must not be reported in gamut");

  v = std::numeric_limits<icFloatNumber>::infinity();
  check(!CIccCmm::IsInGamut(&v), "infinity must not be reported in gamut");
}

// Defect 1: the chain must link, and must advertise one gamut channel out.
void testGamutXformConnection()
{
  CIccProfile *pProfile = buildGamutProfile();
  check(pProfile != NULL, "gamut test profile could be built");
  if (!pProfile)
    return;

  CIccCmm cmm;

  // AddXform takes ownership of pProfile, including on its failure paths.
  icStatusCMM rv = cmm.AddXform(pProfile, icPerceptual, icInterpLinear, NULL,
                                icXformLutGamut, false);
  check(rv == icCmmStatOk, "AddXform accepts a gamt profile as a gamut transform");
  if (rv != icCmmStatOk)
    return;

  // Without the fix this is icCmmStatBadSpaceLink (2): Begin()'s output-count guard
  // compares GetDestSamples() (1, from icSigGamutData) against the xform's
  // GetNumDstSamples(), which followed m_bInput and answered 4 for CMYK.
  rv = cmm.Begin();
  check(rv == icCmmStatOk, "Begin() links a PCS -> gamut chain");
  if (rv != icCmmStatOk)
    return;

  check(cmm.GetSourceSpace() == icSigLabData, "source space is the profile PCS");
  check(cmm.GetDestSpace() == icSigGamutData, "destination space is icSigGamutData");
  check(cmm.GetDestSamples() == 1, "destination carries a single gamut channel");

  CIccXform *pXform = cmm.GetLastXform();
  check(pXform != NULL, "gamut xform is present in the chain");
  if (pXform) {
    check(pXform->GetDstSpace() == icSigGamutData,
          "xform reports icSigGamutData, not the device space");
    check(pXform->GetNumDstSamples() == 1,
          "xform reports one destination sample, not the device channel count");
  }

  // End to end: a PCS pixel must produce one gamut sample, and that sample -- being
  // non-zero but well under 1/255 -- must classify as out of gamut.  This is the
  // pair of defects meeting: before the fix the Apply could not be reached at all,
  // and had it been, the value would have been called in gamut.
  const icFloatNumber src[3] = { 0.5f, 0.5f, 0.5f };
  icFloatNumber dst[1] = { -1.0f };

  rv = cmm.Apply(dst, src);
  check(rv == icCmmStatOk, "Apply produces a gamut value");
  if (rv == icCmmStatOk) {
    check(dst[0] > 0.0f && dst[0] < 1.0f / 255.0f,
          "applied gamut value is non-zero and below the old 1/255 cutoff");
    check(!CIccCmm::IsInGamut(dst), "applied sub-1/255 gamut value is out of gamut");
  }
}

}  // namespace

int main()
{
  testIsInGamutThreshold();
  testGamutXformConnection();

  if (g_fail)
    std::fprintf(stderr, "[gamut-xform] %d assertion(s) failed\n", g_fail);

  return g_fail;
}
