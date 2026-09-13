/*
    File:       parametric-curve-negative-domain.cpp

    Contains:   CTest helper for CIccTagParametricCurve::Apply() given a value
                that puts a power function's base below zero.

    Every parametricCurveType function raises a base to a real exponent: X
    itself for type 0, and aX + b for types 1 to 4.  pow() of a negative base
    and a non-integer exponent is NaN, and nothing stopped one reaching it -
    type 0 has no domain test at all, and the tests types 1 to 4 do have
    compare X against a threshold rather than the base against zero, so a
    negative a, or a d below the base's own zero crossing, still lets a
    negative base through.  The NaN then goes through the matrix into the PCS
    and out to the destination pixel.

    This is reachable through the public API without any float encoding
    change: CIccCmm::Apply() hands device values to the input curves
    unclipped (CIccXformMatrixTRC::Apply), so a caller passing -2.0 to a
    gamma 2.2 matrix/TRC profile gets NaN.  The HDR branch widened the reach -
    CIccCmm::ToInternalEncoding() no longer clamps icEncodeFloat - but did not
    create it.

    The contract pinned here: a negative base evaluates as a base of zero,
    which is what types 1 and 2 already return below their zero crossing, so
    the segment saturates at its own zero instead of producing NaN.  Results
    that were finite are unchanged, which matters for the identity: a gamma
    1.0 curve is how extended-range linear data passes a TRC, and its negative
    values must survive.
*/

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"

#include <cmath>
#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

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

/* Exact-or-both-finite comparison: a NaN result must fail, never pass. */
static void checkValue(double got, double want, const char* msg)
{
  bool ok = std::isfinite(got) && std::fabs(got - want) <= 1e-6;
  if (!ok)
    std::printf("      (got %.9g, want %.9g)\n", got, want);
  check(ok, msg);
}

/* A parametric curve of the given type with its parameters in ICC order
 * (g, a, b, c, d, e, f).  The caller owns the result. */
static CIccTagParametricCurve* make_curve(icUInt16Number nType, const double* params)
{
  CIccTagParametricCurve* pCurve = new CIccTagParametricCurve;
  pCurve->SetFunctionType(nType);
  for (int i = 0; i < pCurve->GetNumParam(); i++)
    (*pCurve)[i] = (icFloatNumber)params[i];
  return pCurve;
}

/* The curve on its own, one function type at a time. */
static void testCurveApply()
{
  /* Type 0, Y = X^g, is the case with no domain test of any kind. */
  const double gamma22[] = { 2.2 };
  CIccTagParametricCurve* pGamma = make_curve(0, gamma22);
  checkValue(pGamma->Apply((icFloatNumber)-2.0), 0.0,
             "type 0, gamma 2.2: a negative input evaluates as zero, not NaN");
  checkValue(pGamma->Apply((icFloatNumber)0.5), pow(0.5, 2.2),
             "type 0, gamma 2.2: an in-domain input is unchanged");
  delete pGamma;

  /* An integer exponent was already finite for a negative base, and the
   * identity is how extended-range linear data crosses a TRC - so it must
   * not be clamped. */
  const double gamma10[] = { 1.0 };
  CIccTagParametricCurve* pIdentity = make_curve(0, gamma10);
  checkValue(pIdentity->Apply((icFloatNumber)-2.0), -2.0,
             "type 0, gamma 1.0: a negative input still passes through");
  delete pIdentity;

  /* Type 1's threshold -b/a keeps the base non-negative only while a > 0. */
  const double negA[] = { 2.2, -1.0, 0.0 };
  CIccTagParametricCurve* pNegA = make_curve(1, negA);
  checkValue(pNegA->Apply((icFloatNumber)0.5), 0.0,
             "type 1 with a < 0: the negative base above the threshold evaluates as zero");
  delete pNegA;

  /* Type 2 is type 1 plus c, and saturates at c. */
  const double negA2[] = { 2.2, -1.0, 0.0, 0.25 };
  CIccTagParametricCurve* pNegA2 = make_curve(2, negA2);
  checkValue(pNegA2->Apply((icFloatNumber)0.5), 0.25,
             "type 2 with a < 0: the negative base evaluates as zero, leaving c");
  delete pNegA2;

  /* Types 3 and 4 test X against d, which says nothing about aX + b once d
   * is below the base's zero crossing (-b/a, about -0.052 here). */
  const double srgbLowD[] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, -0.5 };
  CIccTagParametricCurve* pLowD = make_curve(3, srgbLowD);
  checkValue(pLowD->Apply((icFloatNumber)-0.2), 0.0,
             "type 3 with d below the zero crossing: the negative base evaluates as zero");
  delete pLowD;

  const double srgbLowD4[] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, -0.5, 0.125, 0.0 };
  CIccTagParametricCurve* pLowD4 = make_curve(4, srgbLowD4);
  checkValue(pLowD4->Apply((icFloatNumber)-0.2), 0.125,
             "type 4 with d below the zero crossing: the negative base evaluates as zero, leaving e");
  delete pLowD4;

  /* The ordinary sRGB curve sends negatives down its linear toe, cX, which
   * was always finite and must stay exactly as it was. */
  const double srgb[] = { 2.4, 1.0 / 1.055, 0.055 / 1.055, 1.0 / 12.92, 0.04045 };
  CIccTagParametricCurve* pSrgb = make_curve(3, srgb);
  checkValue(pSrgb->Apply((icFloatNumber)-0.2), -0.2 / 12.92,
             "type 3 sRGB: a negative input still follows the linear toe");
  delete pSrgb;
}

