/** @file
    File:       IccTagXML.cpp

    Contains:   Implementation ICC tag XML format conversions

    Version:    V1

    Copyright:  see ICC Software License
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
#include "IccMpeXml.h"
#include "IccUtil.h"
#include "IccUtilXml.h"
#include "IccIoXml.h"
#include "IccSparseMatrix.h"
#include "IccProfileXml.h"
#include "IccStructFactory.h"
#include "IccArrayFactory.h"
#include "IccConvertUTF.h"
#include <new>     /* std::nothrow */
#include <cstring> /* C strings strcpy, memcpy ... */
#include <set>
#include <map>
#include <sstream>  // Make sure to include this header
#include <iomanip>  // Include this header for setw and setfill
#include <cmath>    // std::isfinite for NaN/Inf-safe float->int casts

typedef  std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

// Parse a non-negative integer XML attribute value.
//
// atoi() returns a signed int, but the ParseXml handlers below store these
// attributes into unsigned icUIntNN members/locals.  A negative attribute
// therefore used to wrap to a huge unsigned value -- either implicitly (caught
// by UBSan's implicit-integer-sign-change, e.g. #1342/#1343) or silently behind
// an explicit (icUIntNN) cast (invisible to UBSan).  This helper centralizes
// the fix: any negative (invalid) input is floored to 0 so the stored value is
// always well-defined.
//
// Usage: replace `atoi(icXmlAttrValue(...))` with
// `icXmlAttrToUInt(icXmlAttrValue(...))`.  The helper returns the widest
// unsigned type (icUInt32Number); callers that target an 8- or 16-bit member
// keep their explicit (icUInt8Number)/(icUInt16Number) cast exactly as before,
// which both narrows the value and suppresses the implicit-integer-truncation
// check, so no behavior other than the negative-wrap is changed (#1346).
static icUInt32Number icXmlAttrToUInt(const char *szValue)
{
  int nValue = atoi(szValue);
  return nValue < 0 ? 0u : (icUInt32Number)nValue;
}

// Reads the "steps" attribute of a <Wavelengths> element into the
// icUInt16Number field of an icSpectralRange.
//
// The count is what tells every consumer how many samples the range holds, so
// narrowing it modulo 65536 substitutes a different spectrum rather than
// failing: steps="65937" was stored -- and written back out on save -- as 401
// (#1909). This is the same defect #1908 fixed for the MPE elements, at the
// tag-level readers those elements sit beside.
//
// The attribute defaults to "0" rather than "" so that an absent "steps" still
// yields a zero-step range exactly as it did before. Only a value that is
// present and cannot be represented stops the parse; existing documents that
// omit the attribute keep loading.
static bool icXmlParseSpectralSteps(xmlNode *pNode, icUInt16Number &steps,
                                    const char *szRangeName, std::string &parseStr)
{
  if (!icXmlParseU16(icXmlAttrValue(pNode, "steps", "0"), steps)) {
    parseStr += std::string("Invalid ") + szRangeName + " Wavelengths steps\n";
    return false;
  }

  return true;
}

static void icXmlCopyFixedString(char *dst, size_t dstSize, const char *src)
{
  if (!dst || !dstSize) {
    return;
  }
  if (!src) {
    src = "";
  }

  size_t len = strlen(src);
  if (len >= dstSize) {
    len = dstSize - 1;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}


bool CIccTagXmlUnknown::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  xml += blanks + "<UnknownData>\n";
  icXmlDumpHexData(xml, blanks+" ", m_pData, m_nSize);
  xml += blanks + "</UnknownData>\n";

  return true;
}


bool CIccTagXmlUnknown::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  if (pNode) {
    const char *tagType = icXmlAttrValue(pNode->parent, "type");
    if (tagType) {
      m_nType = (icTagTypeSignature)icGetSigVal(tagType);
    }
  }

  pNode = icXmlFindNode(pNode, "UnknownData");

  if (pNode && pNode->children && pNode->children->content) {
    m_nSize = icXmlGetHexDataSize((const icChar*)pNode->children->content);

    delete [] m_pData;
    m_pData = NULL;

    if (m_nSize) {
      m_pData = new (std::nothrow) icUInt8Number[m_nSize];
      if (!m_pData)
        return false;

      if (icXmlGetHexData(m_pData, (const icChar*)pNode->children->content, m_nSize)!=m_nSize)
        return false;
    }
    return true;
  }
  return false;
}

static bool isTextLegalCDATA( const char *szText )
{
  // an empty string is legal, and happens in real profiles.
  if (szText[0] == 0)
    return true;

  for (const unsigned char *ptr = (const unsigned char *)szText; *ptr; ptr++) {
    if (*ptr < 0x20 && *ptr != '\n' && *ptr != '\t')
      return false;
  }

  // scan for XML tags that would make this an invalid text block
  if ( strstr(szText, "]]>") )
    return false;

  // and last check for illegal UTF8 values
  size_t length = strlen(szText);
  if ( isLegalUTF8String( (const UTF8 *)szText, (int)length ) == 0 )
    return false;

  return true;
}

static bool icXmlDumpTextData(std::string &xml, std::string blanks, const char *szText, bool bConvert=true)
{
  if ( !isTextLegalCDATA(szText) ) {
    xml += blanks + "<HexTextData>";
    icXmlDumpHexData(xml, blanks+" ", (void*)szText, (icUInt32Number)strlen(szText));
    xml += blanks + "</HexTextData>\n";
  }
  else {
    std::string buf;

    xml += blanks + "<TextData>";
    xml += "<![CDATA[";
    if (bConvert)
      xml += icAnsiToUtf8(buf, szText);
    else
      xml += szText;
    xml += "]]></TextData>\n"; 
  }

  return true;
}

static void icXmlDumpLocalizedText(std::string &xml, const std::string &blanks,
                                   const char *elementName, const char *languageCountry,
                                   const std::string &text)
{
  std::string fix;

  xml += blanks + "<";
  xml += elementName;
  xml += " LanguageCountry=\"";
  xml += icFixXml(fix, languageCountry);
  xml += "\">";

  if (text.find('\0') != std::string::npos || !isTextLegalCDATA(text.c_str())) {
    xml += "\n";
    xml += blanks + "  <HexTextData>";
    icXmlDumpHexData(xml, blanks + "   ", (void*)text.data(), text.size());
    xml += blanks + "  </HexTextData>\n";
    xml += blanks;
  }
  else {
    xml += "<![CDATA[";
    xml += icFixXml(fix, text.c_str());
    xml += "]]>";
  }

  xml += "</";
  xml += elementName;
  xml += ">\n";
}

static xmlAttr *icXmlFindLanguageCountryAttr(xmlNode *pNode)
{
  xmlAttr *pAttr = icXmlFindAttr(pNode, "LanguageCountry");
  if (!pAttr)
    pAttr = icXmlFindAttr(pNode, "languageCountry");
  if (!pAttr)
    pAttr = icXmlFindAttr(pNode, "LanguangeCountry");
  return pAttr;
}

static bool icXmlParseLocalizedText(xmlNode *pNode, std::string &text)
{
  bool haveText = false;
  text.clear();

  for (xmlNode *pText = pNode ? pNode->children : NULL; pText; pText = pText->next) {
    if (pText->type == XML_ELEMENT_NODE && !icXmlStrCmp(pText->name, "HexTextData") &&
        pText->children && pText->children->content) {
      CIccUInt8Array buf;
      icUInt32Number hexSize = icXmlGetHexDataSize((const icChar*)pText->children->content);
      if (!buf.SetSize(hexSize+2) ||
          icXmlGetHexData(buf.GetBuf(), (const icChar*)pText->children->content, hexSize)!=hexSize)
        return false;

      uint8_t *strPtr = buf.GetBuf();
      strPtr[hexSize] = 0;
      strPtr[hexSize+1] = 0;
      text.assign((char*)strPtr, hexSize);
      return true;
    }
    if (!haveText && (pText->type == XML_TEXT_NODE || pText->type == XML_CDATA_SECTION_NODE)) {
      text = (const char*)pText->content;
      haveText = true;
    }
  }

  return haveText;
}

static bool icXmlSetLocalizedUtf8(CIccTagMultiLocalizedUnicode &tag,
                                  const std::string &text,
                                  icLanguageCode langCode,
                                  icCountryCode countryCode)
{
  CIccUTF16String wstr;
  const char *src = text.empty() ? "" : text.data();
  if (!wstr.FromUtf8(src, text.size()))
    return false;
  if (wstr.Size() > (size_t)((icUInt32Number)-1))
    return false;
  return tag.SetText(wstr.c_str(), (icUInt32Number)wstr.Size(), langCode, countryCode);
}

bool CIccTagXmlText::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  return icXmlDumpTextData(xml, blanks, m_szText);
}

bool CIccTagXmlUtf8Text::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  return icXmlDumpTextData(xml, blanks, (icChar*)m_szText, false);
}

bool CIccTagXmlZipUtf8Text::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  xml += blanks + "<HexCompressedData>\n";
  icXmlDumpHexData(xml, blanks+" ", m_pZipBuf, m_nBufSize);
  xml += blanks + "</HexCompressedData>\n";

  return true;
}

bool CIccTagXmlZipXml::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  xml += blanks + "<HexCompressedData>\n";
  icXmlDumpHexData(xml, blanks+" ", m_pZipBuf, m_nBufSize);
  xml += blanks + "</HexCompressedData>\n";

  return true;
}

bool CIccTagXmlUtf16Text::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string buf;
  return icXmlDumpTextData(xml, blanks, GetText(buf), false);
}

static bool icXmlParseTextString(xmlNode *pNode, std::string &parseStr, std::string &str, bool bConvert = true)
{
  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexTextData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        icUInt32Number hexSize = icXmlGetHexDataSize((const icChar*)pNode->children->content);
        if (!buf.SetSize(hexSize+2) ||
          icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, hexSize)!=hexSize)
          return false;
        
        // make sure the string is NULL terminated, even for UTF8
        uint8_t *strPtr = buf.GetBuf();
        strPtr[hexSize] = 0;
        strPtr[hexSize+1] = 0;
        
        str += (char*)strPtr;
      }      
      else if (!icXmlStrCmp(pNode->name, "TextData") ) {
        std::string buf;
        const icChar *filename = icXmlAttrValue(pNode, "File");

        // file exists
        if (filename && filename[0]) {        
          CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");        
          if (!file){          
            parseStr += "Error! - File '";
            parseStr += filename;
            parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
            delete file;
            return false;
          }

          icUInt32Number fileLength;
          if (!icXmlValidateFileCount(file->GetLength(), fileLength, parseStr, filename)) {
            delete file;
            return false;
          }
          char *ansiStr = (char *)malloc((size_t)fileLength + 1);

          if (!ansiStr) {
            perror("Memory Error");
            parseStr += "'";
            parseStr += filename;
            parseStr += "' may not be a valid text file.\n";
            delete file;
            return false;
          }
          // read the contents of the file
          if (file->ReadLine(ansiStr, fileLength)!=fileLength) {
            parseStr += "Error while reading file '";
            parseStr += filename;
            parseStr += "'. Size read is not equal to file length. File may not be a valid text file.\n";
            free(ansiStr);
            delete file;             
            return false;
          }
          ansiStr[fileLength] = '\0';
          // convert utf8 (xml format) to ansi (icc format)
          if (bConvert)
            icUtf8ToAnsi(buf, ansiStr);
          else
            buf = ansiStr;
          free(ansiStr);
          delete file;
        }
        // file does not exist
        else if (pNode->children && pNode->children->content){
          if (bConvert)
            icUtf8ToAnsi(buf, (const icChar*)pNode->children->content);      
          else
            buf = (const icChar*)pNode->children->content;
        }      
        str += buf;      
      } 
    }
    pNode = pNode->next; 
  }

  return true;
}

bool CIccTagXmlText::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string outStr;
  if( !icXmlParseTextString(pNode, parseStr, outStr) )
    return false;
  
  // even an empty string is a valid string
  SetText(outStr.c_str());
  return true;
}

bool CIccTagXmlUtf8Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string outStr;
  if( !icXmlParseTextString(pNode, parseStr, outStr, false) )
    return false;
  
  // even an empty string is a valid string
  SetText(outStr.c_str());
  return true;
}

bool CIccTagXmlZipUtf8Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // The scan below consumes pNode: it only falls out of the loop once pNode is
  // NULL (the <HexCompressedData> match returns from inside the loop), so the
  // plain-text fallback after it cannot reuse the walking pointer.  Keep the
  // head of the sibling list for icXmlParseTextString(), which does its own
  // walk and would otherwise be handed a guaranteed-NULL list.
  xmlNode *pFirstNode = pNode;

  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexCompressedData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        if (!buf.SetSize(icXmlGetHexDataSize((const icChar*)pNode->children->content)) ||
            icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, buf.GetSize())!=buf.GetSize())
          return false;
  
        AllocBuffer(buf.GetSize());
        if (m_nBufSize && m_pZipBuf) {
          memcpy(m_pZipBuf, buf.GetBuf(), m_nBufSize);
        }
        return true;
      }      
    }
    pNode = pNode->next; 
  }

  std::string outStr;
  if( !icXmlParseTextString(pFirstNode, parseStr, outStr, false) )
    return false;

  return SetText(outStr.c_str());
}

bool CIccTagXmlZipXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // Same consumed-pointer problem as CIccTagXmlZipUtf8Text::ParseXml above; see
  // the note there.  This is a separate copy of the code rather than a shared
  // helper, so it needed the same correction.
  xmlNode *pFirstNode = pNode;

  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexCompressedData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        if (!buf.SetSize(icXmlGetHexDataSize((const icChar*)pNode->children->content)) ||
          icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, buf.GetSize())!=buf.GetSize())
          return false;

        AllocBuffer(buf.GetSize());
        if (m_nBufSize && m_pZipBuf) {
          memcpy(m_pZipBuf, buf.GetBuf(), m_nBufSize);
        }
        return true;
      }      
    }
    pNode = pNode->next; 
  }

  std::string outStr;
  if( !icXmlParseTextString(pFirstNode, parseStr, outStr, false) )
    return false;

  return SetText(outStr.c_str());
}

bool CIccTagXmlUtf16Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string outStr;
  if( !icXmlParseTextString(pNode, parseStr, outStr, false) )
    return false;

  // even an empty string is a valid string
  SetText(outStr.c_str());
  return true;
}

bool CIccTagXmlTextDescription::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
    std::string fix;
    std::string buf;
    const size_t dataSize = 128;
    char data[dataSize];  // Adjust the size as needed
    std::string datastr;

    icXmlDumpTextData(xml, blanks, m_szText);

    // Added support for <![CData[Insert Text here]]> for Unicode
    if (m_uzUnicodeText[0]) {
        if (m_nUnicodeLanguageCode == 0)
            buf = "<Unicode>";
        else {
            icGetSigStr(data, dataSize, m_nUnicodeLanguageCode);
            icFixXml(fix, data);
            buf = "<Unicode LanguageCode=\"" + fix + "\">";
        }
        xml += blanks + buf;

        icUtf16ToUtf8(datastr, m_uzUnicodeText);
        icFixXml(fix, datastr.c_str());
        buf = "<![CDATA[" + fix + "]]></Unicode>\n";
        xml += buf;
    }

    if (m_nScriptSize) {
        buf = "<MacScript ScriptCode=\"" + std::to_string(m_nScriptCode) + "\">";
        xml += blanks + buf;

        std::stringstream ss;
        for (int i = 0; i < m_nScriptSize; i++) {
            ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
               << static_cast<int>(static_cast<unsigned char>(m_szScriptText[i]));
        }
        buf = ss.str();

        xml += buf;
        xml += "</MacScript>\n";
    }

    return true;
}


bool CIccTagXmlTextDescription::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  pNode = icXmlFindNode(pNode, "TextData");

  // support for reading desc, dmmd, and dmnd tags from file.
  // if (!pNode || !pNode->children)
  if (!pNode)
    return false;

  // search for "file" attribute in <TextDescription/> tag
  const icChar *filename = icXmlAttrValue(pNode, "File");

  // file exists
  if (filename && filename[0]) {
    CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");

    if (!file){
      parseStr += "Error! - File '";
      parseStr += filename;
      parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
      delete file;
      return false;
    }

    icUInt32Number fileLength;
    if (!icXmlValidateFileCount(file->GetLength(), fileLength, parseStr, filename)) {
      delete file;
      return false;
    }
    // +1 for the NUL terminator. icUtf8ToAnsi(buf) and CIccUTF16String(buf)
    // both treat `buf` as a C-string and walk it with strlen-like logic;
    // without the extra byte plus the explicit terminator below, those
    // functions read past the allocation until they happen to find a
    // zero byte in adjacent heap.
    char *buf = (char *)malloc((size_t)fileLength + 1);

    if (!buf) {
      perror("Memory Error");
      parseStr += "'";
      parseStr += filename;
      parseStr += "' may not be a valid text file.\n";
      delete file;
      return false;
    }
    buf[fileLength] = '\0';

    if (file->ReadLine(buf, fileLength)!=fileLength) {
      parseStr += "Error while reading file '";
      parseStr += filename;
      parseStr += "'. Size read is not equal to file length. File may not be a valid text file.\n";
      free(buf);
      delete file;             
      return false;
    }
    buf[fileLength] = '\0';

    // set ANSII string
    std::string ansiStr;    
    icUtf8ToAnsi(ansiStr, buf);

    icUInt32Number nStrSize = (icUInt32Number)ansiStr.size();
    GetBuffer(nStrSize);
    if (nStrSize) {
      memcpy(m_szText, ansiStr.c_str(), nStrSize);  
      m_nASCIISize = nStrSize + 1;
    }
    else 
      m_szText[0] = '\0';

    // set Unicode String
    CIccUTF16String wstr;
    if (!wstr.FromUtf8(buf, fileLength)) {
      parseStr += "'";
      parseStr += filename;
      parseStr += "' is not valid UTF-8.\n";
      delete file;
      return false;
    }

    nStrSize = (icUInt32Number)wstr.Size();
    m_uzUnicodeText = GetUnicodeBuffer(nStrSize);

    if (nStrSize) {
      // assign each entry in wstr to m_uzUnicodeText
      for (int i=0; i < (int) nStrSize; i++) {
        m_uzUnicodeText[i] = wstr[i];
      }  

      // include the null termintor in the string size.
      m_nUnicodeSize = nStrSize + 1;
    }
    else
      m_uzUnicodeText[0] = 0;

    // Set ScriptCode (m_szScriptText is a fixed 67-byte field per ICC v2
    // textDescriptionType; clamp source read to buf size to avoid OOB read,
    // and clamp m_nScriptSize before the icUInt8Number narrowing.)
    m_nScriptCode = 0;
    icUInt32Number nScriptCopy = fileLength < 67 ? fileLength : 67;
    icUInt32Number nScriptSize = nScriptCopy < 255 ? nScriptCopy + 1 : 255;
    m_nScriptSize = (icUInt8Number)nScriptSize;
    memset(m_szScriptText, 0, sizeof(m_szScriptText));
    if (nScriptCopy)
      memcpy(m_szScriptText, buf, nScriptCopy);

    delete file;
  }

  // file does not exist
  else {
    std::string str;
    if( !icXmlParseTextString(pNode, parseStr, str) )
      return false;
    
    icUInt32Number nSize = (icUInt32Number)str.size();
    (void) GetBuffer(nSize);        // has hidden side effects

    if (nSize) {
      memcpy(m_szText, str.c_str(), nSize);

      // include the null termintor in the string size.
      m_nASCIISize = nSize + 1;
    }
    else 
      m_szText[0] = '\0';

    Release();

    // support for automatically generating unicode and scriptcode tags if these do not 
    // exist in the XML file.
    //bool unicodeExists = false;       // used in commented out code below
    //bool scriptcodeExists = false;
    for (;pNode; pNode = pNode->next) {
      if (pNode->type==XML_ELEMENT_NODE) {
        if (!icXmlStrCmp(pNode->name, "Unicode")) {
          const icChar *pRegion = icXmlAttrValue(pNode, "LanguageCode");

          // *pRegion may not have value.
          if (pRegion && /* *pRegion && */pNode->children && pNode->children->content) {
            CIccUTF16String wstr;
            if (!wstr.FromUtf8((const char*)pNode->children->content)) {
              parseStr += "Invalid UTF-8 in Unicode text.\n";
              return false;
            }

            nSize = (icUInt32Number)wstr.Size();

            // set size of m_uzUnicodeText
            m_uzUnicodeText = GetUnicodeBuffer(nSize);
            if (nSize) {

              // assign each entry in wstr to m_uzUnicodeText
              for (int i=0; i < (int) nSize; i++) {
                m_uzUnicodeText[i] = wstr[i];
              }  

              // include the null termintor in the string size.
              m_nUnicodeSize = nSize + 1;

              //unicodeExists = true;
            }
            else
              m_uzUnicodeText[0] = 0;
          }
        }
        else if (!icXmlStrCmp(pNode->name, "MacScript")) {
          const icChar *pScript = icXmlAttrValue(pNode, "ScriptCode");

          if (pScript && *pScript) {
            icUInt32Number nCode=0;

            sscanf(pScript, "%x", &nCode);
            m_nScriptCode = (icUInt16Number)nCode;
            if (pNode->children && pNode->children->content) {
              // set m_nScriptSize as receiver the return value of icXmlGetHexData
              // no need to add 1 since the return value is already exact.
              m_nScriptSize = (icUInt8Number) icXmlGetHexData(m_szScriptText, (const char*)pNode->children->content, sizeof(m_szScriptText));
              //scriptcodeExists = true;
            }
            else
              m_szScriptText[0] = 0;
          }
        }
      }
    }
#if 0
    // automatically generate unicode tag in the profile if it does not exist
    if (!unicodeExists){    

      m_uzUnicodeText = GetUnicodeBuffer(nSize);

      if (nSize) {
        // assign each entry in wstr to m_uzUnicodeText
        for (int i=0; i < (int) nSize; i++) {
          m_uzUnicodeText[i] = str[i];
        }  

        // include the null termintor in the string size.
        m_nUnicodeSize = nSize + 1;
      }
      else
        m_uzUnicodeText[0] = 0;
    }

    // automatically generate scriptcode tag in the profile if it does not exist
    if (!scriptcodeExists){
      m_nScriptCode=0;
      m_nScriptSize = (icUInt8Number)m_nASCIISize;
      memcpy(m_szScriptText, m_szText, 67);

    }
#endif
  }

  return true;
}


bool CIccTagXmlSignature::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string fix;
  char buf[40];
  const size_t lineSize = 256;
  char line[lineSize];

  // A signatureType tag holds one four-character signature and zero is a legal
  // value for it -- the technologyTag of a profile with no recorded technology is
  // icSigUndefined, which is 0.  icGetSigStr(0) returns the literal text "NULL" as
  // a display convention, but the inverse used by ParseXml() below,
  // icGetSigVal("NULL"), packs the ASCII bytes into 0x4E554C4C.  Writing "NULL"
  // here therefore silently rewrites the tag's four payload bytes on round trip,
  // and because 0x4E554C4C is merely an unrecognised signature rather than a
  // malformed one the validator still reports the profile as valid, so the
  // corruption is invisible (#1843).  Emit an empty element for zero: ParseXml()
  // passes empty content to icGetSigVal(), which returns 0.
  snprintf(line, lineSize, "<Signature>%s</Signature>\n", m_nSig ? icFixXml(fix, icGetSigStr(buf, 40, m_nSig)) : "");

  xml += blanks + line;
  return true;
}


bool CIccTagXmlSignature::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  if ((pNode = icXmlFindNode(pNode, "Signature"))) {
    this->SetValue(icGetSigVal(pNode->children ? (const icChar*)pNode->children->content : ""));

    return true;
  }
  return false;
}


bool CIccTagXmlSpectralDataInfo::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string fix;
  char buf[40];
  const size_t lineSize = 256;
  char line[lineSize];

  snprintf(line, lineSize, "<SpectralSpace>%s</SpectralSpace>\n", icFixXml(fix, icGetColorSigStr(buf, 40, m_nSig)));
  xml += blanks + line;

  xml += blanks + "<SpectralRange>\n";
  snprintf(line, lineSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_spectralRange.start), icF16toF(m_spectralRange.end), m_spectralRange.steps);
  xml += blanks + line;
  xml += blanks + "</SpectralRange>\n";

  if (m_biSpectralRange.steps) {
    xml +=blanks +  "<BiSpectralRange>\n";
    snprintf(line, lineSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_biSpectralRange.start), icF16toF(m_biSpectralRange.end), m_biSpectralRange.steps);
    xml += blanks + line;
    xml += blanks + "</BiSpectralRange>\n";
  }

  return true;
}


bool CIccTagXmlSpectralDataInfo::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlNode *pChild;

  if (!(pChild = icXmlFindNode(pNode, "SpectralSpace"))) {
    parseStr += "No SpectralSpace section found\n";
    return false;
  }
  m_nSig = icGetSigVal(pChild->children ? (const icChar*)pChild->children->content : "");

  if (!(pChild = icXmlFindNode(pNode, "SpectralRange"))) {
    parseStr += "No SpectralRange section found\n";
    return false;
  }

  if (!(pChild = icXmlFindNode(pChild->children, "Wavelengths"))) {
    parseStr += "SpectralRange missing Wavelengths\n";
    return false;
  }

  m_spectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "start")));
  m_spectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "end")));
  if (!icXmlParseSpectralSteps(pChild, m_spectralRange.steps, "SpectralRange", parseStr))
    return false;

  pChild = icXmlFindNode(pNode, "BiSpectralRange");

  if (pChild) {
    if ((pChild = icXmlFindNode(pChild->children, "Wavelengths"))) {
      m_biSpectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "start")));
      m_biSpectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "end")));
      if (!icXmlParseSpectralSteps(pChild, m_biSpectralRange.steps, "BiSpectralRange", parseStr))
        return false;
    }
  }

  return true;
}


