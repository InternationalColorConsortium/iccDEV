/** @file
    File:       IccArrayBasic.cpp

    Contains:   Implementation of the IIccArray class and basic inherited classes

    Version:    V1

    Copyright:  � see ICC Software License
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
  #include <windows.h>
#endif
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "IccArrayBasic.h"
#include "IccStructBasic.h"
#include "IccUtil.h"
#include "IccArrayFactory.h"

#ifndef __min
#include <algorithm>
using std::min;
#define __min min
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


CIccArrayUnknown::CIccArrayUnknown(CIccTagArray *pTagArray, icArraySignature sigArray)
{
  m_pTag = pTagArray;
  m_sig = sigArray;
}


CIccArrayUnknown::~CIccArrayUnknown()
{

}


IIccArray* CIccArrayUnknown::NewCopy(CIccTagArray *pTagArray) const
{
  CIccArrayUnknown *rv = new CIccArrayUnknown(pTagArray);

  return rv;
}


void CIccArrayUnknown::Describe(std::string &sDescription, int nVerboseness) const
{
    sDescription += "\nCIccArrayUnknown::Describe()\n";
}


icValidateStatus CIccArrayUnknown::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile) const
{
  icValidateStatus rv = icValidateOK;

  if (m_pTag) {
    icUInt32Number i;
    
    sReport += "Unknown tag array type!\n";
    for (i=0; i<m_pTag->GetSize(); i++) {
      CIccTag *pTag = m_pTag->GetIndex(i);
      if (!pTag) {
        rv = icMaxStatus(rv, icValidateWarning);
        char buf[80];
        sprintf(buf, "Tag at index %d is NULL\n", i);
        sReport += buf;
      }
      else {
        rv = icMaxStatus(rv, pTag->Validate(sigPath+icGetSigPath(pTag->GetType()), sReport, pProfile));
      }
    }
  }
  else {
    sReport += "Array Handler not connected to CIccTagArray object!";

    rv = icValidateWarning;
  }

  return rv;
}


CIccArrayColorantInfo::CIccArrayColorantInfo(CIccTagArray *pTagArray)
{
  m_pTag = pTagArray;
  m_sig = icSigColorantInfoArray;
}


CIccArrayColorantInfo::~CIccArrayColorantInfo()
{

}


IIccArray* CIccArrayColorantInfo::NewCopy(CIccTagArray *pTagArray) const
{
  CIccArrayColorantInfo *rv = new CIccArrayColorantInfo(pTagArray);

  return rv;
}


void CIccArrayColorantInfo::Describe(std::string &sDescription, int nVerboseness) const
{
    sDescription += "\nCIccArrayColorantInfo::Describe()\n";
}


icValidateStatus CIccArrayColorantInfo::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile) const
{
  icValidateStatus rv = icValidateOK;

  if (m_pTag) {
    icUInt32Number i;

    for (i = 0; i < m_pTag->GetSize(); i++) {
      CIccTag *pTag = m_pTag->GetIndex(i);
      if (!pTag) {
        rv = icMaxStatus(rv, icValidateWarning);
        char buf[80];
        sprintf(buf, "Tag at index %d is NULL\n", i);
        sReport += buf;
      }
      else {
        if (pTag->GetTagStructType() != icSigColorantInfoStruct) {
          rv = icMaxStatus(rv, icValidateCriticalError);
          char buf[80]; 
          sprintf(buf, "Tag at index %d is not a colorantInfoStruct\n", i);
          sReport += buf;
        }
        rv = icMaxStatus(rv, pTag->Validate(sigPath + icGetSigPath(pTag->GetType()), sReport, pProfile));
      }
    }
  }
  else {
    sReport += "Array Handler not connected to CIccTagArray object!";

    rv = icValidateWarning;
  }

  return rv;
}


CIccArrayNamedColor::CIccArrayNamedColor(CIccTagArray *pTagArray)
{
  m_pTag = pTagArray;
  m_sig = icSigNamedColorArray;

  m_list = new icNamedColorStructList;

  m_nDeviceSamples = 0;
  m_nPcsSamples = 0;
  m_nSpectralSamples = 0;
  m_spectralRange = m_biSpectralRange = {};
  m_csDevice = icSigUnknownData;
  m_pZeroTint = NULL;
  m_csSpectralPcs = icSigNoSpectralData;
}


CIccArrayNamedColor::~CIccArrayNamedColor()
{
  delete m_list;
}


IIccArray* CIccArrayNamedColor::NewCopy(CIccTagArray *pTagArray) const
{
  CIccArrayNamedColor *rv = new CIccArrayNamedColor(pTagArray);

  if (rv) {
    rv->m_csPcs = m_csPcs;
    rv->m_csDevice = m_csDevice;
    rv->m_csSpectralPcs = m_csSpectralPcs;
    rv->m_spectralRange = m_spectralRange;
    rv->m_biSpectralRange = m_biSpectralRange;
  }

  return rv;
}


void CIccArrayNamedColor::Describe(std::string &sDescription, int nVerboseness) const
{
    sDescription += "\nCIccArrayNamedColor::Describe()\n";
}


void CIccArrayNamedColor::SetColorSpaces(icColorSpaceSignature csPcs, icColorSpaceSignature csDevice, 
                                         icSpectralColorSignature csSpectralPcs/* =icSigNoSpectralData */,
                                         const icSpectralRange *pSpectralRange /* = NULL */,
                                         const icSpectralRange *pBiSpectralRange /* = NULL */)
{
  m_csPcs = csPcs;
  m_csDevice = csDevice;
  m_csSpectralPcs = csSpectralPcs;
  if (pSpectralRange)
    m_spectralRange = *pSpectralRange;
  else
    memset(&m_spectralRange, 0, sizeof(m_spectralRange));

  if (pBiSpectralRange)
    m_biSpectralRange = *pBiSpectralRange;
  else
    memset(&m_biSpectralRange, 0, sizeof(m_biSpectralRange));

  m_nDeviceSamples = icGetSpaceSamples(m_csDevice);
  m_nPcsSamples = icGetSpaceSamples(m_csPcs);
  m_nSpectralSamples = icGetSpaceSamples((icColorSpaceSignature)m_csSpectralPcs);
}

