/** @file
    File:       IccTagXML.cpp

    Contains:   Implementation ICC tag XML format conversions

    Version:    V1

    Copyright:  © see ICC Software License
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
#include <cstring> /* C strings strcpy, memcpy ... */
#include <set>
#include <map>
#include <sstream>  // Make sure to include this header
#include <iomanip>  // Include this header for setw and setfill

typedef  std::map<icUInt32Number, icTagSignature> IccOffsetTagSigMap;

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


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

    if (m_pData) {
      delete [] m_pData;
      m_pData = NULL;
    }
    if (m_nSize) {
      m_pData = new icUInt8Number[m_nSize];

      if (icXmlGetHexData(m_pData, (const icChar*)pNode->children->content, m_nSize)!=m_nSize)
        return false;
    }
    return true;
  }
  return false;
}


static bool icXmlDumpTextData(std::string &xml, std::string blanks, const char *szText, bool bConvert=true)
{
  if (strstr(szText, "]]>")) {
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

static std::string icXmlParseTextString(xmlNode *pNode, std::string &parseStr, bool bConvert = true)
{
  std::string str;

  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexTextData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        if (!buf.SetSize(icXmlGetHexDataSize((const icChar*)pNode->children->content) ||
          icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, buf.GetSize())!=buf.GetSize()))
          return str;

        str += (char*)buf.GetBuf();
      }      
      else if (!icXmlStrCmp(pNode->name, "TextData") ) {
        std::string buf;
        const icChar *filename = icXmlAttrValue(pNode, "File");

        // file exists
        if (filename[0]) {        
          CIccIO *file = IccOpenFileIO(filename, "rb");        
          if (!file){          
            parseStr += "Error! - File '";
            parseStr += filename;
            parseStr +="' not found.\n";
            delete file;
            return str;
          }

          size_t fileLength = file->GetLength();
          char *ansiStr = (char *)malloc(fileLength+1);

          if (!ansiStr) {
            perror("Memory Error");
            parseStr += "'";
            parseStr += filename;
            parseStr += "' may not be a valid text file.\n";
            delete file;
            return str;
          }
          // read the contents of the file
          if (file->ReadLine(ansiStr, fileLength)!=fileLength) {
            parseStr += "Error while reading file '";
            parseStr += filename;
            parseStr += "'. Size read is not equal to file length. File may not be a valid text file.\n";
            free(ansiStr);
            delete file;             
            return str;
          }   
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

  return str;
}

bool CIccTagXmlText::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string str = icXmlParseTextString(pNode, parseStr);

  if (!str.empty()){    
    SetText(str.c_str());
    return true;
  }
  return false;
}

bool CIccTagXmlUtf8Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string str = icXmlParseTextString(pNode, parseStr, false);

  if (!str.empty()){    
    SetText(str.c_str());
    return true;
  }
  return false;
}

bool CIccTagXmlZipUtf8Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexCompressedData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        if (!buf.SetSize(icXmlGetHexDataSize((const icChar*)pNode->children->content) ||
            icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, buf.GetSize())!=buf.GetSize()))
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

  std::string str = icXmlParseTextString(pNode, parseStr, false);

  return SetText(str.c_str());
}

bool CIccTagXmlZipXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  while (pNode) {
    if (pNode->type==XML_ELEMENT_NODE) {
      if (!icXmlStrCmp(pNode->name, "HexCompressedData") && pNode->children && pNode->children->content) {
        CIccUInt8Array buf;
        if (!buf.SetSize(icXmlGetHexDataSize((const icChar*)pNode->children->content) ||
          icXmlGetHexData(buf.GetBuf(), (const icChar*)pNode->children->content, buf.GetSize())!=buf.GetSize()))
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

  std::string str = icXmlParseTextString(pNode, parseStr, false);

  return SetText(str.c_str());
}

bool CIccTagXmlUtf16Text::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  std::string str = icXmlParseTextString(pNode, parseStr, false);

  if (!str.empty()){    
    SetText(str.c_str());
    return true;
  }
  return false;
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
  if (filename[0]) {
    CIccIO *file = IccOpenFileIO(filename, "rb");

    if (!file){
      parseStr += "Error! - File '";
      parseStr += filename;
      parseStr +="' not found.\n";
      delete file;
      return false;
    }

    size_t fileLength = file->GetLength();
    char *buf = (char *)malloc(fileLength);

    if (!buf) {
      perror("Memory Error");
      parseStr += "'";
      parseStr += filename;
      parseStr += "' may not be a valid text file.\n";
      delete file;
      return false;
    }

    if (file->ReadLine(buf, fileLength)!=fileLength) {
      parseStr += "Error while reading file '";
      parseStr += filename;
      parseStr += "'. Size read is not equal to file length. File may not be a valid text file.\n";
      free(buf);
      delete file;             
      return false;
    }   

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
    CIccUTF16String wstr(buf);

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

    // Set ScriptCode
    m_nScriptCode=0;
    m_nScriptSize = (icUInt8Number) fileLength + 1;  
    memcpy(m_szScriptText, buf, 67);

    delete file;
  }

  // file does not exist
  else {
    std::string str = icXmlParseTextString(pNode, parseStr);
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
            CIccUTF16String wstr((const char*)pNode->children->content);

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
  char fix[40];
  char buf[40];
  const size_t lineSize = 256;
  char line[lineSize];

  snprintf(line, lineSize, "<Signature>%s</Signature>\n", icFixXml(fix, icGetSigStr(buf, 40, m_nSig)));

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
  char fix[40];
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

  m_spectralRange.start = icFtoF16((icUInt16Number)atof(icXmlAttrValue(pChild, "start")));
  m_spectralRange.end = icFtoF16((icUInt16Number)atof(icXmlAttrValue(pChild, "end")));
  m_spectralRange.steps = (icUInt16Number)atoi(icXmlAttrValue(pChild, "steps"));

  pChild = icXmlFindNode(pNode, "BiSpectralRange");

  if (pChild) {
    if ((pChild = icXmlFindNode(pChild->children, "Wavelengths"))) {
      m_biSpectralRange.start = icFtoF16((icUInt16Number)atof(icXmlAttrValue(pChild, "start")));
      m_biSpectralRange.end = icFtoF16((icUInt16Number)atof(icXmlAttrValue(pChild, "end")));
      m_biSpectralRange.steps = (icUInt16Number)atoi(icXmlAttrValue(pChild, "steps"));
    }
  }

  return true;
}


bool CIccTagXmlNamedColor2::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char fix[bufSize];
  char line[bufSize];
  char buf[bufSize];
  int i, j;
  std::string str;

  snprintf(line, bufSize, "<NamedColors VendorFlag=\"%08x\" CountOfDeviceCoords=\"%d\" DeviceEncoding=\"int16\"", m_nVendorFlags, m_nDeviceCoords);
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
          snprintf(buf, bufSize, "%d", (int)(pEntry->deviceCoords[j] * 65535.0 + 0.5));
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

        strncpy(m_szPrefix, icUtf8ToAnsi(str, szPrefix), sizeof(m_szPrefix));
        m_szPrefix[sizeof(m_szPrefix)-1] = '\0';

        strncpy(m_szSufix, icUtf8ToAnsi(str, szSufix), sizeof(m_szSufix));
        m_szSufix[sizeof(m_szSufix)-1] = '\0';

        icUInt32Number newDeviceCoords = atoi(szDeviceCoords);
        icUInt32Number n = icXmlNodeCount3(pNode->children, "NamedColor", "LabNamedColor", "XYZNamedColor");
        SetSize(n, newDeviceCoords);

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
            strncpy(pNamedColor->rootName, icUtf8ToAnsi(str, szName), sizeof(pNamedColor->rootName));
            pNamedColor->rootName[sizeof(pNamedColor->rootName) - 1] = 0;

            if (m_nDeviceCoords && pNode->children) {
              if (!strcmp(szDeviceEncoding, "int8")) {
                CIccUInt8Array coords;

                coords.ParseArray(pNode->children);
                icUInt8Number *pBuf = coords.GetBuf();

                icUInt32Number j;
                for (j = 0; j < m_nDeviceCoords && j < coords.GetSize(); j++) {
                  pNamedColor->deviceCoords[j] = (icFloatNumber)pBuf[j] / 255.0f;
                }
              }
              else if (!strcmp(szDeviceEncoding, "int16")) {
                CIccUInt16Array coords;

                coords.ParseArray(pNode->children);
                icUInt16Number *pBuf = coords.GetBuf();

                icUInt32Number j;
                for (j = 0; j < m_nDeviceCoords && j < coords.GetSize(); j++) {
                  pNamedColor->deviceCoords[j] = (icFloatNumber)pBuf[j] / 65535.0f;
                }
              }
              else if (!strcmp(szDeviceEncoding, "float")) {
                CIccFloatArray coords;

                coords.ParseArray(pNode->children);
                icFloatNumber *pBuf = coords.GetBuf();

                icUInt32Number j;
                for (j = 0; j < m_nDeviceCoords && j < coords.GetSize(); j++) {
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
    SetSize(n);

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


bool CIccTagXmlCicp::ParseXml(xmlNode* pNode, std::string& /*parseStr*/)
{

  pNode = icXmlFindNode(pNode, "cicpFields");

  if (pNode) {
    xmlAttr* attr;
    if ((attr = icXmlFindAttr(pNode, "ColorPrimaries")))
      m_nColorPrimaries = atoi(icXmlAttrValue(attr));
    else
      m_nColorPrimaries = 0;

    if ((attr = icXmlFindAttr(pNode, "TransferCharacteristics")))
      m_nTransferCharacteristics = atoi(icXmlAttrValue(attr));
    else
      m_nTransferCharacteristics = 0;

    if ((attr = icXmlFindAttr(pNode, "MatrixCoefficients")))
      m_nMatrixCoefficients = atoi(icXmlAttrValue(attr));
    else
      m_nMatrixCoefficients = 0;

    if ((attr = icXmlFindAttr(pNode, "VideoFullRangeFlag")))
      m_nVideoFullRangeFlag = atoi(icXmlAttrValue(attr));
    else
      m_nVideoFullRangeFlag = 0;
  }
  else
    return false;

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

  for (i=0; i<(int)m_nSize; i++) {
    mtx.Reset(m_RawData+i*bytesPerMatrix, bytesPerMatrix, icSparseMatrixFloatNum, true);
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
      icUInt32Number nChannelsPerMatrix = atoi(icXmlAttrValue(outputChan));
      icSparseMatrixType nMatrixType = (icSparseMatrixType)atoi(icXmlAttrValue(matrixType));

      xmlNode *pChild;

      int n=0;
      for (pChild = pNode->children; pChild; pChild=pChild->next) {
        if (pChild->type == XML_ELEMENT_NODE &&
            (!icXmlStrCmp(pChild->name, "SparseMatrix") || !icXmlStrCmp(pChild->name, "FullMatrix")))
          n++;

      }
      Reset(n, (icUInt16Number)nChannelsPerMatrix);
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
              icUInt16Number nRows, nCols;

              nRows = atoi(icXmlAttrValue(rows));
              nCols = atoi(icXmlAttrValue(cols));

              mtx.Init(nRows, nCols, true);

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
              icUInt16Number nRows, nCols;

              nRows = atoi(icXmlAttrValue(rows));
              nCols = atoi(icXmlAttrValue(cols));

              mtx.Init(nRows, nCols, true);

              CIccFloatArray data;
              data.ParseTextArray(pChild);
              if (data.GetSize()==nRows*nCols) {
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
      snprintf(buf, bufSize, "%u", (icUInt32Number)this->m_Num[i]);
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

  if (filename[0]) {
    CIccIO *file = IccOpenFileIO(filename, "rb");
    if (!file){
      parseStr += "Error! - File '";
      parseStr += filename;
      parseStr +="' not found.\n";
      delete file;
      return false;
    }

    size_t len = file->GetLength();

    if (!stricmp(icXmlAttrValue(pNode, "Format", "text"), "text")) {
      char *fbuf = (char*)malloc(len+1);
      fbuf[len]=0;
      if (!fbuf) {
        parseStr += "Memory error!\n";
        delete file;
        return false;
      }

      if (file->Read8(fbuf, len)!=len) {
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
      icUInt32Number n = len/sizeof(icFloat16Number);
      this->SetSize(n);
      if (file->Read16(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat16ArrayType && sizeof(T)==sizeof(icFloat32Number)) {
      icUInt32Number n = (icUInt32Number)(len/sizeof(icFloat32Number));
      this->SetSize(n);
      if (file->ReadFloat16Float(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat32ArrayType && sizeof(T)==sizeof(icFloat32Number)) {
      icUInt32Number n =  (icUInt32Number)(len/sizeof(icFloat32Number));
      this->SetSize(n);
      if (file->ReadFloat32Float(&this->m_Num[0], n)!=n) {
        delete file;
        return false;
      }
      delete file;
      return true;
    }
    else if (Tsig==icSigFloat64ArrayType && sizeof(T)==sizeof(icFloat64Number)) {
      icUInt32Number n =  (icUInt32Number)(len/sizeof(icFloat64Number));
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
  std::string xmlbuf;
  char data[256];
  std::string bufstr;
  CIccMultiLocalizedUnicode::iterator i;

  if (!m_Strings)
    return false;

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    xml += blanks + "<LocalizedText LanguageCountry=\"";
    xml += icFixXml(xmlbuf, icGetSigStr(data, 256, (i->m_nLanguageCode<<16) + i->m_nCountryCode));
    xml += "\"><![CDATA[";
    xml += icFixXml(xmlbuf, icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength()));
    xml += "]]></LocalizedText>\n";
  }
  return true;
}


bool CIccTagXmlMultiLocalizedUnicode::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  xmlAttr *langCode;
  int n = 0;

  for (pNode = icXmlFindNode(pNode, "LocalizedText"); pNode; pNode = icXmlFindNode(pNode->next, "LocalizedText")) {    
    if ((langCode = icXmlFindAttr(pNode, "LanguageCountry"))) {
      xmlNode *pText;

      for (pText = pNode->children; pText; pText = pText->next) {
        if (pText->type == XML_TEXT_NODE || pText->type == XML_CDATA_SECTION_NODE)
          break;
      }

      if (pText) {
        icUInt32Number lc = icGetSigVal(icXmlAttrValue(langCode));
        CIccUTF16String str((const char*)pText->content);

        SetText(str.c_str(), (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff));
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
  int i;

  xml += blanks + "<ColorantOrder>\n"; //+ blanks + "  ";
  for (i=0; i<(int)m_nCount; i++) {
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
  char fix[bufSize];
  int i;
  std::string str;

  xml += blanks + "<ColorantTable>\n";
  for (i=0; i<(int)m_nCount; i++) {
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
              strncpy(m_pData[i].name, icUtf8ToAnsi(str, name), sizeof(m_pData[i].name));
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
      m_observerRange.end = icFtoF16((icFloatNumber)atoi(icXmlAttrValue(attr)));
    }
    attr = icXmlFindAttr(pChild, "steps");
    if (attr) {
      m_observerRange.steps = (icUInt16Number)atoi(icXmlAttrValue(attr));
    }
    attr = icXmlFindAttr(pChild, "reserved");
    if (attr) {
      m_reserved2 = (icUInt16Number)atoi(icXmlAttrValue(attr));
    }

    if (pChild->children && pChild->children->content) {
      CIccFloatArray vals;
      vals.ParseTextArray((icChar*)pChild->children->content);
      if (vals.GetSize()!=m_observerRange.steps*3)
        return false;
      m_observer = new icFloatNumber[m_observerRange.steps*3];
      if (!m_observer)
        return false;
      icFloatNumber *pBuf = vals.GetBuf();
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
      m_illuminantRange.steps = (icUInt16Number)atoi(icXmlAttrValue(attr));
    }
    attr = icXmlFindAttr(pChild, "reserved");
    if (attr) {
      m_reserved3 = (icUInt16Number)atoi(icXmlAttrValue(attr));
    }

    if (pChild->children && pChild->children->content && m_illuminantRange.steps) {
      CIccFloatArray vals;
      vals.ParseTextArray((icChar*)pChild->children->content);
      if (vals.GetSize()!=m_illuminantRange.steps)
        return false;
      m_illuminant = new icFloatNumber[m_illuminantRange.steps];
      if (!m_illuminant)
        return false;
      icFloatNumber *pBuf = vals.GetBuf();
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
  char fix[bufSize];
  char buf[bufSize];
  char data[bufSize];

  snprintf(buf, bufSize, "<ProfileDesc>\n");
  xml += blanks + buf;

  snprintf(buf, bufSize, "<DeviceManufacturerSignature>%s</DeviceManufacturerSignature>\n", icFixXml(fix, icGetSigStr(data, bufSize, p.m_deviceMfg)));
  xml += blanks + blanks + buf;

  snprintf(buf, bufSize, "<DeviceModelSignature>%s</DeviceModelSignature>\n", icFixXml(fix, icGetSigStr(data, bufSize, p.m_deviceModel)));
  xml += blanks + blanks + buf;

  std::string szAttributes = icGetDeviceAttrName(p.m_attributes);
  //snprintf(buf, bufSize, "<DeviceAttributes>\"%016lX\" technology=\"%s\">\n", p.m_attributes, icFixXml(fix, icGetSigStr(data, bufSize, p.m_technology)));
  //xml += buf;
  xml += blanks + blanks + icGetDeviceAttrName(p.m_attributes);

  snprintf(buf, bufSize, "<Technology>%s</Technology>\n", icFixXml(fix, icGetSigStr(data, bufSize, p.m_technology)));
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
          icResponse16Number response;

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

  if (!m_nSize) {
    xml += blanks + "<Curve/>\n";
  }
  else if (IsIdentity()) {
    xml += blanks + "<Curve IdentitySize=\"";
    snprintf(buf, bufSize, "%d", m_nSize);
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
      snprintf(buf, bufSize, " %3u", (int)(m_Curve[i] * 255.0 + 0.5));
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
      snprintf(buf, bufSize, " %5u", (int)(m_Curve[i] * 65535.0 + 0.5));
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
    if (filename[0]) {
      CIccIO *file = IccOpenFileIO(filename, "rb");
      if (!file){
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr +="' not found.\n";
        delete file;
        return false;
      }

      const char *format = icXmlAttrValue(pCurveNode, "Format");

      // format is text
      if (!strcmp(format, "text")) {
        size_t num = file->GetLength();
        char *buf = (char *) new char[num];

        if (!buf) {          
          perror("Memory Error");
          parseStr += "'";
          parseStr += filename;
          parseStr += "' may not be a valid text file.\n";
          delete [] buf;
          delete file;
          return false;
        }

        if (file->Read8(buf, num) !=num) {
          perror("Read-File Error");
          parseStr += "'";
          parseStr += filename;
          parseStr += "' may not be a valid text file.\n";
          delete [] buf;
          delete file;             
          return false;
        }         

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
            SetSize(data.GetSize());
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
            SetSize(data.GetSize());

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
            SetSize(data.GetSize());
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
          size_t num = file->GetLength();
          icUInt8Number value;

          SetSize( (icUInt32Number)num);
          icFloatNumber *dst =  GetData(0);
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
          size_t num = file->GetLength() / sizeof(icUInt16Number);
          icUInt16Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          SetSize((icUInt32Number)num);
          icFloatNumber *dst = GetData(0);
          size_t i;
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
          size_t num = file->GetLength()/sizeof(icFloat32Number);
          icFloat32Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          SetSize((icUInt32Number)num);
          icFloatNumber *dst = GetData(0);

          size_t i;
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
            SetSize(nSize);

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
          SetSize(data.GetSize());

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
            SetSize(nSize);

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
          SetSize(data.GetSize());

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

            SetSize(nSize);

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
          SetSize(data.GetSize());

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

        if (!SetFunctionType(atoi(functionType))){
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
          m_nReserved2 = (icUInt16Number)atoi(icXmlAttrValue(reserved2));
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
    return ((CIccSegmentedCurveXml*)m_pCurve)->ToXml(xml, blanks);

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
    CIccSegmentedCurveXml *pCurve = new CIccSegmentedCurveXml();
    
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
        pCurveTag = new CIccTagXmlCurve;
      }
      else if (!icXmlStrCmp(pCurveNode->name, "ParametricCurve")) {
        pCurveTag = new CIccTagXmlParametricCurve();
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

  pCLUT = new CIccCLUT(nInput, nOutput, nPrecision);

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
    icUInt8Number nGridGranularity;

    xmlAttr *gridGranularity = icXmlFindAttr(pNode, "GridGranularity");

    if (gridGranularity) {
      nGridGranularity = (icUInt8Number)atoi(icXmlAttrValue(gridGranularity));
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

    if (filename[0]) {
      CIccIO *file = IccOpenFileIO(filename, "rb");

      if (!file) {
        // added error message
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr +="' not found.\n";
        delete pCLUT;
        return NULL;
      }

      const char *format = icXmlAttrValue(table, "Format");

      if (!strcmp(format, "text")) {
        size_t num = file->GetLength();
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
          icUInt8Number value;
          // if number of entries in file is not equal to size of CLUT table, flag as error
          if (num!=pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }
          icFloatNumber *dst = pCLUT->GetData(0);
          icUInt32Number i;
          for (i=0; i<num; i++) {
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
          icUInt16Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (num<pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }
          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<num; i++) {
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
          icFloat32Number value;
          icUInt8Number *ptr = (icUInt8Number*)&value;

          if (num<pCLUT->NumPoints()*pCLUT->GetOutputChannels()) {
            parseStr += "Error! - Number of entries in file '";
            parseStr += filename;
            parseStr += "'is not equal to the size of the CLUT Table.\n";  
            parseStr += "    a. Check values of GridGranularity/GridPoints, InputChannel, or OutputChannels.\n";
            parseStr += "    b. File may not be a valid binary file.\n";
            delete file;
            delete pCLUT;
            return NULL;
          }
          icFloatNumber *dst = pCLUT->GetData(0);

          icUInt32Number i;
          for (i=0; i<num; i++) {
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

  int nIn = atoi(icXmlAttrValue(in));
  int nOut = atoi(icXmlAttrValue(out));

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
        if (!icCurvesFromXml(pCurves, !pMBB->IsInputMatrix() ? nIn : nOut, pNode->children, nType, parseStr)) {
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
    return new CIccMpeXmlCurveSet;
  }
  if (!strcmp(szElementNodeName, "MatrixElement")) {
    return new CIccMpeXmlMatrix;
  }
  if (!strcmp(szElementNodeName, "CLutElement")) {
    return new CIccMpeXmlCLUT;
  }
  if (!strcmp(szElementNodeName, "ExtCLutElement")) {
    return new CIccMpeXmlExtCLUT;
  }
  if (!strcmp(szElementNodeName, "CalculatorElement")) {
    return new CIccMpeXmlCalculator;
  }
  if (!strcmp(szElementNodeName, "TintArrayElement")) {
    return new CIccMpeXmlTintArray;
  }
  if (!strcmp(szElementNodeName, "ToneMapElement")) {
    return new CIccMpeXmlToneMap;
  }
  if (!strcmp(szElementNodeName, "JabToXYZElement")) {
    return new CIccMpeXmlJabToXYZ;
  }
  if (!strcmp(szElementNodeName, "UnknownElement")) {
    return new CIccMpeXmlUnknown;
  }
  if (!strcmp(szElementNodeName, "XYZToJabElement")) {
    return new CIccMpeXmlXYZToJab;
  }
  if (!strcmp(szElementNodeName, "EmissionMatrixElement")) {
    return new CIccMpeXmlEmissionMatrix;
  }
  if (!strcmp(szElementNodeName, "InvEmissionMatrixElement")) {
    return new CIccMpeXmlInvEmissionMatrix;
  }
  if (!strcmp(szElementNodeName, "EmissionCLutElement")) {
    return new CIccMpeXmlEmissionCLUT;
  }
  if (!strcmp(szElementNodeName, "ReflectanceCLutElement")) {
    return new CIccMpeXmlReflectanceCLUT;
  }
  if (!strcmp(szElementNodeName, "EmissionObserverElement")) {
    return new CIccMpeXmlEmissionObserver;
  }
  if (!strcmp(szElementNodeName, "ReflectanceObserverElement")) {
    return new CIccMpeXmlReflectanceObserver;
  }
  if (!strcmp(szElementNodeName, "BAcsElement")) {
    return new CIccMpeXmlBAcs;
  }
  if (!strcmp(szElementNodeName, "EAcsElement")) {
    return new CIccMpeXmlEAcs;
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

  m_nInputChannels = atoi(icXmlAttrValue(pInputChannels));
  m_nOutputChannels = atoi(icXmlAttrValue(pOutputChannels));

  if (!m_list) {
    m_list = new CIccMultiProcessElementList();
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
    char fix[bufSize];
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
        snprintf(buf, bufSize, "<LocalizedText LanguangeCountry=\"%s\"", icFixXml(fix, icGetSigStr(data, bufSize, (i->m_nLanguageCode<<16) + i->m_nCountryCode)));
        xml += blanks + buf;

        snprintf(buf, bufSize, ">%s</LocalizedText>\n", icFixXml(fix, icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength())));
        xml += buf;
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
      if ((langCode = icXmlFindAttr(pSubNode, "languageCountry")) &&
        pSubNode->children) {
          xmlNode *pText;

          for (pText = pSubNode->children; pText && pText->type != XML_TEXT_NODE; pText = pText->next);

          if (pText) {
            icUInt32Number lc = icGetSigVal(icXmlAttrValue(langCode));
            CIccUTF16String str((const char*)pText->content);
            desc.m_desc.SetText(str.c_str(), (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff));
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
    char buf[bufSize];
    char data[bufSize];
    char fix[bufSize];
    std::string bufstr;

    xml += blanks + " <DictEntry Name=\"";
    xml += icFixXml(fix, icUtf16ToUtf8(bufstr, (icUInt16Number*)nv->GetName().c_str(), (int)nv->GetName().size()));
    xml += "\"";

    if (nv->IsValueSet()) {
      xml += " Value=\"";
      xml += icFixXml(fix, icUtf16ToUtf8(bufstr, (icUInt16Number*)nv->GetValue().c_str(), (int)nv->GetValue().size()));
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
          snprintf(buf, bufSize, "  <LocalizedName LanguageCountry=\"%s\"", icFixXml(fix, icGetSigStr(data, bufSize, (i->m_nLanguageCode<<16) + i->m_nCountryCode)));
          xml += blanks + buf;

          snprintf(buf, bufSize, "><![CDATA[%s]]></LocalizedName>\n", icFixXml(fix, icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength())));
          xml += buf;
        }
      }
      if (nv->GetValueLocalized()) {
        CIccMultiLocalizedUnicode::iterator i;

        for (i=nv->GetValueLocalized()->m_Strings->begin(); i!=nv->GetValueLocalized()->m_Strings->end(); i++) {
          snprintf(buf, bufSize, "  <LocalizedValue LanguageCountry=\"%s\"", icFixXml(fix, icGetSigStr(data, bufSize, (i->m_nLanguageCode<<16) + i->m_nCountryCode)));
          xml += blanks + buf;

          snprintf(buf, bufSize, "><![CDATA[%s]]></LocalizedValue>\n", icFixXml(fix, icUtf16ToUtf8(bufstr, i->GetBuf(), i->GetLength())));
          xml += buf;
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
    CIccDictEntry *pDesc = new CIccDictEntry();
    xmlAttr *pAttr;
    CIccUTF16String str;

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
          pTag = new CIccTagMultiLocalizedUnicode();
          pDesc->SetNameLocalized(pTag);
        }

        if ((pAttr = icXmlFindAttr(pChild, "LanguageCountry")) && pChild->children) {
          xmlNode *pText;
          icUInt32Number lc = icGetSigVal(icXmlAttrValue(pAttr));

          for (pText = pChild->children; pText && pText->type != XML_TEXT_NODE && pText->type != XML_CDATA_SECTION_NODE; pText = pText->next);

          if (pText) {
            CIccUTF16String str((const char*)pText->content);

            pTag->SetText(str.c_str(), (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff));
          }
          else {
            pTag->SetText("");
          }
        }
      }
      else if (pChild->type == XML_ELEMENT_NODE && !icXmlStrCmp(pChild->name, "LocalizedValue")) {
        CIccTagMultiLocalizedUnicode *pTag = pDesc->GetValueLocalized();
        if (!pTag) {
          pTag = new CIccTagMultiLocalizedUnicode();
          pDesc->SetValueLocalized(pTag);
        }

        if ((pAttr = icXmlFindAttr(pChild, "LanguageCountry")) && pChild->children) {
          xmlNode *pText;
          icUInt32Number lc = icGetSigVal(icXmlAttrValue(pAttr));

          for (pText = pChild->children; pText && pText->type != XML_TEXT_NODE && pText->type != XML_CDATA_SECTION_NODE; pText = pText->next);

          if (pText) {
            CIccUTF16String str((const char*)pText->content);
            pTag->SetText(str.c_str(), (icLanguageCode)(lc>>16), (icCountryCode)(lc & 0xffff));
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
  const size_t bufSize = 256;
  char buf[bufSize], fix[bufSize], line[bufSize];
  IIccStruct *pStruct = GetStructHandler();

  const icChar *structName = ((pStruct != NULL) ? pStruct->GetDisplayName() : NULL);
  blanks += "  ";

   if (structName && strcmp(structName, "privateStruct")) {
     snprintf(line, bufSize, "<%s> <MemberTags>\n", structName);
   }
   else {
     // print out the struct signature
     snprintf(line, bufSize, "<privateStruct StructSignature=\"%s\"/> <MemberTags>\n", icFixXml(fix, icGetSigStr(buf,bufSize, m_sigStructType)));
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
              printf("Unable to output sub-tag with type %s\n", icGetSigStr(buf, bufSize, i->TagInfo.sig));
              return false;
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
        xmlAttr *attr = icXmlFindAttr(pTypeNode, "type");
        sigType = (icTagTypeSignature)icGetSigVal((icChar*)icXmlAttrValue(attr));
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
          AttachElem(sigTag, pTag);
        }
        else {
          parseStr += "Unable to Parse \"";
          parseStr += (const char*)pTypeNode->name;
          parseStr += "\" (";
          parseStr += nodeName;
          parseStr += ") Tag\n";
          return false;
        }
      }
      else {
        parseStr += "Invalid tag extension for \"";
        parseStr += (const char*)pTypeNode->name;
        parseStr += "\" (";
        parseStr += nodeName;
        parseStr += ") Tag\n";
        return false;
      }
    }
  }
  else {  //Legacy parsing of XML tags by type
    // get the tag type signature
    icTagTypeSignature sigType = icGetTypeNameTagSig(nodeName.c_str());

    if (sigType == icSigUnknownType) {
      xmlAttr *attr = icXmlFindAttr(pNode, "type");
      sigType = (icTagTypeSignature)icGetSigVal((icChar*)icXmlAttrValue(attr));
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

        for (xmlNode *tagSigNode = pNode->children; tagSigNode; tagSigNode = tagSigNode->next) {
          if (tagSigNode->type == XML_ELEMENT_NODE && !icXmlStrCmp(tagSigNode->name, "TagSignature")) {
            sigTag = (icTagSignature)icGetSigVal((const icChar*)tagSigNode->children->content);
            AttachElem(sigTag, pTag);
          }
        }
      }
      else {
        parseStr += "Unable to Parse \"";
        parseStr += info.GetTagTypeSigName(sigType);
        parseStr += "\" (";
        parseStr += nodeName;
        parseStr += ") Tag\n";
        return false;
      }
    }
    else {
      parseStr += "Invalid tag extension for \"";
      parseStr += info.GetTagTypeSigName(sigType);
      parseStr += "\" (";
      parseStr += nodeName;
      parseStr += ") Tag\n";
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
  const size_t bufSize = 256;
  char buf[bufSize], fix[bufSize], line[bufSize];

  std::string arrayName;
  std::string arrayBlanks = "";
  bool found = CIccArrayCreator::GetArraySigName(arrayName, m_sigArrayType, false);

  if (found) {
    snprintf(line, bufSize, "<%s> ", arrayName.c_str());
    arrayBlanks = "  ";
  }
  else {
    // print out the struct signature
    snprintf(line, bufSize, "<privateArray StructSignature=\"%s\"/> ", icFixXml(fix, icGetSigStr(buf, bufSize, m_sigArrayType)));
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
        xmlAttr *attr = icXmlFindAttr(pNode, "type");
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
            return false;
          }
        }
        else {
          parseStr += "Unable to Parse xml node named  \"";
          parseStr += (icChar*)tagNode->name;
          parseStr += "\"\n";
          return false;
        }
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
      snprintf(line, lineSize, " <T>%u %u %u</T>\n", m_Triangles[i].m_VertexNumbers[0], m_Triangles[i].m_VertexNumbers[1], m_Triangles[i].m_VertexNumbers[2]);
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
    m_nPCSChannels = atoi(icXmlAttrValue(subNode, "channels", "0"));

    if (!m_nPCSChannels) {
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

    m_PCSValues = new icFloatNumber[m_NumberOfVertices * m_nPCSChannels];

    if (!m_PCSValues)
      return false;

    memcpy(m_PCSValues, vals.GetBuf(), m_NumberOfVertices * m_nPCSChannels*sizeof(icFloatNumber));
  }
  else {
    parseStr += "Cannot find PCSValues\n";
    return false;
  }

  subNode = icXmlFindNode(childNode->children, "DeviceValues");

  if (subNode) {
    m_nDeviceChannels = atoi(icXmlAttrValue(subNode, "channels", "0"));

    if (!m_nDeviceChannels) {
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

    m_DeviceValues = new icFloatNumber[m_NumberOfVertices * m_nDeviceChannels];

    if (!m_DeviceValues)
      return false;

    memcpy(m_DeviceValues, vals.GetBuf(), m_NumberOfVertices * m_nDeviceChannels * sizeof(icFloatNumber));
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
  m_Triangles = new icGamutBoundaryTriangle[m_NumberOfTriangles];

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

  if (m_pProfile) {
    delete m_pProfile;
  }

  CIccProfileXml *pProfile = new CIccProfileXml();
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

  m_nSeamlesIndicator = atoi(icXmlAttrValue(tagNode, "SeamlessIndicator", "0"));
  m_nEncodingFormat = (icImageEncodingType)atoi(icXmlAttrValue(tagNode, "EncodingFormat", "0"));
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
    if (filename[0]) {
      CIccIO *file = IccOpenFileIO(filename, "rb");
      if (!file) {
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' not found.\n";
        delete file;
        return false;
      }

      size_t num = file->GetLength();

      SetSize((icUInt32Number)num);
      icUInt8Number *dst = GetData(0);
      if (file->Read8(dst, num)!=num) {
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
  snprintf(buf, bufSize, " SeamlessIndicator=\"%d\"", m_nSeamlesIndicator);
  xml += buf;

  snprintf(buf, bufSize, " EncodingFormat=\"%d\"", m_nEncodingFormat);
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

  m_nSeamlesIndicator = atoi(icXmlAttrValue(tagNode, "SeamlessIndicator", "0"));
  m_nEncodingFormat = (icImageEncodingType)atoi(icXmlAttrValue(tagNode, "EncodingFormat", "0"));

  xmlNode *pImageNode;
  pImageNode = icXmlFindNode(tagNode->children, "Image");

  if (pImageNode) {
    const char *filename = icXmlAttrValue(pImageNode, "File");
    if (!filename || !filename[0]) {
      filename = icXmlAttrValue(pImageNode, "Filename");
    }

    // file exists
    if (filename[0]) {
      CIccIO *file = IccOpenFileIO(filename, "rb");
      if (!file) {
        parseStr += "Error! - File '";
        parseStr += filename;
        parseStr += "' not found.\n";
        delete file;
        return false;
      }

      size_t num = file->GetLength();

      SetSize((icUInt32Number)num);
      icUInt8Number *dst = GetData(0);
      if (file->Read8(dst, num) != num) {
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
  snprintf(buf, bufSize, " SeamlessIndicator=\"%d\"", m_nSeamlesIndicator);
  xml += buf;

  snprintf(buf, bufSize, " EncodingFormat=\"%d\"", m_nEncodingFormat);
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
