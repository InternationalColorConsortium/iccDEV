/** @file
    File:       IccStructBasic.cpp

    Contains:   Implementation of the IIccStruct class and basic inherited classes

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
#include "IccStructBasic.h"
#include "IccUtil.h"
#include "IccStructFactory.h"

#ifndef __min
#include <algorithm>
using std::min;
#define __min min
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


CIccStructUnknown::CIccStructUnknown(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = NULL;
}


CIccStructUnknown::~CIccStructUnknown()
{

}


IIccStruct* CIccStructUnknown::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructUnknown *rv = new CIccStructUnknown(pTagStruct);

  return rv;
}


void CIccStructUnknown::Describe(std::string &sDescription, int nVerboseness) const
{
  const size_t bufSize = 256;
  char buf[bufSize];
  CIccInfo info;
  int n;

  if (m_pTagStruct) {
    TagEntryList *entries = m_pTagStruct->GetElemList();
    TagEntryList::iterator i;

    for (n=0, i=entries->begin(); i!=entries->end(); n++, i++) {
      if (i->pTag) {
        if (n)
          sDescription += "\n";
        snprintf(buf, bufSize, "Begin SubTag(%s)\n", GetElemName(i->TagInfo.sig).c_str());
        sDescription += buf;
        i->pTag->Describe(sDescription, nVerboseness);
        snprintf(buf, bufSize, "End SubTag(%s)\n", GetElemName(i->TagInfo.sig).c_str());
        sDescription += buf;
      }
    }
  }
}


std::string CIccStructUnknown::GetElemName(icSignature sigElem) const
{
  if (m_pElemNameSigTable) {
    int i;
    for (i = 0; m_pElemNameSigTable[i].szName; i++) {
      if (m_pElemNameSigTable[i].nSig == sigElem)
        return m_pElemNameSigTable[i].szName;
    }
  }

  CIccInfo info;
  std::string str;

  str = "PrivateSubTag";
  //str += info.GetSigName(sigElem);

  return str;
}

icSignature CIccStructUnknown::GetElemSig(const icChar *szElemName) const
{
  if (m_pElemNameSigTable) {
    int i;
    for (i = 0; m_pElemNameSigTable[i].szName; i++) {
      if (!strcmp(m_pElemNameSigTable[i].szName, szElemName))
        return m_pElemNameSigTable[i].nSig;
    }
  }

  return (icSignature)0;
}

CIccTag *CIccStructUnknown::GetElem(icSignature sigElem) const
{
  if (!m_pTagStruct)
    return NULL;

  return m_pTagStruct->FindElem(sigElem);
}


icValidateStatus CIccStructUnknown::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/* = NULL*/) const
{
  icValidateStatus rv = icValidateOK;

  TagEntryList *pList = getTagEntries();
  if (pList) {
    // Per Tag tests
    TagEntryList::iterator i;
    for (i = pList->begin(); i != pList->end(); i++) {
      if (i->pTag)
        rv = icMaxStatus(rv, i->pTag->Validate(sigPath + icGetSigPath(i->TagInfo.sig), sReport, pProfile));
    }
  }
  else {
    sReport += "Struct Handler not connected to CIccTagStruct object with valid sub-tags!";

    rv = icValidateWarning;
  }

  return rv;
}



static SIccElemNameSig g_IccStructBRDFMbrTable[] = {
  { icSigBrdfTypeMbr,             "brdfTypeMbr" },
  { icSigBrdfFunctionMbr,         "brdfFunctionMbr" },
  { icSigBrdfParamsPerChannelMbr, "brdfParamsPerChannelMbr" },
  { icSigBrdfTransformMbr,        "brdfTransformMbr" },
  { 0, NULL },
};

CIccStructBRDF::CIccStructBRDF(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructBRDFMbrTable;
}


CIccStructBRDF::~CIccStructBRDF()
{

}


IIccStruct* CIccStructBRDF::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructBRDF *rv = new CIccStructBRDF(pTagStruct);

  return rv;
}


icSigBRDFType CIccStructBRDF::GetBRDFType() const
{
  CIccTag* pTag = GetElem(icSigBrdfTypeMbr);

  if (pTag)
  {
    if (pTag->GetType() == icSigSignatureType)
    {
      CIccTagSignature* pTagSig = dynamic_cast<CIccTagSignature*>(pTag);
      if (pTagSig)
        return  (icSigBRDFType)pTagSig->GetValue();
    }
  }
  return (icSigBRDFType)icSigUnknownType;
}


