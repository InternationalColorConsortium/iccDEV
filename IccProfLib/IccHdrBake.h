/** @file
    File:       IccHdrBake.h

    Contains:   Header for baking an HDR Profile's tone-mapped rendering into
                a lutAToBType/lutBToAType pair, so that a CMM which implements
                none of clause 8.10 still produces the SDR reproduction the
                profile's author intended

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
// -Initial implementation of the A2B0/B2A0 HDR fallback bake
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCHDRBAKE_H)
#define _ICCHDRBAKE_H

#include "IccDefs.h"
#include "IccHdrProfile.h"
#include "IccHdrToneMap.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccProfile;
class CIccTagLutAtoB;
class CIccTagLutBtoA;
class CIccCurve;

/**
 ***********************************************************************
 * Default table sizes, from the white paper's "Choice of parameters".
 *
 * 1024 A-curve entries reconstruct both the PQ and the HLG transfer well
 * enough that the residual is below the CLUT's own interpolation error, and
 * 33 grid points per axis is the de facto default for a v4 A2B; 45 reduces
 * the error on steeply curved gains noticeably at about 2.5 times the CLUT
 * storage, and 17 is the smallest size worth writing.
 *
 * PROPOSAL-ISSUE HAGC-06 (design consequence; candidate NOTE for the annex) --
 * what the grid size buys is bounded by something it cannot fix.  At the gain
 * curve's last control point the HAGC annex's extrapolation makes the tone
 * map flat, so the surface being sampled has a crease rather than a curve,
 * and no interpolation over any grid reproduces a crease.  Measured on
 * Testing/HDR/HagcCommonParams.icc, the worst-case PCS error near that knee
 * is 0.012 at 33 points and 0.005 at 45 - but 0.009 at 65, because what
 * decides it is where the knee falls between two nodes rather than how many
 * nodes there are.  Expect a bigger grid to help on average and not
 * monotonically at any one colour.
 ***********************************************************************
 */
#define icHdrBakeDefaultGridPoints 33
#define icHdrBakeDefaultCurveSize  1024

/**
 * The largest A curve this bake will build.
 *
 * Not a policy of this file but a property of what it writes into:
 * CIccTagCurve::SetSize() refuses anything above 65536, and it does so by
 * freeing its buffer, setting its size to zero and returning *true*.  A
 * caller that trusts that return then writes nCurveSize floats through the
 * unchecked operator[] onto a NULL buffer.  Rejecting the size up front is
 * what keeps that contract from being load bearing here.
 */
#define icHdrBakeMaxCurveSize      65536

/**
 * The exponent the A curves store their samples under, i.e. the curve holds
 * L^(1/icHdrBakeCurveExponent) and the CLUT raises its input coordinate back
 * to that power.
 *
 * PROPOSAL-ISSUE WP-01 (draft text; plausibly an editing artifact, but it must
 * be resolved either way) -- the white paper's step 1 normalises L "so that the
 * reference-white luminance equals 1.0", and its A-curve section then justifies
 * the fifth root by saying a 16-bit curveType "cannot represent values outside
 * [0,1]".  The two cannot both hold: under reference-white normalisation PQ's
 * peak is 10 000/CRWL, about 49.26, and 49.26^(1/5) = 2.18 is still outside
 * [0,1].  The only self-consistent reading is that the curve holds
 * PEAK-normalised light, with the 10 000/CRWL constant applied inside the CLUT,
 * and that is what is implemented here - see GetPeakReferenceLevel().  The two
 * readings differ by ~49x at the CLUT input and both produce a file that
 * validates and renders something.
 *
 * A fifth root is what the white paper specifies, for two reasons that both
 * still hold here.  It maps the working range of an HDR transfer into [0, 1],
 * which an unsigned 16-bit curveType cannot otherwise hold - PQ's peak is
 * 10 000 / CRWL, about 49 reference whites - and it concentrates the samples
 * in the dark part of the range, where quantisation shows as banding.
 */
#define icHdrBakeCurveExponent 5.0

/**
 * The target headroom the bake is evaluated at, in the linear ratio the
 * CIccCreateHdrXformHint uses: 1.0, i.e. a display whose peak luminance is
 * the HDR reference white.
 *
 * PROPOSAL-ISSUE HDR-03 (design-level; a decision the group has not taken) --
 * 8.10.6 strongly recommends the AToB0/BToA0 fallback pair but never says what
 * it should contain, and NOTE 5 puts H_target outside the profile, so a
 * consumer of a baked tag cannot recover the headroom it was baked at.  Two
 * conforming authors can therefore ship fallbacks differing by stops.  Fixed
 * here at 1.0 for the encoding reason below.
 *
 * This is not a parameter and should not become one.  A baked CLUT's samples
 * are unsigned 16-bit and so cannot carry a value above 1.0, which is exactly
 * the reference white; any target above 1.0 would be clipped back to this one
 * on the way into the tag, and the tag would then misrepresent the tone
 * mapping rather than approximate it.  The same ceiling is what makes a
 * headroom-carrying rendering the HAGC tag's business and not this one - see
 * the 16-bit PCS note in the T0 encoding-range decision.
 */