bool CIccTagXmlSpectralRange::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 256;
  char line[lineSize];

  xml += blanks + "<SpectralRange>\n";
  snprintf(line, lineSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_spectralRange.start), icF16toF(m_spectralRange.end), m_spectralRange.steps);
  xml += blanks + line;
  xml += blanks + "</SpectralRange>\n";

  if (m_biSpectralRange.steps) {
    xml += blanks + "<BiSpectralRange>\n";
    snprintf(line, lineSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_biSpectralRange.start), icF16toF(m_biSpectralRange.end), m_biSpectralRange.steps);
    xml += blanks + line;
    xml += blanks + "</BiSpectralRange>\n";
  }

  return true;
}


bool CIccTagXmlSpectralRange::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlNode *pChild;

  if (!(pChild = icXmlFindNode(pNode, "SpectralRange"))) {
    parseStr += "No SpectralRange section found\n";
    return false;
  }

  if (!(pChild = icXmlFindNode(pChild->children, "Wavelengths"))) {
    parseStr += "SpectralRange missing Wavelengths\n";
    return false;
  }

  m_spectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "start")));
  m_spectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "end")));
  if (!icXmlParseSpectralSteps(pChild, m_spectralRange.steps, "SpectralRange", parseStr))
    return false;

  pChild = icXmlFindNode(pNode, "BiSpectralRange");

  if (pChild) {
    if ((pChild = icXmlFindNode(pChild->children, "Wavelengths"))) {
      m_biSpectralRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "start")));
      m_biSpectralRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(pChild, "end")));
      if (!icXmlParseSpectralSteps(pChild, m_biSpectralRange.steps, "BiSpectralRange", parseStr))
        return false;
    }
  }

  return true;
}


bool CIccTagXmlNamedColor2::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  std::string fix;
  char line[bufSize];
  char buf[bufSize];
  int i, j;
  std::string str;

  // CWE-400/CWE-834: the entry walk below iterates m_nSize and, per entry, the
  // device-coord walk iterates m_nDeviceCoords. Read() caps these at
  // kMaxNamedColorEntries / kMaxNamedColorDeviceCoords and sizes the entry list
  // and each entry's deviceCoords[] to match (IccTagBasic.cpp), so on a valid
  // tag the loops never exceed the allocations; assert those same bounds here so
  // a corrupted count can't drive an unbounded serialization walk.
  const icUInt32Number kMaxNamedColorEntries = 65536;
  const icUInt32Number kMaxNamedColorDeviceCoords = 256;
  if (m_nSize > kMaxNamedColorEntries || m_nDeviceCoords > kMaxNamedColorDeviceCoords)
    return false;

  snprintf(line, bufSize, "<NamedColors VendorFlag=\"%08x\" CountOfDeviceCoords=\"%d\" DeviceEncoding=\"int16\"", (unsigned int) m_nVendorFlags, (unsigned int) m_nDeviceCoords);
  xml += blanks + line;

  snprintf(line, bufSize, " Prefix=\"%s\"", icFixXml(fix, icAnsiToUtf8(str, m_szPrefix)));
  xml += line;

  snprintf(line, bufSize, " Suffix=\"%s\">\n", icFixXml(fix, icAnsiToUtf8(str, m_szSufix)));
  xml += line;

  for (i=0; i<(int)m_nSize; i++) {
    SIccNamedColorEntry *pEntry= GetEntry(i);


    if (pEntry) {
      const char *szNodeName = NULL;
      if (m_csPCS==icSigLabData) {
        icFloatNumber lab[3];

        Lab2ToLab4(lab, pEntry->pcsCoords);
        icLabFromPcs(lab);
        szNodeName = "LabNamedColor";
        snprintf(line, bufSize, "  <%s Name=\"%s\" L=\"" icXmlFloatFmt "\" a=\"" icXmlFloatFmt "\" b=\"" icXmlFloatFmt "\"", szNodeName,
          icFixXml(fix, icAnsiToUtf8(str, pEntry->rootName)), lab[0], lab[1], lab[2]);
        xml += blanks + line;
      }
      else {
        icFloatNumber xyz[3];

        memcpy(xyz, pEntry->pcsCoords, 3*sizeof(icFloatNumber));
        icXyzFromPcs(xyz);
        szNodeName = "XYZNamedColor";
        snprintf(line, bufSize, "  <%s Name=\"%s\" X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"", szNodeName,
          icFixXml(fix, icAnsiToUtf8(str, pEntry->rootName)), xyz[0], xyz[1], xyz[2]);
        xml += blanks + line;
      }

      if (!m_nDeviceCoords) {
        xml += "/>\n";
      }
      else {
        xml += ">";
        for (j=0; j<(int)m_nDeviceCoords; j++) {
          if (j)
            xml+=" ";
          // CWE-681 (CodeQL #1902): deviceCoords come from a parsed profile and may be
          // NaN/Inf for malformed input; a non-finite float->int cast is undefined
          // behaviour. Guard finiteness and clamp to the valid 16-bit device range
          // [0,65535] before the cast (a no-op for any conformant coord in [0,1]).
          double dc = pEntry->deviceCoords[j] * 65535.0 + 0.5;
          if (!std::isfinite(dc)) dc = 0.0;
          else if (dc < 0.0) dc = 0.0;
          else if (dc > 65535.0) dc = 65535.0;
          snprintf(buf, bufSize, "%d", (int)dc);
          xml += buf;
        }
        xml += "\n";

        xml += blanks + " </" + szNodeName + ">\n";
      }
    }
  }
  xml += blanks + " </NamedColors>\n";
  return true;
}


bool CIccTagXmlNamedColor2::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "NamedColors");

  if (pNode) {
    const icChar *szVendorFlags = icXmlAttrValue(pNode, "VendorFlag");
    const icChar *szDeviceCoords = icXmlAttrValue(pNode, "CountOfDeviceCoords");
    const icChar *szDeviceEncoding = icXmlAttrValue(pNode, "DeviceEncoding");
    const icChar *szPrefix = icXmlAttrValue(pNode, "Prefix");
    const icChar *szSufix = icXmlAttrValue(pNode, "Suffix");

    if (szVendorFlags && *szVendorFlags &&
      szDeviceCoords && *szDeviceCoords &&
      szDeviceEncoding && *szDeviceEncoding &&
      szPrefix && szSufix) {
        std::string str;

        sscanf(szVendorFlags, "%x", &m_nVendorFlags);

        icXmlCopyFixedString(m_szPrefix, sizeof(m_szPrefix), icUtf8ToAnsi(str, szPrefix));

        icXmlCopyFixedString(m_szSufix, sizeof(m_szSufix), icUtf8ToAnsi(str, szSufix));

        // CountOfDeviceCoords is attacker-controlled text, and atoi() is the wrong
        // tool for it twice over: it accepts a leading '-', and it is undefined on
        // a value that does not fit an int.  The #1846/#1847 PoC carries
        // CountOfDeviceCoords="333333333333", which truncates to -1674115755 and
        // then changes value again on the way into this icUInt32Number -- the
        // "implicit conversion from type 'int' of value -1674115755 ... changed
        // the value to 2620851541" that UBSan reports here.
        //
        // icXmlAttrToUInt() (the #1346 helper near the top of this file) is
        // deliberately not used: it floors negatives to 0 but passes large
        // positive counts through untouched, and this attribute sizes an
        // allocation.  Bound it instead with the same two constants
        // CIccTagNamedColor2::Read (IccTagBasic.cpp) and CIccTagXmlNamedColor2::
        // ToXml above already enforce, so the XML reader stops being the one
        // entry point able to install counts that the binary reader and both
        // writers reject.
        const icUInt32Number kMaxNamedColorEntries = 65536;
        const icUInt32Number kMaxNamedColorDeviceCoords = 256;

        icUInt32Number newDeviceCoords = 0;
        if (!icXmlParseU32(szDeviceCoords, newDeviceCoords, kMaxNamedColorDeviceCoords))
          return false;

        icUInt32Number n = icXmlNodeCount3(pNode->children, "NamedColor", "LabNamedColor", "XYZNamedColor");
        if (n > kMaxNamedColorEntries)
          return false;

        // SetSize allocates the entry array that the loop below walks in
        // m_nColorEntrySize strides, and it returns false when that allocation
        // fails.  Discarding the result left m_NamedColor pointing at the
        // single-entry buffer the constructor allocated while the loop still
        // wrote one entry per XML node -- a heap-buffer-overflow write (CWE-787)
        // that ASan catches at the pcsCoords store below.  The bound above makes
        // the huge-count case unreachable, but the allocation can still fail, so
        // the result is checked rather than assumed.
        if (!SetSize(n, (icInt32Number)newDeviceCoords))
          return false;

        icUInt32Number i;

        SIccNamedColorEntry *pNamedColor = m_NamedColor;

        for (i=0, pNode=pNode->children; pNode; pNode=pNode->next) {
          const icChar *szName = NULL;
          if (pNode->type == XML_ELEMENT_NODE &&
            !icXmlStrCmp(pNode->name, "NamedColor") &&
            i<n) {
              szName = icXmlAttrValue(pNode, "Name");
              xmlAttr *L = icXmlFindAttr(pNode, "L");
              xmlAttr *a = icXmlFindAttr(pNode, "a");
              xmlAttr *b = icXmlFindAttr(pNode, "b");

              if (L && a && b) {
                pNamedColor->pcsCoords[0] = (icFloatNumber)atof(icXmlAttrValue(L));
                pNamedColor->pcsCoords[1] = (icFloatNumber)atof(icXmlAttrValue(a));
                pNamedColor->pcsCoords[2] = (icFloatNumber)atof(icXmlAttrValue(b));

                icLabToPcs(pNamedColor->pcsCoords);
                Lab4ToLab2(pNamedColor->pcsCoords, pNamedColor->pcsCoords);
              }
              else {
                xmlAttr *x = icXmlFindAttr(pNode, "X");
                xmlAttr *y = icXmlFindAttr(pNode, "Y");
                xmlAttr *z = icXmlFindAttr(pNode, "Z");

                if (x && y && z) {
                  pNamedColor->pcsCoords[0] = (icFloatNumber)atof(icXmlAttrValue(x));
                  pNamedColor->pcsCoords[1] = (icFloatNumber)atof(icXmlAttrValue(y));
                  pNamedColor->pcsCoords[2] = (icFloatNumber)atof(icXmlAttrValue(z));

                  icXyzToPcs(pNamedColor->pcsCoords);
                }
                else
                  return false;
              }
          }
          else if (pNode->type == XML_ELEMENT_NODE &&
            !icXmlStrCmp(pNode->name, "LabNamedColor") &&
            i < n) {
            szName = icXmlAttrValue(pNode, "Name");
            xmlAttr *L = icXmlFindAttr(pNode, "L");
            xmlAttr *a = icXmlFindAttr(pNode, "a");
            xmlAttr *b = icXmlFindAttr(pNode, "b");

            if (L && a && b) {
              pNamedColor->pcsCoords[0] = (icFloatNumber)atof(icXmlAttrValue(L));
              pNamedColor->pcsCoords[1] = (icFloatNumber)atof(icXmlAttrValue(a));
              pNamedColor->pcsCoords[2] = (icFloatNumber)atof(icXmlAttrValue(b));

              icLabToPcs(pNamedColor->pcsCoords);
              Lab4ToLab2(pNamedColor->pcsCoords, pNamedColor->pcsCoords);
            }
            else {
              return false;
            }
          }
          else if (pNode->type == XML_ELEMENT_NODE &&
            !icXmlStrCmp(pNode->name, "XYZNamedColor") &&
            i < n) {
            szName = icXmlAttrValue(pNode, "Name");
            xmlAttr *x = icXmlFindAttr(pNode, "X");
            xmlAttr *y = icXmlFindAttr(pNode, "Y");
            xmlAttr *z = icXmlFindAttr(pNode, "Z");

            if (x && y && z) {
              pNamedColor->pcsCoords[0] = (icFloatNumber)atof(icXmlAttrValue(x));
              pNamedColor->pcsCoords[1] = (icFloatNumber)atof(icXmlAttrValue(y));
              pNamedColor->pcsCoords[2] = (icFloatNumber)atof(icXmlAttrValue(z));

              icXyzToPcs(pNamedColor->pcsCoords);
            }
            else
              return false;
          }

          if (szName) {
            icXmlCopyFixedString(pNamedColor->rootName, sizeof(pNamedColor->rootName), icUtf8ToAnsi(str, szName));

            if (m_nDeviceCoords && pNode->children) {
              if (!strcmp(szDeviceEncoding, "int8")) {
                CIccUInt8Array coords;

                coords.ParseArray(pNode->children);
                icUInt8Number *pBuf = coords.GetBuf();

                // CWE-400/CWE-834: clamp the field to the same load-time bound as
                // the float branch below (deviceCoords[] is sized to it) so a
                // corrupted count can't drive an out-of-range copy.  All three
                // encoding branches share the single kMaxNamedColorDeviceCoords
                // declared at the top of this parse block -- the same bound
                // CountOfDeviceCoords was accepted against above -- rather than
                // each redeclaring its own, which would shadow it (-Wshadow is
                // -Werror in the strict build profiles).
                icUInt32Number nDevCoords = (m_nDeviceCoords > kMaxNamedColorDeviceCoords)
                                              ? kMaxNamedColorDeviceCoords : m_nDeviceCoords;
                icUInt32Number j;
                for (j = 0; j < nDevCoords && j < coords.GetSize(); j++) {
                  pNamedColor->deviceCoords[j] = (icFloatNumber)pBuf[j] / 255.0f;
                }
              }
              else if (!strcmp(szDeviceEncoding, "int16")) {
                CIccUInt16Array coords;

                coords.ParseArray(pNode->children);
                icUInt16Number *pBuf = coords.GetBuf();

                // CWE-400/CWE-834: clamp the field to the same load-time bound as
                // the float branch below (deviceCoords[] is sized to it) so a
                // corrupted count can't drive an out-of-range copy.  Shares the
                // single kMaxNamedColorDeviceCoords declared at the top of this
                // parse block, as the int8 branch above does.
                icUInt32Number nDevCoords = (m_nDeviceCoords > kMaxNamedColorDeviceCoords)
                                              ? kMaxNamedColorDeviceCoords : m_nDeviceCoords;
                icUInt32Number j;
                for (j = 0; j < nDevCoords && j < coords.GetSize(); j++) {
                  pNamedColor->deviceCoords[j] = (icFloatNumber)pBuf[j] / 65535.0f;
                }
              }
              else if (!strcmp(szDeviceEncoding, "float")) {
                CIccFloatArray coords;

                coords.ParseArray(pNode->children);
                icFloatNumber *pBuf = coords.GetBuf();

                // CWE-400/CWE-834: m_nDeviceCoords is capped at
                // kMaxNamedColorDeviceCoords on load and sizes deviceCoords[];
                // clamp to that bound so a corrupted count can't drive an
                // unbounded or out-of-range copy.  Shares the single
                // kMaxNamedColorDeviceCoords declared at the top of this parse
                // block, as the int8/int16 branches above do.
                icUInt32Number nDevCoords = (m_nDeviceCoords > kMaxNamedColorDeviceCoords)
                                              ? kMaxNamedColorDeviceCoords : m_nDeviceCoords;
                icUInt32Number j;
                for (j = 0; j < nDevCoords && j < coords.GetSize(); j++) {
                  pNamedColor->deviceCoords[j] = (icFloatNumber)pBuf[j];
                }
              }
              else
                return false;
            }

            i++;
            pNamedColor = (SIccNamedColorEntry*)((icChar*)pNamedColor + m_nColorEntrySize);
          }
        }
        return i==n;
    }
  }
  return false;
}


bool CIccTagXmlXYZ::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  int i;

  for (i=0; i<(int)m_nSize; i++) {
    snprintf(buf, bufSize, "<XYZNumber X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n", (float)icFtoD(m_XYZ[i].X),
      (float)icFtoD(m_XYZ[i].Y),
      (float)icFtoD(m_XYZ[i].Z));
    xml += blanks + buf;
  }
  return true;
}


bool CIccTagXmlXYZ::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  icUInt32Number n = icXmlNodeCount(pNode, "XYZNumber");

  if (n) {
    icUInt32Number i;
    SetSize(n);

    for (i=0; pNode; pNode=pNode->next) {
      if (pNode->type == XML_ELEMENT_NODE &&
        !icXmlStrCmp(pNode->name, "XYZNumber") &&
        i<n) {
          xmlAttr *x = icXmlFindAttr(pNode, "X");
          xmlAttr *y = icXmlFindAttr(pNode, "Y");
          xmlAttr *z = icXmlFindAttr(pNode, "Z");

          if (x && y && z) {
            m_XYZ[i].X = icDtoF((icFloatNumber)atof(icXmlAttrValue(x)));
            m_XYZ[i].Y = icDtoF((icFloatNumber)atof(icXmlAttrValue(y)));
            m_XYZ[i].Z = icDtoF((icFloatNumber)atof(icXmlAttrValue(z)));
            i++;
          }
          else
            return false;
      }
    }
    return i==n;
  }
  return false;
}


bool CIccTagXmlChromaticity::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  int i;

  CIccInfo info;
  snprintf(buf, bufSize, "<Colorant>%s</Colorant>\n",info.GetColorantEncoding((icColorantEncoding)m_nColorantType));
  xml += blanks + buf;

  for (i=0; i<(int)m_nChannels; i++) {
    snprintf(buf, bufSize, "  <Channel x=\"" icXmlFloatFmt "f\" y=\"" icXmlFloatFmt "\"/>\n", (float)icUFtoD(m_xy[i].x),
      (float)icUFtoD(m_xy[i].y));
    xml += blanks + buf;
  }

  return true;
}


bool CIccTagXmlChromaticity::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{

  pNode = icXmlFindNode(pNode, "Colorant");

  if (pNode)    
    m_nColorantType = icGetColorantValue(pNode->children ? (const icChar*)pNode->children->content : ""); 


  icUInt16Number n = (icUInt16Number)icXmlNodeCount(pNode, "Channel");  

  if (n) {
    icUInt32Number i;

    // SetSize() zeroes m_nChannels and returns false when it cannot allocate,
    // so ignoring the result left the loop below writing m_xy[0..n-1] through
    // a null pointer. It is the only sizing call on this path (#2094).
    if (!SetSize(n))
      return false;

    for (i=0; pNode; pNode=pNode->next) {
      if (pNode->type == XML_ELEMENT_NODE &&
        !icXmlStrCmp(pNode->name, "Channel") &&
        i<n) {
          xmlAttr *x = icXmlFindAttr(pNode, "x");
          xmlAttr *y = icXmlFindAttr(pNode, "y");

          if (x && y) {
            m_xy[i].x = icDtoUF((icFloatNumber)atof(icXmlAttrValue(x)));
            m_xy[i].y = icDtoUF((icFloatNumber)atof(icXmlAttrValue(y)));
            i++;
          }
          else
            return false;
      }
    }
    return i==n;
  }
  return false;
}


bool CIccTagXmlCicp::ToXml(std::string& xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];

  CIccInfo info;
  snprintf(buf, bufSize, "<cicpFields ColorPrimaries=\"%d\" TransferCharacteristics=\"%d\" MatrixCoefficients=\"%d\" VideoFullRangeFlag=\"%d\"/>\n",
          m_nColorPrimaries, m_nTransferCharacteristics, m_nMatrixCoefficients, m_nVideoFullRangeFlag);
  xml += blanks + buf;

  return true;
}


// Reads one cicp code point. All four are 8-bit syntax elements in ITU-T H.273
// and are stored in icUInt8Number members, so a value above 255 is not a code
// point the document could have meant. The (icUInt8Number) cast that used to
// stand here narrowed it instead and, being explicit, hid the truncation from
// UBSan too: ColorPrimaries="265" was stored -- and written back out on save --
// as 9, a different and entirely valid primary (#1909). A missing attribute is
// still defaulted to 0 by the caller, exactly as before; only a value that is
// present and unrepresentable now stops the parse.
static bool icXmlParseCicpField(xmlNode *pNode, const char *szName,
                                icUInt8Number &out, std::string &parseStr)
{
  xmlAttr *attr = icXmlFindAttr(pNode, szName);

  if (!attr) {
    out = 0;
    return true;
  }

  if (!icXmlParseU8(icXmlAttrValue(attr), out)) {
    parseStr += std::string("Invalid ") + szName + " in cicpFields\n";
    return false;
  }

  return true;
}

bool CIccTagXmlCicp::ParseXml(xmlNode* pNode, std::string& parseStr)
{

  pNode = icXmlFindNode(pNode, "cicpFields");

  if (pNode) {
    // parseStr was previously unnamed here, so this reader had no way to say why
    // it had refused a tag. It now reports through the same channel every other
    // ParseXml uses, which is what makes the rejections above visible to callers.
    if (!icXmlParseCicpField(pNode, "ColorPrimaries", m_nColorPrimaries, parseStr) ||
        !icXmlParseCicpField(pNode, "TransferCharacteristics", m_nTransferCharacteristics, parseStr) ||
        !icXmlParseCicpField(pNode, "MatrixCoefficients", m_nMatrixCoefficients, parseStr) ||
        !icXmlParseCicpField(pNode, "VideoFullRangeFlag", m_nVideoFullRangeFlag, parseStr))
      return false;
  }
  else {
    parseStr += "Cannot find cicpFields\n";
    return false;
  }

  return true;
}


bool CIccTagXmlSparseMatrixArray::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  int i, j, n;

  snprintf(buf, bufSize, "<SparseMatrixArray outputChannels=\"%d\" matrixType=\"%d\">\n", m_nChannelsPerMatrix, m_nMatrixType);
  xml += blanks + buf;

  CIccSparseMatrix mtx;
  icUInt32Number bytesPerMatrix = GetBytesPerMatrix();

  // CWE-400/CWE-834: m_nSize is bounded by the tag byte size in Read() and m_RawData
  // is allocated to m_nSize*bytesPerMatrix; assert an explicit upper limit so a
  // corrupted count can't drive an unbounded serialization walk.
  const icUInt32Number nMaxMatrices = 0xffffff;
  if (m_nSize > nMaxMatrices)
    return false;

  for (i=0; i<(int)m_nSize; i++) {
    if (!mtx.Reset(m_RawData+i*bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, true) ||
        mtx.GetNumEntries() > mtx.GetMaxEntries() ||
        !mtx.IsValid())
      return false;

    snprintf(buf, bufSize, " <SparseMatrix rows=\"%d\" cols=\"%d\">\n", mtx.Rows(), mtx.Cols());
    xml += blanks + buf;

    for (j=0; j<(int)mtx.Rows(); j++) {
      xml += blanks + "  <SparseRow>\n";
      
      n=mtx.GetNumRowColumns(j);

      xml += blanks + "   <ColIndices>\n";
      CIccUInt16Array::DumpArray(xml, blanks+"    ", mtx.GetColumnsForRow(j), n, icConvert16Bit, 8);
      xml += blanks + "   </ColIndices>\n";

      xml += blanks + "   <ColData>\n";
      CIccFloatArray::DumpArray(xml, blanks+"    ", (icFloatNumber*)(mtx.GetData()->getPtr(mtx.GetRowOffset(j))), n, icConvertFloat, 8);
      xml += blanks + "   </ColData>\n";

      xml += blanks + "  </SparseRow>\n";
    }
    xml += blanks + " </SparseMatrix>\n";
  }

  xml += blanks + "</SparseMatrixArray>\n";

  return true;
}


