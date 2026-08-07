/** @file
    File:       IccHdrBake.cpp

    Contains:   Implementation of the A2B0/B2A0 HDR fallback bake

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

#include <cmath>
#include <cstring>

#include "IccHdrBake.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagHagc.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 ****************************************************************************
 * Name: icHdrBakeFindTag
 *
 * Purpose: Fetch a tag by signature, loading it if the profile has not.
 *
 *  CIccProfile::FindTagConst() answers only from tags already in memory, so a
 *  profile that was opened rather than read reports every tag missing.  The
 *  same hazard that made icGetHdrProfileInfo() see no cicpTag on the CMM's
 *  own path applies to every lookup here; FindTag() loads on demand, and the
 *  cast is sound because loading populates a cache without changing the
 *  profile's value.
 *****************************************************************************
 */
static CIccTag *icHdrBakeFindTag(const CIccProfile *pProfile, icSignature sig)
{
  return ((CIccProfile*)pProfile)->FindTag(sig);
}

/** Clamp to the unit interval, NaN included - a NaN anywhere in a CLUT would
 * propagate through every interpolation that touches its cell. */
static icFloatNumber icHdrBakeClampUnit(icFloatNumber v)
{
  if (std::isnan(v))
    return (icFloatNumber)0.0;

  if (v < 0.0)
    return (icFloatNumber)0.0;

  if (v > 1.0)
    return (icFloatNumber)1.0;

  return v;
}

/** One matrix column tag as an XYZ triplet. */
static bool icHdrBakeGetColumn(const CIccProfile *pProfile, icTagSignature sig,
                               icFloatNumber *pXYZ)
{
  CIccTag *pTag = icHdrBakeFindTag(pProfile, sig);

  if (!pTag || pTag->GetType() != icSigXYZType)
    return false;

  CIccTagXYZ *pXyzTag = (CIccTagXYZ*)pTag;

  if (!pXyzTag->GetSize())
    return false;

  pXYZ[0] = icFtoD((*pXyzTag)[0].X);
  pXYZ[1] = icFtoD((*pXyzTag)[0].Y);
  pXYZ[2] = icFtoD((*pXyzTag)[0].Z);

  return true;
}

/** One TRC tag as a curve, by the same test CIccXform uses. */
static CIccCurve *icHdrBakeGetCurve(const CIccProfile *pProfile, icTagSignature sig)
{
  CIccTag *pTag = icHdrBakeFindTag(pProfile, sig);

  if (pTag && (pTag->GetType() == icSigCurveType ||
               pTag->GetType() == icSigParametricCurveType)) {
    return (CIccCurve*)pTag;
  }

  return NULL;
}

/**
 ****************************************************************************
 * Name: icHdrBakeParamsInit
 *
 * Purpose: Fill a parameter block with the defaults.
 *****************************************************************************
 */
