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

    Type 2's base is the parameter b, and a negative b under a fractional
    exponent follows the same rule.  Type 3 keeps clipPow().

    Types 1 and 4 take a logarithm, which is NaN below zero and -inf at it.
    An argument at or below zero is given the smallest positive float.

    CIccFormulaCurveSegment::Validate() reports, as a Warning, each of those
    places a segment's range reaches, and a denominator or a power that has a
    pole there.  The reports have clean controls, over a range that stays
    clear of the problem or with the parameter that causes it changed, so a
    check that reports everything fails.

    Given the generated argbRef.icc and LaserProjector.icc as arguments, it
    also applies the two tracked transforms that reach a negative base:
    argbRef's B2D3 curves after its 31-to-3 matrix, and LaserProjector's B2A1
    curves before the CLUT that used to map the NaN to grid 0.
*/

#include "IccDefs.h"
#include "IccMpeBasic.h"
#include "IccProfile.h"
#include "IccTagMPE.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>

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

/* The logarithm of the clamped argument, as icFormulaLogArg() gives it,
 * rounded to the float Apply() returns. */
static const double kLogFltMin = (icFloatNumber)log(FLT_MIN);
static const double kLog10FltMin = (icFloatNumber)log10(FLT_MIN);

static void testType2()
{
  /* Y = a * b^(c * X + d) + e           : a b c d e */
  const double frac[] = { 1.0, -2.0, 1.0, 0.0, 0.25 };
  checkValue(applySegment(2, frac, 5, 0.5), 0.25,
             "type 2, b -2: a fractional exponent evaluates as a zero base, leaving e");
  checkValue(applySegment(2, frac, 5, 3.0), -8.0 + 0.25,
             "type 2, b -2: an integer exponent is unchanged");
  const double positive[] = { 1.0, 2.0, 1.0, 0.0, 0.25 };
  checkValue(applySegment(2, positive, 5, 0.5), sqrt(2.0) + 0.25,
             "type 2, b 2: an in-domain exponent is unchanged");
}

static void testType3()
{
  /* Y = a * (b * X + c)^g + d           : g a b c d
   * clipPow() zeroes any base <= 0, even under an integer exponent. */
  const double integer[] = { 2.0, 1.0, 1.0, 0.0, 0.25 };
  checkValue(applySegment(3, integer, 5, -1.0), 0.25,
             "type 3, g 2.0: a negative base still evaluates as zero through clipPow()");
  checkValue(applySegment(3, integer, 5, 0.5), 0.5,
             "type 3, g 2.0: an in-domain X is unchanged");
}

static void testLogArgument()
{
  /* Type 1, Y = log10(X - 1): the argument is negative below X = 1 and zero at
   * it.  g 2.0 goes through the power, g 1.0 through the same expression. */
  const double t1[] = { 1.0, 1.0, 1.0, -1.0, 0.0 };
  checkValue(applySegment(1, t1, 5, 0.5), kLog10FltMin,
             "type 1: a negative logarithm argument evaluates as FLT_MIN");
  checkValue(applySegment(1, t1, 5, 1.0), kLog10FltMin,
             "type 1: a zero logarithm argument evaluates as FLT_MIN, not -inf");
  checkValue(applySegment(1, t1, 5, 11.0), 1.0,
             "type 1: a positive logarithm argument is unchanged");

  /* Type 4, Y = ln(X^g - 1).  g 1.0 takes Begin()'s shortcut, g 2.0 does not. */
  const double t4short[] = { 1.0, 1.0, 1.0, 0.0, 1.0 };
  checkValue(applySegment(4, t4short, 5, 0.5), kLogFltMin,
             "type 4, g 1.0: a negative logarithm argument evaluates as FLT_MIN");
  checkValue(applySegment(4, t4short, 5, 1.0), kLogFltMin,
             "type 4, g 1.0: a zero logarithm argument evaluates as FLT_MIN, not -inf");
  checkValue(applySegment(4, t4short, 5, 3.0), log(2.0),
             "type 4, g 1.0: a positive logarithm argument is unchanged");
  const double t4full[] = { 2.0, 1.0, 1.0, 0.0, 1.0 };
  checkValue(applySegment(4, t4full, 5, 0.5), kLogFltMin,
             "type 4, g 2.0: a negative logarithm argument evaluates as FLT_MIN");
  checkValue(applySegment(4, t4full, 5, 2.0), log(3.0),
             "type 4, g 2.0: a positive logarithm argument is unchanged");
}