icSigBRDFFunction CIccStructBRDF::GetBRDFFunction() const
{
  CIccTag* pTag = GetElem(icSigBrdfFunctionMbr);

  if (pTag)
  {
    if (pTag->GetType() == icSigSignatureType)
    {
      CIccTagSignature* pTagSig = dynamic_cast<CIccTagSignature*>(pTag);
      if (pTagSig)
        return  (icSigBRDFFunction)pTagSig->GetValue();
    }
  }
  return (icSigBRDFFunction)icSigUnknownType;
}

icValidateStatus CIccStructBRDF::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/* = NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  if (m_pTagStruct) {
    if (!m_pTagStruct->FindElem(icSigBrdfTypeMbr) || !m_pTagStruct->FindElem(icSigBrdfFunctionMbr) ||
        !m_pTagStruct->FindElem(icSigBrdfParamsPerChannelMbr) || !m_pTagStruct->FindElem(icSigBrdfTransformMbr)) {
      CIccInfo Info;
      std::string sSigPathName = Info.GetSigPathName(sigPath);

      rv = icValidateCriticalError;
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Missing required struct member(s).\n";
    }
  }

  return icMaxStatus(rv, CIccStructUnknown::Validate(sigPath, sReport, pProfile));
}




static SIccElemNameSig g_IccStructColorEncodingParamsMbrTable[] = {
  { icSigCeptBluePrimaryXYZMbr,                "ceptBluePrimaryXYZMbr" },
  { icSigCeptGreenPrimaryXYZMbr,               "cptGreenPrimaryXYZMbr" },
  { icSigCeptRedPrimaryXYZMbr,                 "ceptRedPrimaryXYZMbr" },
  { icSigCeptTransferFunctionMbr,              "ceptTransferFunctionMbr" },
  { icSigCeptInverseTransferFunctionMbr,       "ceptInverseTransferFunctionMbr" },
  { icSigCeptLumaChromaMatrixMbr,              "ceptLumaChromaMatrixMbr" },
  { icSigCeptWhitePointLuminanceMbr,           "ceptWhitePointLuminanceMbr" },
  { icSigCeptWhitePointChromaticityMbr,        "ceptWhitePointChromaticityMbr" },
  { icSigCeptEncodingRangeMbr,                 "ceptEncodingRangeMbr" },
  { icSigCeptBitDepthMbr,                      "ceptBitDepthMbr" },
  { icSigCeptImageStateMbr,                    "ceptImageStateMbr" },
  { icSigCeptImageBackgroundMbr,               "ceptImageBackgroundMbr" },
  { icSigCeptViewingSurroundMbr,               "ceptViewingSurroundMbr" },
  { icSigCeptAmbientIlluminanceMbr,            "ceptAmbientIlluminanceMbr" },
  { icSigCeptAmbientWhitePointLuminanceMbr,    "ceptAmbientWhitePointLuminanceMbr" },
  { icSigCeptAmbientWhitePointChromaticityMbr, "ceptAmbientWhitePointChromaticityMbr" },
  { icSigCeptViewingFlareMbr,                  "ceptViewingFlareMbr" },
  { icSigCeptValidRelativeLuminanceRangeMbr,   "ceptValidRelativeLuminanceRangeMbr" },
  { icSigCeptMediumWhitePointLuminanceMbr,     "ceptMediumWhitePointLuminanceMbr" },
  { icSigCeptMediumWhitePointChromaticityMbr,  "ceptMediumWhitePointChromaticityMbr" },
  { icSigCeptMediumBlackPointLuminanceMbr,     "ceptMediumBlackPointLuminanceMbr" },
  { icSigCeptMediumBlackPointChromaticityMbr,  "ceptMediumBlackPointChromaticityMbr" },
  { 0, NULL },
};

CIccStructColorEncodingParams::CIccStructColorEncodingParams(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructColorEncodingParamsMbrTable;
}


CIccStructColorEncodingParams::~CIccStructColorEncodingParams()
{

}


IIccStruct* CIccStructColorEncodingParams::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructColorEncodingParams *rv = new CIccStructColorEncodingParams(pTagStruct);

  return rv;
}



static SIccElemNameSig g_IccStructColorantInfoMbrTable[] = {
  { icSigCinfNameMbr,          "cinfNameMbr" },
  { icSigCinfLocalizedNameMbr, "cinfLocalizedNameMbr" },
  { icSigCinfPcsDataMbr,       "cinfPcsDataMbr" },
  { icSigCinfSpectralDataMbr,  "cinfSpectralDataMbr" },
  { 0, NULL },
};

CIccStructColorantInfo::CIccStructColorantInfo(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructColorantInfoMbrTable;
}


CIccStructColorantInfo::~CIccStructColorantInfo()
{

}


IIccStruct* CIccStructColorantInfo::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructColorantInfo *rv = new CIccStructColorantInfo(pTagStruct);

  return rv;
}


icValidateStatus CIccStructColorantInfo::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/* = NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  if (m_pTagStruct) {
    if (!m_pTagStruct->FindElem(icSigCinfNameMbr)) {
      CIccInfo Info;
      std::string sSigPathName = Info.GetSigPathName(sigPath);

      rv = icValidateCriticalError;
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Missing required struct member(s).\n";
    }
  }

  return icMaxStatus(rv, CIccStructUnknown::Validate(sigPath, sReport, pProfile));
}




static SIccElemNameSig g_IccStructMeasurementInfoElemTable[] = {
  { icSigMeasBackingMbr,         "measBackingMbr" },
  { icSigMeasFlareMbr,           "measFlareMbr" },
  { icSigMeasGeometryMbr,        "measGeometryMbr" },
  { icSigMeasIlluminantMbr,      "measIlluminantMbr" },
  { icSigMeasIlluminantRangeMbr, "measIlluminantRangeMbr" },
  { icSigMeasModeMbr,            "measModeMbr" },
  { 0, NULL },
};

CIccStructMeasurementInfo::CIccStructMeasurementInfo(CIccTagStruct *pTagStruct)
{
  m_pTagStruct= pTagStruct;
  m_pElemNameSigTable = g_IccStructMeasurementInfoElemTable;
}


CIccStructMeasurementInfo::~CIccStructMeasurementInfo()
{

}


IIccStruct* CIccStructMeasurementInfo::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructMeasurementInfo *rv = new CIccStructMeasurementInfo(pTagStruct);

  return rv;
}




static SIccElemNameSig g_IccStructNamedColorMbrTable[] = {
  {icSigNmclBrdfColorimetricMbr,       "nmclBrdfColorimetricMbr"},
  {icSigNmclBrdfColorimetricParamsMbr, "nmclBrdfColorimetricParamsMbr"},
  {icSigNmclBrdfSpectralMbr,           "nmclBrdfSpectralMbr"},
  {icSigNmclBrdfSpectralParamsMbr,     "nmclBrdfSpectralParamsMbr"},
  {icSigNmclDeviceDataMbr,             "nmclDeviceDataMbr"},
  {icSigNmclLocalizedNameMbr,          "nmclLocalizedNameMbr"},
  {icSigNmclNameMbr,                   "nmclNameNmClrMbr"},
  {icSigNmclNormalMapMbr,              "nmclNormalMapNmClrMbr"},
  {icSigNmclPcsDataMbr,                "nmclPcsDataMbr"},
  {icSigNmclSpectralDataMbr,           "nmclSpectralDataMbr"},
  {icSigNmclSpectralOverBlackMbr,      "nmclSpectralOverBlackMbr"},
  {icSigNmclSpectralOverGrayMbr,       "nmclSpectralOverGrayMbr"},
  {icSigNmclTintMbr,                   "nmclTintMbr"},
  {0, NULL},
};

CIccStructNamedColor::CIccStructNamedColor(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructNamedColorMbrTable;
}


CIccStructNamedColor::~CIccStructNamedColor()
{

}


IIccStruct* CIccStructNamedColor::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructNamedColor *rv = new CIccStructNamedColor(pTagStruct);

  return rv;
}


std::string CIccStructNamedColor::getName() const
{
  CIccTag *pTag = m_pTagStruct->FindElem(icSigNmclNameMbr);

  if (pTag && pTag->GetType()==icSigUtf8TextType) {
    CIccTagUtf8Text *pText = (CIccTagUtf8Text*)pTag;

    return std::string((char*)pText->GetText());
  }

  return "";
}

CIccTagNumArray *CIccStructNamedColor::GetNumArray(icSignature sigElem) const
{
  CIccTag *pTag = GetElem(sigElem);
  if (pTag && pTag->IsNumArrayType())
    return (CIccTagNumArray*)pTag;

  return NULL;
}

bool CIccStructNamedColor::GetTint(icFloatNumber *dstColor,
                                  icFloatNumber tint,
                                  CIccTagNumArray *pZero,
                                  icSignature sigElem,
                                  icUInt32Number nSamples) const
{

  if (tint<0.0f)
    tint = 0.0f;
  else if (tint>1.0f)
    tint = 1.0f;

  CIccTagNumArray *pData;
  CIccTagNumArray *pTint;

  pData = GetNumArray(sigElem);
  pTint = GetNumArray(icSigNmclTintMbr);

  if (!pData || !nSamples)
    return false;

  icUInt32Number nEntries = pData->GetNumValues()/nSamples;
  if (nEntries<1)
    return false;

  bool bNoZero;
  bool bNeedZero;
  icFloatNumber pos;

  if (pTint) {
    if (pTint->GetNumValues()!=nEntries)
      return false;

    if (!pTint->ValuePos(pos, tint, bNoZero))
      return false;

    bNeedZero = (pos<0.0f);

    if (bNoZero)
      pos = (pos + 1.0f)/nEntries;
    else
      pos = pos / (nEntries-1.0f);
  }
  else {
    bNoZero = true;
    bNeedZero = true;
    pos = tint;
  }

  icFloatNumber vals[256];
  icFloatNumber *zeroVals=NULL;

  if (bNoZero) {
    if (!pZero || pZero->GetNumValues()!=nSamples)
      return false;

    if (bNeedZero) {
      if (nSamples<256)
        zeroVals = vals;
      else if (bNeedZero)
        zeroVals = new icFloatNumber[nSamples];

      pZero->GetValues(zeroVals, 0, nSamples);
    }
    else {
      zeroVals = vals;
    }
  }

  bool rv = pData->Interpolate(dstColor, pos, nSamples, zeroVals);

  if (zeroVals && zeroVals!=vals)
    delete [] zeroVals;

  return rv;
}


icValidateStatus CIccStructNamedColor::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/* = NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  if (m_pTagStruct) {
    if (m_pTagStruct->GetTagStructType()==icSigNamedColorStruct && !m_pTagStruct->FindElem(icSigNmclNameMbr)) {
      CIccInfo Info;
      std::string sSigPathName = Info.GetSigPathName(sigPath);

      rv = icValidateCriticalError;
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Missing required struct member(s).\n";
    }
  }

  return icMaxStatus(rv, CIccStructUnknown::Validate(sigPath, sReport, pProfile));
}


static SIccElemNameSig g_IccStructProfileInfoMbrTable[] = {
  {icSigPinfAttributesMbr, "pinfAttributesMbr"},
  {icSigPinfProfileDescMbr, "pinfProfileDescMbr"},
  {icSigPinfProfileIDMbr, "pinfProfileIDMbr"},
  {icSigPinfManufacturerDescMbr, "pinfManufacturerDescMbr"},
  {icSigPinfManufacturerSigMbr, "pinfManufacturerSigMbr"},
  {icSigPinfModelDescMbr, "pinfModelDescMbr"},
  {icSigPinfModelSigMbr, "oinfModelSigMbr"},
  {icSigPinfRenderTransformMbr, "pinfRenderTransformMbr"},
  {icSigPinfTechnologyMbr, "pinfTechnologyMbr"},
  { 0, NULL },
};

CIccStructProfileInfo::CIccStructProfileInfo(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructProfileInfoMbrTable;
}


CIccStructProfileInfo::~CIccStructProfileInfo()
{

}


IIccStruct* CIccStructProfileInfo::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructProfileInfo *rv = new CIccStructProfileInfo(pTagStruct);

  return rv;
}



static SIccElemNameSig g_IccStructTintZeroMbrTable[] = {
  { icSigTnt0DeviceDataMbr, "tnt0DeviceDataMbr" },
  { icSigTnt0PcsDataMbr, "tnt0PcsDataMbr" },
  { icSigTnt0SpectralDataMbr, "tnt0SpectralDataMbr" },
  { icSigTnt0SpectralOverBlackMbr, "tnt0SpectralOverBlackMbr" },
  { icSigTnt0SpectralOverGrayMbr, "tnt0SpectralOverGrayMbr" },
  { 0, NULL },
};

CIccStructTintZero::CIccStructTintZero(CIccTagStruct *pTagStruct)
{
  m_pTagStruct = pTagStruct;
  m_pElemNameSigTable = g_IccStructTintZeroMbrTable;
}


CIccStructTintZero::~CIccStructTintZero()
{

}


IIccStruct* CIccStructTintZero::NewCopy(CIccTagStruct *pTagStruct) const
{
  CIccStructTintZero *rv = new CIccStructTintZero(pTagStruct);

  return rv;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
