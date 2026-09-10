// Behavioural regression for the tone-mapping step of ICC.1 clause 8.10.2:
// the analytic PQ and HLG transfer functions, the Headroom Adaptive Gain Curve
// evaluator of the HAGC amendment's annex 1, and the CMM path that puts them
// between the TRC linearisation and the 3x3 matrix.
//
// This is the first behavioural HDR test in the tree. Everything HDR before it
// tested that bytes survive a round trip or that a classifier answers
// correctly; nothing tested that a pixel comes out with the right value, so
// there is no baseline here to regress against - the numbers below are either
// hand-computed from the published formulas or independently known anchors.
//
// What each part is guarding, and why it is not obvious:
//
// 1. The transfer functions have to be analytic. A sampled curveType TRC
//    clamps its output at 1.0 (IccTagLut.cpp), so a chain that linearises
//    through the profile's own TRC tags cannot represent any light above HDR
//    reference white at all - every highlight silently flattens to diffuse
//    white and the profile still "works". The PQ anchor at code 0.75 is the
//    one that would catch it: ~983 cd/m^2 is nearly five times reference
//    white, so a clamped implementation returns 1.0 where this expects 4.84.
//
// 2. The gain evaluator's Hermite segments are hand-checkable, and are
//    hand-checked here rather than pinned from this implementation's own
//    output. A cubic with the wrong C2/C3 still produces a smooth plausible
//    curve through the same control points.
//
// 3. The headroom blend is a linear interpolation between two *gain
//    exponents*, not between two gains. Interpolating the gains instead is the
//    natural mistake and agrees at both endpoints, so only a midpoint test
//    separates them: 2^(0.5*g) is not 0.5*2^g.
//
// 4. Above the last control point the annex's extrapolation makes gain(x)*x
//    constant, i.e. a hard clip. That is a property of the whole expression
//    rather than of any one term, so it is tested as the invariant it is.
//
// 5. The inverse is a bisection over t*2^G(t). It is only correct where that
//    function is strictly increasing, and it saturates rather than fails above
//    the last control point - both are tested, because a bisection that
//    silently converges to a bracket end looks exactly like a working inverse.
//
// The end-to-end section uses the Testing/HDR fixtures and skips with a clear
// message when they have not been built, since that means
// Testing/CreateAllProfiles.sh has not run rather than that the code is wrong.
//
// Returns 0 on success; the number of failed assertions otherwise.

#include "IccHdrToneMap.h"
#include "IccHdrProfile.h"
#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

/* A SKIPPED fixture must not report as PASSED.  These tests open generated .icc
 * under Testing/HDR; when one will not open they print SKIP and carry on, and
 * main() used to return g_failures - which is 0 - so ctest recorded a green
 * PASS for a run that asserted nothing.  Measured from a directory with no
 * fixtures, three of these exited 0 having skipped 19, 7 and 2 checks.
 * g_skips lets main() return 77 instead, which SKIP_RETURN_CODE turns into a
 * ctest Skipped result.  A real failure still wins: 77 is only returned when
 * nothing failed. */
int g_skips = 0;


void check(bool cond, const char *szWhat)
{
  if (!cond) {
    printf("FAIL: %s\n", szWhat);
    g_failures++;
  }
}

void checkClose(double got, double want, double tol, const char *szWhat)
{
  if (!(fabs(got - want) <= tol)) {
    printf("FAIL: %s (got %.9f, want %.9f, tol %g)\n", szWhat, got, want, tol);
    g_failures++;
  }
}

// ---------------------------------------------------------------------------
// 1. The analytic transfer functions
// ---------------------------------------------------------------------------
void testTransferFunctions()
{
  // Endpoints, which both transfers pin exactly by construction.
  checkClose(icPqEotf(0.0), 0.0, 0.0, "PQ EOTF at 0");
  checkClose(icPqEotf(1.0), 1.0, 1e-9, "PQ EOTF at 1 is the 10 000 cd/m^2 peak");
  checkClose(icPqInverseEotf(0.0), 0.0, 0.0, "PQ inverse EOTF at 0");
  checkClose(icPqInverseEotf(1.0), 1.0, 1e-9, "PQ inverse EOTF at 1");

  // Independent anchor: PQ code 0.75 is the widely quoted ~983 cd/m^2. Any
  // transcription error in m1, m2, c1, c2 or c3 moves this materially.
  checkClose(icPqEotf(0.75) * icPqPeakLuminance, 982.96, 0.5, "PQ EOTF at code 0.75 is ~983 cd/m^2");

  // The other end of the same anchor: 203 cd/m^2 - the BT.2408 HDR reference
  // white and clause 8.10.4's default - encodes at PQ code 0.5806, the "58%
  // signal level" that report states for HDR Reference White.
  checkClose(icPqInverseEotf((icFloatNumber)(203.0 / icPqPeakLuminance)), 0.5806, 5e-4,
             "PQ code for 203 cd/m^2");

  // Round trip across the whole domain.
  int i;
  for (i = 0; i <= 100; i++) {
    icFloatNumber v = (icFloatNumber)(i / 100.0);
    checkClose(icPqInverseEotf(icPqEotf(v)), v, 1e-5, "PQ EOTF round trip");
  }

  // HLG: both of these are exact consequences of the standard's a, b and c,
  // so they check the constants rather than the arithmetic. c is defined as
  // 0.5 - a*ln(4a) precisely so that the OETF reaches 1.0 at scene 1.0, and
  // the two branches meet at the 1/12 knee where sqrt(3/12) is 0.5.
  checkClose(icHlgOetf(1.0), 1.0, 1e-7, "HLG OETF at scene 1.0 reaches signal 1.0");
  checkClose(icHlgOetf((icFloatNumber)icHlgKnee), 0.5, 1e-9, "HLG OETF branches meet at the 1/12 knee");
  checkClose(icHlgInverseOetf(0.5), (icFloatNumber)icHlgKnee, 1e-9, "HLG inverse OETF at the knee");

  for (i = 0; i <= 100; i++) {
    icFloatNumber v = (icFloatNumber)(i / 100.0);
    checkClose(icHlgInverseOetf(icHlgOetf(v)), v, 1e-5, "HLG OETF round trip");
  }

  // The OOTF gain is unity at unit scene luminance for every gamma, which is
  // what makes peak white land at Lw.
  checkClose(icHlgOotfGain(1.0, 1.2), 1.0, 1e-9, "HLG OOTF gain at unit scene luminance");
}