/* Validate one formula segment over (start, end], with its parameters in the
 * order Table 111 lists them.  Returns the status; the report goes to sReport. */
static icValidateStatus validateSegment(icUInt16Number nType, const double* params, int nParams,
                                        double start, double end, std::string& sReport)
{
  icFloatNumber p[7] = { 0, 0, 0, 0, 0, 0, 0 };
  for (int i = 0; i < nParams; i++)
    p[i] = (icFloatNumber)params[i];

  CIccFormulaCurveSegment seg((icFloatNumber)start, (icFloatNumber)end);
  seg.SetFunction(nType, (icUInt8Number)nParams, p);
  sReport.clear();
  return seg.Validate("", sReport);
}

static bool has(const std::string& s, const char* szText)
{
  return s.find(szText) != std::string::npos;
}

/* The segment reports szText as a Warning, and nothing else about its domain. */
static void checkReported(icUInt16Number nType, const double* params, int nParams,
                          double start, double end, const char* szText, const char* msg)
{
  std::string sReport;
  icValidateStatus rv = validateSegment(nType, params, nParams, start, end, sReport);
  size_t first = sReport.find("formula curve segment");
  bool ok = rv == icValidateWarning && has(sReport, szText) && first != std::string::npos &&
            sReport.find("formula curve segment", first + 1) == std::string::npos;
  if (!ok)
    std::printf("      (status %d, report: %s)\n", (int)rv, sReport.c_str());
  check(ok, msg);
}

/* The segment validates clean. */
static void checkClean(icUInt16Number nType, const double* params, int nParams,
                       double start, double end, const char* msg)
{
  std::string sReport;
  icValidateStatus rv = validateSegment(nType, params, nParams, start, end, sReport);
  bool ok = rv == icValidateOK && sReport.empty();
  if (!ok)
    std::printf("      (status %d, report: %s)\n", (int)rv, sReport.c_str());
  check(ok, msg);
}

static const char* kNegativeBase = "raises a negative base to a fractional power";
static const char* kLog = "takes the logarithm of a value that is not positive";
static const char* kZeroPow = "raises zero to a negative power";
static const char* kDenominator = "has a denominator that reaches zero";
static const char* kClipPow = "or zero to a negative power, for part of its range; the power is evaluated as zero";

static void testValidate()
{
  const double inf = icMaxFloat32Number;

  /* Negative base under a fractional exponent. */
  const double t0frac[] = { 2.2, 1.0, 0.0, 0.0 };
  checkReported(0, t0frac, 4, -inf, inf, kNegativeBase,
                "validate type 0, g 2.2 over (-inf, +inf]: negative base reported");
  checkClean(0, t0frac, 4, 0.0, inf, "validate type 0, g 2.2 over (0, +inf]: clean");
  const double t0int[] = { 2.0, 1.0, 0.0, 0.0 };
  checkClean(0, t0int, 4, -inf, inf, "validate type 0, g 2.0 over (-inf, +inf]: clean");

  /* Type 2's base is b. */
  const double t2neg[] = { 1.0, -2.0, 1.0, 0.0, 0.0 };
  checkReported(2, t2neg, 5, 0.0, 1.0, kNegativeBase,
                "validate type 2, b -2 over (0, 1]: negative base reported");
  const double t2pos[] = { 1.0, 2.0, 1.0, 0.0, 0.0 };
  checkClean(2, t2pos, 5, 0.0, 1.0, "validate type 2, b 2 over (0, 1]: clean");

  /* Outer power of type 7: the ratio is -X. */
  const double t7neg[] = { 2.2, 1.0, 0.0, -1.0, 0.0, 1.0 };
  checkReported(7, t7neg, 6, 0.0, 1.0, kNegativeBase,
                "validate type 7, w 2.2, ratio -X over (0, 1]: negative base reported");
  const double t7int[] = { 2.0, 1.0, 0.0, -1.0, 0.0, 1.0 };
  checkClean(7, t7int, 6, 0.0, 1.0, "validate type 7, w 2.0, ratio -X over (0, 1]: clean");

  /* Logarithm.  Type 4 with ln(1 - X) over (0, 1] reaches zero only at X = 1,
   * the segment's included end. */
  const double t1log[] = { 1.0, 1.0, 1.0, -1.0, 0.0 };
  checkReported(1, t1log, 5, 0.0, 1.0, kLog,
                "validate type 1, log10(X - 1) over (0, 1]: logarithm reported");
  checkClean(1, t1log, 5, 1.0, inf, "validate type 1, log10(X - 1) over (1, +inf]: clean");
  const double t4zero[] = { 1.0, 1.0, -1.0, 0.0, -1.0 };
  checkReported(4, t4zero, 5, 0.0, 1.0, kLog,
                "validate type 4, ln(1 - X) over (0, 1]: a zero argument at the end is reported");
  checkClean(4, t4zero, 5, 0.0, 0.5, "validate type 4, ln(1 - X) over (0, 0.5]: clean");

  /* Type 5's X^-1 has its pole at X = 0, inside (-1, 1] but at neither end. */
  const double t5pole[] = { -1.0, 1.0, 0.0, 0.0, 1.0, 1.0 };
  checkReported(5, t5pole, 6, -1.0, 1.0, kZeroPow,
                "validate type 5, exp(X^-1) over (-1, 1]: pole at zero reported");
  checkClean(5, t5pole, 6, 0.0, 1.0, "validate type 5, exp(X^-1) over (0, 1]: clean");

  /* Pole of X^g, g -1, at X = 0.  The start is exclusive, so (0, 1] is clean. */
  const double t0pole[] = { -1.0, 1.0, 0.0, 0.0 };
  checkReported(0, t0pole, 4, -1.0, 1.0, kZeroPow,
                "validate type 0, g -1 over (-1, 1]: zero to a negative power reported");
  checkClean(0, t0pole, 4, 0.0, 1.0, "validate type 0, g -1 over (0, 1]: clean");

  /* (X - 0.5)^-1 has its pole inside the range, away from either end. */
  const double t0inner[] = { -1.0, 1.0, -0.5, 0.0 };
  checkReported(0, t0inner, 4, 0.0, 1.0, kZeroPow,
                "validate type 0, (X - 0.5)^-1 over (0, 1]: interior pole reported");
  checkClean(0, t0inner, 4, 0.5, 1.0, "validate type 0, (X - 0.5)^-1 over (0.5, 1]: clean");

  /* Denominator 1 - X of type 7 reaches zero at X = 1, the included end,
   * without changing sign; 1 - 3X^2 of type 6 changes sign at 1/sqrt(3),
   * which no float reaches. */
  const double t7den[] = { 1.0, 1.0, 1.0, 0.0, -1.0, 1.0 };
  checkReported(7, t7den, 6, 0.0, 1.0, kDenominator,
                "validate type 7, 1 / (1 - X) over (0, 1]: zero denominator at the end reported");
  checkClean(7, t7den, 6, 0.0, 0.5, "validate type 7, 1 / (1 - X) over (0, 0.5]: clean");
  const double t6den[] = { 1.0, 2.0, -1.0, 1.0, 3.0, 1.0, 1.0 };
  checkReported(6, t6den, 7, 0.0, 1.0, kDenominator,
                "validate type 6, (X^2 + 1) / (1 - 3X^2) over (0, 1]: sign change reported");
  checkClean(6, t6den, 7, 0.0, 0.5, "validate type 6, (X^2 + 1) / (1 - 3X^2) over (0, 0.5]: clean");

  /* Type 7's ratio (0.25 - X) / (1 - 2X) is negative only between its two
   * roots, 0.25 and 0.5, which are both floats: the value at each root is 0
   * or a zero denominator, so only the float after 0.25 shows the negative
   * base. */
  {
    const double t7gap[] = { 2.2, 1.0, 0.25, -1.0, -2.0, 1.0 };
    std::string sReport;
    icValidateStatus rv = validateSegment(7, t7gap, 6, 0.0, 1.0, sReport);
    check(rv == icValidateWarning && has(sReport, kNegativeBase) && has(sReport, kDenominator),
          "validate type 7, w 2.2, (0.25 - X) / (1 - 2X) over (0, 1]: negative base between roots reported");
  }

  /* g 1.0 shortcuts do their arithmetic in float, and so must the check.
   * Type 7's 1 - 3X is exactly 0 in float at X = 0.33333334, so w = -1 gives
   * inf there, though the same sum in double is -3e-8.  Type 4's 3X - 1 is 0
   * in float at that X and positive in double. */
  const double t7float[] = { -1.0, 1.0, 1.0, -3.0, 0.0, 1.0 };
  checkReported(7, t7float, 6, 0.0, 1.0, kZeroPow,
                "validate type 7, g 1.0, (1 - 3X)^-1 over (0, 1]: float zero reported");
  checkClean(7, t7float, 6, 0.5, 1.0, "validate type 7, g 1.0, (1 - 3X)^-1 over (0.5, 1]: clean");
  const double t4float[] = { 1.0, 1.0, 1.0, 0.0, 3.0 };
  checkReported(4, t4float, 5, 0.3333333134651184, 1.0, kLog,
                "validate type 4, g 1.0, ln(3X - 1) from X = 0.33333334: float zero reported");
  checkClean(4, t4float, 5, 0.5, 1.0, "validate type 4, g 1.0, ln(3X - 1) over (0.5, 1]: clean");

  /* Type 6's numerator max(X - 10, 0) is 0 over (0, 1], so 1 - 3X changing
   * sign at 1/3 is no pole: Apply() gives 0 throughout. */
  const double t6flat[] = { 1.0, 1.0, 10.0, 1.0, 3.0, 1.0, 1.0 };
  checkClean(6, t6flat, 7, 0.0, 1.0,
             "validate type 6, max(X - 10, 0) / (1 - 3X) over (0, 1]: no pole, clean");

  /* Type 3, clipPow(): the HLG OOTF shape, (1.00629 X - 0.00629)^(-1/6),
   * has no value below X = 0.00625.  An integer g has one everywhere, even
   * where clipPow() replaces it, as in BT2100HlgNarrow's g = 2 over (-inf, 0.5]. */
  const double t3ootf[] = { -1.0 / 6.0, 1.0, 1.006289308, -0.006289308, 0.0 };
  checkReported(3, t3ootf, 5, 0.0, 1.0, kClipPow,
                "validate type 3, g -1/6 over (0, 1]: undefined power reported");
  checkClean(3, t3ootf, 5, 0.01, 1.0, "validate type 3, g -1/6 over (0.01, 1]: clean");
  const double t3int[] = { 2.0, 1.0 / 3.0, 1.0, 0.0, 0.0 };
  checkClean(3, t3int, 5, -inf, 0.5, "validate type 3, g 2 over (-inf, 0.5]: clean");
  const double t3pole[] = { -1.0, 1.0, 1.0, 0.0, 0.0 };
  checkReported(3, t3pole, 5, -1.0, 1.0, kClipPow,
                "validate type 3, g -1 over (-1, 1]: zero to a negative power reported");
  checkClean(3, t3pole, 5, -1.0, -0.5, "validate type 3, g -1 over (-1, -0.5]: clean");

  /* A non-finite parameter is not examined (and must not crash). */
  const double t0nan[] = { NAN, 1.0, 0.0, 0.0 };
  std::string sReport;
  validateSegment(0, t0nan, 4, -inf, inf, sReport);
  check(!has(sReport, "formula curve segment"), "validate type 0, g NaN: domain not examined");
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

  if (pB2D3) {
    std::string sReport;
    pB2D3->Validate("", sReport, pProfile);
    check(has(sReport, kNegativeBase), "argbRef B2D3: Validate reports the negative base");
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

  if (pB2A1) {
    std::string sReport;
    pB2A1->Validate("", sReport, pProfile);
    check(has(sReport, kNegativeBase), "LaserProjector B2A1: Validate reports the negative base");
  }

  delete pCurves;
  delete pProfile;
}

int main(int argc, char* argv[])
{
  testType0();
  testType1();
  testType2();
  testType3();
  testLogArgument();
  testType4();
  testType5();
  testType6();
  testType7();
  testValidate();

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