bool CIccTagXmlSparseMatrixArray::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  pNode = icXmlFindNode(pNode, "SparseMatrixArray");

  if (pNode) {
    xmlAttr *outputChan = icXmlFindAttr(pNode, "outputChannels");
    xmlAttr *matrixType = icXmlFindAttr(pNode, "matrixType");

    if (outputChan && matrixType) {
      // Reset() below takes the channel count as an icUInt16Number, so parsing
      // it wider and casting on the way in narrowed it modulo 65536:
      // outputChannels="66304" was stored -- and written back out on save -- as
      // 768 (#1909). It also sizes GetBytesPerMatrix(), which is what the raw
      // data buffer for every matrix in the array is allocated from.
      icUInt16Number nChannelsPerMatrix = 0;
      if (!icXmlParseU16(icXmlAttrValue(outputChan), nChannelsPerMatrix)) {
        parseStr += "Invalid SparseMatrixArray outputChannels\n";
        return false;
      }
      // icSparseMatrixType has a fixed underlying type of uint16_t, so the cast
      // narrowed the same way the outputChannels above used to: matrixType="65540"
      // became 4 -- icSparseMatrixFloat32, a fully supported type -- and the
      // profile then "parsed and saved correctly", leaving no way to tell a
      // malformed document from a valid one.  That is the #1931 shape exactly.
      // Parse into the width the enum is stored in and reject what does not fit;
      // the switch-like dispatch on m_nMatrixType below still decides which of
      // the types are actually supported.
      icUInt16Number nMatrixTypeVal = 0;
      if (!icXmlParseU16(icXmlAttrValue(matrixType), nMatrixTypeVal)) {
        parseStr += "Invalid SparseMatrixArray matrixType\n";
        return false;
      }
      icSparseMatrixType nMatrixType = (icSparseMatrixType)nMatrixTypeVal;

      xmlNode *pChild;

      int n=0;
      for (pChild = pNode->children; pChild; pChild=pChild->next) {
        if (pChild->type == XML_ELEMENT_NODE &&
            (!icXmlStrCmp(pChild->name, "SparseMatrix") || !icXmlStrCmp(pChild->name, "FullMatrix")))
          n++;

      }
      Reset(n, nChannelsPerMatrix);
      m_nMatrixType = (icSparseMatrixType)nMatrixType;

      icUInt32Number bytesPerMatrix = GetBytesPerMatrix();
      CIccSparseMatrix mtx;
      int i=0;
      for (pChild = pNode->children; pChild; pChild=pChild->next) {
        if (pChild->type == XML_ELEMENT_NODE) {
          if (!icXmlStrCmp(pChild->name, "SparseMatrix")) {
            mtx.Reset(m_RawData + i*bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, false);

            xmlAttr *rows = icXmlFindAttr(pChild, "rows");
            xmlAttr *cols = icXmlFindAttr(pChild, "cols");

            if (rows && cols) {
              icUInt16Number nRows = 0, nCols = 0;

              // Dimensions are icUInt16Number throughout CIccSparseMatrix, so
              // the casts that stood here wrapped anything larger back into
              // range: rows="65567" was stored -- and written back out on save
              // -- as 31 (#1909).
              if (!icXmlParseU16(icXmlAttrValue(rows), nRows) ||
                  !icXmlParseU16(icXmlAttrValue(cols), nCols)) {
                parseStr += "Invalid SparseMatrix rows or cols\n";
                return false;
              }

              // Init() enforces CIccSparseMatrix's own dimension ceiling and
              // returns false having left the matrix empty -- GetRowStart()
              // NULL and GetMaxEntries() 0. Its result was discarded here, so a
              // matrix that declared more rows than Init() accepts and carried
              // no SparseRow children fell straight through to the row-start
              // fill below and wrote through the NULL pointer. Representable
              // but out-of-domain dimensions are refused here instead.
              if (!mtx.Init(nRows, nCols, true)) {
                parseStr += "Invalid SparseMatrix dimensions\n";
                return false;
              }

              icUInt16Number *rowstart = mtx.GetRowStart();
              icUInt32Number nMaxEntries = mtx.GetMaxEntries();
              xmlNode *pRow;
              int iRow=0;
              icUInt32Number pos = 0;
              for (pRow=pChild->children; pRow; pRow=pRow->next) {
                if (pRow->type == XML_ELEMENT_NODE && !icXmlStrCmp(pRow->name, "SparseRow")) {
                  xmlNode *pIdx = icXmlFindNode(pRow->children, "ColIndices");
                  xmlNode *pData = icXmlFindNode(pRow->children, "ColData");

                  if (pIdx && pData) {
                    CIccUInt16Array idx;
                    CIccFloatArray data;

                    if (!idx.ParseTextArray(pIdx) || !data.ParseTextArray(pData)) {
                      parseStr += "Unable to parse SparseRow index or data values\n";
                      return false;
                    }
                    if (idx.GetSize() != data.GetSize()) {
                      parseStr += "Mismatch between SparseRow index and data lengths\n";
                      return false;
                    }
                    if (pos+idx.GetSize() > nMaxEntries) {
                      parseStr += "Exceeded maximum number of sparse matrix entries\n";
                      return false;
                    }
                    rowstart[iRow] = (icUInt16Number)pos;
                    memcpy(mtx.GetColumnsForRow(iRow), idx.GetBuf(), idx.GetSize()*sizeof(icUInt16Number));
                    memcpy(mtx.GetData()->getPtr(pos), data.GetBuf(), data.GetSize()*sizeof(icFloatNumber));
                    pos += idx.GetSize();
                  }
                  iRow++;
                }
              }
              while(iRow<nRows) {
                rowstart[iRow] = (icUInt16Number)pos;
                iRow++;
              }
              rowstart[iRow] = (icUInt16Number)pos;
            }
            else {
              parseStr += "Cannot find SparseMatrix rows and cols\n";
              return false;
            }

            i++;
          }
          else if (!icXmlStrCmp(pChild->name, "FullMatrix")) {
            mtx.Reset(m_RawData + i*bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, false);

            xmlAttr *rows = icXmlFindAttr(pChild, "rows");
            xmlAttr *cols = icXmlFindAttr(pChild, "cols");

            if (rows && cols) {
              icUInt16Number nRows = 0, nCols = 0;

              // Same narrowing as the SparseMatrix branch above. Here the
              // laundered value also has to agree with the element count for
              // the data to be accepted, so it is the wrapped dimensions that
              // survive: rows="65567" with 31x41 values loaded as 31 rows.
              if (!icXmlParseU16(icXmlAttrValue(rows), nRows) ||
                  !icXmlParseU16(icXmlAttrValue(cols), nCols)) {
                parseStr += "Invalid FullMatrix rows or cols\n";
                return false;
              }

              // FillFromFullMatrix() below checks for the empty matrix a failed
              // Init() leaves behind, so this branch does not have the NULL
              // write the SparseMatrix branch did -- but it silently produced a
              // profile with an unpopulated matrix. Refuse the dimensions.
              if (!mtx.Init(nRows, nCols, true)) {
                parseStr += "Invalid FullMatrix dimensions\n";
                return false;
              }

              CIccFloatArray data;
              data.ParseTextArray(pChild);
              if (data.GetSize()==(icUInt32Number)nRows*nCols) {
                if (!mtx.FillFromFullMatrix(data.GetBuf()))
                  parseStr += "Exceeded maximum number of sparse matrix entries\n";
              }
              else {
                parseStr += "Invalid FullMatrix data dimensions\n";
                return false;
              }
            }
            else {
              parseStr += "Cannot find FullMatrix rows and cols\n";
              return false;
            }
            i++;
          }
        }
      }
      return true;
    }
    else {
      parseStr += "Cannot find outputChannels and matrixType members\n";
    }
  }
  else {
    parseStr += "Cannot find SparseMatrixArray node\n";
  }

  return false;
}


template <class T, icTagTypeSignature Tsig>
const icChar* CIccTagXmlFixedNum<T, Tsig>::GetClassName() const
{
  if (Tsig==icSigS15Fixed16ArrayType)
    return "CIccTagXmlS15Fixed16";
  else 
    return "CIccTagXmlU16Fixed16";
}

// for multi-platform support
// added "this->" modifier to data members.
template <class T, icTagTypeSignature Tsig>
bool CIccTagXmlFixedNum<T, Tsig>::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  int i;

  // CWE-400/CWE-834: m_nSize is bounded by the tag byte size in Read() and m_Num is
  // allocated to match; assert an explicit upper limit so a corrupted count can't
  // drive an unbounded serialization walk.
  const icUInt32Number nMaxNumValues = 0xffffff;
  if (this->m_nSize > nMaxNumValues)
    return false;

  if (Tsig==icSigS15Fixed16ArrayType) {
    int n = 8;
    xml += blanks + "<Array>\n";
    for (i=0; i<(int)this->m_nSize; i++) {
      if (!(i%n)) {
        if (i)
          xml += "\n";
        xml += blanks + blanks;
      }
      else {
        xml += " ";
      }
      snprintf(buf, bufSize, icXmlFloatFmt, (float)icFtoD(this->m_Num[i]));
      xml += buf;
    }

    if ((i%n)!=1) {
      xml += "\n";
    }
  }
  else {
    for (i=0; i<(int)this->m_nSize; i++) {

      if (!(i%8)) {
        if (i)
          xml += "\n";
        xml += blanks + blanks;
      }
      else {
        xml += " ";
      }
      snprintf(buf, bufSize, icXmlFloatFmt, (float)icUFtoD(this->m_Num[i]));
      xml += buf;
    }

    if ((i%8)!=1) {
      xml += "\n";
    }
  }
  xml += blanks + "</Array>\n";
  return true;
}

// for multi-platform support
// added "this->" modifier to data members.
template <class T, icTagTypeSignature Tsig>
bool CIccTagXmlFixedNum<T, Tsig>::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "Array");
  if (!pNode)
    return false;

  pNode = pNode->children;

  CIccFloatArray a;

  if (!a.ParseArray(pNode) || !a.GetSize()) {   
    return false;
  }

  icUInt32Number i, n = a.GetSize();
  icFloatNumber *buf = a.GetBuf();

  this->SetSize(n);

  for (i=0; i<n; i++) {
    if (Tsig==icSigS15Fixed16ArrayType) {
      this->m_Num[i] = icDtoF(buf[i]);
    }
    else {
      this->m_Num[i] = icDtoUF(buf[i]);
    }
  }
  return true;
}


//Make sure typedef classes get built
template class CIccTagXmlFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType>;
template class CIccTagXmlFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType>;


template <class T, class A, icTagTypeSignature Tsig>
const icChar *CIccTagXmlNum<T, A, Tsig>::GetClassName() const
{
  if (sizeof(T)==sizeof(icUInt8Number))
    return "CIccTagXmlUInt8";
  else if (sizeof(T)==sizeof(icUInt16Number))
    return "CIccTagXmlUInt16";
  else if (sizeof(T)==sizeof(icUInt32Number))
    return "CIccTagXmlUInt32";
  else if (sizeof(T)==sizeof(icUInt64Number))
    return "CIccTagXmlUInt64";
  else
    return "CIccTagXmlNum<>";
}

// for multi-platform support
// added "this->" modifier to data members.  
template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagXmlNum<T, A, Tsig>::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 512;
  char buf[bufSize];
  int i;

  // CWE-400/CWE-834: m_nSize is derived from the tag byte size in Read() and m_Num is
  // allocated to match; assert an explicit upper limit so a corrupted count can't drive
  // an unbounded serialization walk (each element expands to several XML characters, so
  // a multi-megabyte ui08/ui16/ui32/ui64 array would otherwise balloon the output).
  // Mirrors the guard already applied to the sibling CIccTagXmlFixedNum::ToXml above.
  const icUInt32Number nMaxNumValues = 0xffffff;
  if (this->m_nSize > nMaxNumValues)
    return false;

  xml += blanks + "<Array>\n";
  for (i=0; i<(int)this->m_nSize; i++) {
    if (!(i%16)) {
      if (i)
        xml += "\n";
      xml += blanks + blanks;
    }
    else {
      xml += " ";
    }
    if (sizeof(T)==16)
      snprintf(buf, bufSize, "%llu", (icUInt64Number)this->m_Num[i]);
    else
      snprintf(buf, bufSize, "%u", (unsigned int) this->m_Num[i]);
    xml += buf;
  }

  if ((i%16)!=1) {
    xml += "\n";
  }

  xml += blanks + "</Array>\n";
  return true;
  return true;
}

// for multi-platform support
// added "this->" modifier to data members.
template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagXmlNum<T, A, Tsig>::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlNode *pDataNode = icXmlFindNode(pNode, "Data");
  if (!pDataNode) {
    pDataNode = icXmlFindNode(pNode, "Array");
  }
  if (!pDataNode) {
    return false;
  }

  pNode = pDataNode->children;  

  A a;

  if (!a.ParseArray(pNode) || !a.GetSize()) {   
    return false;
  }

  icUInt32Number i, n = a.GetSize();
  T *buf = a.GetBuf();

  this->SetSize(n);

  for (i=0; i<n; i++) {
    this->m_Num[i] = buf[i];
  }

  return true;
}

//Make sure typedef classes get built
template class CIccTagXmlNum<icUInt8Number, CIccUInt8Array, icSigUInt8ArrayType>;
template class CIccTagXmlNum<icUInt16Number, CIccUInt16Array, icSigUInt16ArrayType>;
template class CIccTagXmlNum<icUInt32Number, CIccUInt32Array, icSigUInt32ArrayType>;
template class CIccTagXmlNum<icUInt64Number, CIccUInt64Array, icSigUInt64ArrayType>;


template <class T, class A, icTagTypeSignature Tsig>
const icChar *CIccTagXmlFloatNum<T, A, Tsig>::GetClassName() const
{
  if (Tsig==icSigFloat16ArrayType)
    return "CIccTagXmlFloat32";
  else if (Tsig==icSigFloat32ArrayType)
    return "CIccTagXmlFloat32";
  else if (Tsig==icSigFloat64ArrayType)
    return "CIccTagXmlFloat64";
  else
    return "CIccTagXmlFloatNum<>";
}

// for multi-platform support
// added "this->" modifier to data members.  
template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagXmlFloatNum<T, A, Tsig>::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 512;
  char buf[bufSize];

  // CWE-400/CWE-834: m_nSize is bounded by the tag byte size in Read() and m_Num is
  // allocated to match; assert the same explicit upper limit used by the FixedNum/Num
  // siblings so a corrupted count can't drive an unbounded serialization walk.
  const icUInt32Number nMaxNumValues = 0xffffff;
  if (this->m_nSize > nMaxNumValues)
    return false;

  if (this->m_nSize==1) {
#ifdef _WIN32
    if (sizeof(T)==sizeof(icFloat32Number))
      sprintf(buf, "<Data>" icXmlFloatFmt "</Data>\n", this->m_Num[0]);
    else if (sizeof(T)==sizeof(icFloat64Number))
      sprintf(buf, "<Data>" icXmlDoubleFmt "</Data>\n", this->m_Num[0]);
    else
#endif
      snprintf(buf, bufSize, "<Data>" icXmlFloatFmt "</Data>", this->m_Num[0]);
    xml += blanks;
    xml += buf;
  }
  else {
    int i;
    int n = 8;
    xml += blanks + "<Data>\n";
    for (i=0; i<(int)this->m_nSize; i++) {
      if (!(i%n)) {
        if (i)
          xml += "\n";
        xml += blanks + blanks;
      }
      else {
        xml += " ";
      }
#ifdef _WIN32
      if (sizeof(T)==sizeof(icFloat32Number))
        sprintf(buf, icXmlFloatFmt, this->m_Num[i]);

      else if (sizeof(T)==sizeof(icFloat32Number))
        sprintf(buf, icXmlDoubleFmt, this->m_Num[i]);
      else
#endif
        snprintf(buf, bufSize, icXmlFloatFmt, this->m_Num[i]);
      xml += buf;
    }

    if ((i%n)!=1) {
      xml += "\n";
      xml += blanks + "</Data>\n";
    }
    else {
      xml += " </Data>\n";
    }
  }
  return true;
}

// for multi-platform support
// added "this->" modifier to data members.
template <class T, class A, icTagTypeSignature Tsig>
bool CIccTagXmlFloatNum<T, A, Tsig>::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  pNode = icXmlFindNode(pNode, "Data");

  const char *filename = icXmlAttrValue(pNode, "Filename", "");
  if (!filename[0]) {
    filename = icXmlAttrValue(pNode, "File", "");
  }

  A a;

  if (filename && filename[0]) {
    CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");
    if (!file){
      parseStr += "Error! - File '";
      parseStr += filename;
      parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
      delete file;
      return false;
    }

    size_t len = file->GetLength();

    if (!stricmp(icXmlAttrValue(pNode, "Format", "text"), "text")) {
      icUInt32Number nLen;
      if (!icXmlValidateFileCount(len, nLen, parseStr, filename)) {
        delete file;
        return false;
      }
      char *fbuf = (char*)malloc((size_t)nLen + 1);
      if (!fbuf) {
        parseStr += "Memory error!\n";
        delete file;
        return false;
      }
      fbuf[nLen] = 0;

      if (file->Read8(fbuf, nLen)!=nLen) {
        parseStr += "Read error of (";
        parseStr += filename;
        parseStr += ")!\n";
        free(fbuf);
        delete file;
        return false;
      }
      delete file;

      if (!a.ParseTextArray(fbuf) || !a.GetSize()) {
        parseStr += "Parse error of (";
        parseStr += filename;
        parseStr += ")!\n";
        free(fbuf);
        return false;
      }
      free(fbuf);
    }
    else if (Tsig==icSigFloat16ArrayType && sizeof(T)==sizeof(icFloat16Number)) {
      icUInt32Number n;
      if (!icXmlValidateFileCount(len / sizeof(icFloat16Number), n, parseStr, filename)) {
        delete file;
        return false;
      }
      this->SetSize(n);
      if (file->Read16(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat16ArrayType && sizeof(T)==sizeof(icFloat32Number)) {
      icUInt32Number n;
      if (!icXmlValidateFileCount(len / sizeof(icFloat32Number), n, parseStr, filename)) {
        delete file;
        return false;
      }
      this->SetSize(n);
      if (file->ReadFloat16Float(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat32ArrayType && sizeof(T)==sizeof(icFloat32Number)) {
      icUInt32Number n;
      if (!icXmlValidateFileCount(len / sizeof(icFloat32Number), n, parseStr, filename)) {
        delete file;
        return false;
      }
      this->SetSize(n);
      if (file->ReadFloat32Float(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat64ArrayType && sizeof(T)==sizeof(icFloat64Number)) {
      icUInt32Number n;
      if (!icXmlValidateFileCount(len / sizeof(icFloat64Number), n, parseStr, filename)) {
        delete file;
        return false;
      }
      this->SetSize(n);
      if (file->Read64(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;

    }
    else {
      delete file;
      parseStr += "Unsupported file parsing type!\n";
      return false;
    }
  }
  else {
    if (!pNode)
      return false;
    
    pNode = pNode->children;

    if (!a.ParseArray(pNode) || !a.GetSize()) {   
      return false;
    }
  }

  icUInt32Number i, n = a.GetSize();
  T *buf = a.GetBuf();

  this->SetSize(n);

  for (i=0; i<n; i++) {
    this->m_Num[i] = buf[i];
  }
  return true;
}


//Make sure typedef classes get built
template class CIccTagXmlFloatNum<icFloat32Number, CIccFloat32Array, icSigFloat16ArrayType>;
template class CIccTagXmlFloatNum<icFloat32Number, CIccFloat32Array, icSigFloat32ArrayType>;
template class CIccTagXmlFloatNum<icFloat64Number, CIccFloat64Array, icSigFloat64ArrayType>;


bool CIccTagXmlMeasurement::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];

  CIccInfo info;  

  snprintf(buf, bufSize, "<StandardObserver>%s</StandardObserver>\n",icGetStandardObserverName(m_Data.stdObserver));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<MeasurementBacking X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n", icFtoD(m_Data.backing.X),
    icFtoD(m_Data.backing.Y), icFtoD(m_Data.backing.Z));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<Geometry>%s</Geometry>\n",info.GetMeasurementGeometryName(m_Data.geometry));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<Flare>%s</Flare>\n",info.GetMeasurementFlareName(m_Data.flare));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<StandardIlluminant>%s</StandardIlluminant>\n",info.GetIlluminantName(m_Data.illuminant));
  xml += blanks + buf;
  return true;
}


bool CIccTagXmlMeasurement::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  memset(&m_Data, 0, sizeof(m_Data));

  pNode = icXmlFindNode(pNode, "StandardObserver");
  if (pNode) {
    m_Data.stdObserver = icGetNamedStandardObserverValue(pNode->children ? (icChar*)pNode->children->content: "");   
  }


  pNode = icXmlFindNode(pNode, "MeasurementBacking");
  if (pNode){
    xmlAttr *attr;

    attr = icXmlFindAttr(pNode, "X");
    if (attr) {
      m_Data.backing.X = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pNode, "Y");
    if (attr) {
      m_Data.backing.Y = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pNode, "Z");
    if (attr) {
      m_Data.backing.Z = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
  }

  pNode = icXmlFindNode(pNode, "Geometry");
  if (pNode){
    m_Data.geometry = icGeNamedtMeasurementGeometryValue(pNode->children ? (icChar*)pNode->children->content : "");     
  }

  pNode = icXmlFindNode(pNode, "Flare");
  if (pNode){
    m_Data.flare = icGetNamedMeasurementFlareValue(pNode->children ? (icChar*)pNode->children->content : ""); 
  }

  pNode = icXmlFindNode(pNode, "StandardIlluminant");
  if (pNode){
    m_Data.illuminant = icGetIlluminantValue(pNode->children ? (icChar*)pNode->children->content : "");
  }

  return true; 
}

bool CIccTagXmlMultiLocalizedUnicode::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  char data[256];
  std::string bufstr;
  CIccMultiLocalizedUnicode::iterator i;

  if (!m_Strings)
    return false;

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength());
    // Pack the 16-bit language/country codes into the 32-bit signature expected by
    // icGetSigStr(). m_nLanguageCode is a 16-bit unsigned that promotes to (signed)
    // int before the shift, so a high-bit language code (>= 0x8000) makes
    // m_nLanguageCode<<16 overflow into negative territory -- signed overflow UB,
    // and the resulting negative int silently wraps when passed to icGetSigStr's
    // icUInt32Number parameter. Cast to icUInt32Number first so the whole pack is
    // computed in well-defined unsigned arithmetic.
    icXmlDumpLocalizedText(xml, blanks, "LocalizedText",
                           icGetSigStr(data, 256, ((icUInt32Number)i->m_nLanguageCode << 16) |
                                       (icUInt32Number)i->m_nCountryCode),
                           bufstr);
  }
  return true;
}


bool CIccTagXmlMultiLocalizedUnicode::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlAttr *langCode;
  int n = 0;

  for (pNode = icXmlFindNode(pNode, "LocalizedText"); pNode; pNode = icXmlFindNode(pNode->next, "LocalizedText")) {    
    if ((langCode = icXmlFindLanguageCountryAttr(pNode))) {
      std::string text;

      if (icXmlParseLocalizedText(pNode, text)) {
        icUInt32Number lc = icGetSigVal(icXmlAttrValue(langCode));
        if (!icXmlSetLocalizedUtf8(*this, text, (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff)))
          return false;
        n++;
      }
      else {
        SetText("");
        n++;
      }
    }
  }  
  return n>0;  //We succeed if we parsed at least one string
}


bool CIccTagXmlTagData::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 60;
  char buf[bufSize];
  std::string szFlag("ASCII");

  if ( m_nDataFlag & icBinaryData )
    szFlag = "binary";
  snprintf (buf, bufSize, "<Data Flag=\"%s\">\n", szFlag.c_str());
  xml += blanks + buf;
  icXmlDumpHexData(xml, blanks+" ", m_pData, m_nSize);
  xml += blanks + "</Data>\n";

  return true;
}


bool CIccTagXmlTagData::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "Data");
  if (pNode && pNode->children && pNode->children->content) {
    const icChar *szFlag = icXmlAttrValue(pNode, "Flag");    
    m_nDataFlag = icAsciiData;
    if (!strcmp(szFlag,"binary"))
      m_nDataFlag = icBinaryData;

    icUInt32Number nSize = icXmlGetHexDataSize((const char *)pNode->children->content);
    SetSize(nSize, false);
    if (nSize) {
      icXmlGetHexData(m_pData, (const char*)pNode->children->content, nSize);
    }

    return true;
  }
  return false;
}


bool CIccTagXmlDateTime::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  snprintf(buf, bufSize, "<DateTime>%d-%02d-%02dT%02d:%02d:%02d</DateTime>\n",
    m_DateTime.year, m_DateTime.month, m_DateTime.day, m_DateTime.hours, m_DateTime.minutes, m_DateTime.seconds);
  xml += blanks + buf;
  return true;
}


bool CIccTagXmlDateTime::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  memset(&m_DateTime, 0, sizeof(m_DateTime));

  pNode = icXmlFindNode(pNode, "DateTime");
  if (pNode) {      
    m_DateTime = icGetDateTimeValue(pNode->children ? (const char*)pNode->children->content : "");  
    return true;
  }
  return false;
}


bool CIccTagXmlColorantOrder::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 40;
  char buf[bufSize];

  xml += blanks + "<ColorantOrder>\n"; //+ blanks + "  ";
  // CWE-400/CWE-834: SetSize() caps the colorant count at 0xffff and allocates
  // m_pData to match; assert that bound locally so the serialization walk has an
  // explicit upper limit.
  const icUInt32Number nMaxColorants = 0xffff;
  if (m_nCount > nMaxColorants)
    return false;
  for (icUInt32Number i=0; i<m_nCount; i++) {
    snprintf(buf, bufSize, "  <n>%d</n>\n", m_pData[i]);
    xml += blanks + buf;
  }

  xml += blanks + "</ColorantOrder>\n";

  return true;
}

bool CIccTagXmlColorantOrder::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "ColorantOrder");

  if (pNode) {
    int n = icXmlNodeCount(pNode->children, "n");

    if (n) {
      SetSize(n);

      if (m_pData) {
        if (CIccUInt8Array::ParseArray(m_pData, n, pNode->children))
          return true;
      }
    }
  }  
  return false;
}


