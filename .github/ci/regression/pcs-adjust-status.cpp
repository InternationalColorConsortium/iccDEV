// Regression for #2176: a failed PCS adjustment must be reported as
// icCmmStatCantAdjustPcs, not as icCmmStatIncorrectApply.
//
// CIccXform::Begin() asks its IIccAdjustPCSXform hint for the PCS scale and offset
// factors (IccProfLib/IccCmm.cpp, the m_pAdjustPCS block). CalcFactors() returns
// bool, so all 26 of its failure paths arrive there as a bare false and the site
// used to turn every one of them into icCmmStatIncorrectApply -- "Incorrect Apply
// object". That is false on all 26: the caller's apply object is well formed in
// every one of them, and what failed is the adjustment. The other 33
// icCmmStatIncorrectApply returns in IccCmm.cpp all concern a missing apply object
// or an unusable apply interface, so this one site was the lone outlier and a
// caller had no way to tell the two conditions apart -- #1748 had to be
// reconstructed from source because the status said nothing about the real cause.
//
// Bool is also the ceiling on how much this can say. Distinguishing "black point
// compensation does not apply here" from "the black point estimate failed" would
// mean widening the exported IIccAdjustPCSXform interface, which is a separate
// decision; one code that is true of all 26 paths is what this site can report
// without touching that interface.
//
// The name is about the interface rather than about black point compensation.
// CIccApplyBPC is the only in-tree implementer, but IIccAdjustPCSXform is exported
// and is reached through a hint, so an out-of-tree implementer need not be doing
// black point compensation at all.
//
// WHAT THIS DRIVES. Two failures with unrelated causes, plus a control that must
// still succeed:
//
//   1. icAbsoluteColorimetric -- CalcFactors() rejects the intent outright before
//      it estimates anything (IccApplyBPC.cpp, "black point compensation not
//      supported").
//   2. a BToA0 whose CLUT is constant -- CalcFactors() gets all the way into
//      calcDstBlackPoint(), runs the black transform, and finds MaxL <= MinL. A
//      genuine estimation failure, reached by a different route entirely.
//   3. the control -- the same fixture with a well-formed BToA0 and a perceptual
//      intent, where CalcFactors() succeeds and Begin() returns icCmmStatOk.
//
// Cases 1 and 2 are the point: before the change they were indistinguishable from
// each other AND from a genuinely malformed apply object. The control is what
// keeps the fix from being "return an error more often".
//
// The fixture is v2 on purpose. IsVersion2() is what turns on the perceptual
// black-point adjustment, so a v2 profile is what puts an IIccAdjustPCSXform in
// the chain for a perceptual intent at all.
//
// Red-green: restoring the return at the m_pAdjustPCS block to
// icCmmStatIncorrectApply fails cases 1 and 2 (both the positive and the negative
// assertion). Removing the GetStatusText case fails the decode assertion.
// Renumbering the enum fails the append-only assertions.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccApplyBPC.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[pcs-adjust-status] FAIL: %s\n", what);
  }
}

// --- fixture ---------------------------------------------------------------
//
// Built in code rather than read from Testing/: every .icc under Testing/ is
// generated at build time, and case 2 needs a deliberately degenerate BToA0 that
// no generated corpus profile carries.

typedef void (*CellFill)(const double *in, icFloatNumber *out);

// A plausible CMYK -> Lab: L* falls with ink, a* and b* offset-encoded around the
// mid point. Monotone in K, which is what gives calcDstBlackPoint() a MaxL above
// its MinL in the control.
void cmykToLabCell(const double *in, icFloatNumber *out)
{
  const double k = in[3];
  const double lum = (1.0 - k) * (1.0 - 0.30 * in[0] - 0.20 * in[1] - 0.10 * in[2]);
  out[0] = (icFloatNumber)(lum < 0.0 ? 0.0 : lum);
  out[1] = (icFloatNumber)(0.5 + 0.25 * (in[0] - in[1]));
  out[2] = (icFloatNumber)(0.5 + 0.25 * (in[1] - in[2]));
}

