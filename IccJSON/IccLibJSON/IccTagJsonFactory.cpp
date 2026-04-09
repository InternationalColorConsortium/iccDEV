/** @file
File:       IccTagJsonFactory.cpp

Contains:   Implementation of the CIccTagJsonFactory class –
            creates JSON-aware tag objects for all ICC-specified types.

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

#include "IccTagJson.h"
#include "IccTagJsonFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccTag* CIccTagJsonFactory::CreateTag(icTagTypeSignature tagSig)
{
  switch (tagSig) {
  case icSigSignatureType:           return new CIccTagJsonSignature;
  case icSigTextType:                return new CIccTagJsonText;
  case icSigXYZArrayType:            return new CIccTagJsonXYZ;
  case icSigCicpType:                return new CIccTagJsonCicp;
  case icSigUInt8ArrayType:          return new CIccTagJsonUInt8;
  case icSigUInt16ArrayType:         return new CIccTagJsonUInt16;
  case icSigUInt32ArrayType:         return new CIccTagJsonUInt32;
  case icSigUInt64ArrayType:         return new CIccTagJsonUInt64;
  case icSigS15Fixed16ArrayType:     return new CIccTagJsonS15Fixed16;
  case icSigFloat16ArrayType:        return new CIccTagJsonFloat16;
  case icSigFloat32ArrayType:        return new CIccTagJsonFloat32;
  case icSigFloat64ArrayType:        return new CIccTagJsonFloat64;
  case icSigGamutBoundaryDescType:   return new CIccTagJsonGamutBoundaryDesc;
  case icSigCurveType:               return new CIccTagJsonCurve;
  case icSigSegmentedCurveType:      return new CIccTagJsonSegmentedCurve;
  case icSigMeasurementType:         return new CIccTagJsonMeasurement;
  case icSigMultiLocalizedUnicodeType: return new CIccTagJsonMultiLocalizedUnicode;
  case icSigMultiProcessElementType: return new CIccTagJsonMultiProcessElement;
  case icSigParametricCurveType:     return new CIccTagJsonParametricCurve;
  case icSigLutAtoBType:             return new CIccTagJsonLutAtoB;
  case icSigLutBtoAType:             return new CIccTagJsonLutBtoA;
  case icSigLut16Type:               return new CIccTagJsonLut16;
  case icSigLut8Type:                return new CIccTagJsonLut8;
  case icSigTextDescriptionType:     return new CIccTagJsonTextDescription;
  case icSigNamedColor2Type:         return new CIccTagJsonNamedColor2;
  case icSigChromaticityType:        return new CIccTagJsonChromaticity;
  case icSigDataType:                return new CIccTagJsonTagData;
  case icSigDateTimeType:            return new CIccTagJsonDateTime;
  case icSigColorantOrderType:       return new CIccTagJsonColorantOrder;
  case icSigColorantTableType:       return new CIccTagJsonColorantTable;
  case icSigSparseMatrixArrayType:   return new CIccTagJsonSparseMatrixArray;
  case icSigViewingConditionsType:   return new CIccTagJsonViewingConditions;
  case icSigSpectralViewingConditionsType: return new CIccTagJsonSpectralViewingConditions;
  case icSigSpectralDataInfoType:    return new CIccTagJsonSpectralDataInfo;
  case icSigProfileSequenceDescType: return new CIccTagJsonProfileSeqDesc;
  case icSigResponseCurveSet16Type:  return new CIccTagJsonResponseCurveSet16;
  case icSigProfileSequceIdType:     return new CIccTagJsonProfileSequenceId;
  case icSigDictType:                return new CIccTagJsonDict;
  case icSigTagStructType:           return new CIccTagJsonStruct;
  case icSigTagArrayType:            return new CIccTagJsonArray;
  case icSigUtf8TextType:            return new CIccTagJsonUtf8Text;
  case icSigZipUtf8TextType:         return new CIccTagJsonZipUtf8Text;
  case icSigZipXmlType:              return new CIccTagJsonZipXml;
  case icSigUtf16TextType:           return new CIccTagJsonUtf16Text;
  case icSigEmbeddedProfileType:     return new CIccTagJsonEmbeddedProfile;
  case icSigEmbeddedHeightImageType: return new CIccTagJsonEmbeddedHeightImage;
  case icSigEmbeddedNormalImageType: return new CIccTagJsonEmbeddedNormalImage;

  case icSigScreeningType:
  case icSigUcrBgType:
  case icSigCrdInfoType:
  default:
    return new CIccTagJsonUnknown(tagSig);
  }
}

const icChar* CIccTagJsonFactory::GetTagSigName(icTagSignature /*tagSig*/)
{
  return NULL;
}

icTagSignature CIccTagJsonFactory::GetTagNameSig(const icChar * /*szName*/)
{
  return icSigUnknownTag;
}

const icChar* CIccTagJsonFactory::GetTagTypeSigName(icTagTypeSignature /*tagTypeSig*/)
{
  return NULL;
}

icTagTypeSignature CIccTagJsonFactory::GetTagTypeNameSig(const icChar * /*szName*/)
{
  return icSigUnknownType;
}

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif
