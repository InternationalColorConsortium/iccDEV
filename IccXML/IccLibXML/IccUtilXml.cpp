/** @file
    File:       IccUtilXml.cpp

    Contains:   Implementation of XML conversion utilities

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

#include <time.h>
#include "IccUtilXml.h"
#include "IccConvertUTF.h"
#include "IccTagFactory.h"

#ifdef WIN32
#include <windows.h>
#ifdef GetClassName
#undef GetClassName
#endif
#endif
#include <cstring> /* C strings strcpy, memcpy ... */
#include <math.h>  /* nanf */
#include <limits>



CIccUTF16String::CIccUTF16String()
{
  m_alloc=64;
  m_len = 0;
  m_str = (icUInt16Number*)calloc(m_alloc, sizeof(icUInt16Number));
}

CIccUTF16String::CIccUTF16String(const icUInt16Number *uzStr)
{
  m_len = WStrlen(uzStr);
  m_alloc = AllocSize(m_len);

  m_str = (icUInt16Number *)malloc(m_alloc*sizeof(icUInt16Number));
  memcpy(m_str, uzStr, m_len+1*sizeof(icUInt16Number));
}

CIccUTF16String::CIccUTF16String(const char *szStr)
{
  size_t sizeSrc = strlen(szStr);

  if (sizeSrc) {
    m_alloc = AllocSize(sizeSrc*2);
    m_str = (UTF16 *)calloc(m_alloc, sizeof(icUInt16Number)); //overallocate to allow for up to 4 bytes per character
    UTF16 *szDest = m_str;
    icConvertUTF8toUTF16((const UTF8 **)&szStr, (const UTF8 *)&szStr[sizeSrc], &szDest, &szDest[m_alloc], lenientConversion);
    if (m_str[0]==0xfeff) {
      size_t i;
      for (i=1; m_str[i]; i++)
        m_str[i-1] = m_str[i];
      m_str[i-1] = 0;
    }
    m_len = WStrlen(m_str);
  }
  else {
    m_alloc = 64;
    m_len = 0;
    m_str = (icUInt16Number*)calloc(m_alloc, sizeof(icUInt16Number));
  }
}

CIccUTF16String::CIccUTF16String(const CIccUTF16String &str)
{
  m_alloc = str.m_alloc;
  m_len = str.m_len;
  m_str = (icUInt16Number*)malloc(m_alloc*sizeof(icUInt16Number));

  memcpy(m_str, str.m_str, (m_alloc)*sizeof(icUInt16Number));
}

CIccUTF16String::~CIccUTF16String()
{
  free(m_str);
}

void CIccUTF16String::Clear()
{
  m_len = 0;
  m_str[0] = 0;
}

bool CIccUTF16String::Resize(size_t len)
{
  if (len>m_alloc) {
    size_t nAlloc = AllocSize(len);

    m_str = (icUInt16Number*)icRealloc(m_str, nAlloc*sizeof(icUInt16Number));

    if (!m_str) {
      m_len = 0;
      return false;
    }
    m_alloc = nAlloc;
  }

  if (len>m_len) {
    memset(&m_str[m_len], 0x0020, (len-m_len)*sizeof(icUInt16Number));
  }
  m_len = len;
  m_str[m_len] = 0;
  return true;
}

size_t CIccUTF16String::WStrlen(const icUInt16Number *uzStr)
{
  size_t n=0;
  while(uzStr[n]) n++;

  return n;
}

CIccUTF16String& CIccUTF16String::operator=(const CIccUTF16String &wstr)
{
  if (m_alloc<=wstr.m_alloc) {
    m_str = (icUInt16Number*)icRealloc(m_str, wstr.m_alloc *sizeof(icUInt16Number));
    if (m_str)
      m_alloc = wstr.m_alloc;
    else
      m_alloc = 0;
  }
  if (m_str) {
    m_len = wstr.m_len;

    memcpy(m_str, wstr.m_str, (m_len+1)*sizeof(icUInt16Number));
  }
  else {
    m_len = 0;
  }

  return *this;
}

CIccUTF16String& CIccUTF16String::operator=(const char *szStr)
{
  FromUtf8(szStr, 0);

  return *this;
}

CIccUTF16String& CIccUTF16String::operator=(const icUInt16Number *uzStr)
{
  size_t n = WStrlen(uzStr);
  size_t nAlloc = AllocSize(n);

  if (m_alloc<=nAlloc) {
    m_str = (icUInt16Number*)icRealloc(m_str, nAlloc*sizeof(icUInt16Number));
    if (m_str)
      m_alloc = nAlloc;
    else
      m_alloc = 0;
  }
  if (m_str) {
    m_len = n;

    memcpy(m_str, uzStr, (m_len+1)*sizeof(icUInt16Number));
  }
  else {
    m_len = 0;
  }

  return *this;
}


bool CIccUTF16String::FromUtf8(const char *szStr, size_t sizeSrc)
{
  if (!sizeSrc)
    sizeSrc = strlen(szStr);

  if (sizeSrc) {
    size_t nAlloc = AllocSize(sizeSrc*2);
    if (m_alloc<=nAlloc) {
      m_str = (icUInt16Number*)icRealloc(m_str, nAlloc *sizeof(icUInt16Number));
      m_alloc = nAlloc;
    }
    if (m_str) {
      memset(m_str, 0, m_alloc * sizeof(icUInt16Number));
      UTF16 *szDest = m_str;
      icConvertUTF8toUTF16((const UTF8 **)&szStr, (const UTF8 *)&szStr[sizeSrc], &szDest, &szDest[m_alloc], lenientConversion);
      if (m_str[0]==0xfeff) {
        size_t i;
        for (i=1; m_str[i]; i++)
          m_str[i-1] = m_str[i];
        m_str[i-1] = 0;
      }
      m_len = WStrlen(m_str);
    }
    else {
      m_len =0;
      return false;
    }
  }
  else {
    m_len = 0;
    m_str[0] = 0;
  }
  return true;
}