void labToCmykCell(const double *in, icFloatNumber *out)
{
  const double l = in[0];
  out[0] = (icFloatNumber)(0.5 * (1.0 - l));
  out[1] = (icFloatNumber)(0.5 * (1.0 - l));
  out[2] = (icFloatNumber)(0.5 * (1.0 - l));
  out[3] = (icFloatNumber)(1.0 - l);
}

// Case 2's degenerate BToA0. Every grid point maps to the same CMYK, so the
// Lab -> CMYK -> Lab round trip in calcDstBlackPoint() returns one constant L*
// for both its L*=0 and L*=100 probes and the MaxL <= MinL guard fires. The
// profile is otherwise entirely well formed -- the point is that the failure is
// an estimation failure and not a malformed apply object.
void constantCmykCell(const double * /*in*/, icFloatNumber *out)
{
  out[0] = out[1] = out[2] = out[3] = (icFloatNumber)0.5;
}

// nIn/nOut are taken at the width CIccMBB::Init() stores them at. Declaring them
// wider and narrowing at the call would let a future caller pass >255: Init()
// would see the truncated count and size the curve arrays from it, while the
// fill loops below still ran to the un-narrowed value and wrote past the end.
CIccTagLut16 *buildLut(icUInt8Number nGrid, icUInt8Number nIn, icUInt8Number nOut,
                       icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace,
                       CellFill pFill)
{
  CIccTagLut16 *pTag = new CIccTagLut16();
  pTag->Init(nIn, nOut);
  pTag->SetColorSpaces(srcSpace, dstSpace);

  const bool bInputIsB = pTag->IsInputB();

  LPIccCurve *pIn = bInputIsB ? pTag->NewCurvesB() : pTag->NewCurvesA();
  for (icUInt32Number i = 0; i < nIn; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve *)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pIn[i] = pCurve;
  }

  CIccCLUT *pCLUT = new CIccCLUT(nIn, nOut);
  if (!pCLUT->Init(nGrid)) {
    delete pCLUT;
    delete pTag;
    return NULL;
  }
  icFloatNumber *pData = pCLUT->GetData(0);
  for (icUInt32Number p = 0; p < pCLUT->NumPoints(); p++) {
    double in[4] = { 0.0, 0.0, 0.0, 0.0 };
    icUInt32Number rem = p;
    for (int ax = (int)nIn - 1; ax >= 0; ax--) {   // axis 0 varies slowest
      in[ax] = (double)(rem % nGrid) / (double)(nGrid - 1);
      rem /= nGrid;
    }
    pFill(in, &pData[(size_t)p * nOut]);
  }
  pTag->SetCLUT(pCLUT);

  LPIccCurve *pOut = bInputIsB ? pTag->NewCurvesA() : pTag->NewCurvesB();
  for (icUInt32Number i = 0; i < nOut; i++) {
    CIccTagCurve *pCurve = (CIccTagCurve *)CIccTag::Create(icSigCurveType);
    pCurve->SetSize(2);
    (*pCurve)[0] = 0.0f;
    (*pCurve)[1] = 1.0f;
    pOut[i] = pCurve;
  }
  return pTag;
}

// A media white that is deliberately not D50: the absolute-intent path in
// CIccXform::Begin() runs on (mediaWhite != illuminant), and a D50 white would
// make case 1 take a different route to Begin() than the one under test.
void attachRequiredTags(CIccProfile &profile)
{
  CIccTagTextDescription *pDesc = new CIccTagTextDescription();
  pDesc->SetText("issue 2176 pcs adjust status fixture");
  profile.AttachTag(icSigProfileDescriptionTag, pDesc);

  CIccTagText *pCprt = new CIccTagText();
  pCprt->SetText("Copyright (C) 2026 The International Color Consortium");
  profile.AttachTag(icSigCopyrightTag, pCprt);

  CIccTagXYZ *pWtpt = new CIccTagXYZ(1);
  (*pWtpt)[0].X = icDtoF((icFloatNumber)0.9000);
  (*pWtpt)[0].Y = icDtoF((icFloatNumber)0.9500);
  (*pWtpt)[0].Z = icDtoF((icFloatNumber)0.7500);
  profile.AttachTag(icSigMediaWhitePointTag, pWtpt);
}

