/*
    File:       formula-segment-negative-domain.cpp

    Contains:   CTest helper for CIccFormulaCurveSegment::Apply() given a value
                that puts a power function's base below zero (#2547).

    Formula types 0, 1 and 4 to 7 raise a base built from X to a real
    exponent, and types 6 and 7 raise their ratio to a second one.  pow() of a
    negative base under a non-integer exponent is NaN, and nothing clamps
    first: a profile's first segment starts at -inf, and
    CIccMpeCurveSet::Apply() hands values to it unchanged.  ICC.1-2022
    Table 60 and ICC.2-2023 Table 111 require float32Number results over the
    whole segment.

    The contract pinned here is #2544's rule for parametricCurveType: a
    negative base under a non-integer exponent evaluates as a base of zero.
    For every power there are three cases - a negative base under a
    fractional exponent, the same base under an integer exponent (always
    finite, and must not change), and an in-domain value.

    Type 2's base is a parameter, not X, and type 3 keeps clipPow(); neither
    is covered here.

    Given the generated argbRef.icc and LaserProjector.icc as arguments, it
    also applies the two tracked transforms that reach a negative base:
    argbRef's B2D3 curves after its 31-to-3 matrix, and LaserProjector's B2A1
    curves before the CLUT that used to map the NaN to grid 0.
*/

#include "IccDefs.h"
#include "IccMpeBasic.h"
#include "IccProfile.h"
#include "IccTagMPE.h"

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

/* One formula segment over (-inf, +inf), with its parameters in the order
 * Table 111 lists them, applied to v. */
static double applySegment(icUInt16Number nType, const double* params, int nParams, double v)
{
  icFloatNumber p[7] = { 0, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < nParams; i++)
    p[i] = (icFloatNumber)params[i];

  CIccFormulaCurveSegment seg(icMinFloat32Number, icMaxFloat32Number);
  seg.SetFunction(nType, (icUInt8Number)nParams, p);
  if (!seg.Begin(NULL)) {
    std::printf("      (type %u failed to begin)\n", (unsigned)nType);
    return NAN;
  }
  return seg.Apply((icFloatNumber)v);
}

static void testType0()
{
  /* Y = (a * X + b)^g + c               : g a b c */
  const double frac[] = { 2.2, 1.0, 0.0, 0.25 };
  checkValue(applySegment(0, frac, 4, -0.5), 0.25,
             "type 0, g 2.2: a negative X evaluates as a zero base, leaving c");
  const double shifted[] = { 2.2, 1.0, -0.25, 0.25 };
  checkValue(applySegment(0, shifted, 4, 0.1), 0.25,
             "type 0, g 2.2: a negative aX + b from a positive X evaluates as a zero base");
  const double integer[] = { 2.0, 1.0, 0.0, 0.25 };
  checkValue(applySegment(0, integer, 4, -0.5), 0.5,
             "type 0, g 2.0: a negative base under an integer exponent is unchanged");
  checkValue(applySegment(0, frac, 4, 0.5), pow(0.5, 2.2) + 0.25,
             "type 0, g 2.2: an in-domain X is unchanged");
  const double identity[] = { 1.0, 1.0, 0.0, 0.0 };
  checkValue(applySegment(0, identity, 4, -0.5), -0.5,
             "type 0, g 1.0: a negative X still passes through");
}

static void testType1()
{
  /* Y = a * log10(b * X^g + c) + d      : g a b c d */
  const double frac[] = { 2.2, 1.0, 1.0, 1.0, 0.0 };
  checkValue(applySegment(1, frac, 5, -0.5), 0.0,
             "type 1, g 2.2: a negative X evaluates X^g as zero");
  const double integer[] = { 3.0, 1.0, 1.0, 1.0, 0.0 };
  checkValue(applySegment(1, integer, 5, -0.5), log10(0.875),
             "type 1, g 3.0: a negative X under an integer exponent is unchanged");
  checkValue(applySegment(1, frac, 5, 0.5), log10(pow(0.5, 2.2) + 1.0),
             "type 1, g 2.2: an in-domain X is unchanged");
}

static void testType4()
{
  /* Y = a * ln(d * X^g - b) + c         : g a b c d */
  const double frac[] = { 2.2, 1.0, -1.0, 0.0, 1.0 };
  checkValue(applySegment(4, frac, 5, -0.5), 0.0,
             "type 4, g 2.2: a negative X evaluates X^g as zero");
  const double integer[] = { 3.0, 1.0, -1.0, 0.0, 1.0 };
  checkValue(applySegment(4, integer, 5, -0.5), log(0.875),
             "type 4, g 3.0: a negative X under an integer exponent is unchanged");
  checkValue(applySegment(4, frac, 5, 0.5), log(pow(0.5, 2.2) + 1.0),
             "type 4, g 2.2: an in-domain X is unchanged");
}

