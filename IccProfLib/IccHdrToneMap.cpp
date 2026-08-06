/** @file
    File:       IccHdrToneMap.cpp

    Contains:   Implementation of the HDR tone-mapping step of ICC.1 clause
                8.10.2 - analytic PQ and HLG transfer functions, the
                reference white relative normalisation, and the Headroom
                Adaptive Gain Curve evaluator of the HAGC amendment's annex 1

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
// -Initial implementation of the clause 8.10.2 tone-mapping step
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
  #include <windows.h>
#endif
#include <cmath>
#include <cstring>

#include "IccHdrToneMap.h"
#include "IccHdrProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 ****************************************************************************
 * Name: icHdrClampUnit
 *
 * Purpose:
 *  Clamp to [0, 1].  Written with the negated comparison so that a NaN -
 *  which compares false against everything - lands on zero rather than
 *  propagating through the encode.
 ****************************************************************************
 */
static icFloatNumber icHdrClampUnit(icFloatNumber v)
{
  if (!(v > 0.0))
    return 0.0;

  return v > 1.0 ? (icFloatNumber)1.0 : v;
}

/**
 ****************************************************************************
 * Name: icPqEotf
 *
 * Purpose:
 *  SMPTE ST 2084 EOTF, in the form the standard states it:
 *
 *      Y = ( max(V^(1/m2) - c1, 0) / (c2 - c3 * V^(1/m2)) ) ^ (1/m1)
 *
 *  The max() is the standard's own; it is what keeps the numerator at zero
 *  over the toe rather than letting a negative base reach a fractional power.
 *  The denominator cannot reach zero for V in [0, 1]: c2 - c3 is 0.1640625 at
 *  V = 1 and grows as V falls.
 *
 * Args:
 *  v = PQ-encoded value, nominally in [0, 1]
 *
 * Return:
 *  Display luminance normalised so that 1.0 is icPqPeakLuminance cd/m^2.
 ****************************************************************************
 */
icFloatNumber icPqEotf(icFloatNumber v)
{
  // Below zero the EOTF is not defined.  Clamping rather than reflecting is
  // what every PQ implementation does, and it keeps a NaN out of the chain.
  if (!(v > 0.0))
    return 0.0;

  double vp = pow((double)v, 1.0 / icPqM2);
  double num = vp - icPqC1;

  if (num < 0.0)
    num = 0.0;

  double den = icPqC2 - icPqC3 * vp;

  if (den <= 0.0)
    return 0.0;

  return (icFloatNumber)pow(num / den, 1.0 / icPqM1);
}

/**
 ****************************************************************************
 * Name: icPqInverseEotf
 *
 * Purpose:
 *  Inverse of icPqEotf:  V = ( (c1 + c2 * Y^m1) / (1 + c3 * Y^m1) ) ^ m2
 *
 * Args:
 *  l = display luminance normalised so that 1.0 is icPqPeakLuminance cd/m^2
 *
 * Return:
 *  The PQ-encoded value.
 ****************************************************************************
 */
icFloatNumber icPqInverseEotf(icFloatNumber l)
{
  if (!(l > 0.0))
    return 0.0;

  double lm = pow((double)l, icPqM1);

  return (icFloatNumber)pow((icPqC1 + icPqC2 * lm) / (1.0 + icPqC3 * lm), icPqM2);
}

/**
 ****************************************************************************
 * Name: icHlgOetf
 *
 * Purpose:
 *  Rec. ITU-R BT.2100 HLG OETF (Table 5):
 *
 *      E' = sqrt(3E)                for 0 <= E <= 1/12
 *      E' = a * ln(12E - b) + c     for 1/12 < E
 *
 * Args:
 *  e = scene-linear value, nominally in [0, 1]
 *
 * Return:
 *  The HLG signal value.
 ****************************************************************************
 */
icFloatNumber icHlgOetf(icFloatNumber e)
{
  if (!(e > 0.0))
    return 0.0;

  if (e <= icHlgKnee)
    return (icFloatNumber)sqrt(3.0 * (double)e);

  // 12E - b is strictly positive above the knee: at E = 1/12 it is 1 - b,
  // about 0.715, so the logarithm's argument never approaches zero.
  return (icFloatNumber)(icHlgA * log(12.0 * (double)e - icHlgB) + icHlgC);
}

/**
 ****************************************************************************
 * Name: icHlgInverseOetf
 *
 * Purpose:
 *  Inverse HLG OETF (BT.2100 Table 5):
 *
 *      E = E'^2 / 3                     for 0 <= E' <= 1/2
 *      E = (exp((E' - c)/a) + b) / 12   for 1/2 < E'
 *
 * Args:
 *  v = HLG signal value, nominally in [0, 1]
 *
 * Return:
 *  The scene-linear value.
 ****************************************************************************
 */
icFloatNumber icHlgInverseOetf(icFloatNumber v)
{
  if (!(v > 0.0))
    return 0.0;

  if (v <= 0.5)
    return (icFloatNumber)((double)v * (double)v / 3.0);

  return (icFloatNumber)((exp(((double)v - icHlgC) / icHlgA) + icHlgB) / 12.0);
}

/**
 ****************************************************************************
 * Name: icHlgOotfGain
 *
 * Purpose:
 *  The scene-to-display system gain of the BT.2100 HLG OOTF, Y_s^(gamma - 1).
 *  Applied to every channel alike, which is what makes the OOTF a luminance
 *  dependent gain rather than a per-channel curve.
 *
 * Args:
 *  sceneLuminance = Y_s, the BT.2100-weighted scene luminance of the triplet
 *  gamma = the system gamma, 1.2 at the nominal 1000 cd/m^2 peak
 *
 * Return:
 *  The gain.  Zero scene luminance returns zero gain, which is the limit of
 *  the expression for gamma > 1 and keeps black at black.
 ****************************************************************************
 */
