/** @file
    File:       IccCmm.cpp

    Contains:   Implementation of the CIccCmm class.

    Version:    V1

    Copyright:  � see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2025 The International Color Consortium. All rights
 * reserved.
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
 // -Initial implementation by Max Derhak 5-21-2025
 //
 //////////////////////////////////////////////////////////////////////

#include "IccCmmSearch.h"

#include <cmath>


CIccApplyCmmSearch::CIccApplyCmmSearch(CIccCmm* pBaseCmm) : CIccApplyCmm(pBaseCmm)
{
  CIccCmmSearch* pCmm = (CIccCmmSearch*)pBaseCmm;

  m_nApply = pCmm->m_pcc.size();
  if (!m_nApply)
    m_nApply = 1;
  if (m_nApply > pCmm->m_dst_to_mid.size())
    m_nApply = (size_t)pCmm->m_dst_to_mid.size();

  m_nSamples = pCmm->m_dst_to_mid[0]->GetDestSamples();
  icUInt16Number nSrcSamples = pCmm->m_dst_to_mid[0]->GetSourceSamples();

  m_mid_data.resize(m_nApply);

  // CWE-400/CWE-834: the cost walk indexes three parallel containers
  // (m_dst_to_mid, m_mid_data, m_weight) by i; by construction all are
  // >= m_nApply, but clamp to the smallest so a corrupted m_nApply can never
  // index any of them out of range.
  m_nCostApply = m_nApply;
  if (m_nCostApply > pCmm->m_dst_to_mid.size())
    m_nCostApply = pCmm->m_dst_to_mid.size();
  if (m_nCostApply > m_mid_data.size())
    m_nCostApply = m_mid_data.size();
  if (m_nCostApply > pCmm->m_weight.size())
    m_nCostApply = pCmm->m_weight.size();

  for (size_t i = 0; i < m_nCostApply; i++) {
    m_mid_data[i].resize(m_nSamples);
  }
  m_pixel.resize(m_nSamples);
  m_startPixel.resize(nSrcSamples);

  bUseBounds = pCmm->m_bUsesBounds;
  overBoundsCost = pCmm->m_fOverBoundsCost;
  m_bUnitBounds = pCmm->m_minBounds.size() != nSrcSamples || pCmm->m_maxBounds.size() != nSrcSamples;
  m_minBounds = pCmm->m_minBounds;
  m_maxBounds = pCmm->m_maxBounds;

  m_bNeedPcsToLab = pCmm->m_bNeedPcsToLab;
}

CIccApplyCmmSearch::~CIccApplyCmmSearch()
{
}

static icFloatNumber sq(icFloatNumber x) { return x * x; }

icFloatNumber CIccApplyCmmSearch::costFunc(CIccSearchVec& point)
{
  CIccCmmSearch* pCmm = (CIccCmmSearch*)m_pCmm;
  icFloatNumber sum = 0.0;
  icFloatNumber div = 0.0;

  for (size_t i = 0; i < m_nCostApply; i++) {
    // A failed reverse transform leaves m_pixel holding the previous candidate's
    // values, which would silently score this point against stale data (#1860).
    // costFunc has no status channel, so report the point as infeasible instead:
    // the same sentinel the bounds barrier uses keeps the optimizer away from it
    // rather than letting it converge on a garbage minimum.
    if (pCmm->m_dst_to_mid[i]->Apply(&m_pixel[0], &point.vec()[0]) != icCmmStatOk)
      return overBoundsCost;

    if (m_bNeedPcsToLab) {
      icLabFromPcs(&m_pixel[0]);
    }

    icFloatNumber difSum = 0;
    for (icUInt16Number j = 0; j < m_nSamples; j++) {
      difSum += sq(m_pixel[j] - m_mid_data[i][j]);
    }
    sum += sqrt(difSum) * pCmm->m_weight[i];
    div += pCmm->m_weight[i];
  }
  return sum / div;
}

bool CIccApplyCmmSearch::boundsCheck(const CIccSearchVec& point, icFloatNumber& boundsCost) const
{
  bool rv = false;
  boundsCost = 0;
  if (m_bUnitBounds) {
    for (size_t i = 0; i < point.size(); i++) {
      icFloatNumber v = point.index(i);
      if (v < 0.0f) {
        rv = true;
        boundsCost += sq(v);
      }
      else if (v > 1.0f) {
        rv = true;
        boundsCost += sq(v - 1.0f);
      }
    }
  }
  else {
    for (size_t i = 0; i < point.size(); i++) {
      icFloatNumber v = point.index(i);
      if (v < m_minBounds[i]) {
        rv = true;
        boundsCost += sq(m_minBounds[i] - v);
      }
      else if (v > m_maxBounds[i]) {
        rv = true;
        boundsCost += sq(v - m_maxBounds[i]);
      }
    }
  }
  if (rv) {
    boundsCost = sqrt(boundsCost);
  }
  return rv;
}

icStatusCMM CIccApplyCmmSearch::Apply(icFloatNumber* DstPixel, const icFloatNumber* SrcPixel)
{
  CIccCmmSearch* pCmm = (CIccCmmSearch*)m_pCmm;

  if (!pCmm->m_src_to_mid.size()) { 
    for (size_t i = 0; i < m_nCostApply; i++) {
      memcpy(&m_mid_data[i][0], SrcPixel, m_nSamples*sizeof(icFloatNumber));
    }
  }
  else {
    for (size_t i = 0; i < m_nCostApply; i++) {
      // Propagate the per-PCC forward transform status (#1860).  Swallowing it
      // left m_mid_data[i] holding whatever the previous pixel wrote, so the
      // search then optimised against a stale target and still reported success.
      icStatusCMM statMid = pCmm->m_src_to_mid[i]->Apply(&m_mid_data[i][0], SrcPixel);
      if (statMid != icCmmStatOk)
        return statMid;
    }
  }

  // Same for the transform that seeds the search's starting point: if it fails
  // m_startPixel is stale and every subsequent simplex vertex derives from it.
  icStatusCMM statStart = pCmm->m_mid_to_dst->Apply(&m_startPixel[0], &m_mid_data[0][0]);
  if (statStart != icCmmStatOk)
    return statStart;

  //Cost function needs delteEab so convert from PCS encoding to Lab for comparisons
  if (m_bNeedPcsToLab) {
    for (size_t i = 0; i < m_nCostApply; i++) {
      icLabFromPcs(&m_mid_data[i][0]);
    }
  }

  icFloatVector result = findMin(m_startPixel);

  memcpy(DstPixel, &result[0], result.size() * sizeof(icFloatNumber));

  return icCmmStatOk;
}

icStatusCMM CIccApplyCmmSearch::Apply(icFloatNumber* DstPixel, const icFloatNumber* SrcPixel, icUInt32Number nPixels)
{
  CIccCmmSearch* pCmm = (CIccCmmSearch*)m_pCmm;
  icUInt32Number nSrcSamples = pCmm->GetSourceSamples();
  icUInt32Number nDstSamples = pCmm->GetDestSamples();

  for (icUInt32Number i = 0; i < nPixels; i++) {
    // Stop on the first failing pixel and report it (#1860).  The single-pixel
    // overload above now returns real statuses, but discarding them here would
    // still hand the caller a buffer of partially-transformed pixels alongside
    // icCmmStatOk -- the same masking the per-stage Apply() calls had.
    icStatusCMM stat = Apply(DstPixel, SrcPixel);
    if (stat != icCmmStatOk)
      return stat;
    DstPixel += nDstSamples;
    SrcPixel += nSrcSamples;
  }

  return icCmmStatOk;
}

icStatusCMM CIccApplyCmmSearch::GetApplyCost(icFloatNumber& dCost, const icFloatNumber* SrcPixel)
{
  CIccCmmSearch* pCmm = (CIccCmmSearch*)m_pCmm;
  icUInt32Number nDstSamples = pCmm->GetDestSamples();

  // Find device values that best match the SrcPixel
  std::vector<icFloatNumber> dstPixel(nDstSamples, 0);
  icStatusCMM rv = Apply(&dstPixel[0], SrcPixel);
  if (rv != icCmmStatOk) 
    return rv;

  // costFunc reads m_pixel/m_mid_data/etc. set up by the preceding Apply().
  // Evaluate it at the found device-value point and return that cost.
  CIccSearchVec point(dstPixel);
  dCost = costFunc(point);
  return icCmmStatOk;
}

CIccCmmSearch::CIccCmmSearch(bool bUsesBounds, icFloatNumber overBoundsCost, const icFloatVector &minBounds, const icFloatVector &maxBounds)
{
  m_bUsesBounds = bUsesBounds;
  m_fOverBoundsCost = overBoundsCost;
  m_minBounds = minBounds;
  m_maxBounds = maxBounds;
}

CIccCmmSearch::~CIccCmmSearch()
{
  delete m_pSrcProfile;
  delete m_pMidProfile;
  delete m_pDstProfile;
  delete m_pDstInitProfile;

  for (auto pPcc : m_pcc) {
    delete pPcc;
  }
}

//virtual CIccPCS *GetPCS() { return new CIccPCS(); }

///Must make to or three calls to some form of AddXform() before calling Begin()
icStatusCMM CIccCmmSearch::AddXform(CIccProfile* pProfile,
  icRenderingIntent nIntent,
  icXformInterp nInterp,
  IIccProfileConnectionConditions* /* pPc */,
  icXformLutType nLutType,
  bool bUseD2BxB2DxTags,
  CIccCreateXformHintManager* /* pHintManager */)
{
  // This override owns pProfile on every path, matching the base
  // CIccCmm::AddXform contract (#1327): the successful cases below store it and
  // ~CIccCmmSearch frees it, while every rejection deletes it here.  Callers
  // (the filename and reference AddXform overloads) therefore never free it
  // themselves.  A NamedColor profile has no search LUT, so reject it -- and
  // free it, as the default case below already does (#1332).
  if (pProfile->m_Header.deviceClass == icSigNamedColorClass) {
    delete pProfile;
    return icCmmStatInvalidLut;
  }

  switch (m_nAttached) {
  case 0:
    m_pSrcProfile = pProfile;
    m_nSrcIntent = nIntent;
    m_nSrcInterp = nInterp;
    m_nSrcLutType = nLutType;
    m_bSrcUseD2BxB2DxTags = bUseD2BxB2DxTags;
    break;
  case 1:
    m_pDstProfile = pProfile;
    m_nDstIntent = nIntent;
    m_nDstInterp = nInterp;
    m_nDstLutType = nLutType;
    m_bDstUseD2BxB2DxTags = bUseD2BxB2DxTags;
    break;
  case 2:
    m_pMidProfile = m_pDstProfile;
    m_nMidIntent = m_nDstIntent;
    m_nMidInterp = m_nDstInterp;
    m_nMidLutType = m_nDstLutType;
    m_bMidUseD2BxB2DxTags = m_bDstUseD2BxB2DxTags;

    m_pDstProfile = pProfile;
    m_nDstIntent = nIntent;
    m_nDstInterp = nInterp;
    m_nDstLutType = nLutType;
    m_bDstUseD2BxB2DxTags = bUseD2BxB2DxTags;
    break;
  default:
    delete pProfile;
    return icCmmStatInvalidProfile;
  }

  m_nAttached++;

  return icCmmStatOk;
}

