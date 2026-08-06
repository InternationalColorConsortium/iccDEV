/** @file
    File:       IccHdrToneMap.h

    Contains:   Header for the HDR tone-mapping step of ICC.1 clause 8.10.2 -
                the analytic PQ and HLG transfer functions, the reference
                white relative normalisation they feed, and the Headroom
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

#if !defined(_ICCHDRTONEMAP_H)
#define _ICCHDRTONEMAP_H

#include "IccDefs.h"
#include "IccTagHagc.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 ***********************************************************************
 * SMPTE ST 2084 (PQ) constants, in the exact rational forms the standard
 * states them in.  They are written as the divisions rather than as decimal
 * literals so that each one can be read straight against the standard, and
 * so that the two that are not exactly representable in binary floating
 * point (m1 and c1 are, m2 c2 and c3 are not all) round the same way here as
 * they do in every other implementation that transcribes them this way.
 *
 * These are cross-checkable in-tree: Testing/HDR/BT2100PQFullDisplay.xml
 * carries the same five values as segmented-curve formula parameters, and
 * its inverse curve carries 1/m2 = 0.01268331351565597.
 ***********************************************************************
 */
#define icPqM1 (2610.0 / 16384.0)          /* 0.1593017578125 */
#define icPqM2 (2523.0 / 4096.0 * 128.0)   /* 78.84375        */
#define icPqC1 (3424.0 / 4096.0)           /* 0.8359375       */
#define icPqC2 (2413.0 / 4096.0 * 32.0)    /* 18.8515625      */
#define icPqC3 (2392.0 / 4096.0 * 32.0)    /* 18.6875         */

/** Peak luminance of the PQ system in cd/m^2.  The PQ EOTF's output is
 * normalised to this, so it is the constant that turns a PQ EOTF result into
 * an absolute luminance (ICC.1 clause 8.10.2 a): "PQ in cd/m^2 / 10 000"). */
#define icPqPeakLuminance 10000.0

/**
 ***********************************************************************
 * Rec. ITU-R BT.2100 HLG constants.  b and c are not independent - the
 * standard defines b = 1 - 4a and c = 0.5 - a*ln(4a) - but they are given
 * here as the standard's own rounded decimals rather than recomputed from a,
 * because those decimals are what BT.2100 normatively states and what every
 * other implementation uses; recomputing them would move the 1/12 knee by
 * about 1e-9 and make bit-exact comparison against other implementations
 * fail for no gain.
 ***********************************************************************
 */
#define icHlgA 0.17883277
#define icHlgB 0.28466892
#define icHlgC 0.55991073

/** The scene-linear value at which the HLG OETF changes from its square-root
 * branch to its logarithmic branch (BT.2100 Table 5). */
#define icHlgKnee (1.0 / 12.0)

/** BT.2100 HLG system default OOTF gamma and nominal peak display luminance
 * in cd/m^2.  BT.2100 defines gamma as a function of Lw; 1.2 is its value at
 * the 1000 cd/m^2 reference the standard tabulates. */
#define icHlgDefaultGamma 1.2
#define icHlgDefaultPeakLuminance 1000.0

/** Rec. ITU-R BT.2100 luminance coefficients, used by the HLG OOTF to form
 * the scene luminance Y_s that drives the system gain.  BT.2100 states these
 * for its own (BT.2020) primaries and the HLG OOTF is defined in terms of
 * them regardless of what primaries the profile itself carries, so they are
 * deliberately *not* derived from the profile's matrix column tags. */
#define icHlgLumaR 0.2627
#define icHlgLumaG 0.6780
#define icHlgLumaB 0.0593

/**
 * SMPTE ST 2084 EOTF.  Maps a PQ-encoded value in [0, 1] to display
 * luminance normalised so that 1.0 is icPqPeakLuminance cd/m^2.
 *
 * Values below 0 return 0; the function is not defined there and the
 * alternative - returning a NaN from a negative fractional power - would
 * propagate through the whole chain.
 */