bool CIccTagXmlColorantTable::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  std::string fix;
  std::string str;

  xml += blanks + "<ColorantTable>\n";
  // CWE-400/CWE-834: SetSize() caps the colorant count at 0xffff and allocates
  // m_pData to match; assert that bound locally so the serialization walk has an
  // explicit upper limit.
  const icUInt32Number nMaxColorants = 0xffff;
  if (m_nCount > nMaxColorants)
    return false;
  for (icUInt32Number i=0; i<m_nCount; i++) {
    icFloatNumber lab[3];
    lab[0] = icU16toF(m_pData[i].data[0]);
    lab[1] = icU16toF(m_pData[i].data[1]);
    lab[2] = icU16toF(m_pData[i].data[2]);
    icLabFromPcs(lab);
    snprintf(buf, bufSize, "  <Colorant Name=\"%s\" Channel1=\"" icXmlFloatFmt "\" Channel2=\"" icXmlFloatFmt "\" Channel3=\"" icXmlFloatFmt "\"/>\n",
      icFixXml(fix, icAnsiToUtf8(str, m_pData[i].name)), lab[0], lab[1], lab[2]);
    xml += blanks + buf;
  }
  //xml += "\n";
  xml += blanks + "</ColorantTable>\n";

  return true;
}


bool CIccTagXmlColorantTable::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "ColorantTable");

  if (pNode && pNode->children) {
    pNode = pNode->children;

    icUInt16Number n = (icUInt16Number)icXmlNodeCount(pNode, "Colorant");

    if (n) {
      icUInt32Number i;
      SetSize(n);

      for (i=0; pNode; pNode=pNode->next) {
        if (pNode->type == XML_ELEMENT_NODE &&
          !icXmlStrCmp(pNode->name, "Colorant") && 
          i<n) {
            std::string str;
            const icChar *name = icXmlAttrValue(pNode, "Name");
            xmlAttr *L = icXmlFindAttr(pNode, "Channel1");
            xmlAttr *a = icXmlFindAttr(pNode, "Channel2");
            xmlAttr *b = icXmlFindAttr(pNode, "Channel3");

            if (name && L && a && b) {
              strncpy(m_pData[i].name, icUtf8ToAnsi(str, name), sizeof(m_pData[i].name)-1);
              m_pData[i].name[sizeof(m_pData[i].name)-1]=0;

              icFloatNumber lab[3];

              lab[0] = (icFloatNumber)atof(icXmlAttrValue(L));
              lab[1] = (icFloatNumber)atof(icXmlAttrValue(a));
              lab[2] = (icFloatNumber)atof(icXmlAttrValue(b));

              icLabToPcs(lab);
              m_pData[i].data[0] = icFtoU16(lab[0]);
              m_pData[i].data[1] = icFtoU16(lab[1]);
              m_pData[i].data[2] = icFtoU16(lab[2]);

              i++;
            }
            else
              return false;
        }
      }
      return i==n;
    }
    return false;
  }
  return false;
}


bool CIccTagXmlViewingConditions::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];

  snprintf(buf, bufSize, "<IlluminantXYZ X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n",
    icFtoD(m_XYZIllum.X), icFtoD(m_XYZIllum.Y), icFtoD(m_XYZIllum.Z));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<SurroundXYZ X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n",
    icFtoD(m_XYZSurround.X), icFtoD(m_XYZSurround.Y), icFtoD(m_XYZSurround.Z));
  xml += blanks + buf;

  CIccInfo info;
  snprintf(buf, bufSize, "<IllumType>%s</IllumType>\n", info.GetIlluminantName(m_illumType));
  xml += blanks + buf;

  return true;
}