bool CIccArrayNamedColor::Begin()
{
  m_pZeroTint = (CIccStructNamedColor*)icGetTagStructHandlerOfType(m_pTag->GetIndex(0), icSigTintZeroStruct);

  m_list->clear();

  int i, n=m_pTag->GetSize();
  for (i=1; i<n; i++) {
    CIccStructNamedColor *pNamedColor = (CIccStructNamedColor*)icGetTagStructHandlerOfType(m_pTag->GetIndex(i), icSigNamedColorStruct);
    if (pNamedColor) {
      std::string name = pNamedColor->getName();
      if (!name.empty())
        (*m_list)[name] = pNamedColor;
    }
  }

  return true;
}

CIccStructNamedColor* CIccArrayNamedColor::FindColor(const icChar *szColor) const
{
  std::string name(szColor);

  icNamedColorStructList::const_iterator i;
  i=m_list->find(name);
  if (i!=m_list->end())
    return i->second;

  return NULL;
}

CIccStructNamedColor* CIccArrayNamedColor::FindDeviceColor(const icFloatNumber *pDevColor) const
{
  icFloatNumber *temp = new icFloatNumber[m_nDeviceSamples];

  if (!temp)
    return NULL;

  icUInt32Number i, j, n=m_pTag->GetSize();
  for (i=1; i<n; i++) {
    CIccStructNamedColor *pNamedColor = (CIccStructNamedColor*)icGetTagStructHandlerOfType(m_pTag->GetIndex(i), icSigNamedColorStruct);
    if (pNamedColor) {
      CIccTag *pTag = pNamedColor->GetElem(icSigNmclDeviceDataMbr);
      if (pTag && pTag->IsNumArrayType()) {
        CIccTagNumArray *v = (CIccTagNumArray*)pTag;
        icUInt32Number n = v->GetNumValues()/m_nDeviceSamples;
        if (n) {
          v->GetValues(temp, (n-1)*m_nDeviceSamples, m_nDeviceSamples);

          for (j=0; j<m_nDeviceSamples; j++) {
            if (temp[j]!=pDevColor[j])
              break;
          }
          if (n==m_nDeviceSamples) {
            delete [] temp;
            return pNamedColor;
          }
        }
      }
    }
  }
  delete[] temp;
  return NULL;
}

