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
 * the 1000 cd/m^2 reference the standard tabulates.
 *
 * PROPOSAL-ISSUE (no register key -- the ICC corpus is silent; BT.2100 is
 * not): no HDR Profile carries Lw, and because BT.2100 makes gamma a function
 * of it, fixing gamma at 1.2 implicitly pins Lw at 1000 cd/m^2 for every HLG
 * profile, whatever display the profile actually describes.  Clause 8.10.4
 * NOTE 9 licenses exactly this - a CMM lacking metadata "may rely on the
 * conventions of the cicpTag.TransferCharacteristics (e.g. ... the nominal HLG
 * peak per Rec. ITU-R BT.2100)" - but it names no value, so the number below
 * is this implementation's choice and not a transcription.
 *
 * What is NOT missing is the formula.  BT.2100-3 (02/2025) Note 5f, read
 * 2026-09-01, gives gamma = 1,2 + 0,42 * log10(Lw / 1000) over the usual
 * production monitoring range of Lw = 400 to 2 000 cd/m^2, and gamma = 1,2 *
 * k^log2(Lw / 1000) with k = 1,111 outside it - two formulas with a branch at
 * each end of that range, which any per-profile implementation has to get
 * right.  So the only open question is where Lw comes from; the HDR Display
 * DCV maximum luminance this library already parses is the natural source.
 * Wiring it in changes rendered output for every HLG profile that carries one
 * and is deliberately not done here. */
#define icHlgDefaultGamma 1.2
#define icHlgDefaultPeakLuminance 1000.0