// ---------------------------------------------------------------------------
// 2. CIccHdrTransfer: the change of normalisation
// ---------------------------------------------------------------------------
void testTransferNormalisation()
{
  CIccHdrTransfer pq;

  check(pq.Init(icCicpTransferPQ, (icFloatNumber)icHdrDefaultContentReferenceWhite),
        "PQ transfer initialises");
  check(!pq.UsesProfileCurves(), "PQ does not defer to the profile TRC tags");

  icFloatNumber src[3], dst[3];

  // The property the whole convention rests on: the PQ code for the content
  // reference white comes out at exactly 1.0 in reference-white-relative
  // units. If this drifts, every gain curve is evaluated at the wrong x.
  icFloatNumber vWhite = icPqInverseEotf((icFloatNumber)(icHdrDefaultContentReferenceWhite / icPqPeakLuminance));
  src[0] = src[1] = src[2] = vWhite;
  pq.ToLinear(dst, src);
  checkClose(dst[0], 1.0, 1e-4, "PQ reference white normalises to 1.0");

  // ~983 cd/m^2 over 203 is 4.842 - almost 2.3 stops of headroom, and the
  // value a TRC-clamped implementation could not produce.
  src[0] = src[1] = src[2] = 0.75;
  pq.ToLinear(dst, src);
  checkClose(dst[0], 982.96 / 203.0, 5e-3, "PQ code 0.75 is 4.84x reference white");

  pq.FromLinear(dst, dst);
  checkClose(dst[0], 0.75, 1e-4, "PQ normalisation round trip");

  // A luminance beyond the PQ system's own ceiling has no code point; the
  // inverse clamps to 1.0 rather than returning an out-of-range encoding.
  src[0] = src[1] = src[2] = (icFloatNumber)1000.0;
  pq.FromLinear(dst, src);
  checkClose(dst[0], 1.0, 1e-9, "PQ inverse clamps above the system peak");

  CIccHdrTransfer hlg;

  check(hlg.Init(icCicpTransferHLG, (icFloatNumber)icHdrDefaultContentReferenceWhite),
        "HLG transfer initialises");

  // HLG signal 1.0 is peak white: scene 1.0, OOTF gain 1.0, so display light
  // is exactly Lw and the reference-white-relative value is Lw/CRWL.
  src[0] = src[1] = src[2] = 1.0;
  hlg.ToLinear(dst, src);
  checkClose(dst[0], icHlgDefaultPeakLuminance / icHdrDefaultContentReferenceWhite, 1e-4,
             "HLG peak signal is Lw over reference white");

  // Round trip through the OOTF, which is the part with no closed-form
  // inverse per channel - only through the scene luminance.
  src[0] = (icFloatNumber)0.6;
  src[1] = (icFloatNumber)0.4;
  src[2] = (icFloatNumber)0.2;
  hlg.ToLinear(dst, src);
  hlg.FromLinear(dst, dst);
  checkClose(dst[0], 0.6, 1e-4, "HLG round trip R");
  checkClose(dst[1], 0.4, 1e-4, "HLG round trip G");
  checkClose(dst[2], 0.2, 1e-4, "HLG round trip B");

  // Linear defers to the profile's own curves and touches nothing itself.
  CIccHdrTransfer lin;
  check(lin.Init(icCicpTransferLinear, (icFloatNumber)icHdrDefaultContentReferenceWhite),
        "Linear transfer initialises");
  // The 29-08-2026 revision settled what a Linear value means. Step a) of
  // 8.10.2 normalises "PQ in cd/m2 / 10 000, HLG in scene-referred units per
  // Rec. ITU-R BT.2100, or, for Linear, DIRECTLY IN CD/M2". So Linear does not
  // defer to the profile's TRC tags - the same revision prohibits them - and
  // the change of normalisation is a division by the content reference white,
  // exactly as it is for the other two.
  check(!lin.UsesProfileCurves(), "Linear no longer defers to the profile TRC tags");

  src[0] = (icFloatNumber)3.5;
  lin.ToLinear(dst, src);
  checkClose(dst[0], 3.5 / 203.0, 1e-9,
             "a Linear value is a luminance in cd/m^2, divided by CRWL");

  // 3,5 cd/m2 is below reference white, so the result is below 1.0; the whole
  // point of the normalisation is that a value ABOVE reference white survives,
  // which a sampled TRC clamping at 1.0 could never represent.
  src[0] = (icFloatNumber)406.0;
  lin.ToLinear(dst, src);
  checkClose(dst[0], 2.0, 1e-6, "twice reference white is 2.0, not clipped to 1.0");

  // And the inverse puts it back in cd/m^2.
  lin.FromLinear(src, dst);
  checkClose(src[0], 406.0, 1e-3, "FromLinear returns cd/m^2");

  // Anything clause 8.10.1 does not permit is refused rather than guessed at.
  CIccHdrTransfer bad;
  check(!bad.Init(1, (icFloatNumber)203.0), "BT.709 transfer characteristics are refused");
  check(!bad.Init(13, (icFloatNumber)203.0), "sRGB transfer characteristics are refused");
}

// ---------------------------------------------------------------------------
// 3. The gain evaluator, against hand-computed Hermite values
// ---------------------------------------------------------------------------

// Build metadata with one alternate whose curve is fully specified by the
// caller. Everything else is the simplest legal configuration.
void buildMetadata(icHagcMetadata &meta,
                   icFloatNumber baselineHeadroom,
                   icFloatNumber altHeadroom,
                   icHagcMixingType nMixing,
                   const icFloatNumber *x, const icFloatNumber *y, const icFloatNumber *m,
                   icUInt8Number nPoints)
{
  meta.Reset();
  meta.m_bHeadroomAdaptiveToneMap = true;
  meta.m_baselineHeadroom = baselineHeadroom;
  meta.m_bUnpacked = true;
  meta.SetNumAlternates(1);

  icHagcAlternateImage *pAlt = meta.GetAlternate(0);

  pAlt->m_headroom = altHeadroom;
  pAlt->m_nMixingType = nMixing;
  pAlt->m_nControlPoints = nPoints;
  pAlt->m_bPchipSlope = (m == NULL);

  // The coefficients a decoder fills in for each fixed mixing type; the
  // evaluator reads them rather than the type, so a test that only set the
  // type would be exercising a curve with no mixing at all.
  memset(pAlt->m_coef, 0, sizeof(pAlt->m_coef));

  switch (nMixing) {
    case icHagcMixingMax:
      pAlt->m_coef[icHagcCoefMax] = 1.0;
      break;
    case icHagcMixingComponent:
      pAlt->m_coef[icHagcCoefComponent] = 1.0;
      break;
    case icHagcMixingWeighted:
      pAlt->m_coef[icHagcCoefRed] = (icFloatNumber)(1.0 / 6.0);
      pAlt->m_coef[icHagcCoefGreen] = (icFloatNumber)(1.0 / 6.0);
      pAlt->m_coef[icHagcCoefBlue] = (icFloatNumber)(1.0 / 6.0);
      pAlt->m_coef[icHagcCoefMax] = (icFloatNumber)0.5;
      break;
    default:
      pAlt->m_coef[icHagcCoefMax] = 1.0;
      break;
  }

  icUInt8Number i;
  for (i = 0; i < nPoints; i++) {
    pAlt->m_x[i] = x[i];
    pAlt->m_y[i] = y[i];
    pAlt->m_slope[i] = m ? m[i] : (icFloatNumber)0.0;
  }
}