void CIccCmmSearch::SetDstInitProfile(CIccProfile* pProfile,
  icRenderingIntent nIntent,
  icXformInterp nInterp,
  IIccProfileConnectionConditions* pPcc,
  icXformLutType nLutType,
  bool bUseD2BxB2DxTags)
{
  m_pDstInitProfile = pProfile;
  m_nDstInitIntent = nIntent;
  m_nDstInitInterp = nInterp;
  m_pDstInitPcc = pPcc;
  m_nDstInitLutType = nLutType;
  m_bDstInitUseD2BxB2DxTags = bUseD2BxB2DxTags;
}


icStatusCMM CIccCmmSearch::AttachPCC(IIccProfileConnectionConditions* pPCC, icFloatNumber dWeight)
{
  if (!pPCC || !std::isfinite(dWeight) || dWeight <= 0.0f)
    return icCmmStatBadXform;

  m_pcc.push_back(pPCC);
  m_weight.push_back(dWeight);

  return icCmmStatOk;
}

#define checkCmmStatus(rv) if (rv != icCmmStatOk) return rv

icStatusCMM CIccCmmSearch::Begin(bool /* bAllocNewApply */, bool /* bUsePcsConversion */)
{
  icStatusCMM rv;

  if (m_nAttached < 2)
    return icCmmStatBadXform;

  // Resolve every attached profile's connection conditions before any sub-CMM is
  // built (#1860).  The sub-chains below mix the two CIccCmm::AddXform overloads:
  // the src/mid/dst profiles go in by reference, which copy-constructs the profile
  // and then detaches the copy from its file IO (CopyAttach(nullptr)), while the
  // -INIT initial-destination profile goes in by pointer and keeps its IO.  A
  // detached copy can no longer load spectralViewingConditions on demand, so when
  // CIccCmm::Begin() reaches CheckPCSConnections()/pushXYZConvert() it reads the
  // D50/2-degree default for one end of the chain and the profile's real PCC for
  // the other.  isEquivalentPcc() then reports a mismatch and splices a bogus
  // chromatic-adaptation CIccPcsXform into mid_to_dst.  For a v5 profile carrying
  // a non-D50 'svcn' (e.g. Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc,
  // which declares D65) that turned an identity sRGB->sRGB search into a D50<->D65
  // adaptation, and because the search is fenced by a 1e6 out-of-gamut barrier the
  // resulting start point could not be recovered from.  Loading the three PCC tags
  // here means the reference-overload copies inherit them, so both ends of every
  // sub-chain agree and only genuinely differing PCCs get an adaptation.
  // CIccConnectCmm::CreateSearch() already does exactly this for the weighted PCC
  // profiles it attaches; this extends the same treatment to the chain profiles.
  if (m_pSrcProfile)
    m_pSrcProfile->ReadPccTags();
  if (m_pMidProfile)
    m_pMidProfile->ReadPccTags();
  if (m_pDstProfile)
    m_pDstProfile->ReadPccTags();
  if (m_pDstInitProfile)
    m_pDstInitProfile->ReadPccTags();

  if (m_nAttached == 2) {
    //mid_to_dst
    CIccCmmPtr cmm = CIccCmmPtr(new CIccCmm); 
    rv = cmm->AddXform(*m_pSrcProfile, m_nSrcIntent, m_nSrcInterp, m_pSrcPcc, m_nSrcLutType, m_bSrcUseD2BxB2DxTags);
    checkCmmStatus(rv);

    if (m_pDstInitProfile) {
      rv = cmm->AddXform(m_pDstInitProfile, m_nDstInitIntent, m_nDstInitInterp, m_pDstInitPcc, m_nDstInitLutType, m_bDstInitUseD2BxB2DxTags);
      m_pDstInitProfile = nullptr;
      checkCmmStatus(rv);
    }
    else {
      rv = cmm->AddXform(*m_pDstProfile, m_nDstIntent, m_nDstInterp, m_pDstPcc, m_nDstLutType, m_bDstUseD2BxB2DxTags);
      checkCmmStatus(rv);
    }

    rv = cmm->Begin();
    checkCmmStatus(rv);

    m_mid_to_dst = cmm;

    //dst_to_mid
    cmm = CIccCmmPtr(new CIccCmm);
    rv = cmm->AddXform(*m_pDstProfile, m_nDstIntent, m_nDstInterp, m_pcc.size() ? m_pcc[0] : m_pDstPcc, m_nDstLutType, m_bDstUseD2BxB2DxTags);
    checkCmmStatus(rv);

    rv = cmm->AddXform(*m_pSrcProfile, m_nSrcIntent, m_nSrcInterp, m_pcc.size() ? m_pcc[0] : m_pSrcPcc, m_nSrcLutType, m_bSrcUseD2BxB2DxTags);
    checkCmmStatus(rv);

    delete m_pDstProfile;
    m_pDstProfile = nullptr;

    rv = cmm->Begin();
    checkCmmStatus(rv);
    m_dst_to_mid.push_back(cmm);
       
    m_nSrcSpace = m_mid_to_dst->GetSourceSpace();
    m_nDestSpace = m_mid_to_dst->GetDestSpace();
    m_nLastIntent = m_mid_to_dst->GetLastIntent();
    m_nLastSpace = m_mid_to_dst->GetLastSpace();

    if (!m_weight.size())
      m_weight.push_back(1.0);
  }
  else {
    if (m_weight.size() < m_pcc.size())
      return icCmmStatBadXform;

    //mid_to_dst
    CIccCmmPtr cmm = CIccCmmPtr(new CIccCmm);
    rv = cmm->AddXform(*m_pMidProfile, m_nMidIntent, m_nMidInterp, m_pMidPcc, m_nMidLutType, m_bMidUseD2BxB2DxTags);
    checkCmmStatus(rv);

    if (m_pDstInitProfile) {
      rv = cmm->AddXform(m_pDstInitProfile, m_nDstInitIntent, m_nDstInitInterp, m_pDstInitPcc, m_nDstInitLutType, m_bDstInitUseD2BxB2DxTags);
      m_pDstInitProfile = nullptr;
      checkCmmStatus(rv);
    }
    else {
      rv = cmm->AddXform(*m_pDstProfile, m_nDstIntent, m_nDstInterp, m_pDstPcc, m_nDstLutType, m_bDstUseD2BxB2DxTags);
      checkCmmStatus(rv);
    }

    rv = cmm->Begin();
    checkCmmStatus(rv);

    m_mid_to_dst = cmm;

    if (m_pcc.size()) {
      m_nSrcSpace = icSigUnknownData;

      for (auto pcc : m_pcc) {
        //dst_to_mid
        cmm = CIccCmmPtr(new CIccCmm);
        rv = cmm->AddXform(*m_pDstProfile, m_nDstIntent, m_nDstInterp, pcc, m_nDstLutType, m_bDstUseD2BxB2DxTags);
        checkCmmStatus(rv);

        rv = cmm->AddXform(*m_pMidProfile, m_nMidIntent, m_nMidInterp, pcc, m_nMidLutType, m_bMidUseD2BxB2DxTags);
        checkCmmStatus(rv);

        rv = cmm->Begin();
        checkCmmStatus(rv);
        m_dst_to_mid.push_back(cmm);

        //src_to_mid
        cmm = CIccCmmPtr(new CIccCmm);
        rv = cmm->AddXform(*m_pSrcProfile, m_nSrcIntent, m_nSrcInterp, pcc, m_nSrcLutType, m_bSrcUseD2BxB2DxTags);
        checkCmmStatus(rv);

        rv = cmm->AddXform(*m_pMidProfile, m_nMidIntent, m_nMidInterp, pcc, m_nMidLutType, m_bMidUseD2BxB2DxTags);
        checkCmmStatus(rv);

        rv = cmm->Begin();
        checkCmmStatus(rv);
        m_src_to_mid.push_back(cmm);

        if (m_nSrcSpace == icSigUnknownData)
          m_nSrcSpace = cmm->GetSourceSpace();
      }
    }
    else {  //We must have PCC entries if we have a middle profile
      return icCmmStatBadConnection;
    }

    m_nDestSpace = m_mid_to_dst->GetDestSpace();
    m_nLastIntent = m_mid_to_dst->GetLastIntent();
    m_nLastSpace = m_mid_to_dst->GetLastSpace();
  }

  CIccXform* pLastXform = m_dst_to_mid[0]->GetLastXform();
  if (pLastXform && pLastXform->GetProfile() &&
      pLastXform->GetProfile()->m_Header.version<icVersionNumberV5 &&
      pLastXform->GetProfile()->m_Header.colorSpace == icSigLabData)
    m_bNeedPcsToLab = true;
  else
    m_bNeedPcsToLab = false;

  m_pApply = new CIccApplyCmmSearch(this);

  m_bValid = true;

  return rv;
}

//Call to Detach and remove all pending IO objects attached to the profiles used by the CMM. Should be called only after Begin()
icStatusCMM CIccCmmSearch::RemoveAllIO()
{
  for (auto p : m_src_to_mid)
    p->RemoveAllIO();

  for (auto p : m_dst_to_mid)
    p->RemoveAllIO();

  m_mid_to_dst->RemoveAllIO();

  return icCmmStatOk;
}

icStatusCMM CIccCmmSearch::GetApplyCost(icFloatNumber& dCost, const icFloatNumber* SrcPixel)
{
  dCost = -1;
  if (!m_bValid || !m_pApply)
    return icCmmStatBad;

  CIccApplyCmmSearch* pApply = static_cast<CIccApplyCmmSearch*>(m_pApply);

  return pApply->GetApplyCost(dCost, SrcPixel);
}
