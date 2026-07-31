// Coverage for the ICC v2 legacy-PCS path (issue #1883).
//
// ICC v2 encodes 16-bit PCS Lab over 0..0xff00 rather than 0..0xffff, so a v2
// LUT's Lab values have to be rescaled by 65535/65280 before they mean the same
// thing as a v4 profile's. UseLegacyPCS() is the predicate that selects it: it is
// declared false on CIccXform (IccCmm.h:458), overridden true on CIccTagLut16
// (IccTagLut.h:599) and CIccTagNamedColor2 (IccTagBasic.h:707), and the
// CIccXform*DLut classes forward it from whichever tag they wrap.
//
// Two places consume it, and they are not equally observable:
//
//   - CIccCmm::Begin() (IccCmm.cpp:9214 and :9285) splices a CIccPcsXform onto a
//     legacy transform's PCS edge when UseLegacyPCS() is set. This is the leg
//     that produces the rescale seen at the output, and the one group 3 covers.
//   - CIccXform::AdjustPCS() picks Lab2ToXyz/XyzToLab2 over LabToXyz/XyzToLab.
//     It runs here (v2 + perceptual sets m_bAdjustPCS), but it converts
//     Lab -> XYZ -> Lab with the *same* encoding on both sides, so changing both
//     sides together cancels out and is not visible at the output at all. Only a
//     one-sided change is, and group 3 does catch that. Measured, against a
//     deliberately broken library: predicate forced false -> 5 assertions fail;
//     AdjustPCS input leg only -> 4 fail; both AdjustPCS legs together -> passes,
//     correctly, because the encodings cancel.
//
// Nothing in CI reached any of that. A clean checkout's Testing corpus -- the 80
// tracked profiles plus the 130 CreateAllProfiles.sh generates, 210 in all -- is
// 208 v5 and 2 v4 with *no v2 profile at all*, and carries not one 'mft2' or
// 'mft1' tag. (#1883 reports the same zero against a larger working corpus of
// 252.) So every UseLegacyPCS()==true branch above was dead code as far as the
// test suite was concerned. This test supplies the missing coverage without
// depending on any fixture: it builds the profiles it needs in memory, so it
// cannot be silently disabled by a corpus change.
//
// Three groups of assertions:
//
//   1. The predicate itself, per tag class. This is the switch the whole path
//      hangs off, and it is one virtual function away from being flipped by an
//      unrelated refactor. Note CIccTagLut8 is deliberately false: the 0xff00
//      scaling is a property of the 16-bit encoding, and v2 8-bit Lab uses the
//      same encoding as v4.
//
//   2. The conversion math. Lab2ToLab4/Lab4ToLab2 must apply exactly 65535/65280
//      and must be exact inverses, and Lab2ToXyz/XyzToLab2 must round-trip.
//
//      Note this is a contract assertion, not a behavioural one: flipping the
//      predicate is caught here *and* in group 3, so group 1 failing alone would
//      mean the declaration moved without the transform changing.
//
//   3. End to end through CIccCmm. The same CLUT content is installed once as an
//      'mft2' in a v2 profile and once as an 'mAB ' in a v4 profile, and both are
//      applied to the same CMYK inputs. The v4 profile is the unscaled control:
//      it must return its CLUT entries, while the v2 profile must return them
//      expanded by 65535/65280.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[v2-legacy-pcs] FAIL: %s\n", what);
  }
}

// The v2 16-bit Lab encoding runs to 0xff00, not 0xffff. Spelled here the way
// CIccPCSUtil::Lab2ToLab4 spells it so a change to one side shows up against
// the other rather than both moving together.
const double kLegacyFactor = 65535.0 / 65280.0;

icFloatNumber clamp01(double v)
{
  return (icFloatNumber)(v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v));
}

// A smooth, exactly reproducible CMYK->Lab stand-in. The values only have to be
// the same for both profiles under test; nothing here is measured data.
void cmykToLabCell(const double *in, icFloatNumber *out)
{
  const double c = in[0], m = in[1], y = in[2], k = in[3];
  const double ink = 0.30 * c + 0.28 * m + 0.10 * y;
  out[0] = clamp01((1.0 - k) * (1.0 - ink));
  out[1] = clamp01(0.5 + 0.35 * (m - c) * (1.0 - k));
  out[2] = clamp01(0.5 + 0.35 * (y - c) * (1.0 - k));
}

