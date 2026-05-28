/** @file
File:       IccTagJsonFactory.cpp

Contains:   Implementation of the CIccTagJsonFactory class --
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
  case icSigSignatureType:           return new (std::nothrow) CIccTagJsonSignature;
  case icSigTextType:                return new (std::nothrow) CIccTagJsonText;
  case icSigXYZArrayType:            return new (std::nothrow) CIccTagJsonXYZ;
  case icSigCicpType:                return new (std::nothrow) CIccTagJsonCicp;
  case icSigUInt8ArrayType:          return new (std::nothrow) CIccTagJsonUInt8;
  case icSigUInt16ArrayType:         return new (std::nothrow) CIccTagJsonUInt16;
  case icSigUInt32ArrayType:         return new (std::nothrow) CIccTagJsonUInt32;
  case icSigUInt64ArrayType:         return new (std::nothrow) CIccTagJsonUInt64;
  case icSigS15Fixed16ArrayType:     return new (std::nothrow) CIccTagJsonS15Fixed16;
  case icSigFloat16ArrayType:        return new (std::nothrow) CIccTagJsonFloat16;
  case icSigFloat32ArrayType:        return new (std::nothrow) CIccTagJsonFloat32;
  case icSigFloat64ArrayType:        return new (std::nothrow) CIccTagJsonFloat64;
  case icSigGamutBoundaryDescType:   return new (std::nothrow) CIccTagJsonGamutBoundaryDesc;
  case icSigCurveType:               return new (std::nothrow) CIccTagJsonCurve;
  case icSigSegmentedCurveType:      return new (std::nothrow) CIccTagJsonSegmentedCurve;
  case icSigMeasurementType:         return new (std::nothrow) CIccTagJsonMeasurement;
  case icSigMultiLocalizedUnicodeType: return new (std::nothrow) CIccTagJsonMultiLocalizedUnicode;
  case icSigMultiProcessElementType: return new (std::nothrow) CIccTagJsonMultiProcessElement;
  case icSigParametricCurveType:     return new (std::nothrow) CIccTagJsonParametricCurve;
  case icSigLutAtoBType:             return new (std::nothrow) CIccTagJsonLutAtoB;
  case icSigLutBtoAType:             return new (std::nothrow) CIccTagJsonLutBtoA;
  case icSigLut16Type:               return new (std::nothrow) CIccTagJsonLut16;
  case icSigLut8Type:                return new (std::nothrow) CIccTagJsonLut8;
  case icSigTextDescriptionType:     return new (std::nothrow) CIccTagJsonTextDescription;
  case icSigNamedColor2Type:         return new (std::nothrow) CIccTagJsonNamedColor2;
  case icSigChromaticityType:        return new (std::nothrow) CIccTagJsonChromaticity;
  case icSigDataType:                return new (std::nothrow) CIccTagJsonTagData;
  case icSigDateTimeType:            return new (std::nothrow) CIccTagJsonDateTime;
  case icSigColorantOrderType:       return new (std::nothrow) CIccTagJsonColorantOrder;
  case icSigColorantTableType:       return new (std::nothrow) CIccTagJsonColorantTable;
  case icSigSparseMatrixArrayType:   return new (std::nothrow) CIccTagJsonSparseMatrixArray;
  case icSigViewingConditionsType:   return new (std::nothrow) CIccTagJsonViewingConditions;
  case icSigSpectralViewingConditionsType: return new (std::nothrow) CIccTagJsonSpectralViewingConditions;
  case icSigSpectralDataInfoType:    return new (std::nothrow) CIccTagJsonSpectralDataInfo;
  case icSigProfileSequenceDescType: return new (std::nothrow) CIccTagJsonProfileSeqDesc;
  case icSigResponseCurveSet16Type:  return new (std::nothrow) CIccTagJsonResponseCurveSet16;
  case icSigProfileSequceIdType:     return new (std::nothrow) CIccTagJsonProfileSequenceId;
  case icSigDictType:                return new (std::nothrow) CIccTagJsonDict;
  case icSigTagStructType:           return new (std::nothrow) CIccTagJsonStruct;
  case icSigTagArrayType:            return new (std::nothrow) CIccTagJsonArray;
  case icSigUtf8TextType:            return new (std::nothrow) CIccTagJsonUtf8Text;
  case icSigZipUtf8TextType:         return new (std::nothrow) CIccTagJsonZipUtf8Text;
  case icSigZipXmlType:              return new (std::nothrow) CIccTagJsonZipXml;
  case icSigUtf16TextType:           return new (std::nothrow) CIccTagJsonUtf16Text;
  case icSigEmbeddedProfileType:     return new (std::nothrow) CIccTagJsonEmbeddedProfile;
  case icSigEmbeddedHeightImageType: return new (std::nothrow) CIccTagJsonEmbeddedHeightImage;
  case icSigEmbeddedNormalImageType: return new (std::nothrow) CIccTagJsonEmbeddedNormalImage;

  case icSigScreeningType:
  case icSigUcrBgType:
  case icSigCrdInfoType:
  default:
    return new (std::nothrow) CIccTagJsonUnknown(tagSig);
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