void testGainEvaluator()
{
  // A two-point curve from (0, 0) to (1, -1) with zero slopes at both ends.
  // Annex 1's coefficients for that segment are M_i = M_i+1 = 0, so
  //   C3 = 2*0 + 0 - 2*(-1) + 0 =  2
  //   C2 = -3*0 + 3*(-1) - 0 - 0 = -3
  //   C1 = 0, C0 = 0
  // and at the midpoint t = 0.5:  2*(0.125) - 3*(0.25) = -0.5 exactly.
  // A gain of 2^-0.5 follows. Getting C2 and C3 the wrong way round gives
  // -0.25 here and the same values at both endpoints.
  icFloatNumber x[4] = { 0.0, 1.0, 0.0, 0.0 };
  icFloatNumber y[4] = { 0.0, -1.0, 0.0, 0.0 };
  icFloatNumber m[4] = { 0.0, 0.0, 0.0, 0.0 };

  icHagcMetadata meta;
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);

  CIccHagcEvaluator ev;

  check(ev.Init(meta), "evaluator accepts a minimal two-point curve");
  check(ev.GetNumCurves() == 2, "the curve list holds the alternate and the baseline");
  checkClose(ev.GetCurveHeadroom(0), 0.0, 0.0, "the list is ordered by headroom, alternate first");
  checkClose(ev.GetCurveHeadroom(1), 2.0, 0.0, "the baseline sits at its own headroom");

  // Target exactly on the alternate: that curve alone, weight one.
  check(ev.SetTargetHeadroom(0.0), "target headroom set to the alternate");
  check(!ev.IsIdentity(), "a target on the alternate is not the identity");
  checkClose(ev.EvalGainExponent(0.5), -0.5, 1e-6, "hand-computed Hermite midpoint");
  checkClose(ev.EvalGainExponent(0.0), 0.0, 1e-9, "gain exponent at the first control point");
  checkClose(ev.EvalGainExponent(1.0), -1.0, 1e-9, "gain exponent at the last control point");

  // Below the first control point the annex holds G at y[0].
  checkClose(ev.EvalGainExponent(-0.25), 0.0, 1e-9, "G is constant below the first control point");

  // Above the last, G = y_last + log2(x_last / x), which makes gain*x a
  // constant - a hard clip at 2^-1 * 1.0 = 0.5.
  int i;
  for (i = 1; i <= 8; i++) {
    double xx = 1.0 + 0.5 * i;
    double g = (double)ev.EvalGainExponent((icFloatNumber)xx);
    checkClose(pow(2.0, g) * xx, 0.5, 1e-6, "the extrapolation region is a hard clip");
  }

  // Target on the baseline: the Zero Color Gain Function, i.e. the identity.
  check(ev.SetTargetHeadroom(2.0), "target headroom set to the baseline");
  check(ev.IsIdentity(), "a target on the baseline is the identity");
  checkClose(ev.EvalGainExponent(0.5), 0.0, 1e-9, "the baseline gain exponent is zero");

  // Halfway between them in log2 space. The annex's weights are
  //   W_alt = (1 - 2)/(0 - 2) = 0.5,  W_base = (1 - 0)/(2 - 0) = 0.5
  // and they weight the *exponents*, so the midpoint of a -0.5 exponent and a
  // 0.0 exponent is -0.25 - a gain of 0.8409, not the 0.8536 that averaging
  // the two gains would give.
  check(ev.SetTargetHeadroom(1.0), "target headroom set between the two curves");
  checkClose(ev.EvalGainExponent(0.5), -0.25, 1e-6, "headroom blend interpolates the exponent");
  checkClose(pow(2.0, (double)ev.EvalGainExponent(0.5)), 0.840896, 1e-5,
             "the blended gain is not the average of the two gains");

  // Outside the list in either direction, the nearest curve alone.
  check(ev.SetTargetHeadroom((icFloatNumber)-3.0), "target headroom below the list");
  checkClose(ev.EvalGainExponent(0.5), -0.5, 1e-6, "below the list clamps to the lowest curve");
  check(ev.SetTargetHeadroom((icFloatNumber)9.0), "target headroom above the list");
  checkClose(ev.EvalGainExponent(0.5), 0.0, 1e-9, "above the list clamps to the highest curve");
}

// ---------------------------------------------------------------------------
// 4. Component mixing and the shape of Apply()
// ---------------------------------------------------------------------------
void testComponentMixing()
{
  icFloatNumber x[2] = { 0.0, 1.0 };
  icFloatNumber y[2] = { 0.0, -1.0 };
  icFloatNumber m[2] = { 0.0, 0.0 };

  icHagcMetadata meta;
  CIccHagcEvaluator ev;
  icFloatNumber src[3], dst[3];

  // Mixing type 0 takes the maximum component, so one gain drives all three
  // and the ratios between channels are preserved exactly - which is what
  // clause 8.10.2 b) means by "shall preserve the RGB primaries".
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  check(ev.Init(meta), "evaluator accepts the max-mixing curve");
  check(ev.SetTargetHeadroom(0.0), "target set");

  src[0] = (icFloatNumber)0.5;
  src[1] = (icFloatNumber)0.25;
  src[2] = (icFloatNumber)0.125;
  ev.Apply(dst, src);

  double gain = pow(2.0, (double)ev.EvalGainExponent((icFloatNumber)0.5));
  checkClose(dst[0], 0.5 * gain, 1e-6, "max mixing: R scaled by the gain at the maximum");
  checkClose(dst[1], 0.25 * gain, 1e-6, "max mixing: G scaled by the same gain");
  checkClose(dst[2], 0.125 * gain, 1e-6, "max mixing: B scaled by the same gain");
  checkClose(dst[1] / dst[0], 0.5, 1e-6, "max mixing preserves channel ratios");

  check(ev.IsInvertible(), "a common-gain curve is invertible");

  icFloatNumber back[3];
  check(ev.Invert(back, dst), "invert the max-mixing result");
  checkClose(back[0], 0.5, 1e-5, "inverse recovers R");
  checkClose(back[1], 0.25, 1e-5, "inverse recovers G");
  checkClose(back[2], 0.125, 1e-5, "inverse recovers B");

  // Mixing type 1 is per component, so each channel gets its own gain and the
  // ratios do change. This is the case whose inverse is per channel.
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingComponent, x, y, m, 2);
  check(ev.Init(meta), "evaluator accepts the component-mixing curve");
  check(ev.SetTargetHeadroom(0.0), "target set");

  ev.Apply(dst, src);

  checkClose(dst[0], 0.5 * pow(2.0, (double)ev.EvalGainExponent((icFloatNumber)0.5)), 1e-6,
             "component mixing: R uses the gain at R");
  checkClose(dst[1], 0.25 * pow(2.0, (double)ev.EvalGainExponent((icFloatNumber)0.25)), 1e-6,
             "component mixing: G uses the gain at G");

  check(ev.IsInvertible(), "a component-mixing curve is invertible per channel");
  check(ev.Invert(back, dst), "invert the component-mixing result");
  checkClose(back[0], 0.5, 1e-5, "per-channel inverse recovers R");
  checkClose(back[1], 0.25, 1e-5, "per-channel inverse recovers G");
  checkClose(back[2], 0.125, 1e-5, "per-channel inverse recovers B");

  // Above the last control point the forward map is constant, so the inverse
  // cannot recover the input - it saturates at the control point instead of
  // running the bisection off the end of its bracket.
  src[0] = src[1] = src[2] = (icFloatNumber)8.0;
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  check(ev.Init(meta), "evaluator re-initialised");
  check(ev.SetTargetHeadroom(0.0), "target set");
  ev.Apply(dst, src);
  checkClose(dst[0], 0.5, 1e-6, "everything above the last control point clips to one value");
  check(ev.Invert(back, dst), "the saturated inverse still returns a value");
  checkClose(back[0], 1.0, 1e-4, "the saturated inverse returns the last control point");
}

// ---------------------------------------------------------------------------
// 5. The PCHIP slope reconstruction
// ---------------------------------------------------------------------------
void testPchipSlopes()
{
  /* Sized to the tag's own maximum, not to the largest case below.  This was
   * icFloatNumber[4] and the triple-abscissa case further down asks for FIVE
   * slopes, so the library wrote one element past the end - a stack-buffer
   * overflow WRITE that ASan catches and that corrupted whatever the compiler
   * placed next, meaning every assertion after it was reading disturbed stack.
   * icHagcDerivePchipSlopes() writes exactly n entries, so the buffer has to
   * be the largest n the tag can carry, not the largest n this function
   * currently passes. */
  icFloatNumber slope[icHagcMaxControlPoints];

  // Monotonically decreasing data: every derived slope must be non-positive,
  // or the interpolant overshoots and stops being monotone - which is the one
  // property the inverse path depends on.
  icFloatNumber xd[4] = { 0.0, 1.0, 2.0, 3.0 };
  icFloatNumber yd[4] = { 0.0, (icFloatNumber)-0.5, (icFloatNumber)-0.8, (icFloatNumber)-0.9 };

  check(icHagcDerivePchipSlopes(xd, yd, 4, slope), "PCHIP slopes derived for monotone data");
  check(slope[0] <= 0.0 && slope[1] <= 0.0 && slope[2] <= 0.0 && slope[3] <= 0.0,
        "PCHIP slopes of decreasing data are all non-positive");

  // A local extremum: the two adjacent secants have opposite signs, so the
  // slope there is zero by the Fritsch-Carlson rule. A central-difference
  // estimate would give a non-zero slope and overshoot.
  icFloatNumber xe[3] = { 0.0, 1.0, 2.0 };
  icFloatNumber ye[3] = { 0.0, 1.0, 0.0 };

  check(icHagcDerivePchipSlopes(xe, ye, 3, slope), "PCHIP slopes derived across an extremum");
  checkClose(slope[1], 0.0, 0.0, "PCHIP slope at a local extremum is zero");

  // Two points: the segment is the straight line through them.
  icFloatNumber x2[2] = { 0.0, 2.0 };
  icFloatNumber y2[2] = { 0.0, 1.0 };
  check(icHagcDerivePchipSlopes(x2, y2, 2, slope), "PCHIP slopes derived for two points");
  checkClose(slope[0], 0.5, 1e-9, "two-point PCHIP slope is the secant");
  checkClose(slope[1], 0.5, 1e-9, "two-point PCHIP slope is the secant at both ends");

  // The interior formula of SMPTE ST 2094-50 C.3.9, to the value, on a
  // deliberately NON-uniform grid: with h0 = 1 and h1 = 2 the interval
  // weighting is the whole content of the formula, and an unweighted mean of
  // the two secants (the obvious wrong implementation) gives 1.25 here rather
  // than 6/7. s0 = 2, s1 = 0.5, so
  //   3(h0 + h1) s0 s1 / ((2h0 + h1) s0 + (h0 + 2h1) s1) = 9 / 10.5.
  icFloatNumber xw[3] = { 0.0, 1.0, 3.0 };
  icFloatNumber yw[3] = { 0.0, 2.0, 3.0 };
  check(icHagcDerivePchipSlopes(xw, yw, 3, slope), "PCHIP slopes derived on a non-uniform grid");
  checkClose(slope[1], 9.0 / 10.5, 1e-6, "C.3.9's interval-weighted harmonic mean");
  checkClose(slope[0], 2.5, 1e-6, "C.3.9's one-sided estimate at the first point");

  // The endpoint limit of the published C.3.9. The same data is monotonically
  // INCREASING, yet the three-point estimate gives -0.5 at the last point: the
  // final segment would dip below the value it starts from. "If sign(m_i) !=
  // sign(s_i-1) then let m_i = 0" is what prevents it.
  //
  // This assertion was written against the second public committee draft,
  // which had the estimate and NEITHER limit, and it read "the end slope is
  // clamped, not the draft's -0.5". It was a deliberate divergence then,
  // justified only by the draft's own NOTE claiming PCHIP equivalence. The
  // published standard added both limits (HAGC-07 problem 1, fixed), so the
  // same number is now plain conformance.
  checkClose(slope[2], 0.0, 0.0, "the end slope is limited to zero on increasing data");

  // Three collinear flat points. Both secants are zero, so a condition written
  // only as a sign comparison takes the "otherwise" branch and forms 0/0. The
  // published Formula (C.8) adds "or s_i-1 = s_i = 0" for exactly this
  // (HAGC-07 problem 2, fixed); the draft did not have it.
  icFloatNumber xf[3] = { 0.0, 1.0, 2.0 };
  icFloatNumber yf[3] = { 0.0, 0.0, 0.0 };
  check(icHagcDerivePchipSlopes(xf, yf, 3, slope), "PCHIP slopes derived for a flat curve");
  check(slope[0] == 0.0 && slope[1] == 0.0 && slope[2] == 0.0,
        "a flat pair gives zero, per Formula (C.8)'s second condition");

  // A DUPLICATED ABSCISSA, which 6.5.2 permits when the two Y values agree.
  // The draft's C.3.9 could not process it - its secant was an unguarded
  // division - and this implementation refused such a curve outright. The
  // published clause defines s_i as zero on a zero-width interval (Formula
  // C.7) and classifies each control point by the strict inequalities between
  // its neighbours, so the duplicate is a DEGENERATE point and gets zero while
  // its neighbours keep the slopes they would have had.
  // its neighbours keep the slopes they would have had.
  //
  // Note which case each point falls into, because it is not the obvious one:
  // for x = {0, 1, 1, 2} the two duplicated points are NOT degenerate. Point 1
  // has a non-zero-width interval on its left and none on its right, so it is a
  // RIGHT control point; point 2 is the mirror, a LEFT control point. Each has
  // only one usable interval, so each takes the two-point difference and gets
  // the secant of the interval it does have - 0.5, not zero. A point is
  // degenerate only when BOTH neighbouring intervals have zero width.
  icFloatNumber xd2[4] = { 0.0, 1.0, 1.0, 2.0 };
  icFloatNumber yd2[4] = { 0.0, (icFloatNumber)0.5, (icFloatNumber)0.5, 1.0 };
  check(icHagcDerivePchipSlopes(xd2, yd2, 4, slope), "a duplicated abscissa is accepted");
  checkClose(slope[1], 0.5, 1e-9, "the left duplicate takes the two-point difference");
  checkClose(slope[2], 0.5, 1e-9, "and so does the right one");
  check(slope[0] > 0.0 && slope[3] > 0.0, "the outer points keep a positive slope");

  // A genuinely degenerate point: three identical abscissae, so the middle one
  // has zero-width intervals on both sides and the clause assigns it zero.
  icFloatNumber xd3[5] = { 0.0, 1.0, 1.0, 1.0, 2.0 };
  icFloatNumber yd3b[5] = { 0.0, (icFloatNumber)0.5, (icFloatNumber)0.5,
                            (icFloatNumber)0.5, 1.0 };
  check(icHagcDerivePchipSlopes(xd3, yd3b, 5, slope), "a triple abscissa is accepted");
  checkClose(slope[2], 0.0, 0.0, "a point with no interval on either side is degenerate");

  // The same shape with DISAGREEING Y is not degenerate but ambiguous - the
  // curve would have two values at one abscissa - and 6.5.2 forbids it.
  icFloatNumber ydBad[4] = { 0.0, (icFloatNumber)0.5, (icFloatNumber)0.7, 1.0 };
  check(!icHagcDerivePchipSlopes(xd2, ydBad, 4, slope),
        "a duplicated abscissa with differing Y is refused");

  // X going backwards is a decode error, not something to interpolate over.
  icFloatNumber xb[3] = { 0.0, 1.0, (icFloatNumber)0.5 };
  icFloatNumber yb[3] = { 0.0, 1.0, 2.0 };
  check(!icHagcDerivePchipSlopes(xb, yb, 3, slope), "PCHIP refuses decreasing X");

  // A curve that leaves its slopes to be derived must report that it did.
  icFloatNumber x[3] = { 0.0, (icFloatNumber)0.5, 1.0 };
  icFloatNumber y[3] = { 0.0, (icFloatNumber)-0.4, (icFloatNumber)-1.0 };

  icHagcMetadata meta;
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, NULL, 3);

  CIccHagcEvaluator ev;
  check(ev.Init(meta), "evaluator accepts a PCHIP-slope curve");
  check(ev.UsesDerivedSlopes(), "the evaluator reports the derived slopes");
  check(ev.SetTargetHeadroom(0.0), "target set");
  checkClose(ev.EvalGainExponent((icFloatNumber)0.5), -0.4, 1e-6,
             "a derived-slope curve still passes through its control points");
}