/** Rec. ITU-R BT.2100 luminance coefficients, used by the HLG OOTF to form
 * the scene luminance Y_s that drives the system gain.  BT.2100 states these
 * for its own (BT.2020) primaries and the HLG OOTF is defined in terms of
 * them regardless of what primaries the profile itself carries, so they are
 * deliberately *not* derived from the profile's matrix column tags.
 *
 * PROPOSAL-ISSUE (no register key): clause 8.10 never says which luma
 * coefficients the HLG OOTF uses, and a cicpTag may declare any
 * ColourPrimaries alongside TransferCharacteristics 18.  Holding these at
 * BT.2020 is defensible - BT.2100-3 defines the OOTF in exactly these terms,
 * Ys = 0,2627 Rs + 0,6780 Gs + 0,0593 Bs, and an HLG signal is a BT.2100
 * signal - but for a profile declaring, say, ColourPrimaries 1 it silently
 * forms Y_s from primaries the profile does not use.
 *
 * Read against H.273 (V4) on 2026-09-01, the alternative is better supported
 * than this comment previously said.  Table 4 ties these same three numbers to
 * MatrixCoefficients 9 (BT.2020 / BT.2100 non-constant luminance), and
 * equations 39 to 44 give a general chromaticity-derived K_R / K_B for
 * MatrixCoefficients 12 and 13 - a mechanism for computing luma constants from
 * whatever primaries a profile declares.  The cicpTag even carries the
 * MatrixCoefficients field that would select between them; icGetHdrProfileInfo
 * reports it, and nothing in this file consults it.
 *
 * So the choice is between applying BT.2100's OOTF as BT.2100 defines it, and
 * applying a primaries-consistent variant of it that no text asks for.  That
 * is a ruling about rendered output, not a gap in the corpus, and it is
 * deliberately not taken here. */
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
 *  clause 8.10.2 states one normalisation in step a) and depends on the
 *  opposite one in NOTE 6, and only the second can be right.  Step a) leaves
 *  PQ as a fraction of 10 000 cd/m2; NOTE 6 permits an identity operator and
 *  says the chain then "reduces to the conventional matrix/TRC transform of
 *  Annex F.3" with PCSXYZ that "can exceed the conventional SDR range".  With
 *  an identity operator the chain is step a) -> matrix and nothing else, so
 *  step a) as written puts diffuse white at 203/10 000 = 0.0203 and the whole
 *  image about fifty times BELOW the SDR range rather than above it.  Under
 *  reference-white-relative light diffuse white is 1.0 and the 10 000 cd/m2
 *  peak is 49.26, which is what NOTE 6, Annex F.3 and ICC.1's own note on a
 *  media white point Y above 1.0 all describe.
 *
 *  The HAGC encoding corroborates it - control-point X caps at 64.0, six stops
 *  over reference white, so it is plainly not a fraction of 10 000 cd/m2 -
 *  though HAGC delegates the curve's domain to SMPTE ST 2094-50 and does not
 *  state it directly.  A literal reading of step a) evaluates the gain curve at
 *  an abscissa wrong by 10 000 / CRWL, about 49x by default, producing a smooth
 *  and uniformly wrong image that nothing detects.  This class owns the
 *  conversion so that the ruling has one place to live.
 *
 *  PROPOSAL-ISSUE WP-03 (design-level) -- the A2B0 white paper describes the
 *  HLG A curve as the BT.2100 EOTF "with the system gamma included", which
 *  cannot exist: the OOTF's gain is a function of all three channels, so it
 *  has no per-channel form in any revision.  Resolved by the split below -
 *  the inverse OETF is per channel, the OOTF is not.
 *
 *  PROPOSAL-ISSUE HDR-08 (design-level) -- the same split is the ruling for
 *  clause 8.10.2 a), and for the same reason.  That step calls code point 18's
 *  function an EOTF, requires it per channel, and asks for a scene-referred
 *  output; the HLG EOTF is OOTF o OETF^-1, which is neither per channel nor
 *  scene referred, while the per-channel scene-referred function - the inverse
 *  OETF - is what 8.10.6 names and what H.273 code point 18 identifies.
 *
 *  H.273 settles this in its own words, checked 2026-09-01 against the (V4)
 *  (07/2024) text: clause 8.2 says a TransferCharacteristics value indicates
 *  EITHER the reference opto-electronic function as a function of source input
 *  linear optical intensity Lc, OR the inverse of the reference
 *  electro-optical function as a function of output linear optical intensity
 *  Lo - and Table 3 writes code point 16 (PQ) in Lo and code point 18 (HLG) in
 *  Lc, remarking "ARIB STD-B67".  So 8.10.2 a) is contradicted by the document
 *  it cites for the function, not merely by BT.2100.  The
 *  29-08-2026 revision removes the TRC tags, so no tag remains that could have
 *  carried a per-channel curve and the step has to be read literally.  Ruled
 *  the same way here: inverse OETF in the per-channel position, OOTF where a
 *  three-channel function belongs.  See WP-03 above - one split, two texts.
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
 * PROPOSAL-ISSUE HAGC-05 (external dependency) -- clause 1.1.3.5 delegates
 * this derivation to clause C.3.9 of SMPTE ST 2094-50:2026, which is not
 * supplied with the amendment, so the amendment is not independently
 * implementable from ICC documents alone.  UsesDerivedSlopes() exists so a
 * caller can report when a curve relied on this function.
 *
 * SOURCE, AND ITS PROVENANCE.  C.3.9 was read on 2026-09-01 -- but from the SECOND
 * PUBLIC COMMITTEE DRAFT of ST 2094-50, dated 2026-02-23, at
 * github.com/SMPTE/st2094-50.
 *
 * That is the CURRENT PUBLIC STATE of the document, not a lesser substitute
 * for one: checked 2026-09-02, the repository carries no tags, no releases and
 * that single artifact, and nothing has been pushed to it since 2026-05-22.
 * What makes it worth flagging is narrower - its README dates the review
 * period as having ended 2026-03-16, and CONTRIBUTING.md points active
 * drafting at a members-only private repository.  So this is the latest text
 * anyone outside SMPTE can read, and the comments raised in that review may
 * already have changed exactly the parts flagged below.  The SMPTE licence
 * forbids reproducing the text, so it is described here and never quoted.
 *
 * C.3.9 defines, over h_i = x_i+1 - x_i and the secants s_i:
 *   - interior points i in 1..N-2: zero when sign(s_i-1) != sign(s_i), else
 *     3(h_i-1 + h_i) s_i-1 s_i / ((2h_i-1 + h_i) s_i-1 + (h_i-1 + 2h_i) s_i);
 *   - the two endpoints (N >= 3): the one-sided three-point estimate;
 *   - N = 2: both slopes are the single secant; N = 1: unused.
 * Its NOTE states the result is equivalent to the PCHIP algorithm of
 * DOI:10.1137/0905021 (Fritsch and Butland).
 *
 * The interior formula below is that formula exactly.  TWO DELIBERATE
 * DIVERGENCES remain, both in the direction of the NOTE rather than of the
 * draft's own formulas, and both are pinned by hdr-tonemap.cpp:
 *
 *   1. ENDPOINTS.  C.7 and C.8 give the three-point estimate with no sign
 *      test and no magnitude limit, while the PCHIP algorithm their own NOTE
 *      cites clamps both.  The difference is not cosmetic: for x = {0, 1, 3},
 *      y = {0, 2, 3} - data that increases throughout - the draft's C.8
 *      yields a slope of -0,5 at the last point, so the final segment dips
 *      below the value it started from.  A gain curve is monotone by intent
 *      and CIccHagcEvaluator's inverse depends on it, so the clamps are kept.
 *   2. A FLAT PAIR.  When two consecutive secants are both zero their signs
 *      are equal, so C.9 takes its "otherwise" branch, where numerator and
 *      denominator are both zero.  Three collinear flat control points make
 *      the draft formula 0/0.  Zero is used instead, which is the limit from
 *      every direction and what PCHIP gives.
 *
 * Curves that carry explicit slope angles are unaffected by any of this.
 * CIccHagcEvaluator::UsesDerivedSlopes() reports when a contributing curve
 * went through this function.
 *
 * x must be strictly increasing over n points, and n must be 1 to
 * icHagcMaxControlPoints - the working arrays are sized by that maximum, so a
 * larger n is refused rather than accommodated; a gain curve cannot carry more
 * points than that in any case, its count being a 5 bit last index.  Returns
 * false, leaving slope untouched, when either precondition fails or when n
 * is 0.
 */
