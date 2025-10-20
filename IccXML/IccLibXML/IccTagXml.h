/** @file
File:       IccTagXml.h

Contains:   Header for implementation of CIccTagXml class and
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

#ifndef _ICCTAGXML_H
#define _ICCTAGXML_H
#include "IccTag.h"
#include "IccTagMPE.h"
#include "IccXmlConfig.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

class CIccTagXml : public IIccExtensionTag
{
public:
  virtual ~CIccTagXml(void) {}

  virtual const char *GetExtClassName() const { return "CIccTagXml"; }
  virtual const char *GetExtDerivedClassName() const { return ""; }

  virtual bool ToXml(std::string &xml, std::string blanks="") = 0;
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr) = 0;
};

class CIccTagXmlUnknown : public CIccTagUnknown, public CIccTagXml
{
public:
  CIccTagXmlUnknown(icTagTypeSignature nType) { m_nType = nType; }
  virtual ~CIccTagXmlUnknown() {}

  virtual const char *GetClassName() const {return "CIccTagXmlUnknown"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlText : public CIccTagText, public CIccTagXml
{
public:
  CIccTagXmlText() : CIccTagText() {}
  CIccTagXmlText(const CIccTagXmlText &ITT) : CIccTagText(ITT) {}
  virtual ~CIccTagXmlText() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlText(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlText"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlUtf8Text : public CIccTagUtf8Text, public CIccTagXml
{
public:
  CIccTagXmlUtf8Text() : CIccTagUtf8Text() {}
  CIccTagXmlUtf8Text(const CIccTagXmlUtf8Text &ITT) : CIccTagUtf8Text(ITT) {}
  virtual ~CIccTagXmlUtf8Text() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlUtf8Text(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlUtf8Text"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlZipUtf8Text : public CIccTagZipUtf8Text, public CIccTagXml
{
public:
  CIccTagXmlZipUtf8Text() : CIccTagZipUtf8Text() {}
  CIccTagXmlZipUtf8Text(const CIccTagXmlZipUtf8Text &ITT) : CIccTagZipUtf8Text(ITT) {}
  virtual ~CIccTagXmlZipUtf8Text() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlZipUtf8Text(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlZipUtf8Text"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlZipXml : public CIccTagZipXml, public CIccTagXml
{
public:
  CIccTagXmlZipXml() : CIccTagZipXml() {}
  CIccTagXmlZipXml(const CIccTagXmlZipXml &ITT) : CIccTagZipXml(ITT) {}
  virtual ~CIccTagXmlZipXml() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlZipXml(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlZipXml"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlUtf16Text : public CIccTagUtf16Text, public CIccTagXml
{
public:
  CIccTagXmlUtf16Text() : CIccTagUtf16Text() {}
  CIccTagXmlUtf16Text(const CIccTagXmlUtf16Text &ITT) : CIccTagUtf16Text(ITT) {}
  virtual ~CIccTagXmlUtf16Text() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlUtf16Text(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlUtf16Text"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccTagXmlTextDescription : public CIccTagTextDescription, public CIccTagXml
{
public:
  CIccTagXmlTextDescription() : CIccTagTextDescription() {}
  CIccTagXmlTextDescription(const CIccTagXmlTextDescription &ITTD) : CIccTagTextDescription(ITTD) {}
  virtual ~CIccTagXmlTextDescription() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlTextDescription(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlTextDescription"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlSignature : public CIccTagSignature, public CIccTagXml
{
public:
  virtual ~CIccTagXmlSignature() {}

  virtual const char *GetClassName() const {return "CIccTagXmlSignature"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlNamedColor2 : public CIccTagNamedColor2, public CIccTagXml
{
public:
  virtual ~CIccTagXmlNamedColor2() {}

  virtual const char *GetClassName() const {return "CIccTagXmlNamedColor2"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlXYZ : public CIccTagXYZ, public CIccTagXml
{
public:
  virtual ~CIccTagXmlXYZ() {}

  virtual const char *GetClassName() const {return "CIccTagXmlXYZ"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlChromaticity : public CIccTagChromaticity, public CIccTagXml
{
public:
  virtual ~CIccTagXmlChromaticity() {}

  virtual const char *GetClassName() const {return "CIccTagXmlChromaticity"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlCicp : public CIccTagCicp, public CIccTagXml
{
public:
  virtual ~CIccTagXmlCicp() {}

  virtual const char* GetClassName() const { return "CIccTagXmlCicp"; }

  virtual IIccExtensionTag* GetExtension() { return this; }

  virtual bool ToXml(std::string& xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode* pNode, std::string& parseStr);
};


class CIccTagXmlSparseMatrixArray : public CIccTagSparseMatrixArray, public CIccTagXml
{
public:
  virtual ~CIccTagXmlSparseMatrixArray() {}

  virtual const char *GetClassName() const {return "CIccTagXmlSparseMatrixArray"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

template <class T, icTagTypeSignature Tsig>
class CIccTagXmlFixedNum : public CIccTagFixedNum<T, Tsig>, public CIccTagXml
{
public:
  virtual ~CIccTagXmlFixedNum() {}

  virtual const char *GetClassName() const;

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

typedef CIccTagXmlFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType> CIccTagXmlS15Fixed16;
typedef CIccTagFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType> CIccTagXmlU16Fixed16;


template <class T, class A, icTagTypeSignature Tsig>
class CIccTagXmlNum : public CIccTagNum<T, Tsig>, public CIccTagXml
{
public:
  virtual ~CIccTagXmlNum() {}

  virtual const char *GetClassName() const;

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

//Forward Reference to get classes built
template <class T, icTagTypeSignature Tsig> class CIccXmlArrayType;

typedef CIccTagXmlNum<icUInt8Number, CIccXmlArrayType<icUInt8Number, icSigUInt8ArrayType>, icSigUInt8ArrayType> CIccTagXmlUInt8;
typedef CIccTagXmlNum<icUInt16Number, CIccXmlArrayType<icUInt16Number, icSigUInt16ArrayType>, icSigUInt16ArrayType> CIccTagXmlUInt16;
typedef CIccTagXmlNum<icUInt32Number, CIccXmlArrayType<icUInt32Number, icSigUInt32ArrayType>, icSigUInt32ArrayType> CIccTagXmlUInt32;
typedef CIccTagXmlNum<icUInt64Number, CIccXmlArrayType<icUInt64Number, icSigUInt64ArrayType>, icSigUInt64ArrayType> CIccTagXmlUInt64;

template <class T, class A, icTagTypeSignature Tsig>
class CIccTagXmlFloatNum : public CIccTagFloatNum<T, Tsig>, public CIccTagXml
{
public:
  virtual ~CIccTagXmlFloatNum() {}

  virtual const char *GetClassName() const;

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

typedef CIccTagXmlFloatNum<icFloat32Number, CIccXmlArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat16ArrayType> CIccTagXmlFloat16;
typedef CIccTagXmlFloatNum<icFloat32Number, CIccXmlArrayType<icFloat32Number, icSigFloat32ArrayType>, icSigFloat32ArrayType> CIccTagXmlFloat32;
typedef CIccTagXmlFloatNum<icFloat64Number, CIccXmlArrayType<icFloat64Number, icSigFloat64ArrayType>, icSigFloat64ArrayType> CIccTagXmlFloat64;

class CIccTagXmlMeasurement : public CIccTagMeasurement, public CIccTagXml
{
public:
  virtual ~CIccTagXmlMeasurement() {}

  virtual const char *GetClassName() const {return "CIccTagXmlMeasurement"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlMultiLocalizedUnicode : public CIccTagMultiLocalizedUnicode, public CIccTagXml
{
public:
  CIccTagXmlMultiLocalizedUnicode() : CIccTagMultiLocalizedUnicode() {}
  CIccTagXmlMultiLocalizedUnicode(const CIccTagXmlMultiLocalizedUnicode &ITMLU) : CIccTagMultiLocalizedUnicode(ITMLU) {}
  virtual ~CIccTagXmlMultiLocalizedUnicode() {}

  virtual CIccTag* NewCopy() const {return new CIccTagXmlMultiLocalizedUnicode(*this);}

  virtual const char *GetClassName() const {return "CIccTagXmlMultiLocalizedUnicode"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlTagData : public CIccTagData, public CIccTagXml
{
public:
  virtual ~CIccTagXmlTagData() {}

  virtual const char *GetClassName() const {return "CIccTagXmlTagData"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlDateTime : public CIccTagDateTime, public CIccTagXml
{
public:
  virtual ~CIccTagXmlDateTime() {}

  virtual const char *GetClassName() const {return "CIccTagXmlDateTime"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlColorantOrder : public CIccTagColorantOrder, public CIccTagXml
{
public:
  virtual ~CIccTagXmlColorantOrder() {}

  virtual const char *GetClassName() const {return "CIccTagXmlColorantOrder"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlColorantTable : public CIccTagColorantTable, public CIccTagXml
{
public:
  virtual ~CIccTagXmlColorantTable() {}

  virtual const char *GetClassName() const {return "CIccTagXmlColorantTable"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlViewingConditions : public CIccTagViewingConditions, public CIccTagXml
{
public:
  virtual ~CIccTagXmlViewingConditions() {}

  virtual const char *GetClassName() const {return "CIccTagXmlViewingConditions"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlSpectralDataInfo : public CIccTagSpectralDataInfo, public CIccTagXml
{
public:
  virtual ~CIccTagXmlSpectralDataInfo() {}

  virtual const char *GetClassName() const {return "CIccTagXmlSpectralDataInfo"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlSpectralViewingConditions : public CIccTagSpectralViewingConditions, public CIccTagXml
{
public:
  virtual ~CIccTagXmlSpectralViewingConditions() {}

  virtual const char *GetClassName() const {return "CIccTagXmlSpectralViewingConditions"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlProfileSeqDesc : public CIccTagProfileSeqDesc, public CIccTagXml
{
public:
  virtual ~CIccTagXmlProfileSeqDesc() {}

  virtual const char *GetClassName() const {return "CIccTagXmlProfileSeqDesc"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlResponseCurveSet16 : public CIccTagResponseCurveSet16, public CIccTagXml
{
public:
  virtual ~CIccTagXmlResponseCurveSet16() {}

  virtual const char *GetClassName() const {return "CIccTagXmlResponseCurveSet16"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

 class CIccCurveXml : public CIccTagXml
 {
 public:
   virtual ~CIccCurveXml() {}
 
   virtual const char *GetExtDerivedClassName() const {return "CIccCurveXml"; }
 
   virtual bool ToXml(std::string &xml, icConvertType nType, std::string blanks) = 0;
   virtual bool ParseXml(xmlNode *pNode, icConvertType nType, std::string &parseStr) = 0;
 };
 
class CIccTagXmlCurve : public CIccTagCurve, public CIccCurveXml
{
public:
  virtual ~CIccTagXmlCurve() {}

  virtual const char *GetClassName() const {return "CIccTagXmlCurve"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ToXml(std::string &xml, icConvertType nType, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
  virtual bool ParseXml(xmlNode *pNode, icConvertType nType, std::string &parseStr);
};

class CIccTagXmlParametricCurve : public CIccTagParametricCurve, public CIccCurveXml
{
public:
  virtual ~CIccTagXmlParametricCurve() {}

  virtual const char *GetClassName() const {return "CIccTagXmlParametricCurve"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ToXml(std::string &xml, icConvertType nType, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
   virtual bool ParseXml(xmlNode *pNode, icConvertType nType, std::string &parseStr);
};

class CIccTagXmlSegmentedCurve : public CIccTagSegmentedCurve, public CIccCurveXml
{
public:
  virtual ~CIccTagXmlSegmentedCurve() {}

  virtual const char *GetClassName() const {return "CIccTagXmlSegmentedCurve"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ToXml(std::string &xml, icConvertType nType, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
  virtual bool ParseXml(xmlNode *pNode, icConvertType nType, std::string &parseStr);
};

class CIccTagXmlLutAtoB : public CIccTagLutAtoB, public CIccTagXml
{
public:
  virtual ~CIccTagXmlLutAtoB() {}

  virtual const char *GetClassName() const {return "CIccTagXmlLutAtoB"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlLutBtoA : public CIccTagLutBtoA, public CIccTagXml
{
public:
  virtual ~CIccTagXmlLutBtoA() {}

  virtual const char *GetClassName() const {return "CIccTagXmlLutBtoA"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlLut8 : public CIccTagLut8, public CIccTagXml
{
public:
  virtual ~CIccTagXmlLut8() {}

  virtual const char *GetClassName() const {return "CIccTagXmlLut8"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlLut16 : public CIccTagLut16, public CIccTagXml
{
public:
  virtual ~CIccTagXmlLut16() {}

  virtual const char *GetClassName() const {return "CIccTagXmlLut16"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlMultiProcessElement : public CIccTagMultiProcessElement, public CIccTagXml
{
public:
  virtual ~CIccTagXmlMultiProcessElement() {}

  virtual const char *GetClassName() const {return "CIccTagXmlMultiProcessElement"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);

  static CIccMultiProcessElement *CreateElement(const icChar *szElementNodeName);

protected:
  bool ParseElement(xmlNode *pNode, std::string &parseStr);

};

class CIccTagXmlProfileSequenceId : public CIccTagProfileSequenceId, public CIccTagXml
{
public:
  virtual ~CIccTagXmlProfileSequenceId() {}

  virtual const char *GetClassName() const {return "CIccTagXmlProfileSequenceId"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlDict : public CIccTagDict, public CIccTagXml
{
public:
  virtual ~CIccTagXmlDict() {}

  virtual const char *GetClassName() const {return "CIccTagXmlDict"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccTagXmlStruct : public CIccTagStruct, public CIccTagXml
{
public:
  virtual ~CIccTagXmlStruct() {}

  virtual const char *GetClassName() const {return "CIccTagXmlStruct"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);

protected:
  bool ParseTag(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlArray : public CIccTagArray, public CIccTagXml
{
public:
  virtual ~CIccTagXmlArray() {}

  virtual const char *GetClassName() const {return "CIccTagXmlArray"; }

  virtual IIccExtensionTag *GetExtension() {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccTagXmlGamutBoundaryDesc : public CIccTagGamutBoundaryDesc, CIccTagXml
{
public:
  virtual ~CIccTagXmlGamutBoundaryDesc() {}

  virtual const char *GetGlassName() const {return "CIccTagXmlGamutBoundaryDesc"; }

  virtual IIccExtensionTag *GetExtension () {return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccTagXmlEmbeddedHeightImage : public CIccTagEmbeddedHeightImage, CIccTagXml
{
public:
  virtual ~CIccTagXmlEmbeddedHeightImage() {}

  virtual const char *GetGlassName() const { return "CIccTagXmlEmbeddedHeightImage"; }

  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccTagXmlEmbeddedNormalImage : public CIccTagEmbeddedNormalImage, CIccTagXml
{
public:
  virtual ~CIccTagXmlEmbeddedNormalImage() {}

  virtual const char *GetGlassName() const { return "CIccTagXmlEmbeddedNormalImage"; }

  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


//Private class to embed a profile within a profile
class CIccTagXmlEmbeddedProfile : public CIccTagEmbeddedProfile, CIccTagXml
{
public:
  virtual ~CIccTagXmlEmbeddedProfile() {}

  virtual const char *GetGlassName() const { return "CIccTagXmlEmbeddedProfile"; }

  virtual IIccExtensionTag *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};



#endif //_ICCTAGXML_H