// ---------------------------------------------------------------------------
// 6. Configurations the evaluator must decline rather than guess at
// ---------------------------------------------------------------------------
void testUnsupportedConfigurations()
{
  icFloatNumber x[2] = { 0.0, 1.0 };
  icFloatNumber y[2] = { 0.0, -1.0 };
  icFloatNumber m[2] = { 0.0, 0.0 };

  icHagcMetadata meta;
  CIccHagcEvaluator ev;

  // The Headroom Adaptive Tone Map flag clear means the tag carries no tone
  // mapping metadata at all (proposal 1.2.2.2).
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  meta.m_bHeadroomAdaptiveToneMap = false;
  check(!ev.Init(meta), "a tag with no tone mapping metadata is declined");

  // Metadata that never decoded.
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  meta.m_bUnpacked = false;
  check(!ev.Init(meta), "undecoded metadata is declined");

  // Non-increasing control point X. Validate() reports it; the evaluator must
  // not divide by the zero interval width it implies.
  icFloatNumber xbad[3] = { 0.0, (icFloatNumber)0.5, (icFloatNumber)0.5 };
  icFloatNumber ybad[3] = { 0.0, (icFloatNumber)-0.5, (icFloatNumber)-1.0 };
  icFloatNumber mbad[3] = { 0.0, 0.0, 0.0 };
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, xbad, ybad, mbad, 3);
  check(!ev.Init(meta), "non-increasing control point X is declined");

  // No alternate images at all is a *supported* configuration with a meaning
  // of its own (proposal 1.2.2.6): no tone mapping, and clamp the baseline to
  // the target colour volume.
  meta.Reset();
  meta.m_bHeadroomAdaptiveToneMap = true;
  meta.m_bUnpacked = true;
  meta.m_baselineHeadroom = (icFloatNumber)2.0;
  check(ev.Init(meta), "no alternate images is supported");
  check(ev.ClampsToTargetVolume(), "no alternate images means clamp to the target volume");
  check(ev.IsIdentity(), "no alternate images applies no gain");

  // All-zero mixing coefficients would make p_sum zero, which annex 1 note 3
  // forbids and which would otherwise divide by zero on every pixel.
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  memset(meta.GetAlternate(0)->m_coef, 0, sizeof(meta.GetAlternate(0)->m_coef));
  check(!ev.Init(meta), "an all-zero coefficient set is declined");
}

// ---------------------------------------------------------------------------
// 6b. Reference White Tone Mapping: the alternates the file does not carry
// ---------------------------------------------------------------------------
//
// With this flag set the tag contains NO alternate images - four header fields
// are zeroed and the records are absent - so everything asserted here was
// computed from the baseline headroom alone, per SMPTE ST 2094-50 C.3.8. The
// expected values below were derived independently from that clause's formulae
// rather than captured from this implementation's output.
void testReferenceWhiteToneMap()
{
  icHagcAlternateImage alt[2];
  icUInt8Number n = 99;

  // Baseline headroom zero: C.3.8's first branch produces no alternates, which
  // lands on the same defined no-tone-mapping case as an ordinary tag with
  // none. Distinct from a failure, and the return value says so.
  check(icHagcDeriveReferenceWhiteToneMap(0.0, alt, n), "a zero baseline headroom derives");
  check(n == 0, "a zero baseline headroom yields no alternate images");

  // A negative headroom is not a headroom.
  n = 99;
  check(!icHagcDeriveReferenceWhiteToneMap((icFloatNumber)-1.0, alt, n),
        "a negative baseline headroom is refused");
  check(n == 99, "the refused call left the count untouched");

  // H = 2.0, chosen because it is strictly inside the clamp at
  // log2(1000/203) = 2.30 stops: every value below still depends on it.
  n = 0;
  check(icHagcDeriveReferenceWhiteToneMap((icFloatNumber)2.0, alt, n), "H = 2 derives");
  check(n == 2, "a positive baseline headroom yields exactly two alternate images");

  // u = 2 / log2(1000/203) = 0.868589, so the second headroom is
  // log2(8/3) * u = 1.230228 and the first is zero by definition.
  checkClose(alt[0].m_headroom, 0.0, 0.0, "the first alternate sits at headroom zero");
  checkClose(alt[1].m_headroom, 1.230228, 1e-5, "the second is log2(8/3) * u");

  // Max mixing, which is k_max = 1 with every other coefficient zero.
  check(alt[0].m_nMixingType == icHagcMixingMax, "the derived mixing is max");
  checkClose(alt[0].m_coef[icHagcCoefMax], 1.0, 1e-9, "k_max is one");
  checkClose(alt[0].m_coef[icHagcCoefRed], 0.0, 0.0, "k_red is zero");
  checkClose(alt[0].m_coef[icHagcCoefComponent], 0.0, 0.0, "k_component is zero");

  check(alt[0].m_nControlPoints == 8, "eight control points");
  check(!alt[0].m_bPchipSlope, "the slopes come from the Bezier, not from the PCHIP path");

  // The curve runs from the knee at relative linear white to the maximum at
  // 2^H, so the first X is 1 and the last is 4. The endpoints are where the
  // construction is checkable without reproducing the whole Bezier: the gain
  // at the knee is log2(y_white,0) and at the maximum is log2(2^0 / 2^H) = -H.
  checkClose(alt[0].m_x[0], 1.0, 1e-6, "alt 0 starts at relative linear white");
  checkClose(alt[0].m_x[7], 4.0, 1e-6, "alt 0 ends at 2^H");
  checkClose(alt[0].m_y[0], -0.822906, 1e-5, "alt 0 knee gain is log2(1 - u/2)");
  checkClose(alt[0].m_y[7], -2.0, 1e-5, "alt 0 maps 2^H back to 1.0, i.e. a gain of -H");

  checkClose(alt[1].m_x[0], 1.0, 1e-6, "alt 1 starts at relative linear white");
  checkClose(alt[1].m_x[7], 4.0, 1e-6, "alt 1 ends at 2^H");
  checkClose(alt[1].m_y[0], 0.0, 1e-6, "alt 1's knee is unity gain, its white being 1.0");
  checkClose(alt[1].m_y[7], -0.769772, 1e-5, "alt 1 ends at H_alt,1 - H");

  // Two interior points, one per alternate, so a change to the Bezier or to
  // kappa cannot pass by moving only the ends.
  checkClose(alt[0].m_x[3], 1.795834, 1e-5, "alt 0 interior X");
  checkClose(alt[0].m_y[3], -1.196578, 1e-5, "alt 0 interior gain");
  checkClose(alt[0].m_slope[3], -0.495833, 1e-5, "alt 0 interior slope");
  checkClose(alt[1].m_x[5], 2.887725, 1e-5, "alt 1 interior X");
  checkClose(alt[1].m_y[5], -0.498631, 1e-5, "alt 1 interior gain");
  checkClose(alt[1].m_slope[5], -0.265428, 1e-5, "alt 1 interior slope");

  // X strictly increasing and the slope at the knee zero - the Bezier starts
  // tangent to the identity there, which is what makes the knee a knee.
  int j;
  for (j = 1; j < 8; j++) {
    check(alt[0].m_x[j] > alt[0].m_x[j - 1], "alt 0 X is strictly increasing");
    check(alt[1].m_x[j] > alt[1].m_x[j - 1], "alt 1 X is strictly increasing");
  }
  checkClose(alt[0].m_slope[0], 0.0, 1e-6, "the curve leaves the knee with zero gain slope");

  // Above the clamp every derived value stops depending on the baseline, which
  // is why the fixture uses 2.0 and not 3.0.
  icHagcAlternateImage hi[2], hi2[2];
  icUInt8Number nh = 0, nh2 = 0;
  check(icHagcDeriveReferenceWhiteToneMap((icFloatNumber)3.0, hi, nh), "H = 3 derives");
  check(icHagcDeriveReferenceWhiteToneMap((icFloatNumber)6.0, hi2, nh2), "H = 6 derives");
  checkClose(hi[1].m_headroom, 1.415037, 1e-5, "above 2.30 stops the alternate headroom clamps");
  checkClose(hi2[1].m_headroom, hi[1].m_headroom, 1e-6, "and stays clamped");
  check(fabs(hi[0].m_y[7] - hi2[0].m_y[7]) > 1.0,
        "the curves still differ, because the maximum still tracks 2^H");

  // The evaluator accepts the mode now, and says the numbers were derived.
  icFloatNumber x[2] = { 0.0, 1.0 };
  icFloatNumber y[2] = { 0.0, (icFloatNumber)-1.0 };
  icFloatNumber m[2] = { 0.0, 0.0 };
  icHagcMetadata meta;
  CIccHagcEvaluator ev;

  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  meta.m_bReferenceWhiteToneMapping = true;
  check(ev.Init(meta), "Reference White Tone Mapping is supported");
  check(ev.UsesDerivedReferenceWhiteToneMap(), "and reports that its alternates were derived");
  check(ev.GetUnsupportedReason() == NULL, "with no unsupported reason left over");

  // The alternates the metadata object happens to carry are NOT used: in this
  // mode the file has none, and a tag that somehow held some must still be
  // evaluated from the derivation.
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 2);
  CIccHagcEvaluator plain;
  check(plain.Init(meta), "the same metadata without the flag is supported");
  check(!plain.UsesDerivedReferenceWhiteToneMap(), "and reports nothing derived");

  meta.m_bReferenceWhiteToneMapping = true;
  CIccHagcEvaluator flagged;
  check(flagged.Init(meta), "with the flag set it is still supported");
  check(flagged.SetTargetHeadroom((icFloatNumber)1.0) &&
        plain.SetTargetHeadroom((icFloatNumber)1.0), "both evaluators take a target headroom");
  check(!flagged.IsIdentity(), "the derived curves apply a gain");
}

