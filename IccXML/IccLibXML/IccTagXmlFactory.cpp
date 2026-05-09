/** @file
File:       IccTagXMLFactory.cpp

Contains:   Implementation of the CIccTag class and creation factories

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

#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include <new>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccTag* CIccTagXmlFactory::CreateTag(icTagTypeSignature tagSig)
{
  switch(tagSig) {
  case icSigSignatureType:
    return new(std::nothrow) CIccTagXmlSignature;

  case icSigTextType:
    return new(std::nothrow) CIccTagXmlText;

  case icSigXYZArrayType:
    return new(std::nothrow) CIccTagXmlXYZ;

  case icSigCicpType:
    return new(std::nothrow) CIccTagXmlCicp;

  case icSigUInt8ArrayType:
    return new(std::nothrow) CIccTagXmlUInt8;

  case icSigUInt16ArrayType:
    return new(std::nothrow) CIccTagXmlUInt16;

  case icSigUInt32ArrayType:
    return new(std::nothrow) CIccTagXmlUInt32;

  case icSigUInt64ArrayType:
    return new(std::nothrow) CIccTagXmlUInt64;

  case icSigS15Fixed16ArrayType:
    return new(std::nothrow) CIccTagXmlS15Fixed16;

  case icSigU16Fixed16ArrayType:
    return new(std::nothrow) CIccTagXmlU16Fixed16;

  case icSigFloat16ArrayType:
    return new(std::nothrow) CIccTagXmlFloat16;

  case icSigFloat32ArrayType:
    return new(std::nothrow) CIccTagXmlFloat32;

  case icSigFloat64ArrayType:
    return new(std::nothrow) CIccTagXmlFloat64;

  case icSigGamutBoundaryDescType:
    return new(std::nothrow) CIccTagXmlGamutBoundaryDesc;

  case icSigCurveType:
    return new(std::nothrow) CIccTagXmlCurve;

  case icSigSegmentedCurveType:
    return new(std::nothrow) CIccTagXmlSegmentedCurve;

  case icSigMeasurementType:
    return new(std::nothrow) CIccTagXmlMeasurement;

  case icSigMultiLocalizedUnicodeType:
    return new(std::nothrow) CIccTagXmlMultiLocalizedUnicode;

  case icSigMultiProcessElementType:
    return new(std::nothrow) CIccTagXmlMultiProcessElement;

  case icSigParametricCurveType:
    return new(std::nothrow) CIccTagXmlParametricCurve;

  case icSigLutAtoBType:
    return new(std::nothrow) CIccTagXmlLutAtoB;

  case icSigLutBtoAType:
    return new(std::nothrow) CIccTagXmlLutBtoA;

  case icSigLut16Type:
    return new(std::nothrow) CIccTagXmlLut16;

  case icSigLut8Type:
    return new(std::nothrow) CIccTagXmlLut8;

  case icSigTextDescriptionType:
    return new(std::nothrow) CIccTagXmlTextDescription;

  case icSigNamedColor2Type:
    return new(std::nothrow) CIccTagXmlNamedColor2;

  case icSigChromaticityType:
    return new(std::nothrow) CIccTagXmlChromaticity;

  case icSigDataType:
    return new(std::nothrow) CIccTagXmlTagData;

  case icSigDateTimeType:
    return new(std::nothrow) CIccTagXmlDateTime;

  case icSigColorantOrderType:
    return new(std::nothrow) CIccTagXmlColorantOrder;

  case icSigColorantTableType:
    return new(std::nothrow) CIccTagXmlColorantTable;

  case icSigSparseMatrixArrayType:
    return new(std::nothrow) CIccTagXmlSparseMatrixArray;

  case icSigViewingConditionsType:
    return new(std::nothrow) CIccTagXmlViewingConditions;

  case icSigSpectralViewingConditionsType:
    return new(std::nothrow) CIccTagXmlSpectralViewingConditions;

  case icSigSpectralDataInfoType:
    return new(std::nothrow) CIccTagXmlSpectralDataInfo;

  case icSigProfileSequenceDescType:
    return new(std::nothrow) CIccTagXmlProfileSeqDesc;

  case icSigResponseCurveSet16Type:
    return new(std::nothrow) CIccTagXmlResponseCurveSet16;

  case icSigProfileSequceIdType:
    return new(std::nothrow) CIccTagXmlProfileSequenceId;

  case icSigDictType:
    return new(std::nothrow) CIccTagXmlDict;

  case icSigTagStructType:
    return new(std::nothrow) CIccTagXmlStruct;

  case icSigTagArrayType:
    return new(std::nothrow) CIccTagXmlArray;

  case icSigUtf8TextType:
    return new(std::nothrow) CIccTagXmlUtf8Text;

  case icSigZipUtf8TextType:
    return new(std::nothrow) CIccTagXmlZipUtf8Text;

  case icSigZipXmlType:
    return new(std::nothrow) CIccTagXmlZipXml;

  case icSigUtf16TextType:
    return new(std::nothrow) CIccTagXmlUtf16Text;

  case icSigEmbeddedProfileType:
    return new(std::nothrow) CIccTagXmlEmbeddedProfile;

  case icSigEmbeddedHeightImageType:
    return new(std::nothrow) CIccTagXmlEmbeddedHeightImage;

  case icSigEmbeddedNormalImageType:
    return new(std::nothrow) CIccTagXmlEmbeddedNormalImage;

  case icSigScreeningType:
  case icSigUcrBgType:
  case icSigCrdInfoType:

  default:
    return new(std::nothrow) CIccTagXmlUnknown(tagSig);
  }
}

const icChar* CIccTagXmlFactory::GetTagSigName(icTagSignature /*tagSig*/)
{
  return NULL;
}

icTagSignature CIccTagXmlFactory::GetTagNameSig(const icChar * /*szName*/)
{
  return icSigUnknownTag;
}

const icChar* CIccTagXmlFactory::GetTagTypeSigName(icTagTypeSignature /*tagSig*/)
{
  return NULL;
}

icTagTypeSignature CIccTagXmlFactory::GetTagTypeNameSig(const icChar * /*szName*/)
{
  return icSigUnknownType;
}

#ifdef USEICCDEVNAMESPACE
}
#endif