#define icHdrBakeTargetHeadroom 1.0

/**
 * PCSXYZ is encoded as u1Fixed15Number, so a CLUT or matrix output of 1.0
 * means XYZ = 65535/32768 and an XYZ of 1.0 is written as 32768/65535.  The
 * colorant tags are in XYZ, so the baked matrix is the colorants times this.
 * It is the same constant CIccXformMatrixTRC applies through XYZScale().
 */
#define icHdrBakePcsXyzScale (32768.0 / 65535.0)

/**
 ***********************************************************************
 * What to do with the profile version when fallback tags are attached.
 ***********************************************************************
 */
typedef enum {
  /** Leave the version as authored.  The default, because an HDR Profile is
   * a v4.5 profile by clause 8.10.1 and lowering its version would strip it
   * of the sub-class it conforms to. */
  icHdrBakeVersionKeep = 0,

  /** PROPOSAL-ISSUE WP-04 (sequencing artifact) -- the white paper (2026-05-17)
   * writes header version 4.4, and clause 8.10.1 (2026-07-13) later made
   * 4.5.0.0 a requirement of the class, so following the paper strips a
   * conforming profile of the classification the amendment gives it.  The
   * paper's own parenthetical "(or 4.5 after acceptance...)" shows it tracking a
   * target that had not landed.  The set needs reconciling; until then this is
   * opt-in and the default leaves the version alone.
   *
   * Set the header to 4.4.0.0, as the white paper's "Tag assembly and
   * profile patching" does, for consumers that reject a version they do not
   * know.  The profile then no longer classifies as an HDR Profile under
   * clause 8.10.1 - the fallback tags are all such a consumer would have
   * used anyway, but an HDR-aware one loses the classification too. */
  icHdrBakeVersionV4_4 = 1,
} icHdrBakeVersionPolicy;

/**
 ***********************************************************************
 * Everything the bake takes from its caller.
 ***********************************************************************
 */
typedef struct {
  /** Grid points per CLUT axis; at least 2, at most 255. */
  icUInt8Number nGridPoints;

  /** Entries per A curve; at least 2, at most icHdrBakeMaxCurveSize. */
  icUInt32Number nCurveSize;

  /** HLG OOTF parameters, ignored for the other transfer characteristics.
   * These are properties of the intended display, not of the profile, which
   * is why they are the caller's to supply - the same reasoning as for
   * CIccCreateHdrXformHint. */
  icFloatNumber hlgGamma;
  icFloatNumber hlgPeakLuminance;

  icHdrBakeVersionPolicy nVersionPolicy;
} icHdrBakeParams;

/** Fill params with the defaults above.  Always use this rather than a
 * brace initialiser, so that a field added later is initialised in every
 * caller instead of silently becoming zero. */
ICCPROFLIB_API void icHdrBakeParamsInit(icHdrBakeParams &params);

