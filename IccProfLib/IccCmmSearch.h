/** @file
    File:       IccCmm.h

    Contains:   Header file for implementation of the CIccCmm class.

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
 // -Initial implementation by Max Derhak 5-15-2003
 // -Added support for Monochrome ICC profile apply by Rohit Patil 12-03-2008
 // -Integrate changes for PCS adjustment by George Pawle 12-09-2008
 //
 //////////////////////////////////////////////////////////////////////

#if !defined(_ICCCMMSEARCH_H)
#define _ICCCMMSEARCH_H

#include "IccCmm.h"
#include "IccSearch.h"
#include <vector>

#if defined(USEICCDEVNAMESPACE)
namespace iccDEV {
#endif


typedef std::shared_ptr<CIccCmm> CIccCmmPtr;
typedef std::vector<CIccCmmPtr> CIccCmmPtrArray;
typedef std::vector<IIccProfileConnectionConditions*> CIccPccPtrArray;
typedef std::vector<icFloatVector> CIccPixelArray;

/**
**************************************************************************
* Type: Class
*
* Purpose: Defines a class that provides an interface for applying pixel
*  transformations through a CIccCmmSearch cmm.
*
**************************************************************************
*/
class ICCPROFLIB_API CIccApplyCmmSearch : public CIccApplyCmm, public CIccMinSearch
{
  friend class CIccCmmSearch;
public:
  virtual ~CIccApplyCmmSearch();

  virtual icFloatNumber costFunc(CIccSearchVec& point);

  virtual bool boundsCheck(const CIccSearchVec& point, icFloatNumber& boundsCost) const;

  virtual icStatusCMM Apply(icFloatNumber* DstPixel, const icFloatNumber* SrcPixel);

  //Make sure that when DstPixel==SrcPixel the sizeof DstPixel is less than size of SrcPixel
  virtual icStatusCMM Apply(icFloatNumber* DstPixel, const icFloatNumber* SrcPixel, icUInt32Number nPixels);

protected:
  CIccApplyCmmSearch(CIccCmm* pCmm);

  CIccPixelArray m_mid_data;
  icFloatVector m_pixel;
  icFloatVector m_startPixel;
  size_t m_nApply;
  icUInt16Number m_nSamples;

  bool m_bUnitBounds;
  icFloatVector m_minBounds;
  icFloatVector m_maxBounds;

  bool m_bNeedPcsToLab;
};



class ICCPROFLIB_API CIccCmmSearch : public CIccCmm
{
  friend class CIccApplyCmmSearch;
public:
  CIccCmmSearch(bool bUseBounds=true, icFloatNumber overBoundsCost = 1.0e6, const icFloatVector &minBounds=icFloatVector(), const icFloatVector &maxBounds=icFloatVector());
  virtual ~CIccCmmSearch();

  //Must make two or three calls to some form of AddXform() before calling Begin()
  virtual icStatusCMM AddXform(CIccProfile* pProfile,
    icRenderingIntent nIntent = icUnknownIntent,
    icXformInterp nInterp = icInterpLinear,
    IIccProfileConnectionConditions* pPcc = NULL,
    icXformLutType nLutType = icXformLutColor,
    bool bUseD2BxB2DxTags = true,
    CIccCreateXformHintManager* pHintManager = NULL);  //Note: profile will be owned by the CMM
  virtual icStatusCMM AddXform(CIccProfile* pProfile,
    CIccTag* pXformTag,
    icRenderingIntent nIntent = icUnknownIntent,
    icXformInterp nInterp = icInterpLinear,
    IIccProfileConnectionConditions* pPcc = NULL,
    bool bUseD2BxB2DxTags = false,
    CIccCreateXformHintManager* pHintManager = NULL) { return icCmmStatUnsupported; }

  void SetDstInitProfile(CIccProfile* pProfile,
    icRenderingIntent nIntent,
    icXformInterp nInterp,
    IIccProfileConnectionConditions* pPcc,
    icXformLutType nLutType,
    bool bUseD2BxB2DxTags);

  icStatusCMM AttachPCC(IIccProfileConnectionConditions* pPCC, icFloatNumber dWeight);

  //The Begin function should be called before Apply or GetNewApplyCmm()
  virtual icStatusCMM Begin(bool bAllocNewApply = true, bool bUsePcsConversion = false);

  //Get an additional Apply CMM object to apply pixels with.  The Apply object should be deleted by the caller.
  virtual CIccApplyCmm* GetNewApplyCmm(icStatusCMM& status) { return nullptr; }

  //Call to Detach and remove all pending IO objects attached to the profiles used by the CMM. Should be called only after Begin()
  virtual icStatusCMM RemoveAllIO();

  ///Returns the number of profiles/transforms added 
  virtual icUInt32Number GetNumXforms() const { return m_nAttached; }

  virtual void IterateXforms(IXformIterator* pIterater) const {}

protected:

  int m_nAttached=0;
  bool m_bUsesBounds;
  icFloatNumber m_fOverBoundsCost;
  icFloatVector m_minBounds;
  icFloatVector m_maxBounds;

  bool m_bNeedPcsToLab;
  
  CIccProfile* m_pSrcProfile = nullptr;
  icRenderingIntent m_nSrcIntent;
  icXformInterp m_nSrcInterp;
  IIccProfileConnectionConditions* m_pSrcPcc = nullptr;
  icXformLutType m_nSrcLutType;
  bool m_bSrcUseD2BxB2DxTags;

  CIccProfile* m_pDstProfile=nullptr;
  icRenderingIntent m_nDstIntent;
  icXformInterp m_nDstInterp;
  IIccProfileConnectionConditions* m_pDstPcc = nullptr;
  icXformLutType m_nDstLutType;
  bool m_bDstUseD2BxB2DxTags;

  CIccProfile* m_pDstInitProfile = nullptr;
  icRenderingIntent m_nDstInitIntent;
  icXformInterp m_nDstInitInterp;
  IIccProfileConnectionConditions* m_pDstInitPcc = nullptr;
  icXformLutType m_nDstInitLutType;
  bool m_bDstInitUseD2BxB2DxTags;

  CIccProfile* m_pMidProfile=nullptr;
  icRenderingIntent m_nMidIntent;
  icXformInterp m_nMidInterp;
  IIccProfileConnectionConditions* m_pMidPcc = nullptr;
  icXformLutType m_nMidLutType;
  bool m_bMidUseD2BxB2DxTags;

  CIccCmmPtrArray m_dst_to_mid;
  CIccCmmPtrArray m_src_to_mid;
  CIccCmmPtr m_mid_to_dst; //initial

  CIccPccPtrArray m_pcc;
  std::vector<icFloatNumber> m_weight;
};

#if defined(USEICCDEVNAMESPACE)
}; //namespace iccDEV
#endif


#endif