ICCPROFLIB_API bool icHagcDerivePchipSlopes(const icFloatNumber *x, const icFloatNumber *y,
                                            icUInt8Number n, icFloatNumber *slope);

/**
 ***********************************************************************
 * Derive the alternate images of a Reference White Tone Mapping tag.
 *
 * PROPOSAL-ISSUE HAGC-05 (external dependency) -- proposal 1.2.2.5 sets four
 * header fields to zero on the wire and says their effective values "shall
 * not be read from the tag but instead derived on the decoding side as
 * specified in Clause C.3.8" of SMPTE ST 2094-50.  A tag in this mode is
 * therefore not merely unrenderable without that clause, it is UNPARSEABLE:
 * the file does not contain the alternate images at all.  This function is
 * that clause.
 *
 * SOURCE, AND ITS PROVENANCE -- the same caveat as icHagcDerivePchipSlopes()
 * above, and for the same document.  C.3.8 was read on 2026-09-01 from the
 * second public committee draft of ST 2094-50 (2026-02-23), the latest text
 * available outside SMPTE, and the licence forbids reproducing it.  What
 * follows is a description of the construction, not a quotation, and every
 * number in it is worth re-checking whenever a newer draft appears.
 *
 * The construction, for a baseline headroom H (log2, as the tag encodes it):
 *
 *   - H = 0 yields NO alternate images.  That is not a failure - proposal
 *     1.2.2.6's no-alternates case already has a defined meaning, and the
 *     evaluator reaches it by the same path as an ordinary tag that carries
 *     none.
 *   - otherwise exactly TWO, at headrooms 0 and log2(8/3) * u, where
 *     u = min(H / log2(1000/203), 1).  The 1000/203 is the ratio of a typical
 *     mastering peak to BT.2408 HDR reference white, so u is "how much of one
 *     mastering stop this baseline actually has", clamped at one.
 *   - each carries the max component mixing (k_max = 1, the rest zero, which
 *     is icHagcMixingMax exactly) and an eight-point gain curve.
 *   - the curve is log2 of the gain of a quadratic Bezier tone curve in
 *     relative linear light, through a knee at [1, y_white,a] to a maximum at
 *     [2^H, 2^H_alt,a], with a highlight compression factor of 0,65 placing
 *     the middle control point.  y_white,0 = 1 - u/2 and y_white,1 = 1.
 *     Control points are sampled at t = c/7 and the slopes come from the
 *     Bezier's own derivative, so the PCHIP path is not involved.
 *
 * The chromaticities C.3.8 also assigns - BT.2020 primaries and D65 - are not
 * returned: they describe the gain application colour space, which this
 * evaluator does not implement, and inventing a home for them here would
 * imply it does.
 *
 * alternates must have room for two entries.  Returns false only when
 * baselineHeadroom is negative or not a number, in which case nAlternates is
 * left untouched; nAlternates is set to 0 or 2 otherwise.
 ***********************************************************************
 */
ICCPROFLIB_API bool icHagcDeriveReferenceWhiteToneMap(icFloatNumber baselineHeadroom,
                                                      icHagcAlternateImage *alternates,
                                                      icUInt8Number &nAlternates);

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
   * Returns false when the evaluator is unsupported or log2Headroom is NaN,
   * in which case the target is not recorded and Apply() stays whatever the
   * last accepted target made it.  An infinite target is accepted and
   * clamps to an endpoint curve like any other out-of-range value.
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

  /** True when the tag set the Reference White Tone Mapping flag and its
   * alternate images were built by icHagcDeriveReferenceWhiteToneMap() rather
   * than read from the file - which, in that mode, is the only way they can
   * be obtained.  Reported for the same reason UsesDerivedSlopes() is: the
   * construction comes from a committee draft. */
  bool UsesDerivedReferenceWhiteToneMap() const { return m_bDerivedRefWhiteToneMap; }

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
  bool m_bDerivedRefWhiteToneMap;

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