icFloatNumber icHlgOotfGain(icFloatNumber sceneLuminance, icFloatNumber gamma)
{
  if (!(sceneLuminance > 0.0))
    return 0.0;

  return (icFloatNumber)pow((double)sceneLuminance, (double)gamma - 1.0);
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::CIccHdrTransfer
 ****************************************************************************
 */
CIccHdrTransfer::CIccHdrTransfer()
{
  m_bSupported = false;
  m_bUseProfileCurves = false;
  m_nTransfer = 0;
  m_referenceWhite = (icFloatNumber)icHdrDefaultContentReferenceWhite;
  m_hlgGamma = (icFloatNumber)icHlgDefaultGamma;
  m_hlgPeakLuminance = (icFloatNumber)icHlgDefaultPeakLuminance;
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::Init
 *
 * Purpose:
 *  Fix the transfer characteristic and the normalisation constants.
 *
 * Args:
 *  nTransferCharacteristics = the cicpTag field
 *  contentReferenceWhite = resolved CRWL in cd/m^2 (clause 8.10.4)
 *  hlgGamma, hlgPeakLuminance = HLG OOTF parameters, ignored otherwise
 *
 * Return:
 *  true when the configuration can be evaluated.
 ****************************************************************************
 */
bool CIccHdrTransfer::Init(icUInt8Number nTransferCharacteristics,
                           icFloatNumber contentReferenceWhite,
                           icFloatNumber hlgGamma /* =icHlgDefaultGamma */,
                           icFloatNumber hlgPeakLuminance /* =icHlgDefaultPeakLuminance */)
{
  m_bSupported = false;
  m_bUseProfileCurves = false;
  m_nTransfer = nTransferCharacteristics;

  // A non-positive reference white would divide the whole chain by zero or
  // flip its sign.  It is also already refused upstream - #1980 made a
  // non-physical encoding white-point luminance a validation failure - so
  // reaching here with one means the value came from somewhere other than a
  // validated profile, and falling back to the clause 8.10.4 default is
  // better than propagating it.
  if (contentReferenceWhite > 0.0)
    m_referenceWhite = contentReferenceWhite;
  else
    m_referenceWhite = (icFloatNumber)icHdrDefaultContentReferenceWhite;

  // gamma <= 0 would inverse-map to a non-monotone OOTF, and a non-positive
  // peak luminance would zero the whole signal.
  m_hlgGamma = (hlgGamma > 0.0) ? hlgGamma : (icFloatNumber)icHlgDefaultGamma;
  m_hlgPeakLuminance = (hlgPeakLuminance > 0.0) ? hlgPeakLuminance
                                                : (icFloatNumber)icHlgDefaultPeakLuminance;

  switch (nTransferCharacteristics) {
    case icCicpTransferLinear:
      // Nothing analytic to apply: the identity is the EOTF, so the profile's
      // own TRC tags carry whatever linearisation there is.  See
      // UsesProfileCurves() for why this is the only case where they are used.
      m_bUseProfileCurves = true;
      m_bSupported = true;
      break;

    case icCicpTransferPQ:
    case icCicpTransferHLG:
      m_bSupported = true;
      break;

    default:
      // Clause 8.10.1 permits no other value in an HDR Profile.  Guessing a
      // transfer for one would silently render a profile that a conforming
      // implementation would refuse.
      break;
  }

  return m_bSupported;
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::ToLinear
 *
 * Purpose:
 *  Step a) of clause 8.10.2, followed by the change of normalisation the rest
 *  of the chain works in.
 *
 *  PQ: the EOTF gives luminance as a fraction of 10 000 cd/m^2, so the
 *  absolute luminance is that times icPqPeakLuminance and the reference
 *  white relative value is that over CRWL.  A signal at PQ code 0.58 - the
 *  ST 2084 encoding of 203 cd/m^2 - therefore comes out at 1.0 for the
 *  default reference white, which is the property the whole convention rests
 *  on.
 *
 *  HLG: the inverse OETF gives scene-linear values, the OOTF converts them to
 *  display light, and BT.2100's OOTF is scaled by the display peak luminance
 *  Lw.  The result is again absolute, so the same division by CRWL applies.
 *
 *  Linear: the caller applies the profile's TRC tags; here it is a copy.
 *
 * Args:
 *  dst = destination triplet, may alias src
 *  src = device-encoded R'G'B'
 ****************************************************************************
 */
void CIccHdrTransfer::ToLinear(icFloatNumber *dst, const icFloatNumber *src) const
{
  // Expressed as the composition of its two halves rather than written out a
  // second time, so that the split the A2B baker samples through
  // (ToLinearChannel into a 1-D curve, ChannelToReference into the CLUT) is
  // the same function this one is, by construction rather than by test.
  dst[0] = ToLinearChannel(src[0]);
  dst[1] = ToLinearChannel(src[1]);
  dst[2] = ToLinearChannel(src[2]);

  ChannelToReference(dst, dst);
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::ToLinearChannel
 *
 * Purpose:
 *  The part of ToLinear() that acts on one channel at a time, with its output
 *  normalised to the transfer's own peak rather than to the reference white.
 *
 *  Both properties are what make this half storable in a 1-D curve tag: a
 *  curveType's samples are one channel's and are unsigned 16-bit, so they
 *  cannot hold either a cross-channel term or a value above 1.0.  Everything
 *  that violates one of those - the HLG OOTF's dependence on all three
 *  channels, and the scale that turns peak-normalised light into reference
 *  white relative light - is in ChannelToReference() instead.
 *
 * Args:
 *  v = one device-encoded channel value
 *
 * Return:
 *  The channel linearised and normalised so that an encoded 1.0 gives 1.0.
 ****************************************************************************
 */
icFloatNumber CIccHdrTransfer::ToLinearChannel(icFloatNumber v) const
{
  if (m_nTransfer == icCicpTransferPQ)
    return icPqEotf(v);

  if (m_nTransfer == icCicpTransferHLG)
    return icHlgInverseOetf(v);

  return v;
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::ChannelToReference
 *
 * Purpose:
 *  The remainder of ToLinear(): everything that is not expressible one
 *  channel at a time, plus the change of normalisation from the transfer's
 *  own peak to the HDR reference white.
 *
 *  For PQ that is a single constant, 10 000 / CRWL.  For HLG it is the OOTF -
 *  whose gain is a function of the scene luminance of all three channels -
 *  followed by the same kind of constant, Lw / CRWL.  For Linear there is
 *  nothing to do: the profile's TRC tags are the linearisation and they
 *  already produce reference white relative values.
 *
 * Args:
 *  dst = destination triplet, may alias src
 *  src = peak-normalised linear RGB, i.e. ToLinearChannel() per channel
 ****************************************************************************
 */
void CIccHdrTransfer::ChannelToReference(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (m_nTransfer == icCicpTransferPQ) {
    icFloatNumber scale = (icFloatNumber)(icPqPeakLuminance / (double)m_referenceWhite);

    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
    return;
  }

  if (m_nTransfer == icCicpTransferHLG) {
    // Y_s uses the BT.2100 coefficients on the scene-linear triplet, before
    // the gain is applied - the OOTF is defined on scene luminance, not on
    // the display luminance it produces.
    icFloatNumber ys = (icFloatNumber)(icHlgLumaR * src[0] + icHlgLumaG * src[1] + icHlgLumaB * src[2]);
    icFloatNumber scale = icHlgOotfGain(ys, m_hlgGamma) *
                          (icFloatNumber)((double)m_hlgPeakLuminance / (double)m_referenceWhite);

    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
    return;
  }

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::GetPeakReferenceLevel
 *
 * Purpose:
 *  The reference white relative value an encoded 1.0 produces - the top of
 *  the range ToLinear() can return, and therefore the constant that maps the
 *  peak-normalised domain of ToLinearChannel() onto the reference white
 *  relative domain the gain curve and the matrix work in.
 *
 *  For HLG this is evaluated at the neutral, where Y_s is 1.0 and the OOTF's
 *  gain is 1.0; a saturated colour at full amplitude has a lower scene
 *  luminance and so a lower gain, never a higher one, which is what makes
 *  this a ceiling rather than a typical value.
 *
 * Return:
 *  10 000 / CRWL for PQ, Lw / CRWL for HLG, 1.0 for Linear.
 ****************************************************************************
 */
icFloatNumber CIccHdrTransfer::GetPeakReferenceLevel() const
{
  if (m_nTransfer == icCicpTransferPQ)
    return (icFloatNumber)(icPqPeakLuminance / (double)m_referenceWhite);

  if (m_nTransfer == icCicpTransferHLG)
    return (icFloatNumber)((double)m_hlgPeakLuminance / (double)m_referenceWhite);

  return (icFloatNumber)1.0;
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::FromLinear
 *
 * Purpose:
 *  Exact inverse of ToLinear().
 *
 *  The HLG direction is the one worth reading.  Display light is
 *  F_D = Lw * Y_s^(g-1) * E_s per channel, so the display luminance formed
 *  with the same coefficients is Y_D = Lw * Y_s^(g-1) * Y_s = Lw * Y_s^g.
 *  That gives Y_s = (Y_D / Lw)^(1/g) in closed form, and once Y_s is known
 *  the per-channel gain is known too - no iteration is needed to undo the
 *  OOTF.
 *
 * Args:
 *  dst = destination triplet, may alias src
 *  src = display-linear RGB relative to the HDR reference white
 ****************************************************************************
 */
void CIccHdrTransfer::FromLinear(icFloatNumber *dst, const icFloatNumber *src) const
{
  // The same composition as ToLinear(), in the other order: the part that
  // needs the whole triplet first, then the per-channel encoding.
  ReferenceToChannel(dst, src);

  dst[0] = FromLinearChannel(dst[0]);
  dst[1] = FromLinearChannel(dst[1]);
  dst[2] = FromLinearChannel(dst[2]);
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::ReferenceToChannel
 *
 * Purpose:
 *  Inverse of ChannelToReference(): reference white relative display light
 *  back to the peak-normalised per-channel domain.
 *
 *  The HLG direction is the one worth reading.  Display light is
 *  F_D = Lw * Y_s^(g-1) * E_s per channel, so the display luminance formed
 *  with the same coefficients is Y_D = Lw * Y_s^(g-1) * Y_s = Lw * Y_s^g.
 *  That gives Y_s = (Y_D / Lw)^(1/g) in closed form, and once Y_s is known
 *  the per-channel gain is known too - no iteration is needed to undo the
 *  OOTF.
 *
 * Args:
 *  dst = destination triplet, may alias src
 *  src = display-linear RGB relative to the HDR reference white
 ****************************************************************************
 */
void CIccHdrTransfer::ReferenceToChannel(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (m_nTransfer == icCicpTransferPQ) {
    icFloatNumber scale = (icFloatNumber)((double)m_referenceWhite / icPqPeakLuminance);

    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
    return;
  }

  if (m_nTransfer == icCicpTransferHLG) {
    // Back to absolute display light, then to the display luminance the OOTF
    // was driven by.
    icFloatNumber toAbsolute = (icFloatNumber)((double)m_referenceWhite / (double)m_hlgPeakLuminance);

    icFloatNumber fd[3];
    fd[0] = src[0] * toAbsolute;
    fd[1] = src[1] * toAbsolute;
    fd[2] = src[2] * toAbsolute;

    icFloatNumber yd = (icFloatNumber)(icHlgLumaR * fd[0] + icHlgLumaG * fd[1] + icHlgLumaB * fd[2]);

    if (!(yd > 0.0)) {
      dst[0] = dst[1] = dst[2] = 0.0;
      return;
    }

    icFloatNumber ys = (icFloatNumber)pow((double)yd, 1.0 / (double)m_hlgGamma);
    icFloatNumber gain = icHlgOotfGain(ys, m_hlgGamma);

    if (!(gain > 0.0)) {
      dst[0] = dst[1] = dst[2] = 0.0;
      return;
    }

    dst[0] = fd[0] / gain;
    dst[1] = fd[1] / gain;
    dst[2] = fd[2] / gain;
    return;
  }

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

/**
 ****************************************************************************
 * Name: CIccHdrTransfer::FromLinearChannel
 *
 * Purpose:
 *  Inverse of ToLinearChannel(): one peak-normalised linear channel back to
 *  its device encoding.
 *
 *  The clamp is each system's own ceiling rather than a convenience.  A
 *  peak-normalised value above 1.0 is a luminance the transfer has no code
 *  for, and both inverse functions return something above 1.0 for one - out
 *  of range for every consumer of the result.  Clamping here keeps the
 *  out-of-gamut handling in the one place that knows where the gamut ends.
 *
 * Args:
 *  v = one peak-normalised linear channel value
 ****************************************************************************
 */
icFloatNumber CIccHdrTransfer::FromLinearChannel(icFloatNumber v) const
{
  if (m_nTransfer == icCicpTransferPQ)
    return icPqInverseEotf(icHdrClampUnit(v));

  if (m_nTransfer == icCicpTransferHLG)
    return icHlgOetf(icHdrClampUnit(v));

  return v;
}

/**
 ****************************************************************************
 * Name: icHagcDerivePchipSlopes
 *
 * Purpose:
 *  Derive control-point slopes for a curve whose PCHIP Slope flag is set.
 *
 *  RECONSTRUCTION - see the header for the full caveat.  The rule implemented
 *  is Fritsch and Carlson's: at an interior point whose two adjacent secants
 *  have the same sign, the slope is their weighted harmonic mean, weighted by
 *  the interval widths; where the secants change sign or either is zero, the
 *  point is a local extremum and the slope is zero.  Endpoints use the
 *  standard one-sided three-point formula, clamped so that it cannot exceed
 *  three times the adjacent secant, which is the condition that keeps the
 *  end segment monotone.
 *
 * Args:
 *  x, y = control points, x strictly increasing
 *  n = number of control points
 *  slope = output, n values
 *
 * Return:
 *  false when n is zero or x is not strictly increasing, in which case slope
 *  is left untouched.
 ****************************************************************************
 */
bool icHagcDerivePchipSlopes(const icFloatNumber *x, const icFloatNumber *y,
                             icUInt8Number n, icFloatNumber *slope)
{
  if (!x || !y || !slope || !n)
    return false;

  icUInt8Number i;

  for (i = 1; i < n; i++) {
    if (!(x[i] > x[i - 1]))
      return false;
  }

  // A single control point has no secant to derive anything from.  Zero is
  // the only defensible slope: the curve is a point and every segment around
  // it is either the constant extension below x[0] or the logarithmic
  // extrapolation above it, neither of which consults the slope.
  if (n == 1) {
    slope[0] = 0.0;
    return true;
  }

  // Secant slopes; delta[i] spans x[i]..x[i+1], so there are n-1 of them.
  double delta[icHagcMaxControlPoints];
  double h[icHagcMaxControlPoints];

  for (i = 0; i + 1 < n; i++) {
    h[i] = (double)x[i + 1] - (double)x[i];
    delta[i] = ((double)y[i + 1] - (double)y[i]) / h[i];
  }

  if (n == 2) {
    slope[0] = slope[1] = (icFloatNumber)delta[0];
    return true;
  }

  for (i = 1; i + 1 < n; i++) {
    if (delta[i - 1] * delta[i] > 0.0) {
      double w1 = 2.0 * h[i] + h[i - 1];
      double w2 = h[i] + 2.0 * h[i - 1];
      slope[i] = (icFloatNumber)((w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]));
    }
    else {
      // Sign change or a flat secant: a local extremum, where a non-zero
      // slope is exactly what would make the cubic overshoot.
      slope[i] = 0.0;
    }
  }

  // Endpoints: the one-sided three-point estimate, with the two guards that
  // make it monotonicity preserving.
  double dEnd = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);

  if (dEnd * delta[0] <= 0.0)
    dEnd = 0.0;
  else if (delta[0] * delta[1] <= 0.0 && fabs(dEnd) > fabs(3.0 * delta[0]))
    dEnd = 3.0 * delta[0];

  slope[0] = (icFloatNumber)dEnd;

  icUInt8Number last = (icUInt8Number)(n - 1);
  double hl = h[last - 1], hl2 = h[last - 2];
  double dl = delta[last - 1], dl2 = delta[last - 2];

  dEnd = ((2.0 * hl + hl2) * dl - hl * dl2) / (hl + hl2);

  if (dEnd * dl <= 0.0)
    dEnd = 0.0;
  else if (dl * dl2 <= 0.0 && fabs(dEnd) > fabs(3.0 * dl))
    dEnd = 3.0 * dl;

  slope[last] = (icFloatNumber)dEnd;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Curve::Curve
 ****************************************************************************
 */
CIccHagcEvaluator::Curve::Curve()
{
  bBaseline = false;
  headroom = 0.0;
  nPoints = 0;
  coefSum = 0.0;

  memset(x, 0, sizeof(x));
  memset(y, 0, sizeof(y));
  memset(c3, 0, sizeof(c3));
  memset(c2, 0, sizeof(c2));
  memset(c1, 0, sizeof(c1));
  memset(c0, 0, sizeof(c0));
  memset(invdx, 0, sizeof(invdx));
  memset(coef, 0, sizeof(coef));
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Curve::Mix
 *
 * Purpose:
 *  Annex 1's input component mixing, in its general type 3 form:
 *
 *    mix = [R*kR + G*kG + B*kB + max*kMax + min*kMin + component*kComponent]
 *          / p_sum
 *
 *  Types 0 to 2 are that same expression with the coefficients the annex
 *  fixes for them - which is why CIccTagHagc's decode populates coef[] for
 *  every type rather than only for type 3, and why there is no switch here.
 *  For types 0 and 2 p_sum is 1.0 and the division is a no-op; the annex
 *  states the division only for type 3, and stating it for the others would
 *  be a difference only if their coefficients failed to sum to one, which by
 *  construction they do not.
 *
 * Args:
 *  mixed = three output values; they differ from each other only when
 *          k_component is non-zero
 *  src = the input triplet
 ****************************************************************************
 */
void CIccHagcEvaluator::Curve::Mix(icFloatNumber *mixed, const icFloatNumber *src) const
{
  icFloatNumber vmax = src[0] > src[1] ? src[0] : src[1];
  if (src[2] > vmax)
    vmax = src[2];

  icFloatNumber vmin = src[0] < src[1] ? src[0] : src[1];
  if (src[2] < vmin)
    vmin = src[2];

  icFloatNumber common = coef[icHagcCoefRed]   * src[0] +
                         coef[icHagcCoefGreen] * src[1] +
                         coef[icHagcCoefBlue]  * src[2] +
                         coef[icHagcCoefMax]   * vmax +
                         coef[icHagcCoefMin]   * vmin;

  icFloatNumber kc = coef[icHagcCoefComponent];

  // coefSum is guaranteed non-zero by Init(); annex 1 note 3 states that
  // p_sum cannot be zero, and a tag that says otherwise is refused there
  // rather than divided by here.
  icFloatNumber inv = (icFloatNumber)(1.0 / (double)coefSum);

  mixed[0] = (common + kc * src[0]) * inv;
  mixed[1] = (common + kc * src[1]) * inv;
  mixed[2] = (common + kc * src[2]) * inv;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Curve::Gain
 *
 * Purpose:
 *  Annex 1's gain evaluator G(x): constant below the first control point,
 *  the precomputed piecewise cubic between control points, and a logarithmic
 *  extrapolation above the last one.
 *
 *  The extrapolation is worth naming for what it does rather than for its
 *  form.  G(x) = y_last + log2(x_last / x) makes 2^G(x) * x equal to
 *  2^y_last * x_last, a constant - so every input above the last control
 *  point maps to the same output.  That is a hard clip at the top of the
 *  authored range, and it is the reason the inverse saturates there rather
 *  than failing.
 *
 * Args:
 *  v = the mixed input value
 *
 * Return:
 *  The gain exponent, to be raised as 2^G.
 ****************************************************************************
 */
icFloatNumber CIccHagcEvaluator::Curve::Gain(icFloatNumber v) const
{
  if (bBaseline || !nPoints)
    return 0.0;

  if (v <= x[0])
    return y[0];

  icUInt8Number last = (icUInt8Number)(nPoints - 1);

  if (v >= x[last]) {
    if (v == x[last])
      return y[last];

    // x[last] is positive here (v > x[last] >= x[0] >= 0), so the ratio is
    // well formed and the logarithm is of a value below 1, i.e. negative.
    return (icFloatNumber)((double)y[last] + log((double)x[last] / (double)v) / log(2.0));
  }

  // Linear scan rather than a bisection: the encoding caps the array at 32
  // control points, and at that size the scan wins on both branch prediction
  // and code size.
  icUInt8Number i = 0;
  while (i + 1 < nPoints && v >= x[i + 1])
    i++;

  double t = ((double)v - (double)x[i]) * (double)invdx[i];

  return (icFloatNumber)((((double)c3[i] * t + (double)c2[i]) * t + (double)c1[i]) * t + (double)c0[i]);
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::CIccHagcEvaluator
 ****************************************************************************
 */
CIccHagcEvaluator::CIccHagcEvaluator()
{
  m_bSupported = false;
  m_szUnsupported = "not initialized";
  m_bDerivedSlopes = false;
  m_nCurves = 0;
  m_targetHeadroom = 0.0;
  m_nCurveA = m_nCurveB = 0;
  m_weightA = 1.0;
  m_weightB = 0.0;
  m_bInvertible = false;
  m_bMonotone = false;
  m_bPerChannelMix = false;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Init
 *
 * Purpose:
 *  Build the headroom-ordered curve list from decoded HAGC metadata and
 *  precompute everything that does not depend on the target headroom.
 *
 * Args:
 *  meta = the decoded ST 2094-50 metadata block
 *
 * Return:
 *  IsSupported().
 ****************************************************************************
 */
bool CIccHagcEvaluator::Init(const icHagcMetadata &meta)
{
  icUInt8Number i, j;

  m_bSupported = false;
  m_bDerivedSlopes = false;
  m_nCurves = 0;
  m_bInvertible = false;
  m_bMonotone = false;
  m_bPerChannelMix = false;

  if (!meta.m_bUnpacked) {
    // The tag survived as opaque bytes but its content was never decoded, so
    // there is nothing to evaluate.  CIccTagHagc::Validate() has already
    // reported this; here it is just a fall-through to the next descriptor.
    m_szUnsupported = "metadata did not decode";
    return false;
  }

  if (!meta.m_bHeadroomAdaptiveToneMap) {
    // Proposal 1.2.2.2: with the flag clear the tag carries no tone-mapping
    // metadata at all and the operator is left to the implementer.  That is
    // descriptor b) of clause 8.10.3, not a) - so this evaluator declines.
    m_szUnsupported = "Headroom Adaptive Tone Map flag is not set";
    return false;
  }

  if (meta.m_bReferenceWhiteToneMapping) {
    // Proposal 1.2.2.5 defines this mode's parameters by reference to clause
    // C.3.8 of SMPTE ST 2094-50:2026, which this implementation does not
    // have.  Reporting it unsupported makes the CMM fall cleanly through the
    // descriptor precedence; inventing the derivation would render every such
    // profile differently from a conforming implementation.
    m_szUnsupported = "Reference White Tone Mapping requires SMPTE ST 2094-50 clause C.3.8";
    return false;
  }

  icUInt8Number nAlt = meta.GetNumAlternates();

  if (nAlt > icHagcMaxAlternates) {
    m_szUnsupported = "alternate image count exceeds the four the proposal permits";
    return false;
  }

  // No alternates is a supported configuration with a defined meaning, not an
  // empty one: proposal 1.2.2.6 says no tone mapping is performed and the
  // baseline is clamped to the target colour volume.  The curve list stays
  // empty and ClampsToTargetVolume() tells the caller what to do.
  if (!nAlt) {
    m_szUnsupported = NULL;
    m_bSupported = true;
    m_bInvertible = true;
    m_bMonotone = true;
    return true;
  }

  // The baseline occupies slot 0 of the working list; the alternates follow
  // and are then sorted with it by headroom.
  m_curves[0].bBaseline = true;
  m_curves[0].headroom = meta.m_baselineHeadroom;
  m_curves[0].coefSum = 1.0;
  m_nCurves = 1;

  for (i = 0; i < nAlt; i++) {
    const icHagcAlternateImage *pAlt = meta.GetAlternate(i);

    if (!pAlt) {
      m_szUnsupported = "alternate image count disagrees with the alternates present";
      m_nCurves = 0;
      return false;
    }

    Curve &c = m_curves[m_nCurves];

    c.bBaseline = false;
    c.headroom = pAlt->m_headroom;
    c.nPoints = pAlt->m_nControlPoints;

    if (!c.nPoints || c.nPoints > icHagcMaxControlPoints) {
      m_szUnsupported = "control point count outside 1..32";
      m_nCurves = 0;
      return false;
    }

    for (j = 0; j < c.nPoints; j++) {
      c.x[j] = pAlt->m_x[j];
      c.y[j] = pAlt->m_y[j];
    }

    // Strictly increasing X is what makes the piecewise cubic a function at
    // all; Validate() reports it as NonCompliant, and evaluating it anyway
    // would divide by a zero interval width.
    for (j = 1; j < c.nPoints; j++) {
      if (!(c.x[j] > c.x[j - 1])) {
        m_szUnsupported = "control point X coordinates are not strictly increasing";
        m_nCurves = 0;
        return false;
      }
    }

    icFloatNumber slope[icHagcMaxControlPoints];

    if (pAlt->m_bPchipSlope) {
      if (!icHagcDerivePchipSlopes(c.x, c.y, c.nPoints, slope)) {
        m_szUnsupported = "PCHIP slope derivation failed";
        m_nCurves = 0;
        return false;
      }
      m_bDerivedSlopes = true;
    }
    else {
      for (j = 0; j < c.nPoints; j++)
        slope[j] = pAlt->m_slope[j];
    }

    // Per-segment Hermite coefficients, exactly as annex 1's gain evaluator
    // states them:  M_i = (x_i+1 - x_i) * m_i, and C3..C0 from the two end
    // values and the two scaled slopes.
    for (j = 0; j + 1 < c.nPoints; j++) {
      double dx = (double)c.x[j + 1] - (double)c.x[j];
      double mi = dx * (double)slope[j];
      double mi1 = dx * (double)slope[j + 1];
      double yi = (double)c.y[j];
      double yi1 = (double)c.y[j + 1];

      c.invdx[j] = (icFloatNumber)(1.0 / dx);
      c.c3[j] = (icFloatNumber)(2.0 * yi + mi - 2.0 * yi1 + mi1);
      c.c2[j] = (icFloatNumber)(-3.0 * yi + 3.0 * yi1 - 2.0 * mi - mi1);
      c.c1[j] = (icFloatNumber)mi;
      c.c0[j] = (icFloatNumber)yi;
    }

    icFloatNumber sum = 0.0;

    for (j = 0; j < icHagcNumCoefficients; j++) {
      c.coef[j] = pAlt->m_coef[j];

      // Annex 1 note 1 confines every coefficient to [0, 1].  The bound is
      // load bearing rather than cosmetic: it is what makes the mixing
      // positively homogeneous with a positive scale, which is the property
      // the inverse relies on.
      if (!(c.coef[j] >= 0.0) || c.coef[j] > 1.0) {
        m_szUnsupported = "component mixing coefficient outside [0, 1]";
        m_nCurves = 0;
        return false;
      }

      sum += c.coef[j];
    }

    if (!(sum > 0.0)) {
      // Annex 1 note 3: p_sum cannot be zero.  An all-zero coefficient set
      // would also make the mixed value identically zero, i.e. one gain for
      // every pixel in the image.
      m_szUnsupported = "component mixing coefficients sum to zero";
      m_nCurves = 0;
      return false;
    }

    c.coefSum = sum;
    m_nCurves++;
  }

  // Sort the whole list, baseline included, by headroom.  Insertion sort over
  // at most five entries; the point is a stable well-defined order for the
  // bracket search, not speed.
  for (i = 1; i < m_nCurves; i++) {
    for (j = i; j > 0 && m_curves[j].headroom < m_curves[j - 1].headroom; j--) {
      Curve tmp = m_curves[j];
      m_curves[j] = m_curves[j - 1];
      m_curves[j - 1] = tmp;
    }
  }

  m_szUnsupported = NULL;
  m_bSupported = true;

  // Default to SDR.  A caller that never sets a target still gets a defined
  // evaluator rather than one whose blend indices are uninitialised.
  SetTargetHeadroom(0.0);

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::GetCurveHeadroom
 ****************************************************************************
 */
icFloatNumber CIccHagcEvaluator::GetCurveHeadroom(icUInt8Number n) const
{
  return n < m_nCurves ? m_curves[n].headroom : (icFloatNumber)0.0;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::SharesMixing
 *
 * Purpose:
 *  Whether the curves contributing at the current target agree on their
 *  component mixing, so that one mixed value drives both.  A baseline
 *  contributor imposes nothing, since its gain function is identically zero
 *  and never consults a mixed value.
 ****************************************************************************
 */
bool CIccHagcEvaluator::SharesMixing() const
{
  if (m_nCurveA == m_nCurveB)
    return true;

  const Curve &a = m_curves[m_nCurveA];
  const Curve &b = m_curves[m_nCurveB];

  if (a.bBaseline || b.bBaseline)
    return true;

  for (icUInt8Number i = 0; i < icHagcNumCoefficients; i++) {
    if (a.coef[i] != b.coef[i])
      return false;
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::SetTargetHeadroom
 *
 * Purpose:
 *  Bracket the target between two members of the headroom-ordered list,
 *  compute annex 1's two interpolation weights, and re-derive the properties
 *  that depend on which curves are in play - whether one mixed value drives
 *  the blend, and whether the blended curve can be inverted.
 *
 * Args:
 *  log2Headroom = the target headroom, in the log2 space the tag encodes
 *
 * Return:
 *  false when the evaluator is unsupported.
 ****************************************************************************
 */
bool CIccHagcEvaluator::SetTargetHeadroom(icFloatNumber log2Headroom)
{
  if (!m_bSupported)
    return false;

  m_targetHeadroom = log2Headroom;
  m_bInvertible = false;
  m_bMonotone = false;
  m_bPerChannelMix = false;

  if (!m_nCurves) {
    // The no-alternates case: identity, and trivially invertible.
    m_nCurveA = m_nCurveB = 0;
    m_weightA = 1.0;
    m_weightB = 0.0;
    m_bInvertible = true;
    m_bMonotone = true;
    return true;
  }

  icUInt8Number last = (icUInt8Number)(m_nCurves - 1);

  if (log2Headroom <= m_curves[0].headroom) {
    m_nCurveA = m_nCurveB = 0;
  }
  else if (log2Headroom >= m_curves[last].headroom) {
    m_nCurveA = m_nCurveB = last;
  }
  else {
    icUInt8Number i = 0;
    while (i + 1 < m_nCurves && log2Headroom > m_curves[i + 1].headroom)
      i++;

    m_nCurveA = i;
    m_nCurveB = (icUInt8Number)(i + 1);
  }

  if (m_nCurveA == m_nCurveB) {
    m_weightA = 1.0;
    m_weightB = 0.0;
  }
  else {
    // Annex 1's Output Evaluator weights.  The denominators are the same
    // interval with opposite signs; both are non-zero because the bracket
    // search only pairs two curves when the target lies strictly between
    // them, which needs their headrooms to differ.
    double hA = (double)m_curves[m_nCurveA].headroom;
    double hB = (double)m_curves[m_nCurveB].headroom;
    double ht = (double)log2Headroom;

    if (hB == hA) {
      m_nCurveB = m_nCurveA;
      m_weightA = 1.0;
      m_weightB = 0.0;
    }
    else {
      m_weightA = (icFloatNumber)((ht - hB) / (hA - hB));
      m_weightB = (icFloatNumber)((ht - hA) / (hB - hA));
    }
  }

  // Invertibility, condition one: the mixing has to be recoverable.  See the
  // header for why these two forms are the recoverable ones.
  if (SharesMixing()) {
    const Curve &c = m_curves[m_nCurveA].bBaseline ? m_curves[m_nCurveB] : m_curves[m_nCurveA];

    if (c.bBaseline) {
      // Both contributors are the baseline, so the blend is the identity.
      m_bInvertible = true;
      m_bMonotone = true;
      return true;
    }

    bool bComponentOnly = (c.coef[icHagcCoefComponent] > 0.0);

    for (icUInt8Number i = 0; i < icHagcNumCoefficients && bComponentOnly; i++) {
      if (i != icHagcCoefComponent && c.coef[i] != 0.0)
        bComponentOnly = false;
    }

    if (c.coef[icHagcCoefComponent] == 0.0 || bComponentOnly) {
      m_bPerChannelMix = bComponentOnly;

      // Condition two: t * 2^G(t) strictly increasing.  Sampled rather than
      // proved, because G is a blend of two independently authored cubics and
      // there is no closed form for its monotonicity.  The sample grid covers
      // every segment of both contributing curves; below the first control
      // point G is constant and the product is trivially increasing, and above
      // the last it is constant, which the inverse handles as saturation
      // rather than as a failure.
      icFloatNumber xEnd = 0.0;

      if (m_curves[m_nCurveA].nPoints)
        xEnd = m_curves[m_nCurveA].x[m_curves[m_nCurveA].nPoints - 1];
      if (m_curves[m_nCurveB].nPoints && m_curves[m_nCurveB].x[m_curves[m_nCurveB].nPoints - 1] > xEnd)
        xEnd = m_curves[m_nCurveB].x[m_curves[m_nCurveB].nPoints - 1];

      m_bMonotone = true;

      if (xEnd > 0.0) {
        const int nSamples = 512;
        double prev = -1.0;

        for (int s = 0; s <= nSamples; s++) {
          double t = (double)s * (double)xEnd / (double)nSamples;
          bool bValid = false;
          double g = (double)EvalGainExponent((icFloatNumber)t, &bValid);
          double f = t * pow(2.0, g);

          if (s && !(f > prev)) {
            m_bMonotone = false;
            break;
          }
          prev = f;
        }
      }

      m_bInvertible = m_bMonotone;
    }
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::IsIdentity
 ****************************************************************************
 */
bool CIccHagcEvaluator::IsIdentity() const
{
  if (!m_bSupported)
    return true;

  if (!m_nCurves)
    return true;

  if (m_nCurveA == m_nCurveB)
    return m_curves[m_nCurveA].bBaseline;

  // A blend is the identity only when both ends are, which cannot happen -
  // the list holds one baseline.
  return false;
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::EvalGainExponent
 *
 * Purpose:
 *  The blended gain exponent at one scalar input.  Only defined when the two
 *  contributing curves share a component mixing, since otherwise each is
 *  evaluated at a different mixed value and no single-argument G exists.
 *
 * Args:
 *  x = the mixed input value
 *  bValid = optional, set false when there is no single-argument G
 *
 * Return:
 *  The gain exponent; zero when bValid would be false.
 ****************************************************************************
 */
icFloatNumber CIccHagcEvaluator::EvalGainExponent(icFloatNumber x, bool *bValid /* =NULL */) const
{
  if (bValid)
    *bValid = false;

  if (!m_bSupported || !m_nCurves)
    return 0.0;

  if (!SharesMixing())
    return 0.0;

  if (bValid)
    *bValid = true;

  if (m_nCurveA == m_nCurveB)
    return m_curves[m_nCurveA].Gain(x);

  return (icFloatNumber)((double)m_weightA * (double)m_curves[m_nCurveA].Gain(x) +
                         (double)m_weightB * (double)m_curves[m_nCurveB].Gain(x));
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Apply
 *
 * Purpose:
 *  Annex 1's three steps for one triplet.
 *
 *  Each contributing curve is mixed with its *own* coefficients before its
 *  gain function is evaluated.  The annex writes the blend as
 *  G(x) = W_i*G_i(x) + W_i+1*G_i+1(x) with a single x, which is exact when
 *  the Common Component Mixing flag makes both curves share one coefficient
 *  set - the usual case.  When they do not share one, there is no single x to
 *  write, and evaluating each curve at the mixed value its own coefficients
 *  define is the only reading that uses the coefficients the tag actually
 *  carries for it.
 *
 *  The gain multiplies the original component, not the mixed value - that is
 *  what makes the operator "preserve the RGB primaries" in the sense clause
 *  8.10.2 b) requires.
 *
 * Args:
 *  dst = destination triplet, may alias src
 *  src = display-linear RGB relative to the HDR reference white
 ****************************************************************************
 */
void CIccHagcEvaluator::Apply(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (!m_bSupported || !m_nCurves || IsIdentity()) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return;
  }

  icFloatNumber in[3];
  in[0] = src[0];
  in[1] = src[1];
  in[2] = src[2];

  double g[3] = { 0.0, 0.0, 0.0 };
  icFloatNumber mixed[3];
  icUInt8Number i;

  const Curve &a = m_curves[m_nCurveA];

  if (!a.bBaseline) {
    a.Mix(mixed, in);
    for (i = 0; i < 3; i++)
      g[i] += (double)m_weightA * (double)a.Gain(mixed[i]);
  }

  if (m_nCurveB != m_nCurveA) {
    const Curve &b = m_curves[m_nCurveB];

    if (!b.bBaseline) {
      b.Mix(mixed, in);
      for (i = 0; i < 3; i++)
        g[i] += (double)m_weightB * (double)b.Gain(mixed[i]);
    }
  }

  for (i = 0; i < 3; i++)
    dst[i] = (icFloatNumber)(pow(2.0, g[i]) * (double)in[i]);
}

/**
 ****************************************************************************
 * Name: CIccHagcEvaluator::Invert
 *
 * Purpose:
 *  Recover the input triplet from an output triplet.
 *
 *  The mixing is positively homogeneous, so applying it to the output gives
 *  gain * mix(input) whenever one gain is common to all three channels; the
 *  input's mixed value is then the solution t of  t * 2^G(t) = mix(output),
 *  which is a scalar equation in one unknown on a function Init() has already
 *  established to be strictly increasing.  Bisection solves it: no derivative
 *  is needed, it cannot diverge on a monotone function, and 60 halvings take
 *  the bracket below the float epsilon of any value the domain can hold.
 *
 *  With component-only mixing the same equation holds per channel, with the
 *  channel's own output as the right-hand side.
 *
 * Args:
 *  dst = destination triplet, untouched on failure; may alias src
 *  src = the tone-mapped triplet
 *
 * Return:
 *  false when this configuration is not invertible.
 ****************************************************************************
 */
bool CIccHagcEvaluator::Invert(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (!m_bInvertible)
    return false;

  if (!m_nCurves || IsIdentity()) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return true;
  }

  const Curve &c = m_curves[m_nCurveA].bBaseline ? m_curves[m_nCurveB] : m_curves[m_nCurveA];

  // The top of the authored range, where the logarithmic extrapolation makes
  // t * 2^G(t) constant and the bisection has nothing to search.
  icFloatNumber xEnd = c.nPoints ? c.x[c.nPoints - 1] : (icFloatNumber)0.0;

  if (m_nCurveB != m_nCurveA && m_curves[m_nCurveB].nPoints &&
      m_curves[m_nCurveB].x[m_curves[m_nCurveB].nPoints - 1] > xEnd) {
    xEnd = m_curves[m_nCurveB].x[m_curves[m_nCurveB].nPoints - 1];
  }

  double fEnd = (double)xEnd * pow(2.0, (double)EvalGainExponent(xEnd));

  icFloatNumber out[3];
  out[0] = src[0];
  out[1] = src[1];
  out[2] = src[2];

  // The right-hand side: one value shared by all three channels for a common
  // gain, or the channel's own output for component-only mixing.
  icFloatNumber target[3];

  if (m_bPerChannelMix) {
    target[0] = out[0];
    target[1] = out[1];
    target[2] = out[2];
  }
  else {
    icFloatNumber mixed[3];
    c.Mix(mixed, out);
    target[0] = target[1] = target[2] = mixed[0];
  }

  // Initialised because the common-gain case below fills only gain[0] and
  // leaves the other two untouched by design; an uninitialised read is not
  // reachable, but leaving the slots undefined invites a later edit to make it
  // reachable without anything saying so.
  icFloatNumber gain[3] = { 1.0, 1.0, 1.0 };
  icUInt8Number i;

  // With a common mixed value all three channels solve the same equation, so
  // the bisection runs once rather than three times.  This is a per-pixel
  // path on the output side of every HDR profile, and the search is by far
  // the most expensive thing in it.
  icUInt8Number nSolve = m_bPerChannelMix ? 3 : 1;

  for (i = 0; i < nSolve; i++) {
    if (!(target[i] > 0.0)) {
      // At or below zero the gain curve's constant-below-x[0] region applies
      // directly; no search is possible or needed.
      gain[i] = (icFloatNumber)pow(2.0, (double)EvalGainExponent(0.0));
      continue;
    }

    if ((double)target[i] >= fEnd) {
      // Saturated: every input above the last control point produced this
      // same output, so x_last is the only defensible pre-image.
      gain[i] = (icFloatNumber)pow(2.0, (double)EvalGainExponent(xEnd));
      continue;
    }

    // Bracket [0, xEnd] holds the solution: the function is zero at zero,
    // strictly increasing, and reaches fEnd at xEnd.
    double lo = 0.0, hi = (double)xEnd, t = 0.0;

    for (int it = 0; it < 60; it++) {
      t = 0.5 * (lo + hi);
      double f = t * pow(2.0, (double)EvalGainExponent((icFloatNumber)t));

      if (f < (double)target[i])
        lo = t;
      else
        hi = t;
    }

    t = 0.5 * (lo + hi);
    gain[i] = (icFloatNumber)pow(2.0, (double)EvalGainExponent((icFloatNumber)t));
  }

  for (i = 0; i < nSolve; i++) {
    if (!(gain[i] > 0.0)) {
      // A zero gain destroyed the value; there is no pre-image to return.
      return false;
    }
  }

  if (m_bPerChannelMix) {
    for (i = 0; i < 3; i++)
      dst[i] = out[i] / gain[i];
  }
  else {
    // One common gain, recovered once and divided out of all three.
    for (i = 0; i < 3; i++)
      dst[i] = out[i] / gain[0];
  }

  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
