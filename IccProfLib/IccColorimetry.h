/** @file
    File:       IccColorimetry.h

    Contains:   Header for spectral-to-colorimetric (XYZ) reduction methods that
                follow the ICC colorimetry-data registry recommendations
                (ICC TN-06-2025, ICC White Paper 56, CIE 15, CIE 167:2005,
                ISO 13655 / ASTM E308). Provides the plural set of computation
                methods (direct summation, weighting functions, Sprague
                interpolation) plus the range/interval reconciliation building
                blocks they require.

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

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation for issue #1475 (reconcile iccDEV spectral
//  colorimetry with registry.color.org/colorimetry-data methods).
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCCOLORIMETRY_H)
#define _ICCCOLORIMETRY_H

#include "IccUtil.h"   // icFloatNumber, icSpectralRange, icF16toF, icNotZero, ICCPROFLIB_API
#include <vector>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 * Computation method for reducing a spectral reflectance/transmittance vector to
 * CIE XYZ.  These mirror the plural methods recommended by ICC TN-06-2025:
 *  - icXYZCalcDirectSum: the naive rectangular summation X = k*Sum(xbar*S*R) at
 *    the measurement interval. This reproduces the behaviour of the existing
 *    iccMAX spectral-PCS path and is provided as the comparison baseline.
 *  - icXYZCalcWeighting: the weighting-function method X = R.w (eqn 2 of WP56),
 *    using either an externally supplied table (e.g. the registry LWL tables) or
 *    self-computed triangular/ASTM-style weights. Recommended for intervals that
 *    are not 1 or 5 nm (e.g. 10 nm instrument data).
 *  - icXYZCalcSpragueTo1nm: reconstruct the measurement onto a fine grid with
 *    CIE 167:2005 Sprague interpolation before summation. Recommended fallback
 *    when no suitable weighting function is available.
 */
enum icXYZCalcMethod {
  icXYZCalcDirectSum     = 0,
  icXYZCalcWeighting     = 1,
  icXYZCalcSpragueTo1nm  = 2,
};

/** Interpolation used when resampling equally-spaced spectral data between ranges. */
enum icSpectralInterpMethod {
  icSpectralInterpLinear  = 0,  // matches the existing CIccMatrixMath::SetRange behaviour
  icSpectralInterpCubic   = 1,  // 3rd-order (TN-06 illuminant reconciliation)
  icSpectralInterpSprague = 2,  // CIE 167:2005 Sprague (requires >= 6 source samples)
};

/** End-handling when a measurement range is narrower than the target range. */
enum icSpectralExtendMethod {
  icSpectralExtendHold   = 0,  // TN-06: extend the last known data point (constant)
  icSpectralExtendLinear = 1,  // WP56: linear extrapolation from the end values
};

/**
 * Resample equally-spaced spectral data from srcRange onto dstRange.  Samples of
 * dstRange that fall outside srcRange are filled per the chosen extend method.
 * pSrc has srcRange.steps samples; pDst must hold dstRange.steps samples.
 */
ICCPROFLIB_API bool icSpectralResample(const icSpectralRange &srcRange, const icFloatNumber *pSrc,
                                       const icSpectralRange &dstRange, icFloatNumber *pDst,
                                       icSpectralInterpMethod interp = icSpectralInterpSprague,
                                       icSpectralExtendMethod extend = icSpectralExtendHold);

/**
 * Compute a weighting table that folds the observer and illuminant (and the
 * reconstruction implied by interp) into per-sample weights for the coarse
 * outRange.  pObs is 3*obsRange.steps (xbar,ybar,zbar blocks); pIllum is
 * illumRange.steps; pWeights receives 3*outRange.steps (Wx,Wy,Wz blocks).
 * Weights are normalised for relative colorimetry (perfect diffuser -> Y = 1).
 * interp = Linear gives the ASTM-style triangular reconstruction; interp =
 * Sprague gives a CIE 167 reconstruction.
 */
ICCPROFLIB_API bool icComputeWeightingTable(const icSpectralRange &obsRange, const icFloatNumber *pObs,
                                            const icSpectralRange &illumRange, const icFloatNumber *pIllum,
                                            const icSpectralRange &outRange, icFloatNumber *pWeights,
                                            icSpectralInterpMethod interp = icSpectralInterpLinear);

/** Apply a weighting table (3*range.steps) to a reflectance vector -> XYZ[3]. */
ICCPROFLIB_API void icApplyWeightingTable(const icSpectralRange &range, const icFloatNumber *pWeights,
                                          const icFloatNumber *pReflectance, icFloatNumber *pXYZ);