bool attachLut(CIccProfile &p, icTagSignature sig, CIccTagLut16 *pTag)
{
  if (!pTag)
    return false;
  p.AttachTag(sig, pTag);
  return true;
}

// bToAFill selects the control fixture from case 2's degenerate one; everything
// else about the two profiles is identical, so the only thing that can account
// for a difference in status is the black-point estimate.
CIccProfile *buildV2CmykOutputProfile(CellFill bToAFill)
{
  CIccProfile *p = new CIccProfile;
  p->InitHeader();
  p->m_Header.version     = icVersionNumberV2_1;
  p->m_Header.deviceClass = icSigOutputClass;
  p->m_Header.colorSpace  = icSigCmykData;
  p->m_Header.pcs         = icSigLabData;

  // buildLut() returns NULL only when CIccCLUT::Init() fails, which the four
  // fixed shapes below cannot provoke. Routed through attachLut() anyway because
  // CIccProfile::AttachTag() calls SetParentObject() on its argument with no NULL
  // test, so a build failure would segfault the fixture rather than report one --
  // the same reason the sibling pcs-adjust-placement.cpp checks its SetSize().
  bool bBuilt = true;
  bBuilt &= attachLut(*p, icSigAToB0Tag, buildLut(5, 4, 3, icSigCmykData, icSigLabData, cmykToLabCell));
  bBuilt &= attachLut(*p, icSigAToB1Tag, buildLut(5, 4, 3, icSigCmykData, icSigLabData, cmykToLabCell));
  bBuilt &= attachLut(*p, icSigBToA0Tag, buildLut(9, 3, 4, icSigLabData, icSigCmykData, bToAFill));
  bBuilt &= attachLut(*p, icSigBToA1Tag, buildLut(9, 3, 4, icSigLabData, icSigCmykData, labToCmykCell));
  check(bBuilt, "fixture: all four LUT tags built");

  attachRequiredTags(*p);
  return p;
}

// One xform carrying the BPC hint, begun. The hint is what puts an
// IIccAdjustPCSXform in the chain; without it the m_pAdjustPCS block never runs
// and none of this is reachable.
//
// The direction matters and is the whole reason it is a parameter.
// CIccApplyBPC::calcBlackPoint() branches on CIccXform::IsInput(): a source
// profile goes to calcSrcBlackPoint(), which reads AToB0 and never looks at
// BToA0 at all. Only a destination profile reaches calcDstBlackPoint(), which is
// where the black-point estimation -- and case 2's MaxL <= MinL guard -- lives.
icStatusCMM beginWithBpc(CellFill bToAFill, icRenderingIntent nIntent, bool bAsSource)
{
  CIccProfile *pICC = buildV2CmykOutputProfile(bToAFill);

  CIccCreateXformHintManager hints;
  hints.AddHint(new CIccApplyBPCHint());

  CIccCmm cmm(bAsSource ? icSigCmykData : icSigLabData,
              bAsSource ? icSigLabData  : icSigCmykData,
              bAsSource);

  // AddXform's own contract: "takes ownership of the profile, or deletes the
  // profile on error" (IccProfLib/IccCmm.cpp). Every failure return in this
  // overload either calls delete pProfile itself or returns icCmmStatBadXform
  // after CIccXform::Create has already deleted it; the only delete-free return
  // is the !pProfile guard, which never owned anything. So the caller must not
  // free pICC on failure -- doing so is a double free, not a leak fix.
  icStatusCMM stat = cmm.AddXform(pICC, nIntent, icInterpTetrahedral, NULL,
                                  icXformLutColor, true, &hints);
  if (stat != icCmmStatOk)
    return stat;

  return cmm.Begin();
}

} // namespace

