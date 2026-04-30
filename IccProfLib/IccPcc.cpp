/** @file
    File:       IccPcc.cpp

    Contains:   Implementation of the IIccProfileConnectionConditions interface class.

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
#endif
#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccCmm.h"

static bool icCanMapSpectralRange(const icSpectralRange &srcRange, const icSpectralRange &dstRange)
{
  if (icSameSpectralRange(srcRange, dstRange))
    return true;

  return srcRange.steps > 1 &&
         dstRange.steps > 1 &&
         icNotZero(icF16toF(srcRange.end) - icF16toF(srcRange.start)) &&
         icNotZero(icF16toF(dstRange.end) - icF16toF(dstRange.start));
}

bool IIccProfileConnectionConditions::isEquivalentPcc(IIccProfileConnectionConditions &IPCC)
{
  icIlluminant illum = getPccIlluminant();
  icStandardObserver obs = getPccObserver();

  if (illum!=IPCC.getPccIlluminant() || obs!= IPCC.getPccObserver())
    return false;

  if ((illum==icIlluminantDaylight || illum==icIlluminantBlackBody) && getPccCCT()!=IPCC.getPccCCT())
    return false;

  if (illum==icIlluminantUnknown)
    return false;
    
  if (obs==icStdObsCustom) { 
    if (!hasIlluminantSPD() && !IPCC.hasIlluminantSPD()) {
      icFloatNumber XYZ1[3], XYZ2[3];
      getNormIlluminantXYZ(&XYZ1[0]);
      IPCC.getNormIlluminantXYZ(&XYZ2[0]);

      if (XYZ1[0]!=XYZ2[0] ||
          XYZ1[1]!=XYZ2[1] ||
          XYZ1[2]!=XYZ2[2])
        return false;
    }
    else {
      return false;
    }
  }

  return true;
}

icIlluminant IIccProfileConnectionConditions::getPccIlluminant()
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond)
    return icIlluminantD50;

  return pCond->getStdIllumiant();
}

icFloatNumber IIccProfileConnectionConditions::getPccCCT()
{
  const CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond)
    return 0.0f;

  return pCond->getIlluminantCCT();
}

icStandardObserver IIccProfileConnectionConditions::getPccObserver()
{
 const  CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond)
    return icStdObs1931TwoDegrees;

  return pCond->getStdObserver();
}

bool IIccProfileConnectionConditions::isStandardPcc()
{
  if (getPccIlluminant()==icIlluminantD50 && getPccObserver()==icStdObs1931TwoDegrees)
    return true;

  return false;
}

bool IIccProfileConnectionConditions::hasIlluminantSPD()
{
  const  CIccTagSpectralViewingConditions *pCond = getPccViewingConditions();
  if (!pCond)
    return false;

  icSpectralRange illumRange;
  const icFloatNumber *illum = pCond->getIlluminant(illumRange);

  if (!illumRange.steps || !illum)
    return false;

  return true;
}

icFloatNumber IIccProfileConnectionConditions::getObserverIlluminantScaleFactor()
{
  const CIccTagSpectralViewingConditions *pView = getPccViewingConditions();
  if (!pView)
    return 1.0;

  icSpectralRange illumRange;
  const icFloatNumber *illum = pView->getIlluminant(illumRange);

  icSpectralRange obsRange;
  const icFloatNumber *obs = pView->getObserver(obsRange);

  if (!illum || !obs || !illumRange.steps || !obsRange.steps)
    return 1.0;

  if (!icCanMapSpectralRange(obsRange, illumRange))
    return 1.0;

  int i, n = illumRange.steps;
  CIccMatrixMath *mapRange=CIccMatrixMath::rangeMap(obsRange, illumRange);
  icFloatNumber rv=0;

  if (mapRange) {
    icFloatNumber *Ycmf = new icFloatNumber[illumRange.steps];
    mapRange->VectorMult(Ycmf, &obs[obsRange.steps]);
    delete mapRange;

    for (i=0; i<n; i++) {
      rv += Ycmf[i]*illum[i];
    }
    delete [] Ycmf;
  }
  else {
   const icFloatNumber *Ycmf = &obs[obsRange.steps];

    for (i=0; i<n; i++) {
      rv += Ycmf[i]*illum[i];
    }
  }
  return rv;
}

icFloatNumber IIccProfileConnectionConditions::getObserverWhiteScaleFactor(const icFloatNumber *pWhite, const icSpectralRange &whiteRange)
{
  const CIccTagSpectralViewingConditions *pView = getPccViewingConditions();
  if (!pView)
    return 1.0;

  icSpectralRange obsRange;
  const icFloatNumber *obs = pView->getObserver(obsRange);

  if (!pWhite || !obs || !whiteRange.steps || !obsRange.steps)
    return 1.0;

  if (!icCanMapSpectralRange(obsRange, whiteRange))
    return 1.0;

  int i, n = whiteRange.steps;
  CIccMatrixMath *mapRange=CIccMatrixMath::rangeMap(obsRange, whiteRange);
  icFloatNumber rv=0;

  if (mapRange) {
    icFloatNumber *Ycmf = new icFloatNumber[whiteRange.steps];
    mapRange->VectorMult(Ycmf, &obs[obsRange.steps]);
    delete mapRange;

    for (i=0; i<n; i++) {
      rv += Ycmf[i]*pWhite[i];
    }
    delete [] Ycmf;
  }
  else {
    const icFloatNumber *Ycmf = &obs[obsRange.steps];

    for (i=0; i<n; i++) {
      rv += Ycmf[i]*pWhite[i];
    }
  }
  return rv;
}

icFloatNumber *IIccProfileConnectionConditions::getEmissiveObserver(const icSpectralRange &range, const icFloatNumber *pWhite, icFloatNumber *obs)
{
  const CIccTagSpectralViewingConditions *pView = getPccViewingConditions();
  if (!pView || !pWhite)
    return NULL;

  int i, n = range.steps, size = 3*n;
  const icFloatNumber *fptr;
  icFloatNumber *tptr;

  icSpectralRange observerRange;
  const icFloatNumber *observer = pView->getObserver(observerRange);
  
  // we can't do any calculations with this (non)observer
  if (!observer || observerRange.steps == 0)
    return NULL;

  if (!range.steps || !icCanMapSpectralRange(observerRange, range))
    return NULL;

  bool allocObs = !obs;
  if (!obs)
    obs = (icFloatNumber*)malloc(size*sizeof(icFloatNumber));

  if (obs) {
    CIccMatrixMath *mapRange=CIccMatrixMath::rangeMap(observerRange, range);

    //Copy observer while adjusting to range
    if (mapRange) {
        fptr = &observer[0];
        tptr = obs;
        for (i = 0; i < 3; i++) {
            mapRange->VectorMult(tptr, fptr);
            fptr += observerRange.steps;
            tptr += range.steps;
        }
        delete mapRange;
    }
    else {
      memcpy(obs, observer, size*sizeof(icFloatNumber));
    }

    //Calculate scale constant 
    icFloatNumber k=0.0f;
    fptr = &obs[range.steps]; //Using second color matching function
    for (i=0; i<(int)range.steps; i++) {
      k += fptr[i]*pWhite[i];
    }

    if (!icNotZero(k)) {
      if (allocObs)
        free(obs);
      return NULL;
    }

    //Scale observer so application of observer against white results in 1.0.
    for (i=0; i<size; i++) {
      obs[i] = obs[i] / k;
    }

    CIccMatrixMath observerMtx(3,range.steps);
    memcpy(observerMtx.entry(0), obs, size*sizeof(icFloatNumber));

    icFloatNumber xyz[3];
    observerMtx.VectorMult(xyz, pWhite);
  }

  return obs;
}

CIccMatrixMath *IIccProfileConnectionConditions::getReflectanceObserver(const icSpectralRange &rangeRef)
{
  CIccMatrixMath *pAdjust=NULL, *pMtx;
  const CIccTagSpectralViewingConditions *pView = getPccViewingConditions();

  icSpectralRange illumRange;
  const icFloatNumber *illum = pView->getIlluminant(illumRange);

  if (!illum || !rangeRef.steps || !illumRange.steps || !icCanMapSpectralRange(rangeRef, illumRange))
    return NULL;

  pMtx = CIccMatrixMath::rangeMap(rangeRef, illumRange);
  if (pMtx)
    pAdjust = pMtx;

  pMtx = pView->getObserverMatrix(illumRange);

  if (pAdjust) {
    pMtx = pAdjust->Mult(pMtx);
    delete pAdjust;
  }
  pAdjust = pMtx;

  if (!pAdjust)
    return NULL;

  pAdjust->VectorScale(illum);
  icFloatNumber rowSum = pAdjust->RowSum(1);
  if (!icNotZero(rowSum)) {
    delete pAdjust;
    return NULL;
  }

  pAdjust->Scale(1.0f / rowSum);

  return pAdjust;
}

CIccCombinedConnectionConditions::CIccCombinedConnectionConditions(CIccProfile *pProfile, 
                                                                   IIccProfileConnectionConditions *pAppliedPCC, 
                                                                   bool bReflectance/*=false*/)
{
  if (pAppliedPCC) {
      const CIccTagSpectralViewingConditions *pView = pAppliedPCC->getPccViewingConditions();
      if (!pView) {
        m_pPCC = NULL;
        m_pViewingConditions = NULL;
        m_bValidMediaXYZ = false;
        return;
      }
      if (bReflectance) {
          m_pPCC = pAppliedPCC;
          m_pViewingConditions = NULL;

          bool bValidIlluminantXYZ = icNotZero(pView->m_illuminantXYZ.Y);
          if (bValidIlluminantXYZ) {
            m_illuminantXYZ[0] = pView->m_illuminantXYZ.X / pView->m_illuminantXYZ.Y;
            m_illuminantXYZ[1] = 1.0f;
            m_illuminantXYZ[2] = pView->m_illuminantXYZ.Z / pView->m_illuminantXYZ.Y;
          }
          else {
            memset(m_illuminantXYZ, 0, 3 * sizeof(icFloatNumber));
          }
          m_illuminantXYZLum[0] = pView->m_illuminantXYZ.X;
          m_illuminantXYZLum[1] = pView->m_illuminantXYZ.Y;
          m_illuminantXYZLum[2] = pView->m_illuminantXYZ.Z;
          m_bValidMediaXYZ = pProfile->calcMediaWhiteXYZ(m_mediaXYZ, pAppliedPCC);
          if (!bValidIlluminantXYZ)
            m_bValidMediaXYZ = false;
      }
      else {
          m_pPCC = pAppliedPCC;
          m_pViewingConditions = (CIccTagSpectralViewingConditions *) pView->NewCopy();

          icSpectralRange illumRange;
          const icFloatNumber *illum = pView->getIlluminant(illumRange);

          m_pViewingConditions->setIlluminant(pView->getStdIllumiant(), illumRange, illum, pView->getIlluminantCCT());

          pProfile->calcNormIlluminantXYZ(m_illuminantXYZ, this);
          pProfile->calcLumIlluminantXYZ(m_illuminantXYZLum, this);
          m_bValidMediaXYZ = pProfile->calcMediaWhiteXYZ(m_mediaXYZ, this);
      }
  }
  else {
    m_pPCC = NULL;
    m_pViewingConditions = NULL;
    m_bValidMediaXYZ = false;
  }
}