bool CIccTagXmlViewingConditions::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlAttr *attr;
  xmlNode *pChild;

  memset(&m_XYZIllum, 0, sizeof(m_XYZIllum));
  memset(&m_XYZSurround, 0, sizeof(m_XYZSurround));
  m_illumType = (icIlluminant)0;

  pChild = icXmlFindNode(pNode, "IlluminantXYZ");
  if (pChild) {

    attr = icXmlFindAttr(pChild, "X");
    if (attr) {
      m_XYZIllum.X = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pChild, "Y");
    if (attr) {
      m_XYZIllum.Y = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pChild, "Z");
    if (attr) {
      m_XYZIllum.Z = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
  }

  pChild = icXmlFindNode(pNode, "SurroundXYZ");
  if (pChild) {
    attr = icXmlFindAttr(pChild, "X");
    if (attr) {
      m_XYZSurround.X = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pChild, "Y");
    if (attr) {
      m_XYZSurround.Y = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }

    attr = icXmlFindAttr(pChild, "Z");
    if (attr) {
      m_XYZSurround.Z = icDtoF((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
  }

  pChild = icXmlFindNode(pNode, "IllumType");
  if (pChild && pChild->children && pChild->children->content) {
    m_illumType = icGetIlluminantValue((icChar*)pChild->children->content);
  }

  return true;
}

bool CIccTagXmlSpectralViewingConditions::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  int i, j;
  icFloatNumber *ptr;
  CIccInfo info;

  snprintf(buf, bufSize, "<StdObserver>%s</StdObserver>\n", info.GetStandardObserverName(m_stdObserver));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<IlluminantXYZ X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n",
    m_illuminantXYZ.X, m_illuminantXYZ.Y, m_illuminantXYZ.Z);
  xml += blanks + buf;

  if (m_observer) {
    snprintf(buf, bufSize, "<ObserverFuncs start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"",
            icF16toF(m_observerRange.start), icF16toF(m_observerRange.end), m_observerRange.steps);
    xml += blanks + buf;

    if (m_reserved2) {
      snprintf(buf, bufSize, " Reserved=\"%d\"", m_reserved2);
      xml += buf;
    }
    xml += ">\n";
    
    ptr = &m_observer[0];

    for (j=0; j<3; j++) {
      xml += blanks;
      for (i=0; i<m_observerRange.steps; i++) {
        if (i && !(i%8)) {
          xml += "\n";
          xml += blanks;
        }
        snprintf(buf, bufSize, " " icXmlFloatFmt, *ptr);
        ptr++;
        xml += buf;
      }
      xml += "\n";
    }
    xml += blanks + "</ObserverFuncs>\n";
  }

  snprintf(buf, bufSize, "<StdIlluminant>%s</StdIlluminant>\n", info.GetIlluminantName(m_stdIlluminant));
  xml += blanks + buf;

  snprintf(buf, bufSize, "<ColorTemperature>" icXmlFloatFmt "</ColorTemperature>\n", m_colorTemperature);
  xml += blanks + buf;

  if (m_illuminant) {
    snprintf(buf, bufSize, "<IlluminantSPD start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"",
            icF16toF(m_illuminantRange.start), icF16toF(m_illuminantRange.end), m_illuminantRange.steps);
    xml += blanks + buf;

    if (m_reserved3) {
      snprintf(buf, bufSize, " Reserved=\"%d\"", m_reserved3);
      xml += buf;
    }
    xml += ">\n";

    ptr = &m_illuminant[0];

    xml += blanks;
    for (i=0; i<m_illuminantRange.steps; i++) {
      if (i && !(i%8)) {
        xml += "\n";
        xml += blanks;
      }
      snprintf(buf, bufSize, " " icXmlFloatFmt, *ptr);
      ptr++;
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "</IlluminantSPD>\n";
  }

  snprintf(buf, bufSize, "<SurroundXYZ X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>\n",
    m_surroundXYZ.X, m_surroundXYZ.Y, m_surroundXYZ.Z);
  xml += blanks + buf;


  return true;
}

bool CIccTagXmlSpectralViewingConditions::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlNode *pChild;
  xmlAttr *attr;

  memset(&m_illuminantXYZ, 0, sizeof(m_illuminantXYZ));
  memset(&m_surroundXYZ, 0, sizeof(m_surroundXYZ));
  m_stdIlluminant = icIlluminantUnknown;
  m_stdObserver = icStdObsUnknown;
  m_colorTemperature = 0;
  m_reserved2 = 0;
  m_reserved3 = 0;

  pChild = icXmlFindNode(pNode, "StdObserver");
  if (pChild && pChild->children && pChild->children->content) {
    m_stdObserver = icGetNamedStandardObserverValue((icChar*)pChild->children->content);
  }

  pChild = icXmlFindNode(pNode, "IlluminantXYZ");
  if (pChild) {

    attr = icXmlFindAttr(pChild, "X");
    if (attr) {
      m_illuminantXYZ.X = (icFloatNumber)atof(icXmlAttrValue(attr));
    }

    attr = icXmlFindAttr(pChild, "Y");
    if (attr) {
      m_illuminantXYZ.Y = (icFloatNumber)atof(icXmlAttrValue(attr));
    }

    attr = icXmlFindAttr(pChild, "Z");
    if (attr) {
      m_illuminantXYZ.Z = (icFloatNumber)atof(icXmlAttrValue(attr));
    }
  }

  pChild = icXmlFindNode(pNode, "ObserverFuncs");
  if (pChild) {
    attr = icXmlFindAttr(pChild, "start");
    if (attr) {
      m_observerRange.start =icFtoF16((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
    attr = icXmlFindAttr(pChild, "end");
    if (attr) {
      // This alone among the four range endpoints in this function parsed with
      // atoi() rather than atof().  "end" is a wavelength in nm held as a
      // float16, so the integer parse silently truncated the fraction: an
      // ObserverFuncs range ending at end="702.5" was stored as 702, and the
      // writer at the top of this file then emitted the truncated value.  The
      // matching "start" three lines above, and both endpoints of the
      // IlluminantSPD range below, already use atof() -- this brings the odd one
      // out in line with them.
      m_observerRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
    attr = icXmlFindAttr(pChild, "steps");
    if (attr) {
      // The <=0 / >0xffff guard below already refused every value that could not
      // reach the icUInt16Number field, so unlike the sites above this one was
      // not narrowing.  What remained was atoi() itself: its behaviour is
      // undefined -- not merely wrong -- when the text does not fit an int, so
      // steps="99999999999999999999" was UB before the guard could look at it.
      // icXmlParseU16 uses strtoul and reports the overflow instead.  A value of
      // 0 now reaches the shared "if these are not set correctly" check below
      // rather than returning here, which is the same refusal one step later.
      if (!icXmlParseU16(icXmlAttrValue(attr), m_observerRange.steps))
        return false;
    }
    attr = icXmlFindAttr(pChild, "reserved");
    if (attr) {
      m_reserved2 = (icUInt16Number)icXmlAttrToUInt(icXmlAttrValue(attr));
    }
    
    // if these are not set correctly, then later allocations and calculations WILL fail
    if (m_observerRange.start == 0 || m_observerRange.end == 0 || m_observerRange.steps == 0)
      return false;

    if (pChild->children && pChild->children->content) {
      CIccFloatArray vals;
      vals.ParseTextArray((icChar*)pChild->children->content);
      if (vals.GetSize()!=m_observerRange.steps*3)
        return false;
      m_observer = new (std::nothrow) icFloatNumber[m_observerRange.steps*3];
      if (!m_observer)
        return false;
      icFloatNumber *pBuf = vals.GetBuf();
      if (!pBuf)
        return false;
      memcpy(m_observer, pBuf, m_observerRange.steps*3*sizeof(icFloatNumber));
    }
    else {
      setObserver(m_stdObserver, m_observerRange, NULL);
    }
  }

  pChild = icXmlFindNode(pNode, "StdIlluminant");
  if (pChild && pChild->children && pChild->children->content) {
    m_stdIlluminant = icGetIlluminantValue((icChar*)pChild->children->content);
  }

  pChild = icXmlFindNode(pNode, "ColorTemperature");
  if (pChild && pChild->children && pChild->children->content) {
    m_colorTemperature = (icFloatNumber)atof((icChar*)pChild->children->content);
  }

  pChild = icXmlFindNode(pNode, "IlluminantSPD");
  if (pChild) {
    attr = icXmlFindAttr(pChild, "start");
    if (attr) {
      m_illuminantRange.start = icFtoF16((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
    attr = icXmlFindAttr(pChild, "end");
    if (attr) {
      m_illuminantRange.end = icFtoF16((icFloatNumber)atof(icXmlAttrValue(attr)));
    }
    attr = icXmlFindAttr(pChild, "steps");
    if (attr) {
      // Same reasoning as the ObserverFuncs steps above: already range-guarded,
      // so this replaces atoi()'s undefined behaviour on an out-of-int input,
      // not a narrowing.  Zero is caught by the m_illuminantRange.steps == 0
      // check a few lines below.
      if (!icXmlParseU16(icXmlAttrValue(attr), m_illuminantRange.steps))
        return false;
    }
    attr = icXmlFindAttr(pChild, "reserved");
    if (attr) {
      m_reserved3 = (icUInt16Number)icXmlAttrToUInt(icXmlAttrValue(attr));
    }
    
    // if these are not set correctly, then later allocations and calculations WILL fail
    if (m_illuminantRange.start == 0 || m_illuminantRange.end == 0 || m_illuminantRange.steps == 0)
      return false;

    if (pChild->children && pChild->children->content) {
      CIccFloatArray vals;
      vals.ParseTextArray((icChar*)pChild->children->content);
      if (vals.GetSize()!=m_illuminantRange.steps)
        return false;
      m_illuminant = new (std::nothrow) icFloatNumber[m_illuminantRange.steps];
      if (!m_illuminant)
        return false;
      icFloatNumber *pBuf = vals.GetBuf();
      if (!pBuf)
        return false;
      memcpy(m_illuminant, pBuf, m_illuminantRange.steps * sizeof(icFloatNumber));
    }
    else {
      setIlluminant(m_stdIlluminant, m_illuminantRange, NULL, m_colorTemperature);
    }
  }

  pChild = icXmlFindNode(pNode, "SurroundXYZ");
  if (pChild) {
    attr = icXmlFindAttr(pChild, "X");
    if (attr) {
      m_surroundXYZ.X = (icFloatNumber)atof(icXmlAttrValue(attr));
    }

    attr = icXmlFindAttr(pChild, "Y");
    if (attr) {
      m_surroundXYZ.Y = (icFloatNumber)atof(icXmlAttrValue(attr));
    }

    attr = icXmlFindAttr(pChild, "Z");
    if (attr) {
      m_surroundXYZ.Z = (icFloatNumber)atof(icXmlAttrValue(attr));
    }
  }

  return true;
}



bool icProfDescToXml(std::string &xml, CIccProfileDescStruct &p, std::string blanks = "")
{
  const size_t bufSize = 256;
  std::string fix;
  char buf[bufSize];
  char data[bufSize];

  snprintf(buf, bufSize, "<ProfileDesc>\n");
  xml += blanks + buf;

  // Each profileSequenceDescType entry carries three four-character signatures,
  // and all three are legitimately zero for a profile whose origin is unrecorded
  // -- a technology of zero is the named value icSigUndefined.  icGetSigStr(0)
  // returns the literal text "NULL", which is a *display* convention: the inverse
  // icGetSigVal("NULL") packs the ASCII bytes 'N','U','L','L' into 0x4E554C4C, so
  // a zero written here comes back as a bogus signature and the reparsed profile
  // fails validation ("Unknown technology") even though the original was clean
  // (#1843).  Emit an empty element for zero instead -- the reader below runs the
  // content through icXmlStrToSig(), and icGetSigVal("") returns 0, restoring the
  // original value.  Commit 751a0c6b (PR #1365) applied this same guard to the
  // header signatures; the profileSequenceDesc struct was missed.
  snprintf(buf, bufSize, "<DeviceManufacturerSignature>%s</DeviceManufacturerSignature>\n", p.m_deviceMfg ? icFixXml(fix, icGetSigStr(data, bufSize, p.m_deviceMfg)) : "");
  xml += blanks + blanks + buf;

  snprintf(buf, bufSize, "<DeviceModelSignature>%s</DeviceModelSignature>\n", p.m_deviceModel ? icFixXml(fix, icGetSigStr(data, bufSize, p.m_deviceModel)) : "");
  xml += blanks + blanks + buf;

  std::string szAttributes = icGetDeviceAttrName(p.m_attributes);
  //snprintf(buf, bufSize, "<DeviceAttributes>\"%016lX\" technology=\"%s\">\n", p.m_attributes, icFixXml(fix, icGetSigStr(data, bufSize, p.m_technology)));
  //xml += buf;
  xml += blanks + blanks + icGetDeviceAttrName(p.m_attributes);

  // Guard the zero case as above.  This is the field the round trip actually trips
  // over in practice: iccFromCube leaves the technology as icSigUndefined, so every
  // profile it writes used to acquire a 0x4E554C4C technology on the way back in.
  snprintf(buf, bufSize, "<Technology>%s</Technology>\n", p.m_technology ? icFixXml(fix, icGetSigStr(data, bufSize, p.m_technology)) : "");
  xml += blanks + blanks + buf;

  CIccTag *pTag = p.m_deviceMfgDesc.GetTag();
  CIccTagXml *pExt;

  if (pTag) {

    pExt = (CIccTagXml*)(pTag->GetExtension());

    if (!pExt || !pExt->GetExtClassName() || strcmp(pExt->GetExtClassName(), "CIccTagXml"))
      return false;

    xml += blanks + blanks + "<DeviceManufacturer>\n";

    const icChar* tagSig = icGetTagSigTypeName(pTag->GetType());
    snprintf(buf, bufSize, "<%s>\n", tagSig);
    xml += blanks + blanks + blanks + buf;

    if (!pExt->ToXml(xml, blanks + "        "))
      return false;

    snprintf(buf, bufSize, "</%s>\n", tagSig);
    xml += blanks + blanks + blanks + buf;

    xml += blanks + blanks +"</DeviceManufacturer>\n";
  }

  pTag = p.m_deviceModelDesc.GetTag();

  if (pTag) {
    pExt = (CIccTagXml*)(pTag->GetExtension());

    if (!pExt || !pExt->GetExtClassName() || strcmp(pExt->GetExtClassName(), "CIccTagXml"))
      return false;

    xml += blanks + blanks + "<DeviceModel>\n";

    const icChar* tagSig = icGetTagSigTypeName(pTag->GetType());
    snprintf(buf, bufSize, "<%s>\n", tagSig);
    xml += blanks + blanks + blanks + buf;

    if (!pExt->ToXml(xml, blanks + "        "))
      return false;

    snprintf(buf, bufSize, "</%s>\n", tagSig);
    xml += blanks + blanks + blanks + buf;

    xml += blanks + "  </DeviceModel>\n";
  }

  xml += blanks + "</ProfileDesc>\n";

  return true;
}

bool icXmlParseProfDesc(xmlNode *pNode, CIccProfileDescStruct &p, std::string &parseStr)
{
  if (pNode->type==XML_ELEMENT_NODE && !icXmlStrCmp(pNode->name, "ProfileDesc")) {
    xmlNode *pDescNode;  

    for (pDescNode = pNode->children; pDescNode; pDescNode=pDescNode->next) {
      if (pDescNode->type == XML_ELEMENT_NODE) {
        if (!icXmlStrCmp(pDescNode->name, "DeviceManufacturerSignature")){
          p.m_deviceMfg = icXmlStrToSig(pDescNode->children ? (const icChar*)pDescNode->children->content: "");
        }
        else if (!icXmlStrCmp(pDescNode->name, "DeviceModelSignature")){
          p.m_deviceModel = icXmlStrToSig(pDescNode->children ? (const icChar*)pDescNode->children->content : "");
        }
        else if (!icXmlStrCmp(pDescNode->name, "DeviceAttributes")){
          p.m_attributes = icGetDeviceAttrValue(pDescNode);      
        }
        else if (!icXmlStrCmp(pDescNode->name, "Technology")){
          p.m_technology = (icTechnologySignature)icXmlStrToSig(pDescNode->children ? (const icChar*)pDescNode->children->content : "");
        }
        else if (!icXmlStrCmp(pDescNode->name, "DeviceManufacturer")) {  
          xmlNode *pDevManNode = icXmlFindNode(pDescNode->children, "multiLocalizedUnicodeType");

          if (!pDevManNode){
            pDevManNode = icXmlFindNode(pDescNode->children, "textDescriptionType");
          }      

          if (pDevManNode){
            icTagTypeSignature tagSig = icGetTypeNameTagSig ((icChar*) pDevManNode->name);

            if (!p.m_deviceMfgDesc.SetType(tagSig)){
              return false;         
            }        
            CIccTag *pTag = p.m_deviceMfgDesc.GetTag();

            if (!pTag)
              return false;

            CIccTagXml *pExt = (CIccTagXml*)(pTag->GetExtension());

            if (!pExt || !pExt->GetExtClassName() || strcmp(pExt->GetExtClassName(), "CIccTagXml"))
              return false;

            pExt->ParseXml(pDevManNode->children, parseStr);
          }            
        }
        else if (!icXmlStrCmp(pDescNode->name, "DeviceModel")) {
          xmlNode *pDevModNode = icXmlFindNode(pDescNode->children, "multiLocalizedUnicodeType");

          if (!pDevModNode){
            pDevModNode = icXmlFindNode(pDescNode->children, "textDescriptionType");
          }

          if (pDevModNode){
            icTagTypeSignature tagSig = icGetTypeNameTagSig ((icChar*) pDevModNode->name);

            if (!p.m_deviceModelDesc.SetType(tagSig)) {
              return false;
            }
            CIccTag *pTag = p.m_deviceModelDesc.GetTag();

            if (!pTag)
              return false;

            CIccTagXml *pExt = (CIccTagXml*)(pTag->GetExtension());

            if (!pExt || !pExt->GetExtClassName() || strcmp(pExt->GetExtClassName(), "CIccTagXml"))
              return false;

            pExt->ParseXml(pDevModNode->children, parseStr);
          }
        }
      }
    }
  }
  else
    return false;

  if (!p.m_deviceMfgDesc.GetTag() || !p.m_deviceModelDesc.GetTag())
    return false;

  return true;
}


bool CIccTagXmlProfileSeqDesc::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  CIccProfileSeqDesc::iterator i;
  if (!m_Descriptions) 
    return false;

  xml += blanks + "<ProfileSequence>\n";
  for (i=m_Descriptions->begin(); i!=m_Descriptions->end(); i++) {
    if (!icProfDescToXml(xml, *i, blanks + "  "))
      return false;
  }
  xml += blanks + "</ProfileSequence>\n";
  return true;
}


bool CIccTagXmlProfileSeqDesc::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  pNode = icXmlFindNode(pNode, "ProfileSequence");

  if (!m_Descriptions)
    return false;

  m_Descriptions->clear();

  if (pNode) {
    for (pNode = pNode->children; pNode; pNode=pNode->next) {
      if (pNode->type==XML_ELEMENT_NODE && !icXmlStrCmp(pNode->name, "ProfileDesc")) {
        CIccProfileDescStruct ProfileDescStruct;

        if (!icXmlParseProfDesc(pNode, ProfileDescStruct, parseStr))
          return false;

        m_Descriptions->push_back(ProfileDescStruct);
      }
    }
  }
  return true;
}


bool CIccTagXmlResponseCurveSet16::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 100;
  char line[lineSize];
  int i;

  CIccInfo info;

  snprintf(line, lineSize, "<CountOfChannels>%d</CountOfChannels>\n", m_nChannels);
  xml += blanks + line;

  CIccResponseCurveStruct *pCurves=GetFirstCurves();
  while (pCurves) {
    snprintf(line, lineSize, "<ResponseCurve MeasUnitSignature=\"%s\">\n", info.GetMeasurementUnit(pCurves->GetMeasurementType()));
    xml += blanks + line;
    for (i=0; i<pCurves->GetNumChannels(); i++) {
      CIccResponse16List *pResponseList = pCurves->GetResponseList(i);
      icXYZNumber *pXYZ = pCurves->GetXYZ(i);
      snprintf(line, lineSize, "    <ChannelResponses X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\" >\n", icFtoD(pXYZ->X), icFtoD(pXYZ->Y), icFtoD(pXYZ->Z));
      xml += blanks + line;

      CIccResponse16List::iterator j;    
      for (j=pResponseList->begin(); j!=pResponseList->end(); j++) {
        snprintf(line, lineSize, "      <Measurement DeviceCode=\"%d\" MeasValue=\"" icXmlFloatFmt "\"", j->deviceCode, icFtoD(j->measurementValue));
        xml += blanks + line;

        if (j->reserved) {
          snprintf(line, lineSize, " Reserved=\"%d\"", j->reserved);
          xml += line;
        }
        xml += "/>\n";
      }

      xml += blanks + "    </ChannelResponses>\n";
    }
    xml += blanks + "  </ResponseCurve>\n";
    pCurves = GetNextCurves();
  }

  return true;
}


bool CIccTagXmlResponseCurveSet16::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  pNode = icXmlFindNode(pNode, "CountOfChannels"); 

  if(!pNode)
    return false;

  icUInt32Number nChannels = (icUInt32Number)atoi((const char*)pNode->children->content);
  SetNumChannels(nChannels);

  if (!m_ResponseCurves)
    return false;

  if (!m_ResponseCurves->empty())
    m_ResponseCurves->clear();

  for (pNode = pNode->next; pNode; pNode = pNode->next) {
    if (pNode->type==XML_ELEMENT_NODE && !icXmlStrCmp(pNode->name, "ResponseCurve")) {
      const icChar *szMeasurmentType = icXmlAttrValue(pNode, "MeasUnitSignature");

      if (nChannels != icXmlNodeCount(pNode->children, "ChannelResponses"))
        return false;

      CIccResponseCurveStruct curves(icGetMeasurementValue(szMeasurmentType), nChannels);
      xmlNode *pChild, *pMeasurement;
      int i;

      for (i=0, pChild = pNode->children; pChild; pChild = pChild->next) {
        if (pChild->type == XML_ELEMENT_NODE && !icXmlStrCmp(pChild->name, "ChannelResponses")) {
          CIccResponse16List *pResponseList = curves.GetResponseList(i);
          icXYZNumber *pXYZ = curves.GetXYZ(i);
          icResponse16Number response{};  // zero-init (.reserved is conditionally set; ICC spec requires it be 0)

          const icChar *szX = icXmlAttrValue(pChild, "X");
          const icChar *szY = icXmlAttrValue(pChild, "Y");
          const icChar *szZ = icXmlAttrValue(pChild, "Z");

          if (!szX || !szY || !szZ || !*szX || !*szY || !*szZ)
            return false;

          pXYZ->X = icDtoF((icFloatNumber)atof(szX));
          pXYZ->Y = icDtoF((icFloatNumber)atof(szY));
          pXYZ->Z = icDtoF((icFloatNumber)atof(szZ));

          for (pMeasurement = pChild->children; pMeasurement; pMeasurement = pMeasurement->next) {
            if (pMeasurement->type == XML_ELEMENT_NODE && !icXmlStrCmp(pMeasurement->name, "Measurement")) {
              const icChar *szDeviceCode = icXmlAttrValue(pMeasurement, "DeviceCode");
              const icChar *szValue = icXmlAttrValue(pMeasurement, "MeasValue");
              const icChar *szReserved = icXmlAttrValue(pMeasurement, "Reserved");

              if (!szDeviceCode || !szValue || !*szDeviceCode || !*szValue)
                return false;

              response.deviceCode = (icUInt16Number)atoi(szDeviceCode);
              response.measurementValue = icDtoF((icFloatNumber)atof(szValue));

              if (szReserved && *szReserved)
                response.reserved = atoi(szReserved);

              pResponseList->push_back(response);
            }
          }
          i++;
        }
      }
      m_ResponseCurves->push_back(curves);
    }
  }

  return true;
}


bool CIccTagXmlCurve::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  return ToXml(xml, icConvert16Bit, blanks);
}


bool CIccTagXmlCurve::ToXml(std::string &xml, icConvertType nType, std::string blanks/*= ""*/)
{
  const size_t bufSize = 40;
  char buf[bufSize];
  int i;

  // CWE-400/CWE-834: each encoding branch below walks m_Curve over m_nSize.
  // CIccTagCurve::Read() bounds m_nSize by the tag byte size and allocates
  // m_Curve to match (IccTagLut.cpp), so on a valid tag the walk never exceeds
  // the allocation; assert an explicit upper limit so a corrupted count can't
  // drive an unbounded serialization walk.
  const icUInt32Number nMaxCurveEntries = 0xffffff;
  if (m_nSize > nMaxCurveEntries)
    return false;

  if (!m_nSize) {
    xml += blanks + "<Curve/>\n";
  }
  else if (IsIdentity()) {
    xml += blanks + "<Curve IdentitySize=\"";
    snprintf(buf, bufSize, "%d", (unsigned int) m_nSize);
    xml += buf;
    xml += "\"/>\n";
  }
  else if (nType==icConvert8Bit) {
    xml += blanks + "<Curve>\n" + blanks;  
    for (i=0; i<(int)m_nSize; i++) {  
      if ( i && !(i%16)) {
        xml += "\n";
        xml += blanks;  
      }
      snprintf(buf, bufSize, " %3u", icFtoU8(m_Curve[i]));
      xml += buf;
    }
    xml += "\n";
    xml += blanks + "</Curve>\n";
  }
  else if (nType==icConvert16Bit || nType==icConvertVariable) {
    xml += blanks + "<Curve>\n" + blanks;
    for (i=0; i<(int)m_nSize; i++) {
      if (i && !(i%16)) {
        xml += "\n";
        xml += blanks + " ";
      }
      snprintf(buf, bufSize, " %5u", icFtoU16(m_Curve[i]));
      xml += buf;
    }
    xml += "\n";
    xml += blanks + "</Curve>\n";
  }
  else if (nType==icConvertFloat) {
    xml += blanks + "<Curve>\n" + blanks + "  ";
    for (i=0; i<(int)m_nSize; i++) {
      if (i && !(i%16)) {
        xml += "\n";
        xml += blanks + " ";
      }
      snprintf(buf, bufSize, " %13.8f", m_Curve[i]);
      xml += buf;
    }
    xml += "\n";
    xml += blanks + "</Curve>\n";
  }
  else
    return false;

  return true;
}


bool CIccTagXmlCurve::ParseXml(xmlNode *pNode, std::string &parseStr )
{
  return ParseXml(pNode, icConvert16Bit, parseStr);
}



bool CIccTagXmlCurve::ParseXml(xmlNode *pNode, icConvertType nType, std::string &parseStr)
{
  xmlNode *pCurveNode;
  pCurveNode = icXmlFindNode(pNode, "Curve");  

  if(pCurveNode){
    const char *filename = icXmlAttrValue(pCurveNode, "File");

    // file exists
    if (filename && filename[0]) {
      CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");
      if (!file){
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
        delete file;
        return false;
      }

      const char *format = icXmlAttrValue(pCurveNode, "Format");

      // format is text
      if (!strcmp(format, "text")) {
        icUInt32Number num;
        if (!icXmlValidateFileCount(file->GetLength(), num, parseStr, filename)) {
          delete file;
          return false;
        }
        // Shared ceiling: prevents num+1 from wrapping size_t and caps
        // downstream ParseTextArrayNum memory use. See IccUtilXml.h.
        if (num > icXmlMaxTextFileBytes) {
          parseStr += "'";
          parseStr += filename;
          parseStr += "' exceeds 256 MB limit.\n";
          delete file;
          return false;
        }
        // +1 and explicit NUL; use nothrow so the immediate
        // !buf check below actually fires on OOM.
        char *buf = new(std::nothrow) char[num + 1];

        if (!buf) {
          parseStr += "Out of memory allocating ";
          parseStr += std::to_string(num + 1);
          parseStr += " bytes for text buffer from '";
          parseStr += filename;
          parseStr += "'.\n";
          delete file;
          return false;
        }

        if (file->Read8(buf, num) != num) {
          parseStr += "Read error: could not read ";
          parseStr += std::to_string(num);
          parseStr += " bytes from '";
          parseStr += filename;
          parseStr += "'.\n";
          delete [] buf;
          delete file;
          return false;
        }
        buf[num] = '\0';  // NUL-terminate for ParseText downstream

        // lut8type
        if (nType == icConvert8Bit) {
          CIccUInt8Array data;

          //if (!data.ParseTextArray(buf)) {
          if (!data.ParseTextArrayNum(buf, num, parseStr)){
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";
            SetSize(0);
            delete [] buf;
            delete file;
            return false;
          }

          else {
            // CIccTagCurve::SetSize() answers a request above its 65536-entry cap
            // by freeing the table, setting m_nSize to 0 and returning true, so
            // the return value alone does not say whether the size was honoured.
            // GetData(0) then hands back NULL and the loop below writes
            // data.GetSize() floats through it.  The three inline-curve branches
            // further down this same function already test m_nSize against the
            // requested size for exactly this reason; these File=/Format="text"
            // branches never did, and are still the 2015 import code.  Match them,
            // and release the buffer and the file this branch owns and they do not.
            if (!SetSize(data.GetSize()) || m_nSize != data.GetSize()) {
              parseStr += "Curve in '";
              parseStr += filename;
              parseStr += "' has more entries than a curveType can hold.\n";
              delete[] buf;
              delete file;
              return false;
            }
            icUInt8Number *src = data.GetBuf();
            icFloatNumber *dst = GetData(0);

            icUInt32Number i;
            for (i=0; i<data.GetSize(); i++) {  
              *dst = (icFloatNumber)(*src) / 255.0f;              
              dst++;
              src++;
            }

            //if (i!=256) {
            //printf("Error! - Input/Output table does not have 256 entries.\n");                            
            //SetSize(0);
            //return false;
            //}
            delete[] buf;
            delete file;
            return true;
          }
        } 

        //lut16type
        else if (nType == icConvert16Bit || nType == icConvertVariable) {
          CIccUInt16Array data;

          //if (!data.ParseTextArray(buf)) {
          if (!data.ParseTextArrayNum(buf, num, parseStr)){
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";
            SetSize(0);
            delete[] buf;
            delete file;
            return false;
          }

          else {
            // See the lut8 branch above: an over-cap SetSize() reports success
            // with m_Curve NULL, and this is the loop that writes through it.
            if (!SetSize(data.GetSize()) || m_nSize != data.GetSize()) {
              parseStr += "Curve in '";
              parseStr += filename;
              parseStr += "' has more entries than a curveType can hold.\n";
              delete[] buf;
              delete file;
              return false;
            }

            icUInt16Number *src = data.GetBuf();
            icFloatNumber *dst = GetData(0);

            icUInt32Number i;
            for (i=0; i<data.GetSize(); i++) {
              *dst = (icFloatNumber)(*src) / 65535.0f; 
              dst++;
              src++;
            }
          }
          delete[] buf;
          delete file;
          return true;
        }

        //float type
        else if (nType == icConvertFloat){
          CIccFloatArray data;

          //if (!data.ParseTextArray(buf)) {
          if (!data.ParseTextArrayNum(buf, num, parseStr)){
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";
            SetSize(0);
            delete[] buf;
            delete file;
            return false;
          }

          else {
            // See the lut8 branch above: an over-cap SetSize() reports success
            // with m_Curve NULL, and this is the loop that writes through it.
            if (!SetSize(data.GetSize()) || m_nSize != data.GetSize()) {
              parseStr += "Curve in '";
              parseStr += filename;
              parseStr += "' has more entries than a curveType can hold.\n";
              delete[] buf;
              delete file;
              return false;
            }
            icFloatNumber *src = data.GetBuf();
            icFloatNumber *dst = GetData(0);

            icUInt32Number i;
            for (i=0; i<data.GetSize(); i++) {
              *dst = *src; 
              dst++;
              src++;
            }
          } 
          delete[] buf;
          delete file;
          return true;
        }        
        else {
          delete[] buf;
          delete file;
          return false;
        }
      }
      // format is binary
      else if (!strcmp(format, "binary")) {
        const char *order = icXmlAttrValue(pCurveNode, "Endian");
        bool little_endian = !strcmp(order, "little");    

        if (nType == icConvert8Bit){
          icUInt32Number num;
          icUInt8Number value;

          if (!icXmlValidateFileCount(file->GetLength(), num, parseStr, filename)) {
            delete file;
            return false;
          }

          if (!SetSize(num)) {
            parseStr += "Out of memory allocating curve data from '";
            parseStr += filename;
            parseStr += "'.\n";
            delete file;
            return false;
          }
          icFloatNumber *dst =  GetData(0);
          // The !SetSize(num) test above catches a genuine allocation failure, so
          // the only way to reach here with a NULL table is the over-cap refusal,
          // which returns true after emptying the curve.  The message used to say
          // "Curve data allocation failed", which describes the one case this
          // branch cannot see; nothing failed to allocate, the size was refused.
          if (num && !dst) {
            parseStr += "Curve in '";
            parseStr += filename;
            parseStr += "' has more entries than a curveType can hold.\n";
            delete file;
            return false;
          }
          icUInt32Number i;
          for (i=0; i<num; i++) {
            if (!file->Read8(&value)) { 
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;
              return false;
            } 
            *dst++ = (icFloatNumber)value / 255.0f;
          }         
          delete file;
          return true;
        }        
        else if (nType == icConvert16Bit || nType == icConvertVariable){
          icUInt32Number num;
          icUInt16Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (!icXmlValidateFileCount(file->GetLength() / sizeof(icUInt16Number), num, parseStr, filename)) {
            delete file;
            return false;
          }

          if (!SetSize(num)) {
            parseStr += "Out of memory allocating curve data from '";
            parseStr += filename;
            parseStr += "'.\n";
            delete file;
            return false;
          }
          icFloatNumber *dst = GetData(0);
          if (num && !dst) {
            parseStr += "Curve in '";
            parseStr += filename;
            parseStr += "' has more entries than a curveType can hold.\n";
            delete file;
            return false;
          }
          icUInt32Number i;
          for (i=0; i<num; i++) {
            if (!file->Read16(&value)) {  //this assumes data is big endian
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;              
              return false;
            }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
            if (little_endian) {
#else
            if (!little_endian) {
#endif
              icUInt8Number t = ptr[0];
              ptr[0] = ptr[1];
              ptr[1] = t;
            }
            *dst++ = (icFloatNumber)value / 65535.0f;
          }
          delete file;
          return true;
        }
        else if (nType == icConvertFloat) {
          icUInt32Number num;
          icFloat32Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (!icXmlValidateFileCount(file->GetLength() / sizeof(icFloat32Number), num, parseStr, filename)) {
            delete file;
            return false;
          }

          if (!SetSize(num)) {
            parseStr += "Out of memory allocating curve data from '";
            parseStr += filename;
            parseStr += "'.\n";
            delete file;
            return false;
          }
          icFloatNumber *dst = GetData(0);
          if (num && !dst) {
            parseStr += "Curve in '";
            parseStr += filename;
            parseStr += "' has more entries than a curveType can hold.\n";
            delete file;
            return false;
          }

          icUInt32Number i;
          for (i=0; i<num; i++) {
            if (!file->ReadFloat32Float(&value)) { //assumes data is big endian
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;
              return false;
            }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
            if (little_endian) {
#else
            if (!little_endian) {
#endif
              icUInt8Number tmp;
              tmp = ptr[0]; ptr[0] = ptr[3]; ptr[3] = tmp;
              tmp = ptr[1]; ptr[1] = ptr[2]; ptr[2] = tmp;
            }
            *dst++ = value;
          }        
          delete file;
          return true;
        }
        else { //not 8bit/16bit/float        
          delete file;
          return false;
        } 
      }       
      else {//not text/binary
        delete file;
        return false;
      }
    }    
    // no file
    else{
      if (nType == icConvert8Bit){
        CIccUInt8Array data;      

        if (!data.ParseArray(pCurveNode->children)) {
          const char *szSize = icXmlAttrValue(pCurveNode, "IdentitySize");

          if (szSize && *szSize) {
            icUInt32Number nSize = (icUInt32Number)atol(szSize);
            if (nSize <= 1)
              return false;
            
            if (!SetSize(nSize))
              return false;

            if (m_nSize == nSize) {
              icUInt32Number j;
              icFloatNumber *dst = GetData(0);
              for (j=0; j<nSize; j++) {
                *dst = (icFloatNumber)(j) / (icFloatNumber)(nSize-1); 
                dst++;
              }
            }
            else
              return false;
          }
          else { //Identity curve with size=0
            SetSize(0);
          }
        }
        else {
          if (!SetSize(data.GetSize()) || m_nSize != data.GetSize())
            return false;

          icUInt32Number j;
          icUInt8Number *src = data.GetBuf();
          icFloatNumber *dst = GetData(0);
          for (j=0; j<data.GetSize(); j++) {
            *dst = (icFloatNumber)(*src) / 255.0f; 
            dst++;
            src++;
          }          

          //if (j!=256) {
          //printf("Error! - Input/Output table does not have 256 entries.\n");                            
          //SetSize(0);
          //return false;
          //}
        }
        return true;
      }

      else if (nType == icConvert16Bit || nType == icConvertVariable){
        CIccUInt16Array data;

        if (!data.ParseArray(pCurveNode->children)) {
          const char *szSize = icXmlAttrValue(pCurveNode, "IdentitySize");

          if (szSize && *szSize) {
            icUInt32Number nSize = (icUInt32Number)atol(szSize);
            if (nSize <= 1)
              return false;
            
            if (!SetSize(nSize))
              return false;

            if (m_nSize == nSize) {
              icUInt32Number j;
              icFloatNumber *dst = GetData(0);
              for (j=0; j<nSize; j++) {
                *dst = (icFloatNumber)(j) / (icFloatNumber)(nSize-1); 
                dst++;
              }
            }
            else
              return false;
          }
          else { //Identity curve with size=0
            SetSize(0);
          }
        }              
        else {
          if (!SetSize(data.GetSize()) || m_nSize != data.GetSize())
            return false;

          icUInt32Number j;
          icUInt16Number *src = data.GetBuf();
          icFloatNumber *dst = GetData(0);
          for (j=0; j<data.GetSize(); j++) {
            *dst = (icFloatNumber)(*src) / 65535.0f; 
            dst++;
            src++;
          }
        }              
        return true;
      }      
      else if (nType == icConvertFloat){
        CIccFloatArray data;

        if (!data.ParseArray(pCurveNode->children)) {
          const char *szSize = icXmlAttrValue(pCurveNode, "IdentitySize");

          if (szSize && *szSize) {
            icUInt32Number nSize = (icUInt32Number)atol(szSize);
            if (nSize <= 1)
              return false;

            if (!SetSize(nSize))
              return false;

            if (m_nSize == nSize) {
              icUInt32Number j;
              icFloatNumber *dst = GetData(0);
              for (j=0; j<nSize; j++) {
                *dst = (icFloatNumber)(j) / (icFloatNumber)(nSize-1); 
                dst++;
              }
            }
            else
              return false;
          }
          else { //Identity curve with size=0
            SetSize(0);
          }
        }
        else {
          if (!SetSize(data.GetSize()) || m_nSize != data.GetSize())
            return false;

          icUInt32Number j;
          icFloatNumber *src = data.GetBuf();
          icFloatNumber *dst = GetData(0);
          for (j=0; j<data.GetSize(); j++) {
            *dst = *src; 
            dst++;
            src++;
          }
        }        
        return true;
      }
      // unsupported encoding
      else
        return false;
    }
  }
  return false;
}


bool CIccTagXmlParametricCurve::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 80;
  char buf[bufSize];
  int i;

  snprintf(buf, bufSize, "<ParametricCurve FunctionType=\"%d\"", m_nFunctionType);
  xml += blanks + buf;

  if (m_nReserved2) {
    snprintf(buf, bufSize, " Reserved=\"%d\"", m_nReserved2);
    xml += buf;
  }
  xml += ">\n";
  xml += blanks + " ";

  for (i=0; i<(int)m_nNumParam; i++) {
    snprintf(buf, bufSize, " " icXmlFloatFmt, m_dParam[i]);
    xml += buf;
  }
  xml += "\n";

  snprintf(buf, bufSize, "</ParametricCurve>\n");
  xml += blanks + buf;

  return true;
}

bool CIccTagXmlParametricCurve::ToXml(std::string &xml, icConvertType /*nType*/, std::string blanks/* = */)
{
  return ToXml(xml, blanks);
}

bool CIccTagXmlParametricCurve::ParseXml(xmlNode *pNode, icConvertType /*nType*/, std::string &parseStr)
{
  return ParseXml (pNode, parseStr);
}

bool CIccTagXmlParametricCurve::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlNode *pCurveNode = NULL;   //  icXmlFindNode(pNode->children, "ParametricCurve");  // this appears to do the same search as below, but value is never used

  for (pCurveNode = pNode; pCurveNode; pCurveNode=pCurveNode->next) {
    if (pCurveNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pCurveNode->name, "ParametricCurve")) {

        const char *functionType = icXmlAttrValue(pCurveNode, "FunctionType");

        if (!functionType)
          return false;    

        // ICC.1 encodes the parametric curve function type as 0-4 (see the
        // switch in CIccTagParametricCurve::SetFunctionType), so parse it as a
        // bounded unsigned value rather than through atoi().
        //
        // atoi() returned a signed int that was then narrowed to
        // SetFunctionType()'s icUInt16Number parameter, which is undefined for
        // anything outside 0..65535 and silently truncating inside it (#1851):
        //
        //   IccTagXml.cpp:3538:30: runtime error: implicit conversion from type
        //   'int' of value 96948242 (32-bit, signed) to type 'icUInt16Number'
        //   (aka 'unsigned short') changed the value to 20498 (16-bit, unsigned)
        //
        // Truncation is the part that matters beyond the sanitizer diagnostic:
        // any input congruent to a legal type mod 65536 was laundered into that
        // legal type, so "65540" was accepted as function type 4 and the profile
        // was written as though the file had said 4.  atoi() also stops at the
        // first non-digit, so "96948242 1.23 0.44994" parsed as a number at all.
        //
        // The existing `if (!SetFunctionType(...))` guard could not catch either
        // case: SetFunctionType is documented "Return: always true!!" and ends in
        // an unconditional `return true`, so this test has never rejected
        // anything.  Out-of-range types fell through its switch to nNumParam = 0,
        // leaving the truncated value stored in m_nFunctionType and written back
        // out by Write16().  icXmlParseU16 rejects trailing garbage, a leading
        // '-', and anything above the bound, which makes the guard live.
        icUInt16Number nFunctionType = 0;
        if (!icXmlParseU16(functionType, nFunctionType, 4) ||
            !SetFunctionType(nFunctionType)){
          return false;
        }
        CIccFloatArray data;

        if (!data.ParseArray(pCurveNode->children)){
          return false;       
        }

        if (data.GetSize()!=GetNumParam()){
          return false;
        }

        icUInt32Number j;
        icFloatNumber *dParams = GetParams();
        icFloatNumber *dataBuf = data.GetBuf();
        for (j=0; j<data.GetSize(); j++) {
          dParams[j] = dataBuf[j];   
        }

        xmlAttr *reserved2 = icXmlFindAttr(pCurveNode, "Reserved");

        if (reserved2) {
          m_nReserved2 = (icUInt16Number)icXmlAttrToUInt(icXmlAttrValue(reserved2));
        }
        return true;
      }
    }
  }
  return false;
}


bool icCurvesToXml(std::string &xml, const char *szName, CIccCurve **pCurves, int numCurve, icConvertType nType, std::string blanks)
{
  if (pCurves) {
    int i;

    xml += blanks + "<" + szName + ">\n";
    for (i=0; i<numCurve; i++) {
      IIccExtensionTag *pTag = pCurves[i]->GetExtension();
      if (!pTag || strcmp(pTag->GetExtDerivedClassName(), "CIccCurveXml"))
        return false;

      if (!((CIccCurveXml *)pTag)->ToXml(xml, nType, blanks + "  "))
        return false;
    }
    xml += blanks + "</" + szName + ">\n";
  }
  return true;
}


bool CIccTagXmlSegmentedCurve::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  if (m_pCurve)
    return CIccSegmentedCurveXml(m_pCurve).ToXml(xml, blanks);

  return true;
}


bool CIccTagXmlSegmentedCurve::ToXml(std::string &xml, icConvertType /*nType*/, std::string blanks/* = */)
{
  return ToXml(xml, blanks);
}


bool CIccTagXmlSegmentedCurve::ParseXml(xmlNode *pNode, std::string &parseStr )
{
  xmlNode *pCurveNode = icXmlFindNode(pNode, "SegmentedCurve");
  if (pCurveNode) {
    CIccSegmentedCurveXml *pCurve = new (std::nothrow) CIccSegmentedCurveXml();
    
    if (pCurve) {
      if (pCurve->ParseXml(pCurveNode, parseStr)) {
        SetCurve(pCurve);
        return true;
      }
      else {
        delete pCurve;
        return false;
      }
    }
    parseStr += "Unable to allocate Segmented Curve\n";
    return false;
  }
  parseStr += "Unable to find Segmented Curve\n";
  return false;
}


bool CIccTagXmlSegmentedCurve::ParseXml(xmlNode *pNode, icConvertType /*nType*/, std::string &parseStr)
{
  return ParseXml(pNode, parseStr);
}


bool icMatrixToXml(std::string &xml, CIccMatrix *pMatrix, std::string blanks)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  xml += blanks + "<Matrix\n";

  snprintf(buf, bufSize, "  e1=\"" icXmlFloatFmt "\" e2=\"" icXmlFloatFmt "\" e3=\"" icXmlFloatFmt "\"\n", pMatrix->m_e[0], pMatrix->m_e[1], pMatrix->m_e[2]);
  xml += blanks + buf;

  snprintf(buf, bufSize, "  e4=\"" icXmlFloatFmt "\" e5=\"" icXmlFloatFmt "\" e6=\"" icXmlFloatFmt "\"\n", pMatrix->m_e[3], pMatrix->m_e[4], pMatrix->m_e[5]);
  xml += blanks + buf;

  snprintf(buf, bufSize, "  e7=\"" icXmlFloatFmt "\" e8=\"" icXmlFloatFmt "\" e9=\"" icXmlFloatFmt "\"", pMatrix->m_e[6], pMatrix->m_e[7], pMatrix->m_e[8]);
  xml += blanks + buf;

  if (pMatrix->m_bUseConstants) {
    xml += "\n";
    snprintf(buf, bufSize, "  e10=\"" icXmlFloatFmt "\" e11=\"" icXmlFloatFmt "\" e12=\"" icXmlFloatFmt "\"", pMatrix->m_e[9], pMatrix->m_e[10], pMatrix->m_e[11]);
    xml += blanks + buf;
  }
  xml += "/>\n";
  return true;
}