// Build a 4-in/3-out CMYK->Lab LUT tag of the requested type, with identity
// curves either side of a CLUT holding cmykToLabCell().
//
// Which curve array holds the input side depends on which side of the CLUT the
// matrix sits: 'mft2' keeps the matrix on the input side so B is the input
// array, while 'mAB ' clears m_bInputMatrix so A is. CIccMBB::NewCurvesA and
// NewCurvesB size themselves off IsInputB() accordingly, so filling the wrong
// one writes 4 pointers into a 3-pointer array.
template <class TTag>
TTag *buildCmykToLab(icUInt8Number nGrid)
{
  TTag *pTag = new TTag();
  pTag->Init(4, 3);
  pTag->SetColorSpaces(icSigCmykData, icSigLabData);

  const bool bInputIsB = pTag->IsInputB();

  LPIccCurve *pIn = bInputIsB ? pTag->NewCurvesB() : pTag->NewCurvesA();
  for (int i = 0; i < 4; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pIn[i] = pCurve;
  }

  CIccCLUT *pCLUT = new CIccCLUT(4, 3);
  if (!pCLUT->Init(nGrid)) {
    delete pCLUT;
    delete pTag;
    return NULL;
  }
  icFloatNumber *pData = pCLUT->GetData(0);
  for (icUInt32Number p = 0; p < pCLUT->NumPoints(); p++) {
    // Axis 0 varies slowest, matching CIccCLUT's storage order.
    double in[4];
    icUInt32Number rem = p;
    for (int ax = 3; ax >= 0; ax--) {
      in[ax] = (double)(rem % nGrid) / (double)(nGrid - 1);
      rem /= nGrid;
    }
    cmykToLabCell(in, &pData[(size_t)p * 3]);
  }
  pTag->SetCLUT(pCLUT);

  LPIccCurve *pOut = bInputIsB ? pTag->NewCurvesA() : pTag->NewCurvesB();
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve*)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pOut[i] = pCurve;
  }
  return pTag;
}

// desc/cprt/wtpt are the tags CIccProfile::CheckRequiredTags insists on for any
// non-link class, so without them the profiles below would not survive Begin().
void attachRequiredTags(CIccProfile &profile)
{
  CIccTagTextDescription *pDesc = new CIccTagTextDescription();
  pDesc->SetText("v2 legacy PCS regression");
  profile.AttachTag(icSigProfileDescriptionTag, pDesc);

  CIccTagText *pCprt = new CIccTagText();
  pCprt->SetText("Copyright (C) 2026 The International Color Consortium");
  profile.AttachTag(icSigCopyrightTag, pCprt);

  CIccTagXYZ *pWtpt = new CIccTagXYZ(1);
  (*pWtpt)[0].X = icDtoF((icFloatNumber)0.9642);
  (*pWtpt)[0].Y = icDtoF((icFloatNumber)1.0000);
  (*pWtpt)[0].Z = icDtoF((icFloatNumber)0.8249);
  profile.AttachTag(icSigMediaWhitePointTag, pWtpt);
}

// Group 1 -- the predicate that selects the encoding, per tag class.
void legacyPcsPredicateContract()
{
  CIccTagLut16 lut16;
  check(lut16.UseLegacyPCS(),
        "CIccTagLut16 ('mft2') must report UseLegacyPCS() -- v2 16-bit Lab is 0..0xff00");

  CIccTagNamedColor2 ncl2;
  check(ncl2.UseLegacyPCS(),
        "CIccTagNamedColor2 ('ncl2') must report UseLegacyPCS()");

  // 8-bit v2 Lab is encoded the same way v4 encodes it, so the 0xff00 rescale
  // must not be applied to 'mft1'. This asserts the absence of a plausible
  // over-correction as much as it pins current behaviour.
  CIccTagLut8 lut8;
  check(!lut8.UseLegacyPCS(),
        "CIccTagLut8 ('mft1') must NOT report UseLegacyPCS() -- 8-bit Lab is not rescaled");

  CIccTagLutAtoB lutAtoB;
  check(!lutAtoB.UseLegacyPCS(),
        "CIccTagLutAtoB ('mAB ') must NOT report UseLegacyPCS()");

  CIccTagLutBtoA lutBtoA;
  check(!lutBtoA.UseLegacyPCS(),
        "CIccTagLutBtoA ('mBA ') must NOT report UseLegacyPCS()");
}

