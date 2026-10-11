/** @file
    File:       IccPcc.h

    Contains:   Header for implementation of the IIccProfileConnectionConditions interface class.

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 4-10-2012
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCPCC_H)
#define _ICCPCC_H

#include "IccDefs.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class ICCPROFLIB_API CIccProfile;
class CIccTagSpectralViewingConditions;
class CIccTagMultiProcessElement;
class ICCPROFLIB_API CIccMatrixMath;

/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Interface for accessing Connection Conditions
**************************************************************************
*/
class ICCPROFLIB_API IIccProfileConnectionConditions
{
public:
  virtual ~IIccProfileConnectionConditions() {}

  virtual const CIccTagSpectralViewingConditions *getPccViewingConditions() = 0;
  virtual CIccTagMultiProcessElement *getCustomToStandardPcc() = 0;
  virtual CIccTagMultiProcessElement *getStandardToCustomPcc() = 0;

  bool isEquivalentPcc(IIccProfileConnectionConditions &IPCC);
  bool isStandardPcc();
  bool hasIlluminantSPD();

  icFloatNumber getObserverIlluminantScaleFactor();
  icFloatNumber getObserverWhiteScaleFactor(const icFloatNumber *pWhite, const icSpectralRange &whiteRange);


  icFloatNumber *getEmissiveObserver(const icSpectralRange &range, const icFloatNumber *pWhite, icFloatNumber *obsMatrix=NULL);  //Caller responsible for freeing results if obsMatrix is NULL
  CIccMatrixMath *getReflectanceObserver(const icSpectralRange &rangeRef);  //Caller responsible for deleting results of returned CIccMatrixMath object

  icIlluminant getPccIlluminant();
  icFloatNumber getPccCCT();
  icStandardObserver getPccObserver();

  virtual void getNormIlluminantXYZ(icFloatNumber *pXYZ)=0;
  virtual void getLumIlluminantXYZ(icFloatNumber *pXYZ)=0;
  virtual bool getMediaWhiteXYZ(icFloatNumber *pXYZ)=0;
};


/**
**************************************************************************
* Type: Class
* 
* Purpose: 
*  Class used for defining combined connection conditions where the 
*  viewing conditions are the combined result of information from
*  the profile and external connection conditions based on whether
*  reflectance or emission is used.
**************************************************************************
*/
class ICCPROFLIB_API CIccCombinedConnectionConditions : public IIccProfileConnectionConditions
{
public:
  CIccCombinedConnectionConditions(CIccProfile *pProfile, IIccProfileConnectionConditions *pAppliedPCC, bool bReflectance=false);
  virtual ~CIccCombinedConnectionConditions();

  virtual const CIccTagSpectralViewingConditions *getPccViewingConditions();
  virtual CIccTagMultiProcessElement *getCustomToStandardPcc();
  virtual CIccTagMultiProcessElement *getStandardToCustomPcc();

  virtual void getNormIlluminantXYZ(icFloatNumber *pXYZ);
  virtual void getLumIlluminantXYZ(icFloatNumber *pXYZLum);
  virtual bool getMediaWhiteXYZ(icFloatNumber *pXYZ);

protected:
  CIccTagSpectralViewingConditions *m_pViewingConditions;
  icFloatNumber m_illuminantXYZ[3];
  icFloatNumber m_illuminantXYZLum[3];
  icFloatNumber m_mediaXYZ[3];
  IIccProfileConnectionConditions *m_pPCC;
  bool m_bValidMediaXYZ;
  bool m_bValidIllumXYZ;
};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCPCC_H)