ICCPROFLIB_API icFloatNumber icPqEotf(icFloatNumber v);

/** Inverse of icPqEotf: normalised display luminance to PQ-encoded value. */
ICCPROFLIB_API icFloatNumber icPqInverseEotf(icFloatNumber l);

/** Rec. ITU-R BT.2100 HLG OETF: scene-linear [0, 1] to HLG signal [0, 1]. */
ICCPROFLIB_API icFloatNumber icHlgOetf(icFloatNumber e);

/** Inverse HLG OETF: HLG signal [0, 1] to scene-linear [0, 1]. */
ICCPROFLIB_API icFloatNumber icHlgInverseOetf(icFloatNumber v);

/**
 * The HLG OOTF's system gain Y_s^(gamma - 1), where Y_s is the scene
 * luminance formed with the BT.2100 coefficients.  Split out from the OOTF
 * itself because both the forward and the inverse direction need it and
 * because it is the one place the gamma enters.
 */
ICCPROFLIB_API icFloatNumber icHlgOotfGain(icFloatNumber sceneLuminance, icFloatNumber gamma);

/**
 ***********************************************************************
 * Class: CIccHdrTransfer
 *
 * Purpose:
 *  Step a) of clause 8.10.2 - the HDR EOTF - together with the change of
 *  normalisation that makes its output usable by the rest of the chain.
 *
 *  The two are one object because they are not separable.  Clause 8.10.2 a)
 *  defines each transfer characteristic's output in its own convention (PQ in
 *  cd/m^2 / 10 000, HLG in scene-referred BT.2100 units, Linear as-is), while
 *  steps b) and c) both need display-linear values normalised so that 1.0 is
 *  the HDR reference white: the HAGC gain curve's control-point X coordinates
 *  are in that scale (their decode caps them at 64.0, i.e. six stops of
 *  headroom over reference white), and the matrix column tags produce
 *  PCSXYZ Y = 1.0 for the media white point.  Converting between the two
 *  conventions needs the content reference white luminance of clause 8.10.4,
 *  which is why it is a member here rather than a caller's concern.
 *
 *  PROPOSAL-ISSUE HDR-01 (design-level; the highest-value item in the set) --
 *  clause 8.10.2 never states where the conversion between those two
 *  conventions happens.  Step a) leaves PQ as a fraction of 10 000 cd/m2 and
 *  HLG scene-referred; step c)'s matrix columns and the HAGC control-point X
 *  coordinates both expect reference-white-relative light (the HAGC encoding
 *  caps X at 64.0, six stops over reference white, so it is plainly not a
 *  fraction of 10 000 cd/m2); step b) mentions the CRWL of 8.10.4 only as
 *  something that "may inform" the operator.  A literal reading evaluates the
 *  gain curve at an abscissa wrong by 10 000 / CRWL, about 49x by default,
 *  producing a smooth and uniformly wrong image that nothing detects.  This
 *  class owns the conversion so that the ruling has one place to live.
 *
 *  PROPOSAL-ISSUE WP-03 (design-level) -- the A2B0 white paper describes the
 *  HLG A curve as the BT.2100 EOTF "with the system gamma included", which
 *  cannot exist: the OOTF's gain is a function of all three channels, so it
 *  has no per-channel form in any revision.  Resolved by the split below -
 *  the inverse OETF is per channel, the OOTF is not.
 *
 *  This is also why the class works on a whole RGB triplet rather than one
 *  channel at a time: the HLG OOTF's gain is a function of the scene
 *  luminance of all three channels, so an HLG "per-channel curve" does not
 *  exist.
 *
 *  Parse once, evaluate many: everything is fixed by Init(), so ToLinear()
 *  and FromLinear() are const and safe to call from multiple threads.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccHdrTransfer
{
public:
  CIccHdrTransfer();

  /**
   * Configure from a profile's resolved HDR parameters.
   *
   * nTransferCharacteristics is the cicpTag field; only the three values
   * clause 8.10.1 permits an HDR Profile to carry are supported and anything
   * else leaves the object unsupported rather than guessing at a transfer.
   *
   * contentReferenceWhite is the resolved CRWL of clause 8.10.4 in cd/m^2
   * (the 203 cd/m^2 default already applied by the caller).
   *
   * hlgGamma and hlgPeakLuminance parameterise the HLG OOTF and are ignored
   * for the other transfer characteristics.
   *
   * Returns false when the configuration is not one this class can evaluate.
   */
  bool Init(icUInt8Number nTransferCharacteristics,
            icFloatNumber contentReferenceWhite,
            icFloatNumber hlgGamma = (icFloatNumber)icHlgDefaultGamma,
            icFloatNumber hlgPeakLuminance = (icFloatNumber)icHlgDefaultPeakLuminance);

  bool IsSupported() const { return m_bSupported; }

  /**
   * True when this transfer characteristic has no analytic EOTF of its own
   * and the profile's own TRC tags are the linearisation.
   *
   * This is the Linear case (TransferCharacteristics = 8), where clause
   * 8.10.2 a)'s "the EOTF implied by cicpTag.TransferCharacteristics" is the
   * identity and the parenthetical "(equivalent to applying redTRCTag,
   * greenTRCTag, blueTRCTag)" is the whole of the definition.  For PQ and HLG
   * the two are *not* interchangeable and the analytic form is the one to
   * use: a sampled curveType TRC clamps its output at 1.0, so it cannot
   * represent display-linear light above reference white at all.
   */
  bool UsesProfileCurves() const { return m_bUseProfileCurves; }

  /** Convert device-encoded R'G'B' to display-linear RGB normalised so that
   * 1.0 is the content HDR reference white.  src and dst may alias. */
  void ToLinear(icFloatNumber *dst, const icFloatNumber *src) const;

  /** Inverse of ToLinear(). */
  void FromLinear(icFloatNumber *dst, const icFloatNumber *src) const;

  /**
   * ToLinear() split at the seam a lutAToBType has between its A curves and
   * its CLUT: the per-channel half, whose output is normalised to the
   * transfer's own peak so that it stays inside a curveType's 0..1 range.
   *
   * ToLinear() is literally ToLinearChannel() on each channel followed by
   * ChannelToReference(), so a baker that samples the two separately gets the
   * same function and not a second transcription of it.
   */
  icFloatNumber ToLinearChannel(icFloatNumber v) const;

  /** The rest of ToLinear(): the HLG OOTF, which is a function of the whole
   * triplet, and the scale onto reference white relative units.  dst and src
   * may alias. */
  void ChannelToReference(icFloatNumber *dst, const icFloatNumber *src) const;

  /** FromLinear() split at the same seam, in the other order: the triplet
   * half, which undoes the OOTF and the reference white scale. */
  void ReferenceToChannel(icFloatNumber *dst, const icFloatNumber *src) const;

  /** FromLinear()'s per-channel half - the device encoding of one
   * peak-normalised linear value, clamped at the transfer's own ceiling. */
  icFloatNumber FromLinearChannel(icFloatNumber v) const;

  /** The reference white relative value an encoded 1.0 produces, i.e. the
   * constant ChannelToReference() applies at the neutral. */
  icFloatNumber GetPeakReferenceLevel() const;

  icUInt8Number GetTransferCharacteristics() const { return m_nTransfer; }
  icFloatNumber GetContentReferenceWhite() const { return m_referenceWhite; }