CIccStructNamedColor* CIccArrayNamedColor::FindPcsColor(const icFloatNumber *pPCS, icFloatNumber dMinDE/*=1000.0*/) const
{
  icFloatNumber dCalcDE, dLeastDE=dMinDE;
  icFloatNumber pLabIn[3], pLab[3];
  CIccStructNamedColor* leastDEindex = NULL;

  if (m_csPcs != icSigLabData) {
    icXYZtoLab(pLabIn,pPCS);
  }
  else {
    memcpy(pLabIn, pPCS, 3*sizeof(icFloatNumber));
  }

  icUInt32Number i, n=m_pTag->GetSize();
  for (i=1; i<n; i++) {
    CIccStructNamedColor *pNamedColor = (CIccStructNamedColor*)icGetTagStructHandlerOfType(m_pTag->GetIndex(i), icSigNamedColorStruct);
    if (pNamedColor) {
      CIccTag *pTag = pNamedColor->GetElem(icSigNmclDeviceDataMbr);
      if (pTag && pTag->IsNumArrayType()) {
        CIccTagNumArray *v = (CIccTagNumArray*)pTag;
        icUInt32Number n = v->GetNumValues()/m_nDeviceSamples;
        if (n) {
          v->GetValues(pLab, (n-1)*m_nDeviceSamples, 3);

          dCalcDE = icDeltaE(pLabIn, pLab);

          if (dCalcDE<dMinDE && dCalcDE<dLeastDE) {
            dLeastDE = dCalcDE;
            leastDEindex = pNamedColor;
          }
        }
      }
    }
  }

  return leastDEindex;
}

CIccStructNamedColor* CIccArrayNamedColor::FindSpectralColor(const icFloatNumber *pSpec, icFloatNumber dMinRMS/*=1000.0*/) const
{
  icFloatNumber dCalcRMS, dLeastRMS=dMinRMS;
  CIccStructNamedColor* leastRMSindex = NULL;

  icFloatNumber *temp = new icFloatNumber[m_nSpectralSamples];

  if (!temp)
    return NULL;

  icUInt32Number i, n=m_pTag->GetSize();
  for (i=1; i<n; i++) {
    CIccStructNamedColor *pNamedColor = (CIccStructNamedColor*)icGetTagStructHandlerOfType(m_pTag->GetIndex(i), icSigNamedColorStruct);
    if (pNamedColor) {
      CIccTag *pTag = pNamedColor->GetElem(icSigNmclDeviceDataMbr);
      if (pTag && pTag->IsNumArrayType()) {
        CIccTagNumArray *v = (CIccTagNumArray*)pTag;
        icUInt32Number n = v->GetNumValues()/m_nDeviceSamples;
        if (n) {
          v->GetValues(temp, (n-1)*m_nSpectralSamples, m_nSpectralSamples);

          dCalcRMS = icRmsDif(pSpec, temp, m_nSpectralSamples);

          if (dCalcRMS<dMinRMS && dCalcRMS<dLeastRMS) {
            dLeastRMS = dCalcRMS;
            leastRMSindex = pNamedColor;
          }
        }
      }
    }
  }

  delete[] temp;

  return leastRMSindex;
}

bool CIccArrayNamedColor::GetDeviceTint(icFloatNumber *dstColor,
                                        const CIccStructNamedColor *pColor, 
                                        icFloatNumber tint/*=1.0f*/,
                                        icNamedColorlMemberSignature sig/*=icSigDeviceNmClrMember*/) const
{
  CIccTagNumArray *pZero;

  if (m_pZeroTint)
    pZero = m_pZeroTint->GetNumArray(sig);
  else
    pZero = NULL;

  if (!pColor)
    return false;

  return pColor->GetTint(dstColor, tint, pZero, sig, m_nDeviceSamples);
}


bool CIccArrayNamedColor::GetPcsTint(icFloatNumber *dstColor,
                                     const CIccStructNamedColor *pColor, 
                                     icFloatNumber tint/*=1.0f*/,
                                     icNamedColorlMemberSignature sig/*=icSigPcsNmClrMember*/) const
{
  CIccTagNumArray *pZero;

  if (m_pZeroTint)
    pZero = m_pZeroTint->GetNumArray(sig);
  else
    pZero = NULL;

  if (!pColor)
    return false;

  return pColor->GetTint(dstColor, tint, pZero, sig, m_nPcsSamples);
}

