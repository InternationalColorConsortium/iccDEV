/** @file
File:       IccTagJson.h

Contains:   Header for implementation of CIccTagJson class and
            creation factories

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

#ifndef _ICCTAGJSON_H
#define _ICCTAGJSON_H

#include "IccTag.h"
#include "IccTagMPE.h"
#include "IccJsonConfig.h"
#include <nlohmann/json.hpp>

using IccJson = nlohmann::ordered_json;

// ---------------------------------------------------------------------------
// Base extension interface
// ---------------------------------------------------------------------------
class CIccTagJson : public IIccExtensionTag
{
public:
  virtual ~CIccTagJson() {}

  virtual const char *GetExtClassName() const { return "CIccTagJson"; }
  virtual const char *GetExtDerivedClassName() const { return ""; }

  virtual bool ToJson(IccJson &j) = 0;
  virtual bool ParseJson(const IccJson &j, std::string &parseStr) = 0;
};

// ---------------------------------------------------------------------------
// Unknown tag
// ---------------------------------------------------------------------------
class CIccTagJsonUnknown : public CIccTagUnknown, public CIccTagJson
{
public:
  CIccTagJsonUnknown(icTagTypeSignature nType) { m_nType = nType; }
  virtual ~CIccTagJsonUnknown() {}

  virtual const char *GetClassName() const { return "CIccTagJsonUnknown"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Text tags
// ---------------------------------------------------------------------------
class CIccTagJsonText : public CIccTagText, public CIccTagJson
{
public:
  CIccTagJsonText() : CIccTagText() {}
  CIccTagJsonText(const CIccTagJsonText &t) : CIccTagText(t) {}
  virtual ~CIccTagJsonText() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonText(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonText"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonUtf8Text : public CIccTagUtf8Text, public CIccTagJson
{
public:
  CIccTagJsonUtf8Text() : CIccTagUtf8Text() {}
  CIccTagJsonUtf8Text(const CIccTagJsonUtf8Text &t) : CIccTagUtf8Text(t) {}
  virtual ~CIccTagJsonUtf8Text() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonUtf8Text(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonUtf8Text"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonZipUtf8Text : public CIccTagZipUtf8Text, public CIccTagJson
{
public:
  CIccTagJsonZipUtf8Text() : CIccTagZipUtf8Text() {}
  CIccTagJsonZipUtf8Text(const CIccTagJsonZipUtf8Text &t) : CIccTagZipUtf8Text(t) {}
  virtual ~CIccTagJsonZipUtf8Text() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonZipUtf8Text(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonZipUtf8Text"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonZipXml : public CIccTagZipXml, public CIccTagJson
{
public:
  CIccTagJsonZipXml() : CIccTagZipXml() {}
  CIccTagJsonZipXml(const CIccTagJsonZipXml &t) : CIccTagZipXml(t) {}
  virtual ~CIccTagJsonZipXml() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonZipXml(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonZipXml"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonUtf16Text : public CIccTagUtf16Text, public CIccTagJson
{
public:
  CIccTagJsonUtf16Text() : CIccTagUtf16Text() {}
  CIccTagJsonUtf16Text(const CIccTagJsonUtf16Text &t) : CIccTagUtf16Text(t) {}
  virtual ~CIccTagJsonUtf16Text() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonUtf16Text(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonUtf16Text"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonTextDescription : public CIccTagTextDescription, public CIccTagJson
{
public:
  CIccTagJsonTextDescription() : CIccTagTextDescription() {}
  CIccTagJsonTextDescription(const CIccTagJsonTextDescription &t) : CIccTagTextDescription(t) {}
  virtual ~CIccTagJsonTextDescription() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonTextDescription(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonTextDescription"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Signature / basic value tags
// ---------------------------------------------------------------------------
class CIccTagJsonSignature : public CIccTagSignature, public CIccTagJson
{
public:
  virtual ~CIccTagJsonSignature() {}
  virtual const char *GetClassName() const { return "CIccTagJsonSignature"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonDateTime : public CIccTagDateTime, public CIccTagJson
{
public:
  virtual ~CIccTagJsonDateTime() {}
  virtual const char *GetClassName() const { return "CIccTagJsonDateTime"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Colorimetric / measurement tags
// ---------------------------------------------------------------------------
class CIccTagJsonXYZ : public CIccTagXYZ, public CIccTagJson
{
public:
  virtual ~CIccTagJsonXYZ() {}
  virtual const char *GetClassName() const { return "CIccTagJsonXYZ"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonChromaticity : public CIccTagChromaticity, public CIccTagJson
{
public:
  virtual ~CIccTagJsonChromaticity() {}
  virtual const char *GetClassName() const { return "CIccTagJsonChromaticity"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonCicp : public CIccTagCicp, public CIccTagJson
{
public:
  virtual ~CIccTagJsonCicp() {}
  virtual const char *GetClassName() const { return "CIccTagJsonCicp"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonMeasurement : public CIccTagMeasurement, public CIccTagJson
{
public:
  virtual ~CIccTagJsonMeasurement() {}
  virtual const char *GetClassName() const { return "CIccTagJsonMeasurement"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonViewingConditions : public CIccTagViewingConditions, public CIccTagJson
{
public:
  virtual ~CIccTagJsonViewingConditions() {}
  virtual const char *GetClassName() const { return "CIccTagJsonViewingConditions"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonSpectralDataInfo : public CIccTagSpectralDataInfo, public CIccTagJson
{
public:
  virtual ~CIccTagJsonSpectralDataInfo() {}
  virtual const char *GetClassName() const { return "CIccTagJsonSpectralDataInfo"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonSpectralViewingConditions : public CIccTagSpectralViewingConditions, public CIccTagJson
{
public:
  virtual ~CIccTagJsonSpectralViewingConditions() {}
  virtual const char *GetClassName() const { return "CIccTagJsonSpectralViewingConditions"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Named / colorant tags
// ---------------------------------------------------------------------------
class CIccTagJsonNamedColor2 : public CIccTagNamedColor2, public CIccTagJson
{
public:
  virtual ~CIccTagJsonNamedColor2() {}
  virtual const char *GetClassName() const { return "CIccTagJsonNamedColor2"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonColorantOrder : public CIccTagColorantOrder, public CIccTagJson
{
public:
  virtual ~CIccTagJsonColorantOrder() {}
  virtual const char *GetClassName() const { return "CIccTagJsonColorantOrder"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonColorantTable : public CIccTagColorantTable, public CIccTagJson
{
public:
  virtual ~CIccTagJsonColorantTable() {}
  virtual const char *GetClassName() const { return "CIccTagJsonColorantTable"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Numeric array tags
// ---------------------------------------------------------------------------
class CIccTagJsonSparseMatrixArray : public CIccTagSparseMatrixArray, public CIccTagJson
{
public:
  virtual ~CIccTagJsonSparseMatrixArray() {}
  virtual const char *GetClassName() const { return "CIccTagJsonSparseMatrixArray"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

template <class T, icTagTypeSignature Tsig>
class CIccTagJsonFixedNum : public CIccTagFixedNum<T, Tsig>, public CIccTagJson
{
public:
  virtual ~CIccTagJsonFixedNum() {}
  virtual const char *GetClassName() const;
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

typedef CIccTagJsonFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType> CIccTagJsonS15Fixed16;
typedef CIccTagFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType>      CIccTagJsonU16Fixed16;

// Forward declaration for array helper used by CIccTagJsonNum
template <class T, icTagTypeSignature Tsig> class CIccJsonArrayType;

template <class T, class A, icTagTypeSignature Tsig>
class CIccTagJsonNum : public CIccTagNum<T, Tsig>, public CIccTagJson
{
public:
  virtual ~CIccTagJsonNum() {}
  virtual const char *GetClassName() const;
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

typedef CIccTagJsonNum<icUInt8Number,  CIccJsonArrayType<icUInt8Number,  icSigUInt8ArrayType>,  icSigUInt8ArrayType>  CIccTagJsonUInt8;
typedef CIccTagJsonNum<icUInt16Number, CIccJsonArrayType<icUInt16Number, icSigUInt16ArrayType>, icSigUInt16ArrayType> CIccTagJsonUInt16;
typedef CIccTagJsonNum<icUInt32Number, CIccJsonArrayType<icUInt32Number, icSigUInt32ArrayType>, icSigUInt32ArrayType> CIccTagJsonUInt32;
typedef CIccTagJsonNum<icUInt64Number, CIccJsonArrayType<icUInt64Number, icSigUInt64ArrayType>, icSigUInt64ArrayType> CIccTagJsonUInt64;

template <class T, class A, icTagTypeSignature Tsig>
class CIccTagJsonFloatNum : public CIccTagFloatNum<T, Tsig>, public CIccTagJson
{
public:
  virtual ~CIccTagJsonFloatNum() {}
  virtual const char *GetClassName() const;
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

typedef CIccTagJsonFloatNum<icFloat32Number, CIccJsonArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat16ArrayType> CIccTagJsonFloat16;
typedef CIccTagJsonFloatNum<icFloat32Number, CIccJsonArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat32ArrayType> CIccTagJsonFloat32;
typedef CIccTagJsonFloatNum<icFloat64Number, CIccJsonArrayType<icFloat64Number, icSigFloat64ArrayType>, icSigFloat64ArrayType> CIccTagJsonFloat64;

// ---------------------------------------------------------------------------
// Sequence / description tags
// ---------------------------------------------------------------------------
class CIccTagJsonMultiLocalizedUnicode : public CIccTagMultiLocalizedUnicode, public CIccTagJson
{
public:
  CIccTagJsonMultiLocalizedUnicode() : CIccTagMultiLocalizedUnicode() {}
  CIccTagJsonMultiLocalizedUnicode(const CIccTagJsonMultiLocalizedUnicode &t) : CIccTagMultiLocalizedUnicode(t) {}
  virtual ~CIccTagJsonMultiLocalizedUnicode() {}

  virtual CIccTag* NewCopy() const { return new CIccTagJsonMultiLocalizedUnicode(*this); }
  virtual const char *GetClassName() const { return "CIccTagJsonMultiLocalizedUnicode"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonProfileSeqDesc : public CIccTagProfileSeqDesc, public CIccTagJson
{
public:
  virtual ~CIccTagJsonProfileSeqDesc() {}
  virtual const char *GetClassName() const { return "CIccTagJsonProfileSeqDesc"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonProfileSequenceId : public CIccTagProfileSequenceId, public CIccTagJson
{
public:
  virtual ~CIccTagJsonProfileSequenceId() {}
  virtual const char *GetClassName() const { return "CIccTagJsonProfileSequenceId"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Data tag
// ---------------------------------------------------------------------------
class CIccTagJsonTagData : public CIccTagData, public CIccTagJson
{
public:
  virtual ~CIccTagJsonTagData() {}
  virtual const char *GetClassName() const { return "CIccTagJsonTagData"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Response curve
// ---------------------------------------------------------------------------
class CIccTagJsonResponseCurveSet16 : public CIccTagResponseCurveSet16, public CIccTagJson
{
public:
  virtual ~CIccTagJsonResponseCurveSet16() {}
  virtual const char *GetClassName() const { return "CIccTagJsonResponseCurveSet16"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Curve tags -- base interface with convert-type overloads (mirrors CIccCurveXml)
// ---------------------------------------------------------------------------
class CIccCurveJson : public CIccTagJson
{
public:
  virtual ~CIccCurveJson() {}
  virtual const char *GetExtDerivedClassName() const { return "CIccCurveJson"; }

  virtual bool ToJson(IccJson &j) = 0;
  virtual bool ParseJson(const IccJson &j, std::string &parseStr) = 0;
  virtual bool ToJson(IccJson &j, icConvertType nType) = 0;
  virtual bool ParseJson(const IccJson &j, icConvertType nType, std::string &parseStr) = 0;
};

class CIccTagJsonCurve : public CIccTagCurve, public CIccCurveJson
{
public:
  virtual ~CIccTagJsonCurve() {}
  virtual const char *GetClassName() const { return "CIccTagJsonCurve"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ToJson(IccJson &j, icConvertType nType);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
  virtual bool ParseJson(const IccJson &j, icConvertType nType, std::string &parseStr);
};

class CIccTagJsonParametricCurve : public CIccTagParametricCurve, public CIccCurveJson
{
public:
  virtual ~CIccTagJsonParametricCurve() {}
  virtual const char *GetClassName() const { return "CIccTagJsonParametricCurve"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ToJson(IccJson &j, icConvertType nType);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
  virtual bool ParseJson(const IccJson &j, icConvertType nType, std::string &parseStr);
};

class CIccTagJsonSegmentedCurve : public CIccTagSegmentedCurve, public CIccCurveJson
{
public:
  virtual ~CIccTagJsonSegmentedCurve() {}
  virtual const char *GetClassName() const { return "CIccTagJsonSegmentedCurve"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ToJson(IccJson &j, icConvertType nType);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
  virtual bool ParseJson(const IccJson &j, icConvertType nType, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// LUT tags
// ---------------------------------------------------------------------------
class CIccTagJsonLutAtoB : public CIccTagLutAtoB, public CIccTagJson
{
public:
  virtual ~CIccTagJsonLutAtoB() {}
  virtual const char *GetClassName() const { return "CIccTagJsonLutAtoB"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonLutBtoA : public CIccTagLutBtoA, public CIccTagJson
{
public:
  virtual ~CIccTagJsonLutBtoA() {}
  virtual const char *GetClassName() const { return "CIccTagJsonLutBtoA"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonLut8 : public CIccTagLut8, public CIccTagJson
{
public:
  virtual ~CIccTagJsonLut8() {}
  virtual const char *GetClassName() const { return "CIccTagJsonLut8"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonLut16 : public CIccTagLut16, public CIccTagJson
{
public:
  virtual ~CIccTagJsonLut16() {}
  virtual const char *GetClassName() const { return "CIccTagJsonLut16"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Multi-process element tag
// ---------------------------------------------------------------------------
class CIccTagJsonMultiProcessElement : public CIccTagMultiProcessElement, public CIccTagJson
{
public:
  virtual ~CIccTagJsonMultiProcessElement() {}
  virtual const char *GetClassName() const { return "CIccTagJsonMultiProcessElement"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);

  static CIccMultiProcessElement *CreateElement(const icChar *szElementName);

protected:
  bool ParseElement(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Dict / Struct / Array tags
// ---------------------------------------------------------------------------
class CIccTagJsonDict : public CIccTagDict, public CIccTagJson
{
public:
  virtual ~CIccTagJsonDict() {}
  virtual const char *GetClassName() const { return "CIccTagJsonDict"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonStruct : public CIccTagStruct, public CIccTagJson
{
public:
  virtual ~CIccTagJsonStruct() {}
  virtual const char *GetClassName() const { return "CIccTagJsonStruct"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);

protected:
  bool ParseTag(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonArray : public CIccTagArray, public CIccTagJson
{
public:
  virtual ~CIccTagJsonArray() {}
  virtual const char *GetClassName() const { return "CIccTagJsonArray"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Gamut / embedded image / embedded profile tags
// ---------------------------------------------------------------------------
class CIccTagJsonGamutBoundaryDesc : public CIccTagGamutBoundaryDesc, public CIccTagJson
{
public:
  virtual ~CIccTagJsonGamutBoundaryDesc() {}
  virtual const char *GetClassName() const { return "CIccTagJsonGamutBoundaryDesc"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonEmbeddedHeightImage : public CIccTagEmbeddedHeightImage, public CIccTagJson
{
public:
  virtual ~CIccTagJsonEmbeddedHeightImage() {}
  virtual const char *GetClassName() const { return "CIccTagJsonEmbeddedHeightImage"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonEmbeddedNormalImage : public CIccTagEmbeddedNormalImage, public CIccTagJson
{
public:
  virtual ~CIccTagJsonEmbeddedNormalImage() {}
  virtual const char *GetClassName() const { return "CIccTagJsonEmbeddedNormalImage"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccTagJsonEmbeddedProfile : public CIccTagEmbeddedProfile, public CIccTagJson
{
public:
  virtual ~CIccTagJsonEmbeddedProfile() {}
  virtual const char *GetClassName() const { return "CIccTagJsonEmbeddedProfile"; }
  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

#endif // _ICCTAGJSON_H