// ---------------------------------------------------------------------------
// 6c. The gain application colour space (SMPTE ST 2094-50 Annex A)
// ---------------------------------------------------------------------------
//
// PROVISIONAL, and the test says so as loudly as the code does: this pins an
// informative annex of a committee draft against silence in the ICC amendment
// (HAGC-10). What it can pin properly is that the machinery is off by default,
// that it is an exact no-op for the identity, and that the forward and inverse
// directions agree - none of which depends on the annex being right.
void testGainApplicationSpace()
{
  icFloatNumber x[3] = { 0.0, (icFloatNumber)0.5, (icFloatNumber)4.0 };
  icFloatNumber y[3] = { 0.0, (icFloatNumber)-0.3, (icFloatNumber)-1.0 };
  icFloatNumber m[3] = { 0.0, 0.0, 0.0 };

  icHagcMetadata meta;
  buildMetadata(meta, (icFloatNumber)2.0, (icFloatNumber)0.0, icHagcMixingMax, x, y, m, 3);

  icFloatNumber src[3] = { (icFloatNumber)0.8, (icFloatNumber)0.4, (icFloatNumber)0.2 };
  icFloatNumber plain[3], converted[3];

  CIccHagcEvaluator ev;
  check(ev.Init(meta), "evaluator initialised");
  check(!ev.UsesGainApplicationSpace(), "no conversion until a caller asks for one");
  check(ev.SetTargetHeadroom((icFloatNumber)1.0), "target headroom set");
  ev.Apply(plain, src);

  // The identity matrix must be indistinguishable from no matrix at all,
  // bit for bit: if it is not, the conversion is doing something on its own.
  icFloatNumber ident[9] = { 1, 0, 0,  0, 1, 0,  0, 0, 1 };
  check(ev.SetGainApplicationMatrix(ident), "the identity installs");
  check(ev.UsesGainApplicationSpace(), "and is reported as installed");
  ev.Apply(converted, src);
  check(converted[0] == plain[0] && converted[1] == plain[1] && converted[2] == plain[2],
        "an identity conversion changes nothing at all");

  // A singular matrix has no inverse, so there is no way back out of the gain
  // space; refusing is the only correct answer, and the state must be cleared
  // rather than left half set.
  icFloatNumber singular[9] = { 1, 2, 3,  2, 4, 6,  1, 1, 1 };
  check(!ev.SetGainApplicationMatrix(singular), "a singular matrix is refused");
  check(!ev.UsesGainApplicationSpace(), "and leaves no conversion behind");
  ev.Apply(converted, src);
  check(converted[0] == plain[0], "a refused matrix leaves the evaluator as it was");

  // A real conversion: BT.709 to BT.2020, which is what a BT.709 profile
  // carrying chromaticities mode 2 would install. The result must differ -
  // that is the whole point of the annex - and the inverse must undo it.
  icCicpPrimaries p709, p2020;
  check(icGetCicpPrimaries(1, p709) && icGetCicpPrimaries(9, p2020), "primaries resolved");

  icFloatNumber conv[9];
  check(icBuildPrimariesConversionMatrix(p709, p2020, conv), "conversion matrix built");
  check(ev.SetGainApplicationMatrix(conv), "the conversion installs");
  ev.Apply(converted, src);

  check(fabs(converted[0] - plain[0]) > 1e-4 || fabs(converted[1] - plain[1]) > 1e-4,
        "applying the gain in another space gives a different answer");

  // Round trip. The inverse searches in the gain space and converts back, so
  // this fails if either leg uses the wrong matrix or the wrong direction.
  if (ev.IsInvertible()) {
    icFloatNumber back[3];
    check(ev.Invert(back, converted), "the converted result inverts");
    checkClose(back[0], src[0], 1e-3, "round trip recovers R");
    checkClose(back[1], src[1], 1e-3, "round trip recovers G");
    checkClose(back[2], src[2], 1e-3, "round trip recovers B");
  }

  // Neutral in, neutral out: white is white in both spaces, so an achromatic
  // input must stay achromatic however the gain is applied. A transposed or
  // mis-scaled matrix breaks this while still looking plausible.
  icFloatNumber grey[3] = { (icFloatNumber)0.6, (icFloatNumber)0.6, (icFloatNumber)0.6 };
  ev.Apply(converted, grey);
  checkClose(converted[0], converted[1], 1e-5, "a neutral stays neutral, R against G");
  checkClose(converted[1], converted[2], 1e-5, "a neutral stays neutral, G against B");
}

// ---------------------------------------------------------------------------
// 7. End to end through the CMM
// ---------------------------------------------------------------------------

CIccProfile *openFixture(const char *szName)
{
  std::string path = "Testing/HDR/";
  path += szName;

  CIccProfile *pProfile = ReadIccProfile(path.c_str());
  if (!pProfile) {
    g_skips++;
    printf("SKIP: cannot open %s (run Testing/CreateAllProfiles.sh)\n", path.c_str());
  }

  return pProfile;
}

// Build a device-to-PCS xform for a fixture, with or without the HDR hint,
// and evaluate one pixel through it. Returns false when the xform could not be
// created or begun, which every caller treats as a failure of its own.
bool applyPixel(const char *szFixture, const CIccCreateHdrXformHint *pHint,
                const icFloatNumber *src, icFloatNumber *dst, icXformType *pType,
                bool bInput = true)
{
  CIccProfile *pProfile = openFixture(szFixture);

  if (!pProfile)
    return false;

  CIccCreateXformHintManager hints;
  CIccCreateHdrXformHint *pOwned = NULL;

  if (pHint) {
    // The manager owns and deletes what it is given, so the hint handed to it
    // has to be a copy rather than the caller's stack object.
    pOwned = new CIccCreateHdrXformHint(*pHint);
    hints.AddHint(pOwned);
  }

  CIccXform *pXform = CIccXform::Create(pProfile, bInput, icRelativeColorimetric, icInterpLinear,
                                        NULL, icXformLutColor, true, pHint ? &hints : NULL);

  if (!pXform) {
    printf("FAIL: no xform created for %s\n", szFixture);
    return false;
  }

  if (pType)
    *pType = pXform->GetXformType();

  icStatusCMM status = pXform->Begin();

  if (status != icCmmStatOk) {
    printf("FAIL: Begin() returned %d for %s\n", (int)status, szFixture);
    delete pXform;
    return false;
  }

  CIccApplyXform *pApply = pXform->GetNewApply(status);

  if (!pApply || status != icCmmStatOk) {
    printf("FAIL: GetNewApply() failed for %s\n", szFixture);
    delete pApply;
    delete pXform;
    return false;
  }

  pXform->Apply(pApply, dst, src);

  delete pApply;
  delete pXform;

  return true;
}