const char *CIccUTF16String::ToUtf8(std::string &buf)
{
  return icUtf16ToUtf8(buf, m_str, (int)m_len);
}

const wchar_t *CIccUTF16String::ToWString(std::wstring &buf)
{
  size_t i;

  buf.clear();

  for (i=0; i<m_len; i++) {
    buf += (wchar_t)m_str[i];
  }

  return buf.c_str();
}

const char *icFixXml(std::string &buf, const char *szStr)
{
  buf = "";
  while (*szStr) {
    switch (*szStr) {
    case '\'':
      buf += "&apos;";
      break;
    case '&':
      buf += "&amp;";
      break;
    case '\"':
      buf += "&quot;";
      break;
    case '<':
      buf += "&lt;";
      break;
    case '>':
      buf += "&gt;";
      break;
    default:
      buf += *szStr;
    }
    szStr++;
  }

  return buf.c_str();
}

const char *icFixXml(char *szDest, const char *szStr)
{
  char *m_ptr = szDest; 

  while (*szStr) {
    switch (*szStr) {
    case '\'':
      strcpy(m_ptr, "&apos;");
      m_ptr += 6;
      break;
    case '&':
      strcpy(m_ptr, "&amp;");
      m_ptr += 5;
      break;
    case '\"':
      strcpy(m_ptr, "&quot;");
      m_ptr += 6;
      break;
    case '<':
      strcpy(m_ptr, "&lt;");
      m_ptr += 4;
      break;
    case '>':
      strcpy(m_ptr, "&gt;");
      m_ptr += 4;
      break;
    default:
      *m_ptr++ = *szStr;
    }
    szStr++;
  }
  *m_ptr = '\0';

  return szDest;
}

const char *icUtf16ToUtf8(std::string &buf, const icUInt16Number *szSrc, int sizeSrc/*=0*/) 
{
  if (!sizeSrc) {
    sizeSrc = (int)CIccUTF16String::WStrlen(szSrc);
  }

  int n = sizeSrc*4;

  if (n) {
    char *szBuf = (char *)malloc(n+1);
    char *szDest = szBuf;
    icConvertUTF16toUTF8(&szSrc, &szSrc[sizeSrc], (UTF8**)&szDest, (UTF8*)&szDest[n+1], lenientConversion);
    *szDest= '\0';

    buf = szBuf;
    free(szBuf);
  }
  else {
    buf.clear();
  }

  return buf.c_str();
}

const unsigned short *icUtf8ToUtf16(CIccUTF16String &buf, const char *szSrc, int sizeSrc/*=0*/) 
{ 
  buf.FromUtf8(szSrc, sizeSrc);

  return buf.c_str();
}

const char *icAnsiToUtf8(std::string &buf, const char *szSrc)
{
#ifdef WIN32
  size_t len = strlen(szSrc)+1;
  wchar_t *szUnicodeBuf = (wchar_t*)malloc(len*sizeof(icUInt16Number)*2);
  char *szBuf = (char*)malloc(len*2);

  size_t n;

  n=MultiByteToWideChar(CP_ACP, 0, szSrc, (int)len-1, szUnicodeBuf, (int)len*2);
  szUnicodeBuf[n] = '\0';

  n = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)szUnicodeBuf, (int)n, szBuf, (int)len*2, 0, NULL);
  szBuf[n] = '\0';

  buf = szBuf;

  free(szBuf);
  free(szUnicodeBuf);
#else
  buf = szSrc;
#endif
  return buf.c_str();
}

const char *icUtf8ToAnsi(std::string &buf, const char *szSrc)
{
#ifdef WIN32
  size_t len = strlen(szSrc)+1;
  wchar_t *szUnicodeBuf = (wchar_t*)malloc(len*sizeof(icUInt16Number)*2);
  char *szBuf = (char*)malloc(len*2);

  size_t n;

  n=MultiByteToWideChar(CP_UTF8, 0, szSrc, (int)len, szUnicodeBuf, (int)len*2);
  szUnicodeBuf[n] = '\0';

  n = WideCharToMultiByte(CP_ACP, 0, (const wchar_t*)szUnicodeBuf, (int)n, szBuf, (int)len*2, "?", NULL);
  szBuf[n] = '\0';

  buf = szBuf;

  free(szBuf);
  free(szUnicodeBuf);
#else
  buf = szSrc;
#endif
  return buf.c_str();
}

class CIccDumpXmlCLUT : public IIccCLUTExec
{
public:
  CIccDumpXmlCLUT(std::string *xml, icConvertType nType, std::string blanks, icUInt16Number nSamples, icUInt8Number nPixelsPerRow)
  {
    m_xml = xml;
    m_nType = nType;
    m_blanks = blanks;
    m_nSamples = nSamples;
    m_nPixelsPerRow = nPixelsPerRow;
    m_nCurPixel = 0;
  }

