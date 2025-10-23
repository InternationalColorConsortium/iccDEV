/** @file
    File:       IccTagFactory.cpp

    Contains:   Implementation of the CIccTag class and creation factories

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
// -Oct 30, 2005
// Added CICCTag Creation using factory support
//
//////////////////////////////////////////////////////////////////////

#include "IccTag.h"
#include "IccTagFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include <map>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

typedef std::map<icTagSignature, std::string> icTagSigToNameMap;
static icTagSigToNameMap g_TagSigToNameMap;

typedef std::map<std::string, icTagSignature> icTagNameToSigMap;
static icTagNameToSigMap g_TagNameToSigMap;

struct {
  icTagSignature sig;
  const icChar *szName;
} g_icTagNameTable[] = {
  {icSigAToB0Tag, "AToB0Tag"},
  {icSigAToB1Tag, "AToB1Tag"},
  {icSigAToB2Tag, "AToB2Tag"},
  {icSigAToB3Tag, "AToB3Tag"},
  {icSigAToM0Tag, "AToM0Tag"},
  {icSigBlueColorantTag, "blueColorantTag"},
  {icSigBlueTRCTag, "blueTRCTag"},
  {icSigBrdfColorimetricParameter0Tag, "brdfColorimetricParameter0Tag"},
  {icSigBrdfColorimetricParameter1Tag, "brdfColorimetricParameter1Tag"},
  {icSigBrdfColorimetricParameter2Tag, "brdfColorimetricParameter2Tag"},
  {icSigBrdfColorimetricParameter3Tag, "brdfColorimetricParameter3Tag"},
  {icSigBrdfSpectralParameter0Tag, "brdfSpectralParameter0Tag"},
  {icSigBrdfSpectralParameter1Tag, "brdfSpectralParameter1Tag"},
  {icSigBrdfSpectralParameter2Tag, "brdfSpectralParameter2Tag"},
  {icSigBrdfSpectralParameter3Tag, "brdfSpectralParameter3Tag"},
  {icSigBRDFMToB0Tag, "brdfMToB0Tag"},
  {icSigBRDFMToB1Tag, "brdfMToB1Tag"},
  {icSigBRDFMToB2Tag, "brdfMToB2Tag"},
  {icSigBRDFMToB3Tag, "brdfMToB3Tag"},
  {icSigBRDFMToS0Tag, "brdfMToS0Tag"},
  {icSigBRDFMToS1Tag, "brdfMToS1Tag"},
  {icSigBRDFMToS2Tag, "brdfMToS2Tag"},
  {icSigBRDFMToS3Tag, "brdfMToS3Tag"},
  {icSigBRDFAToB0Tag, "BRDFAToB0Tag"},
  {icSigBRDFAToB1Tag, "BRDFAToB1Tag"},
  {icSigBRDFAToB2Tag, "BRDFAToB2Tag"},
  {icSigBRDFAToB3Tag, "BRDFAToB3Tag"},
  {icSigBRDFDToB0Tag, "BRDFDToB0Tag"},
  {icSigBRDFDToB1Tag, "BRDFDToB1Tag"},
  {icSigBRDFDToB2Tag, "BRDFDToB2Tag"},
  {icSigBRDFDToB3Tag, "BRDFDToB3Tag"},
  {icSigSurfaceMapTag, "surfaceMapTag"},
  {icSigBToA0Tag, "BToA0Tag"},
  {icSigBToA1Tag, "BToA1Tag"},
  {icSigBToA2Tag, "BToA2Tag"},
  {icSigBToA3Tag, "BToA3Tag"},
  {icSigBToD0Tag, "BToD0Tag"},
  {icSigBToD1Tag, "BToD1Tag"},
  {icSigBToD2Tag, "BToD2Tag"},
  {icSigBToD3Tag, "BToD3Tag"},
  {icSigCalibrationDateTimeTag, "calibrationDateTimeTag"},
  {icSigCharTargetTag, "charTargetTag"},
  {icSigChromaticityTag, "chromaticityTag"},
  {icSigCicpTag, "cicpTag"},
  {icSigCopyrightTag, "copyrightTag"},
  {icSigCrdInfoTag, "crdInfoTag"},
  {icSigDataTag, "dataTag"},
  {icSigDateTimeTag, "dateTimeTag"},
  {icSigDeviceMfgDescTag, "deviceMfgDescTag"},
  {icSigDeviceModelDescTag, "deviceModelDescTag"},
  {icSigMetaDataTag, "metaDataTag"},
  {icSigDToB0Tag, "DToB0Tag"},
  {icSigDToB1Tag, "DToB1Tag"},
  {icSigDToB2Tag, "DToB2Tag"},
  {icSigDToB3Tag, "DToB3Tag"},
  {icSigGamutTag, "gamutTag"},
  {icSigGamutBoundaryDescription0Tag, "gamutBoundaryDescription0Tag"},
  {icSigGamutBoundaryDescription1Tag, "gamutBoundaryDescription1Tag"},
  {icSigGamutBoundaryDescription2Tag, "gamutBoundaryDescription2Tag"},
  {icSigGamutBoundaryDescription3Tag, "gamutBoundaryDescription3Tag"},
  {icSigGrayTRCTag, "grayTRCTag"},
  {icSigGreenColorantTag, "greenColorantTag"},
  {icSigGreenTRCTag, "greenTRCTag"},
  {icSigHToS0Tag, "HToS0Tag"},
  {icSigHToS1Tag, "HToS1Tag"},
  {icSigHToS2Tag, "HTSB2Tag"},
  {icSigHToS3Tag, "HToS3Tag"},
  {icSigLuminanceTag, "luminanceTag"},
  {icSigMeasurementTag, "measurementTag"},
  {icSigMediaBlackPointTag, "mediaBlackPointTag"},
  {icSigMediaWhitePointTag, "mediaWhitePointTag"},
  {icSigMToA0Tag, "MToA0Tag"},
  {icSigMToB0Tag, "MToB0Tag"},
  {icSigMToB1Tag, "MToB1Tag"},
  {icSigMToB2Tag, "MToB2Tag"},
  {icSigMToB3Tag, "MToB3Tag"},
  {icSigMToS0Tag, "MToS0Tag"},
  {icSigMToS1Tag, "MToS1Tag"},
  {icSigMToS2Tag, "MToS2Tag"},
  {icSigMToS3Tag, "MToS3Tag"},
  {icSigNamedColor2Tag, "namedColor2Tag"},
  {icSigPreview0Tag, "preview0Tag"},
  {icSigPreview1Tag, "preview1Tag"},
  {icSigPreview2Tag, "preview2Tag"},
  {icSigPrintConditionTag, "printConditionTag"},
  {icSigProfileDescriptionTag, "profileDescriptionTag"},
  {icSigProfileSequenceDescTag, "profileSequenceDescTag"},
  {icSigProfileSequceIdTag, "profileSequenceIdentifierTag"},
  {icSigPs2CRD0Tag, "ps2CRD0Tag"},
  {icSigPs2CRD1Tag, "ps2CRD1Tag"},
  {icSigPs2CRD2Tag, "ps2CRD2Tag"},
  {icSigPs2CRD3Tag, "ps2CRD3Tag"},
  {icSigPs2CSATag, "ps2CSATag"},
  {icSigPs2RenderingIntentTag, "ps2RenderingIntentTag"},
  {icSigRedColorantTag, "redColorantTag"},
  {icSigRedTRCTag, "redTRCTag"},
  {icSigScreeningDescTag, "screeningDescTag"},
  {icSigScreeningTag, "screeningTag"},
  {icSigTechnologyTag, "technologyTag"},
  {icSigUcrBgTag, "ucrBgTag"},
  {icSigViewingCondDescTag, "viewingCondDescTag"},
  {icSigViewingConditionsTag, "viewingConditionsTag"},
  {icSigSpectralViewingConditionsTag, "spectralViewingConditionsTag"},
  {icSigColorantInfoTag, "colorantInfoTag" },
  {icSigColorantInfoOutTag, "colorantInfoOutTag" },
  {icSigColorantOrderTag, "colorantOrderTag" },
  {icSigColorantOrderOutTag, "colorantOrderOutTag"},
  {icSigColorantTableTag, "colorantTableTag"},
  {icSigChromaticAdaptationTag, "chromaticAdaptationTag"},
  {icSigColorantTableOutTag, "colorantTableOutTag"},
  {icSigOutputResponseTag, "outputResponseTag"},
  {icSigPerceptualRenderingIntentGamutTag, "perceptualRenderingIntentGamutTag"},
  {icSigSaturationRenderingIntentGamutTag, "saturationRenderingIntentGamutTag"},
  {icSigCxFTag, "CxfTag"},
  {icSigSpectralDataInfoTag, "spectralDataInfoTag"},
  {icSigSpectralWhitePointTag, "spectralWhitePointTag"},
  {icSigCustomToStandardPccTag, "customToStandardPccTag"},
  {icSigStandardToCustomPccTag, "standardToCustomPccTag"},
  {icSigColorEncodingParamsTag, "colorEncodingParamsTag"},
  {icSigColorSpaceNameTag, "colorSpaceNameTag"},
  {icSigReferenceNameTag, "referenceNameTag"},
  {icSigMaterialTypeArrayTag, "materialTypeArrayTag"},
  {icSigMaterialDefaultValuesTag, "materialDefaultValuesTag"},
  {icSigEmbeddedV5ProfileTag, "embeddedV5ProfileTag"},
  {(icTagSignature)0,""},
};

typedef std::map<icTagTypeSignature, std::string> icTagTypeSigToNameMap;
static icTagTypeSigToNameMap g_TagTypeSigToNameMap;

typedef std::map<std::string, icTagTypeSignature> icTagTypeNameToSigMap;
static icTagTypeNameToSigMap g_TagTypeNameToSigMap;
static icTagTypeNameToSigMap g_AltTagTypeNameToSigMap;

struct {
  icTagTypeSignature sig;
  const icChar *szName;
} g_icTagTypeNameTable[] = {
  {icSigChromaticityType, "chromaticityType"},
  {icSigCicpType, "cicpType"},
  {icSigColorantOrderType, "colorantOrderType"},
  {icSigColorantTableType, "colorantTableType"},
  {icSigCrdInfoType, "crdInfoType"},
  {icSigCurveType, "curveType"},
  {icSigSegmentedCurveType, "segmentedCurveType"},
  {icSigDataType, "dataType"},
  {icSigDateTimeType, "dateTimeType"},
  {icSigDeviceSettingsType, "deviceSettingsType"},
  {icSigDictType, "dictType"},
  {icSigEmbeddedHeightImageType, "embeddedHeightImageType"},
  {icSigEmbeddedNormalImageType, "embeddedNormalImageType" },
  {icSigGamutBoundaryDescType, "gamutBoundaryDescType"},
  {icSigLut16Type, "lut16Type"},
  {icSigLut8Type, "lut8Type"},
  {icSigLutAtoBType, "lutAtoBType"},
  {icSigLutBtoAType, "lutBtoAType"},
  {icSigMeasurementType, "measurementType"},
  {icSigMultiLocalizedUnicodeType, "multiLocalizedUnicodeType"},
  {icSigMultiProcessElementType, "multiProcessElementType"},
  {icSigNamedColor2Type, "namedColor2Type"},
  {icSigParametricCurveType, "parametricCurveType"},
  {icSigResponseCurveSet16Type, "responseCurveSet16Type"},
  {icSigProfileSequenceDescType, "profileSequenceDescType"},
  {icSigS15Fixed16ArrayType, "s15Fixed16ArrayType"},
  {icSigScreeningType, "screeningType"},
  {icSigSignatureType, "signatureType"},
  {icSigSpectralDataInfoType, "spectralDataInfoType"},
  {icSigSpectralViewingConditionsType, "spectralViewingConditionsType"},
  {icSigTextType, "textType"},
  {icSigTextDescriptionType, "textDescriptionType"},
  {icSigU16Fixed16ArrayType, "u16Fixed16ArrayType"},
  {icSigUcrBgType, "ucrBgType"},
  {icSigSparseMatrixArrayType, "sparseMatrixArrayType"},
  {icSigUInt16ArrayType, "uInt16ArrayType"},
  {icSigUInt32ArrayType, "uInt32ArrayType"},
  {icSigUInt64ArrayType, "uInt64ArrayType"},
  {icSigUInt8ArrayType, "uInt8ArrayType"},
  {icSigFloat16ArrayType, "float16ArrayType"},
  {icSigFloat32ArrayType, "float32ArrayType"},
  {icSigFloat64ArrayType, "float64ArrayType"},
  {icSigUtf8TextType, "utf8Type"},
  {icSigZipUtf8TextType, "utf8ZipType"},
  {icSigZipXmlType, "zipXmlType"},
  {icSigZipXMLType, "ZIPXmlType"},
  {icSigUtf16TextType, "utf16Type"},
  {icSigTagArrayType, "tagArrayType"},
  {icSigTagStructType, "tagStructType"},
  {icSigViewingConditionsType, "viewingConditionsType"},
  {icSigXYZArrayType, "XYZArrayType"},
  {icSigProfileSequceIdType, "profileSequenceIdentifierType"},
  {icSigEmbeddedProfileType, "embeddedProfileType"},
  {(icTagTypeSignature)0,"" },
};

//Tag type names used in previous versions of these libraries
struct {
  icTagTypeSignature sig;
  const icChar *szName;
} g_icAltTagTypeNameTable[] = {
  { icSigFloat16ArrayType             , "float16NumberType" },
  { icSigFloat32ArrayType             , "float32NumberType" },
  { icSigFloat64ArrayType             , "float64NumberType" },
  { icSigS15Fixed16ArrayType          , "s15Fixed16NumberType" },
  { icSigU16Fixed16ArrayType          , "u16Fixed16NumberType" },
  { icSigUInt16ArrayType              , "uInt16NumberType" },
  { icSigUInt32ArrayType              , "uInt32NumberType" },
  { icSigUInt64ArrayType              , "uInt64NumberType" },
  { icSigUInt8ArrayType               , "uInt8NumberType" },
  { icSigUtf8TextType                 , "utf8TextType" },
  { icSigTagStructType                , "tagStructureType" },
  { icSigXYZArrayType                 , "XYZType" },
  { icSigZipUtf8TextType              , "zipUtf8TextType" },
  { (icTagTypeSignature)0,"" },
};

CIccTag* CIccSpecTagFactory::CreateTag(icTagTypeSignature tagSig)
{
  switch(tagSig) {
    case icSigSignatureType:
      return new CIccTagSignature;

    case icSigTextType:
      return new CIccTagText;

    case icSigXYZArrayType:
      return new CIccTagXYZ;

    case icSigCicpType:
      return new CIccTagCicp;

    case icSigUInt8ArrayType:
      return new CIccTagUInt8;

    case icSigUInt16ArrayType:
      return new CIccTagUInt16;

    case icSigUInt32ArrayType:
      return new CIccTagUInt32;

    case icSigUInt64ArrayType:
      return new CIccTagUInt64;

    case icSigS15Fixed16ArrayType:
      return new CIccTagS15Fixed16;

    case icSigU16Fixed16ArrayType:
      return new CIccTagU16Fixed16;

    case icSigFloat16ArrayType:
      return new CIccTagFloat16;

    case icSigFloat32ArrayType:
      return new CIccTagFloat32;

    case icSigFloat64ArrayType:
      return new CIccTagFloat64;

    case icSigGamutBoundaryDescType:
      return new CIccTagGamutBoundaryDesc;

    case icSigCurveType:
      return new CIccTagCurve;

    case icSigSegmentedCurveType:
      return new CIccTagSegmentedCurve;

    case icSigMeasurementType:
      return new CIccTagMeasurement;

    case icSigMultiLocalizedUnicodeType:
      return new CIccTagMultiLocalizedUnicode;

    case icSigMultiProcessElementType:
      return new CIccTagMultiProcessElement();

    case icSigParametricCurveType:
      return new CIccTagParametricCurve;

    case icSigLutAtoBType:
      return new CIccTagLutAtoB;

    case icSigLutBtoAType:
      return new CIccTagLutBtoA;

    case icSigLut16Type:
      return new CIccTagLut16;

    case icSigLut8Type:
      return new CIccTagLut8;

    case icSigTextDescriptionType:
      return new CIccTagTextDescription;

    case icSigNamedColor2Type:
      return new CIccTagNamedColor2;

    case icSigChromaticityType:
      return new CIccTagChromaticity;

    case icSigDataType:
      return new CIccTagData;

    case icSigDateTimeType:
      return new CIccTagDateTime;

#ifndef ICC_UNSUPPORTED_TAG_DICT
    case icSigDictType:
      return new CIccTagDict;
#endif

    case icSigColorantOrderType:
      return new CIccTagColorantOrder;

    case icSigColorantTableType:
      return new CIccTagColorantTable;

    case icSigSparseMatrixArrayType:
      return new CIccTagSparseMatrixArray;

    case icSigViewingConditionsType:
      return new CIccTagViewingConditions;

    case icSigSpectralDataInfoTag:
      return new CIccTagSpectralDataInfo;

    case icSigSpectralViewingConditionsTag:
      return new CIccTagSpectralViewingConditions;

    case icSigProfileSequenceDescType:
      return new CIccTagProfileSeqDesc;

    case icSigResponseCurveSet16Type:
      return new CIccTagResponseCurveSet16;

    case icSigProfileSequceIdType:
      return new CIccTagProfileSequenceId;

    case icSigUtf8TextType:
      return new CIccTagUtf8Text;

    case icSigZipUtf8TextType:
      return new CIccTagZipUtf8Text;

    case icSigZipXmlType:
    case icSigZipXMLType:
      return new CIccTagZipXml;

    case icSigUtf16TextType:
      return new CIccTagUtf16Text;

    case icSigTagArrayType:
      return new CIccTagArray;

    case icSigTagStructType:
      return new CIccTagStruct;

    case icSigEmbeddedProfileType:
      return new CIccTagEmbeddedProfile;

    case icSigEmbeddedHeightImageType:
      return new CIccTagEmbeddedHeightImage;

    case icSigEmbeddedNormalImageType:
      return new CIccTagEmbeddedNormalImage;

    case icSigScreeningType:
    case icSigUcrBgType:
    case icSigCrdInfoType:

    default:
      return new CIccTagUnknown;
  }
}

const icChar* CIccSpecTagFactory::GetTagSigName(icTagSignature tagSig)
{
  if (g_TagSigToNameMap.empty()) {
    for (int i = 0; g_icTagNameTable[i].sig; i++)
      g_TagSigToNameMap[g_icTagNameTable[i].sig] = std::string(g_icTagNameTable[i].szName);
  }
  icTagSigToNameMap::iterator sig = g_TagSigToNameMap.find(tagSig);
  if (sig != g_TagSigToNameMap.end())
    return sig->second.c_str();

  return NULL;
}

icTagSignature CIccSpecTagFactory::GetTagNameSig(const icChar *szName)
{
  if (g_TagSigToNameMap.empty()) {
    for (int i = 0; g_icTagNameTable[i].sig; i++)
      g_TagNameToSigMap[g_icTagNameTable[i].szName] = g_icTagNameTable[i].sig;
  }
  icTagNameToSigMap::iterator sig = g_TagNameToSigMap.find(szName);
  if (sig != g_TagNameToSigMap.end())
    return sig->second;

  return icSigUnknownTag;
}

const icChar* CIccSpecTagFactory::GetTagTypeSigName(icTagTypeSignature typeSig)
{
  if (g_TagTypeSigToNameMap.empty()) {
    for (int i = 0; g_icTagTypeNameTable[i].sig; i++)
      g_TagTypeSigToNameMap[g_icTagTypeNameTable[i].sig] = std::string(g_icTagTypeNameTable[i].szName);
  }
  icTagTypeSigToNameMap::iterator sig = g_TagTypeSigToNameMap.find(typeSig);
  if (sig != g_TagTypeSigToNameMap.end())
    return sig->second.c_str();

  return NULL;
}

icTagTypeSignature CIccSpecTagFactory::GetTagTypeNameSig(const icChar *szName)
{
  if (g_TagTypeNameToSigMap.empty()) {
    for (int i = 0; g_icTagTypeNameTable[i].sig; i++)
      g_TagTypeNameToSigMap[g_icTagTypeNameTable[i].szName] = g_icTagTypeNameTable[i].sig;
  }
  icTagTypeNameToSigMap::iterator sig = g_TagTypeNameToSigMap.find(szName);
  if (sig != g_TagTypeNameToSigMap.end())
    return sig->second;

  //Allow conversion from alternate names (backwards compatibility with earlier versions)
  if (g_AltTagTypeNameToSigMap.empty()) {
    for (int i = 0; g_icAltTagTypeNameTable[i].sig; i++)
      g_AltTagTypeNameToSigMap[g_icAltTagTypeNameTable[i].szName] = g_icAltTagTypeNameTable[i].sig;
  }
  sig = g_AltTagTypeNameToSigMap.find(szName);
  if (sig != g_AltTagTypeNameToSigMap.end())
    return sig->second;

  return icSigUnknownType;
}

std::unique_ptr<CIccTagCreator> CIccTagCreator::theTagCreator;

CIccTagCreator::~CIccTagCreator()
{
  IIccTagFactory *pFactory = DoPopFactory(true);

  while (pFactory) {
    delete pFactory;
    pFactory = DoPopFactory(true);
  }
}

CIccTagCreator* CIccTagCreator::GetInstance()
{
  if (!theTagCreator.get()) {
    theTagCreator = CIccTagCreatorPtr(new CIccTagCreator);

    theTagCreator->DoPushFactory(new CIccSpecTagFactory);
  }

  return theTagCreator.get();
}

CIccTag* CIccTagCreator::DoCreateTag(icTagTypeSignature tagTypeSig)
{
  CIccTagFactoryList::iterator i;
  CIccTag *rv = NULL;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->CreateTag(tagTypeSig);
    if (rv)
      break;
  }
  return rv;
}

const icChar* CIccTagCreator::DoGetTagSigName(icTagSignature tagSig)
{
  CIccTagFactoryList::iterator i;
  const icChar* rv;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->GetTagSigName(tagSig);
    if (rv)
      return rv;
  }

  return NULL;
}

icTagSignature CIccTagCreator::DoGetTagNameSig(const icChar *szName)
{
  CIccTagFactoryList::iterator i;
  icTagSignature rv;

  for (i = factoryStack.begin(); i != factoryStack.end(); i++) {
    rv = (*i)->GetTagNameSig(szName);
    if (rv!=icSigUnknownTag)
      return rv;
  }

  return icSigUnknownTag;
}


const icChar* CIccTagCreator::DoGetTagTypeSigName(icTagTypeSignature tagTypeSig)
{
  CIccTagFactoryList::iterator i;
  const icChar* rv;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->GetTagTypeSigName(tagTypeSig);
    if (rv)
      return rv;
  }

  return NULL;
}

icTagTypeSignature CIccTagCreator::DoGetTagTypeNameSig(const icChar* szName)
{
  CIccTagFactoryList::iterator i;
  icTagTypeSignature rv;

  for (i = factoryStack.begin(); i != factoryStack.end(); i++) {
    rv = (*i)->GetTagTypeNameSig(szName);
    if (rv!=icSigUnknownType)
      return rv;
  }

  return icSigUnknownType;
}

void CIccTagCreator::DoPushFactory(IIccTagFactory *pFactory)
{
  factoryStack.push_front(pFactory);
}

IIccTagFactory* CIccTagCreator::DoPopFactory(bool bAll /*=false*/)
{
//  int nNum = (bAll ? 0 : 1);

  if (factoryStack.size()>0) {
    CIccTagFactoryList::iterator i=factoryStack.begin();
    IIccTagFactory* rv = (*i);
    factoryStack.pop_front();
    return rv;
  }
  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
