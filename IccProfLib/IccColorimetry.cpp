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

// 4-point cubic (Catmull-Rom) at segment i, fractional X, indices clamped to data.
static double cubicEval(const std::vector<double> &v, int i, double X) {
  int n = (int)v.size();
  int im1 = i-1 < 0 ? 0 : i-1;
  int ip1 = i+1 > n-1 ? n-1 : i+1;
  int ip2 = i+2 > n-1 ? n-1 : i+2;
  double p0 = v[im1], p1 = v[i], p2 = v[ip1], p3 = v[ip2];
  double a = -0.5*p0 + 1.5*p1 - 1.5*p2 + 0.5*p3;
  double b =      p0 - 2.5*p1 + 2.0*p2 - 0.5*p3;
  double c = -0.5*p0          + 0.5*p2;
  return ((a*X + b)*X + c)*X + p1;
}

// Resample equally-spaced data v (grid src) onto grid dst, into out.
static void resampleCore(const Grid &src, const std::vector<double> &v,
                         const Grid &dst, std::vector<double> &out,
                         icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  int n = src.n;
  out.assign(dst.n, 0.0);
  std::vector<double> ext;
  bool haveExt = (interp == icSpectralInterpSprague) && spragueExtend(v, ext);

  for (int j = 0; j < dst.n; j++) {
    double w = dst.nm(j);
    double t = (src.step != 0.0) ? (w - src.start) / src.step : 0.0;

    if (t <= 0.0) {                                 // at/below first sample
      if (t > -1e-9) { out[j] = v[0]; continue; }
      out[j] = (extend == icSpectralExtendLinear && n > 1)
                 ? v[0] + t * (v[1] - v[0]) : v[0]; // linear extrapolation or hold
      continue;
    }
    if (t >= n - 1) {                               // at/above last sample
      if (t < n - 1 + 1e-9) { out[j] = v[n-1]; continue; }
      out[j] = (extend == icSpectralExtendLinear && n > 1)
                 ? v[n-1] + (t - (n-1)) * (v[n-1] - v[n-2]) : v[n-1];
      continue;
    }

    int i = (int)std::floor(t);
    if (i > n - 2) i = n - 2;
    double X = t - i;
    if (interp == icSpectralInterpSprague && haveExt)
      out[j] = spragueEval(ext, i, X);
    else if (interp == icSpectralInterpCubic)
      out[j] = cubicEval(v, i, X);
    else
      out[j] = v[i] * (1.0 - X) + v[i+1] * X;       // linear (matches CIccMatrixMath::SetRange)
  }
}

static void toDouble(const icFloatNumber *p, int n, std::vector<double> &v) {
  v.resize(n);
  for (int i = 0; i < n; i++) v[i] = (double)p[i];
}

} // anonymous namespace

//==========================================================================
// Public free functions
//==========================================================================

bool icSpectralResample(const icSpectralRange &srcRange, const icFloatNumber *pSrc,
                        const icSpectralRange &dstRange, icFloatNumber *pDst,
                        icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  if (!pSrc || !pDst || !srcRange.steps || !dstRange.steps)
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
  if (!pObs || !pIllum || !pWeights || !obsRange.steps || !illumRange.steps || !outRange.steps)
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
  int n = (int)range.steps;
  double X = 0.0, Y = 0.0, Z = 0.0;
  for (int m = 0; m < n; m++) {
    double r = pReflectance[m];
    X += pWeights[m]        * r;
    Y += pWeights[n + m]    * r;
    Z += pWeights[2*n + m]  * r;
  }
  pXYZ[0] = (icFloatNumber)X;
  pXYZ[1] = (icFloatNumber)Y;
  pXYZ[2] = (icFloatNumber)Z;
}

//==========================================================================
// CIccColorimetricCalculator
//==========================================================================

CIccColorimetricCalculator::CIccColorimetricCalculator()
  : m_bHaveWt(false), m_bReady(false) {
  memset(&m_obsRange, 0, sizeof(m_obsRange));
  memset(&m_illumRange, 0, sizeof(m_illumRange));
  memset(&m_wtRange, 0, sizeof(m_wtRange));
  memset(&m_measRange, 0, sizeof(m_measRange));
}

CIccColorimetricCalculator::~CIccColorimetricCalculator() {}

bool CIccColorimetricCalculator::SetObserver(const icSpectralRange &range, const icFloatNumber *pObserver) {
  if (!range.steps || !pObserver)
    return false;
  m_obsRange = range;
  m_obs.assign(pObserver, pObserver + 3*(int)range.steps);
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::SetIlluminant(const icSpectralRange &range, const icFloatNumber *pIlluminant) {
  if (!range.steps || !pIlluminant)
    return false;
  m_illumRange = range;
  m_illum.assign(pIlluminant, pIlluminant + (int)range.steps);
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::SetWeightingTable(const icSpectralRange &range, const icFloatNumber *pWeights) {
  if (!range.steps || !pWeights)
    return false;
  m_wtRange = range;
  m_wt.assign(pWeights, pWeights + 3*(int)range.steps);
  m_bHaveWt = true;
  m_bReady = false;
  return true;
}

bool CIccColorimetricCalculator::Prepare(const icSpectralRange &measRange, icXYZCalcMethod method,
                                         icSpectralInterpMethod interp, icSpectralExtendMethod extend) {
  if (!measRange.steps || !m_obs.size() || !m_illum.size())
    return false;
  int nm = (int)measRange.steps;
  m_measRange = measRange;
  m_M.assign(3*nm, 0.0f);

  if (method == icXYZCalcWeighting && m_bHaveWt &&
      m_wtRange.start == measRange.start && m_wtRange.end == measRange.end &&
      m_wtRange.steps == measRange.steps) {
    // Use the externally supplied (e.g. registry LWL) weighting table directly.
    m_M = m_wt;
    m_bReady = true;
    return true;
  }

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
  m_bReady = ok;
  return ok;
}

bool CIccColorimetricCalculator::ReflectanceToXYZ(const icFloatNumber *pReflectance, icFloatNumber *pXYZ) const {
  if (!m_bReady || !pReflectance || !pXYZ)
    return false;
  icApplyWeightingTable(m_measRange, &m_M[0], pReflectance, pXYZ);
  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