  virtual void PixelOp(icFloatNumber* /*pGridAdr*/, icFloatNumber* pData)
  {
    int i;
    const size_t bufSize = 128;
    char buf[bufSize];

    if (!(m_nCurPixel % m_nPixelsPerRow))
      *m_xml += m_blanks;

    switch(m_nType) {
      case icConvert8Bit:
        for (i=0; i<m_nSamples; i++) {
          snprintf(buf, bufSize, " %3d", (icUInt8Number)(pData[i]*255.0 + 0.5));
          *m_xml += buf;
        }
        break;
      case icConvert16Bit:
        for (i=0; i<m_nSamples; i++) {
          snprintf(buf, bufSize, " %5d", (icUInt16Number)(pData[i]*65535.0 + 0.5));
          *m_xml += buf;
        }
        break;
      case icConvertFloat:
      default:
        for (i=0; i<m_nSamples; i++) {
          snprintf(buf, bufSize, " %13.8f", pData[i]);
          *m_xml += buf;
        }
        break;
    }
    m_nCurPixel++;
    if (!(m_nCurPixel % m_nPixelsPerRow)) {
      *m_xml += "\n";
    }
  }

  void Finish()
  {
    if (m_nCurPixel % m_nPixelsPerRow) {
      *m_xml += "\n";
    }
  }

  std::string *m_xml;
  icConvertType m_nType;
  std::string m_blanks;
  icUInt16Number m_nSamples;
  icUInt8Number m_nPixelsPerRow;
  icUInt32Number m_nCurPixel;
};

