/** @file
    File:       IccUtilXml.cpp

    Contains:   Implementation of Utilities used for ICC/XML processing

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

#ifndef _ICCUTILXML_H
#define _ICCUTILXML_H

#include "IccUtil.h"
#include "IccTag.h"
#include "IccXmlConfig.h"
#include "libxml/parser.h"
#include <vector>
#include <string>

class CIccUTF16String
{
public:
  CIccUTF16String();
  CIccUTF16String(const icUInt16Number *uzStr);
  CIccUTF16String(const char *szStr);
  CIccUTF16String(const CIccUTF16String &str);
  virtual ~CIccUTF16String();

  void Clear();
  size_t Size() { return m_len; }
  bool Resize(size_t len);

  CIccUTF16String& operator=(const CIccUTF16String &wstr);
  CIccUTF16String& operator=(const char *szStr);
  CIccUTF16String& operator=(const icUInt16Number *uzStr);

  icUInt16Number operator[](size_t m_nIndex) { return m_str[m_nIndex]; }

  const icUInt16Number *c_str() { return m_str; }

  bool FromUtf8(const char *szStr, size_t sizeSrc=0);
  const char *ToUtf8(std::string &buf);
  const wchar_t *ToWString(std::wstring &buf);

  static size_t WStrlen(const icUInt16Number *uzStr);

protected:
  static size_t AllocSize(size_t n) { return (((n+64)/64)*64); }
  size_t m_alloc;
  size_t m_len;
  icUInt16Number *m_str;
};


const char *icFixXml(char *szDest, const char *szStr);
const char *icFixXml(std::string &buf, const char *szStr);
const char *icAnsiToUtf8(std::string &buf, const char *szSrc);
const char *icUtf8ToAnsi(std::string &buf, const char *szSrc);

const char *icUtf16ToUtf8(std::string &buf, const icUInt16Number *szSrc, int sizeSrc=0);
const unsigned short *icUtf8ToUtf16(CIccUTF16String &buf, const char *szSrc, int sizeSrc=0);

bool icCLUTDataToXml(std::string &xml, CIccCLUT *pCLUT, icConvertType nType, std::string blanks,
                     bool bSaveGridPoints=false);
bool icCLUTToXml(std::string &xml, CIccCLUT *pCLUT, icConvertType nType, std::string blanks,
                 bool bSaveGridPoints=false, const char *szExtraAttrs="", const char *szName="CLUT");
icFloatNumber icXmlStrToFloat(const xmlChar *szStr);
icSignature icXmlStrToSig(const char *szStr);
const char *icXmlAttrValue(xmlAttr *attr, const char *szDefault="");
const char *icXmlAttrValue(xmlNode *pNode, const char *szName, const char *szDefault="");

icUInt32Number icXmlGetChildSigVal(xmlNode *pNode);

icUInt32Number icXmlGetHexData(void *pBuf, const char *szText, icUInt32Number nBufSize);

icUInt32Number icXmlGetHexDataSize(const char *szText);

icUInt32Number icXmlDumpHexData(std::string &xml, std::string blanks, void *pBuf, icUInt32Number nBufSize);

#define icXmlStrCmp(x, y) strcmp((const char *)(x), (const char*)(y))

xmlAttr *icXmlFindAttr(xmlNode *pNode, const char *szAttrName);
xmlNode *icXmlFindNode(xmlNode *pNode, const char *szNodeName);

icUInt32Number icXmlNodeCount(xmlNode *pNode, const char *szNodeName);
icUInt32Number icXmlNodeCount2(xmlNode *pNode, const char *szNodeName1, const char *szNodeName2);
icUInt32Number icXmlNodeCount3(xmlNode *pNode, const char *szNodeName1, const char *szNodeName2, const char *szNodeName3);

template <class T, icTagTypeSignature Tsig>
class CIccXmlArrayType
{
public:
  CIccXmlArrayType();
  ~CIccXmlArrayType();

  static bool DumpArray(std::string &xml, std::string blanks, T *buf, icUInt32Number nBufSize,
                        icConvertType nType, icUInt8Number nColumns);
  static bool ParseArray(T *buf, icUInt32Number nBufSize, xmlNode *pNode);

  static icUInt32Number ParseTextCount(const char *szText);
  static icUInt32Number ParseText(T *buf, icUInt32Number nBufSize, const char *szText);
  static icUInt32Number ParseTextCountNum(const char *szText, icUInt32Number num, std::string &parseStr);

  bool ParseArray(xmlNode *pNode);
  bool ParseTextArray(const char *szText);
  bool ParseTextArray(xmlNode *pNode);
  bool ParseTextArrayNum(const char *szText, icUInt32Number num, std::string &parseStr);

  bool SetSize(icUInt32Number nSize);

  T *GetBuf() { return m_pBuf; }
  icUInt32Number GetSize() { return m_nSize; }

protected:
  icUInt32Number m_nSize;
  T *m_pBuf;
};

#define icSigFloatArrayType ((icTagTypeSignature)0x66637420)  /* 'flt ' */

typedef CIccXmlArrayType<icUInt8Number, icSigUInt8ArrayType> CIccUInt8Array;
typedef CIccXmlArrayType<icUInt16Number, icSigUInt16ArrayType> CIccUInt16Array;
typedef CIccXmlArrayType<icUInt32Number, icSigUInt32ArrayType> CIccUInt32Array;
typedef CIccXmlArrayType<icUInt64Number, icSigUInt64ArrayType> CIccUInt64Array;
typedef CIccXmlArrayType<icFloatNumber, icSigFloatArrayType> CIccFloatArray;
typedef CIccXmlArrayType<icFloat32Number, icSigFloat32ArrayType> CIccFloat32Array;
typedef CIccXmlArrayType<icFloat64Number, icSigFloat64ArrayType> CIccFloat64Array;

const icChar* icGetTagSigTypeName(icTagTypeSignature tagTypeSig);
icTagTypeSignature icGetTypeNameTagSig(const icChar *szTagType);
const icChar* icGetTagSigName(icTagSignature tagSig);
icTagSignature icGetTagNameSig(const icChar *szTagName);
icRenderingIntent icGetRenderingIntentValue (const icChar *szRenderingIntent);
icStandardObserver icGetNamedStandardObserverValue(const icChar *str);
icMeasurementGeometry icGeNamedtMeasurementGeometryValue(const icChar *str);
icMeasurementFlare icGetNamedMeasurementFlareValue(const icChar *str);
icIlluminant icGetIlluminantValue(const icChar *str);
const icChar* icGetStandardObserverName(icStandardObserver str);
icDateTimeNumber icGetDateTimeValue(const icChar *str);
icUInt64Number icGetDeviceAttrValue(xmlNode *pNode);
icColorantEncoding icGetColorantValue(const icChar* str);
icMeasurementUnitSig icGetMeasurementValue(const icChar* str);
const std::string icGetDeviceAttrName(icUInt64Number devAttr);
const std::string icGetHeaderFlagsName(icUInt32Number flags, bool bUsesMCS=false);
const std::string icGetPadSpace(double value);




#endif //_ICCUTILXML_H