CIccCombinedConnectionConditions::~CIccCombinedConnectionConditions()
{
  if (m_pViewingConditions)
    delete m_pViewingConditions;
}

const CIccTagSpectralViewingConditions *CIccCombinedConnectionConditions::getPccViewingConditions()
{
  if (m_pViewingConditions)
    return m_pViewingConditions;
  if (m_pPCC)
    return m_pPCC->getPccViewingConditions();
  return NULL;
}

CIccTagMultiProcessElement *CIccCombinedConnectionConditions::getCustomToStandardPcc()
{
  if (m_pPCC)
    return m_pPCC->getCustomToStandardPcc();
  return NULL;
}

CIccTagMultiProcessElement *CIccCombinedConnectionConditions::getStandardToCustomPcc()
{
  if (m_pPCC)
    return m_pPCC->getStandardToCustomPcc();
  return NULL;
}

void CIccCombinedConnectionConditions::getNormIlluminantXYZ(icFloatNumber *pXYZ)
{
  memcpy(pXYZ, m_illuminantXYZ, 3*sizeof(icFloatNumber));
}

void CIccCombinedConnectionConditions::getLumIlluminantXYZ(icFloatNumber *pXYZLum)
{
  memcpy(pXYZLum, m_illuminantXYZLum, 3 * sizeof(icFloatNumber));
}

bool CIccCombinedConnectionConditions::getMediaWhiteXYZ(icFloatNumber *pXYZ)
{
  if (m_pPCC || m_pViewingConditions) {
    memcpy(pXYZ, m_mediaXYZ, 3*sizeof(icFloatNumber));
    return m_bValidMediaXYZ;
  }
  return false;
}
