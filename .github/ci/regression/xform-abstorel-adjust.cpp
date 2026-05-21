/*
    File:       xform-abstorel-adjust.cpp

    Contains:   CTest helper for CIccXform::Create() relative-intent fallback
                through absolute colorimetric AToB3 tags (issue #1662).

    The profile built here has no AToB1 relative tag. A relative-colorimetric
    AddXform() must therefore use AToB3 and set bAbsToRel. The CMM still has to
    return relative PCS values, so CIccXform::Begin() must apply the inverse of
    the normal absolute-intent media-white adjustment.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"

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

static void attach_xyz(CIccProfile& p, icSignature sig, double x, double y, double z)
{
  CIccTagXYZ* t = new CIccTagXYZ;
  (*t)[0].X = icDtoF(x);
  (*t)[0].Y = icDtoF(y);
  (*t)[0].Z = icDtoF(z);
  p.AttachTag(sig, t);
}

static CIccTagLut16* make_const_atob3(float v)
{
  CIccTagLut16* pLut = new CIccTagLut16;
  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve* aCurves = pLut->NewCurvesA();
  LPIccCurve* bCurves = pLut->NewCurvesB();
  if (!aCurves || !bCurves) {
    delete pLut;
    return NULL;
  }
  for (int i = 0; i < 3; i++) {
    CIccTagCurve* ca = new CIccTagCurve();
    ca->SetSize(2, icInitIdentity);
    aCurves[i] = ca;

    CIccTagCurve* cb = new CIccTagCurve();
    cb->SetSize(2, icInitIdentity);
    bCurves[i] = cb;
  }

  icUInt8Number grid[3] = {2, 2, 2};
  if (!pLut->NewCLUT(grid, 2)) {
    delete pLut;
    return NULL;
  }

  CIccCLUT* clut = pLut->GetCLUT();
  icFloatNumber* data = clut->GetData(0);
  const icUInt32Number n = clut->NumPoints() * 3;
  for (icUInt32Number i = 0; i < n; i++)
    data[i] = v;

  return pLut;
}

static void build_abs_only_profile(CIccProfile& p, float lutValue)
{
  p.InitHeader();
  p.m_Header.deviceClass = icSigOutputClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigXYZData;
  p.m_Header.version = icVersionNumberV4_3;

  attach_xyz(p, icSigMediaWhitePointTag, 0.9505, 1.0, 1.0890);
  p.AttachTag(icSigAToB3Tag, make_const_atob3(lutValue));

  for (icSignature s : {icSigProfileDescriptionTag, icSigCopyrightTag}) {
    CIccTagMultiLocalizedUnicode* m = new CIccTagMultiLocalizedUnicode();
    m->SetText("xform-abstorel-adjust test");
    p.AttachTag(static_cast<icSignature>(s), m);
  }
}

static bool apply_relative(CIccProfile& p, icFloatNumber out[3])
{
  CIccCmm cmm(icSigRgbData, icSigXYZData, true);
  if (cmm.AddXform(p, icRelativeColorimetric, icInterpTetrahedral) != icCmmStatOk)
    return false;
  if (cmm.Begin() != icCmmStatOk)
    return false;

  const icFloatNumber in[3] = {0.25f, 0.50f, 0.75f};
  return cmm.Apply(out, in) == icCmmStatOk;
}

static bool close_to(double a, double b)
{
  return std::fabs(a - b) < 0.0005;
}

int main()
{
  const float lutValue = 0.5f;
  CIccProfile p;
  build_abs_only_profile(p, lutValue);

  icFloatNumber out[3] = {0, 0, 0};
  check(apply_relative(p, out), "relative CMM builds through AToB3 fallback");

  const CIccTagXYZ* mediaTag = (const CIccTagXYZ*)p.FindTag(icSigMediaWhitePointTag);
  const double mediaX = icFtoD((*mediaTag)[0].X);
  const double mediaY = icFtoD((*mediaTag)[0].Y);
  const double mediaZ = icFtoD((*mediaTag)[0].Z);
  const double illumX = icFtoD(p.m_Header.illuminant.X);
  const double illumY = icFtoD(p.m_Header.illuminant.Y);
  const double illumZ = icFtoD(p.m_Header.illuminant.Z);

  const double expX = lutValue * illumX / mediaX;
  const double expY = lutValue * illumY / mediaY;
  const double expZ = lutValue * illumZ / mediaZ;
  std::printf("out=(%.6f %.6f %.6f), expected=(%.6f %.6f %.6f)\n",
              out[0], out[1], out[2], expX, expY, expZ);

  check(close_to(out[0], expX), "X uses absolute-to-relative media-white scale");
  check(close_to(out[1], expY), "Y uses absolute-to-relative media-white scale");
  check(close_to(out[2], expZ), "Z uses absolute-to-relative media-white scale");
  check(!close_to(out[2], lutValue), "Z differs from raw AToB3 output");

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
