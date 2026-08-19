/*
    File:       pcs-xyzconvert-matrix-channels.cpp

    Contains:   CTest helper for CIccPcsXform::pushXYZConvert() matrix-element
                bounds checking (issue #2175).

    pushXYZConvert() checks the channel counts of the customToStandardPcc /
    standardToCustomPcc MPE, then reaches into element 0 when the MPE holds a
    single matrix element. Those are two different objects: the container can
    declare 3x3 while the CIccMpeMatrix inside it declares something else.
    CIccMpeMatrix::SetSize allocates inChannels*outChannels matrix entries and
    outChannels constants (IccMpeBasic.cpp), so for a 1x1 element the offset
    test reads pOffset[1..2] and the memcpy copies 9 floats out of a 1-float
    allocation.

    PR #632 added exactly this guard -- but only to the standardToCustomPcc arm.
    The customToStandardPcc arm kept the unguarded read; its one later edit
    (#1081) only made the allocations std::nothrow. That arm is what the AFL PoC
    in #2175 reaches (ASAN: heap-buffer-overflow, READ of size 4, in
    CIccCmm::Begin -> CheckPCSConnections -> Connect -> pushXYZConvert).

    The assertion here does NOT depend on a sanitizer. Before the fix the bogus
    matrix step is built from out-of-bounds data and Begin() reports success;
    after it, the malformed MPE falls through to the CIccPcsStepMpe path, whose
    Begin() rejects it and yields icCmmStatBadConnection. So this is red-green
    in a plain build: pre-fix Begin()==icCmmStatOk, post-fix != icCmmStatOk.

    Both arms are covered: the src profile carries the malformed MPE as its
    customToStandardPcc tag, and a second case moves it to standardToCustomPcc
    so the already-fixed arm stays pinned against a future edit.

    Returns 0 on success; the number of failed assertions otherwise.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccMpeBasic.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"

#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char *msg)
{
  if (cond) {
    std::printf("ok:   %s\n", msg);
  }
  else {
    std::printf("FAIL: %s\n", msg);
    ++g_failures;
  }
}

static void attach_xyz(CIccProfile &p, icSignature sig, double x, double y, double z)
{
  CIccTagXYZ *t = new CIccTagXYZ;
  (*t)[0].X = icDtoF(x);
  (*t)[0].Y = icDtoF(y);
  (*t)[0].Z = icDtoF(z);
  p.AttachTag(sig, t);
}

// A 3-in/3-out identity LUT so CIccCmm::AddXform has a transform to use.
static CIccTagLut16 *make_identity_lut(icColorSpaceSignature src, icColorSpaceSignature dst)
{
  CIccTagLut16 *pLut = new CIccTagLut16;
  pLut->Init(3, 3);

  // Fill the curves BEFORE SetColorSpaces. For an XYZ input it moves the B
  // curves to M and installs its own zero-size B curves (IccTagLut.cpp:5745),
  // so calling it first and then assigning over NewCurvesB() would overwrite
  // -- and leak -- the ones it just created.
  LPIccCurve *aCurves = pLut->NewCurvesA();
  LPIccCurve *bCurves = pLut->NewCurvesB();
  if (!aCurves || !bCurves) {
    delete pLut;
    return NULL;
  }
  for (int i = 0; i < 3; i++) {
    CIccTagCurve *ca = new CIccTagCurve();
    ca->SetSize(2, icInitIdentity);
    aCurves[i] = ca;

    CIccTagCurve *cb = new CIccTagCurve();
    cb->SetSize(2, icInitIdentity);
    bCurves[i] = cb;
  }

  pLut->SetColorSpaces(src, dst);

  icUInt8Number grid[3] = {2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) {
    delete pLut;
    return NULL;
  }

  CIccCLUT *clut = pLut->GetCLUT();
  icFloatNumber *data = clut->GetData(0);
  const icUInt32Number n = clut->NumPoints() * 3;
  for (icUInt32Number i = 0; i < n; i++)
    data[i] = 0.5f;

  return pLut;
}

// The MPE always declares 3x3, so pushXYZConvert's own container check passes.
// nElemChannels sets what the single matrix element inside it declares:
//   3 -> well formed, and the control case for this test
//   1 -> the #2175 shape; GetMatrix() owns 1 float, GetConstants() owns 1 float
static CIccTagMultiProcessElement *make_single_matrix_mpe(icUInt16Number nElemChannels)
{
  CIccTagMultiProcessElement *pMpe = new CIccTagMultiProcessElement(3, 3);

  CIccMpeMatrix *pMtx = new CIccMpeMatrix();
  pMtx->SetSize(nElemChannels, nElemChannels, true);

  // Identity down the diagonal, zero constants -- the shape pushXYZConvert
  // wants to promote to a CIccPcsStepMatrix.
  icFloatNumber *pMat = pMtx->GetMatrix();
  if (pMat) {
    for (icUInt16Number i = 0; i < nElemChannels; i++)
      pMat[i * nElemChannels + i] = 1.0f;
  }

  pMpe->Attach(pMtx);
  return pMpe;
}

// Non-D50 viewing conditions, which is what makes isStandardPcc() false and so
// routes the CMM into the pushXYZConvert arms at all.
static CIccTagSpectralViewingConditions *make_d65_viewing_conditions()
{
  CIccTagSpectralViewingConditions *pSvc = new CIccTagSpectralViewingConditions();

  icSpectralRange range;
  range.start = icFtoF16(400.0f);
  range.end   = icFtoF16(700.0f);
  range.steps = 4;
  const icFloatNumber spd[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  pSvc->setIlluminant(icIlluminantD65, range, spd, 6504.0f);

  return pSvc;
}

static void attach_description(CIccProfile &p, const char *text)
{
  for (icSignature s : {icSigProfileDescriptionTag, icSigCopyrightTag}) {
    CIccTagMultiLocalizedUnicode *m = new CIccTagMultiLocalizedUnicode();
    m->SetText(text);
    p.AttachTag(static_cast<icSignature>(s), m);
  }
}

// One builder for both ends of the chain. When mpeTagSig is 0 the profile is
// left with standard (D50/1931) connection conditions; otherwise it gets D65
// viewing conditions -- which is what makes isStandardPcc() false and routes
// pushXYZConvert into the arm that reads the named tag -- plus the MPE itself.
//
// Which end carries it decides which arm runs: pushXYZConvert takes the
// customToStandardPcc branch for a non-standard SOURCE and the
// standardToCustomPcc branch for a non-standard DESTINATION.
static void build_profile(CIccProfile &p, double wpX, double wpY, double wpZ,
                          icSignature mpeTagSig, icUInt16Number nElemChannels)
{
  p.InitHeader();
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace  = icSigRgbData;
  p.m_Header.pcs         = icSigXYZData;
  p.m_Header.version     = icVersionNumberV4_3;

  attach_xyz(p, icSigMediaWhitePointTag, wpX, wpY, wpZ);
  p.AttachTag(icSigAToB1Tag, make_identity_lut(icSigRgbData, icSigXYZData));
  p.AttachTag(icSigBToA1Tag, make_identity_lut(icSigXYZData, icSigRgbData));

  if (mpeTagSig) {
    p.AttachTag(icSigSpectralViewingConditionsTag, make_d65_viewing_conditions());
    p.AttachTag(mpeTagSig, make_single_matrix_mpe(nElemChannels));
  }

  attach_description(p, "pcs-xyzconvert-matrix-channels");
}

// Drives CIccCmm::Begin(), which is the call chain the PoC crashes through:
// Begin -> CheckPCSConnections -> CIccPcsXform::Connect -> pushXYZConvert.
static icStatusCMM begin_chain(icSignature mpeTagSig, icUInt16Number nElemChannels)
{
  const bool bOnSource = (mpeTagSig == icSigCustomToStandardPccTag);

  CIccProfile *pSrc = new CIccProfile();
  CIccProfile *pDst = new CIccProfile();
  build_profile(*pSrc, 0.9505, 1.0, 1.0890,
                bOnSource ? mpeTagSig : (icSignature)0, nElemChannels);
  build_profile(*pDst, 0.9642, 1.0, 0.8249,
                bOnSource ? (icSignature)0 : mpeTagSig, nElemChannels);

  // AddXform "takes ownership of the profile, or deletes the profile on error"
  // (IccCmm.cpp:9219), so a profile handed to it must never be deleted here --
  // only the one that was not passed yet.
  CIccCmm cmm(icSigRgbData, icSigRgbData, true);
  if (cmm.AddXform(pSrc, icRelativeColorimetric, icInterpTetrahedral) != icCmmStatOk) {
    delete pDst;
    return icCmmStatBadXform;
  }
  if (cmm.AddXform(pDst, icRelativeColorimetric, icInterpTetrahedral) != icCmmStatOk) {
    return icCmmStatBadXform;
  }

  return cmm.Begin();
}

int main()
{
  // Anti-vacuity controls. A well-formed 3x3 element must build the chain, which
  // is what proves the malformed cases below actually reach pushXYZConvert and
  // are refused there rather than dying earlier for some unrelated reason. These
  // fail if the profiles ever stop being usable, instead of the test quietly
  // passing on a chain that never got that far.
  const icStatusCMM c2sOk = begin_chain(icSigCustomToStandardPccTag, 3);
  std::printf("control customToStandardPcc 3x3: Begin() = %d\n", (int)c2sOk);
  check(c2sOk == icCmmStatOk,
        "control: a well-formed 3x3 matrix element builds the PCS chain");

  const icStatusCMM s2cOk = begin_chain(icSigStandardToCustomPccTag, 3);
  std::printf("control standardToCustomPcc 3x3: Begin() = %d\n", (int)s2cOk);
  check(s2cOk == icCmmStatOk,
        "control: a well-formed 3x3 matrix element builds the PCS chain (dst arm)");

  // customToStandardPcc -- the arm PR #632 missed, and the one #2175 reaches.
  const icStatusCMM c2s = begin_chain(icSigCustomToStandardPccTag, 1);
  std::printf("customToStandardPcc 1x1: Begin() = %d\n", (int)c2s);
  check(c2s != icCmmStatOk,
        "customToStandardPcc: a 1x1 matrix element in a 3x3 MPE is refused, not read out of bounds");

  // standardToCustomPcc -- already guarded by #632; pinned so it stays that way.
  const icStatusCMM s2c = begin_chain(icSigStandardToCustomPccTag, 1);
  std::printf("standardToCustomPcc 1x1: Begin() = %d\n", (int)s2c);
  check(s2c != icCmmStatOk,
        "standardToCustomPcc: a 1x1 matrix element in a 3x3 MPE is refused, not read out of bounds");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);

  return g_failures;
}