protected:
  bool m_bSupported;
  bool m_bUseProfileCurves;
  icUInt8Number m_nTransfer;
  icFloatNumber m_referenceWhite;
  icFloatNumber m_hlgGamma;
  icFloatNumber m_hlgPeakLuminance;
};

/**
 * Derive the control-point slopes of a gain curve whose PCHIP Slope flag is
 * set, i.e. whose slope angles were not carried in the tag (HAGC proposal
 * 1.1.3.5).
 *
 * PROPOSAL-ISSUE HAGC-05 (external dependency) -- the derivation this function
 * implements lives in a clause of SMPTE ST 2094-50:2026 that is not supplied
 * with the amendment, so the amendment is not independently implementable here.
 * UsesDerivedSlopes() exists so a caller can report when a curve relied on the
 * reconstruction below.
 *
 * RECONSTRUCTION, NOT A TRANSCRIPTION.  Clause 1.1.3.5 delegates the
 * derivation to clause C.3.9 of SMPTE ST 2094-50:2026, which this
 * implementation does not have.  What is implemented here is the standard
 * piecewise cubic Hermite interpolating polynomial slope rule that the
 * flag's own name points at - Fritsch and Carlson's monotonicity-preserving
 * weighted harmonic mean of the adjacent secant slopes, as used by
 * MATLAB/SciPy pchip - because that is what "PCHIP" denotes everywhere else
 * and because a curve built from it is guaranteed monotone between control
 * points, which is what the inverse path in CIccHagcEvaluator depends on.
 *
 * It is nevertheless a reconstruction: if C.3.9 specifies a different slope
 * rule, curves that set this flag will evaluate slightly differently here
 * than in a conforming implementation, while curves that carry explicit
 * slope angles are unaffected.  CIccHagcEvaluator::UsesDerivedSlopes()
 * reports when any contributing curve went through this function.
 *
 * x must be strictly increasing over n points.  Returns false, leaving slope
 * untouched, when it is not or when n is 0.
 */