void icHdrBakeParamsInit(icHdrBakeParams &params)
{
  memset(&params, 0, sizeof(params));

  params.nGridPoints = icHdrBakeDefaultGridPoints;
  params.nCurveSize = icHdrBakeDefaultCurveSize;
  params.hlgGamma = (icFloatNumber)icHlgDefaultGamma;
  params.hlgPeakLuminance = (icFloatNumber)icHlgDefaultPeakLuminance;
  params.nVersionPolicy = icHdrBakeVersionKeep;
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::CIccHdrBaker
 *****************************************************************************
 */
CIccHdrBaker::CIccHdrBaker()
{
  m_bSupported = false;
  m_szUnsupported = "Not initialized";
  m_pProfile = NULL;
  m_bToneMap = false;
  m_bClampToTarget = false;
  m_pTrc[0] = m_pTrc[1] = m_pTrc[2] = NULL;
  m_peakLevel = (icFloatNumber)1.0;
  m_bInverseValid = false;

  icHdrBakeParamsInit(m_params);

  memset(m_matrix, 0, sizeof(m_matrix));
  memset(m_inverse, 0, sizeof(m_inverse));
}

CIccHdrBaker::~CIccHdrBaker()
{
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::Init
 *
 * Purpose:
 *  Resolve the whole pipeline once, so that sampling it is a pure function.
 *
 *  The profile requirements are deliberately the structural ones and not the
 *  full clause 8.10.1 conformance: matrix-based RGB with a cicpTag naming an
 *  HDR transfer characteristic.  The profile version is not among them
 *  because the bake exists precisely to serve consumers that will not accept
 *  the version an HDR Profile carries, and icHdrBakeVersionV4_4 may be about
 *  to change it anyway.
 *
 * Args:
 *  pProfile = the HDR profile to bake
 *  pParams  = table sizes and HLG parameters, or NULL for the defaults
 *
 * Return:
 *  IsSupported(); GetUnsupportedReason() says what failed.
 *****************************************************************************
 */
bool CIccHdrBaker::Init(const CIccProfile *pProfile, const icHdrBakeParams *pParams)
{
  m_bSupported = false;
  m_pProfile = NULL;
  m_bToneMap = false;
  m_bClampToTarget = false;
  m_pTrc[0] = m_pTrc[1] = m_pTrc[2] = NULL;
  m_bInverseValid = false;

  if (pParams)
    m_params = *pParams;
  else
    icHdrBakeParamsInit(m_params);

  if (m_params.nGridPoints < 2) {
    m_szUnsupported = "A CLUT needs at least two grid points per axis";
    return false;
  }

  if (m_params.nCurveSize < 2) {
    // One entry is not an undersized table but a different tag: a curveType
    // of size 1 encodes a gamma, not a sampled curve.
    m_szUnsupported = "An A curve needs at least two entries";
    return false;
  }

  if (!pProfile) {
    m_szUnsupported = "No profile";
    return false;
  }

  icHdrProfileInfo info;

  if (!icGetHdrProfileInfo(pProfile, info)) {
    m_szUnsupported = "Profile could not be classified";
    return false;
  }

  if (!info.bRgbMatrixBased) {
    m_szUnsupported = "Not a three-component matrix-based RGB profile";
    return false;
  }

  if (!info.bHasCicp) {
    m_szUnsupported = "No cicpTag, so no transfer characteristic to invert";
    return false;
  }

  if (!info.bTransferIsHdr) {
    m_szUnsupported = "cicpTag TransferCharacteristics is not one clause 8.10.1 permits";
    return false;
  }

  if (!m_transfer.Init(info.nTransferCharacteristics, info.contentReferenceWhite,
                       m_params.hlgGamma, m_params.hlgPeakLuminance)) {
    m_szUnsupported = "Transfer characteristic has no analytic form here";
    return false;
  }

  /* PROPOSAL-ISSUE WP-08 (probable editing artifact; two words to the scope
   * list) -- the white paper's "Scope and constraints" admits only PQ and HLG,
   * but its abstract ("PQ, HLG or linear RGB profile"), its step 1 ("for
   * Linear, the identity"), its A-curve section ("or a linearly scaled identity
   * for the Linear case") and its "chosen for PQ and HLG" qualifier all assume
   * a third case.  Clause 8.10.1 permits TransferCharacteristics = 8, so it is
   * a real one.  We support it -- and because Linear, unlike PQ and HLG, has no
   * intrinsic peak for "linearly scaled" to refer to, the scale is taken from
   * the profile itself below. */
  if (m_transfer.UsesProfileCurves()) {
    // TransferCharacteristics = 8 (Linear): the TRC tags are the
    // linearisation, so their own output range is the headroom.  A sampled
    // curveType clamps at 1.0 and so carries none - that is a property of the
    // profile and not an error here, it just leaves the tone mapping nothing
    // to do.
    m_pTrc[0] = icHdrBakeGetCurve(pProfile, icSigRedTRCTag);
    m_pTrc[1] = icHdrBakeGetCurve(pProfile, icSigGreenTRCTag);
    m_pTrc[2] = icHdrBakeGetCurve(pProfile, icSigBlueTRCTag);

    if (!m_pTrc[0] || !m_pTrc[1] || !m_pTrc[2]) {
      m_szUnsupported = "Linear transfer characteristic with no TRC tags to linearise with";
      return false;
    }

    icUInt8Number i;

    m_peakLevel = (icFloatNumber)0.0;

    for (i = 0; i < 3; i++) {
      m_pTrc[i]->Begin();

      icFloatNumber peak = m_pTrc[i]->Apply((icFloatNumber)1.0);

      if (peak > m_peakLevel)
        m_peakLevel = peak;
    }

    if (!(m_peakLevel > 0.0)) {
      // Every channel maps full scale to zero, so there is no scale that
      // brings the A curves into range and nothing to represent.
      m_szUnsupported = "TRC tags map full scale to zero";
      return false;
    }
  }
  else {
    m_peakLevel = m_transfer.GetPeakReferenceLevel();
  }

  // The gain curve, at the one target headroom a 16-bit tag can hold.
  CIccTag *pTag = icHdrBakeFindTag(pProfile, icSigHeadroomAdaptiveGainCurveTag);

  if (pTag && pTag->GetType() == icSigHeadroomAdaptiveGainCurveType) {
    CIccTagHagc *pHagc = (CIccTagHagc*)pTag;

    if (m_evaluator.Init(pHagc->GetMetadata())) {
      m_evaluator.SetTargetHeadroom((icFloatNumber)(log(icHdrBakeTargetHeadroom) / log(2.0)));

      m_bClampToTarget = m_evaluator.ClampsToTargetVolume();
      m_bToneMap = !m_evaluator.IsIdentity();
    }
    // An evaluator that declines the tag leaves the bake as the transfer, the
    // SDR clamp and the matrix - the same fallback clause 8.10.3 gives a CMM
    // that cannot run the descriptor, which is what this tag's readers are.
  }

  if (!icHdrBakeGetColumn(pProfile, icSigRedMatrixColumnTag,   m_matrix + 0) ||
      !icHdrBakeGetColumn(pProfile, icSigGreenMatrixColumnTag, m_matrix + 3) ||
      !icHdrBakeGetColumn(pProfile, icSigBlueMatrixColumnTag,  m_matrix + 6)) {
    m_szUnsupported = "Missing or malformed matrix column tag";
    return false;
  }

  // icHdrBakeGetColumn() wrote each column's XYZ contiguously, which is the
  // transpose of what the matrix needs: column j holds the XYZ of primary j,
  // and row i of the matrix holds component i of all three primaries.
  icFloatNumber columns[9];
  memcpy(columns, m_matrix, sizeof(columns));

  icUInt8Number row, col;

  for (row = 0; row < 3; row++) {
    for (col = 0; col < 3; col++)
      m_matrix[row * 3 + col] = columns[col * 3 + row];
  }

  memcpy(m_inverse, m_matrix, sizeof(m_inverse));
  m_bInverseValid = icMatrixInvert3x3(m_inverse);

  m_bSupported = true;
  m_szUnsupported = NULL;

  m_pProfile = pProfile;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::IsInvertible
 *
 * Purpose:
 *  Whether a BToA can be built.  Three things have to be invertible: the
 *  matrix, the gain curve when there is one, and the transfer.  The transfer
 *  always is - FromLinear() is closed form for PQ and HLG, and the Linear
 *  case goes through CIccCurve::Find(), whose bisection needs the TRC to be
 *  monotone, which a TRC that is not is a defect of the profile rather than
 *  of the bake.
 *****************************************************************************
 */
bool CIccHdrBaker::IsInvertible() const
{
  if (!m_bSupported || !m_bInverseValid)
    return false;

  return !m_bToneMap || m_evaluator.IsInvertible();
}

bool CIccHdrBaker::UsesDerivedSlopes() const
{
  return m_bSupported && m_evaluator.UsesDerivedSlopes();
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::LinearizeChannel
 *
 * Purpose:
 *  The per-channel half of the linearisation, normalised so that an encoded
 *  1.0 gives 1.0 - i.e. what an A curve holds before the fifth root.
 *
 * Args:
 *  nChannel = 0, 1 or 2; only the Linear case distinguishes them
 *  v = the device-encoded channel value
 *****************************************************************************
 */
icFloatNumber CIccHdrBaker::LinearizeChannel(icUInt8Number nChannel, icFloatNumber v) const
{
  if (m_transfer.UsesProfileCurves()) {
    // Divided by the common peak rather than by the channel's own, because a
    // per-channel scale here would be a per-channel gain the matrix does not
    // know about and would move the profile's white point.
    return m_pTrc[nChannel]->Apply(v) / m_peakLevel;
  }

  return m_transfer.ToLinearChannel(v);
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::ToReference
 *
 * Purpose:
 *  Complete a peak-normalised triplet into reference white relative linear
 *  light, undoing exactly what LinearizeChannel() normalised by.
 *****************************************************************************
 */
void CIccHdrBaker::ToReference(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (m_transfer.UsesProfileCurves()) {
    dst[0] = src[0] * m_peakLevel;
    dst[1] = src[1] * m_peakLevel;
    dst[2] = src[2] * m_peakLevel;
    return;
  }

  m_transfer.ChannelToReference(dst, src);
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::FromReference
 *
 * Purpose:
 *  Exact inverse of ToReference().
 *****************************************************************************
 */
void CIccHdrBaker::FromReference(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (m_transfer.UsesProfileCurves()) {
    dst[0] = src[0] / m_peakLevel;
    dst[1] = src[1] / m_peakLevel;
    dst[2] = src[2] / m_peakLevel;
    return;
  }

  m_transfer.ReferenceToChannel(dst, src);
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::ToneMap
 *
 * Purpose:
 *  Step b) of clause 8.10.2 at the fixed target, in place.  The two branches
 *  are the same ones CIccXformMatrixTrcHdr::Apply() takes, so that the bake
 *  and the live path agree by construction.
 *****************************************************************************
 */
void CIccHdrBaker::ToneMap(icFloatNumber *pixel) const
{
  if (m_bToneMap) {
    m_evaluator.Apply(pixel, pixel);
    return;
  }

  if (m_bClampToTarget) {
    // HAGC proposal 1.2.2.6: no alternate images means no tone mapping and a
    // clamp of the baseline to the target colour volume.
    icUInt8Number i;

    for (i = 0; i < 3; i++) {
      if (pixel[i] > icHdrBakeTargetHeadroom)
        pixel[i] = (icFloatNumber)icHdrBakeTargetHeadroom;
    }
  }
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::AtoBClutOp
 *
 * Purpose:
 *  What the AToB CLUT holds: the second half of the linearisation, the tone
 *  mapping, and the clamp into the SDR colour volume.
 *
 *  The clamp is the tag's encoding speaking, not a rendering decision.  A
 *  CLUT sample is unsigned 16-bit, so 1.0 - the reference white - is the
 *  largest value it can carry.  A gain curve evaluated at a target headroom
 *  of 1.0 should already be landing at or below it, which is why this reads
 *  as a guard; where it does bite, the alternative is a wrapped or
 *  meaningless sample.
 *
 * Args:
 *  dst = SDR linear RGB in [0, 1], may alias src
 *  src = peak-normalised linear RGB, i.e. a grid coordinate raised back to
 *        icHdrBakeCurveExponent
 *****************************************************************************
 */
void CIccHdrBaker::AtoBClutOp(icFloatNumber *dst, const icFloatNumber *src) const
{
  icFloatNumber pixel[3];

  ToReference(pixel, src);
  ToneMap(pixel);

  dst[0] = icHdrBakeClampUnit(pixel[0]);
  dst[1] = icHdrBakeClampUnit(pixel[1]);
  dst[2] = icHdrBakeClampUnit(pixel[2]);
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::BtoAClutOp
 *
 * Purpose:
 *  What the BToA CLUT holds: the gain curve inverted, then the triplet half
 *  of the inverse transfer, then the same fifth-root compression the AToB's A
 *  curves store their samples under.
 *
 *  The mirror of the AToB direction, and for the same reason.  It would be
 *  simpler for this stage to produce the device encoding outright and leave
 *  the A curves as identities, but a uniform grid cannot represent the
 *  encoding: PQ is so steep near black that the first few nodes of a 33-point
 *  grid span the whole shadow range, and everything between them comes back
 *  as a straight line through it.  Measured on a PQ fixture, a dark pixel came
 *  back from the round trip at nearly half the code it went in as.  Leaving
 *  the steep part to a 1024-entry A curve - which is exactly the inverse of
 *  the AToB's A curve - moves it onto a table that can hold it, and leaves
 *  this stage the smooth job of undoing a gain.
 *
 * Args:
 *  dst = peak-normalised linear RGB under the fifth root, i.e. what the A
 *        curves expect, in [0, 1]; may alias src
 *  src = SDR linear RGB in [0, 1]
 *
 * Return:
 *  false when the gain curve has no inverse, leaving dst untouched.
 *****************************************************************************
 */
bool CIccHdrBaker::BtoAClutOp(icFloatNumber *dst, const icFloatNumber *src) const
{
  if (!IsInvertible())
    return false;

  icFloatNumber pixel[3];

  pixel[0] = src[0];
  pixel[1] = src[1];
  pixel[2] = src[2];

  if (m_bToneMap) {
    // A false return here is a value the gain curve destroyed - a zero gain -
    // and not a configuration failure, which IsInvertible() already excluded.
    // Leaving the linear values alone is the closest defined answer, and is
    // what the CMM's output path does with the same case.
    m_evaluator.Invert(pixel, pixel);
  }

  FromReference(pixel, pixel);

  icUInt8Number i;

  for (i = 0; i < 3; i++)
    dst[i] = (icFloatNumber)pow((double)icHdrBakeClampUnit(pixel[i]), 1.0 / icHdrBakeCurveExponent);

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::EncodeChannel
 *
 * Purpose:
 *  Inverse of LinearizeChannel(): one peak-normalised linear channel back to
 *  its device encoding.  This is what a BToA A curve holds.
 *
 * Args:
 *  nChannel = 0, 1 or 2; only the Linear case distinguishes them
 *  v = the peak-normalised linear channel value
 *****************************************************************************
 */
icFloatNumber CIccHdrBaker::EncodeChannel(icUInt8Number nChannel, icFloatNumber v) const
{
  if (m_transfer.UsesProfileCurves()) {
    // Find() bisects the curve rather than assuming a sampled inverse, so it
    // works for a parametricCurveType as well as for a curveType.  Its domain
    // is the curve's own output, which for the Linear case is reference white
    // relative light - hence the peak scale that LinearizeChannel() divided
    // by has to go back on first.
    return icHdrBakeClampUnit(m_pTrc[nChannel]->Find(v * m_peakLevel));
  }

  return m_transfer.FromLinearChannel(v);
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::ToPcs
 *
 * Purpose:
 *  The whole reference pipeline: the three steps of clause 8.10.2 at a target
 *  headroom of 1.0, ending in the profile's PCSXYZ encoding.
 *
 *  This is what the AToB tag approximates, so the difference between the two
 *  is the tag's representation error and nothing else.
 *
 * Args:
 *  dstXyz = PCSXYZ, may alias srcRgb
 *  srcRgb = device-encoded RGB in [0, 1]
 *****************************************************************************
 */
void CIccHdrBaker::ToPcs(icFloatNumber *dstXyz, const icFloatNumber *srcRgb) const
{
  icFloatNumber lin[3];

  lin[0] = LinearizeChannel(0, srcRgb[0]);
  lin[1] = LinearizeChannel(1, srcRgb[1]);
  lin[2] = LinearizeChannel(2, srcRgb[2]);

  AtoBClutOp(lin, lin);

  icUInt8Number row;

  icFloatNumber xyz[3];

  for (row = 0; row < 3; row++) {
    xyz[row] = (icFloatNumber)((m_matrix[row * 3 + 0] * lin[0] +
                                m_matrix[row * 3 + 1] * lin[1] +
                                m_matrix[row * 3 + 2] * lin[2]) * icHdrBakePcsXyzScale);
  }

  dstXyz[0] = xyz[0];
  dstXyz[1] = xyz[1];
  dstXyz[2] = xyz[2];
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::FromPcs
 *
 * Purpose:
 *  Inverse of ToPcs(), and the reference the BToA tag approximates.
 *
 * Args:
 *  dstRgb = device-encoded RGB, may alias srcXyz
 *  srcXyz = PCSXYZ
 *****************************************************************************
 */
bool CIccHdrBaker::FromPcs(icFloatNumber *dstRgb, const icFloatNumber *srcXyz) const
{
  if (!IsInvertible())
    return false;

  icFloatNumber xyz[3];

  xyz[0] = (icFloatNumber)(srcXyz[0] / icHdrBakePcsXyzScale);
  xyz[1] = (icFloatNumber)(srcXyz[1] / icHdrBakePcsXyzScale);
  xyz[2] = (icFloatNumber)(srcXyz[2] / icHdrBakePcsXyzScale);

  icFloatNumber lin[3];
  icUInt8Number row;

  for (row = 0; row < 3; row++) {
    // Clamped into the SDR colour volume for the same reason the forward CLUT
    // clamps into it: that is the domain the inverse gain curve is defined
    // over, and an out-of-gamut PCS colour has no device value outside it.
    lin[row] = icHdrBakeClampUnit((icFloatNumber)(m_inverse[row * 3 + 0] * xyz[0] +
                                                  m_inverse[row * 3 + 1] * xyz[1] +
                                                  m_inverse[row * 3 + 2] * xyz[2]));
  }

  if (!BtoAClutOp(lin, lin))
    return false;

  // The CLUT stage stops at the fifth-root domain the A curves expect, so the
  // reference pipeline has to finish the job the same way they do.
  for (row = 0; row < 3; row++)
    dstRgb[row] = EncodeChannel(row, (icFloatNumber)pow((double)lin[row], icHdrBakeCurveExponent));

  return true;
}

/**
 ***********************************************************************
 * A curve of nSize samples of f, or the identity when nSize is zero.
 *
 * A curveType with no samples is ICC.1 10.6's own encoding of the identity,
 * so the M and B curves this bake does not use cost four bytes each rather
 * than a table.
 ***********************************************************************
 */
static CIccTagCurve *icHdrBakeNewCurve(icUInt32Number nSize)
{
  CIccTagCurve *pCurve = new CIccTagCurve(0);

  if (pCurve && nSize && !pCurve->SetSize(nSize, icInitZero)) {
    delete pCurve;
    return NULL;
  }

  return pCurve;
}

/**
 ***********************************************************************
 * Populate an AToB CLUT.  Held apart from the baker so that the walk over
 * the grid stays CIccCLUT's own - Iterate() hands each node its normalised
 * coordinates and a pointer to its samples, which is the whole of what this
 * needs.
 ***********************************************************************
 */
class CIccHdrBakeAtoBExec : public IIccCLUTExec
{
public:
  CIccHdrBakeAtoBExec(const CIccHdrBaker *pBaker) : m_pBaker(pBaker) {}

  virtual void PixelOp(icFloatNumber *pGridAdr, icFloatNumber *pData)
  {
    icFloatNumber lin[3];
    icUInt8Number i;

    /* PROPOSAL-ISSUE WP-07 (wording; delete one clause) -- the white paper's
     * A-curve section says "Any CMM consuming the tag must therefore apply the
     * inverse, x -> x^5, between the A-curve output and the CLUT lookup;
     * equivalently, the CLUT must be sampled in the same fifth-root domain".
     * Those two are not equivalent and only the second is available: a v4
     * lutAToBType runs A-curves -> CLUT with no stage in between, and the
     * paper's own audience is legacy CMMs that do nothing but follow the tag.
     * So the undo happens HERE, at sampling time, and the tag we emit needs
     * nothing of the CMM.  Sampling the CLUT on linear coordinates instead --
     * the reading the first clause invites -- leaves every conformant CMM
     * indexing a linear grid with fifth-root coordinates, worst in the shadows,
     * with no symptom but a wrong image. */
    for (i = 0; i < 3; i++) {
      // Undo the A curves' fifth root, so that the coordinate is once again
      // the peak-normalised linear value the curve was sampled from.
      lin[i] = (icFloatNumber)pow((double)pGridAdr[i], icHdrBakeCurveExponent);
    }

    m_pBaker->AtoBClutOp(pData, lin);
  }

protected:
  const CIccHdrBaker *m_pBaker;
};

/** Populate a BToA CLUT.  Its input is the M curves' fifth-root compressed
 * SDR linear light, so the same power undoes it. */
class CIccHdrBakeBtoAExec : public IIccCLUTExec
{
public:
  CIccHdrBakeBtoAExec(const CIccHdrBaker *pBaker) : m_pBaker(pBaker) {}

  virtual void PixelOp(icFloatNumber *pGridAdr, icFloatNumber *pData)
  {
    icFloatNumber lin[3];
    icUInt8Number i;

    for (i = 0; i < 3; i++)
      lin[i] = (icFloatNumber)pow((double)pGridAdr[i], icHdrBakeCurveExponent);

    if (!m_pBaker->BtoAClutOp(pData, lin)) {
      // CreateBtoA() refuses a non-invertible bake before it gets here, so
      // this is unreachable; zeroing rather than leaving the node untouched
      // keeps an unwritten cell out of the tag if it ever becomes reachable.
      pData[0] = pData[1] = pData[2] = (icFloatNumber)0.0;
    }
  }

protected:
  const CIccHdrBaker *m_pBaker;
};

/**
 ***********************************************************************
 * Build and fill a CLUT for a tag that is about to own it.
 *
 * Built standalone rather than through CIccMBB::NewCLUT(), which calls
 * CIccCLUT::Init() but discards its result and returns the CLUT either way -
 * a refused allocation then arrives as a CLUT with a committed point count
 * and no data (#1781).  SetCLUT() takes ownership on success, so the caller
 * must not free what it hands over.
 ***********************************************************************
 */
static CIccCLUT *icHdrBakeNewClut(icUInt8Number nGridPoints, IIccCLUTExec *pExec)
{
  CIccCLUT *pCLUT = new CIccCLUT(3, 3);

  if (!pCLUT->Init(nGridPoints)) {
    delete pCLUT;
    return NULL;
  }

  pCLUT->Iterate(pExec);

  return pCLUT;
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::CreateAtoB
 *
 * Purpose:
 *  Assemble the AToB tag: fifth-root A curves, the tone-mapping CLUT,
 *  identity M curves, the profile's matrix scaled to the PCS encoding, and
 *  identity B curves.
 *
 *  The identity M and B curves are what makes the stage list one of the four
 *  ICC.1 10.12 permits; the white paper's A, CLUT, Matrix is not among them.
 *
 * Return:
 *  The tag, owned by the caller, or NULL.
 *****************************************************************************
 */
CIccTagLutAtoB *CIccHdrBaker::CreateAtoB() const
{
  if (!m_bSupported)
    return NULL;

  CIccTagLutAtoB *pLut = new CIccTagLutAtoB();

  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve *pCurves = pLut->NewCurvesA();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  icUInt8Number i;
  icUInt32Number n;

  for (i = 0; i < 3; i++) {
    CIccTagCurve *pCurve = icHdrBakeNewCurve(m_params.nCurveSize);

    pCurves[i] = pCurve;

    if (!pCurve) {
      delete pLut;
      return NULL;
    }

    for (n = 0; n < m_params.nCurveSize; n++) {
      icFloatNumber v = (icFloatNumber)((double)n / (double)(m_params.nCurveSize - 1));
      icFloatNumber lin = icHdrBakeClampUnit(LinearizeChannel(i, v));

      (*pCurve)[n] = (icFloatNumber)pow((double)lin, 1.0 / icHdrBakeCurveExponent);
    }
  }

  pCurves = pLut->NewCurvesM();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 3; i++) {
    pCurves[i] = icHdrBakeNewCurve(0);

    if (!pCurves[i]) {
      delete pLut;
      return NULL;
    }
  }

  CIccMatrix *pMatrix = pLut->NewMatrix();

  if (!pMatrix) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 9; i++)
    pMatrix->m_e[i] = (icFloatNumber)(m_matrix[i] * icHdrBakePcsXyzScale);

  pMatrix->m_e[9] = pMatrix->m_e[10] = pMatrix->m_e[11] = (icFloatNumber)0.0;
  pMatrix->m_bUseConstants = false;

  pCurves = pLut->NewCurvesB();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 3; i++) {
    pCurves[i] = icHdrBakeNewCurve(0);

    if (!pCurves[i]) {
      delete pLut;
      return NULL;
    }
  }

  CIccHdrBakeAtoBExec exec(this);
  CIccCLUT *pCLUT = icHdrBakeNewClut(m_params.nGridPoints, &exec);

  if (!pCLUT || !pLut->SetCLUT(pCLUT)) {
    // SetCLUT() deletes the CLUT itself when the dimensions disagree, so a
    // false return leaves nothing here to free.
    delete pLut;
    return NULL;
  }

  return pLut;
}

/**
 ****************************************************************************
 * Name: CIccHdrBaker::CreateBtoA
 *
 * Purpose:
 *  Assemble the BToA tag, which is the AToB read backwards: identity B
 *  curves, the inverse matrix, fifth-root M curves, the inverse-gain CLUT,
 *  and A curves that are the inverse of the AToB's.
 *
 *  Both compressed stages are load-bearing and for the same reason as in the
 *  other direction - a uniform grid cannot represent an HDR transfer, so the
 *  transfer belongs in 1024-entry curves at both ends and the CLUT is left
 *  the smooth job in between.  See BtoAClutOp() for what happens when it is
 *  not.
 *
 *  This is the first of the two constructions the HAGC amendment's annex 2.2
 *  offers - the inverted gain curve sampled on its own grid, rather than a
 *  numerical inversion of the tag built above.  Inverting the analytic form
 *  once is both more accurate and simpler than inverting an approximation.
 *
 *  PROPOSAL-ISSUE HAGC-04 (design-level) -- annex 2.2 recommends building the
 *  BToA "by applying the inverted Headroom Adaptive Gain Curve" without stating
 *  that the curve is not always invertible: the gain is recoverable from the
 *  output only when one gain is common to all three channels or when mixing is
 *  component-only, and above the last control point the forward map is flat and
 *  has no inverse at all.  An implementer following the annex literally emits a
 *  BToA that silently does not invert its own AToB.  Resolved here by refusing.
 *
 * Return:
 *  The tag, owned by the caller, or NULL - which includes the case of a gain
 *  curve with no inverse, where a BToA cannot be built at all.
 *****************************************************************************
 */
CIccTagLutBtoA *CIccHdrBaker::CreateBtoA() const
{
  if (!IsInvertible())
    return NULL;

  CIccTagLutBtoA *pLut = new CIccTagLutBtoA();

  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigXYZData, icSigRgbData);

  LPIccCurve *pCurves = pLut->NewCurvesB();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  icUInt8Number i;
  icUInt32Number n;

  for (i = 0; i < 3; i++) {
    pCurves[i] = icHdrBakeNewCurve(0);

    if (!pCurves[i]) {
      delete pLut;
      return NULL;
    }
  }

  CIccMatrix *pMatrix = pLut->NewMatrix();

  if (!pMatrix) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 9; i++)
    pMatrix->m_e[i] = (icFloatNumber)(m_inverse[i] / icHdrBakePcsXyzScale);

  pMatrix->m_e[9] = pMatrix->m_e[10] = pMatrix->m_e[11] = (icFloatNumber)0.0;
  pMatrix->m_bUseConstants = false;

  pCurves = pLut->NewCurvesM();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 3; i++) {
    CIccTagCurve *pCurve = icHdrBakeNewCurve(m_params.nCurveSize);

    pCurves[i] = pCurve;

    if (!pCurve) {
      delete pLut;
      return NULL;
    }

    for (n = 0; n < m_params.nCurveSize; n++) {
      icFloatNumber v = (icFloatNumber)((double)n / (double)(m_params.nCurveSize - 1));

      (*pCurve)[n] = (icFloatNumber)pow((double)v, 1.0 / icHdrBakeCurveExponent);
    }
  }

  pCurves = pLut->NewCurvesA();

  if (!pCurves) {
    delete pLut;
    return NULL;
  }

  for (i = 0; i < 3; i++) {
    CIccTagCurve *pCurve = icHdrBakeNewCurve(m_params.nCurveSize);

    pCurves[i] = pCurve;

    if (!pCurve) {
      delete pLut;
      return NULL;
    }

    for (n = 0; n < m_params.nCurveSize; n++) {
      icFloatNumber v = (icFloatNumber)((double)n / (double)(m_params.nCurveSize - 1));

      // The inverse of the AToB's A curve, sampled over the same fifth-root
      // domain: undo the root, then encode.
      (*pCurve)[n] = EncodeChannel(i, (icFloatNumber)pow((double)v, icHdrBakeCurveExponent));
    }
  }

  CIccHdrBakeBtoAExec exec(this);
  CIccCLUT *pCLUT = icHdrBakeNewClut(m_params.nGridPoints, &exec);

  if (!pCLUT || !pLut->SetCLUT(pCLUT)) {
    delete pLut;
    return NULL;
  }

  return pLut;
}

/**
 ****************************************************************************
 * Name: icCreateHdrFallbackAtoB
 *****************************************************************************
 */
CIccTagLutAtoB *icCreateHdrFallbackAtoB(const CIccProfile *pProfile,
                                        const icHdrBakeParams *pParams,
                                        const icChar **pReason)
{
  CIccHdrBaker baker;

  if (!baker.Init(pProfile, pParams)) {
    if (pReason)
      *pReason = baker.GetUnsupportedReason();

    return NULL;
  }

  CIccTagLutAtoB *pLut = baker.CreateAtoB();

  if (!pLut && pReason)
    *pReason = "AToB tag could not be allocated";

  return pLut;
}

/**
 ****************************************************************************
 * Name: icCreateHdrFallbackBtoA
 *****************************************************************************
 */
CIccTagLutBtoA *icCreateHdrFallbackBtoA(const CIccProfile *pProfile,
                                        const icHdrBakeParams *pParams,
                                        const icChar **pReason)
{
  CIccHdrBaker baker;

  if (!baker.Init(pProfile, pParams)) {
    if (pReason)
      *pReason = baker.GetUnsupportedReason();

    return NULL;
  }

  if (!baker.IsInvertible()) {
    if (pReason)
      *pReason = "Tone mapping at this target has no inverse, so no BToA can be built";

    return NULL;
  }

  CIccTagLutBtoA *pLut = baker.CreateBtoA();

  if (!pLut && pReason)
    *pReason = "BToA tag could not be allocated";

  return pLut;
}

/**
 ****************************************************************************
 * Name: icAddHdrFallbackTags
 *
 * Purpose:
 *  Bake both directions into a profile.
 *
 *  Both tags are built before either is attached, so that a failure in the
 *  second leaves the profile exactly as it was rather than half patched.
 *
 * Args:
 *  pProfile = the profile to patch, in place
 *  pParams  = table sizes, HLG parameters and version policy, or NULL
 *  pReason  = receives static text on failure
 *****************************************************************************
 */
bool icAddHdrFallbackTags(CIccProfile *pProfile, const icHdrBakeParams *pParams,
                          const icChar **pReason)
{
  CIccHdrBaker baker;

  if (!baker.Init(pProfile, pParams)) {
    if (pReason)
      *pReason = baker.GetUnsupportedReason();

    return false;
  }

  if (!baker.IsInvertible()) {
    // ICC.1 8.3.2 pairs AToB0Tag with BToA0Tag for a display profile, and
    // clause 8.10's own pairing rule says the same for an HDR Profile.
    // Attaching only the forward direction would leave a profile whose
    // rendering cannot be undone, which is worse than not baking at all.
    if (pReason)
      *pReason = "Tone mapping at this target has no inverse, so the tag pair cannot be completed";

    return false;
  }

  CIccTagLutAtoB *pAtoB = baker.CreateAtoB();
  CIccTagLutBtoA *pBtoA = pAtoB ? baker.CreateBtoA() : NULL;

  if (!pAtoB || !pBtoA) {
    delete pAtoB;
    delete pBtoA;

    if (pReason)
      *pReason = "Fallback tags could not be allocated";

    return false;
  }

  // AttachTag() refuses a signature the profile already carries, so the old
  // pair goes first.  DeleteTag() on an absent tag is a no-op.
  pProfile->DeleteTag(icSigAToB0Tag);
  pProfile->DeleteTag(icSigBToA0Tag);

  pProfile->AttachTag(icSigAToB0Tag, pAtoB);
  pProfile->AttachTag(icSigBToA0Tag, pBtoA);

  if ((pParams ? pParams->nVersionPolicy : icHdrBakeVersionKeep) == icHdrBakeVersionV4_4)
    pProfile->m_Header.version = icVersionNumberV4_4;

  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
