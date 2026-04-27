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

// CIccUTF16String, icUtf16ToUtf8, icUtf8ToUtf16 are now declared in IccUtil.h
// and available here via the #include "IccUtil.h" above.

const char *icFixXml(char *szDest, const char *szStr);
const char *icFixXml(std::string &buf, const char *szStr);
const char *icAnsiToUtf8(std::string &buf, const char *szSrc);
const char *icUtf8ToAnsi(std::string &buf, const char *szSrc);

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

size_t icXmlDumpHexData(std::string &xml, std::string blanks, void *pBuf, size_t nBufSize);

bool icXmlValidateFileCount(size_t value, icUInt32Number &count, std::string &parseStr, const char *filename);

// Maximum byte count for text file buffers: caps num+1 from wrapping size_t
// and prevents downstream ParseTextArrayNum from consuming gigabytes.
// Used at every new(std::nothrow) char[num+1] text-buffer site.
static const size_t icXmlMaxTextFileBytes = 256ULL * 1024 * 1024;

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
  static icUInt32Number ParseTextCountNum(const char *szText, size_t num, std::string &parseStr);

  bool ParseArray(xmlNode *pNode);
  bool ParseTextArray(const char *szText);
  bool ParseTextArray(xmlNode *pNode);
  bool ParseTextArrayNum(const char *szText, size_t num, std::string &parseStr);

  bool SetSize(icUInt32Number nSize);

  T *GetBuf() { return m_pBuf; }
  icUInt32Number GetSize() { return m_nSize; }

protected:
  icUInt32Number m_nSize;
  T *m_pBuf;
};

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
// icGetRenderingIntentValue, icGetDateTimeValue, icGetNamedStandardObserverValue,
// and icGetIlluminantValue are now in IccUtil.h
icMeasurementGeometry icGeNamedtMeasurementGeometryValue(const icChar *str);
icMeasurementFlare icGetNamedMeasurementFlareValue(const icChar *str);
const icChar* icGetStandardObserverName(icStandardObserver str);
icUInt64Number icGetDeviceAttrValue(xmlNode *pNode);
icColorantEncoding icGetColorantValue(const icChar* str);
// icGetMeasurementValue moved to IccProfLib/IccUtil.h
const std::string icGetDeviceAttrName(icUInt64Number devAttr);
const std::string icGetHeaderFlagsName(icUInt32Number flags, bool bUsesMCS=false);
const std::string icGetPadSpace(double value);




#endif //_ICCUTILXML_H