ICCPROFLIB_API bool icHagcDerivePchipSlopes(const icFloatNumber *x, const icFloatNumber *y,
                                            icUInt8Number n, icFloatNumber *slope);

/**
 ***********************************************************************
 * Class: CIccHagcEvaluator
 *
 * Purpose:
 *  The Headroom Adaptive Gain Curve Function of the HAGC amendment's
 *  informative annex 1: input component mixing, gain evaluator, output
 *  evaluator, evaluated at a target headroom the consumer supplies.
 *
 *  Split into two phases on purpose.  Init() takes the decoded metadata and
 *  does everything that depends only on it - validation, PCHIP slope
 *  derivation, per-segment Hermite coefficients, ordering the headroom list.
 *  SetTargetHeadroom() then does everything that depends only on H_target -
 *  bracketing the target between two members of that list and computing the
 *  two interpolation weights.  After that Apply() is a const function of the
 *  pixel alone, which is what lets a single evaluator be shared by every
 *  pixel of an image without per-pixel state, and by multiple threads
 *  without synchronisation.
 *
 *  H_target is never encoded in the profile (clause 8.10.2 NOTE 5); it comes
 *  from the consumer, which is why setting it is a separate call.
 *
 *  All headroom values here are in log2 space, as the tag encodes them: 0.0
 *  is SDR (peak luminance equal to reference white), 1.0 is one stop of
 *  headroom, and the encoding caps them at 6.0.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccHagcEvaluator
{
public:
  CIccHagcEvaluator();

  /**
   * Build the evaluator from decoded HAGC metadata.  The metadata is copied
   * in the sense that nothing is retained by reference, so meta may go out of
   * scope afterwards.
   *
   * Returns IsSupported().  A false return is not an error: it is the signal
   * for the caller to move down the tone-mapping descriptor precedence of
   * clause 8.10.3, which is exactly what a CMM that does not implement this
   * descriptor would do.  GetUnsupportedReason() says which condition fired.
   */
  bool Init(const icHagcMetadata &meta);

  bool IsSupported() const { return m_bSupported; }

  /** Static text naming the condition that made IsSupported() false, or NULL
   * when it is true.  Never a formatted or allocated string - callers embed
   * it in validation and trace output. */
  const icChar *GetUnsupportedReason() const { return m_szUnsupported; }

  /**
   * Fix the target headroom, in log2 space, and precompute the blend of the
   * two adjacent gain curves for it (annex 1, Output Evaluator).
   *
   * A target below the lowest or above the highest headroom in the tag is
   * clamped to that endpoint's curve rather than extrapolated between
   * curves: the annex defines interpolation only *between* two adjacent
   * headroom values, and extrapolating a gain exponent past the last curve
   * would amplify by an amount no one authored.
   *
   * Returns false when the evaluator is unsupported, in which case the target
   * is not recorded and Apply() stays the identity.
   */
  bool SetTargetHeadroom(icFloatNumber log2Headroom);

  icFloatNumber GetTargetHeadroom() const { return m_targetHeadroom; }

  /** True when the configured target makes this evaluator a no-op, either
   * because the tag carries no alternate images at all or because the target
   * resolves to the baseline curve alone (whose gain function is identically
   * zero, i.e. a gain of 1). */
  bool IsIdentity() const;

  /**
   * True when the tag carries no alternate images.  HAGC proposal 1.2.2.6
   * gives that case a meaning of its own: no tone mapping is to be performed
   * and the baseline image is to be clamped to the target colour volume.  The
   * clamp is the caller's to apply because it needs the target colour volume,
   * which is not in the tag.
   */
  bool ClampsToTargetVolume() const { return m_bSupported && !m_nCurves; }

  /** True when any gain curve this evaluator holds had its slopes derived by
   * icHagcDerivePchipSlopes() rather than read from the tag.  Exposed because
   * that derivation is a reconstruction of an unavailable SMPTE clause. */
  bool UsesDerivedSlopes() const { return m_bDerivedSlopes; }

  /**
   * Apply the gain curve to one display-linear RGB triplet, normalised so
   * that 1.0 is the HDR reference white.  dst and src may alias.
   *
   * This is annex 1's three steps in order: component mixing to form the
   * curve input for each contributing curve, the piecewise cubic gain
   * evaluator, and the output evaluator's gain = 2^G(x) applied as a
   * multiplier on the *original* component (not on the mixed value).
   */
  void Apply(icFloatNumber *dst, const icFloatNumber *src) const;

  /**
   * Invert Apply() for one triplet.  Returns false when this configuration is
   * not invertible, in which case dst is untouched and the caller must fall
   * back to a BToA tag.
   *
   * See IsInvertible() for what "not invertible" means here; it is a property
   * of the component mixing and of the curve's monotonicity, both fixed by
   * Init() and SetTargetHeadroom(), so a caller can test it once at Begin()
   * rather than per pixel.
   */
  bool Invert(icFloatNumber *dst, const icFloatNumber *src) const;

  /**
   * True when Invert() can work for the current target headroom.
   *
   * Two conditions have to hold.  First the component mixing has to be one
   * whose gain is recoverable from the output alone.  Every mixing form the
   * tag can express is positively homogeneous - max, min and non-negative
   * weighted sums all satisfy mix(a*v) = a*mix(v) - so when the mixed value
   * is common to all three channels the mix of the *output* triplet equals
   * gain * mix(input), and the gain can be recovered by solving the scalar
   * equation t*2^G(t) = mix(output).  That holds when k_component is zero
   * (types 0 and 2, and type 3 with a zero component coefficient) because
   * then all three channels share one gain; and it holds again, per channel,
   * when k_component is the *only* non-zero coefficient (type 1), because
   * then the mixed value of a channel is that channel.  Any other type 3
   * blend couples the channels with different gains and is not invertible
   * this way.  Second, when the target blends two curves, both have to use
   * the same mixing coefficients, or "the" mixed value is not one number.
   *
   * The second condition is that t*2^G(t) is strictly increasing over the
   * control-point range, which Init() checks by sampling; a curve that is not
   * is one where two different inputs produce the same output and no inverse
   * exists.
   */
  bool IsInvertible() const { return m_bInvertible; }

  /**
   * The gain exponent G(x) at the configured target headroom - the blended
   * piecewise cubic, before the 2^G.  Public because the A2B0 baking path
   * and the regression tests both need to sample the curve directly rather
   * than through a pixel.
   *
   * Only meaningful when all contributing curves share one component mixing,
   * which is the case IsInvertible() already requires; when they do not,
   * there is no single G(x) and Apply() evaluates each curve at its own mixed
   * value instead.  bValid, when supplied, reports which case this is.
   */
  icFloatNumber EvalGainExponent(icFloatNumber x, bool *bValid = NULL) const;

  /** Number of gain curves in the headroom-ordered list, baseline included.
   * Zero when the tag carried no alternate images. */
  icUInt8Number GetNumCurves() const { return m_nCurves; }

  /** Headroom of curve n in the ordered list, in log2 space. */
  icFloatNumber GetCurveHeadroom(icUInt8Number n) const;