// Group 2 -- the conversion math, isolated from any profile or CMM chain.
void legacyEncodingMath()
{
  const icFloatNumber probe[4][3] = {
    {0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f},
    {0.5f, 0.0f, 1.0f},
    {0.25f, 0.75f, 0.125f},
  };

  for (int i = 0; i < 4; i++) {
    icFloatNumber lab4[3], back[3];
    CIccPCSUtil::Lab2ToLab4(lab4, probe[i], true);

    for (int c = 0; c < 3; c++) {
      const double want = (double)probe[i][c] * kLegacyFactor;
      if (std::fabs((double)lab4[c] - want) > 1e-9) {
        check(false, "Lab2ToLab4 did not apply exactly 65535/65280");
        break;
      }
    }

    // The pair is used in both directions inside a single AdjustPCS call, so a
    // one-sided change would corrupt every v2 transform rather than fail loudly.
    CIccPCSUtil::Lab4ToLab2(back, lab4);
    for (int c = 0; c < 3; c++) {
      if (std::fabs((double)back[c] - (double)probe[i][c]) > 1e-6) {
        check(false, "Lab4ToLab2 is not the exact inverse of Lab2ToLab4");
        break;
      }
    }

    // The full legacy leg of AdjustPCS: Lab2 -> XYZ -> Lab2 must be an identity
    // to within float noise, exactly as the v4 leg is.
    icFloatNumber xyz[3], roundTrip[3];
    CIccPCSUtil::Lab2ToXyz(xyz, probe[i], true);
    CIccPCSUtil::XyzToLab2(roundTrip, xyz, true);
    if (std::fabs((double)roundTrip[0] - (double)probe[i][0]) > 1e-5) {
      check(false, "Lab2ToXyz followed by XyzToLab2 did not round-trip L*");
    }
  }
}