// The classifier has to work on a profile that was *opened* rather than read.
//
// This is not a hypothetical. CIccCmm opens every profile it is given a path
// to, which leaves the tag directory populated and the tag objects unloaded
// until something asks for them. CIccProfile::FindTagConst() answers only from
// what is already loaded, so a classifier built on it reports "no cicpTag" for
// a profile that plainly has one - and reports it silently, and only on the
// lazy path, so a test that reads its fixtures eagerly (as every other test
// here does, through ReadIccProfile) cannot see it. It was found by running
// the CLI, not by running the suite, which is exactly why it is pinned here.
void testLazyLoadedProfile()
{
  std::string path = "Testing/HDR/HagcDisplay.icc";

  CIccProfile *pOpened = OpenIccProfile(path.c_str());

  if (!pOpened) {
    g_skips++;
    printf("SKIP: cannot open %s (run Testing/CreateAllProfiles.sh)\n", path.c_str());
    return;
  }

  icHdrProfileInfo info;

  check(icGetHdrProfileInfo(pOpened, info), "classify an opened (not read) profile");
  check(info.bHasCicp, "an opened profile's cicpTag is found");
  checkClose(info.nTransferCharacteristics, icCicpTransferPQ, 0.0,
             "an opened profile's TransferCharacteristics is read");
  check(info.nClass == icHdrProfileConforming, "an opened profile classifies as conforming");

  // The same failure hides a second value: the HAGC tag's own custom HDR
  // reference white. Unloaded, it falls back to the 203 default and every gain
  // curve is then evaluated at the wrong x.
  checkClose(info.contentReferenceWhite, 300.0, 1e-3,
             "an opened profile's custom HDR reference white is read");

  delete pOpened;
}

// The gain application space resolved from a real profile, which is the only
// way to exercise icHagcApplyGainApplicationSpace()'s two resolutions at once.
void testGainApplicationSpaceFromProfile()
{
  // HagcRefWhiteToneMap sets the Reference White Tone Mapping flag, and
  // proposal 1.2.2.5 ZEROES the chromaticities mode on the wire along with the
  // three other fields. Reading that zero at face value resolves mode 0 -
  // BT.709 - where C.3.8 assigns the gain application chromaticities itself,
  // and assigns BT.2020. The fixture's own primaries are BT.2020, so the
  // correct answer is that no conversion is needed at all; a reader that took
  // the zeroed field would install a BT.2020-to-BT.709 conversion here and
  // tone map every such profile in the wrong space.
  CIccProfile *pProfile = openFixture("HagcRefWhiteToneMap.icc");

  if (pProfile) {
    CIccTag *pTag = pProfile->FindTag(icSigHeadroomAdaptiveGainCurveTag);

    if (pTag && pTag->GetType() == icSigHeadroomAdaptiveGainCurveType) {
      CIccTagHagc *pHagc = (CIccTagHagc*)pTag;
      CIccHagcEvaluator ev;

      check(ev.Init(pHagc->GetMetadata()), "the derived-alternates fixture initialises");
      check(icHagcApplyGainApplicationSpace(ev, pProfile, pHagc->GetMetadata()),
            "its gain application space resolves");
      check(!ev.UsesGainApplicationSpace(),
            "C.3.8's BT.2020 chromaticities are used, not the zeroed mode field");
    }

    delete pProfile;
  }

  // A profile whose own primaries differ from the mode its tag declares is the
  // case the conversion exists for, and one is already in the corpus.
  pProfile = openFixture("HagcMixingTypes.icc");

  if (pProfile) {
    CIccTag *pTag = pProfile->FindTag(icSigHeadroomAdaptiveGainCurveTag);

    if (pTag && pTag->GetType() == icSigHeadroomAdaptiveGainCurveType) {
      CIccTagHagc *pHagc = (CIccTagHagc*)pTag;
      CIccHagcEvaluator ev;

      if (ev.Init(pHagc->GetMetadata())) {
        check(icHagcApplyGainApplicationSpace(ev, pProfile, pHagc->GetMetadata()),
              "a differing pair resolves");
        check(ev.UsesGainApplicationSpace(),
              "and installs a conversion rather than silently skipping it");
      }
    }

    delete pProfile;
  }
}