bool CIccArrayNamedColor::GetSpectralTint(icFloatNumber *dstColor,
                                          const CIccStructNamedColor *pColor, 
                                          icFloatNumber tint/*=1.0f*/,
                                          icNamedColorlMemberSignature sig/*=icSigSpectralNmClrMember*/) const
{
  CIccTagNumArray *pZero;

  if (m_pZeroTint)
    pZero = m_pZeroTint->GetNumArray(sig);
  else
    pZero = NULL;

  if (!pColor)
    return false;

  return pColor->GetTint(dstColor, tint, pZero, sig, m_nSpectralSamples);
}


icValidateStatus CIccArrayNamedColor::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile) const
{
  icValidateStatus rv = icValidateOK;

  if (m_pTag) {
    icUInt32Number i, n = m_pTag->GetSize();
    CIccTag *pSubTag;
    int nBad = 0;

    if (n<1) {
      sReport += "Named Color array must have at least 1 entry\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    for (i=0; i<n; i++) {
      pSubTag = m_pTag->GetIndex(i);
      if (pSubTag) {
        icStructSignature sig = pSubTag->GetTagStructType();
        if (!((i && sig==icSigNamedColorStruct) || (!i && sig==icSigTintZeroStruct))) {
          nBad++;
        }
        else {
         CIccTagStruct *pTag = (CIccTagStruct*)pSubTag;
         CIccStructNamedColor *pColor = (CIccStructNamedColor*)pTag->GetStructHandler();

         if (pColor) {
           CIccTagNumArray *pArray, *pTint;
           char str[256];

           pTint = pColor->GetNumArray(icSigNmclTintMbr);

           pArray = pColor->GetNumArray(icSigNmclDeviceDataMbr);
           if (pArray && m_nDeviceSamples) {
             icUInt32Number n = pArray->GetNumValues()/m_nDeviceSamples;

             if (n<1) {
               sprintf(str, "Insufficient device samples in NamedColor[%d]\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
             else if (pArray->GetNumValues() != n*m_nDeviceSamples) {
               sprintf(str, "Number of Device samples isn't an even multiple of Device samples in NamedColor[%d]!\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
             if (pTint && pTint->GetNumValues()!=n) {
               sprintf(str, "Number of device samples doesn't match tint values in NamedColor[%d]\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
           }

           pArray = pColor->GetNumArray(icSigNmclPcsDataMbr);
           if (pArray && m_nPcsSamples) {
             icUInt32Number n = pArray->GetNumValues()/m_nPcsSamples;

             if (n<1) {
               sprintf(str, "Insufficient PCS samples in NamedColor[%d]\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
             else if (pArray->GetNumValues() != n*m_nPcsSamples) {
               sprintf(str, "Number of PCS samples isn't an even multiple of PCS samples in NamedColor[%d]!\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
             if (pTint && pTint->GetNumValues()!=n) {
               sprintf(str, "Number of PCS samples doesn't match tint values in NamedColor[%d]\n", i);
               sReport += str;
               rv = icMaxStatus(rv, icValidateCriticalError);
             }
           }

           pArray = pColor->GetNumArray(icSigNmclSpectralDataMbr);
           if (pArray) {
             if (pArray->IsMatrixArray()) {
               CIccTagSparseMatrixArray *pArrayTag = (CIccTagSparseMatrixArray *) pArray;

               if (pArrayTag->GetChannelsPerMatrix()!= m_nSpectralSamples) {
                 sprintf(str, "Incompatible SpectralPcs samples in NamedColor[%d]\n", i);
                 sReport += str;
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
             }
             else if (m_nSpectralSamples) {
               icUInt32Number n = pArray->GetNumValues()/m_nSpectralSamples;

               if (n<1) {
                 sprintf(str, "Insufficient SpectralPcs samples in NamedColor[%d]\n", i);
                 sReport += str;
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
               else if (pArray->GetNumValues() != n*m_nSpectralSamples) {
                 sprintf(str, "Number of spectral samples isn't an even multiple of spectral PCS samples in Namedcolor[%d]!\n", i);
                 sReport += str;
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
               if (pTint && pTint->GetNumValues()!=n) {
                 sprintf(str, "Number of SpectralPCS samples doesn't match tint values in NamedColor[%d]\n", i);
                 sReport += str;
                 rv = icMaxStatus(rv, icValidateCriticalError);
               }
             }
           }
         }

        }
        rv = icMaxStatus(rv, pSubTag->Validate(sigPath, sReport, pProfile));
      }
      else {
        nBad++;
      }
    }
    if (nBad) {
      sReport += "Named Color array has invalid tag struct entries!\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }

  return rv;
}


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