/**
 ***********************************************************************
 * Class: CIccHdrBaker
 *
 * Purpose:
 *  Build the static SDR rendering of an HDR Profile - the procedure of the
 *  "Representation of HDR-to-SDR Tone Mapping from a Headroom Adaptive Gain
 *  Curve in a v4 A2B0 Tag" white paper, and of the HAGC amendment's
 *  informative annex 2.
 *
 *  The pipeline that gets sampled is exactly the one CIccXformMatrixTrcHdr
 *  evaluates at a target headroom of 1.0: the EOTF the cicpTag names, the
 *  gain curve at that target, then the profile's own matrix columns.  Baking
 *  it is therefore a change of representation and not a second rendering
 *  algorithm - ToPcs() below *is* that pipeline, and the tag is a resampling
 *  of it, which is what lets the two be compared numerically in a test.
 *
 *  Where the stages fall is forced by what a lutAToBType can hold:
 *
 *    A curves  the per-channel part of the linearisation, stored as its fifth
 *              root because a curveType's samples are unsigned 16-bit and the
 *              linearised signal reaches ~49 (PQ) or ~4.9 (HLG) reference
 *              whites.  This stage carries the transfer's dynamic range so
 *              that the CLUT does not have to: a 33-point grid cannot
 *              represent a PQ EOTF, but it represents a gain surface well.
 *    CLUT      the rest of the linearisation - the HLG OOTF, which is a
 *              function of all three channels and so has no per-channel form
 *              at all - then the gain curve, then a clamp to [0, 1].
 *    M curves  identity.
 *    Matrix    the profile's matrix column tags, scaled to the PCSXYZ
 *              encoding.
 *    B curves  identity.
 *
 *  PROPOSAL-ISSUE WP-02 (design-level oversight, not an artifact - the paper's
 *  table and its prose agree with each other and both are outside what ICC.1
 *  10.12 permits).  The white paper describes the tag as A curves, CLUT and
 *  matrix with the M and B curves absent.  That combination is not one of the four ICC.1 10.12
 *  permits, so the identity M and B curves above are written instead; they
 *  cost 24 bytes together and make the tag legal for the readers the whole
 *  procedure exists to serve.
 *
 *  The BToA direction is the same five stages read backwards, with the same
 *  compression at both ends: identity B curves, the inverse matrix,
 *  fifth-root M curves, the inverse-gain CLUT, and A curves that are the
 *  inverse of the AToB's.  Leaving those A curves as identities and having
 *  the CLUT produce the device encoding outright is the obvious shortcut and
 *  is wrong for the reason above, in the direction where it does visible
 *  damage - see BtoAClutOp().
 *
 *  PROPOSAL-ISSUE WP-05 (gap across the set) -- the white paper covers only the
 *  AToB0, while ICC.1 8.3.2 and clause 8.10.6 both want the pair, so an author
 *  following it produces half of what the amendment asks for.  The only
 *  guidance for the other half is HAGC annex 2.2, two sentences that do not
 *  mention where the compression belongs; the shortcut it reads as produces a
 *  dark PQ pixel at roughly half its input code.
 *
 *  It is annex 2.2's first option, the inverted gain curve sampled on its own
 *  uniform grid, rather than its second, a numerical inversion of the baked
 *  AToB.  Inverting the analytic form once is both more accurate and simpler
 *  than inverting an approximation, and CIccHagcEvaluator::Invert() already
 *  exists for the CMM path.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccHdrBaker
{
public:
  CIccHdrBaker();
  virtual ~CIccHdrBaker();

  /**
   * Resolve everything the bake depends on: the profile's HDR
   * classification, its transfer characteristic, its gain curve at the fixed
   * target headroom, and its matrix columns.
   *
   * pParams may be NULL, which means the defaults.
   *
   * Returns IsSupported().  A profile that carries no HAGC tag is still
   * supported: the bake is then the EOTF and a clamp to the SDR volume, which
   * is a meaningful legacy rendering and the one clause 8.10.3's lowest
   * ranked descriptor implies.
   *
   * LIFETIME.  The baker does not own pProfile and does not copy it.  It
   * keeps the pointer, and it keeps raw CIccCurve pointers into the profile's
   * own TRC tags, so pProfile and every tag reachable from it must outlive
   * every later call on this object.  Init(p); delete p; CreateAtoB(); is a
   * use after free, and so is deleting the profile's rTRC between the two.
   * Re-Init() on a different profile is fine and drops the old pointers.
   */
  bool Init(const CIccProfile *pProfile, const icHdrBakeParams *pParams = NULL);

  bool IsSupported() const { return m_bSupported; }

  /** Static text naming what made IsSupported() false, or NULL when it is
   * true.  Never formatted or allocated - callers put it straight into
   * diagnostics. */
  const icChar *GetUnsupportedReason() const { return m_szUnsupported; }

  /** True when a BToA can be built, i.e. when the gain curve at this target
   * has an inverse.  CreateBtoA() returns NULL when it does not; the AToB is
   * unaffected. */
  bool IsInvertible() const;

  /** True when the tone-mapping step is a no-op at this target and the bake
   * is the transfer, the clamp and the matrix alone. */
  bool IsToneMapIdentity() const { return m_bSupported && !m_bToneMap; }

  /** True when any contributing gain curve had its slopes reconstructed by
   * icHagcDerivePchipSlopes() rather than read from the tag - see that
   * function for why a caller might want to say so. */
  bool UsesDerivedSlopes() const;

  /**
   * The reference pipeline the tags approximate: device-encoded RGB in
   * [0, 1] to PCSXYZ in the profile's own encoding, tone mapped to a target
   * headroom of 1.0.
   *
   * Public because it is the ground truth a test compares the interpolated
   * tag against, and because a caller that only wants the rendering has no
   * reason to build a tag at all.
   */
  void ToPcs(icFloatNumber *dstXyz, const icFloatNumber *srcRgb) const;

  /** Inverse of ToPcs().  Returns false when IsInvertible() is false, leaving
   * dstRgb untouched. */
  bool FromPcs(icFloatNumber *dstRgb, const icFloatNumber *srcXyz) const;

  /**
   * The AToB CLUT's own stage: peak-normalised linear RGB - an A curve output
   * raised back to icHdrBakeCurveExponent - to SDR linear RGB in [0, 1].
   *
   * Public because it is one half of the seam the whole design turns on, and
   * a test that wants to know whether the tag's CLUT holds what it should has
   * to be able to ask for the exact value a node was filled from.  dst and
   * src may alias.
   */
  void AtoBClutOp(icFloatNumber *dst, const icFloatNumber *src) const;

  /** The BToA CLUT's own stage: SDR linear RGB in [0, 1] to peak-normalised
   * linear RGB under the fifth root, which is what its A curves expand.
   * Returns false, leaving dst untouched, when the gain curve has no
   * inverse. */
  bool BtoAClutOp(icFloatNumber *dst, const icFloatNumber *src) const;

  /** Build the AToB tag.  Caller owns the result; NULL means the bake is
   * unsupported or an allocation this file makes was refused.
   *
   * The qualifier is not pedantry.  Every allocation made here is
   * new(std::nothrow) and unwinds, but the stage arrays come from
   * CIccMBB::NewCurvesA/M/B and NewMatrix, which use throwing new, so an OOM
   * inside those terminates rather than arriving as a NULL return.  The NULL
   * checks on their results are kept because they are what a nothrow
   * conversion upstream would need, and because a future NewCurvesA that
   * returns NULL for a reason other than OOM would otherwise be a UAF here. */
  CIccTagLutAtoB *CreateAtoB() const;

  /** Build the BToA tag.  Caller owns the result; NULL also means the gain
   * curve at this target has no inverse.  Same allocation caveat as
   * CreateAtoB(). */
  CIccTagLutBtoA *CreateBtoA() const;