// Group 3 -- the same CLUT through a v2 'mft2' and a v4 'mAB ', end to end.
void legacyPathIsObservableThroughTheCmm()
{
  CIccProfile v2Profile;
  v2Profile.InitHeader();
  v2Profile.m_Header.version     = icVersionNumberV2_1;
  v2Profile.m_Header.deviceClass = icSigOutputClass;
  v2Profile.m_Header.colorSpace  = icSigCmykData;
  v2Profile.m_Header.pcs         = icSigLabData;
  CIccTagLut16 *pLut16 = buildCmykToLab<CIccTagLut16>(3);
  if (!pLut16) {
    check(false, "could not build the 'mft2' A2B0 tag");
    return;
  }
  v2Profile.AttachTag(icSigAToB0Tag, pLut16);
  attachRequiredTags(v2Profile);

  CIccProfile v4Profile;
  v4Profile.InitHeader();
  v4Profile.m_Header.version     = icVersionNumberV4_3;
  v4Profile.m_Header.deviceClass = icSigOutputClass;
  v4Profile.m_Header.colorSpace  = icSigCmykData;
  v4Profile.m_Header.pcs         = icSigLabData;
  CIccTagLutAtoB *pLutAtoB = buildCmykToLab<CIccTagLutAtoB>(3);
  if (!pLutAtoB) {
    check(false, "could not build the 'mAB ' A2B0 tag");
    return;
  }
  v4Profile.AttachTag(icSigAToB0Tag, pLutAtoB);
  attachRequiredTags(v4Profile);

  const icFloatNumber src[6][4] = {
    {0.0f, 0.0f, 0.0f, 0.0f},   // paper white -- the discriminating sample
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.25f, 0.5f, 0.75f, 0.1f},
  };

  icFloatNumber out[2][6][3];
  for (int pass = 0; pass < 2; pass++) {
    CIccCmm cmm(icSigUnknownData, icSigUnknownData);
    // AddXform(CIccProfile&) copy-constructs, so the profiles above stay owned
    // here and no temporary file is needed.
    if (cmm.AddXform(pass ? v4Profile : v2Profile, icPerceptual) != icCmmStatOk) {
      check(false, pass ? "AddXform rejected the v4 'mAB ' profile"
                        : "AddXform rejected the v2 'mft2' profile");
      return;
    }
    if (cmm.Begin() != icCmmStatOk) {
      check(false, pass ? "CIccCmm::Begin failed for the v4 'mAB ' profile"
                        : "CIccCmm::Begin failed for the v2 'mft2' profile");
      return;
    }
    if (!pass) {
      // A v2 profile has to reach the CMM as CMYK->Lab at all; if the header or
      // the tag were misread this is where it shows.
      check(cmm.GetSourceSpace() == icSigCmykData,
            "the v2 profile did not present a CMYK source space");
      check(cmm.GetDestSpace() == icSigLabData,
            "the v2 profile did not present a Lab destination space");
    }
    for (int i = 0; i < 6; i++) {
      icFloatNumber dst[16] = {0};
      if (cmm.Apply(dst, src[i]) != icCmmStatOk) {
        check(false, "CIccCmm::Apply failed");
        return;
      }
      for (int c = 0; c < 3; c++)
        out[pass][i][c] = dst[c];
    }
  }

  // The assertions below use samples 0, 2 and 3 only. Those inputs are all 0.0 or
  // 1.0, which land exactly on grid nodes of the 3-points-per-axis CLUT, so the
  // v4 profile returns its CLUT entry with no interpolation error and serves as
  // the unscaled control. (Sample 5 is deliberately off-node; it is applied to
  // exercise interpolation but is not asserted on.)
  //
  // The legacy rescale is not a uniform multiply on the way out: L* is carried
  // through XYZ by Lab2ToXyz/XyzToLab2, and that leg is non-linear away from the
  // reference white. What does scale exactly is a chroma channel sitting at the
  // neutral 0.5, which comes back as 0.5 * 65535/65280 == 0.501953125. Those are
  // the assertions worth making, and they are pinned tight because they are
  // exact rather than approximate.
  //
  // Measured, correct build vs the same test with CIccTagLut16::UseLegacyPCS()
  // forced false:
  //                        legacy         without      expected
  //   paper-white L*     1.003892779    1.000000000    1.00390625
  //   neutral a* at white 0.501952827   0.500006914    0.501953125
  //
  // So against the 1e-4 tolerance: the residual on a correct build is 1.3e-5 for
  // L* (7x inside) and 3.0e-7 for the neutral (330x inside), while a broken build
  // misses by 3.9e-3 (39x outside) and 1.9e-3 (19x outside). L* is the tighter of
  // the two because of CLUT quantization; the neutral channels are the sharp ones.
  // A failure landing between 1e-4 and 1e-3 would be neither of these and is worth
  // investigating rather than retuning.
  const double kNeutral = 0.5 * kLegacyFactor;   // 0.501953125

  check(std::fabs((double)out[1][0][0] - 1.0) < 5e-4,
        "the v4 'mAB ' control did not reproduce L*=1.0 for paper white");
  check(std::fabs((double)out[1][0][1] - 0.5) < 5e-4,
        "the v4 'mAB ' control rescaled a neutral a* it should have left alone");

  check(std::fabs((double)out[0][0][0] - kLegacyFactor) < 1e-4,
        "the v2 'mft2' profile did not expand paper-white L* by 65535/65280 "
        "-- legacy PCS not applied");
  check(std::fabs((double)out[0][0][1] - kNeutral) < 1e-4 &&
        std::fabs((double)out[0][0][2] - kNeutral) < 1e-4,
        "the v2 'mft2' profile did not expand the neutral a*/b* at paper white");

  // Two further neutral channels, so the result does not rest on one sample:
  // sample 2 is CMYK 0,1,0,0 (b* neutral) and sample 3 is 0,0,1,0 (a* neutral).
  check(std::fabs((double)out[0][2][2] - kNeutral) < 1e-4,
        "the v2 'mft2' profile did not expand the neutral b* of sample 2");
  check(std::fabs((double)out[0][3][1] - kNeutral) < 1e-4,
        "the v2 'mft2' profile did not expand the neutral a* of sample 3");
}

} // namespace

int main()
{
  legacyPcsPredicateContract();
  legacyEncodingMath();
  legacyPathIsObservableThroughTheCmm();

  if (g_fail)
    std::fprintf(stderr, "[v2-legacy-pcs] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[v2-legacy-pcs] all assertions passed\n");

  return g_fail;
}
