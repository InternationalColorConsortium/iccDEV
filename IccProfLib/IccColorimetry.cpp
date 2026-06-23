/** @file
    File:       IccColorimetry.cpp

    Contains:   Implementation of the spectral-to-colorimetric (XYZ) reduction
                methods recommended by the ICC colorimetry-data registry
                (ICC TN-06-2025, ICC White Paper 56, CIE 15, CIE 167:2005,
                ISO 13655 / ASTM E308). See IccColorimetry.h.

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

#include "IccColorimetry.h"
#include "IccTagBasic.h"   // CIccTagSpectralViewingConditions (built-in obs/illum tables)
#include <cmath>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//==========================================================================
// Internal numeric core. All math is performed in double precision (CIE/WP56
// note that finite precision limits achievable accuracy); the public API
// converts to/from icFloatNumber at the boundary.
//==========================================================================
namespace {

// Decompose an icSpectralRange into nm endpoints, sample count and step size.
struct Grid {
  double start;   // first wavelength (nm)
  double step;    // sample spacing (nm); 0 if a single sample
  int    n;       // number of samples
  Grid(const icSpectralRange &r) {
    start = icF16toF(r.start);
    double end = icF16toF(r.end);
    n = (int)r.steps;
    step = (n > 1) ? (end - start) / (n - 1) : 0.0;
  }
  double nm(int i) const { return start + i * step; }
};

// CIE 167:2005 Sprague end-point coefficients (Table V). Each synthetic point is
// a weighted sum (divided by 209) of the six nearest measured values. The two
// leading points use the first six samples; the two trailing points use the last
// six. Verified: a unit ramp r_i = i yields the exact off-grid values (-2,-1) and
// (n,n+1), i.e. the construction reproduces linear (and higher) data.
const double SPRAGUE_LEAD0[6] = { 884, -1960, 3033, -2648, 1080, -180 }; // two before first
const double SPRAGUE_LEAD1[6] = { 508,  -540,  488,  -367,  144,  -24 }; // one before first
const double SPRAGUE_TRAIL0[6]= { -24,   144, -367,   488, -540,  508 }; // one after last
const double SPRAGUE_TRAIL1[6]= { -180, 1080,-2648,  3033,-1960,  884 }; // two after last

// Six-term dot product of an end-point coefficient row (c) with six samples (v).
static double dot6(const double *c, const double *v) {
  return c[0]*v[0]+c[1]*v[1]+c[2]*v[2]+c[3]*v[3]+c[4]*v[4]+c[5]*v[5];
}

// Build the Sprague-extended array: [lead0, lead1, v0..v(n-1), trail0, trail1].
// Requires n >= 6. Returns false otherwise.
static bool spragueExtend(const std::vector<double> &v, std::vector<double> &ext) {
  int n = (int)v.size();
  if (n < 6)
    return false;
  const double *f = &v[0];          // first six
  const double *l = &v[n-6];        // last six
  ext.resize(n + 4);
  ext[0]     = dot6(SPRAGUE_LEAD0,  f) / 209.0;
  ext[1]     = dot6(SPRAGUE_LEAD1,  f) / 209.0;
  for (int i = 0; i < n; i++)
    ext[i+2] = v[i];
  ext[n+2]   = dot6(SPRAGUE_TRAIL0, l) / 209.0;
  ext[n+3]   = dot6(SPRAGUE_TRAIL1, l) / 209.0;
  return true;
}

// Evaluate the Sprague quintic for source segment i (0-based, base point v[i])
// at fractional position X in [0,1]. ext is the Sprague-extended array; the six
// surrounding samples r[i-2..i+3] live at ext[i..i+5].
static double spragueEval(const std::vector<double> &ext, int i, double X) {
  const double *r = &ext[i];   // r[0..5] = v[i-2 .. i+3]
  double a0 = ( 2*r[0] -16*r[1]        +16*r[3] - 2*r[4]        ) / 24.0;
  double a1 = (-1*r[0] +16*r[1] -30*r[2]+16*r[3] - 1*r[4]        ) / 24.0;
  double a2 = (-9*r[0] +39*r[1] -70*r[2]+66*r[3] -33*r[4] + 7*r[5]) / 24.0;
  double a3 = (13*r[0] -64*r[1]+126*r[2]-124*r[3]+61*r[4] -12*r[5]) / 24.0;
  double a4 = (-5*r[0] +25*r[1] -50*r[2]+50*r[3] -25*r[4] + 5*r[5]) / 24.0;
  double base = r[2];          // v[i]
  return base + X*(a0 + X*(a1 + X*(a2 + X*(a3 + X*a4))));
}

// 4-point cubic (Catmull-Rom) interpolation of equally-spaced data v.
//
// Evaluates the spline on source segment i -- between samples v[i] (the segment
// base, at X=0) and v[i+1] (at X=1) -- at fractional position X in [0,1]. A
// Catmull-Rom segment needs the two bracketing samples plus one neighbour on each
// side (v[i-1], v[i], v[i+1], v[i+2]); near the array ends those neighbour indices
// are clamped to the first/last sample so the curve degrades to a one-sided slope
// instead of reading out of bounds. This is the 3rd-order interpolant used for
// illuminant-range reconciliation (TN-06).
static double cubicEval(const std::vector<double> &v, int i, double X) {
  int n = (int)v.size();
  int im1 = i-1 < 0 ? 0 : i-1;       // v[i-1], clamped at the low end
  int ip1 = i+1 > n-1 ? n-1 : i+1;   // v[i+1], clamped at the high end
  int ip2 = i+2 > n-1 ? n-1 : i+2;   // v[i+2], clamped at the high end
  double p0 = v[im1], p1 = v[i], p2 = v[ip1], p3 = v[ip2];
  // Catmull-Rom basis (tension 1/2): coefficients of the cubic a*X^3+b*X^2+c*X+p1,
  // where p1 is the value at X=0 (segment base) and the spline passes through p2 at X=1.
  double a = -0.5*p0 + 1.5*p1 - 1.5*p2 + 0.5*p3;   // X^3 term
  double b =      p0 - 2.5*p1 + 2.0*p2 - 0.5*p3;   // X^2 term
  double c = -0.5*p0          + 0.5*p2;            // X^1 term
  return ((a*X + b)*X + c)*X + p1;                 // Horner evaluation, constant term = p1
}

// Resample equally-spaced data v (sampled on grid src) onto grid dst, writing
// dst.n values into out.
//
// For each destination wavelength, its position t is expressed in source-sample
// units (t = 0 at src.start, t = n-1 at the last source sample), then one of three
// regimes applies:
//   - t at/below the first sample or at/above the last: hold the nearest end value,
//     or (extend == Linear) linearly extrapolate from the two end samples;
//   - interior: interpolate within source segment i = floor(t) at fraction X = t-i,
//     using Sprague (CIE 167) when requested and available, else cubic, else linear.
// The 1e-9 tolerances absorb floating-point error so a t that lands a hair outside
// [0, n-1] snaps onto the end sample rather than triggering extrapolation. interp
// is assumed already downgraded by the caller when src has too few samples for the
// requested method (icSpectralResample / icComputeWeightingTable do this).
static void resampleCore(const Grid &src, const std::vector<double> &v,
                         const Grid &dst, std::vector<double> &out,
                         icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  int n = src.n;
  out.assign(dst.n, 0.0);
  // Sprague interpolation needs a 6-point-extended copy of v; build it once up
  // front, and only when Sprague is actually the active method.
  std::vector<double> ext;
  bool haveExt = (interp == icSpectralInterpSprague) && spragueExtend(v, ext);

  for (int j = 0; j < dst.n; j++) {
    double w = dst.nm(j);                           // destination wavelength (nm)
    double t = (src.step != 0.0) ? (w - src.start) / src.step : 0.0;  // position in source samples

    // Defence in depth for the (int)floor(t) cast below: callers validate ranges
    // via rangeWellFormed() (which now rejects non-finite endpoints), so t should
    // always be finite here, but a non-finite t would slip past both end-range
    // tests (every comparison with NaN is false) and reach the cast as undefined
    // behaviour (CWE-681, #2230).  Treat a degenerate position as the first
    // sample rather than casting it.
    if (!std::isfinite(t)) { out[j] = v[0]; continue; }

    if (t <= 0.0) {                                 // at/below first sample
      if (t > -1e-9) { out[j] = v[0]; continue; }   // within rounding of the first sample
      out[j] = (extend == icSpectralExtendLinear && n > 1)
                 ? v[0] + t * (v[1] - v[0]) : v[0]; // linear extrapolation or hold
      continue;
    }
    if (t >= n - 1) {                               // at/above last sample
      if (t < n - 1 + 1e-9) { out[j] = v[n-1]; continue; }  // within rounding of the last sample
      out[j] = (extend == icSpectralExtendLinear && n > 1)
                 ? v[n-1] + (t - (n-1)) * (v[n-1] - v[n-2]) : v[n-1];
      continue;
    }

    // CWE-681 (#2230): the isfinite(t) guard above already prevents a non-finite
    // t from reaching this float->int conversion, and t is in (0, n-1) here, but
    // clamp the cast operand to the valid segment range [0, n-2] right at the
    // conversion so the NaN/range check is local to the cast (a float->int cast
    // of a non-finite or out-of-range value is undefined behaviour). For any
    // valid t this is a no-op: floor(t) is already in [0, n-2]. Mirrors the
    // adjacent-guard form used in #1478.
    double tf = std::floor(t);                      // source segment index (base sample v[i])
    if (!(tf >= 0.0)) tf = 0.0;                     // also catches NaN
    else if (tf > n - 2) tf = n - 2;               // keep the i+1 neighbour in range at the top
    int i = (int)tf;
    double X = t - i;                               // fractional position within segment i
    if (interp == icSpectralInterpSprague && haveExt)
      out[j] = spragueEval(ext, i, X);
    else if (interp == icSpectralInterpCubic)
      out[j] = cubicEval(v, i, X);
    else
      out[j] = v[i] * (1.0 - X) + v[i+1] * X;       // linear (matches CIccMatrixMath::SetRange)
  }
}

// Widen n icFloatNumber samples at p into a double-precision working vector v
// (all internal spectral math runs in double; see the section banner above).
static void toDouble(const icFloatNumber *p, int n, std::vector<double> &v) {
  v.resize(n);
  for (int i = 0; i < n; i++) v[i] = (double)p[i];
}

// ---- Input-validation helpers (Tier 1 structural + Tier 2 finiteness guards) ----
//
// Note (deliberate): range.steps is intentionally NOT capped. It is an
// icUInt16Number (<= 65535), so the largest transient allocation a single call can
// drive is ~3*65535 doubles (~1.5 MB) -- bounded and short-lived. An explicit cap
// would add an arbitrary magic limit without closing a real DoS surface, so none is
// imposed at this time.

// A spectral range is well-formed if it has at least one sample and, when it has
// more than one, its endpoints are strictly increasing (so Grid::step > 0). A
// single sample (steps==1) may carry any endpoints. Rejecting inverted or
// zero-span multi-sample ranges at the API boundary keeps the per-sample resample
// loops from silently working off a negative or zero wavelength step.
static bool rangeWellFormed(const icSpectralRange &r) {
  if (!r.steps)
    return false;
  // Endpoints are icFloat16 and so can encode +/-Inf or NaN from a malformed
  // profile.  Reject any non-finite endpoint: a multi-sample range with an
  // infinite endpoint slips past the strictly-increasing check below (e.g.
  // start = -Inf with a finite end still satisfies end > start) yet yields
  // Grid::step = Inf and a per-sample position t = (w - start)/step = NaN, which
  // then reaches the (int)floor(t) cast in resampleCore() -- undefined behaviour
  // on the cast (CWE-681, #2230).  Reject it here, the module's Tier-1 boundary.
  if (!std::isfinite((double)icF16toF(r.start)) ||
      !std::isfinite((double)icF16toF(r.end)))
    return false;
  if (r.steps > 1 && !(icF16toF(r.end) > icF16toF(r.start)))
    return false;
  return true;
}

// True iff all n samples at p are finite (no NaN/Inf). Used both to reject poisoned
// spectral input at the API boundary and to reject a prepared operator that came
// out non-finite. NOTE: this checks only finiteness, never sign or magnitude --
// negative and >1 spectral values are legitimate (see icApplyWeightingTable).
static bool allFinite(const icFloatNumber *p, int n) {
  for (int i = 0; i < n; i++)
    if (!std::isfinite((double)p[i]))
      return false;
  return true;
}

} // anonymous namespace

//==========================================================================
// Registry weighting tables (registry.color.org/colorimetry-data)
//
// The static const arrays below are GENERATED from the registry's published
// 10 nm weighting-table CSVs by IccProfLib/registry/generate_colorimetry_weights.py
// (a maintainer reruns it when the registry is revised; the LWL numbers are
// provisional pending CIE TC1-101). They are not loaded at runtime -- the values
// are baked in as program data. See that script and IccProfLib/registry/data/ for
// the source snapshot and provenance.
//==========================================================================
// >>>BEGIN GENERATED WEIGHTING TABLES (do not edit by hand)
// Snapshot: registry.color.org/colorimetry-data, fetched 2026-06-21.
// CIE Y=100 scale (a perfect diffuser sums to the illuminant XYZ with Y=100);
// 380-780 nm @ 10 nm = 41 samples; consecutive Wx,Wy,Wz blocks (3*41 = 123).

// CIE 1931 2-degree standard observer, D50 daylight (~5000 K; ICC PCS illuminant).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1931D50[123] = {
  // Wx (41)
  0.00142657f, 0.00886669f, 0.05797580f, 0.19074179f, 0.74819756f, 1.60457598f,
  2.51529070f, 2.81823288f, 2.55506404f, 1.71817944f, 0.83004909f, 0.25118037f,
  0.02509422f, 0.04721854f, 0.53860083f, 1.58762110f, 2.77294643f, 4.20660488f,
  5.66208761f, 7.09999720f, 8.66498216f, 9.19970020f, 9.95812501f, 9.55383367f,
  8.09324237f, 5.84554901f, 4.19129819f, 2.54328349f, 1.51749777f, 0.82962643f,
  0.42199100f, 0.17869971f, 0.09593937f, 0.04886720f, 0.02007087f, 0.01171139f,
  0.00574291f, 0.00228511f, 0.00086644f, 0.00069781f, 0.00012192f,
  // Wy (41)
  0.00003976f, 0.00025773f, 0.00161363f, 0.00525369f, 0.02128094f, 0.06086255f,
  0.15789637f, 0.30881603f, 0.51097297f, 0.77704972f, 1.24338128f, 1.78656265f,
  2.88981640f, 4.60900260f, 6.59464749f, 8.42125990f, 9.19357616f, 9.72383450f,
  9.50376383f, 8.89025040f, 8.21051091f, 6.74678995f, 5.87844083f, 4.75107250f,
  3.58101439f, 2.39662321f, 1.63050626f, 0.95597461f, 0.56042647f, 0.30342140f,
  0.15324152f, 0.06455981f, 0.03464949f, 0.01764506f, 0.00724811f, 0.00422900f,
  0.00207384f, 0.00082517f, 0.00031288f, 0.00025198f, 0.00004403f,
  // Wz (41)
  0.00673871f, 0.04179773f, 0.27483004f, 0.90728039f, 3.58754224f, 7.81006174f,
  12.57589667f, 14.80270016f, 14.65446292f, 11.35597547f, 7.22582066f, 3.94392694f,
  2.44437902f, 1.43215625f, 0.68919990f, 0.40243332f, 0.18574314f, 0.08008682f,
  0.03462542f, 0.01861110f, 0.01554030f, 0.00990075f, 0.00776883f, 0.00311712f,
  0.00165840f, 0.00037144f, 0.00018888f, -0.00000338f, 0.00000089f, -0.00000023f,
  0.00000006f, -0.00000002f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1931 2-degree standard observer, D65 daylight (~6500 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1931D65[123] = {
  // Wx (41)
  0.00280186f, 0.01704361f, 0.09698451f, 0.31056867f, 1.16402130f, 2.40044086f,
  3.50592778f, 3.75520585f, 3.29771917f, 2.14075019f, 1.00103069f, 0.29306382f,
  0.02763093f, 0.05398913f, 0.58146536f, 1.66821094f, 2.86035728f, 4.25670190f,
  5.63227921f, 6.96053200f, 8.34471129f, 8.67680817f, 9.12038087f, 8.56856654f,
  7.11945709f, 5.04911609f, 3.52276827f, 2.11225921f, 1.22902240f, 0.65746340f,
  0.33131193f, 0.14179325f, 0.07443019f, 0.03903267f, 0.01596217f, 0.00942211f,
  0.00462884f, 0.00184687f, 0.00069173f, 0.00055950f, 0.00009819f,
  // Wy (41)
  0.00007824f, 0.00049486f, 0.00269541f, 0.00856776f, 0.03307000f, 0.09156097f,
  0.22113589f, 0.41281108f, 0.66180806f, 0.97343948f, 1.50899065f, 2.10679175f,
  3.28812204f, 5.12217321f, 7.08207368f, 8.83253786f, 9.47167272f, 9.82952481f,
  9.44651970f, 8.70875131f, 7.90061326f, 6.35650254f, 5.37927300f, 4.25919042f,
  3.14914692f, 2.06959467f, 1.37014121f, 0.79393010f, 0.45383832f, 0.24044932f,
  0.12030819f, 0.05122637f, 0.02688029f, 0.01409377f, 0.00576419f, 0.00340226f,
  0.00167149f, 0.00066690f, 0.00024978f, 0.00020204f, 0.00003546f,
  // Wz (41)
  0.01322731f, 0.08038631f, 0.45972740f, 1.47749029f, 5.58168778f, 11.68702639f,
  17.53529201f, 19.73266811f, 18.92506223f, 14.16381864f, 8.73105282f, 4.62455502f,
  2.76876350f, 1.58465153f, 0.73573218f, 0.42156632f, 0.19072838f, 0.08077718f,
  0.03434745f, 0.01822228f, 0.01494716f, 0.00932082f, 0.00710261f, 0.00278749f,
  0.00145706f, 0.00031891f, 0.00015886f, -0.00000304f, 0.00000080f, -0.00000021f,
  0.00000006f, -0.00000001f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1931 2-degree standard observer, incandescent/tungsten (illuminant A, ~2856 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1931A[123] = {
  // Wx (41)
  0.00045699f, 0.00396559f, 0.01701452f, 0.05724145f, 0.24583158f, 0.66030724f,
  0.94175544f, 1.03889049f, 1.04310658f, 0.79049653f, 0.41633999f, 0.14820872f,
  0.01567202f, 0.02833286f, 0.38750037f, 1.18749407f, 2.28823672f, 3.70229427f,
  5.48438193f, 7.56227685f, 9.73871380f, 11.64439204f, 12.81124285f, 12.78158035f,
  11.46104912f, 8.99034321f, 6.53689892f, 4.29576925f, 2.58272012f, 1.40456725f,
  0.78054535f, 0.38774928f, 0.20019968f, 0.10648008f, 0.05445935f, 0.02784181f,
  0.01355930f, 0.00670636f, 0.00341695f, 0.00188893f, 0.00038704f,
  // Wy (41)
  0.00001214f, 0.00011766f, 0.00046060f, 0.00162627f, 0.00679924f, 0.02523001f,
  0.05942205f, 0.11344247f, 0.20551872f, 0.35290669f, 0.60828067f, 1.01241915f,
  1.74891021f, 3.04657289f, 4.77801819f, 6.34522378f, 7.62485531f, 8.59410139f,
  9.25512213f, 9.49606722f, 9.26513652f, 8.56708089f, 7.56297179f, 6.36538262f,
  5.07604107f, 3.68896913f, 2.54282909f, 1.61568862f, 0.95359147f, 0.51377626f,
  0.28347276f, 0.14010962f, 0.07229877f, 0.03845032f, 0.01966628f, 0.01005398f,
  0.00489646f, 0.00242175f, 0.00123391f, 0.00068212f, 0.00013977f,
  // Wz (41)
  0.00215213f, 0.01873130f, 0.08057872f, 0.27255376f, 1.17738867f, 3.21462839f,
  4.71044964f, 5.45332659f, 5.97026227f, 5.20940932f, 3.60213267f, 2.27749070f,
  1.49336242f, 0.96304487f, 0.50483459f, 0.30498921f, 0.15649640f, 0.07128969f,
  0.03424711f, 0.01992228f, 0.01758244f, 0.01261645f, 0.01000169f, 0.00420069f,
  0.00236183f, 0.00058562f, 0.00029240f, -0.00000216f, 0.00000057f, -0.00000015f,
  0.00000004f, -0.00000001f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1931 2-degree standard observer, phosphor-converted blue LED (LED-B1, ~2700 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1931LED_B1[123] = {
  // Wx (41)
  0.00002146f, -0.00008782f, 0.00063351f, 0.00101954f, 0.03732256f, 0.24579556f,
  0.98326054f, 1.93846396f, 1.25691510f, 0.54844052f, 0.22538573f, 0.08855693f,
  0.01143808f, 0.01932704f, 0.30899091f, 1.02164198f, 2.09767842f, 3.61442580f,
  5.72682316f, 8.40598676f, 11.38230386f, 13.97566026f, 15.26650124f, 14.52401881f,
  11.88084779f, 8.19251959f, 5.06548181f, 2.73191646f, 1.31215569f, 0.55923440f,
  0.23998881f, 0.09005935f, 0.03512067f, 0.01369500f, 0.00518761f, 0.00195933f,
  0.00070319f, 0.00026103f, 0.00010167f, 0.00004229f, 0.00000696f,
  // Wy (41)
  0.00000060f, -0.00000245f, 0.00001777f, 0.00002735f, 0.00105512f, 0.00874233f,
  0.05629800f, 0.21264472f, 0.26245158f, 0.26390330f, 0.33750675f, 0.57804246f,
  1.14997643f, 2.26187248f, 3.86374020f, 5.49179040f, 7.01824296f, 8.42033705f,
  9.69645070f, 10.58373979f, 10.84737608f, 10.28638809f, 9.00474502f, 7.22148265f,
  5.25310971f, 3.35644911f, 1.96802433f, 1.02654812f, 0.48425611f, 0.20449716f,
  0.08713749f, 0.03253475f, 0.01268528f, 0.00494484f, 0.00187352f, 0.00070750f,
  0.00025395f, 0.00009426f, 0.00003672f, 0.00001527f, 0.00000251f,
  // Wz (41)
  0.00010167f, -0.00041647f, 0.00300128f, 0.00487346f, 0.17839179f, 1.19359430f,
  4.88506963f, 10.18486317f, 7.25681685f, 3.66852069f, 1.96226708f, 1.33023433f,
  1.00340899f, 0.72660713f, 0.41336008f, 0.26575251f, 0.14542693f, 0.07048571f,
  0.03616849f, 0.02226652f, 0.02059159f, 0.01516522f, 0.01187535f, 0.00473873f,
  0.00242028f, 0.00051383f, 0.00022602f, -0.00000587f, 0.00000155f, -0.00000041f,
  0.00000011f, -0.00000003f, 0.00000001f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1931 2-degree standard observer, narrow-band tri-phosphor fluorescent (F11, ~4000 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1931F11[123] = {
  // Wx (41)
  0.00211347f, -0.00759300f, 0.03753289f, 0.20977571f, -0.29331224f, 2.60787141f,
  4.72748403f, 0.85437942f, 1.59585818f, 0.61055033f, 0.43907681f, 0.36634406f,
  0.00332657f, -0.15075374f, 0.67971602f, -2.35946484f, 10.70862597f, 12.11496241f,
  -0.98267453f, 0.56203191f, 7.00777371f, 11.66964140f, -3.30537846f, 39.91781329f,
  7.27092045f, 5.45048801f, -0.02755740f, 0.82643030f, 0.22428478f, 0.09466402f,
  0.04270587f, 0.03091844f, 0.00896595f, 0.02445432f, -0.00187847f, 0.00084644f,
  -0.00006237f, 0.00008896f, 0.00002372f, 0.00001115f, 0.00000038f,
  // Wy (41)
  0.00001576f, 0.00000879f, 0.00019827f, 0.00897726f, -0.02006629f, 0.12493642f,
  0.26796600f, 0.13875871f, 0.30164757f, 0.28926754f, 0.57646336f, 2.28484672f,
  1.08691550f, -0.04880594f, 2.36121686f, -6.83257405f, 33.10643649f, 29.27496269f,
  -3.77119669f, 1.67833628f, 6.76523368f, 8.08594261f, -1.23614637f, 19.79953899f,
  3.04211000f, 2.28062513f, -0.04001457f, 0.32108916f, 0.07961901f, 0.03547200f,
  0.01527420f, 0.01124130f, 0.00321917f, 0.00883580f, -0.00067964f, 0.00030601f,
  -0.00002261f, 0.00003215f, 0.00000856f, 0.00000403f, 0.00000014f,
  // Wz (41)
  0.00969539f, -0.03442946f, 0.17178096f, 1.02163662f, -1.48062351f, 12.83498633f,
  23.48613745f, 4.73565484f, 9.03073702f, 4.10275371f, 3.70786588f, 5.37703712f,
  0.32451506f, 0.19916349f, 0.05488298f, -0.09157670f, 0.61243064f, 0.28991097f,
  -0.05580005f, 0.01677527f, 0.00972242f, 0.01199088f, 0.00092626f, 0.01298237f,
  0.00096730f, 0.00049759f, -0.00005390f, 0.00002239f, -0.00000590f, 0.00000155f,
  -0.00000041f, 0.00000011f, -0.00000003f, 0.00000001f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1964 10-degree supplementary observer, D50 daylight (~5000 K; ICC PCS illuminant).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1964D50[123] = {
  // Wx (41)
  0.00030102f, 0.00227985f, 0.05818689f, 0.38449921f, 1.08358618f, 1.61104187f,
  2.55220311f, 2.88139509f, 2.43622504f, 1.57576147f, 0.62814175f, 0.09630591f,
  0.00631526f, 0.28427234f, 0.96618050f, 2.09730926f, 3.32030626f, 4.74101424f,
  6.19366762f, 7.55569051f, 8.83023151f, 9.24268343f, 9.70424671f, 9.03326420f,
  7.45862439f, 5.43663542f, 3.70629594f, 2.21250802f, 1.28940833f, 0.71247476f,
  0.33729996f, 0.14510344f, 0.07457374f, 0.03490999f, 0.01379355f, 0.00779117f,
  0.00390999f, 0.00159996f, 0.00060438f, 0.00050166f, 0.00009096f,
  // Wy (41)
  0.00003320f, 0.00024278f, 0.00618405f, 0.03966232f, 0.11177293f, 0.19118937f,
  0.39794301f, 0.67362911f, 0.99915667f, 1.47049438f, 2.12543931f, 2.72046007f,
  3.83975108f, 5.13673314f, 6.50853774f, 7.85955544f, 8.54061416f, 8.92255098f,
  8.78030207f, 8.22233688f, 7.54249928f, 6.39249001f, 5.65749207f, 4.59628423f,
  3.44468688f, 2.37027563f, 1.53761064f, 0.88358253f, 0.50888068f, 0.27844831f,
  0.13125093f, 0.05636632f, 0.02894576f, 0.01356213f, 0.00536782f, 0.00303870f,
  0.00152939f, 0.00062780f, 0.00023791f, 0.00019822f, 0.00003610f,
  // Wz (41)
  0.00137592f, 0.00984820f, 0.26088017f, 1.75082050f, 5.13604510f, 7.92743908f,
  13.04639067f, 15.47777888f, 14.01664208f, 10.63316107f, 6.29896077f, 3.23572379f,
  1.79318758f, 0.91953749f, 0.50199798f, 0.26248249f, 0.11464729f, 0.03047096f,
  -0.00300579f, 0.00079157f, -0.00020846f, 0.00005490f, -0.00001446f, 0.00000381f,
  -0.00000100f, 0.00000026f, -0.00000007f, 0.00000002f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1964 10-degree supplementary observer, D65 daylight (~6500 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1964D65[123] = {
  // Wx (41)
  0.00049486f, 0.00469161f, 0.09732896f, 0.61608037f, 1.66000233f, 2.37709244f,
  3.51180562f, 3.78883640f, 3.10262300f, 1.93705587f, 0.74636811f, 0.11014113f,
  0.00726122f, 0.31400007f, 1.02722210f, 2.17408979f, 3.37968903f, 4.73453479f,
  6.08071267f, 7.31014930f, 8.39277327f, 8.60323036f, 8.77119713f, 7.99566964f,
  6.47580241f, 4.63467139f, 3.07392935f, 1.81373457f, 1.03059551f, 0.55732754f,
  0.26133004f, 0.11364008f, 0.05709455f, 0.02754254f, 0.01082118f, 0.00618890f,
  0.00310996f, 0.00127644f, 0.00047619f, 0.00039701f, 0.00007230f,
  // Wy (41)
  0.00005426f, 0.00050269f, 0.01033419f, 0.06356464f, 0.17118191f, 0.28288491f,
  0.54926100f, 0.88805941f, 1.27716857f, 1.81729987f, 2.54479724f, 3.16413044f,
  4.30921272f, 5.63046887f, 6.89586248f, 8.13602724f, 8.68461050f, 8.90243011f,
  8.61440764f, 7.94983283f, 7.16390970f, 5.94530290f, 5.11014384f, 4.06694909f,
  2.99004584f, 2.02030556f, 1.27499263f, 0.72432897f, 0.40670930f, 0.21781087f,
  0.10168915f, 0.04414442f, 0.02216102f, 0.01069993f, 0.00421113f, 0.00241376f,
  0.00121646f, 0.00050085f, 0.00018746f, 0.00015687f, 0.00002870f,
  // Wz (41)
  0.00224551f, 0.02044295f, 0.43610972f, 2.80668426f, 7.86897633f, 11.70201889f,
  17.95855255f, 20.35825712f, 17.86077746f, 13.08639630f, 7.50906239f, 3.74318343f,
  2.00325463f, 1.00405873f, 0.52947166f, 0.27111176f, 0.11618996f, 0.03024352f,
  -0.00299114f, 0.00078771f, -0.00020744f, 0.00005463f, -0.00001439f, 0.00000379f,
  -0.00000100f, 0.00000026f, -0.00000007f, 0.00000002f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1964 10-degree supplementary observer, incandescent/tungsten (illuminant A, ~2856 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1964A[123] = {
  // Wx (41)
  0.00000159f, 0.00152127f, 0.01758005f, 0.11837661f, 0.37211724f, 0.68554706f,
  0.98240730f, 1.09421781f, 1.02438022f, 0.74703411f, 0.32587620f, 0.06094369f,
  0.00265573f, 0.18894026f, 0.71751781f, 1.61744677f, 2.82264118f, 4.29649235f,
  6.17713360f, 8.28575714f, 10.21762341f, 12.04198766f, 12.85023719f, 12.44103075f,
  10.87146708f, 8.60452773f, 5.95029430f, 3.84688392f, 2.25828977f, 1.24198268f,
  0.64266764f, 0.32372314f, 0.16017960f, 0.07837979f, 0.03856297f, 0.01903079f,
  0.00950803f, 0.00482234f, 0.00245860f, 0.00139956f, 0.00029605f,
  // Wy (41)
  -0.00000048f, 0.00016762f, 0.00184773f, 0.01229164f, 0.03807339f, 0.08158413f,
  0.15372881f, 0.25502621f, 0.41398728f, 0.68763005f, 1.07346868f, 1.58878344f,
  2.39717125f, 3.50278757f, 4.85772092f, 6.09506952f, 7.29097483f, 8.11585825f,
  8.79902574f, 9.03975072f, 8.75708295f, 8.35008520f, 7.49161562f, 6.33747841f,
  5.02481118f, 3.75353764f, 2.46860689f, 1.53722668f, 0.89101523f, 0.48545468f,
  0.25008137f, 0.12575819f, 0.06217128f, 0.03044942f, 0.01500572f, 0.00742323f,
  0.00371870f, 0.00189193f, 0.00096776f, 0.00055317f, 0.00011744f,
  // Wz (41)
  0.00001406f, 0.00668891f, 0.07878361f, 0.53912209f, 1.76062628f, 3.37376506f,
  5.02415512f, 5.87581406f, 5.88109257f, 5.02405984f, 3.23530589f, 1.92637219f,
  1.12847619f, 0.63760552f, 0.37728326f, 0.20518788f, 0.09954676f, 0.02815021f,
  -0.00266037f, 0.00070060f, -0.00018450f, 0.00004859f, -0.00001280f, 0.00000337f,
  -0.00000089f, 0.00000023f, -0.00000006f, 0.00000002f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1964 10-degree supplementary observer, phosphor-converted blue LED (LED-B1, ~2700 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1964LED_B1[123] = {
  // Wx (41)
  0.00003071f, -0.00014968f, 0.00082066f, 0.00244244f, 0.06246108f, 0.25870111f,
  1.03581078f, 2.04999088f, 1.24014903f, 0.51953690f, 0.17646791f, 0.03823788f,
  0.00085114f, 0.13836029f, 0.58031851f, 1.40466059f, 2.61012270f, 4.22951310f,
  6.50280931f, 9.28645362f, 12.03881873f, 14.56698545f, 15.43235174f, 14.24325139f,
  11.35789079f, 7.90074852f, 4.64031267f, 2.46532680f, 1.15539053f, 0.49846976f,
  0.19841109f, 0.07596634f, 0.02819122f, 0.01011669f, 0.00370268f, 0.00134623f,
  0.00049904f, 0.00018903f, 0.00007379f, 0.00003162f, 0.00000537f,
  // Wy (41)
  0.00000296f, -0.00001436f, 0.00008120f, 0.00027023f, 0.00637032f, 0.02993794f,
  0.15195328f, 0.48033126f, 0.53108236f, 0.51522270f, 0.59920130f, 0.91628715f,
  1.59503909f, 2.62761596f, 3.96342915f, 5.31796785f, 6.76459348f, 8.01465853f,
  9.29073733f, 10.15514799f, 10.33317564f, 10.10402575f, 8.99091761f, 7.24606278f,
  5.24229767f, 3.44257506f, 1.92238452f, 0.98425232f, 0.45577666f, 0.19474784f,
  0.07720900f, 0.02950731f, 0.01094243f, 0.00393048f, 0.00144094f, 0.00052520f,
  0.00019521f, 0.00007417f, 0.00002905f, 0.00001250f, 0.00000213f,
  // Wz (41)
  0.00015355f, -0.00075163f, 0.00398646f, 0.01008711f, 0.29544605f, 1.26290375f,
  5.26465350f, 11.00598621f, 7.18507613f, 3.55515533f, 1.77287574f, 1.13719943f,
  0.76614779f, 0.48563092f, 0.31065962f, 0.18064108f, 0.09331698f, 0.02832338f,
  -0.00262662f, 0.00069172f, -0.00018216f, 0.00004797f, -0.00001263f, 0.00000333f,
  -0.00000088f, 0.00000023f, -0.00000006f, 0.00000002f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

// CIE 1964 10-degree supplementary observer, narrow-band tri-phosphor fluorescent (F11, ~4000 K).
// Registry 10 nm LWL weighting table -- Wx,Wy,Wz blocks, CIE Y=100 scale.
static const icFloatNumber kWtsObs1964F11[123] = {
  // Wx (41)
  0.00420851f, -0.02061773f, 0.08524728f, 0.28828959f, -0.21033984f, 2.70395710f,
  4.89290666f, 0.91380739f, 1.57181850f, 0.57435704f, 0.34581290f, 0.17370985f,
  0.03060426f, -0.16127879f, 0.84426310f, -2.83721518f, 13.01183330f, 14.13306940f,
  -1.23418888f, 0.69346988f, 7.38623412f, 11.90195011f, -3.05597764f, 38.74261625f,
  6.81797079f, 5.21742944f, -0.06615669f, 0.75268084f, 0.19088422f, 0.08496891f,
  0.03447391f, 0.02589827f, 0.00705485f, 0.01833468f, -0.00152402f, 0.00064469f,
  -0.00005827f, 0.00006694f, 0.00001628f, 0.00000843f, 0.00000027f,
  // Wy (41)
  0.00037287f, -0.00181345f, 0.00760174f, 0.03475510f, -0.04047783f, 0.36070336f,
  0.71869390f, 0.28903455f, 0.61383820f, 0.55774236f, 1.04031581f, 3.58263857f,
  1.36074298f, 0.03365050f, 2.27436442f, -6.50284989f, 31.54602850f, 27.67237021f,
  -3.55911772f, 1.59586182f, 6.38153372f, 7.88442260f, -1.22890495f, 19.66630051f,
  3.00370567f, 2.31265002f, -0.05407084f, 0.31071883f, 0.07235034f, 0.03399823f,
  0.01320008f, 0.01011995f, 0.00272156f, 0.00712603f, -0.00059206f, 0.00025075f,
  -0.00002246f, 0.00002613f, 0.00000645f, 0.00000332f, 0.00000011f,
  // Wz (41)
  0.01826455f, -0.08942567f, 0.37041996f, 1.37199432f, -1.14290609f, 13.51180048f,
  24.79228364f, 5.18543384f, 8.93893556f, 3.93412175f, 3.33903940f, 4.56213694f,
  0.17614172f, 0.15301954f, 0.03302943f, -0.05178212f, 0.37634198f, 0.13698845f,
  -0.03237707f, 0.00852643f, -0.00224542f, 0.00059133f, -0.00015572f, 0.00004101f,
  -0.00001080f, 0.00000284f, -0.00000075f, 0.00000020f, -0.00000005f, 0.00000001f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
};

struct IccColorimetryWtEntry {
  icStandardObserver               obs;
  icColorimetryWeightingIlluminant illum;
  const icFloatNumber             *pWeights;   // 3*41 Wx,Wy,Wz blocks
};

static const IccColorimetryWtEntry kColorimetryWtTables[] = {
  { icStdObs1931TwoDegrees, icWtIllumD50,    kWtsObs1931D50 },
  { icStdObs1931TwoDegrees, icWtIllumD65,    kWtsObs1931D65 },
  { icStdObs1931TwoDegrees, icWtIllumA,      kWtsObs1931A },
  { icStdObs1931TwoDegrees, icWtIllumLED_B1, kWtsObs1931LED_B1 },
  { icStdObs1931TwoDegrees, icWtIllumF11,    kWtsObs1931F11 },
  { icStdObs1964TenDegrees, icWtIllumD50,    kWtsObs1964D50 },
  { icStdObs1964TenDegrees, icWtIllumD65,    kWtsObs1964D65 },
  { icStdObs1964TenDegrees, icWtIllumA,      kWtsObs1964A },
  { icStdObs1964TenDegrees, icWtIllumLED_B1, kWtsObs1964LED_B1 },
  { icStdObs1964TenDegrees, icWtIllumF11,    kWtsObs1964F11 },
};
static const int            kColorimetryWtTableCount = 10;
static const icUInt16Number kColorimetryWtSteps      = 41;
// <<<END GENERATED WEIGHTING TABLES

//==========================================================================
// Public free functions
//==========================================================================

bool icSpectralResample(const icSpectralRange &srcRange, const icFloatNumber *pSrc,
                        const icSpectralRange &dstRange, icFloatNumber *pDst,
                        icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  // Tier 1: null pointers and ill-formed (empty/inverted) ranges are rejected.
  if (!pSrc || !pDst || !rangeWellFormed(srcRange) || !rangeWellFormed(dstRange))
    return false;
  // Tier 2: refuse NaN/Inf input rather than propagate it into pDst.
  if (!allFinite(pSrc, (int)srcRange.steps))
    return false;
  Grid src(srcRange), dst(dstRange);
  std::vector<double> v, out;
  toDouble(pSrc, src.n, v);
  // Sprague needs >= 6 samples; gracefully fall back to cubic, then linear.
  icSpectralInterpMethod use = interp;
  if (use == icSpectralInterpSprague && src.n < 6)
    use = (src.n >= 4) ? icSpectralInterpCubic : icSpectralInterpLinear;
  resampleCore(src, v, dst, out, use, extend);
  for (int j = 0; j < dst.n; j++) pDst[j] = (icFloatNumber)out[j];
  return true;
}

bool icComputeWeightingTable(const icSpectralRange &obsRange, const icFloatNumber *pObs,
                             const icSpectralRange &illumRange, const icFloatNumber *pIllum,
                             const icSpectralRange &outRange, icFloatNumber *pWeights,
                             icSpectralInterpMethod interp) {
  // Tier 1: null pointers and ill-formed (empty/inverted) ranges are rejected.
  if (!pObs || !pIllum || !pWeights ||
      !rangeWellFormed(obsRange) || !rangeWellFormed(illumRange) || !rangeWellFormed(outRange))
    return false;
  // Tier 2: refuse NaN/Inf observer (3*steps xbar,ybar,zbar) or illuminant input.
  if (!allFinite(pObs, 3*(int)obsRange.steps) || !allFinite(pIllum, (int)illumRange.steps))
    return false;

  Grid obs(obsRange), out(outRange);
  int nf = obs.n;            // fine integration grid = observer grid
  int nc = out.n;            // coarse output grid

  // Observer blocks (xbar,ybar,zbar) and illuminant resampled onto the fine grid.
  std::vector<double> xb, yb, zb, ill;
  toDouble(pObs,           nf, xb);
  toDouble(pObs + nf,      nf, yb);
  toDouble(pObs + 2*nf,    nf, zb);
  {
    Grid ig(illumRange);
    std::vector<double> illv; toDouble(pIllum, ig.n, illv);
    // illuminant range reconciliation: cubic (3rd-order) per TN-06, hold ends.
    std::vector<double> tmp;
    resampleCore(ig, illv, obs, tmp, icSpectralInterpCubic, icSpectralExtendHold);
    ill.swap(tmp);
  }

  // Fine color-stimulus products Pc(lambda) = CMFc(lambda) * S(lambda).
  std::vector<double> Px(nf), Py(nf), Pz(nf);
  double sumPy = 0.0;
  for (int l = 0; l < nf; l++) {
    Px[l] = xb[l] * ill[l];
    Py[l] = yb[l] * ill[l];
    Pz[l] = zb[l] * ill[l];
    sumPy += Py[l];
  }
  if (!(std::fabs(sumPy) > 1e-12))
    return false;
  double k = 1.0 / sumPy;    // relative colorimetry: perfect diffuser -> Y = 1

  // For each coarse sample, the reconstruction basis b_m on the fine grid is the
  // resampling of a unit coarse impulse (triangular for linear, CIE 167 kernel
  // for Sprague). W_c[m] = k * sum_lambda Pc(lambda) * b_m(lambda).
  std::vector<double> impulse(nc, 0.0), basis;
  std::vector<double> wx(nc, 0.0), wy(nc, 0.0), wz(nc, 0.0);
  icSpectralInterpMethod use = interp;
  if (use == icSpectralInterpSprague && nc < 6)
    use = (nc >= 4) ? icSpectralInterpCubic : icSpectralInterpLinear;

  for (int m = 0; m < nc; m++) {
    impulse[m] = 1.0;
    resampleCore(out, impulse, obs, basis, use, icSpectralExtendHold);
    impulse[m] = 0.0;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (int l = 0; l < nf; l++) {
      sx += Px[l] * basis[l];
      sy += Py[l] * basis[l];
      sz += Pz[l] * basis[l];
    }
    wx[m] = k * sx; wy[m] = k * sy; wz[m] = k * sz;
  }

  for (int m = 0; m < nc; m++) {
    pWeights[m]        = (icFloatNumber)wx[m];
    pWeights[nc + m]   = (icFloatNumber)wy[m];
    pWeights[2*nc + m] = (icFloatNumber)wz[m];
  }
  return true;
}

void icApplyWeightingTable(const icSpectralRange &range, const icFloatNumber *pWeights,
                           const icFloatNumber *pReflectance, icFloatNumber *pXYZ) {
  // Tier 1: this is the one apply kernel with no caller-side gate, so it must not
  // dereference null/degenerate input. Being void, it signals "no result" by
  // emitting XYZ = 0 (when pXYZ is available) and bailing.
  if (!pWeights || !pReflectance || !pXYZ || !range.steps) {
    if (pXYZ) pXYZ[0] = pXYZ[1] = pXYZ[2] = 0;
    return;
  }
  int n = (int)range.steps;
  // DELIBERATE (no value clamping): weights and samples pass through untouched.
  // The registry LWL weights are legitimately negative by construction, and
  // reflectance/radiance samples are legitimately > 1.0 (emissive and scene
  // spectrophotometry) or slightly < 0 (measurement noise/fluorescence). Clamping
  // to [0,1] here would silently corrupt valid data, so we do not range-limit.
  double X = 0.0, Y = 0.0, Z = 0.0;
  for (int m = 0; m < n; m++) {
    double r = pReflectance[m];
    X += pWeights[m]        * r;
    Y += pWeights[n + m]    * r;
    Z += pWeights[2*n + m]  * r;
  }
  // Tier 2: a NaN/Inf weight or sample propagates into the sums; rather than emit a
  // non-finite XYZ, fall back to 0 on each affected channel.
  pXYZ[0] = std::isfinite(X) ? (icFloatNumber)X : 0;
  pXYZ[1] = std::isfinite(Y) ? (icFloatNumber)Y : 0;
  pXYZ[2] = std::isfinite(Z) ? (icFloatNumber)Z : 0;
}

const icFloatNumber *icGetStandardObserver(icStandardObserver obs, icSpectralRange &outRange)
{
  // iccDEV's built-in CMF tables live as file-static data reachable only through
  // CIccTagSpectralViewingConditions::getObserver(). Select the standard id on a
  // throwaway viewing-conditions tag (no custom data) and let getObserver() return
  // the shared table -- a single source of truth, no duplicated curves here. The
  // returned pointer is into program-lifetime static storage, not the temporary,
  // so it stays valid after vc is destroyed.
  CIccTagSpectralViewingConditions vc;
  icSpectralRange none;
  memset(&none, 0, sizeof(none));
  vc.setObserver(obs, none, NULL);
  const icFloatNumber *p = vc.getObserver(outRange);
  return (p && outRange.steps) ? p : NULL;   // nullptr if obs has no built-in table
}

const icFloatNumber *icGetStandardIlluminant(icIlluminant illum, icSpectralRange &outRange)
{
  // Same indirection as icGetStandardObserver for the built-in illuminant SPDs.
  CIccTagSpectralViewingConditions vc;
  icSpectralRange none;
  memset(&none, 0, sizeof(none));
  vc.setIlluminant(illum, none, NULL);
  const icFloatNumber *p = vc.getIlluminant(outRange);
  return (p && outRange.steps) ? p : NULL;   // nullptr if illum has no built-in table
}

const icFloatNumber *icGetColorimetryWeightingTable(icStandardObserver obs,
                                                    icColorimetryWeightingIlluminant illum,
                                                    icSpectralRange &outRange)
{
  // Linear scan of the generated registry-table directory (only 10 entries). All
  // registry tables share one grid: 380-780 nm @ 10 nm = 41 samples (icRange380nm /
  // icRange780nm are the icFloat16 encodings iccDEV's spectral code uses elsewhere).
  for (int i = 0; i < kColorimetryWtTableCount; i++) {
    if (kColorimetryWtTables[i].obs == obs && kColorimetryWtTables[i].illum == illum) {
      outRange.start = icRange380nm;
      outRange.end   = icRange780nm;
      outRange.steps = kColorimetryWtSteps;
      return kColorimetryWtTables[i].pWeights;
    }
  }
  memset(&outRange, 0, sizeof(outRange));
  return NULL;   // (obs,illum) not one of the registry's 10 published tables
}

//==========================================================================
// CIccColorimetricCalculator
//==========================================================================

CIccColorimetricCalculator::CIccColorimetricCalculator()
  : m_bHaveWhite(false), m_bReady(false) {
  memset(&m_obsRange, 0, sizeof(m_obsRange));
  memset(&m_illumRange, 0, sizeof(m_illumRange));
  memset(&m_whiteRange, 0, sizeof(m_whiteRange));
  memset(&m_measRange, 0, sizeof(m_measRange));
}

CIccColorimetricCalculator::~CIccColorimetricCalculator() {}

bool CIccColorimetricCalculator::SetObserver(const icSpectralRange &range, const icFloatNumber *pObserver) {
  // Tier 1: validate the range geometry once, here at the boundary (not per sample).
  if (!rangeWellFormed(range) || !pObserver)
    return false;
  m_obsRange = range;
  m_obs.assign(pObserver, pObserver + 3*(int)range.steps);
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::SetIlluminant(const icSpectralRange &range, const icFloatNumber *pIlluminant) {
  if (!rangeWellFormed(range) || !pIlluminant)
    return false;
  m_illumRange = range;
  m_illum.assign(pIlluminant, pIlluminant + (int)range.steps);
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::SetStandardObserver(icStandardObserver obs)
{
  icSpectralRange range;
  const icFloatNumber *p = icGetStandardObserver(obs, range);
  return p ? SetObserver(range, p) : false;   // SetObserver copies, so p need not persist
}

bool CIccColorimetricCalculator::SetStandardIlluminant(icIlluminant illum)
{
  icSpectralRange range;
  const icFloatNumber *p = icGetStandardIlluminant(illum, range);
  return p ? SetIlluminant(range, p) : false;
}

bool CIccColorimetricCalculator::SetEmissiveWhite(const icSpectralRange &range, const icFloatNumber *pWhite) {
  if (!rangeWellFormed(range) || !pWhite)
    return false;
  m_whiteRange = range;
  m_white.assign(pWhite, pWhite + (int)range.steps);
  m_bHaveWhite = true;
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::Prepare(const icSpectralRange &measRange, icXYZCalcMethod method,
                                         icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  if (!rangeWellFormed(measRange) || !m_obs.size() || !m_illum.size())
    return false;
  int nm = (int)measRange.steps;
  m_measRange = measRange;
  m_M.assign(3*nm, 0.0f);

  if (method == icXYZCalcDirectSum) {
    // Baseline: rectangular sum at the measurement interval (current iccDEV
    // behaviour). Observer + illuminant are resampled onto measRange (linear),
    // then M[c][m] = k * CMFc(meas_m) * S(meas_m), k = 1 / sum(ybar*S).
    Grid om(m_obsRange), me(measRange);
    std::vector<double> xb, yb, zb;
    toDouble(&m_obs[0],            om.n, xb);
    toDouble(&m_obs[om.n],         om.n, yb);
    toDouble(&m_obs[2*om.n],       om.n, zb);
    std::vector<double> xm, ym, zm, sm, illv;
    resampleCore(om, xb, me, xm, icSpectralInterpLinear, extend);
    resampleCore(om, yb, me, ym, icSpectralInterpLinear, extend);
    resampleCore(om, zb, me, zm, icSpectralInterpLinear, extend);
    Grid ig(m_illumRange);
    toDouble(&m_illum[0], ig.n, illv);
    resampleCore(ig, illv, me, sm, icSpectralInterpLinear, extend);
    double sumPy = 0.0;
    for (int m = 0; m < nm; m++) sumPy += ym[m]*sm[m];
    if (!(std::fabs(sumPy) > 1e-12)) return false;
    double k = 1.0/sumPy;
    for (int m = 0; m < nm; m++) {
      m_M[m]        = (icFloatNumber)(k * xm[m] * sm[m]);
      m_M[nm + m]   = (icFloatNumber)(k * ym[m] * sm[m]);
      m_M[2*nm + m] = (icFloatNumber)(k * zm[m] * sm[m]);
    }
    // Tier 2: a non-finite observer/illuminant sample (e.g. NaN only in xbar/zbar)
    // can slip past the sumPy guard, which only tests ybar*S. Reject any operator
    // that came out non-finite so ReflectanceToXYZ never applies a poisoned matrix.
    if (!allFinite(&m_M[0], 3*nm))
      return false;
    m_bReady = true;
    return true;
  }

  // Weighting (self-computed) and Sprague-to-fine both reduce to a weighting
  // table over measRange; the reconstruction kernel differs.
  icSpectralInterpMethod recon = (method == icXYZCalcSpragueTo1nm)
                                   ? icSpectralInterpSprague : icSpectralInterpLinear;
  (void)interp;
  bool ok = icComputeWeightingTable(m_obsRange, &m_obs[0], m_illumRange, &m_illum[0],
                                    measRange, &m_M[0], recon);
  // Tier 2: belt-and-braces -- icComputeWeightingTable already finiteness-checks its
  // inputs, but reject the built operator too if it somehow came out non-finite.
  if (ok && !allFinite(&m_M[0], 3*nm))
    ok = false;
  m_bReady = ok;
  return ok;
}

bool CIccColorimetricCalculator::PrepareEmissive(const icSpectralRange &measRange,
                                                 icSpectralInterpMethod interp,
                                                 icSpectralExtendMethod extend) {
  if (!rangeWellFormed(measRange) || !m_obs.size() || !m_bHaveWhite || !m_white.size())
    return false;
  int nm = (int)measRange.steps;
  m_measRange = measRange;
  m_M.assign(3*nm, 0.0f);

  // Emissive/radiant colorimetry: the spectrum IS the stimulus, so there is no
  // illuminant factor (cf. the reflectance path's R*S). Mirror
  // CIccPcc::getEmissiveObserver: resample the observer onto measRange and scale all
  // three CMFs by 1/k with k = sum(ybar(meas)*white(meas)), so the adopted white
  // radiance maps to Y = 1; then M[c][m] = CMFc(meas_m) / k. The default linear
  // interp matches getEmissiveObserver's rangeMap.
  Grid om(m_obsRange), me(measRange);
  std::vector<double> xb, yb, zb;
  toDouble(&m_obs[0],      om.n, xb);
  toDouble(&m_obs[om.n],   om.n, yb);
  toDouble(&m_obs[2*om.n], om.n, zb);
  std::vector<double> xm, ym, zm, wm, wv;
  resampleCore(om, xb, me, xm, interp, extend);
  resampleCore(om, yb, me, ym, interp, extend);
  resampleCore(om, zb, me, zm, interp, extend);
  Grid wg(m_whiteRange);
  toDouble(&m_white[0], wg.n, wv);
  resampleCore(wg, wv, me, wm, interp, extend);

  double k = 0.0;
  for (int m = 0; m < nm; m++) k += ym[m]*wm[m];
  if (!(std::fabs(k) > 1e-12)) return false;
  for (int m = 0; m < nm; m++) {
    m_M[m]        = (icFloatNumber)(xm[m] / k);
    m_M[nm + m]   = (icFloatNumber)(ym[m] / k);
    m_M[2*nm + m] = (icFloatNumber)(zm[m] / k);
  }
  // Tier 2: reject a non-finite operator (e.g. NaN observer/white outside the
  // sum(ybar*white) term) so RadianceToXYZ never applies a poisoned matrix.
  if (!allFinite(&m_M[0], 3*nm))
    return false;
  m_bReady = true;
  return true;
}

bool CIccColorimetricCalculator::ReflectanceToXYZ(const icFloatNumber *pReflectance, icFloatNumber *pXYZ) const {
  if (!m_bReady || !pReflectance || !pXYZ)
    return false;
  icApplyWeightingTable(m_measRange, &m_M[0], pReflectance, pXYZ);
  return true;
}

bool CIccColorimetricCalculator::RadianceToXYZ(const icFloatNumber *pRadiance, icFloatNumber *pXYZ) const {
  // Same prepared-operator application as ReflectanceToXYZ; the operator built by
  // PrepareEmissive already carries the emissive (no-illuminant) normalization.
  if (!m_bReady || !pRadiance || !pXYZ)
    return false;
  icApplyWeightingTable(m_measRange, &m_M[0], pRadiance, pXYZ);
  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