static void attach_xyz(CIccProfile& p, icSignature sig, double x, double y, double z)
{
  CIccTagXYZ* t = new CIccTagXYZ;
  (*t)[0].X = icDtoF(x);
  (*t)[0].Y = icDtoF(y);
  (*t)[0].Z = icDtoF(z);
  p.AttachTag(sig, t);
}

/* The same value through a whole CMM, to show the NaN is reachable from the
 * public API with no encoding step in between. */
static void testCmmApply()
{
  CIccProfile p;
  p.m_Header.deviceClass = icSigDisplayClass;
  p.m_Header.colorSpace = icSigRgbData;
  p.m_Header.pcs = icSigXYZData;
  p.m_Header.version = icVersionNumberV4_3;

  attach_xyz(p, icSigMediaWhitePointTag, 0.9642, 1.0, 0.8249);
  attach_xyz(p, icSigRedMatrixColumnTag, 0.4361, 0.2225, 0.0139);
  attach_xyz(p, icSigGreenMatrixColumnTag, 0.3851, 0.7169, 0.0971);
  attach_xyz(p, icSigBlueMatrixColumnTag, 0.1431, 0.0606, 0.7141);

  const double gamma22[] = { 2.2 };
  p.AttachTag(icSigRedTRCTag, make_curve(0, gamma22));
  p.AttachTag(icSigGreenTRCTag, make_curve(0, gamma22));
  p.AttachTag(icSigBlueTRCTag, make_curve(0, gamma22));

  CIccCmm cmm(icSigRgbData, icSigXYZData, true);
  bool bReady = cmm.AddXform(p, icRelativeColorimetric) == icCmmStatOk &&
                cmm.Begin() == icCmmStatOk;
  check(bReady, "a gamma 2.2 matrix/TRC profile starts a CMM");
  if (!bReady)
    return;

  icFloatNumber in[3] = { (icFloatNumber)-2.0, (icFloatNumber)0.5, (icFloatNumber)0.5 };
  icFloatNumber out[3] = { 0, 0, 0 };
  check(cmm.Apply(out, in) == icCmmStatOk, "the CMM applies a pixel with a negative channel");
  check(std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]),
        "and the PCS result is finite in every channel");
}

int main()
{
  testCurveApply();
  testCmmApply();

  if (g_failures)
    std::printf("parametric-curve-negative-domain: %d assertion(s) failed\n", g_failures);
  else
    std::printf("parametric-curve-negative-domain: all assertions passed\n");

  return g_failures ? 1 : 0;
}