void testEndToEnd()
{
  icFloatNumber src[3], dst[3], noHint[3];
  icXformType nType = icXformTypeUnknown;

  // The reference-white PQ code, which the chain should place at Y = 1.0
  // before the XYZ scaling - the same anchor as the transfer test, now through
  // the whole matrix/TRC path.
  icFloatNumber vWhite = icPqInverseEotf((icFloatNumber)(300.0 / icPqPeakLuminance));

  src[0] = src[1] = src[2] = vWhite;

  // HagcDisplay declares a custom HDR reference white of 300 cd/m^2, so that
  // is the luminance its own chain normalises to - not the 203 default. A
  // reader that ignored the tag's custom value would put this pixel 48% high.

  // 1. No hint: exactly the pre-amendment behaviour - except that under the
  //    29-08-2026 revision there is no longer a conventional chain to fall
  //    back TO. An HDR Profile carries no TRC tags, so a consumer that does
  //    not ask for HDR processing gets the AToB0Tag, which is precisely what
  //    8.10.1 NOTE 4 says the mandatory pair is for: "a backward-compatible
  //    HDR->SDR fallback for consumers that do not implement HDR processing".
  //    Before the revision this profile had TRC tags and this assertion read
  //    icXformTypeMatrixTRC.
  if (applyPixel("HagcDisplay.icc", NULL, src, noHint, &nType)) {
    check(nType == icXformType3DLut || nType == icXformTypeMatrixTRC,
          "without a hint an HDR profile falls back to its mandatory AToB0Tag");
  }

  // 2. With a hint at SDR target headroom: the HDR chain, tone mapped by the
  //    alternate at headroom 0.0.
  CIccCreateHdrXformHint hint;
  hint.m_targetHeadroom = 1.0;              /* SDR */
  hint.m_nPolicy = icHdrToneMapAuto;

  if (applyPixel("HagcDisplay.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr, "with a hint an HDR profile uses the tone-mapping chain");

    // The two paths must not agree: if they did, the hint would be doing
    // nothing and every other assertion here would pass vacuously.
    check(fabs(dst[1] - noHint[1]) > 1e-4, "the HDR chain changes the result");

    // Reference white through the PQ EOTF is 1.0 in reference-white-relative
    // units. The gain curve's first control point is at x = 0.25 and its
    // coefficients are kRed 0.25, kGreen 0.5, kMax 0.25 - which for a neutral
    // triplet mix to the triplet's own value, 1.0 - so the gain applied is
    // 2^G(1.0) = 2^-0.4 = 0.7579. The matrix takes a neutral 0.7579 to
    // Y = 0.7579, and XYZScale() then multiplies by 32768/65535.
    double want = 0.757858 * (32768.0 / 65535.0);
    checkClose(dst[1], want, 2e-3, "PQ reference white through the tone-mapped chain");
  }

  // 3. A brighter pixel, to prove the chain carries light above reference
  //    white rather than clamping it. PQ code 0.75 is 983 cd/m^2, i.e. 3.28x
  //    this profile's 300 cd/m^2 reference white - which the conventional
  //    chain's 2.2 TRC cannot represent at all.
  src[0] = src[1] = src[2] = (icFloatNumber)0.75;

  if (applyPixel("HagcDisplay.icc", &hint, src, dst, &nType)) {
    // 3.2765 is above the curve's last control point at x = 1.0, so the
    // extrapolation clips it: gain * x = 2^-0.4 * 1.0 = 0.757858.
    double want = 0.757858 * (32768.0 / 65535.0);
    checkClose(dst[1], want, 3e-3, "a highlight clips to the top of the authored curve");
  }

  // 4. A target headroom between two curves, exercising the blend through the
  //    whole chain rather than through the evaluator alone.
  hint.m_targetHeadroom = (icFloatNumber)4.0;   /* log2 = 2.0, between 0.0 and the 3.0 baseline */

  if (applyPixel("HagcDisplay.icc", &hint, src, dst, &nType)) {
    // W_alt = (2 - 3)/(0 - 3) = 1/3 and the baseline contributes zero, so the
    // gain exponent is a third of the alternate's. At the clipped input the
    // alternate's exponent is G(1.0) + log2(1.0/3.2765) = -0.4 - 1.7124, so
    // the blend is (1/3)*(-2.1124) = -0.70413 and the value carried through
    // is 3.2765 * 2^-0.70413 = 2.0117.
    double want = 2.0117 * (32768.0 / 65535.0);
    checkClose(dst[1], want, 2e-2, "a blended target headroom carries headroom through");

    // And it must be brighter than the SDR target, which is the entire point.
    check(dst[1] > 0.757858 * (32768.0 / 65535.0), "a higher target headroom is brighter");
  }

  // 5. The HLG fixture, whose OOTF couples the three channels.
  hint.m_targetHeadroom = 1.0;
  src[0] = src[1] = src[2] = (icFloatNumber)0.75;   /* the BT.2100 reference white signal level */

  if (applyPixel("HagcCommonParams.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr, "the HLG fixture uses the tone-mapping chain");
    check(dst[1] > 0.0, "the HLG chain produces light");
  }

  // 6. A conforming HDR profile with no tone-mapping descriptor at all. NOTE 6
  //    of 8.10.2 permits the identity operator, and the chain still differs
  //    from the conventional one because the linearisation is the analytic
  //    EOTF rather than the profile's TRC tags.
  src[0] = src[1] = src[2] = (icFloatNumber)0.75;

  if (applyPixel("HdrDisplayMetadata.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr, "a descriptor-less HDR profile still uses the HDR chain");

    // 983 cd/m^2 over the 203 default is 4.842, and with no tone mapping that
    // is what reaches the matrix. Nothing in the conventional chain can
    // produce a value that large from a 0.75 input.
    checkClose(dst[1], 4.8421 * (32768.0 / 65535.0), 5e-2,
               "the identity operator carries display-linear HDR light unchanged");
  }

  // 7. Descriptor precedence, clause 8.10.3, on the two profiles where the
  //    ranking has more than one possible outcome.
  //
  //    HdrMissingBToA0 carries both a HAGC tag and an AToB0Tag: descriptor a)
  //    against descriptor c), which the clause ranks a) first.
  hint.m_nPolicy = icHdrToneMapAuto;

  if (applyPixel("HdrMissingBToA0.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr, "the HAGC tag outranks a baked AToB0");
  }

  //    HdrBakedLut carries the AToB0/BToA0 pair and no HAGC, so the choice is
  //    between descriptor c) and this build's own operator - and 8.10.3 ranks
  //    the AToB0Tag LAST of the three. This assertion used to read the other
  //    way, on the reasoning that a rendering the author baked beats an
  //    identity; 8.10.6 then made the pair MANDATORY, so that reading would
  //    disable the 8.10.2 chain for every profile without a HAGC tag. NOTE 4
  //    settles it: the pair is the fallback "for consumers that do not
  //    implement HDR processing", which this is not.
  if (applyPixel("HdrBakedLut.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr,
          "Auto ranks the CMM's own operator above a baked AToB0, per 8.10.3");
  }

  hint.m_nPolicy = icHdrToneMapPreferHagc;

  if (applyPixel("HdrBakedLut.icc", &hint, src, dst, &nType)) {
    check(nType == icXformTypeMatrixTrcHdr, "PreferHagc overrides a baked AToB0");
  }

  //    And the converse: PreferLut takes the baked pair even from a profile
  //    that has a HAGC tag.
  hint.m_nPolicy = icHdrToneMapPreferLut;

  if (applyPixel("HdrMissingBToA0.icc", &hint, src, dst, &nType)) {
    check(nType != icXformTypeMatrixTrcHdr, "PreferLut takes the baked AToB0 over the HAGC tag");
  }

  // 8. Disable is exactly the hint-less path: whatever a consumer that never
  //    asked for HDR processing would have got, which for a revision-shaped
  //    HDR Profile is the mandatory AToB0Tag rather than a matrix/TRC chain
  //    that no longer exists.
  hint.m_nPolicy = icHdrToneMapDisable;
  src[0] = src[1] = src[2] = vWhite;

  if (applyPixel("HagcDisplay.icc", &hint, src, dst, &nType)) {
    check(nType != icXformTypeMatrixTrcHdr, "the Disable policy engages no HDR chain");
  }
}

// ---------------------------------------------------------------------------
// 8. The output direction, and its round trip
// ---------------------------------------------------------------------------
void testOutputDirection()
{
  CIccCreateHdrXformHint hint;
  hint.m_targetHeadroom = 1.0;
  hint.m_nPolicy = icHdrToneMapAuto;

  icFloatNumber src[3], pcs[3], back[3];
  icXformType nType = icXformTypeUnknown;

  src[0] = (icFloatNumber)0.6;
  src[1] = (icFloatNumber)0.55;
  src[2] = (icFloatNumber)0.5;

  // HagcCommonParams uses mixing type 0 for every alternate, so the blend at
  // any target headroom shares one mixing and the inverse exists.
  if (!applyPixel("HagcCommonParams.icc", &hint, src, pcs, &nType))
    return;

  if (!applyPixel("HagcCommonParams.icc", &hint, pcs, back, &nType, false))
    return;

  check(nType == icXformTypeMatrixTrcHdr, "the output direction uses the tone-mapping chain");

  // Device to PCS to device. The tolerance is looser than the forward tests
  // because the inverse runs a bisection and the matrix is inverted
  // numerically, but it is far tighter than any real error would be.
  checkClose(back[0], src[0], 1e-3, "device to PCS to device round trip R");
  checkClose(back[1], src[1], 1e-3, "device to PCS to device round trip G");
  checkClose(back[2], src[2], 1e-3, "device to PCS to device round trip B");
}

} // namespace

int main(int /*argc*/, char * /*argv*/[])
{
  printf("HDR tone-mapping regression (ICC.1 clause 8.10.2)\n");

  testTransferFunctions();
  testTransferNormalisation();
  testGainEvaluator();
  testComponentMixing();
  testPchipSlopes();
  testUnsupportedConfigurations();
  testReferenceWhiteToneMap();
  testGainApplicationSpace();
  testLazyLoadedProfile();
  testGainApplicationSpaceFromProfile();
  testEndToEnd();
  testOutputDirection();

  if (g_failures)
    printf("%d assertion(s) failed\n", g_failures);
  else
    printf("all assertions passed\n");

  /* 77 = ctest Skipped (see SKIP_RETURN_CODE); a real failure still wins. */
  if (!g_failures && g_skips)
    return 77;

  return g_failures;
}