// for luts
bool icMBBToXml(std::string &xml, CIccMBB *pMBB, icConvertType nType, std::string blanks="", bool bSaveGridPoints=false)
{
  //blanks += "  ";
  const size_t bufSize = 256;
  char buf[bufSize];

  snprintf(buf, bufSize, "<Channels InputChannels=\"%d\" OutputChannels=\"%d\"/>\n", pMBB->InputChannels(), pMBB->OutputChannels());
  xml += blanks + buf;

  if (pMBB->IsInputMatrix()) {
    if (pMBB->SwapMBCurves()) {
      if (pMBB->GetMatrix()) {
        // added if-statement 
        if (!icMatrixToXml(xml, pMBB->GetMatrix(), blanks)) {
          return false;
        }
      }

      if (pMBB->GetCurvesB()) {
        // added if-statement 
        if (!icCurvesToXml(xml, "BCurves", pMBB->GetCurvesM(), pMBB->InputChannels(), nType, blanks)) {
          return false;
        }
      }
    }
    else {
      if (pMBB->GetCurvesB()) {
        // added if-statement 
        if (!icCurvesToXml(xml, "BCurves", pMBB->GetCurvesB(), pMBB->InputChannels(), nType, blanks)) {
          return false;
        }
      }

      if (pMBB->GetMatrix()) {
        // added if-statement 
        if (!icMatrixToXml(xml, pMBB->GetMatrix(), blanks)) {
          return false;
        }
      }

      if (pMBB->GetCurvesM()) {
        // added if-statement 
        // hard coding the channel count to 3 could cause a buffer overread, as seen below
        if (!icCurvesToXml(xml, "MCurves", pMBB->GetCurvesM(), pMBB->InputChannels(), nType, blanks)){
          return false;
        }
      }
    }

    if (pMBB->GetCLUT()) {
      // added if-statement 
      if (!icCLUTToXml(xml, pMBB->GetCLUT(), nType, blanks, bSaveGridPoints)){
        return false;
      }
    }

    if (pMBB->GetCurvesA()) {
      // added if-statement 
      if (!icCurvesToXml(xml, "ACurves", pMBB->GetCurvesA(), pMBB->OutputChannels(), nType, blanks)){
        return false;
      }
    }
  }
  else {
    // is an output matrix
    if (pMBB->GetCurvesA()) {
      // added if-statement 
      if (!icCurvesToXml(xml, "ACurves", pMBB->GetCurvesA(), pMBB->InputChannels(), nType, blanks)){
        return false;
      }
    }

    if (pMBB->GetCLUT()) {
      // added if-statement 
      if (!icCLUTToXml(xml, pMBB->GetCLUT(), nType, blanks, bSaveGridPoints)){
        return false;
      }
    }

    if (pMBB->GetCurvesM()) {
      // added if-statement
      // hard coding the channel count to 3 causes a buffer overread when output channels == 1
      if (!icCurvesToXml(xml, "MCurves", pMBB->GetCurvesM(), pMBB->OutputChannels(), nType, blanks)){
        return false;
      }
    }

    if (pMBB->GetMatrix()) {
      // added if-statement 
      if (!icMatrixToXml(xml, pMBB->GetMatrix(), blanks)) {
        return false;
      }
    }

    if (pMBB->GetCurvesB()) {
      // added if-statement 
      if (!icCurvesToXml(xml, "BCurves", pMBB->GetCurvesB(), pMBB->OutputChannels(), nType, blanks)){
        return false;
      }
    }
  }
  return true;
}


bool icCurvesFromXml(LPIccCurve *pCurve, icUInt32Number nChannels, xmlNode *pNode, icConvertType nType, std::string &parseStr)
{
  icUInt32Number i;
  xmlNode *pCurveNode;
  const size_t numSize = 40;
  char num[numSize];

  for (i=0, pCurveNode = pNode; i<nChannels && pCurveNode; pCurveNode=pCurveNode->next) {
    if (pCurveNode->type==XML_ELEMENT_NODE) {
      CIccCurve *pCurveTag = NULL;
      if (!icXmlStrCmp(pCurveNode->name, "Curve")) {
        pCurveTag = new (std::nothrow) CIccTagXmlCurve;
      }
      else if (!icXmlStrCmp(pCurveNode->name, "ParametricCurve")) {
        pCurveTag = new (std::nothrow) CIccTagXmlParametricCurve();
      }

      if (pCurveTag) {
        IIccExtensionTag *pExt = pCurveTag->GetExtension();

        if (pExt) {
          if (!strcmp(pExt->GetExtDerivedClassName(), "CIccCurveXml")) {
            CIccCurveXml *pCurveXml = (CIccCurveXml *)pExt;

            if (pCurveXml->ParseXml(pCurveNode, nType, parseStr)) {
              pCurve[i] = pCurveTag;
              i++;
            }
            // added else statement
            else  {
              parseStr += "Unable to parse curve at Line";
              snprintf(num, numSize, "%d\n", pCurveNode->line);
              parseStr += num;

              delete pCurveTag;
              return false;
            }
          }
          else if (!strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
            CIccTagXml *pXmlTag = (CIccTagXml *)pExt;

            if (pXmlTag->ParseXml(pCurveNode, parseStr)) {
              pCurve[i] = pCurveTag;
              i++;
            }
            // added else statement
            else {
              parseStr += "Unable to parse curve tag at Line";
              snprintf(num, numSize, "%d\n", pCurveNode->line);
              parseStr += num;

              delete pCurveTag;
              return false;
            }
          }
          else {
            delete pCurveTag;
            return false;
          }
        }
      }
    }
  }
  
  if (i != nChannels) {
    parseStr += "Channel number mismatch!\n";
  }
  return i==nChannels;
}

bool icMatrixFromXml(CIccMatrix *pMatrix, xmlNode *pNode)
{
  memset(pMatrix->m_e, 0, sizeof(pMatrix->m_e));
  pMatrix->m_bUseConstants = false;

  const size_t nameSize = 15;
  char attrName[15];
  int i;

  for (i=0; i<9; i++) {
    snprintf(attrName, nameSize, "e%d", i+1);
    xmlAttr *attr = icXmlFindAttr(pNode, attrName);
    if (attr) {
      pMatrix->m_e[i] = (icFloatNumber)atof(icXmlAttrValue(attr));
    }
  }
  for (i=9; i<12; i++) {
    snprintf(attrName, nameSize, "e%d", i+1);
    xmlAttr *attr = icXmlFindAttr(pNode, attrName);
    if (attr) {
      pMatrix->m_e[i] = (icFloatNumber)atof(icXmlAttrValue(attr));
      pMatrix->m_bUseConstants = true;
    }
  }
  return true;
}

CIccCLUT *icCLutFromXml(xmlNode *pNode, int nIn, int nOut, icConvertType nType,  std::string &parseStr)
{
  CIccCLUT *pCLUT = NULL;

  int nPrecision = 2;
  if (nType == icConvert8Bit)
    nPrecision = 1;  

  icUInt8Number nInput = (icUInt8Number)nIn;
  icUInt16Number nOutput = (icUInt16Number)nOut;

  pCLUT = new (std::nothrow) CIccCLUT(nInput, nOutput, nPrecision);

  if (!pCLUT){
    parseStr += "Error in creating CLUT Table. Check values of Precision, InputChannel, or OutputChannels.\n";
    return NULL;
  }

  xmlNode *grid = icXmlFindNode(pNode->children, "GridPoints");
  if (grid) {
    CIccUInt8Array points;

    if (points.ParseArray(grid->children)) {
      if (points.GetSize() < nInput) {
        parseStr += "Error! - The number of GridPoints and InputChannels do not match.\n";
        delete pCLUT;
        return NULL;
      }
      if (!pCLUT->Init(points.GetBuf())) {
        parseStr += "Error in setting the size of GridPoints. Check values of GridPoints, InputChannel, or OutputChannels.\n";
        delete pCLUT;
        return NULL;
      }
    }
    else {
      parseStr += "Error parsing GridPoints.\n";
      delete pCLUT;
      return NULL;
    }
  }
  else {
    icUInt8Number nGridGranularity = 0;

    xmlAttr *gridGranularity = icXmlFindAttr(pNode, "GridGranularity");

    if (gridGranularity) {
      // A CLUT stores one grid point count per input channel in an
      // icUInt8Number, so 255 is what the field can hold and the cast that
      // stood here silently reduced anything larger modulo 256 --
      // GridGranularity="258" became 2, an ordinary grid that pCLUT->Init()
      // accepted, producing a profile byte-identical to the one "2" gives
      // (#1909). Refuse the count rather than substitute one.
      if (!icXmlParseU8(icXmlAttrValue(gridGranularity), nGridGranularity)) {
        parseStr += "Invalid GridGranularity value.\n";
        delete pCLUT;
        return NULL;
      }
    }
    else {
      delete pCLUT;
      return NULL;
    }
    if (!pCLUT->Init(nGridGranularity)) {
      parseStr += "Error in setting the size of GridGranularity. Check values of GridGranularity, InputChannel, or OutputChannels.\n";
      delete pCLUT;
      return NULL;
    }
  }

  xmlNode *table = icXmlFindNode(pNode->children, "TableData");

  if (table) {
    if (nType == icConvertVariable) {
      const char *precision = icXmlAttrValue(table, "Precision");
      if (precision && atoi(precision) == 1) {
        nType = icConvert8Bit;
        pCLUT->SetPrecision(1);
      }
      else {
        nType = icConvert16Bit;
        pCLUT->SetPrecision(2);
      }
    }

    const char *filename = icXmlAttrValue(table, "Filename");
    if (!filename || !filename[0]) {
      filename = icXmlAttrValue(table, "File");
    }

    if (filename && filename[0]) {
      CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");

      if (!file) {
        // added error message
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
        delete pCLUT;
        return NULL;
      }

      const char *format = icXmlAttrValue(table, "Format");

      if (!strcmp(format, "text")) {
        icUInt32Number num;
        if (!icXmlValidateFileCount(file->GetLength(), num, parseStr, filename)) {
          delete file;
          delete pCLUT;
          return NULL;
        }
        char *buf = (char *)malloc(num);

        if (!buf) {  
          perror("Memory Error");
          parseStr += "'";
          parseStr += filename;
          parseStr += "' may not be a valid text file.\n";
          delete file;
          delete pCLUT;
          return NULL;
        }

        //Allow for different encoding in text file than implied by the table type
        const char *encoding = icXmlAttrValue(table, "FileEncoding");

        if (!strcmp(encoding, "int8"))
          nType = icConvert8Bit;
        else if (!strcmp(encoding, "int16"))
          nType = icConvert16Bit;
        else if (!strcmp(encoding, "float"))
          nType = icConvertFloat;
        else if (encoding[0]) {
          parseStr+= "Unknown encoding \"";
          parseStr+= encoding;
          parseStr+= "\" - using default encoding\n";
        }

        if (file->Read8(buf, num)!=num) { 
          perror("Read-File Error");
          parseStr += "'";
          parseStr += filename;
          parseStr += "' may not be a valid text file.\n";
          free(buf);
          delete file;
          delete pCLUT;
          return NULL;
        }

        if (nType == icConvert8Bit) {
          CIccUInt8Array data;

          if (!data.ParseTextArrayNum(buf, num, parseStr)) {
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (data.GetSize()!=(size_t)pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {     
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid text file.\n";
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }
          icUInt8Number *src = data.GetBuf();
          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<data.GetSize(); i++) {
            *dst++ = (icFloatNumber)(*src++) / 255.0f;
          }
        }
        else if (nType == icConvert16Bit) {
          CIccUInt16Array data;

          if (!data.ParseTextArrayNum(buf, num, parseStr)) {
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";      
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (data.GetSize()!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid text file.\n";
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }
          icUInt16Number *src = data.GetBuf();
          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<data.GetSize(); i++) {
            *dst++ = (icFloatNumber)(*src++) / 65535.0f;
          }
        }
        else if (nType == icConvertFloat) {
          CIccFloatArray data;

          if (!data.ParseTextArrayNum(buf, num, parseStr)) {
            parseStr += "File '";
            parseStr += filename;
            parseStr += "' is not a valid text file.\n";
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (data.GetSize()!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid text file.\n";
            free(buf);
            delete file;
            delete pCLUT;
            return NULL;
          }
          icFloatNumber *src = data.GetBuf();
          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<data.GetSize(); i++) {
            *dst++ = *src++;
          }
        }
        else {
          parseStr += "Error! Unknown text data type.\n";
          free(buf);
          delete file;
          delete pCLUT;
          return NULL;
        }
        free(buf);
      }
      else if (!strcmp(format, "binary")) {
        const char *order = icXmlAttrValue(table, "Endian");
        bool little_endian = !strcmp(order, "little");

        if (nType==icConvertVariable) {
          //Allow encoding to be defined
          const char *encoding = icXmlAttrValue(table, "FileEncoding");

          if (!strcmp(encoding, "int8"))
            nType = icConvert8Bit;
          else if (!strcmp(encoding, "int16"))
            nType = icConvert16Bit;
          else if (!strcmp(encoding, "float"))
            nType = icConvertFloat;
          else if (encoding[0]) {
            parseStr+= "Unknown encoding \"";
            parseStr+= encoding;
            parseStr+= "\" - using int16.\n";
            nType = icConvert16Bit;
          }
          else {
            parseStr+= "CLUT TableData Encoding type not specified.\n";
          }
        }

        if (nType == icConvert8Bit){
          size_t num = file->GetLength();
          icUInt32Number count;
          icUInt8Number value;
          // if number of entries in file is not equal to size of CLUT table, flag as error
          if (num!=(size_t)pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (!icXmlValidateFileCount(num, count, parseStr, filename)) {
            delete file;
            delete pCLUT;
            return NULL;
          }

          icFloatNumber *dst = pCLUT->GetData(0);
          icUInt32Number i;
          for (i=0; i<count; i++) {
            if (!file->Read8(&value)) {
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;
              delete pCLUT;
              return NULL;
            }      
            *dst++ = (icFloatNumber)value / 255.0f;
          }
        }
        else if (nType == icConvert16Bit){
          size_t num = file->GetLength() / sizeof(icUInt16Number);
          icUInt32Number count;
          icUInt16Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (num!=(size_t)pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (!icXmlValidateFileCount(num, count, parseStr, filename)) {
            delete file;
            delete pCLUT;
            return NULL;
          }

          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<count; i++) {
            if (!file->Read16(&value)) {  //this assumes data is big endian
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;
              delete pCLUT;
              return NULL;
            }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
            if (little_endian) {
#else
            if (!little_endian) {
#endif
              icUInt8Number t = ptr[0];
              ptr[0] = ptr[1];
              ptr[1] = t;
            }
            *dst++ = (icFloatNumber)value / 65535.0f;
          }
        }
        else if (nType == icConvertFloat){
          size_t num = file->GetLength()/sizeof(icFloat32Number);
          icUInt32Number count;
          icFloat32Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (num!=(size_t)pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }

          if (!icXmlValidateFileCount(num, count, parseStr, filename)) {
            delete file;
            delete pCLUT;
            return NULL;
          }

          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<count; i++) {
            if (!file->ReadFloat32Float(&value)) {  //this assumes data is big endian
              perror("Read-File Error");
              parseStr += "'";
              parseStr += filename;
              parseStr += "' may not be a valid binary file.\n";
              delete file;
              delete pCLUT;
              return NULL;
            }
#ifdef ICC_BYTE_ORDER_LITTLE_ENDIAN
            if (little_endian) {
#else
            if (!little_endian) {
#endif
              icUInt8Number tmp;
              tmp = ptr[0]; ptr[0] = ptr[3]; ptr[3] = tmp;
              tmp = ptr[1]; ptr[1] = ptr[2]; ptr[2] = tmp;
            }
            *dst++ = value;
          }
        }
        else {
          parseStr += "Error! Unknown binary data type.\n";
          delete file;
          delete pCLUT;
          return NULL;
        }

      }
      else {
        parseStr += "Error! Unknown Format type.\n";
        delete pCLUT;
        return NULL;
      }

      delete file;
    }
    else { // no file
      if (nType == icConvert8Bit) {
        CIccUInt8Array data;

        if (!data.ParseArray(table->children)) {
          parseStr += "Error! - unable to parse data in CLUT.\n";
          delete pCLUT;
          return NULL;
        }

        if (data.GetSize()!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
          parseStr += "Error! - Number of entries is not equal to the size of the CLUT Table.\n";  
          delete pCLUT;
          return NULL;
        }
        icUInt8Number *src = data.GetBuf();
        icFloatNumber *dst = pCLUT->GetData(0);

        icUInt32Number i;
        for (i=0; i<data.GetSize(); i++) {
          *dst++ = (icFloatNumber)(*src++) / 255.0f;
        }
      }
      else if (nType == icConvert16Bit || nType==icConvertVariable) {
        CIccUInt16Array data;

        if (!data.ParseArray(table->children)) {
          parseStr += "Error! - unable to parse data in CLUT.\n";
          delete pCLUT;
          return NULL;
        }

        if (data.GetSize()!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
          parseStr += "Error! - Number of entries is not equal to the size of the CLUT Table.\n";  
          delete pCLUT;
          return NULL;
        }
        icUInt16Number *src = data.GetBuf();
        icFloatNumber *dst = pCLUT->GetData(0);

        icUInt32Number i;
        for (i=0; i<data.GetSize(); i++) {
          *dst++ = (icFloatNumber)(*src++) / 65535.0f;
        }    
      }      
      else if (nType == icConvertFloat){
        CIccFloatArray data;

        if (!data.ParseArray(table->children)) {
          parseStr += "Error! - unable to parse data in CLUT.\n";
          delete pCLUT;
          return NULL;
        }

        if (data.GetSize()!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
          parseStr += "Error! - Number of entries is not equal to the size of the CLUT Table.\n";  
          delete pCLUT;
          return NULL;
        }
        icFloatNumber *src = data.GetBuf();
        icFloatNumber *dst = pCLUT->GetData(0);

        icUInt32Number i;
        for (i=0; i<data.GetSize(); i++) {
          *dst++ = *src++;
        }
      }
      else {
        parseStr += "Error! Unknown table data type.";
        delete pCLUT;
        return NULL;
      }
    }
  }
  else {
    parseStr += "Error! Cannot find table data.";
    delete pCLUT;
    return NULL;
  }

  return pCLUT;
}

bool icMBBFromXml(CIccMBB *pMBB, xmlNode *pNode, icConvertType nType, std::string &parseStr)
{
  xmlNode *pChannels = icXmlFindNode(pNode, "Channels");

  if (!pChannels)
    return false;

  xmlAttr *in = icXmlFindAttr(pChannels, "InputChannels");
  xmlAttr *out = icXmlFindAttr(pChannels, "OutputChannels");

  if (!in || !out)
    return false;

  // The 1..15 range checks below already refused everything that could not be
  // used, so these two were never narrowing.  atoi() is still undefined when the
  // attribute does not fit an int, though, so the parse happens through
  // icXmlParseU16 first and the range checks then apply to a value that is
  // defined.  The 15-channel ceiling is passed as the parser's max_value so an
  // over-large count is rejected at the parse rather than after it; the explicit
  // "< 1" test below still rejects 0, which the unsigned parse accepts.
  icUInt16Number nInVal = 0, nOutVal = 0;
  if (!icXmlParseU16(icXmlAttrValue(in), nInVal, 15) ||
      !icXmlParseU16(icXmlAttrValue(out), nOutVal, 15))
    return false;

  int nIn = (int)nInVal;
  int nOut = (int)nOutVal;

  // must have at least 1 input and 1 output
  if (nIn < 1 || nOut < 1)
    return false;

  pMBB->Init(nIn, nOut);

  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "ACurves") && !pMBB->GetCurvesA()) {
        LPIccCurve *pCurves = pMBB->NewCurvesA();
        if (!icCurvesFromXml(pCurves, !pMBB->IsInputB() ? nIn : nOut, pNode->children, nType, parseStr)) {
          parseStr += "Error! - Failed to parse ACurves.\n";
          return false;
        }
      }
      else if (!icXmlStrCmp(pNode->name, "BCurves") && !pMBB->GetCurvesB()) {
        LPIccCurve *pCurves = pMBB->NewCurvesB();
        if (!icCurvesFromXml(pCurves, pMBB->IsInputB() ? nIn : nOut, pNode->children, nType, parseStr)) {
          parseStr += "Error! - Failed to parse BCurves.\n";
          return false;
        }
      }
      else if (!icXmlStrCmp(pNode->name, "MCurves") && !pMBB->GetCurvesM()) {
        LPIccCurve *pCurves = pMBB->NewCurvesM();
        if (!icCurvesFromXml(pCurves, pMBB->IsInputMatrix() ? nIn : nOut, pNode->children, nType, parseStr)) {
          parseStr += "Error! - Failed to parse MCurves.\n";
          return false;
        }
      }
      else if (!icXmlStrCmp(pNode->name, "Matrix") && !pMBB->GetMatrix()) {
        CIccMatrix *pMatrix = pMBB->NewMatrix();
        if (!icMatrixFromXml(pMatrix, pNode)) {
          parseStr += "Error! - Failed to parse Matrix.\n";
          return false;
        }
      }
      else if (!icXmlStrCmp(pNode->name, "CLUT") && !pMBB->GetCLUT()) {
        CIccCLUT *pCLUT = icCLutFromXml(pNode, nIn, nOut, nType, parseStr);
        if (pCLUT) {
          if (!pMBB->SetCLUT(pCLUT)) {
            parseStr += "Error! - Failed to set CLUT to LUT.\n";
            return false;
          }
        }
        else {
          parseStr += "Error! - Failed to parse CLUT.\n";
          return false;
        }
      }
    }
  }

  return true;
}

bool CIccTagXmlLutAtoB::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  bool rv = icMBBToXml(xml, this, icConvertVariable, blanks, true);

  return rv;
}


bool CIccTagXmlLutAtoB::ParseXml(xmlNode *pNode, std::string &parseStr)
{

  if (pNode) {
    return icMBBFromXml(this, pNode, icConvertVariable, parseStr);
  }

  return false;
}


bool CIccTagXmlLutBtoA::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  bool rv = icMBBToXml(xml, this, icConvertVariable, blanks, true);

  return rv;
}


bool CIccTagXmlLutBtoA::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  if (pNode) {
    return icMBBFromXml(this, pNode, icConvertVariable, parseStr);
  }

  return false;
}


bool CIccTagXmlLut8::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  bool rv = icMBBToXml(xml, this, icConvert8Bit, blanks, false);

  return rv;
}


bool CIccTagXmlLut8::ParseXml(xmlNode *pNode, std::string &parseStr)
{

  if (pNode) {
    return icMBBFromXml(this, pNode, icConvert8Bit, parseStr);
  }
  return false;
}


bool CIccTagXmlLut16::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  bool rv = icMBBToXml(xml, this, icConvert16Bit, blanks, false);

  return rv;
}


bool CIccTagXmlLut16::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  if (pNode) {
    return icMBBFromXml(this, pNode, icConvert16Bit, parseStr);
  }
  return false;
}


bool CIccTagXmlMultiProcessElement::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;
  const size_t lineSize = 256;
  char line[lineSize];

  CIccMultiProcessElementList::iterator i;

  snprintf(line, lineSize, "<MultiProcessElements InputChannels=\"%d\" OutputChannels=\"%d\">\n", NumInputChannels(), NumOutputChannels());
  xml += blanks + line;

  for (i=m_list->begin(); i!=m_list->end(); i++) {
    if (i->ptr) {
      CIccMultiProcessElement *pMpe = i->ptr;

      IIccExtensionMpe *pMpeExt = pMpe->GetExtension();

      if (pMpeExt) {
        if (!strcmp(pMpeExt->GetExtClassName(), "CIccMpeXml")) {
          CIccMpeXml *pMpeXml = (CIccMpeXml*)pMpeExt;

          pMpeXml->ToXml(xml, blanks + "  ");
        }
        else {
          return false;
        }
      }
      else {
        return false;
      }
    }
  }
  xml += blanks + "</MultiProcessElements>\n";
  return true;
}


CIccMultiProcessElement *CIccTagXmlMultiProcessElement::CreateElement(const icChar *szElementNodeName)
{
  if (!strcmp(szElementNodeName, "CurveSetElement")) {
    return new(std::nothrow) CIccMpeXmlCurveSet;
  }
  if (!strcmp(szElementNodeName, "MatrixElement")) {
    return new(std::nothrow) CIccMpeXmlMatrix;
  }
  if (!strcmp(szElementNodeName, "CLutElement")) {
    return new(std::nothrow) CIccMpeXmlCLUT;
  }
  if (!strcmp(szElementNodeName, "ExtCLutElement")) {
    return new(std::nothrow) CIccMpeXmlExtCLUT;
  }
  if (!strcmp(szElementNodeName, "CalculatorElement")) {
    return new(std::nothrow) CIccMpeXmlCalculator;
  }
  if (!strcmp(szElementNodeName, "TintArrayElement")) {
    return new(std::nothrow) CIccMpeXmlTintArray;
  }
  if (!strcmp(szElementNodeName, "ToneMapElement")) {
    return new(std::nothrow) CIccMpeXmlToneMap;
  }
  if (!strcmp(szElementNodeName, "JabToXYZElement")) {
    return new(std::nothrow) CIccMpeXmlJabToXYZ;
  }
  if (!strcmp(szElementNodeName, "UnknownElement")) {
    return new(std::nothrow) CIccMpeXmlUnknown;
  }
  if (!strcmp(szElementNodeName, "XYZToJabElement")) {
    return new(std::nothrow) CIccMpeXmlXYZToJab;
  }
  if (!strcmp(szElementNodeName, "EmissionMatrixElement")) {
    return new(std::nothrow) CIccMpeXmlEmissionMatrix;
  }
  if (!strcmp(szElementNodeName, "InvEmissionMatrixElement")) {
    return new(std::nothrow) CIccMpeXmlInvEmissionMatrix;
  }
  if (!strcmp(szElementNodeName, "EmissionCLutElement")) {
    return new(std::nothrow) CIccMpeXmlEmissionCLUT;
  }
  if (!strcmp(szElementNodeName, "ReflectanceCLutElement")) {
    return new(std::nothrow) CIccMpeXmlReflectanceCLUT;
  }
  if (!strcmp(szElementNodeName, "EmissionObserverElement")) {
    return new(std::nothrow) CIccMpeXmlEmissionObserver;
  }
  if (!strcmp(szElementNodeName, "ReflectanceObserverElement")) {
    return new(std::nothrow) CIccMpeXmlReflectanceObserver;
  }
  if (!strcmp(szElementNodeName, "BAcsElement")) {
    return new(std::nothrow) CIccMpeXmlBAcs;
  }
  if (!strcmp(szElementNodeName, "EAcsElement")) {
    return new(std::nothrow) CIccMpeXmlEAcs;
  }
  return NULL;
}