protected:
  /**
   * One gain curve, with its Hermite segments precomputed.
   *
   * The baseline curve is represented as a member of this array with
   * bBaseline set rather than as a special case in the blending code: annex
   * 1 note 1 defines it as the Zero Color Gain Function, so it is an ordinary
   * member of the ordered headroom list whose G(x) happens to be identically
   * zero, and treating it as one keeps the bracket search from having to know
   * about it.
   */
  class Curve {
  public:
    Curve();

    bool bBaseline;
    icFloatNumber headroom;          /* log2 space */

    icUInt8Number nPoints;
    icFloatNumber x[icHagcMaxControlPoints];
    icFloatNumber y[icHagcMaxControlPoints];

    /* Hermite coefficients of segment i, spanning x[i]..x[i+1], in the form
     * annex 1's gain evaluator states: f(t) = ((c3*t + c2)*t + c1)*t + c0 with
     * t = (x - x[i]) * invdx[i].  Precomputed because Apply() is per pixel and
     * these are per curve. */
    icFloatNumber c3[icHagcMaxControlPoints];
    icFloatNumber c2[icHagcMaxControlPoints];
    icFloatNumber c1[icHagcMaxControlPoints];
    icFloatNumber c0[icHagcMaxControlPoints];
    icFloatNumber invdx[icHagcMaxControlPoints];

    icFloatNumber coef[icHagcNumCoefficients];
    icFloatNumber coefSum;           /* p_sum of annex 1's type 3 formula */

    /** The mixed value of a triplet under this curve's coefficients.  Writes
     * three values because k_component makes the mix per channel. */
    void Mix(icFloatNumber *mixed, const icFloatNumber *src) const;

    /** G(x) for this curve alone. */
    icFloatNumber Gain(icFloatNumber v) const;
  };

  /** True when the two curves contributing at the current target headroom
   * agree on their component mixing, so that a single mixed value drives
   * both.  Set by SetTargetHeadroom(). */
  bool SharesMixing() const;

  bool m_bSupported;
  const icChar *m_szUnsupported;
  bool m_bDerivedSlopes;

  icUInt8Number m_nCurves;
  Curve m_curves[icHagcMaxAlternates + 1];   /* alternates plus the baseline */

  icFloatNumber m_targetHeadroom;

  /* The blend fixed by SetTargetHeadroom(): curve indices and their weights.
   * When the target lands exactly on a curve, or outside the list, both
   * indices are that curve and m_weightB is zero. */
  icUInt8Number m_nCurveA, m_nCurveB;
  icFloatNumber m_weightA, m_weightB;

  bool m_bInvertible;
  bool m_bMonotone;                 /* t*2^G(t) strictly increasing */
  bool m_bPerChannelMix;            /* k_component is the only non-zero coefficient */
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCHDRTONEMAP_H)