int main()
{
  // --- case 1: an intent CalcFactors() refuses outright ------------------------

  {
    const icStatusCMM stat = beginWithBpc(labToCmykCell, icAbsoluteColorimetric, true);

    check(stat == icCmmStatCantAdjustPcs,
          "absolute intent with a BPC hint should report that the PCS adjustment could not be calculated");
    check(stat != icCmmStatIncorrectApply,
          "absolute intent with a BPC hint must no longer be reported as an incorrect apply object (#2176)");
  }

  // --- case 2: a black-point estimate that genuinely fails ---------------------
  // Different cause, and a different depth in CalcFactors(): case 1 is refused
  // before anything is estimated, this one runs the black transform and fails on
  // the result. As a destination profile, so calcDstBlackPoint() is what runs.

  {
    const icStatusCMM stat = beginWithBpc(constantCmykCell, icPerceptual, false);

    check(stat == icCmmStatCantAdjustPcs,
          "a degenerate BToA0 should report that the PCS adjustment could not be calculated");
    check(stat != icCmmStatIncorrectApply,
          "a degenerate BToA0 must no longer be reported as an incorrect apply object (#2176)");
  }

  // --- case 3: the controls ------------------------------------------------------
  // Without these the assertions above are satisfied by any change that simply makes
  // the adjustment fail more often. One control per direction, because cases 1 and 2
  // run through different halves of calcBlackPoint(): the destination control is what
  // shows case 2's failure is the degenerate CLUT and not the direction itself.

  {
    const icStatusCMM stat = beginWithBpc(labToCmykCell, icPerceptual, true);

    check(stat == icCmmStatOk,
          "a well-formed v2 fixture used as a source at perceptual intent should still begin cleanly");
    check(stat != icCmmStatCantAdjustPcs,
          "a successful source-side PCS adjustment must not report the new failure status");
  }

  {
    const icStatusCMM stat = beginWithBpc(labToCmykCell, icPerceptual, false);

    check(stat == icCmmStatOk,
          "a well-formed v2 fixture used as a destination at perceptual intent should still begin cleanly");
    check(stat != icCmmStatCantAdjustPcs,
          "a successful destination-side PCS adjustment must not report the new failure status");
  }

  // --- the status decodes to text ------------------------------------------------
  // icStatusCMM's declaration in IccCmm.h asks for GetStatusText to be kept in sync,
  // and that note is there because it has been missed before -- icCmmStatUnsupported
  // was added without a case and fell through to the default. The text is what the
  // CLI tools print, so an undecoded status would leave the user-visible message no
  // better than the "Incorrect Apply object" this change set out to replace.

  {
    const char *szNew = CIccCmm::GetStatusText(icCmmStatCantAdjustPcs);
    const char *szOld = CIccCmm::GetStatusText(icCmmStatIncorrectApply);

    // One past the last enumerator, and it has to stay there. icStatusCMM is an
    // unscoped enum with no fixed underlying type, so its values span only the
    // smallest bit-field holding -1..20 (a signed 6-bit field, i.e. -32..31);
    // loading anything outside that is undefined behaviour, which UBSan's enum
    // check flags at the switch in GetStatusText.
    const char *szUnknown = CIccCmm::GetStatusText((icStatusCMM)21);

    check(szNew != NULL && strcmp(szNew, szUnknown) != 0,
          "icCmmStatCantAdjustPcs should decode to text rather than fall through to the default");
    check(szOld != NULL && strcmp(szOld, "Incorrect Apply object") == 0,
          "icCmmStatIncorrectApply keeps its own text -- the other 33 sites still report it");
    check(szNew != NULL && szOld != NULL && strcmp(szNew, szOld) != 0,
          "the PCS-adjustment text must differ from the incorrect-apply text (#2176)");
  }

  // --- the enum stays append-only --------------------------------------------------
  // icStatusCMM values cross the IccProfLib ABI and appear in saved QA output, so a
  // new status has to be appended rather than inserted. Pinning the last previously
  // defined value alongside the new one catches a renumbering that would silently
  // change what every existing status means.

  check((int)icCmmStatIncorrectApply == 8,
        "icCmmStatIncorrectApply must keep its value -- icStatusCMM is append-only");
  check((int)icCmmStatUnsupportedProfileClass == 19,
        "icCmmStatUnsupportedProfileClass must keep its value -- icStatusCMM is append-only");
  check((int)icCmmStatCantAdjustPcs == 20,
        "icCmmStatCantAdjustPcs must be appended after icCmmStatUnsupportedProfileClass");

  if (g_fail)
    std::fprintf(stderr, "[pcs-adjust-status] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[pcs-adjust-status] all assertions passed\n");

  return g_fail;
}
