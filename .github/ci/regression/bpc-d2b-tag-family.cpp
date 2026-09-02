/*
    File:       bpc-d2b-tag-family.cpp

    Contains:   CTest helper pinning that CIccApplyBPC estimates the black
                point through the same tag family the xform being adjusted was
                built from.

    CIccXform::Create() picks between the AToBx/BToAx colorimetric tags and the
    DToBx/BToDx MPE tags, and a caller can force the colorimetric ones by
    passing bUseD2BxB2DxTags=false to AddXform().  CIccApplyBPC rebuilds its own
    cmm objects to find black, and it used to hardcode that choice as
    "version >= V5 ? colorimetric : MPE" instead of asking the xform.  For a v4
    profile carrying both families that is backwards: the transform applies
    BToAx while black is measured through BToDx, so the scale and offset BPC
    installs come from a pipeline that is never applied.

    Only the B-side is affected.  Create()'s input branch gates its DToBx lookup
    on spectralPCS or version >= V5, so a plain v4 profile resolves device->PCS
    through AToBx whatever the flag says; the output branch has no such gate and
    follows the flag alone.

    The fixture below is a v4.3 CMYK output profile whose BToA0 and BToD0 tags
    return deliberately different inks, and whose AToB0 turns those inks into
    different L* values.  Applying a pixel with BPC enabled must therefore
    depend on bUseD2BxB2DxTags.  Before the fix both settings produced identical
    output because the flag never reached CIccApplyBPC.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccApplyBPC.h"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

// The two B-side tags hand back these cyan values.  The AToB0 ramp below turns
// them into L* 40 and L* 5, so both survive calcSrcBlackPoint()'s L*<=50 clip
// as distinct numbers -- a pair that both clipped would make the test toothless.
static const icFloatNumber kCyanFromBToA0 = 0.60f;
static const icFloatNumber kCyanFromBToD0 = 0.95f;

static void fill_identity_curves(LPIccCurve* curves, int n)
{
  for (int i = 0; i < n; i++) {
    CIccTagCurve* c = new CIccTagCurve();
    c->SetSize(2, icInitIdentity);
    curves[i] = c;
  }
}

// CIccTagLut16 keeps the matrix on the input side of the CLUT, so NewCurvesB()
// sizes to the input channels and NewCurvesA() to the output channels.
static bool fill_mbb_curves(CIccTagLut16* pLut, int nIn, int nOut)
{
  LPIccCurve* aCurves = pLut->NewCurvesA();
  LPIccCurve* bCurves = pLut->NewCurvesB();
  if (!aCurves || !bCurves)
    return false;

  fill_identity_curves(bCurves, nIn);
  fill_identity_curves(aCurves, nOut);
  return true;
}

// CMYK -> Lab.  L* falls from 100 at cyan 0 to 0 at cyan 1, so a black point
// measured through a B-side tag returning more cyan lands at a lower L*.
// a* and b* are held at the 0 encoding.
static CIccTagLut16* make_atob0()
{
  CIccTagLut16* pLut = new CIccTagLut16;
  pLut->Init(4, 3);
  pLut->SetColorSpaces(icSigCmykData, icSigLabData);

  if (!fill_mbb_curves(pLut, 4, 3)) {
    delete pLut;
    return NULL;
  }

  icUInt8Number grid[4] = {2, 2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) {
    delete pLut;
    return NULL;
  }

  CIccCLUT* clut = pLut->GetCLUT();
  icFloatNumber* data = clut->GetData(0);
  const icUInt32Number nNodes = clut->NumPoints();
  for (icUInt32Number n = 0; n < nNodes; n++) {
    // The first channel varies slowest over the 2x2x2x2 grid.
    const icFloatNumber cyan = (n & 0x8) ? 1.0f : 0.0f;
    data[n * 3 + 0] = 1.0f - cyan;          // L* encoded 0..1 for 0..100
    data[n * 3 + 1] = 128.0f / 255.0f;      // a* == 0
    data[n * 3 + 2] = 128.0f / 255.0f;      // b* == 0
  }

  return pLut;
}

// Lab -> CMYK returning a constant ink, for the legacy BToA0 tag.
static CIccTagLut16* make_btoa0(icFloatNumber cyan)
{
  CIccTagLut16* pLut = new CIccTagLut16;
  pLut->Init(3, 4);
  pLut->SetColorSpaces(icSigLabData, icSigCmykData);

  if (!fill_mbb_curves(pLut, 3, 4)) {
    delete pLut;
    return NULL;
  }

  icUInt8Number grid[3] = {2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) {
    delete pLut;
    return NULL;
  }

  CIccCLUT* clut = pLut->GetCLUT();
  icFloatNumber* data = clut->GetData(0);
  const icUInt32Number nNodes = clut->NumPoints();
  for (icUInt32Number n = 0; n < nNodes; n++) {
    data[n * 4 + 0] = cyan;
    data[n * 4 + 1] = 0.0f;
    data[n * 4 + 2] = 0.0f;
    data[n * 4 + 3] = 0.0f;
  }

  return pLut;
}

// Lab -> CMYK as an MPE tag, returning a different constant ink.  A 3->4 matrix
// with zero coefficients and a constant column is the smallest element that
// changes the channel count and ignores its input.
static CIccTagMultiProcessElement* make_btod0(icFloatNumber cyan)
{
  CIccTagMultiProcessElement* pTag = new CIccTagMultiProcessElement;
  pTag->SetChannels(3, 4);

  CIccMpeMatrix* pMtx = new CIccMpeMatrix;
  if (!pMtx->SetSize(3, 4, true)) {
    delete pMtx;
    delete pTag;
    return NULL;
  }

  icFloatNumber* m = pMtx->GetMatrix();
  for (int i = 0; i < 3 * 4; i++)
    m[i] = 0.0f;

  icFloatNumber* k = pMtx->GetConstants();
  k[0] = cyan;
  k[1] = 0.0f;
  k[2] = 0.0f;
  k[3] = 0.0f;

  pTag->Attach(pMtx);
  return pTag;
}

static void add_text(CIccProfile& p, icSignature sig, const char* text)
{
  CIccTagMultiLocalizedUnicode* m = new CIccTagMultiLocalizedUnicode();
  m->SetText(text);
  p.AttachTag(sig, m);
}

// v4.3 CMYK printer profile carrying both B-side families.  bWithBToD0 drops
// the MPE tag so the test can compute a colorimetric-only reference.
static bool build_profile(CIccProfile& p, bool bWithBToD0)
{
  p.InitHeader();
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace = icSigCmykData;
  p.m_Header.pcs = icSigLabData;
  p.m_Header.version = icVersionNumberV4_3;

  CIccTagXYZ* wtpt = new CIccTagXYZ;
  (*wtpt)[0].X = icDtoF(0.9642);
  (*wtpt)[0].Y = icDtoF(1.0);
  (*wtpt)[0].Z = icDtoF(0.8249);
  p.AttachTag(icSigMediaWhitePointTag, wtpt);

  CIccTagLut16* pAToB0 = make_atob0();
  CIccTagLut16* pBToA0 = make_btoa0(kCyanFromBToA0);
  if (!pAToB0 || !pBToA0) {
    delete pAToB0;
    delete pBToA0;
    return false;
  }
  p.AttachTag(icSigAToB0Tag, pAToB0);
  p.AttachTag(icSigBToA0Tag, pBToA0);

  if (bWithBToD0) {
    CIccTagMultiProcessElement* pBToD0 = make_btod0(kCyanFromBToD0);
    if (!pBToD0)
      return false;
    p.AttachTag(icSigBToD0Tag, pBToD0);
  }

  add_text(p, icSigProfileDescriptionTag, "bpc-d2b-tag-family test");
  add_text(p, icSigCopyrightTag, "bpc-d2b-tag-family test");
  return true;
}

// Runs one CMYK pixel through the profile used as a source, with BPC enabled.
static bool apply_with_bpc(bool bWithBToD0, bool bUseD2BxB2DxTags, icFloatNumber out[3])
{
  CIccProfile* pICC = new CIccProfile;
  if (!build_profile(*pICC, bWithBToD0)) {
    delete pICC;
    return false;
  }

  CIccCreateXformHintManager hint;
  hint.AddHint(new CIccApplyBPCHint());

  CIccCmm cmm(icSigCmykData, icSigLabData, true);
  if (cmm.AddXform(pICC, icPerceptual, icInterpTetrahedral, NULL,
                   icXformLutColor, bUseD2BxB2DxTags, &hint) != icCmmStatOk)
    return false;
  if (cmm.Begin() != icCmmStatOk)
    return false;

  const icFloatNumber in[4] = {0.5f, 0.0f, 0.0f, 0.0f};
  return cmm.Apply(out, in) == icCmmStatOk;
}

static bool same(const icFloatNumber a[3], const icFloatNumber b[3])
{
  for (int i = 0; i < 3; i++) {
    if (std::fabs(a[i] - b[i]) > 1e-5)
      return false;
  }
  return true;
}

static void show(const char* label, const icFloatNumber v[3])
{
  std::printf("      %-28s (%.6f %.6f %.6f)\n", label, v[0], v[1], v[2]);
}

int main()
{
  // 1. The xform must record which family Create() resolved it through.
  for (int i = 0; i < 2; i++) {
    const bool bUseD2B = (i != 0);
    CIccProfile* pICC = new CIccProfile;
    if (!build_profile(*pICC, true)) {
      delete pICC;
      check(false, "fixture profile builds");
      return 1;
    }
    // bInput false: the B-side direction, where the flag selects the family.
    CIccXform* pXform = CIccXform::Create(pICC, false, icPerceptual,
                                          icInterpTetrahedral, NULL,
                                          icXformLutColor, bUseD2B);
    check(pXform != NULL, bUseD2B ? "B-side xform builds with MPE tags allowed"
                                  : "B-side xform builds with MPE tags refused");
    if (pXform) {
      check(pXform->UseD2BTags() == bUseD2B,
            bUseD2B ? "UseD2BTags() reports the MPE family"
                    : "UseD2BTags() reports the colorimetric family");
      delete pXform;
    }
  }

  // 2. With both families present, BPC output must follow the flag.
  icFloatNumber withMpe[3] = {0, 0, 0};
  icFloatNumber withoutMpe[3] = {0, 0, 0};
  icFloatNumber colorimetricOnly[3] = {0, 0, 0};

  check(apply_with_bpc(true, true, withMpe),
        "BPC applies with MPE tags allowed");
  check(apply_with_bpc(true, false, withoutMpe),
        "BPC applies with MPE tags refused");
  // Same profile minus BToD0: the only black available is the colorimetric one.
  check(apply_with_bpc(false, true, colorimetricOnly),
        "BPC applies on the colorimetric-only reference profile");

  show("BToD0 present, flag true:", withMpe);
  show("BToD0 present, flag false:", withoutMpe);
  show("BToD0 absent (reference):", colorimetricOnly);

  check(!same(withMpe, withoutMpe),
        "bUseD2BxB2DxTags must change the BPC result when both families exist");
  check(same(withoutMpe, colorimetricOnly),
        "refusing the MPE tags must measure black through BToA0");

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