bool CIccTagXmlMultiProcessElement::ParseElement(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *attr;

  if (pNode->type != XML_ELEMENT_NODE) {
    return false;
  }

  CIccMultiProcessElement *pMpe = CreateElement((const icChar*)pNode->name);

  if (!pMpe) {
    parseStr += std::string("Unknown Element Type (") + (const icChar*)pNode->name + ")\n";
    return false;
  }

  CIccMultiProcessElementPtr ptr;

  IIccExtensionMpe *pExt = pMpe->GetExtension();

  if (pExt) {
    if (!strcmp(pExt->GetExtClassName(), "CIccMpeXml")) {
      CIccMpeXml* pXmlMpe = (CIccMpeXml*)pExt;

      if (pXmlMpe->ParseXml(pNode, parseStr)) {
        if ((attr=icXmlFindAttr(pNode, "Reserved"))) {
          sscanf(icXmlAttrValue(attr), "%u", &pMpe->m_nReserved);
        }

        ptr.ptr = pMpe;
        m_list->push_back(ptr);
      }
      else {
        parseStr += std::string("Unable to parse element of type ") + pMpe->GetClassName() + "\n";
        delete pMpe;
        return false;
      }
    }
    else {
      parseStr += std::string("Element ") + pMpe->GetClassName() + "isn't of type CIccMpeXml\n";
      delete pMpe;
      return false;
    }
  }
  else {
    parseStr += std::string("Element ") + pMpe->GetClassName() + "isn't of type CIccMpeXml\n";
    delete pMpe;
    return false;
  }

  return true;
}


bool CIccTagXmlMultiProcessElement::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  pNode = icXmlFindNode(pNode, "MultiProcessElements");

  if (!pNode) {
    parseStr += "Cannot Find MultiProcessElements\n";
    return false;
  }

  xmlAttr *pInputChannels = icXmlFindAttr(pNode, "InputChannels");
  xmlAttr *pOutputChannels = icXmlFindAttr(pNode, "OutputChannels");

  if (!pInputChannels || !pOutputChannels) {
    parseStr += "Invalid channels in MultiProcessElements\n";
    return false;
  }

  // icXmlAttrToUInt floors a negative to zero but returns icUInt32Number, and
  // the explicit narrowing cast that followed both changed the value and
  // suppressed the sanitizer check that would have reported it -- exactly the
  // caller shape described at the helper's definition near the top of this
  // file. InputChannels="360200" was therefore stored as 32520 and written
  // back out on save (#1901). icXmlParseU16 refuses the count instead.
  if (!icXmlParseU16(icXmlAttrValue(pInputChannels), m_nInputChannels) ||
      !icXmlParseU16(icXmlAttrValue(pOutputChannels), m_nOutputChannels)) {
    parseStr += "Invalid channels in MultiProcessElements\n";
    return false;
  }

  if (!m_list) {
    m_list = new (std::nothrow) CIccMultiProcessElementList();
    if (!m_list)
      return false;
  }
  else {
    m_list->clear();
  }

  xmlNode *elemNode;
  for (elemNode = pNode->children; elemNode; elemNode = elemNode->next) {
    if (elemNode->type == XML_ELEMENT_NODE) {
      if (!ParseElement(elemNode, parseStr)) {
        const size_t strSize = 100;
        char str[strSize];
        parseStr += "Unable to parse element (";
        parseStr += (char*)elemNode->name;
        snprintf(str, strSize, ") starting on line %d\n", elemNode->line);
        parseStr += str;
        return false;
      }
    }
  }

  return true;
}


bool CIccTagXmlProfileSequenceId::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  xml += blanks + "<ProfileSequenceId>\n";

  CIccProfileIdDescList::iterator pid;

  for (pid=m_list->begin(); pid!=m_list->end(); pid++) {
    const size_t bufSize = 256;
    char buf[bufSize];
    char data[bufSize];
    std::string bufstr;
    int n;

    for (n=0; n<16; n++) {
      snprintf(buf + n*2, bufSize-n*2, "%02X", pid->m_profileID.ID8[n]);
    }
    buf[n*2]='\0';
    xml += blanks + " <ProfileIdDesc id=\"";
    xml += buf;
    xml += "\">\n";

    if (pid->m_desc.m_Strings) {
      CIccMultiLocalizedUnicode::iterator i;

      for (i=pid->m_desc.m_Strings->begin(); i!=pid->m_desc.m_Strings->end(); i++) {
        icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength());
        // Cast to icUInt32Number before the shift: see CIccTagXmlMultiLocalizedUnicode::ToXml
        // -- m_nLanguageCode<<16 otherwise promotes to signed int and overflows for
        // language codes >= 0x8000, then wraps when handed to icGetSigStr.
        icXmlDumpLocalizedText(xml, blanks + " ", "LocalizedText",
                               icGetSigStr(data, bufSize, ((icUInt32Number)i->m_nLanguageCode << 16) |
                                           (icUInt32Number)i->m_nCountryCode),
                               bufstr);
      }
    }
    xml += blanks + " </ProfileIdDesc>\n";
  }

  xml += blanks + "</ProfileSequenceId>\n";
  return true;
}


bool CIccTagXmlProfileSequenceId::ParseXml(xmlNode *pNode, std::string & /* parseStr */)
{
  pNode = icXmlFindNode(pNode, "ProfileSequenceId");

  if (!pNode)
    return false;

  m_list->clear();

  for (pNode = icXmlFindNode(pNode->children, "ProfileIdDesc"); pNode; pNode = icXmlFindNode(pNode->next, "ProfileIdDesc")) {
    CIccProfileIdDesc desc;
    const icChar *szDesc = icXmlAttrValue(pNode, "id");

    if (szDesc && *szDesc)
      icXmlGetHexData(&desc.m_profileID, szDesc, sizeof(desc.m_profileID));

    xmlAttr *langCode;

    xmlNode* pSubNode;
    for (pSubNode = icXmlFindNode(pNode, "LocalizedText"); pSubNode; pSubNode = icXmlFindNode(pSubNode->next, "LocalizedText")) {
      if ((langCode = icXmlFindLanguageCountryAttr(pSubNode)) &&
        pSubNode->children) {
          std::string text;

          if (icXmlParseLocalizedText(pSubNode, text)) {
            icUInt32Number lc = icGetSigVal(icXmlAttrValue(langCode));
            if (!icXmlSetLocalizedUtf8(desc.m_desc, text, (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff)))
              return false;
          }
          else {
            desc.m_desc.SetText("");
          }
      }
    }
    m_list->push_back(desc);
  }

  return true;
}


bool CIccTagXmlDict::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;

  CIccNameValueDict::iterator nvp;

  for (nvp=m_Dict->begin(); nvp!=m_Dict->end(); nvp++) {
    CIccDictEntry *nv = nvp->ptr;
    if (!nv)
      continue;

    const size_t bufSize = 256;
    char data[bufSize];
    std::string fix;
    std::string bufstr;

    xml += blanks + " <DictEntry Name=\"";
    auto nameStr = nv->GetName();       // wstring
    xml += icFixXml(fix, icWCharToUtf8(bufstr, nameStr.c_str(), nameStr.size()));

    xml += "\"";

    if (nv->IsValueSet()) {
      xml += " Value=\"";
      auto valueStr = nv->GetValue();   // wstring
      xml += icFixXml(fix, icWCharToUtf8(bufstr, valueStr.c_str(), valueStr.size()));
      xml += "\"";
    }

    if (!nv->GetNameLocalized() && !nv->GetValueLocalized()) {
      xml += "/>\n";
    }
    else {
      xml += ">\n";

      if (nv->GetNameLocalized()) {
        CIccMultiLocalizedUnicode::iterator i;

        for (i=nv->GetNameLocalized()->m_Strings->begin(); i!=nv->GetNameLocalized()->m_Strings->end(); i++) {
          icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength());
          // Cast to icUInt32Number before the shift: see CIccTagXmlMultiLocalizedUnicode::ToXml
          // -- avoids signed-int overflow / value-changing wrap for language codes >= 0x8000.
          icXmlDumpLocalizedText(xml, blanks + "  ", "LocalizedName",
                                 icGetSigStr(data, bufSize, ((icUInt32Number)i->m_nLanguageCode << 16) |
                                             (icUInt32Number)i->m_nCountryCode),
                                 bufstr);
        }
      }
      if (nv->GetValueLocalized()) {
        CIccMultiLocalizedUnicode::iterator i;

        for (i=nv->GetValueLocalized()->m_Strings->begin(); i!=nv->GetValueLocalized()->m_Strings->end(); i++) {
          icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength());
          // Cast to icUInt32Number before the shift: see CIccTagXmlMultiLocalizedUnicode::ToXml
          // -- avoids signed-int overflow / value-changing wrap for language codes >= 0x8000.
          icXmlDumpLocalizedText(xml, blanks + "  ", "LocalizedValue",
                                 icGetSigStr(data, bufSize, ((icUInt32Number)i->m_nLanguageCode << 16) |
                                             (icUInt32Number)i->m_nCountryCode),
                                 bufstr);
        }
      }
      xml += blanks + " </DictEntry>\n";
    }
  }
  return true;
}


bool CIccTagXmlDict::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  m_Dict->clear();

  for (pNode = icXmlFindNode(pNode, "DictEntry"); pNode; pNode = icXmlFindNode(pNode->next, "DictEntry")) {
    CIccDictEntryPtr ptr;
    xmlAttr *pAttr;
    CIccUTF16String str;

    CIccDictEntry *pDesc = new (std::nothrow) CIccDictEntry();
    if (!pDesc)
      return false;
    
    ptr.ptr = pDesc;

    str = icXmlAttrValue(pNode, "Name", "");
    str.ToWString(pDesc->GetName());

    pAttr = icXmlFindAttr(pNode, "Value");
    if (pAttr) {
      std::wstring wstr;
      str = icXmlAttrValue(pAttr, "");
      str.ToWString(wstr);

      pDesc->SetValue(wstr);
    }

    xmlNode *pChild;

    for (pChild = pNode->children; pChild; pChild = pChild->next) {
      if (pChild->type == XML_ELEMENT_NODE && !icXmlStrCmp(pChild->name, "LocalizedName")) {
        CIccTagMultiLocalizedUnicode *pTag = pDesc->GetNameLocalized();
        if (!pTag) {
          pTag = new (std::nothrow) CIccTagMultiLocalizedUnicode();
          if (!pTag) {
            delete pDesc;
            ptr.ptr = NULL;
            return false;
          }
          pDesc->SetNameLocalized(pTag);
        }

        if ((pAttr = icXmlFindLanguageCountryAttr(pChild)) && pChild->children) {
          std::string text;
          icUInt32Number lc = icGetSigVal(icXmlAttrValue(pAttr));

          if (icXmlParseLocalizedText(pChild, text)) {
            if (!icXmlSetLocalizedUtf8(*pTag, text, (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff))) {
              delete pDesc;
              ptr.ptr = NULL;
              return false;
            }
          }
          else {
            pTag->SetText("");
          }
        }
      }
      else if (pChild->type == XML_ELEMENT_NODE && !icXmlStrCmp(pChild->name, "LocalizedValue")) {
        CIccTagMultiLocalizedUnicode *pTag = pDesc->GetValueLocalized();
        if (!pTag) {
          pTag = new (std::nothrow) CIccTagMultiLocalizedUnicode();
          if (!pTag) {
            delete pDesc;
            ptr.ptr = NULL;
            return false;
          }
          pDesc->SetValueLocalized(pTag);
        }

        if ((pAttr = icXmlFindLanguageCountryAttr(pChild)) && pChild->children) {
          std::string text;
          icUInt32Number lc = icGetSigVal(icXmlAttrValue(pAttr));

          if (icXmlParseLocalizedText(pChild, text)) {
            if (!icXmlSetLocalizedUtf8(*pTag, text, (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff))) {
              delete pDesc;
              ptr.ptr = NULL;
              return false;
            }
          }
          else {
            pTag->SetText("");
          }
        }
      }
    }

    m_Dict->push_back(ptr);
  }

  return true;
}


bool CIccTagXmlStruct::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;
  std::string fix;
  const size_t bufSize = 256;
  char buf[bufSize], line[bufSize];
  IIccStruct *pStruct = GetStructHandler();

  const icChar *structName = ((pStruct != NULL) ? pStruct->GetDisplayName() : NULL);
  blanks += "  ";

   if (structName && strcmp(structName, "privateStruct")) {
     snprintf(line, bufSize, "<%s> <MemberTags>\n", structName);
   }
   else {
     // print out the struct signature
     // No "/>" here: this element is closed by the matching "</privateStruct>"
     // emitted at the end of this function, so writing it self-closed produced a
     // start tag with no body and an end tag with no start, i.e. malformed XML
     // that no parser would accept back (#1779).
     snprintf(line, bufSize, "<privateStruct StructSignature=\"%s\"> <MemberTags>\n", icFixXml(fix, icGetSigStr(buf,bufSize, m_sigStructType)));
     structName = "privateStruct";
   }

  xml += blanks + line;
  TagEntryList::iterator i, j;
  std::set<icTagSignature> sigSet;
  CIccInfo Fmt;
  IccOffsetTagSigMap offsetTags;

  for (i=m_ElemEntries->begin(); i!=m_ElemEntries->end(); i++) {
    if (sigSet.find(i->TagInfo.sig)==sigSet.end()) {
      CIccTag *pTag = FindElem(i->TagInfo.sig);

      if (pTag) {
        CIccTagXml *pTagXml = (CIccTagXml*)(pTag->GetExtension());
        if (pTagXml) {
          IccOffsetTagSigMap::iterator prevTag = offsetTags.find(i->TagInfo.offset);
          std::string tagName = ((pStruct!=NULL) ? pStruct->GetElemName((icSignature)i->TagInfo.sig) : "");
          if (prevTag == offsetTags.end()) {
            const icChar* tagSig = icGetTagSigTypeName(pTag->GetType());

            // Start of this member's markup, kept so the skip path below can
            // rewind the opening elements that are appended before ToXml() runs.
            const size_t nTagStart = xml.size();

            if (tagName.size() && strncmp(tagName.c_str(), "PrivateSubTag", 13)) {
              snprintf(line, bufSize, "  <%s>", icFixXml(fix, tagName.c_str()));
            }
            else {
              snprintf(line, bufSize, "  <PrivateSubTag TagSignature=\"%s\">", icFixXml(fix, icGetSigStr(buf, bufSize, i->TagInfo.sig)));
              tagName = "PrivateSubTag";
            }
            xml += blanks + line;

            // PrivateType - a type that does not belong to the list in the icc specs - custom for vendor.
            if (!strcmp("PrivateType", tagSig))
              snprintf(line, bufSize, " <PrivateType type=\"%s\">\n", icFixXml(fix, icGetSigStr(buf, bufSize, pTag->GetType())));
            else
              snprintf(line, bufSize, " <%s>\n", tagSig); //parent node is the tag type

            xml += line;
            j = i;
#if 0
            // print out the tag signature (there is at least one)
            sprintf(line, "  <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, bufSize, i->TagInfo.sig)));
            xml += blanks + line;

            sigSet.insert(i->TagInfo.sig);

            // print out the rest of the tag signatures
            for (j++; j != m_ElemEntries->end(); j++) {
              if (j->pTag == i->pTag || j->TagInfo.offset == i->TagInfo.offset) {
                sprintf(line, "  <TagSignature>%s</TagSignature>\n", icFixXml(fix, icGetSigStr(buf, bufSize, j->TagInfo.sig)));
                xml += blanks + line;
                sigSet.insert(j->TagInfo.sig);
              }
            }
            // if (pTag->m_nReserved) {
            //   sprintf(line, " Reserved=\"%08x\"", pTag->m_nReserved);
            //   xml += line;
            // }
            // xml += ">\n";
#endif
            //convert the rest of the tag to xml
            if (!pTagXml->ToXml(xml, blanks + "    ")) {
              // Qualified for the same reason as the profile-level message: the
              // struct still serializes and the tool still exits 0, so the bare
              // "Unable to output ..." wording would now overstate the outcome.
              printf("Unable to output sub-tag with type %s - sub-tag skipped\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));

              // Drop just this member instead of failing the enclosing struct,
              // which previously propagated all the way up and cost the whole
              // document (#1779).  Struct members are addressed by name --
              // CIccTagXmlStruct::ParseXml walks <MemberTags> and resolves each
              // child element by its own name, with no positional count and no
              // "every member must be present" sweep afterwards -- so an omitted
              // member re-parses cleanly as simply absent.  This mirrors the JSON
              // struct writer, which likewise continues past a member whose
              // ToJson() fails (IccTagJson.cpp).
              std::string sigFix, typeFix;
              xml.resize(nTagStart);
              snprintf(line, bufSize, "  <!-- sub-tag %s (type %s): unable to serialize, skipped -->\n",
                       icFixXmlComment(sigFix, icGetSigStr(buf, bufSize, i->TagInfo.sig)),
                       icFixXmlComment(typeFix, tagSig ? tagSig : ""));
              xml += blanks + line;

              // As at profile level, not recorded in offsetTags so that a later
              // member at the same offset does not emit a SameAs reference to a
              // member that was dropped.
              continue;
            }
            snprintf(line, bufSize, "  </%s> </%s>\n", tagSig, tagName.c_str());
            xml += blanks + line;
            offsetTags[i->TagInfo.offset] = i->TagInfo.sig;
          }
          else {
            std::string prevTagName = ((pStruct != NULL) ? pStruct->GetElemName(prevTag->second) : "");
            char fix2[200];

            if (tagName.size() && strncmp(tagName.c_str(), "PrivateSubTag", 13))
              snprintf(line, bufSize, "    <%s SameAs=\"%s\"", icFixXml(fix, tagName.c_str()), icFixXml(fix2, prevTagName.c_str())); //parent node is the tag type
            else
              snprintf(line, bufSize, "    <PrivateSubTag TagSignature=\"%s\" SameAs=\"%s\"", icFixXml(fix2, icGetSigStr(buf, bufSize, i->TagInfo.sig)), icFixXml(fix, prevTagName.c_str()));

            xml += line;
            if (prevTagName.size() || !strncmp(prevTagName.c_str(), "PrivateSubTag", 13)) {
              snprintf(line, bufSize, " SameAsSignature=\"%s\"", icFixXml(fix2, icGetSigStr(buf, bufSize, prevTag->second)));
              xml += line;
            }

            xml += "/>\n";
          }
        }
        else {
          printf("Non XML tag in list with type %s!\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
          return false;
        }
      }
      else {
        printf("Unable to find tag with type %s!\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
        return false;
      }
    }
  }

  xml += blanks + "</MemberTags> </" + structName + ">\n";
  return true;
}

/**
******************************************************************************
* Name: CIccTagXmlStruct::ParseTag
* 
* Purpose: This will load from the indicated IO object and associate a tag
*  object to a tag directory entry.  Nothing happens if tag directory entry
*  is associated with a tag object.
* 
* Args: 
*  pNode - pointer to xmlNode object to parse from
* 
* Return: 
*  true - tag from node successfully parsed,
*  false - failure
*******************************************************************************
*/
bool CIccTagXmlStruct::ParseTag(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *attr;

  if (pNode->type != XML_ELEMENT_NODE) {// || icXmlStrCmp(pNode->name, "Tag")) {
    parseStr += "Invalid Tag Node: ";
    parseStr += (const char *)pNode->name;
    parseStr += "\n";
    return false;
  }

  CIccTag *pTag = NULL;

  std::string nodeName = (icChar*)pNode->name;
  icSignature sigTag;
  if (m_pStruct)
    sigTag = m_pStruct->GetElemSig(nodeName.c_str());
  else
    sigTag = 0;

  if (sigTag != 0 || nodeName == "PrivateSubTag") { //Parsing of XML tags by name
    if (nodeName == "PrivateSubTag") {
      const char *tagSig = icXmlAttrValue(pNode, "TagSignature", "");
      if (tagSig[0]) {
        sigTag = (icTagSignature)icGetSigVal(tagSig);
      }
      else {
        parseStr += "Invalid TagSignature for PrivateSubTag\n";
        return false;
      }
    }

    const char *sameAs = icXmlAttrValue(pNode, "SameAs", "");

    if (sameAs[0]) {
      icTagSignature sigParentTag = icGetTagNameSig(sameAs);
      if (!strcmp(sameAs, "PrivateSubTag") || sigParentTag == icSigUnknownTag) {
        const char *sameAsSig = icXmlAttrValue(pNode, "SameAsSignature", "");
        if (sameAsSig[0]) {
          sigParentTag = (icTagSignature)icGetSigVal(sameAsSig);
        }
        else {
          parseStr += "Invalid SameAsSignature for PrivateSubTag\n";
          return false;
        }
      }
      pTag = this->FindElem(sigParentTag);
      if (pTag) {
        AttachElem(sigTag, pTag);
      }
      else {
        parseStr += "SameAs tag ";
        parseStr += sameAs;
        parseStr += " for ";
        parseStr += nodeName + " does not exist\n";
        return false;
      }

      return true;
    }
    else { //Parse the type node as the first child
      xmlNode *pTypeNode;
      for (pTypeNode = pNode->children; pTypeNode; pTypeNode = pTypeNode->next) {
        if (pTypeNode->type == XML_ELEMENT_NODE) {
          break;
        }
      }

      if (!pTypeNode) {
        parseStr += "No tag type node defined for ";
        parseStr += nodeName;
        parseStr += "\n";
        return false;
      }

      // get the tag type signature
      icTagTypeSignature sigType = icGetTypeNameTagSig((const icChar*)pTypeNode->name);

      if (sigType == icSigUnknownType) {
        attr = icXmlFindAttr(pTypeNode, "type");
        const char *typeSig = icXmlAttrValue(attr);
        if (!typeSig[0]) {
          parseStr += "Invalid private tag type attribute for ";
          parseStr += nodeName;
          parseStr += "\n";
          return false;
        }
        sigType = (icTagTypeSignature)icGetSigVal(typeSig);
      }

      CIccInfo info;

      // create a tag based on the signature
      pTag = CIccTag::Create(sigType);

      IIccExtensionTag *pExt;

      if (pTag && (pExt = pTag->GetExtension()) && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
        CIccTagXml* pXmlTag = (CIccTagXml*)pExt;

        if (pXmlTag->ParseXml(pTypeNode->children, parseStr)) {
          if ((attr = icXmlFindAttr(pTypeNode, "reserved"))) {
            sscanf(icXmlAttrValue(attr), "%u", &pTag->m_nReserved);
          }
          if (!AttachElem(sigTag, pTag)) {
            parseStr += "Unable to Attach \"";
            parseStr += (const char*)pTypeNode->name;
            parseStr += "\" (";
            parseStr += nodeName;
            parseStr += ") Tag\n";
            delete pTag;
            return false;
          }
        }
        else {
          parseStr += "Unable to Parse \"";
          parseStr += (const char*)pTypeNode->name;
          parseStr += "\" (";
          parseStr += nodeName;
          parseStr += ") Tag\n";
          delete pTag;
          return false;
        }
      }
      else {
        parseStr += "Invalid tag extension for \"";
        parseStr += (const char*)pTypeNode->name;
        parseStr += "\" (";
        parseStr += nodeName;
        parseStr += ") Tag\n";
        delete pTag;
        return false;
      }
    }
  }
  else {  //Legacy parsing of XML tags by type
    // get the tag type signature
    icTagTypeSignature sigType = icGetTypeNameTagSig(nodeName.c_str());

    if (sigType == icSigUnknownType) {
      attr = icXmlFindAttr(pNode, "type");
      const char *typeSig = icXmlAttrValue(attr);
      if (!typeSig[0]) {
        parseStr += "Invalid private tag type attribute for ";
        parseStr += nodeName;
        parseStr += "\n";
        return false;
      }
      sigType = (icTagTypeSignature)icGetSigVal(typeSig);
    }

    CIccInfo info;

    // create a tag based on the signature
    pTag = CIccTag::Create(sigType);

    IIccExtensionTag *pExt;

    if (pTag && (pExt = pTag->GetExtension()) && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
      CIccTagXml* pXmlTag = (CIccTagXml*)pExt;

      if (pXmlTag->ParseXml(pNode->children, parseStr)) {
        if ((attr = icXmlFindAttr(pNode, "reserved"))) {
          sscanf(icXmlAttrValue(attr), "%u", &pTag->m_nReserved);
        }

        bool bAttached = false;
        for (xmlNode *tagSigNode = pNode->children; tagSigNode; tagSigNode = tagSigNode->next) {
          if (tagSigNode->type == XML_ELEMENT_NODE && !icXmlStrCmp(tagSigNode->name, "TagSignature")
            && tagSigNode->children != NULL) {
            sigTag = (icTagSignature)icGetSigVal((const icChar*)tagSigNode->children->content);
            if (AttachElem(sigTag, pTag))
              bAttached = true;
          }
        }
        if (!bAttached) {
          delete pTag;
        }
      }
      else {
        parseStr += "Unable to Parse \"";
        parseStr += info.GetTagTypeSigName(sigType);
        parseStr += "\" (";
        parseStr += nodeName;
        parseStr += ") Tag\n";
        delete pTag;
        return false;
      }
    }
    else {
      parseStr += "Invalid tag extension for \"";
      parseStr += info.GetTagTypeSigName(sigType);
      parseStr += "\" (";
      parseStr += nodeName;
      parseStr += ") Tag\n";
      delete pTag;
      return false;
    }
  }

  return true;
}


bool CIccTagXmlStruct::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse each tag
  xmlNode *tagNode, *firstNode=pNode;

  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE)
      break;
  }
  if (!pNode) {
    parseStr += "Invalid Tag Structure: ";
    if (firstNode)
      parseStr += (const char*)firstNode->name;
    else
      parseStr += ":";
    parseStr += "\n";
    return false;
  }

  std::string nodeName = (icChar*)pNode->name;

  icStructSignature sigStruct = CIccStructCreator::GetStructSig(nodeName.c_str());

  if (sigStruct) {
    SetTagStructType(sigStruct);
    pNode = pNode->children;
  }
  else {
    tagNode = icXmlFindNode(firstNode, "StructureSignature");
    if (!tagNode) {
      parseStr += "Unable to find StructureSignature\n";
      return false;
    }

    if (tagNode->type == XML_ELEMENT_NODE && tagNode->children && tagNode->children->content) {

      sigStruct = (icStructSignature)icGetSigVal(tagNode->children ? (const icChar*)tagNode->children->content : "");
      SetTagStructType(sigStruct);
    }
    else {
      parseStr += "Invalid XNode type for StructureSignature\n";
      return false;
    }
  }

  tagNode = icXmlFindNode(pNode, "MemberTags");
  if (!tagNode) {
    parseStr += "Unable to find structure MemberTags\n";
    return false;
  }

  for (tagNode = tagNode->children; tagNode; tagNode = tagNode->next) {
    if (tagNode->type == XML_ELEMENT_NODE) {
      if (!ParseTag(tagNode, parseStr)) {
        parseStr += "Failed to parse tag member (";
        parseStr += (char*)tagNode->name;
        parseStr += ")\n";
        return false;
      }
    }
  }

  return true;
}