bool icCLUTDataToXml(std::string &xml, CIccCLUT *pCLUT, icConvertType nType, std::string blanks, 
                     bool bSaveGridPoints/*=false*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  int nStartType = nType;
  if (nType == icConvertVariable) {
    nType = pCLUT->GetPrecision()==1 ? icConvert8Bit : icConvert16Bit;
  }

  if (bSaveGridPoints) {
    xml += blanks + "  <GridPoints>";
    int i;

    for (i=0; i<pCLUT->GetInputDim(); i++) {
      if (i)
        snprintf(buf, bufSize, " %d", pCLUT->GridPoint(i));
      else
        snprintf(buf, bufSize, "%d", pCLUT->GridPoint(i));
      xml += buf;
    }
    xml += "</GridPoints>\n";
  }

  int nPixelsPerRow = pCLUT->GridPoint(0);

  // if the CLUT has no GridPoints, profile is invalid
  if (nPixelsPerRow == 0) {
    printf("\nError! - CLUT Table not found.\n");
    return false;
  }

  CIccDumpXmlCLUT dumper(&xml, nType, blanks + "   ", pCLUT->GetOutputChannels(), nPixelsPerRow);

  xml += blanks + "  <TableData";

  if (nStartType == icConvertVariable && nType == icConvert8Bit) {
    snprintf(buf, bufSize, " Precision=\"1\"");
    xml += buf;
  }

  xml += ">\n";

  pCLUT->Iterate(&dumper);

  dumper.Finish();

  xml += blanks + "  </TableData>\n";

  return true;

}

bool icCLUTToXml(std::string &xml, CIccCLUT *pCLUT, icConvertType nType, std::string blanks, 
                 bool bSaveGridPoints/*=false*/, const char *szExtraAttrs/*=""*/, const char *szName/*="CLUT"*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  xml += blanks + "<" + szName;

  if (!bSaveGridPoints) {
    snprintf(buf, bufSize, " GridGranularity=\"%d\"", pCLUT->GridPoint(0));
    xml += buf;
  }

   if (szExtraAttrs && *szExtraAttrs) {
    xml += szExtraAttrs;
  }
  xml += ">\n";

  icCLUTDataToXml(xml, pCLUT, nType, blanks, bSaveGridPoints);

  xml += blanks + "</" + szName + ">\n";
  return true;
}

icFloatNumber icXmlStrToFloat(const xmlChar *szStr)
{
  float f=0.0;
  sscanf((const char*)szStr, "%f", &f);

  return (icFloatNumber)f;
}

icSignature icXmlStrToSig(const char *szStr)
{
  return icGetSigVal(szStr);
}

const char *icXmlAttrValue(xmlAttr *attr, const char *szDefault)
{
  if (attr && attr->children && attr->children->type == XML_TEXT_NODE && attr->children->content)
    return (const char*)attr->children->content;

  return szDefault;
}

const char *icXmlAttrValue(xmlNode *pNode, const char *szName, const char *szDefault)
{
  xmlAttr *attr = icXmlFindAttr(pNode, szName);
  if (attr) {
    return icXmlAttrValue(attr, szDefault);
  }
  return szDefault;
}

icUInt32Number icXmlGetChildSigVal(xmlNode *pNode)
{
  if (!pNode || !pNode->children || !pNode->children->content)
    return 0;

  return icGetSigVal((const char*)pNode->children->content);
}

static int hexValue(char c)
{
  if (c>='0' && c<='9')
    return c-'0';
  if (c>='A' && c<='F')
    return c-'A'+10;
  if (c>='a' && c<='f')
    return c-'a'+10;
  return -1;
}


icUInt32Number icXmlGetHexData(void *pBuf, const char *szText, icUInt32Number nBufSize)
{
  unsigned char *pDest = (unsigned char*)pBuf;
  icUInt32Number rv =0;

  while( rv<nBufSize && *szText ) {
    int c1=hexValue(szText[0]);
    int c2=hexValue(szText[1]);
    if (c1>=0 && c2>=0) {
      *pDest = c1*16+ c2;
      pDest++;
      szText +=2;
      rv++;
    }
    else {
      szText++;
    }
  }
  return rv;
}

icUInt32Number icXmlGetHexDataSize(const char *szText)
{
  icUInt32Number rv =0;

  while(*szText) {
    int c1=hexValue(szText[0]);
    int c2=hexValue(szText[1]);
    if (c1>=0 && c2>=0) {
      szText +=2;
      rv++;
    }
    else {
      szText++;
    }
  }
  return rv;
}

size_t icXmlDumpHexData(std::string &xml, std::string blanks, void *pBuf, size_t nBufSize)
{
  icUInt8Number *m_ptr = (icUInt8Number *)pBuf;
  const size_t bufSize = 15;
  char buf[bufSize];
  size_t i;

  for (i=0; i<nBufSize; i++, m_ptr++) {
    if (!(i%32)) {
      if (i)
        xml += "\n";
      xml += blanks;
    }
    snprintf(buf, bufSize, "%02x", *m_ptr);
    xml += buf;
  }
  if (i) {
    xml += "\n";
  }
  return i;
}

xmlAttr *icXmlFindAttr(xmlNode *pNode, const char *szAttrName)
{
  if (!pNode) return NULL;

  xmlAttr *attr;

  for (attr = pNode->properties; attr; attr = attr->next) {
    if (attr->type != XML_ATTRIBUTE_NODE)
      continue;

    if (!icXmlStrCmp(attr->name, szAttrName)) {
      return attr;
    }
  }

  return NULL;
}

xmlNode *icXmlFindNode(xmlNode *pNode, const char *szNodeName)
{
  if (!pNode) return NULL;

  for (; pNode; pNode = pNode->next) {
    if (pNode->type != XML_ELEMENT_NODE)
      continue;

    if (!icXmlStrCmp(pNode->name, szNodeName)) {
      return pNode;
    }
  }

  return NULL;
}

icUInt32Number icXmlNodeCount(xmlNode *pNode, const char *szNodeName)
{
  icUInt32Number rv = 0;
  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE &&
        !icXmlStrCmp(pNode->name, szNodeName)) {
      rv++;
    }
  }
  return rv;
}

icUInt32Number icXmlNodeCount2(xmlNode *pNode, const char *szNodeName1, const char *szNodeName2)
{
  icUInt32Number rv = 0;
  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE &&
      (!icXmlStrCmp(pNode->name, szNodeName1) || !icXmlStrCmp(pNode->name, szNodeName2))) {
      rv++;
    }
  }
  return rv;
}

icUInt32Number icXmlNodeCount3(xmlNode *pNode, const char *szNodeName1, const char *szNodeName2, const char *szNodeName3)
{
  icUInt32Number rv = 0;
  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE &&
      (!icXmlStrCmp(pNode->name, szNodeName1) || !icXmlStrCmp(pNode->name, szNodeName2) || !icXmlStrCmp(pNode->name, szNodeName3))) {
      rv++;
    }
  }
  return rv;
}

template <class T, icTagTypeSignature Tsig>
CIccXmlArrayType<T, Tsig>::CIccXmlArrayType()
{
  m_pBuf = NULL;
  m_nSize = 0;
}

template <class T, icTagTypeSignature Tsig>
CIccXmlArrayType<T, Tsig>::~CIccXmlArrayType()
{
  if (m_pBuf) {
    free(m_pBuf);
  }
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::ParseArray(xmlNode *pNode)
{
  char scanType[2] = {0};
  scanType[0] = (Tsig == icSigFloatArrayType ? 'f' : 'n');
  scanType[1] = 0;

  icUInt32Number n = icXmlNodeCount(pNode, scanType);
  
  if (n) {
    if (!SetSize(n))
      return false;
    return ParseArray(m_pBuf, m_nSize, pNode);
  }

  for ( ;pNode && pNode->type!= XML_TEXT_NODE; pNode=pNode->next);

  if (!pNode || !pNode->content)
    return false;

  n = ParseTextCount((const char*)pNode->content);

  if (!n || !SetSize(n))
    return false;

  return ParseArray(m_pBuf, m_nSize, pNode);
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::ParseTextArray(const char *szText)
{
  icUInt32Number n = ParseTextCount(szText);

  if (n) {
    if (!SetSize(n))
      return false;

    return ParseText(m_pBuf, m_nSize, szText)==m_nSize;
  }

  return false;
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::ParseTextArray(xmlNode *pNode)
{
  if (pNode->children && pNode->children->type==XML_TEXT_NODE) {
    return ParseTextArray((const char*)pNode->children->content);
  }
  return false;
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::ParseTextArrayNum(const char *szText, size_t num, std::string &parseStr)
{
  icUInt32Number n = ParseTextCountNum(szText, num, parseStr);
  if (n) {	  
    if (!SetSize(n))
      return false;
    return ParseText(m_pBuf, m_nSize, szText)==m_nSize;
  }

  return false;
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::DumpArray(std::string &xml, std::string blanks, T *buf, icUInt32Number nBufSize, 
                                          icConvertType nType,  icUInt8Number nColumns)
{
  const size_t strSize = 200;
  char str[strSize];

  if (!nColumns) nColumns = 1;
  icUInt32Number i;

  for (i=0; i<nBufSize; i++) {
    if (!(i%nColumns)) {
      xml += blanks;
    }
    else {
      xml += " ";
    }

    switch (Tsig) {
      case icSigUInt8ArrayType:
        switch (nType) {
          case icConvert8Bit:
          default:
            snprintf(str, strSize, "%u", (icUInt8Number)buf[i]);
            break;

          case icConvert16Bit:
            snprintf(str, strSize, "%u", (icUInt16Number)((icFloatNumber)buf[i] * 65535.0 / 255.0 + 0.5));
            break;

          case icConvertFloat:
            snprintf(str, strSize, icXmlFloatFmt, (icFloatNumber)buf[i] / 255.0);
            break;
        }
        break;

      case icSigUInt16ArrayType:
        switch (nType) {
          case icConvert8Bit:
            snprintf(str, strSize, "%u", (icUInt16Number)((icFloatNumber)buf[i] * 255.0 / 65535.0 + 0.5));
            break;

          case icConvert16Bit:
          default:
            snprintf(str, strSize, "%u", (icUInt16Number)buf[i]);
            break;

          case icConvertFloat:
            snprintf(str, strSize, icXmlFloatFmt, (icFloatNumber)buf[i] / 65535.0);
            break;
        }
        break;

      case icSigUInt32ArrayType:
        snprintf(str, strSize, "%u", (icUInt32Number)buf[i]);
        break;
      
      case icSigUInt64ArrayType:
        // unused
        break;
        
      case icSigFloatArrayType:
      case icSigFloat32ArrayType:
      case icSigFloat64ArrayType:
        switch (nType) {
          case icConvert8Bit:
            snprintf(str, strSize, "%u", (icUInt8Number)(buf[i] * 255.0 + 0.5));
            break;

          case icConvert16Bit:
            snprintf(str, strSize, "%u", (icUInt16Number)(buf[i] * 65535.0 + 0.5));
            break;

          case icConvertFloat:
          default:
            snprintf(str, strSize, icXmlFloatFmt, (icFloatNumber)buf[i]);
        }
        break;
    }
    xml += str;
    if (i%nColumns == nColumns-1) {
      xml += "\n";
    }
  }

  if (i%nColumns) {
    xml += "\n";
  }

  return true;
}

// for multi-platform support
// replaced "_inline" with "inline"
static inline bool icIsNumChar(char c)
{
  if ((c>='0' && c<='9') || c=='.' || c=='+' || c=='-' || c=='e' || c == 'n' || c == 'a')
    return true;
  return false;
}

// function used when checking contents of a file
// count the number of entries.
template <class T, icTagTypeSignature Tsig>
icUInt32Number CIccXmlArrayType<T, Tsig>::ParseTextCountNum(const char *szText, size_t num, std::string &parseStr)
{
  icUInt32Number n = 0;
  bool bInNum = false;
  //icUInt32Number count = 1;
  
  //while (*szText) {
  for (icUInt32Number i=0; i<num; i++) {
    if (icIsNumChar(*szText)) {
      if (!bInNum) {
        bInNum = true;
      }
    }
    else if (bInNum && !strncmp(szText, "#QNAN", 5)) { //Handle 1.#QNAN000 (non a number)
      i+=4;
      szText+=4;
    }
    // an invalid character is encountered (not digit and not space)
    else if ( i <= num && !isspace(*szText) ) {
      const size_t lineSize = 100;
      char line[lineSize];
      snprintf(line, lineSize, "Data '%c' in position %d is not a number. ", *szText, i);
      parseStr += line;
      return false;
    }
    else if (bInNum) { //char is a space
      n++;
      bInNum = false;
    }
    szText++;
    //count++;
  }
  if (bInNum) {
    n++;
  }

  return n;
}

template <class T, icTagTypeSignature Tsig>
icUInt32Number CIccXmlArrayType<T, Tsig>::ParseTextCount(const char *szText)
{
  icUInt32Number n = 0;
  bool bInNum = false;

  while (*szText) {
    if (icIsNumChar(*szText)) {
      if (!bInNum) {
        bInNum = true;
      }
    }
    else if (bInNum && !strncmp(szText, "#QNAN", 5)) { //Handle 1.#QNAN000 (non a number)
      szText+=4;
    }
    else if (bInNum) {
      n++;
      bInNum = false;
    }
    szText++;
  }
  if (bInNum) {
    n++;
  }

  return n;
}

// because VisualC has some really bad macros and doesn't test with standard templates
#undef max

// clip the input value to the valid output range
template<typename T, typename F>
T clipTypeRange( const F &input )
{
  if (input > std::numeric_limits<T>::max())
    return std::numeric_limits<T>::max();
  if (input < std::numeric_limits<T>::lowest()) // not min, which is a positive small number for floating point
    return std::numeric_limits<T>::lowest();
  if ( !std::numeric_limits<F>::is_integer && isnan(input) )
    return T(0);    // flush NaN to zero
  return T(input);  // passed all the checks, just cast it
}

// special case when types are equal
double clipTypeRange( const double &input )
{
  return input;
}

// special case when types are equal
float clipTypeRange( const float &input )
{
  return input;
}

template <class T, icTagTypeSignature Tsig>
icUInt32Number CIccXmlArrayType<T, Tsig>::ParseText(T* pBuf, icUInt32Number nSize, const char *szText)
{	
  icUInt32Number n = 0, b = 0;
  bool bInNum = false;
  char num[256] = {0};

  while ( (n < nSize) && *szText ) {
    if (icIsNumChar(*szText)) {
      if (!bInNum) {
        bInNum = true;
        b=0;
      }
      num[b] = *szText;
 
      if (b+2<sizeof(num))
        b++;
    }
    else if (bInNum) {
      num[b] = 0;
      if (!strncmp(num, "nan", 3) || !strncmp(num, "-nan", 4)) {
        pBuf[n] = (T)nanf(num);
      }
      else {
        pBuf[n] = clipTypeRange<T>(atof(num));  // clip input to valid output range
      }
      n++;
      bInNum = false;
    }
    szText++;
  }
  if ( bInNum && (n < nSize) ) {
    num[b] = 0;
    if (!strncmp(num, "nan", 3) || !strncmp(num, "-nan", 4)) {
      pBuf[n] = (T)nanf(num);
    }
    else {
      pBuf[n] = clipTypeRange<T>(atof(num));  // clip input to valid output range
    }
    n++;
  } 

  return n;
}

template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::ParseArray(T* pBuf, icUInt32Number nSize, xmlNode *pNode)
{
  icUInt32Number n;
  
  if (!pNode)
    return false;
  
  if (Tsig==icSigFloatArrayType) {
    n = icXmlNodeCount(pNode, "f");

    if (!n) {
      if (pNode->type!=XML_TEXT_NODE || !pNode->content)
        return false;

      n = ParseTextCount((const char*)pNode->content);
      if (!n || n>nSize)
        return false;

      ParseText(pBuf, n, (const char*)pNode->content);
    }
    else {
      if (n>nSize)
        return false;

      icUInt32Number i;
      for (i=0; i<nSize && pNode; pNode = pNode->next) {
        if (pNode->type == XML_ELEMENT_NODE &&
          !icXmlStrCmp(pNode->name, "f") &&
          pNode->children &&
          pNode->children->content) {
            float f;
            sscanf((const char *)(pNode->children->content), "%f", &f);
            pBuf[i] = (T)f;
            i++;
        }
      }
    }
  }
  else {
    n = icXmlNodeCount(pNode, "n");

    if (!n) {
      if (pNode->type!=XML_TEXT_NODE || !pNode->content)
        return false;

      n = ParseTextCount((const char *)pNode->content);
      if (!n || n>nSize)
        return false;

      n = ParseText(pBuf, n, (const char *)pNode->content);
    }
    else {
      if (n>nSize)
        return false;

      icUInt32Number i;
      for (i=0; i<nSize && pNode; pNode = pNode->next) {
        if (pNode->type == XML_ELEMENT_NODE &&
          !icXmlStrCmp(pNode->name, "n") &&
          pNode->children &&
          pNode->children->content) {
            pBuf[i] = (T)atol((const char *)(pNode->children->content));
            i++;
        }
      }
    }
  }
  return nSize==n;
}


template <class T, icTagTypeSignature Tsig>
bool CIccXmlArrayType<T, Tsig>::SetSize(icUInt32Number nSize)
{
  if (m_pBuf) {
    free(m_pBuf);
  }
  m_pBuf = (T*)malloc(nSize * sizeof(T));
  if (!m_pBuf) {
    m_nSize = 0;
    return false;
  }
  m_nSize = nSize;

  return true;
}

//Make sure typedef classes get built
template class CIccXmlArrayType<icUInt8Number, icSigUInt8ArrayType>;
template class CIccXmlArrayType<icUInt16Number, icSigUInt16ArrayType>;
template class CIccXmlArrayType<icUInt32Number, icSigUInt32ArrayType>;
template class CIccXmlArrayType<icUInt64Number, icSigUInt64ArrayType>;
template class CIccXmlArrayType<icFloatNumber, icSigFloatArrayType>;
template class CIccXmlArrayType<icFloat32Number, icSigFloat32ArrayType>;
template class CIccXmlArrayType<icFloat64Number, icSigFloat64ArrayType>;


icRenderingIntent icGetRenderingIntentValue (const icChar *szRenderingIntent)
{
  if (!strcmp(szRenderingIntent, "Perceptual"))
	  return icPerceptual;
  else if (!strcmp(szRenderingIntent, "Media-relative colorimetric"))
	  return icRelativeColorimetric;
  else if (!strcmp(szRenderingIntent, "Saturation"))
	  return icSaturation;
  else if (!strcmp(szRenderingIntent, "ICC-absolute colorimetric"))
	  return icAbsoluteColorimetric;
  
  return icPerceptual;
}

const icChar* icGetTagSigTypeName(icTagTypeSignature tagTypeSig)
{
  const icChar *rv = CIccTagCreator::GetTagTypeSigName(tagTypeSig);
  
  // if  the tag signature is not found, it is a custom type.
  if (!rv)
    return "PrivateType";

  return rv;
}

icTagTypeSignature icGetTypeNameTagSig(const icChar* szTagType)
{	
  return CIccTagCreator::GetTagTypeNameSig(szTagType);

}

const icChar* icGetTagSigName(icTagSignature tagTypeSig)
{
  const icChar *rv = CIccTagCreator::GetTagSigName(tagTypeSig);

  // if  the tag signature is not found, it is a custom type.
  if (!rv)
    return "PrivateTag";

  return rv;
}

icTagSignature icGetTagNameSig(const icChar *szName)
{
  return CIccTagCreator::GetTagNameSig(szName);
}


icStandardObserver icGetNamedStandardObserverValue(const icChar* str)
{
  if (!strcmp(str, "Unknown observer"))
	  return icStdObsUnknown;

  if (!strcmp(str, "CIE 1931 (two degree) standard observer"))
	  return icStdObs1931TwoDegrees;

  if (!strcmp(str, "CIE 1964 (ten degree) standard observer"))
	  return icStdObs1964TenDegrees;   

  return icStdObsCustom;
}


icMeasurementGeometry icGeNamedtMeasurementGeometryValue(const icChar* str)
{
  if (!strcmp(str, "Geometry Unknown"))
  	return icGeometryUnknown;
 
  if (!strcmp(str, "Geometry 0-45 or 45-0"))
	  return icGeometry045or450;

  if (!strcmp(str, "Geometry 0-d or d-0"))
	  return icGeometry0dord0;

  if (!strcmp(str, "Max Geometry"))
	  return icMaxEnumGeometry;  

  return icGeometryUnknown;
}

icMeasurementFlare icGetNamedMeasurementFlareValue(const icChar * str)
{
  if (!strcmp(str, "Flare 0"))
	  return icFlare0;
 
  if (!strcmp(str, "Flare 100"))
	  return icFlare100;
  
  if (!strcmp(str, "Max Flare"))
	  return icMaxEnumFlare;

  return icFlare0;
}

icIlluminant icGetIlluminantValue(const icChar* str)
{
  if (!strcmp(str, "Illuminant Unknown"))
	  return icIlluminantUnknown;
 
  if (!strcmp(str, "Illuminant D50") || !strcmp(str, "D50"))  
	  return icIlluminantD50;  

  if (!strcmp(str, "Illuminant D65") || !strcmp(str, "D65"))
	  return icIlluminantD65;

  if (!strcmp(str, "Illuminant D93") || !strcmp(str, "D93"))
	  return icIlluminantD93;

  if (!strcmp(str, "Illuminant F2") || !strcmp(str, "F2"))
	  return icIlluminantF2;
  
  if (!strcmp(str, "Illuminant D55") || !strcmp(str, "D55"))
	  return icIlluminantD55;
  
  if (!strcmp(str, "Illuminant A") || !strcmp(str, "A"))
	  return icIlluminantA;  

  if (!strcmp(str, "Illuminant EquiPowerE") || !strcmp(str, "Illuminant E") || !strcmp(str, "E"))
	  return icIlluminantEquiPowerE;

  if (!strcmp(str, "Illuminant F8") || !strcmp(str, "F8"))
	  return icIlluminantF8; 
  
  if (!strcmp(str, "Illuminant Black Body") || !strcmp(str, "Black Body"))
    return icIlluminantBlackBody;

  if (!strcmp(str, "Illuminant Daylight") || !strcmp(str, "Daylight"))
    return icIlluminantDaylight;

  if (!strcmp(str, "Illuminant B") || !strcmp(str, "B"))
    return icIlluminantB;

  if (!strcmp(str, "Illuminant C") || !strcmp(str, "C"))
    return icIlluminantC;

  if (!strcmp(str, "Illuminant F1") || !strcmp(str, "F1"))
    return icIlluminantF1;

  if (!strcmp(str, "Illuminant F3") || !strcmp(str, "F3"))
    return icIlluminantF3;

  if (!strcmp(str, "Illuminant F4") || !strcmp(str, "F4"))
    return icIlluminantF4;

  if (!strcmp(str, "Illuminant F5") || !strcmp(str, "F5"))
    return icIlluminantF5;

  if (!strcmp(str, "Illuminant F6") || !strcmp(str, "F6"))
    return icIlluminantF6;

  if (!strcmp(str, "Illuminant F7") || !strcmp(str, "F7"))
    return icIlluminantF7;

  if (!strcmp(str, "Illuminant F9") || !strcmp(str, "F9"))
    return icIlluminantF9;

  if (!strcmp(str, "Illuminant F10") || !strcmp(str, "F10"))
    return icIlluminantF10;

  if (!strcmp(str, "Illuminant F11") || !strcmp(str, "F11"))
    return icIlluminantF11;

  if (!strcmp(str, "Illuminant F12") || !strcmp(str, "F12"))
    return icIlluminantF12;
  
  return icIlluminantCustom;
}

const icChar* icGetStandardObserverName(icStandardObserver str)
{
  switch (str) {
  case icStdObsUnknown:
    return "Unknown Observer";

  case icStdObs1931TwoDegrees:
    return "CIE 1931 standard colorimetric observer";

  case icStdObs1964TenDegrees:
    return "CIE 1964 standard colorimetric observer";

  default:    
    return "Unknown Observer";
  }
}

icDateTimeNumber icGetDateTimeValue(const icChar* str)
{
	unsigned int day=0, month=0, year=0, hours=0, minutes=0, seconds=0;
	icDateTimeNumber dateTime = {};	

  if (!stricmp(str, "now")) {
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    year = timeinfo->tm_year+1900;
    month = timeinfo->tm_mon+1;
    day = timeinfo->tm_mday;
    hours = timeinfo->tm_hour;
    minutes = timeinfo->tm_min;
    seconds = timeinfo->tm_sec;
  }
  else {
  	sscanf(str, "%d-%02d-%02dT%02d:%02d:%02d", &year, &month, &day, &hours, &minutes, &seconds);
  }

	dateTime.year = year;
	dateTime.month = month;
	dateTime.day = day;
	dateTime.hours = hours;
	dateTime.minutes = minutes;
	dateTime.seconds = seconds;
	
	return dateTime;		
}

icUInt64Number icGetDeviceAttrValue(xmlNode *pNode)
{	
	icUInt64Number devAttr = 0;
	xmlAttr *attr = icXmlFindAttr(pNode, "ReflectiveOrTransparency");
  if (attr && !strcmp(icXmlAttrValue(attr), "transparency")) {
    devAttr |= icTransparency;
  }

	attr = icXmlFindAttr(pNode, "GlossyOrMatte");
  if (attr && !strcmp(icXmlAttrValue(attr), "matte")) {
    devAttr |= icMatte;
	}

	attr = icXmlFindAttr(pNode, "MediaPolarity");
  if (attr && !strcmp(icXmlAttrValue(attr), "negative")) {
    devAttr |= icMediaNegative;
  }

	attr = icXmlFindAttr(pNode, "MediaColour");
  if (attr && !strcmp(icXmlAttrValue(attr), "blackAndWhite")) {
		devAttr |= icMediaBlackAndWhite;
	}
	
  attr = icXmlFindAttr(pNode, "VendorSpecific");
  if (attr) {
    icUInt64Number vendor;
    sscanf(icXmlAttrValue(attr), "%llx", &vendor);
    devAttr |= vendor;
  }

	return devAttr;
}

icColorantEncoding icGetColorantValue(const icChar* str)
{
	if (!strcmp(str, "ITU-R BT.709"))  
		return icColorantITU;  

	if (!strcmp(str, "SMPTE RP145-1994"))  
		return icColorantSMPTE;  

	if (!strcmp(str, "EBU Tech.3213-E"))  
		return icColorantEBU;  

	if (!strcmp(str, "P22"))  
		return icColorantP22;  

  return icColorantUnknown;
}

icMeasurementUnitSig icGetMeasurementValue(const icChar* str)
{
	if (!strcmp(str, "Status A"))  
		return icSigStatusA;  

	if (!strcmp(str, "Status E"))  
		return icSigStatusE;  

	if (!strcmp(str, "Status I"))  
		return icSigStatusI;  

	if (!strcmp(str, "Status T"))  
		return icSigStatusT;  

	if (!strcmp(str, "Status M"))  
		return icSigStatusM;  

	if (!strcmp(str, "DIN with no polarizing filter"))  
		return icSigDN;  

	if (!strcmp(str, "DIN with polarizing filter"))  
		return icSigDNP;  

	if (!strcmp(str, "Narrow band DIN with no polarizing filter"))  
		return icSigDNN;  

	if (!strcmp(str, "Narrow band DIN with polarizing filter"))  
		return icSigDNNP; 

  return icSigStatusA;
}

const std::string icGetDeviceAttrName(icUInt64Number devAttr)
{
    const size_t lineSize = 256;
	char line[lineSize];
	std::string xml;
		
	if (devAttr & icTransparency)
		snprintf(line, lineSize, "<DeviceAttributes ReflectiveOrTransparency=\"transparency\"");
	else
		snprintf(line, lineSize, "<DeviceAttributes ReflectiveOrTransparency=\"reflective\"");
	xml += line;
 
	
	if (devAttr & icMatte)
		snprintf(line, lineSize, " GlossyOrMatte=\"matte\"");
	else
		snprintf(line, lineSize, " GlossyOrMatte=\"glossy\"");
	xml += line;
	
	
	if (devAttr & icMediaNegative)
		snprintf(line, lineSize, " MediaPolarity=\"negative\"");
	else
		snprintf(line, lineSize, " MediaPolarity=\"positive\"");
	xml += line;
	
	if (devAttr & icMediaBlackAndWhite)
		snprintf(line, lineSize, " MediaColour=\"blackAndwhite\"");
	else
		snprintf(line, lineSize, " MediaColour=\"colour\"");
	xml += line;

  icUInt64Number otherAttr = ~((icUInt64Number)icTransparency|icMatte|icMediaNegative|icMediaBlackAndWhite);

  if (devAttr & otherAttr) {
    snprintf(line, lineSize, " VendorSpecific=\"%016llx\"", devAttr & otherAttr);
    xml += line;
  }

  xml += "/>\n";

	return xml;
}

const std::string icGetHeaderFlagsName(icUInt32Number flags, bool bUsesMCS)
{
    const size_t lineSize = 256;
	char line[lineSize];
	std::string xml;	

  if (flags & icEmbeddedProfileTrue)
		snprintf(line, lineSize, "<ProfileFlags EmbeddedInFile=\"true\"");
	else
	  snprintf(line, lineSize, "<ProfileFlags EmbeddedInFile=\"false\"");
	xml += line;

  if (flags & icUseWithEmbeddedDataOnly)
		snprintf(line, lineSize, " UseWithEmbeddedDataOnly=\"true\"");
	else
		snprintf(line, lineSize, " UseWithEmbeddedDataOnly=\"false\"");
	xml += line;

  if (flags & icExtendedRangePCS) {
    snprintf(line, lineSize, " ExtendedRangePCS=\"true\"");
    xml += line;
  }

  icUInt32Number otherFlags = ~(icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly | icExtendedRangePCS);

  if (bUsesMCS) {
    if (flags & icMCSNeedsSubsetTrue)
      snprintf(line, lineSize, " MCSNeedsSubset=\"true\"");
    else
      snprintf(line, lineSize, " MCSNeedsSubset=\"false\"");
    xml += line;

    otherFlags &= ~icMCSNeedsSubsetTrue;
  }

  if (flags & otherFlags) {
    snprintf(line, lineSize, " VendorFlags=\"%08x\"", flags & otherFlags);
    xml += line;  		
  }

  xml += "/>\n";

  return xml;
}

const std::string icGetPadSpace(double value)
{
  std::string space = "";  
  if ( value >=0 && value < 10)
    space = "    ";
  if ( value >=10 && value < 100)
	space = "   ";
   if ( value >=100 && value < 1000 )
	space = "  ";

   return space;
}