/**
 * Fetch one of iccDEV's built-in CIE colour-matching-function tables (380-780 nm
 * @ 5 nm = 81 samples, laid out as consecutive xbar,ybar,zbar blocks -- exactly the
 * 3*steps form SetObserver() expects). Fills outRange and returns the shared table,
 * or nullptr if obs has no built-in data (only icStdObs1931TwoDegrees and
 * icStdObs1964TenDegrees do). The returned pointer is program-lifetime static data
 * (the same table CIccTagSpectralViewingConditions uses); do not free it.
 */
ICCPROFLIB_API const icFloatNumber *icGetStandardObserver(icStandardObserver obs,
                                                          icSpectralRange &outRange);

/**
 * Fetch one of iccDEV's built-in relative illuminant SPD tables (380-780 nm @ 5 nm
 * = 81 samples, the form SetIlluminant() expects). Fills outRange and returns the
 * shared table, or nullptr if illum has no built-in data (only icIlluminantD50,
 * icIlluminantD65, icIlluminantD93 and icIlluminantA do). The returned pointer is
 * program-lifetime static data; do not free it.
 */
ICCPROFLIB_API const icFloatNumber *icGetStandardIlluminant(icIlluminant illum,
                                                            icSpectralRange &outRange);

// NOTE: ingesting the registry weighting-table CSVs is intentionally NOT part of
// this live library. The registry values are effectively static; an external
// "parallel" loader/converter (run by a maintainer) can fetch the registry CSVs
// and produce the weight arrays, which then enter via SetWeightingTable().

/**
 **************************************************************************
 * Type: Class
 *
 * Purpose:
 *  Reduces spectral reflectance to CIE XYZ using a selectable, registry-aligned
 *  method.  Observer and illuminant are supplied explicitly (so the caller can
 *  feed authoritative CIE 1 nm data or the built-in 5 nm tables); an external
 *  weighting table (e.g. a registry LWL CSV) may be supplied for the weighting
 *  method.  Prepare() builds a 3 x measRange.steps operator so that subsequent
 *  ReflectanceToXYZ() calls are a single matrix-vector product.
 **************************************************************************
 */
class ICCPROFLIB_API CIccColorimetricCalculator
{
public:
  CIccColorimetricCalculator();
  ~CIccColorimetricCalculator();

  /// Set the colour-matching functions: pObserver holds 3*range.steps samples as
  /// consecutive xbar, ybar, zbar blocks. Resets the prepared operator.
  bool SetObserver(const icSpectralRange &range, const icFloatNumber *pObserver);     // 3*range.steps
  /// Convenience: set the observer from a built-in CIE table (see
  /// icGetStandardObserver). Returns false if obs has no built-in data.
  bool SetStandardObserver(icStandardObserver obs);
  /// Set the illuminant SPD: pIlluminant holds range.steps relative-power samples.
  /// Resets the prepared operator.
  bool SetIlluminant(const icSpectralRange &range, const icFloatNumber *pIlluminant); // range.steps
  /// Convenience: set the illuminant from a built-in SPD table (see
  /// icGetStandardIlluminant). Returns false if illum has no built-in data.
  bool SetStandardIlluminant(icIlluminant illum);
  /// Supply an external weighting table (e.g. a registry LWL CSV converted to
  /// 3*range.steps Wx,Wy,Wz weights). Used only by icXYZCalcWeighting when its
  /// range matches measRange exactly; resets the prepared operator.
  bool SetWeightingTable(const icSpectralRange &range, const icFloatNumber *pWeights);// 3*range.steps

  /// Build the 3 x measRange.steps reduction operator for measurements sampled on
  /// measRange using the chosen method (interp/extend select the reconstruction and
  /// end handling). Requires an observer and illuminant; returns false otherwise.
  bool Prepare(const icSpectralRange &measRange, icXYZCalcMethod method,
               icSpectralInterpMethod interp = icSpectralInterpSprague,
               icSpectralExtendMethod extend = icSpectralExtendHold);

  /// Reduce one reflectance vector (measRange.steps samples) to XYZ[3] using the
  /// prepared operator. Returns false if Prepare() has not succeeded.
  bool ReflectanceToXYZ(const icFloatNumber *pReflectance, icFloatNumber *pXYZ) const;

  /// True once Prepare() has built a usable operator.
  bool IsReady() const { return m_bReady; }

private:
  icSpectralRange m_obsRange;
  std::vector<icFloatNumber> m_obs;     // 3*m_obsRange.steps
  icSpectralRange m_illumRange;
  std::vector<icFloatNumber> m_illum;   // m_illumRange.steps

  bool m_bHaveWt;
  icSpectralRange m_wtRange;
  std::vector<icFloatNumber> m_wt;      // 3*m_wtRange.steps (external table)

  icSpectralRange m_measRange;
  std::vector<icFloatNumber> m_M;       // prepared 3*m_measRange.steps operator
  bool m_bReady;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCCOLORIMETRY_H