bool CIccTagXmlArray::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;
  std::string fix;
  const size_t bufSize = 256;
  char buf[bufSize], line[bufSize];

  std::string arrayName;
  std::string arrayBlanks = "";
  bool found = CIccArrayCreator::GetArraySigName(arrayName, m_sigArrayType, false);

  if (found) {
    snprintf(line, bufSize, "<%s> ", arrayName.c_str());
    arrayBlanks = "  ";
  }
  else {
    // print out the struct signature
    // Same defect as the privateStruct case above: the closing tag written at the
    // end of this function is "</privateArray>", so the start tag must not be
    // self-closed.  Left unfixed, every profile carrying an unregistered array
    // signature serialized to XML that libxml2 rejects with "Opening and ending
    // tag mismatch" -- which only became observable once the writer stopped
    // discarding such documents wholesale (#1779).
    snprintf(line, bufSize, "<privateArray StructSignature=\"%s\"> ", icFixXml(fix, icGetSigStr(buf, bufSize, m_sigArrayType)));
    arrayName = "privateArray";
  }
  
  xml += blanks + line + "<ArrayTags>\n";
  int i;

  for (i=0; i<(int)m_nSize; i++) {
    CIccTag* pTag = m_TagVals[i].ptr;
    if (pTag) {
      CIccTagXml *pTagXml = (CIccTagXml*)(pTag->GetExtension());
      if (pTagXml) {
        const icChar* tagSig = icGetTagSigTypeName(pTag->GetType());

        // PrivateType - a type that does not belong to the list in the icc specs - custom for vendor.
        if ( !strcmp("PrivateType", tagSig) )
          snprintf(line, bufSize, " <PrivateType type=\"%s\">\n",  icFixXml(fix, icGetSigStr(buf, bufSize, pTag->GetType())));
        else
          snprintf(line, bufSize, " <%s>\n", tagSig); //parent node is the tag type

        xml += blanks + arrayBlanks + line; 				

        //convert the rest of the tag to xml
        // Unlike the profile-level and struct-member loops, this one deliberately
        // still fails the whole tag rather than skipping the bad element (#1779).
        // Array elements are positional, and the XML array format cannot express
        // a hole: CIccTagXmlArray::ParseXml sizes the array by counting the
        // XML_ELEMENT_NODE children of <ArrayTags> and then rejects the profile
        // outright if any slot is left unfilled ("Undefined Array Tag at index").
        // Emitting only a comment would therefore silently shrink the array and
        // renumber every later element, and a placeholder element would not
        // re-parse.  Failing here is contained rather than fatal: the caller at
        // profile level now skips just this one array tag and still writes the
        // document, which is the outcome #1779 asked for.  (The JSON writer can
        // keep the element because JSON *can* represent the hole -- it stores a
        // null "type" placeholder -- which XML has no equivalent for here.)
        if (!pTagXml->ToXml(xml, blanks + arrayBlanks + " ")) {
          printf("Unable to output tag with type %s\n", icGetSigStr(buf, bufSize, pTag->GetType()));
          return false;
        }
        snprintf(line, bufSize, " </%s>\n\n",  tagSig);
        xml += blanks + arrayBlanks + line; 	
      }
      else {
        printf("Non XML tag in list with type %s!\n", icGetSigStr(buf, bufSize, pTag->GetType()));
        return false;
      }
    }
  }
  xml += blanks + "</ArrayTags> </" + arrayName + ">\n";
  
  return true;
}


bool CIccTagXmlArray::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse each tag
  xmlNode *tagNode, *indexNode, *firstNode = pNode;;
  xmlAttr *attr;

  for (; pNode; pNode = pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE)
      break;
  }
  if (!pNode) {
    parseStr += "Invalid Tag Array: ";
    if (firstNode)
      parseStr += (const char*)firstNode->name;
    else
      parseStr += ":";
    parseStr += "\n";
    return false;
  }

  std::string nodeName = (icChar*)pNode->name;
  icArraySignature sigArray = CIccArrayCreator::GetArraySig(nodeName.c_str());

  if (sigArray) {
    SetTagArrayType(sigArray);
    pNode = pNode->children;
  }
  else {
    tagNode = icXmlFindNode(firstNode, "ArraySignature");
    if (!tagNode) {
      parseStr += "Unable to find ArraySignature\n";
      return false;
    }

    if (tagNode->type == XML_ELEMENT_NODE && tagNode->children && tagNode->children->content) {
      sigArray = (icArraySignature)icGetSigVal(tagNode->children ? (const icChar*)tagNode->children->content : "");
      SetTagArrayType(sigArray);
    }
    else {
      parseStr += "Invalid XNode type for ArraySignature\n";
      return false;
    }
  }

  indexNode = icXmlFindNode(pNode, "ArrayTags");
  if (!indexNode)
    return false;

  int nMaxIndex = 0, n=0;
  for (tagNode = indexNode->children; tagNode; tagNode = tagNode->next) {
    if (tagNode->type == XML_ELEMENT_NODE) {
      nMaxIndex++;
    }
  }
  if (!SetSize(nMaxIndex))
    return false;

  n=0; 
  for (tagNode = indexNode->children; tagNode; tagNode = tagNode->next) {
    if (tagNode->type == XML_ELEMENT_NODE) {
      CIccTag *pTag = NULL;

      // get the tag signature
      icTagTypeSignature sigType = icGetTypeNameTagSig ((icChar*) tagNode->name);

      if (sigType==icSigUnknownType){
        attr = icXmlFindAttr(pNode, "type");
        sigType = (icTagTypeSignature)icGetSigVal((icChar*) icXmlAttrValue(attr));
      }

      CIccInfo info;

      // create a tag based on the signature
      pTag = CIccTag::Create(sigType);

      IIccExtensionTag *pExt;

      if (pTag && (pExt = pTag->GetExtension()) && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
        CIccTagXml* pXmlTag = (CIccTagXml*)pExt;

        if (pXmlTag->ParseXml(tagNode->children, parseStr)) {
          if ((attr=icXmlFindAttr(pNode, "reserved"))) {
            sscanf(icXmlAttrValue(attr), "%u", &pTag->m_nReserved);
          }

          if (!m_TagVals[n].ptr)
            m_TagVals[n].ptr = pTag;
          else {
            parseStr += "Tag Array Index ";
            parseStr += n;
            parseStr += " already filled!\n";
            delete pTag;
            return false;
          }
        }
        else {
          parseStr += "Unable to Parse xml node named  \"";
          parseStr += (icChar*)tagNode->name;
          parseStr += "\"\n";
          delete pTag;
          return false;
        }
      }
      else {
        delete pTag;
      }
      n++;
    }
  }

  for (n=0; n<(int)m_nSize; n++) {
    if (!m_TagVals[n].ptr) {
      parseStr += "Undefined Array Tag at index ";
      parseStr += n;
      parseStr += "\n";
      return false;
    }
  }

  return true;
}

bool CIccTagXmlGamutBoundaryDesc::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  std::string info;
  const size_t lineSize = 256;
  char line[lineSize];
  int i;

  if (m_NumberOfVertices && (m_PCSValues || m_DeviceValues)) {
    xml += blanks + "<Vertices>\n";

    if (m_PCSValues) {
      snprintf(line, lineSize, " <PCSValues channels=\"%d\">\n", m_nPCSChannels);
      xml += blanks + line;
      CIccFloatArray::DumpArray(xml, blanks+"  ", m_PCSValues, m_NumberOfVertices*m_nPCSChannels, icConvertFloat, 9);
      xml += blanks + " </PCSValues>\n";
    }
    if (m_DeviceValues) {
      snprintf(line, lineSize, " <DeviceValues channels=\"%d\">\n", m_nDeviceChannels);
      xml += blanks + line;
        CIccFloatArray::DumpArray(xml, blanks+"  ", m_DeviceValues, m_NumberOfVertices*m_nDeviceChannels, icConvertFloat, 8);
      xml += blanks + " </DeviceValues>\n";
    }
    xml += blanks + "</Vertices>\n";
  }

  if (m_Triangles && m_NumberOfTriangles) {
    xml += blanks + "<Triangles>\n";

    for (i=0; i<m_NumberOfTriangles; i++) {
      snprintf(line, lineSize, " <T>%u %u %u</T>\n",
              (unsigned int) m_Triangles[i].m_VertexNumbers[0],
              (unsigned int) m_Triangles[i].m_VertexNumbers[1],
              (unsigned int) m_Triangles[i].m_VertexNumbers[2]);
      xml += blanks + line;
    }

    xml += blanks + "</Triangles>\n";
  }

  return true;
}


bool CIccTagXmlGamutBoundaryDesc::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse each tag
  xmlNode *childNode, *subNode;

  childNode = icXmlFindNode(pNode, "Vertices");
  if (!childNode) {
    parseStr += "Cannot find Vertices\n";
    return false;
  }

  subNode = icXmlFindNode(childNode->children, "PCSValues");

  if (subNode) {
    // The count divides the parsed value list into vertices below, so wrapping
    // it produces a different vertex geometry rather than an error:
    // channels="65539" was stored -- and written back out on save -- as 3, and
    // the value list was then read as three-channel vertices (#1909). The zero
    // check that follows already rejected the exact multiples of 65536; every
    // other out-of-range count reached it as a plausible small number.
    if (!icXmlParseU16(icXmlAttrValue(subNode, "channels", "0"), m_nPCSChannels) ||
        !m_nPCSChannels) {
      parseStr += "Bad PCSValues channels\n";
      return false;
    }

    CIccFloatArray vals;
    if (!vals.ParseArray(subNode->children)) {
      parseStr += "Unable to parse GamutBoundaryDesc PCSValues\n";
      return false;
    }

    m_NumberOfVertices = vals.GetSize() / m_nPCSChannels;

    if (m_NumberOfVertices<4) {
      parseStr += "Must have at least 4 PCSValues vertices\n";
      return false;
    }
    
    size_t totalSize = (size_t)m_NumberOfVertices * (size_t)m_nPCSChannels;
    m_PCSValues = new (std::nothrow) icFloatNumber[totalSize];

    if (!m_PCSValues)
      return false;

    memcpy(m_PCSValues, vals.GetBuf(), totalSize*sizeof(icFloatNumber));
  }
  else {
    parseStr += "Cannot find PCSValues\n";
    return false;
  }

  subNode = icXmlFindNode(childNode->children, "DeviceValues");

  if (subNode) {
    // Same narrowing as PCSValues above, on the device side of the same tag.
    if (!icXmlParseU16(icXmlAttrValue(subNode, "channels", "0"), m_nDeviceChannels) ||
        !m_nDeviceChannels) {
      parseStr += "Bad DeviceValues channels\n";
      return false;
    }

    CIccFloatArray vals;
    if (!vals.ParseArray(subNode->children)) {
      parseStr += "Unable to parse GamutBoundaryDesc DeviceValues\n";
      return false;
    }

    int nVertices = vals.GetSize() / m_nDeviceChannels;

    if (m_NumberOfVertices != nVertices) {
      parseStr += "Number of Device vertices doesn't match PCS verticies\n";
      return false;
    }
    
    size_t totalSize = (size_t)m_NumberOfVertices * (size_t)m_nDeviceChannels;
    m_DeviceValues = new (std::nothrow) icFloatNumber[totalSize];

    if (!m_DeviceValues)
      return false;

    memcpy(m_DeviceValues, vals.GetBuf(), totalSize * sizeof(icFloatNumber));
  }
  else if (!m_PCSValues)
    m_NumberOfVertices = 0;

  childNode = icXmlFindNode(pNode, "Triangles");
  if (!childNode) {
    parseStr += "Cannot find Triangles\n";
    return false;
  }

  int nMaxIndex = 0, n=0;
  for (subNode = childNode->children; subNode; subNode = subNode->next) {
    if (subNode->type == XML_ELEMENT_NODE && !strcmp((icChar*)subNode->name, "T")) {
      nMaxIndex++;
    }
  }
  m_NumberOfTriangles = nMaxIndex;
  m_Triangles = new (std::nothrow) icGamutBoundaryTriangle[m_NumberOfTriangles];
  if (!m_Triangles)
    return false;

  n=0; 
  for (subNode = childNode->children; subNode && n<nMaxIndex; subNode = subNode->next) {
    if (subNode->type == XML_ELEMENT_NODE && !strcmp((icChar*)subNode->name, "T")) {
      CIccUInt32Array ids;
      
      if (!ids.ParseArray(subNode->children) || ids.GetSize()!=3) {
        parseStr += "Invalid Triangle entry\n";
        return false;
      }
      icUInt32Number *v = ids.GetBuf();

      m_Triangles[n].m_VertexNumbers[0] = v[0];
      m_Triangles[n].m_VertexNumbers[1] = v[1];
      m_Triangles[n].m_VertexNumbers[2] = v[2];

      n++;
    }
  }

  return true;
}

bool CIccTagXmlEmbeddedProfile::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse each tag
  xmlNode *tagNode;

  tagNode = icXmlFindNode(pNode, "IccProfile");
  if (!tagNode)
    return false;

  delete m_pProfile;

  CIccProfileXml *pProfile = new (std::nothrow) CIccProfileXml();
  if (!pProfile)
    return false;
  m_pProfile = pProfile;

  if (!pProfile->ParseXml(tagNode, parseStr)) {
    delete m_pProfile;
    m_pProfile = NULL;
    return false;
  }
  return true;
}

bool CIccTagXmlEmbeddedProfile::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  if (!m_pProfile || strcmp(m_pProfile->GetClassName(), "CIccProfileXml")) {
    return false;
  }

  CIccProfileXml *pProfile = (CIccProfileXml*)m_pProfile;

  return pProfile->ToXmlWithBlanks(xml, blanks);
}

bool CIccTagXmlEmbeddedHeightImage::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse tag
  xmlNode *tagNode;

  tagNode = icXmlFindNode(pNode, "HeightImage");
  if (!tagNode)
    return false;

  // SeamlessIndicator is stored as an unsigned 32-bit indicator (the binary
  // Read() path uses Read32).  #1342 stopped the negative wrap by flooring a
  // negative atoi() result to 0, which removed the UBSan finding but left the
  // upper half of the range unreachable: atoi() is undefined above INT_MAX, so
  // SeamlessIndicator="2147483650" -- a perfectly ordinary 32-bit value -- came
  // out of the parse negative and was then floored, i.e. written as 0.  Parsing
  // with icXmlParseU32 makes the whole 32-bit range read back as itself and
  // refuses what genuinely does not fit instead of laundering it into 0.
  if (!icXmlParseU32(icXmlAttrValue(tagNode, "SeamlessIndicator", "0"), m_nSeamlesIndicator)) {
    parseStr += "Invalid SeamlessIndicator in HeightImage\n";
    return false;
  }

  // icImageEncodingType has a fixed underlying type of icUInt32Number and its
  // own header defines icImageTypeMaximum = 0xffffffff, so the full unsigned
  // range is representable.  The cast from atoi()'s signed int therefore did not
  // narrow so much as re-label: EncodingFormat="-12623518" was accepted as
  // 4282343778, and the writer below -- which printed the value with "%d" --
  // turned it straight back into "-12623518", so the malformed attribute
  // survived a full round-trip unchanged.  The two defects cancelled, which is
  // why neither showed up on its own; both are fixed here, exactly as #1962 had
  // to fix the MPE Flags reader and writer together.
  icUInt32Number nEncodingFormat = 0;
  if (!icXmlParseU32(icXmlAttrValue(tagNode, "EncodingFormat", "0"), nEncodingFormat)) {
    parseStr += "Invalid EncodingFormat in HeightImage\n";
    return false;
  }
  m_nEncodingFormat = (icImageEncodingType)nEncodingFormat;

  m_fMetersMinPixelValue = (icFloatNumber)atof(icXmlAttrValue(tagNode, "MetersMinPixelValue", "0.0"));
  m_fMetersMaxPixelValue = (icFloatNumber)atof(icXmlAttrValue(tagNode, "MetersMaxPixelValue", "0.0"));

  xmlNode *pImageNode;
  pImageNode = icXmlFindNode(tagNode->children, "Image");

  if (pImageNode) {
    const char *filename = icXmlAttrValue(pImageNode, "File");
    if (!filename || !filename[0]) {
      filename = icXmlAttrValue(pImageNode, "Filename");
    }

    // file exists
    if (filename && filename[0]) {
      CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");
      if (!file) {
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
        delete file;
        return false;
      }

      size_t num = file->GetLength();
      icUInt32Number count;

      if (!icXmlValidateFileCount(num, count, parseStr, filename)) {
        delete file;
        return false;
      }

      SetSize(count);
      icUInt8Number *dst = GetData(0);
      if (file->Read8(dst, count)!=count) {
        perror("Read-File Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid binary file.\n";
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    // no file
    else if (pImageNode->children && pImageNode->children->content){
      icUInt32Number nSize = icXmlGetHexDataSize((const icChar*)pImageNode->children->content);

      SetSize(nSize);
      if (m_pData) {
        if (icXmlGetHexData(m_pData, (const icChar*)pImageNode->children->content, m_nSize) != m_nSize)
          return false;
      }

      return true;
    }
  }
  return false;
}

bool CIccTagXmlEmbeddedHeightImage::ToXml(std::string &xml, std::string blanks/*= ""*/)
{
  const size_t bufSize = 200;
  char buf[bufSize];

  xml += blanks + "<HeightImage";
  // Both fields are unsigned 32-bit -- m_nSeamlesIndicator is an icUInt32Number
  // and icImageEncodingType has a fixed underlying type of icUInt32Number, with
  // icImageTypeMaximum = 0xffffffff defined in icProfileHeader.h.  The
  // (unsigned int) casts already said as much, but "%d" reinterprets the
  // argument as signed, so anything with bit 31 set was written out negative:
  // an encoding format of icImageTypeMaximum was emitted as
  // EncodingFormat="-1".  "%u" is what makes these attributes correct on their
  // own rather than only in combination with a reader that wraps them back
  // (#1931); the ParseXml above no longer does that.
  snprintf(buf, bufSize, " SeamlessIndicator=\"%u\"", (unsigned int) m_nSeamlesIndicator);
  xml += buf;

  snprintf(buf, bufSize, " EncodingFormat=\"%u\"", (unsigned int) m_nEncodingFormat);
  xml += buf;

  snprintf(buf, bufSize, " MetersMinPixelValue=\"%.12f\"", m_fMetersMinPixelValue);
  xml += buf;

  snprintf(buf, bufSize, " MetersMaxPixelValue=\"%.12f\"", m_fMetersMaxPixelValue);
  xml += buf;

  if (!m_nSize) {
    xml += blanks + "/>\n";
  }
  else {
    xml += ">\n";
    xml += blanks + " <Image>\n";
    icXmlDumpHexData(xml, blanks + "  ", m_pData, m_nSize);
    xml += blanks + " </Image>\n";
    xml += blanks + "</HeightImage>\n";
  }

  return true;
}

bool CIccTagXmlEmbeddedNormalImage::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  // parse tag
  xmlNode *tagNode;

  tagNode = icXmlFindNode(pNode, "NormalImage");
  if (!tagNode)
    return false;

  // The normal-image tag carries the same two attributes as the height-image
  // tag above and had the same pair of defects; #1343 is the twin of #1342.  See
  // CIccTagXmlEmbeddedHeightImage::ParseXml for the reasoning -- the flooring
  // that closed the negative wrap also made every value above INT_MAX read back
  // as 0, and the EncodingFormat reader cancelled against its own writer.
  if (!icXmlParseU32(icXmlAttrValue(tagNode, "SeamlessIndicator", "0"), m_nSeamlesIndicator)) {
    parseStr += "Invalid SeamlessIndicator in NormalImage\n";
    return false;
  }

  icUInt32Number nEncodingFormat = 0;
  if (!icXmlParseU32(icXmlAttrValue(tagNode, "EncodingFormat", "0"), nEncodingFormat)) {
    parseStr += "Invalid EncodingFormat in NormalImage\n";
    return false;
  }
  m_nEncodingFormat = (icImageEncodingType)nEncodingFormat;

  xmlNode *pImageNode;
  pImageNode = icXmlFindNode(tagNode->children, "Image");

  if (pImageNode) {
    const char *filename = icXmlAttrValue(pImageNode, "File");
    if (!filename || !filename[0]) {
      filename = icXmlAttrValue(pImageNode, "Filename");
    }

    // file exists
    if (filename && filename[0]) {
      CIccIO *file = IccXmlSafeOpenFileIO(filename, "rb");
      if (!file) {
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' could not be opened (file includes may be disabled or path rejected as unsafe).\n";
        delete file;
        return false;
      }

      size_t num = file->GetLength();
      icUInt32Number count;

      if (!icXmlValidateFileCount(num, count, parseStr, filename)) {
        delete file;
        return false;
      }

      SetSize(count);
      icUInt8Number *dst = GetData(0);
      if (file->Read8(dst, count) != count) {
        perror("Read-File Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid binary file'.\n";
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    // no file
    else if (pImageNode->children && pImageNode->children->content) {
      icUInt32Number nSize = icXmlGetHexDataSize((const icChar*)pImageNode->children->content);

      SetSize(nSize);
      if (m_pData) {
        if (icXmlGetHexData(m_pData, (const icChar*)pImageNode->children->content, m_nSize) != m_nSize)
          return false;
      }
      return true;
    }
  }
  return false;
}

bool CIccTagXmlEmbeddedNormalImage::ToXml(std::string &xml, std::string blanks/*= ""*/)
{
  const size_t bufSize = 200;
  char buf[bufSize];

  xml += blanks + "<NormalImage";
  // Same two unsigned 32-bit fields, same "%d"-on-an-unsigned-value defect as
  // CIccTagXmlEmbeddedHeightImage::ToXml above.
  snprintf(buf, bufSize, " SeamlessIndicator=\"%u\"", (unsigned int) m_nSeamlesIndicator);
  xml += buf;

  snprintf(buf, bufSize, " EncodingFormat=\"%u\"", (unsigned int) m_nEncodingFormat);
  xml += buf;

  if (!m_nSize) {
    xml += blanks + "/>\n";
  }
  else {
    xml += ">\n";
    xml += blanks + " <Image>\n";
    icXmlDumpHexData(xml, blanks + "  ", m_pData, m_nSize);
    xml += blanks + " </Image>\n";
    xml += blanks + "</NormalImage>\n";
  }

  return true;
}


#ifdef USEICCDEVNAMESPACE
}
#endif