static void testType5()
{
  /* Y = e * exp((d * X^g - c) / a) + b  : g a b c d e */
  const double frac[] = { 2.2, 1.0, 0.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(5, frac, 6, -0.5), 1.0,
             "type 5, g 2.2: a negative X evaluates X^g as zero");
  const double integer[] = { 3.0, 1.0, 0.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(5, integer, 6, -0.5), exp(-0.125),
             "type 5, g 3.0: a negative X under an integer exponent is unchanged");
  checkValue(applySegment(5, frac, 6, 0.5), exp(pow(0.5, 2.2)),
             "type 5, g 2.2: an in-domain X is unchanged");
}

static void testType6()
{
  /* Y = d * (max(e * X^g - a, 0) / (b - c * X^g))^w   : w g a b c d e */

  /* Inner X^g, with an integer w so only the inner power is under test:
   * Y = (X^g + 1) / 2. */
  const double inner[] = { 1.0, 2.2, -1.0, 2.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, inner, 7, -0.5), 0.5,
             "type 6, g 2.2: a negative X evaluates X^g as zero");
  const double innerInt[] = { 1.0, 3.0, -1.0, 2.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, innerInt, 7, -0.5), 0.4375,
             "type 6, g 3.0: a negative X under an integer exponent is unchanged");
  checkValue(applySegment(6, inner, 7, 0.5), (pow(0.5, 2.2) + 1.0) / 2.0,
             "type 6, g 2.2: an in-domain X is unchanged");

  /* Outer power: a negative b makes the ratio negative for a positive X.
   * g 1.0 takes the shortcut branch, g 2.2 the full one. */
  const double outerShort[] = { 2.2, 1.0, 0.0, -1.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, outerShort, 7, 0.5), 0.0,
             "type 6, g 1.0, w 2.2: a negative ratio evaluates as zero");
  const double outerFull[] = { 2.2, 2.2, 0.0, -1.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, outerFull, 7, 0.5), 0.0,
             "type 6, g 2.2, w 2.2: a negative ratio evaluates as zero");
  const double outerInt[] = { 2.0, 1.0, 0.0, -1.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, outerInt, 7, 0.5), 0.25,
             "type 6, w 2.0: a negative ratio under an integer exponent is unchanged");
  const double outerPos[] = { 2.2, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0 };
  checkValue(applySegment(6, outerPos, 7, 0.5), pow(0.5, 2.2),
             "type 6, w 2.2: an in-domain ratio is unchanged");
}

static void testType7()
{
  /* Y = d * ((a + b * X^g) / (1 + c * X^g))^w          : w g a b c d */

  /* Inner X^g with an integer w: Y = 1 + X^g. */
  const double inner[] = { 1.0, 2.2, 1.0, 1.0, 0.0, 1.0 };
  checkValue(applySegment(7, inner, 6, -0.5), 1.0,
             "type 7, g 2.2: a negative X evaluates X^g as zero");
  const double innerInt[] = { 1.0, 3.0, 1.0, 1.0, 0.0, 1.0 };
  checkValue(applySegment(7, innerInt, 6, -0.5), 0.875,
             "type 7, g 3.0: a negative X under an integer exponent is unchanged");
  checkValue(applySegment(7, inner, 6, 0.5), 1.0 + pow(0.5, 2.2),
             "type 7, g 2.2: an in-domain X is unchanged");

  /* Outer power: b = -1 makes the ratio -X^g, negative for a positive X,
   * from the parameters alone. */
  const double outerShort[] = { 2.2, 1.0, 0.0, -1.0, 0.0, 1.0 };
  checkValue(applySegment(7, outerShort, 6, 0.5), 0.0,
             "type 7, g 1.0, w 2.2: a negative ratio evaluates as zero");
  const double outerFull[] = { 2.2, 2.2, 0.0, -1.0, 0.0, 1.0 };
  checkValue(applySegment(7, outerFull, 6, 0.5), 0.0,
             "type 7, g 2.2, w 2.2: a negative ratio evaluates as zero");
  const double outerInt[] = { 2.0, 1.0, 0.0, -1.0, 0.0, 1.0 };
  checkValue(applySegment(7, outerInt, 6, 0.5), 0.25,
             "type 7, w 2.0: a negative ratio under an integer exponent is unchanged");
  checkValue(applySegment(7, outerShort, 6, -0.5), pow(0.5, 2.2),
             "type 7, w 2.2: an in-domain ratio is unchanged");
}

/* A tag holding copies of elements [first, last] of pSrc, begun and ready
 * to apply.  The caller owns the result, or gets NULL. */
static CIccTagMultiProcessElement* sliceMpe(CIccTagMultiProcessElement* pSrc, int first, int last)
{
  CIccMultiProcessElement* pFirst = pSrc->GetElement(first);
  CIccMultiProcessElement* pLast = pSrc->GetElement(last);
  if (!pFirst || !pLast)
    return NULL;

  CIccTagMultiProcessElement* pTag =
    new CIccTagMultiProcessElement(pFirst->NumInputChannels(), pLast->NumOutputChannels());
  for (int i = first; i <= last; i++)
    pTag->Attach(pSrc->GetElement(i)->NewCopy());

  if (!pTag->Begin()) {
    delete pTag;
    return NULL;
  }
  return pTag;
}

