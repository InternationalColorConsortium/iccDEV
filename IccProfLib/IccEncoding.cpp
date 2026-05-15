/*
File:       IccEncoding.cpp

Contains:   Implementation of Encoding profile class utilities

Version:    V1

Copyright:  see ICC Software License
*/

/*
* The ICC Software License, Version 0.2
*
*
* Copyright (c) 2003-2013 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 1-27-2013
//
//////////////////////////////////////////////////////////////////////

#include "IccEncoding.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccCAM.h"
#include "IccUtil.h"
#include <string>
#include <time.h>
#include <cstring>

class CIccDefaultEncProfileCacheHandler : public IIccEncProfileCacheHandler
{
public:
  CIccDefaultEncProfileCacheHandler() {}

  virtual CIccProfile *GetEncodingProfile(const icUChar *szColorSpaceName)
  {
    std::string file = "ISO22028-Encoded-";
    file += (char*)szColorSpaceName;
    file += ".icc";

    return OpenIccProfile(file.c_str());
  }
};

static IIccEncProfileCacheHandler* g_pEncProfileCacheHandler = NULL;

IIccEncProfileCacheHandler *IIccEncProfileCacheHandler::GetHandler()
{
  if (!g_pEncProfileCacheHandler) {
    g_pEncProfileCacheHandler = new CIccDefaultEncProfileCacheHandler();
  }
  return g_pEncProfileCacheHandler;
}

void IIccEncProfileCacheHandler::SetEncCacheHandler(IIccEncProfileCacheHandler *pHandler)
{
  if (pHandler) {
    if (g_pEncProfileCacheHandler) {
      delete g_pEncProfileCacheHandler;
    }
    g_pEncProfileCacheHandler = pHandler;
  }
}

class CIccDefaultEncProfileConverter : public IIccEncProfileConverter
{
public:
  CIccDefaultEncProfileConverter() {}

  virtual icStatusEncConvert ConvertFromParams(CIccProfilePtr &newIcc, CIccTagStruct *pParams, icHeader *pHeader);
};

#if 0
// currently unused
static icFloatNumber icGetParamFloatNum(CIccTagStruct *pParams, icColorEncodingParamsMemberSignature sig, icFloatNumber defaultValue=0)
{
  CIccTagFloat32 *pTag = (CIccTagFloat32*)pParams->FindElemOfType(sig, icSigFloat32ArrayType);

  if (!pTag || pTag->GetSize()<1)
    return defaultValue;

  return (*pTag)[0];
}
#endif

static void icYxy2XYZVector(icFloatNumber*XYZ, icFloatNumber Y, icFloatNumber *xy, icUInt8Number idxOffset=1)
{
  XYZ[0] = Y*xy[0] / xy[1];
  XYZ[idxOffset] = Y;
  XYZ[idxOffset<<1] = Y*(1.0f-xy[0]-xy[1]) / xy[1];
}

icStatusEncConvert CIccDefaultEncProfileConverter::ConvertFromParams(CIccProfilePtr& newIcc, CIccTagStruct* pParams, icHeader* pHeader)
{
  newIcc = NULL;

  if (!pParams || pParams->GetTagStructType() != icSigColorEncodingParamsSruct || !pHeader)
    return icEncConvertBadParams;

  CIccTagFloat32* pWhitePt = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptWhitePointChromaticityMbr, icSigFloat32ArrayType);
  CIccTagFloat32* pMediaWhitePt = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptMediumWhitePointChromaticityMbr, icSigFloat32ArrayType);

  if (!pWhitePt || pWhitePt->GetNumValues() < 2) {
    return icEncConvertBadParams;
  }

  if (!pMediaWhitePt)
    pMediaWhitePt = pWhitePt;

  if (!pMediaWhitePt || pMediaWhitePt->GetNumValues() < 2) {
    return icEncConvertBadParams;
  }

  CIccProfile* pIcc = new CIccProfile;
  pIcc->m_Header = *pHeader;

  struct tm* newtime;
  time_t long_time;

  time(&long_time);                /* Get time as long integer. */
  newtime = gmtime(&long_time);

  pIcc->m_Header.date.year = newtime->tm_year + 1900;
  pIcc->m_Header.date.month = newtime->tm_mon + 1;
  pIcc->m_Header.date.day = newtime->tm_mday;
  pIcc->m_Header.date.hours = newtime->tm_hour;
  pIcc->m_Header.date.minutes = newtime->tm_min;
  pIcc->m_Header.date.seconds = newtime->tm_sec;

  float XYZWhite[3];
  icYxy2XYZVector(XYZWhite, 1.0f, &(*pWhitePt)[0], 1);
  //Fill header from pParams
  pIcc->m_Header.pcs = icSigXYZPcsData;
  pIcc->m_Header.deviceClass = icSigDisplayClass;
  pIcc->m_Header.illuminant.X = icDtoF(XYZWhite[0]);
  pIcc->m_Header.illuminant.Y = icDtoF(XYZWhite[1]);
  pIcc->m_Header.illuminant.Z = icDtoF(XYZWhite[2]);

  //Fill Media White Point tag
  CIccTagXYZ* pXYZ = new CIccTagXYZ();
  float XYZMedia[3];
  icYxy2XYZVector(XYZMedia, 1.0f, &(*pMediaWhitePt)[0], 1);
  if (!pXYZ->SetSize(1)) {
    delete pIcc;
    return icEncConvertMemoryError;
  }

  (*pXYZ)[0].X = icDtoF(XYZMedia[0]);
  (*pXYZ)[0].Y = icDtoF(XYZMedia[1]);
  (*pXYZ)[0].Z = icDtoF(XYZMedia[2]);
  pIcc->AttachTag(icSigMediaWhitePointTag, pXYZ);

  //Create A2B tag from pParams
  CIccTagMultiProcessElement *pMpeTag = (CIccTagMultiProcessElement*)CIccTag::Create(icSigMultiProcessElementType);
  if (!pMpeTag) {
    delete pIcc;
    return icEncConvertMemoryError;
  }

  pMpeTag->SetChannels(3, 3);

  CIccMpeMatrix *pMtx;
  CIccTagFloat32 *pLumMtx = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptLumaChromaMatrixMbr, icSigFloat32ArrayType);
  icFloatNumber lumMtx[9];
  bool bHaveLumMtx = false;
  if (pLumMtx) {
    if (pLumMtx->GetSize()<9) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertBadParams;
    }
    pMtx = (CIccMpeMatrix*)CIccMultiProcessElement::Create(icSigMatrixElemType);
    if (!pMtx) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    if (!pMtx->SetSize(3, 3))
      return icEncConvertMemoryError;

    pLumMtx->GetValues(pMtx->GetMatrix(), 0, 9);
    pLumMtx->GetValues(&lumMtx[0], 0, 9);
    pMpeTag->Attach(pMtx);
    delete pLumMtx;
    bHaveLumMtx = true;
  }

  CIccTagSegmentedCurve *pSegCurve = (CIccTagSegmentedCurve*)pParams->FindElemOfType(icSigCeptTransferFunctionMbr, icSigSegmentedCurveType);
  if (pSegCurve && pSegCurve->GetCurve()) {
    CIccSegmentedCurve *pCurve = pSegCurve->GetCurve();
    CIccMpeCurveSet *pCurves = (CIccMpeCurveSet*)CIccMultiProcessElement::Create(icSigCurveSetElemType);
    if (!pCurves) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCurves->SetSize(3);
    pCurves->SetCurve(0, pCurve->NewCopy());
    pCurves->SetCurve(1, pCurve->NewCopy());
    pCurves->SetCurve(2, pCurve->NewCopy());
    pMpeTag->Attach(pCurves);
  }
  pMtx = (CIccMpeMatrix*)CIccMultiProcessElement::Create(icSigMatrixElemType);
  if (!pMtx) {
    delete pMpeTag;
    delete pIcc;
    return icEncConvertMemoryError;
  }

  CIccTagFloat32 *pxy;
  if (!pMtx->SetSize(3,3)) {
    delete pMtx;
    delete pMpeTag;
    delete pIcc;
    return icEncConvertMemoryError;
  }
  icFloatNumber *mtx=pMtx->GetMatrix();
  
  pxy = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptRedPrimaryXYZMbr, icSigFloat32ArrayType);
  if (!pxy || pxy->GetSize()<2) {
    delete pMpeTag;
    delete pIcc;
    return icEncConvertMemoryError;
  }
  mtx[0]=(*pxy)[0] * XYZMedia[0];
  mtx[3]=(*pxy)[1] * XYZMedia[1];
  mtx[6]=(1.0f - (*pxy)[0] - (*pxy)[1]) * XYZMedia[2];

  pxy = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptGreenPrimaryXYZMbr, icSigFloat32ArrayType);
  if (!pxy || pxy->GetSize()<2) {
    delete pMpeTag;
    delete pIcc;
    return icEncConvertMemoryError;
  }
  mtx[1]=(*pxy)[0] * XYZMedia[0];
  mtx[4]=(*pxy)[1] * XYZMedia[1];
  mtx[7]=(1.0f - (*pxy)[0] - (*pxy)[1]) * XYZMedia[2];

  pxy = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptBluePrimaryXYZMbr, icSigFloat32ArrayType);
  if (!pxy || pxy->GetSize()<2) {
    delete pMpeTag;
    delete pIcc;
    return icEncConvertMemoryError;
  }
  mtx[2]=(*pxy)[0] * XYZMedia[0];
  mtx[5]=(*pxy)[1] * XYZMedia[1];
  mtx[8]=(1.0f - (*pxy)[0] - (*pxy)[1]) * XYZMedia[2];

  CIccMpeMatrix *pMtx2 = (CIccMpeMatrix*)pMtx->NewCopy();
  pMpeTag->Attach(pMtx);

  pIcc->AttachTag(icSigAToB3Tag, pMpeTag);

  if (!icMatrixInvert3x3(pMtx2->GetMatrix())) {
    delete pMtx2;
    delete pIcc;
    return icEncConvertBadParams;
  }

  //Create B2A tag from pParams
  pMpeTag = (CIccTagMultiProcessElement*)CIccTag::Create(icSigMultiProcessElementType);
  if (!pMpeTag) {
    delete pMtx2;
    delete pIcc;
    return icEncConvertMemoryError;
  }

  pMpeTag->SetChannels(3, 3);

  pMpeTag->Attach(pMtx2);

  pSegCurve = (CIccTagSegmentedCurve*)pParams->FindElemOfType(icSigCeptInverseTransferFunctionMbr, icSigSegmentedCurveType);
  if (pSegCurve && pSegCurve->GetCurve()) {
    CIccSegmentedCurve *pCurve = pSegCurve->GetCurve();
    CIccMpeCurveSet *pCurves = (CIccMpeCurveSet*)CIccMultiProcessElement::Create(icSigCurveSetElemType);
    if (!pCurves || !pCurves->SetSize(3)) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCurves->SetCurve(0, pCurve->NewCopy());
    pCurves->SetCurve(1, pCurve->NewCopy());
    pCurves->SetCurve(2, pCurve->NewCopy());
    pMpeTag->Attach(pCurves);
  }

  if (bHaveLumMtx) {
    if (!icMatrixInvert3x3(&lumMtx[0])) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertBadParams;
    }

    pMtx = (CIccMpeMatrix*)CIccMultiProcessElement::Create(icSigMatrixElemType);
    if (!pMtx) {
      delete pMpeTag;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pMpeTag->Attach(pMtx);
  }
  pIcc->AttachTag(icSigBToA3Tag, pMpeTag);

  //Add ProfileConnectionConditions (if needed)
  if (pIcc->m_Header.illuminant.X!=icDtoF(icD50XYZ[0]) ||
      pIcc->m_Header.illuminant.Y!=icDtoF(icD50XYZ[1]) ||
      pIcc->m_Header.illuminant.Z!=icDtoF(icD50XYZ[2])) {

     //Set Spectral Viewing Conditions
    CIccTagSpectralViewingConditions *pCond = (CIccTagSpectralViewingConditions*)CIccTag::Create(icSigSpectralViewingConditionsType);
    if (!pCond) {
      delete pIcc;
      return icEncConvertMemoryError;
    }

    icFloatNumber illXYZ[3];
    icFloatNumber Lw = pParams->GetElemNumberValue(icSigCeptWhitePointLuminanceMbr, 100);
    pCond->setIlluminant(XYZWhite);
    //pCond->m_stdIlluminant = icIlluminantCustom;
    illXYZ[0] = pCond->m_illuminantXYZ.X = Lw * XYZWhite[0];
    illXYZ[1] = pCond->m_illuminantXYZ.Y = Lw * XYZWhite[1];
    illXYZ[2] = pCond->m_illuminantXYZ.Z = Lw * XYZWhite[2];

    icFloatNumber La = pParams->GetElemNumberValue(icSigCeptAmbientWhitePointLuminanceMbr, Lw/5.0f);
    CIccTagFloat32 *pSurround = (CIccTagFloat32*)pParams->FindElemOfType(icSigCeptAmbientWhitePointChromaticityMbr, icSigFloat32ArrayType);
    if (pSurround && pSurround->GetSize()>=2) {
      icFloatNumber XYZSurround[3];
      icYxy2XYZVector(XYZSurround, 1, &(*pSurround)[0], 1);
      pCond->m_surroundXYZ.X = La * XYZSurround[0];
      pCond->m_surroundXYZ.Y = La * XYZSurround[1];
      pCond->m_surroundXYZ.Z = La * XYZSurround[2];
    }
    else {
      pCond->m_surroundXYZ.X = La * XYZWhite[0];
      pCond->m_surroundXYZ.Y = La * XYZWhite[1];
      pCond->m_surroundXYZ.Z = La * XYZWhite[2];
    }

    pIcc->AttachTag(icSigSpectralViewingConditionsTag, pCond);

    icFloatNumber Lsw=pParams->GetElemNumberValue(icSigCeptViewingSurroundMbr, Lw/5.0f - 0.001f);
    CIccCamConverter *pCstmConvert = new CIccCamConverter();
    if (!pCstmConvert) {
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCstmConvert->SetParameter_WhitePoint(&illXYZ[0]);
    pCstmConvert->SetParameter_La(La);
    pCstmConvert->SetParameter_Yb(Lw);

    icFloatNumber SWr = Lsw / Lw;

    if (SWr>0.2) { //Average Surround
      pCstmConvert->SetParameter_C(0.69f);
      pCstmConvert->SetParameter_Nc(1.0f);
      pCstmConvert->SetParameter_F(1.0f);
    }
    else if (SWr>0.0) { //Dim Surround
      pCstmConvert->SetParameter_C(0.59f);
      pCstmConvert->SetParameter_Nc(0.95f);
      pCstmConvert->SetParameter_F(0.9f);
    }
    else { //Dark Surround
      pCstmConvert->SetParameter_C(0.525f);
      pCstmConvert->SetParameter_Nc(0.8f);
      pCstmConvert->SetParameter_F(0.8f);
    }

    CIccCamConverter *pCstmConvert2 = new CIccCamConverter();
    if (!pCstmConvert2) {
      delete pCstmConvert;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    *pCstmConvert2 = *pCstmConvert;

    //By default New CIccCamConverter objects are initialized with default PCS conditions
    CIccCamConverter *pStdConvert = new CIccCamConverter();
    if (!pStdConvert) {
      delete pCstmConvert;
      delete pCstmConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    CIccCamConverter *pStdConvert2 = new CIccCamConverter();
    if (!pStdConvert2) {
      delete pCstmConvert;
      delete pCstmConvert2;
      delete pStdConvert;
      delete pIcc;
      return icEncConvertMemoryError;
    }

    CIccMpeCAM *pCam;
    pMpeTag = (CIccTagMultiProcessElement*)CIccTag::Create(icSigMultiProcessElementType);
    if (!pMpeTag) {
      delete pCstmConvert;
      delete pStdConvert;
      delete pCstmConvert2;
      delete pStdConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pMpeTag->SetChannels(3, 3);

    pCam = (CIccMpeCAM*)CIccMultiProcessElement::Create(icSigXYZToJabElemType);
    if (!pCam) {
      delete pCstmConvert;
      delete pStdConvert;
      delete pCstmConvert2;
      delete pStdConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCam->SetCAM(pCstmConvert);
    pMpeTag->Attach(pCam);

    pCam = (CIccMpeCAM*)CIccMultiProcessElement::Create(icSigJabToXYZElemType);
    if (!pCam) {
      delete pStdConvert;
      delete pCstmConvert2;
      delete pStdConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCam->SetCAM(pStdConvert);
    pMpeTag->Attach(pCam);
    pIcc->AttachTag(icSigCustomToStandardPccTag, pMpeTag);

    pMpeTag = (CIccTagMultiProcessElement*)CIccTag::Create(icSigMultiProcessElementType);
    if (!pMpeTag) {
      delete pCstmConvert2;
      delete pStdConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pMpeTag->SetChannels(3,3);

    pCam = (CIccMpeCAM*)CIccMultiProcessElement::Create(icSigXYZToJabElemType);
    if (!pCam) {
      delete pCstmConvert2;
      delete pStdConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCam->SetCAM(pStdConvert2);
    pMpeTag->Attach(pCam);

    pCam = (CIccMpeCAM*)CIccMultiProcessElement::Create(icSigJabToXYZElemType);
    if (!pCam) {
      delete pCstmConvert2;
      delete pIcc;
      return icEncConvertMemoryError;
    }
    pCam->SetCAM(pCstmConvert2);
    pMpeTag->Attach(pCam);
    pIcc->AttachTag(icSigStandardToCustomPccTag, pMpeTag);
  }

#if 1 && defined(_DEBUG)
  SaveIccProfile("WEncConv.icc", pIcc);
#endif

  newIcc = pIcc;
  return icEncConvertOk;
}

static IIccEncProfileConverter* g_pEncProfileConverter = NULL;

IIccEncProfileConverter *IIccEncProfileConverter::GetHandler()
{
  if (!g_pEncProfileConverter) {
    g_pEncProfileConverter = new CIccDefaultEncProfileConverter();
  }
  return g_pEncProfileConverter;
}

void IIccEncProfileConverter::SetEncProfileConverter(IIccEncProfileConverter *pConverter)
{
  if (pConverter) {
    if (g_pEncProfileConverter) {
      delete g_pEncProfileConverter;
    }
    g_pEncProfileConverter = pConverter;
  }
}


icStatusEncConvert icConvertEncodingProfile(CIccProfilePtr &newIcc, CIccProfile *pEncodeIcc)
{
  newIcc = NULL;

  if (!pEncodeIcc || pEncodeIcc->m_Header.deviceClass!=icSigColorEncodingClass)
    return icEncConvertBadProfile;

  CIccTag *pTag;
  CIccTagStruct *pParams=NULL;
  pTag = pEncodeIcc->FindTagOfType(icSigReferenceNameTag, icSigUtf8TextType);
  if (pTag) {
    CIccTagUtf8Text *pText = (CIccTagUtf8Text*)pTag;
    const icUChar *szRefName = pText->GetText();
    if (!strcmp((const char*)szRefName, "ISO 22028-1")) {
      pTag = pEncodeIcc->FindTagOfType(icSigColorEncodingParamsTag, icSigTagStructType);
      if (pTag)
        pParams = (CIccTagStruct*)pTag->NewCopy();
    }
    else {
      CIccTag *pNameTag = pEncodeIcc->FindTagOfType(icSigColorSpaceNameTag, icSigUtf8TextType);
      if (pNameTag) {
        pText = (CIccTagUtf8Text *)pNameTag;
        const icUChar *szName = pText->GetText();

        CIccProfile *pBaseIcc = IIccEncProfileCacheHandler::GetHandler()->GetEncodingProfile(szName);
        if (!pBaseIcc) {
          return icEncConvertNoBaseProfile;
        }
        
        pTag = pBaseIcc->FindTagOfType(icSigColorEncodingParamsTag, icSigTagStructType);
        if (!pTag) {
           delete pBaseIcc;
           return icEncConvertBadBaseProfile;
        }
        pParams = (CIccTagStruct*)pTag->NewCopy();
        delete pBaseIcc;

        //Now Copy Overrides into pParams (if they exist in pEncodeIcc
        CIccTag *pOverridesTag = pEncodeIcc->FindTagOfType(icSigColorEncodingParamsTag, icSigTagStructType);
        if (pOverridesTag) {
          CIccTagStruct *pStruct = (CIccTagStruct*)pOverridesTag;
          TagEntryList *pTags = pStruct->GetElemList();
          if (pTags) {
            TagEntryList::iterator entry;
            for (entry=pTags->begin(); entry!=pTags->end(); entry++) {
              if (entry->pTag) {
                pStruct->DeleteElem(entry->TagInfo.sig);
                pStruct->AttachElem(entry->TagInfo.sig, entry->pTag->NewCopy());
              }
            }
          }
        }
      }
    }
  }

  if (!pParams) {
    return icEncConvertBadProfile;
  }

  //Create and initialize new profile from pEncodeIcc and pParams
  icStatusEncConvert stat = IIccEncProfileConverter::GetHandler()->ConvertFromParams(newIcc, pParams, &pEncodeIcc->m_Header);

  delete pParams;
 
  return stat;
}