protected:
  /** The per-channel linearisation an A curve stores, before the fifth root:
   * ToLinearChannel() for PQ and HLG, the profile's own TRC tag for Linear.
   * Normalised so that an encoded 1.0 gives 1.0. */
  icFloatNumber LinearizeChannel(icUInt8Number nChannel, icFloatNumber v) const;

  /** Inverse of LinearizeChannel(): what a BToA A curve holds. */
  icFloatNumber EncodeChannel(icUInt8Number nChannel, icFloatNumber v) const;

  /** Complete a peak-normalised triplet into reference white relative linear
   * light - ChannelToReference() plus the Linear case's own peak scale. */
  void ToReference(icFloatNumber *dst, const icFloatNumber *src) const;

  /** Inverse of ToReference(). */
  void FromReference(icFloatNumber *dst, const icFloatNumber *src) const;

  /** The gain curve and the target-volume clamp, in place. */
  void ToneMap(icFloatNumber *pixel) const;

  bool m_bSupported;
  const icChar *m_szUnsupported;

  const CIccProfile *m_pProfile;
  icHdrBakeParams m_params;

  CIccHdrTransfer m_transfer;
  CIccHagcEvaluator m_evaluator;

  bool m_bToneMap;
  bool m_bClampToTarget;

  /** The three TRC tags, borrowed from the profile and used only when the
   * transfer characteristic is Linear.  Not owned. */
  CIccCurve *m_pTrc[3];

  /** The reference white relative value an encoded 1.0 produces.  For Linear
   * it comes from the TRC tags; for PQ and HLG from the transfer. */
  icFloatNumber m_peakLevel;

  /** Matrix columns, in XYZ and not yet scaled to the PCS encoding, and their
   * inverse. */
  icFloatNumber m_matrix[9];
  icFloatNumber m_inverse[9];
  bool m_bInverseValid;
};

/**
 * Build the HDR fallback AToB tag for a profile.  Convenience over
 * CIccHdrBaker for the common case; pReason, when supplied, receives the
 * static text of GetUnsupportedReason() on failure.
 */
ICCPROFLIB_API CIccTagLutAtoB *icCreateHdrFallbackAtoB(const CIccProfile *pProfile,
                                                       const icHdrBakeParams *pParams = NULL,
                                                       const icChar **pReason = NULL);

/** Build the HDR fallback BToA tag for a profile.  See icCreateHdrFallbackAtoB(). */
ICCPROFLIB_API CIccTagLutBtoA *icCreateHdrFallbackBtoA(const CIccProfile *pProfile,
                                                       const icHdrBakeParams *pParams = NULL,
                                                       const icChar **pReason = NULL);

/**
 * Attach both fallback tags to a profile, replacing any AToB0Tag and
 * BToA0Tag it already carries, and apply the version policy.
 *
 * The two are attached together or not at all: ICC.1 8.3.2 pairs them, and a
 * profile with an AToB0Tag and no BToA0Tag is one whose rendering cannot be
 * undone.  Every other tag, the headroomAdaptiveGainCurveTag included, is
 * left exactly as it was, so an HDR-aware CMM keeps evaluating the curve
 * dynamically and only a CMM that cannot reaches for these.
 */
ICCPROFLIB_API bool icAddHdrFallbackTags(CIccProfile *pProfile,
                                         const icHdrBakeParams *pParams = NULL,
                                         const icChar **pReason = NULL);

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCHDRBAKE_H)