static CIccTagMultiProcessElement* findMpe(CIccProfile* pProfile, icSignature sig)
{
  CIccTag* pTag = pProfile ? pProfile->FindTag(sig) : NULL;
  if (!pTag || pTag->GetType() != icSigMultiProcessElementType)
    return NULL;
  return (CIccTagMultiProcessElement*)pTag;
}

/* argbRef's B2D3 is a 31-to-3 matrix and then three type 0 curves with
 * g = 0.4547 over (-inf, +inf).  Negative matrix lobes send a single-band
 * spectrum below zero, which used to leave the curve as NaN. */
static void testArgbRef(const char* szPath)
{
  CIccProfile* pProfile = OpenIccProfile(szPath);
  check(pProfile != NULL, "argbRef.icc opens");
  CIccTagMultiProcessElement* pB2D3 = findMpe(pProfile, icSigBToD3Tag);
  check(pB2D3 && pB2D3->NumElements() == 2 && pB2D3->NumInputChannels() == 31 &&
          pB2D3->NumOutputChannels() == 3,
        "argbRef B2D3 is the 31-to-3 matrix and curve set");

  CIccTagMultiProcessElement* pMatrix = pB2D3 ? sliceMpe(pB2D3, 0, 0) : NULL;
  CIccTagMultiProcessElement* pWhole = pB2D3 ? sliceMpe(pB2D3, 0, 1) : NULL;
  check(pMatrix && pWhole, "argbRef B2D3 begins");

  if (pMatrix && pWhole) {
    CIccApplyTagMpe* pMatrixApply = pMatrix->GetNewApply();
    CIccApplyTagMpe* pWholeApply = pWhole->GetNewApply();
    int nNegative = 0, nBad = 0;

    /* 31 single-band spectra, then a flat 0.5. */
    for (int s = 0; s <= 31; s++) {
      icFloatNumber spectrum[31];
      for (int i = 0; i < 31; i++)
        spectrum[i] = (icFloatNumber)(s == 31 ? 0.5 : (i == s ? 1.0 : 0.0));

      icFloatNumber base[3], out[3];
      pMatrix->Apply(pMatrixApply, base, spectrum);
      pWhole->Apply(pWholeApply, out, spectrum);

      for (int c = 0; c < 3; c++) {
        if (base[c] < 0)
          nNegative++;
        if (!std::isfinite(out[c]) || (base[c] < 0 && out[c] != 0))
          nBad++;
      }
    }

    std::printf("      (%d negative matrix outputs, %d bad curve outputs)\n", nNegative, nBad);
    check(nNegative > 0, "argbRef B2D3: the test spectra send the matrix below zero");
    check(nBad == 0, "argbRef B2D3: every output is finite, and 0 where the base was negative");

    delete pMatrixApply;
    delete pWholeApply;
  }

  delete pMatrix;
  delete pWhole;
  delete pProfile;
}

/* LaserProjector's B2A1 starts with three type 0 curves, g = 0.4545, then a
 * CLUT.  Only the curve set is applied, so a NaN cannot be hidden by the
 * CLUT's grid clamp. */
static void testLaserProjector(const char* szPath)
{
  CIccProfile* pProfile = OpenIccProfile(szPath);
  check(pProfile != NULL, "LaserProjector.icc opens");
  CIccTagMultiProcessElement* pB2A1 = findMpe(pProfile, icSigBToA1Tag);
  check(pB2A1 && pB2A1->NumElements() == 3 && pB2A1->GetElement(0) &&
          pB2A1->GetElement(0)->GetType() == icSigCurveSetElemType,
        "LaserProjector B2A1 starts with a curve set");

  CIccTagMultiProcessElement* pCurves = pB2A1 ? sliceMpe(pB2A1, 0, 0) : NULL;
  check(pCurves != NULL, "LaserProjector B2A1 curve set begins");

  if (pCurves) {
    CIccApplyTagMpe* pApply = pCurves->GetNewApply();
    icFloatNumber in[3] = { (icFloatNumber)-0.05, (icFloatNumber)0.5, (icFloatNumber)0.5 };
    icFloatNumber out[3] = { 0, 0, 0 };
    pCurves->Apply(pApply, out, in);

    checkValue(out[0], 0.0,
               "LaserProjector B2A1 curves: X = -0.05 evaluates as a zero base, not NaN");
    checkValue(out[1], pow(0.5, 0.45449999), "LaserProjector B2A1 curves: Y = 0.5 is unchanged");
    delete pApply;
  }

  delete pCurves;
  delete pProfile;
}

int main(int argc, char* argv[])
{
  testType0();
  testType1();
  testType4();
  testType5();
  testType6();
  testType7();

  if (argc == 3) {
    testArgbRef(argv[1]);
    testLaserProjector(argv[2]);
  }
  else {
    check(false, "usage: formula-segment-negative-domain <argbRef.icc> <LaserProjector.icc>");
  }

  if (g_failures)
    std::printf("formula-segment-negative-domain: %d assertion(s) failed\n", g_failures);
  else
    std::printf("formula-segment-negative-domain: all assertions passed\n");

  return g_failures ? 1 : 0;
}
