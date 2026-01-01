/** @file
    File:       IccMpeXml.cpp

    Contains:   Implementation of MultiProcessElementType Element XML
                conversion functionality

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
#include "IccMpeXml.h"
#include "IccUtilXml.h"
#include "IccIoXml.h"
#include "IccCAM.h"

#include <cstring> /* C strings strcpy, memcpy ... */

#ifdef WIN32
#include <windows.h>
#ifdef GetClassName
#undef GetClassName
#endif
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

bool CIccMpeXmlUnknown::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  //icUInt8Number *m_ptr = m_pData;

  const size_t bufSize = 256;
  char line[bufSize];
  char buf[bufSize];
  char fix[bufSize];
  snprintf(line, bufSize, "<UnknownElement Type=\"%s\" InputChannels=\"%d\" OutputChannels=\"%d\"", 
           icFixXml(fix, icGetSigStr(buf, bufSize, GetType())), NumInputChannels(), NumOutputChannels());
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  icXmlDumpHexData(xml, blanks + "  ", m_pData, m_nSize);

  xml += blanks + "</UnknownElement>\n";
  return true;
}


bool CIccMpeXmlUnknown::ParseXml(xmlNode *pNode, std::string & /*parseStr*/)
{
  SetType((icElemTypeSignature)icXmlStrToSig(icXmlAttrValue(pNode, "type")));
  SetChannels(atoi(icXmlAttrValue(pNode, "InputChannels")), atoi(icXmlAttrValue(pNode, "OutputChannels")));

  if (pNode->children && pNode->children->type == XML_TEXT_NODE && pNode->children->content) {
    icUInt32Number nSize = icXmlGetHexDataSize((const char *)pNode->children->content);

    if (!SetDataSize(nSize, false)) 
      return false;

    icXmlGetHexData(m_pData, (const char *)pNode->children->content, nSize);
  }
  else {
    SetDataSize(0);
  }
  return true;
}


class CIccFormulaCurveSegmentXml : public CIccFormulaCurveSegment
{
public:
  CIccFormulaCurveSegmentXml(icFloatNumber start, icFloatNumber end) : CIccFormulaCurveSegment(start, end) {}

  bool ToXml(std::string &xml, std::string blanks/* = ""*/);
  bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

static char* icSegPos(char *buf, size_t bufSize, icFloatNumber pos)
{
  if (pos==icMinFloat32Number)
    strncpy(buf, "-infinity", bufSize);
  else if (pos==icMaxFloat32Number)
    strncpy(buf, "+infinity", bufSize);
  else
    snprintf(buf, bufSize, "%.8" ICFLOATSFX, pos);

  return buf;
}

icFloatNumber icGetSegPos(const char *str)
{
  if (!strncmp(str, "-inf", 4)) {
    return icMinFloat32Number;
  }
  if (!strncmp(str, "+inf", 4)) {
    return icMaxFloat32Number;
  }

  return (icFloatNumber)atof(str);
}


bool CIccFormulaCurveSegmentXml::ToXml(std::string &xml, std::string blanks)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  char line[bufSize];

  snprintf(line, bufSize, "<FormulaSegment Start=\"%s\"", icSegPos(buf, bufSize, m_startPoint));
  xml += blanks + line;

  snprintf(line, bufSize, " End=\"%s\"",icSegPos(buf, bufSize, m_endPoint));
  xml += line;
    
  snprintf(line, bufSize, " FunctionType=\"%d\"", m_nFunctionType);
  xml += line;

  if (m_nReserved) {
    snprintf(line, bufSize, " Reserved=\"%d\"", m_nReserved);
    xml += line;
  }
  if (m_nReserved2) {
    snprintf(line, bufSize, " Reserved2=\"%d\"", m_nReserved2);
    xml += line;
  }
  xml += ">\n";

  CIccFloatArray::DumpArray(xml, blanks+"  ", m_params, m_nParameters, icConvertFloat, 8);

  xml += blanks + "</FormulaSegment>\n";

  return true;
}


bool CIccFormulaCurveSegmentXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *funcType = icXmlFindAttr(pNode, "FunctionType");

  if (!funcType) {
    parseStr += "Bad FunctionType in Formula Segment\n";
    return false;
  }

  m_nReserved2 = atoi(icXmlAttrValue(pNode, "Reserved2"));
  m_nFunctionType = atoi(icXmlAttrValue(funcType));


  switch(m_nFunctionType) {
    case 0x0000:
      m_nParameters = 4;
      break;

    case 0x0001:
    case 0x0002:
    case 0x0003:
    case 0x0004:
      m_nParameters = 5;
      break;

    case 0x0005:
    case 0x0007:
      m_nParameters = 6;
      break;

    case 0x0006:
      m_nParameters = 7;
      break;

    default:
      parseStr += "Unsupported FunctionType value in Formula Segment\n";
      return false;
  }

  CIccFloatArray args;

  if (!args.ParseArray(pNode->children))
    return false;

  if (args.GetSize()<m_nParameters)
    return false;

  if (m_params) {
    free(m_params);
  }

  if (m_nParameters) {
    m_params = (icFloatNumber*)malloc(m_nParameters * sizeof(icFloatNumber));
    if (!m_params)
      return false;

    memcpy(m_params, args.GetBuf(), m_nParameters * sizeof(icFloatNumber));
  }
  else
    m_params = NULL;

  return true;
}


class CIccSampledCurveSegmentXml : public CIccSampledCurveSegment
{
public:
  CIccSampledCurveSegmentXml(icFloatNumber start, icFloatNumber end) : CIccSampledCurveSegment(start, end) {}

  bool ToXml(std::string &xml, std::string blanks/* = ""*/);
  bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

bool CIccSampledCurveSegmentXml::ToXml(std::string &xml, std::string blanks)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  char line[bufSize];

  snprintf(line, bufSize, "<SampledSegment Start=\"%s\"", icSegPos(buf, bufSize, m_startPoint));
  xml += blanks + line;

  snprintf(line, bufSize, " End=\"%s\">\n",icSegPos(buf, bufSize, m_endPoint));
  xml += line;

  CIccFloatArray::DumpArray(xml, blanks+"  ", m_pSamples, m_nCount, icConvertFloat, 8);

  xml += blanks + "</SampledSegment>\n";

  return true;
}


bool CIccSampledCurveSegmentXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  const char *filename = icXmlAttrValue(pNode, "Filename");

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

    const char *format = icXmlAttrValue(pNode, "Format");

    // format is text
    if (!strcmp(format, "text")) {
      size_t num = file->GetLength();
      char *buf = new char[num];

      if (!buf) {          
        perror("Memory Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid text file.\n";
        delete file;
        return false;
      }

      if (file->Read8(buf, num) !=num) {
        perror("Read-File Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid text file.\n";
        delete[] buf;
        delete file;             
        return false;
      }   

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
        icFloatNumber *dst = m_pSamples;          

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
    // format is binary
    else if (!strcmp(format, "binary")) {
      const char *order = icXmlAttrValue(pNode, "Endian");
      bool little_endian = !strcmp(order, "little");

      icUInt16Number storageType = icValueTypeFloat32;
      xmlAttr *attr = icXmlFindAttr(pNode, "StorageType");
      if (attr) {
       storageType = (icUInt16Number)atoi(icXmlAttrValue(attr));
      }

      if (storageType == icValueTypeUInt8){
        size_t num = file->GetLength();
        icUInt8Number value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst =  m_pSamples;
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
      else if (storageType == icValueTypeUInt16){
        size_t num = file->GetLength() / sizeof(icUInt16Number);
        icUInt16Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;
        icUInt32Number i;
        for (i=0; i<num; i++) {
          if (!file->Read16(&value)) {  //assumes data is big endian
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
            icUInt8Number t = m_ptr[0];
            m_ptr[0] = m_ptr[1];
            m_ptr[1] = t;
          }
          *dst++ = (icFloatNumber)value / 65535.0f;
        }
        delete file;
        return true;
      }
      else if (storageType == icValueTypeFloat16){
        size_t num = file->GetLength() / sizeof(icFloat16Number);
        icFloat16Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;
        icUInt32Number i;
        for (i=0; i<num; i++) {
          if (!file->Read16(&value)) { //assumes data is big endian
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
            icUInt8Number t = m_ptr[0];
            m_ptr[0] = m_ptr[1];
            m_ptr[1] = t;
          }
          *dst++ = icF16toF(value);
        }
        delete file;
        return true;
      }
      else if (storageType == icValueTypeFloat32) {
        size_t num = file->GetLength()/sizeof(icFloat32Number);
        icFloat32Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;

        icUInt32Number i;
        for (i=0; i<num; i++) {
          if (!file->ReadFloat32Float(&value)) {  //assumes data is big endian
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
            tmp = m_ptr[0]; m_ptr[0] = m_ptr[3]; m_ptr[3] = tmp;
            tmp = m_ptr[1]; m_ptr[1] = m_ptr[2]; m_ptr[2] = tmp;
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
  }
  else {
    CIccFloatArray samples;

    if (!samples.ParseArray(pNode->children)) {
      return false;
    }

    if (!SetSize(samples.GetSize())) {
      return false;
    }

    memcpy(m_pSamples, samples.GetBuf(), samples.GetSize() * sizeof(icFloatNumber));
  }

  return true;
}


class CIccSampledCalculatorCurveXml : public CIccSampledCalculatorCurve
{
public:
  CIccSampledCalculatorCurveXml(icFloatNumber first=0, icFloatNumber last=0) : CIccSampledCalculatorCurve(first, last) {}

  bool ToXml(std::string &xml, std::string blanks/* = ""*/);
  bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

bool CIccSampledCalculatorCurveXml::ToXml(std::string &xml, std::string blanks)
{
  const size_t lineSize = 256;
  char line[lineSize];

  xml += blanks;
  snprintf(line, lineSize, "<SampledCalculatorCurve FirstEntry=\"" icXmlFloatFmt "\"", m_firstEntry);
  xml += line;

  snprintf(line, lineSize, " LastEntry=\"" icXmlFloatFmt "\"", m_lastEntry);
  xml += line;

  snprintf(line, lineSize, " ExtensionType=\"%u\"", m_extensionType);
  xml += line;

  snprintf(line, lineSize, " DesiredSize=\"%u\">\n", m_nDesiredSize);
  xml += line;

  if (m_nReserved2) {
    snprintf(line, lineSize, " Reservered2=\"%u\">\n", m_nReserved2);
    xml += line;
  }

  if (m_pCalc && !strcmp(m_pCalc->GetClassName(), "CIccMpeXmlCalculator")) {
    CIccMpeXmlCalculator *pXmlCalc = (CIccMpeXmlCalculator*)m_pCalc;
    if (!pXmlCalc->ToXml(xml, blanks + "  "))
      return false;
  }
  else {
    return false;
  }

  xml += blanks + "</SampledCalculatorCurve>\n";

  return true;
}


bool CIccSampledCalculatorCurveXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *attr = icXmlFindAttr(pNode, "FirstEntry");

  if (!attr) {
    parseStr += "Bad FirstEntry in Sampled Calculator Curve\n";
    return false;
  }

  m_firstEntry = (icFloatNumber)atof(icXmlAttrValue(attr));

  attr = icXmlFindAttr(pNode, "LastEntry");

  if (!attr) {
    parseStr += "Bad LastEntry in Sampled Calculator Curve\n";
    return false;
  }

  m_lastEntry = (icFloatNumber)atof(icXmlAttrValue(attr));

  attr = icXmlFindAttr(pNode, "DesiredSize");

  if (!attr) {
    parseStr += "Bad DesiredSize in Sampled Calculator Curve\n";
    return false;
  }

  m_nDesiredSize = (icUInt32Number)atoi(icXmlAttrValue(attr));

  xmlNode *pCalcNode = icXmlFindNode(pNode->children, "CalculatorElement");
  if (pCalcNode) {
    CIccMpeXmlCalculator *pCalc = new CIccMpeXmlCalculator();

    if (!pCalc->ParseXml(pCalcNode, parseStr)) {
      parseStr += "Unable to parse calculator element in Sampled Calculator Curve\n";
      return false;
    }
    m_pCalc = pCalc;
  }
  else {
    parseStr += "Missing Calculator Element in Sampled Calculator Curve\n";
    return false;
  }

  return true;
}


class CIccSingleSampledeCurveXml : public CIccSingleSampledCurve
{
public:
  CIccSingleSampledeCurveXml(icFloatNumber first = 0, icFloatNumber last = 0) : CIccSingleSampledCurve(first, last) {}

  bool ToXml(std::string &xml, std::string blanks/* = ""*/);
  bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

bool CIccSingleSampledeCurveXml::ToXml(std::string &xml, std::string blanks)
{
  const size_t lineSize = 256;
  char line[lineSize];

  xml += blanks;
  snprintf(line, lineSize, "<SingleSampledCurve FirstEntry=\"" icXmlFloatFmt "\"", m_firstEntry);
  xml += line;

  snprintf(line, lineSize, " LastEntry=\"" icXmlFloatFmt "\"", m_lastEntry);
  xml += line;

  snprintf(line, lineSize, " StorageType=\"%u\"", m_storageType);
  xml += line;

  snprintf(line, lineSize, " ExtensionType=\"%u\">\n", m_extensionType);
  xml += line;

  CIccFloatArray::DumpArray(xml, blanks + "  ", m_pSamples, m_nCount, icConvertFloat, 8);

  xml += blanks + "</SingleSampledCurve>\n";

  return true;
}


bool CIccSingleSampledeCurveXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlAttr *attr = icXmlFindAttr(pNode, "FirstEntry");

  if (!attr) {
    parseStr += "Bad FirstEntry in Simple Sampled Segment\n";
    return false;
  }

  m_firstEntry = (icFloatNumber)atof(icXmlAttrValue(attr));

  attr = icXmlFindAttr(pNode, "LastEntry");

  if (!attr) {
    parseStr += "Bad LastEntry in Simple Sampled Segment\n";
    return false;
  }

  m_lastEntry = (icFloatNumber)atof(icXmlAttrValue(attr));

  m_storageType = icValueTypeFloat32;
  attr = icXmlFindAttr(pNode, "StorageType");
  if (attr) {
    m_storageType = (icUInt16Number)atoi(icXmlAttrValue(attr));
  }

  m_extensionType = icClipSingleSampledCurve;
  attr = icXmlFindAttr(pNode, "ExtensionType");
  if (attr) {
    m_extensionType = (icUInt16Number)atoi(icXmlAttrValue(attr));
  }

  const char *filename = icXmlAttrValue(pNode, "Filename");

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

    const char *format = icXmlAttrValue(pNode, "Format");

    // format is text
    if (!strcmp(format, "text")) {
      size_t num = file->GetLength();
      char *buf = new char[num];

      if (!buf) {
        perror("Memory Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid text file.\n";
        delete file;
        return false;
      }

      if (file->Read8(buf, num) != num) {
        perror("Read-File Error");
        parseStr += "'";
        parseStr += filename;
        parseStr += "' may not be a valid text file.\n";
        delete [] buf;
        delete file;
        return false;
      }

      // lut8type
      if (m_storageType == icValueTypeUInt8) {
        CIccUInt8Array data;

        //if (!data.ParseTextArray(buf)) {
        if (!data.ParseTextArrayNum(buf, num, parseStr)) {
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
          icFloatNumber *dst = m_pSamples;

          icUInt32Number i;
          for (i = 0; i < data.GetSize(); i++) {
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
      else if (m_storageType == icValueTypeUInt16) {
        CIccUInt16Array data;

        //if (!data.ParseTextArray(buf)) {
        if (!data.ParseTextArrayNum(buf, num, parseStr)) {
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
          icFloatNumber *dst = m_pSamples;

          icUInt32Number i;
          for (i = 0; i < data.GetSize(); i++) {
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
      else if (m_storageType == icValueTypeFloat32) {
        CIccFloatArray data;

        //if (!data.ParseTextArray(buf)) {
        if (!data.ParseTextArrayNum(buf, num, parseStr)) {
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
          icFloatNumber *dst = m_pSamples;

          icUInt32Number i;
          for (i = 0; i < data.GetSize(); i++) {
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
        delete[]buf;
        delete file;
        return false;
      }
    }
    // format is binary
    else if (!strcmp(format, "binary")) {
      const char *order = icXmlAttrValue(pNode, "Endian");
      bool little_endian = !strcmp(order, "little");

      if (m_storageType == icValueTypeUInt8) {
        size_t num = file->GetLength();
        icUInt8Number value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;
        icUInt32Number i;
        for (i = 0; i < num; i++) {
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
      else if (m_storageType == icValueTypeUInt16) {
        size_t num = file->GetLength() / sizeof(icUInt16Number);
        icUInt16Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;
        icUInt32Number i;
        for (i = 0; i < num; i++) {
          if (!file->Read16(&value)) { //assumes data is big endian
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
            icUInt8Number t = m_ptr[0];
            m_ptr[0] = m_ptr[1];
            m_ptr[1] = t;
          }
          *dst++ = (icFloatNumber)value / 65535.0f;
          }
        delete file;
        return true;
        }
      else if (m_storageType == icValueTypeFloat16) {
        size_t num = file->GetLength() / sizeof(icFloat16Number);
        icFloat16Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;
        icUInt32Number i;
        for (i = 0; i < num; i++) {
          if (!file->Read16(&value)) {  //assumes data is big endian
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
            icUInt8Number t = m_ptr[0];
            m_ptr[0] = m_ptr[1];
            m_ptr[1] = t;
          }
          *dst++ = icF16toF(value);
          }
        delete file;
        return true;
        }
      else if (m_storageType == icValueTypeFloat32) {
        size_t num = file->GetLength() / sizeof(icFloat32Number);
        icFloat32Number value;
        icUInt8Number *m_ptr = (icUInt8Number*)&value;

        SetSize((icUInt32Number)num);
        icFloatNumber *dst = m_pSamples;

        icUInt32Number i;
        for (i = 0; i < num; i++) {
          if (!file->ReadFloat32Float(&value)) {
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
            tmp = m_ptr[0]; m_ptr[0] = m_ptr[3]; m_ptr[3] = tmp;
            tmp = m_ptr[1]; m_ptr[1] = m_ptr[2]; m_ptr[2] = tmp;
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
  else {
    CIccFloatArray samples;

    if (!samples.ParseArray(pNode->children)) {
      return false;
    }

    if (!SetSize(samples.GetSize())) {
      return false;
    }

    memcpy(m_pSamples, samples.GetBuf(), samples.GetSize() * sizeof(icFloatNumber));
  }

  return true;
      }


bool CIccSegmentedCurveXml::ToXml(std::string &xml, std::string blanks)
{
  CIccCurveSegmentList::iterator seg;
  xml += blanks + "<SegmentedCurve>\n";
  for (seg = m_list->begin(); seg!=m_list->end(); seg++) {
    CIccCurveSegment* pSeg = *seg;
    if (pSeg && pSeg->GetType() == icSigFormulaCurveSeg) {
      if (!((CIccFormulaCurveSegmentXml*)pSeg)->ToXml(xml, blanks+"  "))
        return false;
    }
    else if (pSeg && pSeg->GetType() == icSigSampledCurveSeg) {
      if (!((CIccSampledCurveSegmentXml*)pSeg)->ToXml(xml, blanks+"  "))
        return false;
    }
    else
      return false;
  }
  xml += blanks + "</SegmentedCurve>\n";
  return true;
}


bool CIccSegmentedCurveXml::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_list->clear();
  for (pNode=pNode->children; pNode; pNode=pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE) {
      icFloatNumber start = icGetSegPos(icXmlAttrValue(pNode, "Start"));
      icFloatNumber end = icGetSegPos(icXmlAttrValue(pNode, "End"));

      if (!icXmlStrCmp(pNode->name, "FormulaSegment")) {
        CIccFormulaCurveSegmentXml *pSegXml = new CIccFormulaCurveSegmentXml(start, end);

        if (!pSegXml->ParseXml(pNode, parseStr)) {
          parseStr += "Unable to parse FormulaSegment\n";
          delete pSegXml;
          return false;
        }
        m_list->push_back(pSegXml);
      }
      else if (!icXmlStrCmp(pNode->name, "SampledSegment")) {
        CIccSampledCurveSegmentXml *pSegXml = new CIccSampledCurveSegmentXml(start, end);

        if (!pSegXml->ParseXml(pNode, parseStr)) {
          parseStr += "Unable to parse SampledSegment\n";
          delete pSegXml;
          return false;
        }
        m_list->push_back(pSegXml);
      }
      else {
        parseStr += "Bad Segment Type\n";
        return false;
      }
    }
  }
  return true;
}


static bool ToXmlCurve(std::string& xml, std::string blanks, icCurveSetCurvePtr pCurve)
{
  if (pCurve->GetType() == icSigSingleSampledCurve) {
    CIccSingleSampledeCurveXml* m_ptr = (CIccSingleSampledeCurveXml*)pCurve;

    if (!(m_ptr->ToXml(xml, blanks + "  ")))
      return false;
  }
  else if (pCurve->GetType() == icSigSegmentedCurve) {
    CIccSegmentedCurveXml* m_ptr = (CIccSegmentedCurveXml*)pCurve;

    if (!(m_ptr->ToXml(xml, blanks + "  ")))
      return false;
  }
  else if (pCurve->GetType() == icSigSampledCalculatorCurve) {
    CIccSampledCalculatorCurveXml* m_ptr = (CIccSampledCalculatorCurveXml*)pCurve;

    if (!(m_ptr->ToXml(xml, blanks + "  ")))
      return false;
  }
  else
    return false;

  return true;
}

bool CIccMpeXmlCurveSet::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 256;
  char line[lineSize];
  snprintf(line, lineSize, "<CurveSetElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, lineSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }
  xml += ">\n";

  int i, j;

  for (i=0; i<NumInputChannels(); i++) {

    //check for duplicate curves
    for (j = 0; j < i; j++) {
      if (m_curve[i] == m_curve[j])
        break;
    }

    //handle case of duplicate curve
    if (j < i) {
      snprintf(line, lineSize, "<DuplicateCurve Index=\"%d\"/>\n", j);
      xml += blanks + "  ";
      xml += line;
    }
    else if (!ToXmlCurve(xml, blanks, m_curve[i]))
      return false;
  }

  xml += blanks + "</CurveSetElement>\n";
  return true;
}

static icCurveSetCurvePtr ParseXmlCurve(xmlNode* pNode, std::string parseStr)
{
  icCurveSetCurvePtr rv = NULL;
  
  if (!pNode)
    return rv;

  if (!strcmp((const char*)pNode->name, "SegmentedCurve")) {
    CIccSegmentedCurveXml* pCurve = new CIccSegmentedCurveXml();

    if (pCurve->ParseXml(pNode, parseStr)) {
      rv = pCurve;
    }
    else
      delete pCurve;
  }
  else if (!strcmp((const char*)pNode->name, "SingleSampledCurve")) {
    CIccSingleSampledeCurveXml* pCurve = new CIccSingleSampledeCurveXml();

    if (pCurve->ParseXml(pNode, parseStr)) {
      rv = pCurve;
    }
    else
      delete pCurve;
  }
  else if (!strcmp((const char*)pNode->name, "SampledCalculatorCurve")) {
    CIccSampledCalculatorCurveXml* pCurve = new CIccSampledCalculatorCurveXml();

    if (pCurve->ParseXml(pNode, parseStr)) {
      rv = pCurve;
    }
    else
      delete pCurve;
  }

  return rv;
}


bool CIccMpeXmlCurveSet::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nChannels = (icUInt16Number)atoi(icXmlAttrValue(pNode, "InputChannels"));

  if (!nChannels || atoi(icXmlAttrValue(pNode, "OutputChannels")) != nChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In CurveSetElement\n";
    return false;
  }

  SetSize(nChannels);
  int nIndex = 0;
  for (pNode = pNode->children, nIndex = 0;
       pNode;
       pNode=pNode->next) {
    if (pNode->type == XML_ELEMENT_NODE) {
      if (nIndex >= nChannels) {
        return false;
      }
      else if (!strcmp((const char*)pNode->name, "DuplicateCurve")) {
        const char *attr = icXmlAttrValue(pNode, "Index", NULL);

        if (attr) {
          int nCopyIndex = atoi(attr);
          if (nCopyIndex >= 0 && nCopyIndex < nIndex) {
            m_curve[nIndex] = m_curve[nCopyIndex];
            nIndex++;
          }
          else {
            parseStr += "Invalid index for duplicate CurveSet Curve\n";
            return false;
          }
        }
        else {
          parseStr += "Missing index for duplicate CurveSet Curve\n";
          return false;
        }
      }
      else {
        icCurveSetCurvePtr pCurve = ParseXmlCurve(pNode, parseStr);

        if (!pCurve)
          return false;

        if (!SetCurve(nIndex, pCurve))
          return false;
        nIndex++;
      }
    }
  }

  if (!pNode && nIndex == nChannels)
    return true;

  return false;
}


bool CIccMpeXmlMatrix::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  snprintf(buf, bufSize, "<MatrixElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  int i, j, n;

  if (m_pMatrix) {
    xml += blanks + "  <MatrixData>\n";

    for (n=0, j=0; j<NumOutputChannels(); j++) {
      xml += blanks + "   ";
      for (i=0; i<NumInputChannels(); i++, n++) {
         snprintf(buf, bufSize, " " icXmlFloatFmt, m_pMatrix[n]);
         xml += buf;
      }
      xml += "\n";
    }
    xml += blanks + "  </MatrixData>\n";
  }

  if (m_pConstants) {
    for (i = 0; i < NumOutputChannels(); i++) {
      if (icNotZero(m_pConstants[i]))
        break;
    }
    if (i < NumOutputChannels()) {
      xml += blanks + "  <ConstantData>\n";

      xml += blanks + "   ";
      for (i = 0; i < NumOutputChannels(); i++) {
        snprintf(buf, bufSize, " " icXmlFloatFmt, m_pConstants[i]);
        xml += buf;
      }
      xml += "\n";

      xml += blanks + "  </ConstantData>\n";
    }
  }

  xml += blanks + "</MatrixElement>\n";

  return true;
}


bool CIccMpeXmlMatrix::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In MatrixElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "MatrixData");
  if (pData) {
    SetSize(nInputChannels, nOutputChannels);

    if (!CIccFloatArray::ParseArray(m_pMatrix, m_nInputChannels*m_nOutputChannels, pData->children))
      return false;

    const char *invert = icXmlAttrValue(pData, "InvertMatrix", "false");
    if (!stricmp(invert, "true")) {
      if (m_nInputChannels != m_nOutputChannels) {
        parseStr += "Inversion of MatrixData requires square matrix\n";
        return false;
      }
      IIccMatrixInverter *pInverter = IccGetDefaultMatrixInverter();
      if (!pInverter || !pInverter->Invert(m_pMatrix, m_nOutputChannels, m_nInputChannels)) {
        parseStr += "Unable to invert matrix!\n";
        return false;
      }
    }
  }
  else { //Having no MatrixData implies having a matrix full of zeros which is represented by m_pMatrix set to NULL
    SetSize(0, nOutputChannels); //Initialize with m_pMatrix set to NULL
    m_nInputChannels = nInputChannels; //Fix m_nInputChannels
  }

  pData = icXmlFindNode(pNode->children, "ConstantData");
  if (pData) {
    if (!CIccFloatArray::ParseArray(m_pConstants, m_nOutputChannels, pData->children))
      return false;
  }
  return true;
}

bool CIccMpeXmlEmissionMatrix::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  snprintf(buf, bufSize, "<EmissionMatrixElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += blanks + buf;

  int i, j, n;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }

  if (m_pMatrix) {
    xml += blanks + "  <MatrixData>\n";

    for (n=0, j=0; j<numVectors(); j++) {
      xml += blanks + "   ";
      for (i=0; i<(int)m_Range.steps; i++, n++) {
        snprintf(buf, bufSize, " " icXmlFloatFmt, m_pMatrix[n]);
        xml += buf;
      }
      xml += "\n";
    }
    xml += blanks + "  </MatrixData>\n";
  }

  if (m_pOffset) {
    xml += blanks + "  <OffsetData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pOffset[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </OffsetData>\n";
  }

  xml += blanks + "</EmissionMatrixElement>\n";

  return true;
}


bool CIccMpeXmlEmissionMatrix::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In MatrixElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (!nSteps) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }

  SetSize(nInputChannels, nOutputChannels, m_Range);
  if (!m_pWhite || !m_pMatrix || !m_pOffset) {
    parseStr += "Unable to SetSize\n";
    return false;
  }

  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing MatrixData";
  }

  pData = icXmlFindNode(pNode->children, "MatrixData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pMatrix, numVectors()*(int)m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing MatrixData";
  }

  pData = icXmlFindNode(pNode->children, "OffsetData");
  if (pData) {
    if (!CIccFloatArray::ParseArray(m_pOffset, m_Range.steps, pData->children))
      return false;
  }
  else {
    memset(m_pOffset, 0, (int)m_Range.steps*sizeof(icFloatNumber));
  }
  return true;
}

bool CIccMpeXmlInvEmissionMatrix::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  snprintf(buf, bufSize, "<InvEmissionMatrixElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += blanks + buf;

  int i, j, n;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }

  if (m_pMatrix) {
    xml += blanks + "  <MatrixData>\n";

    for (n=0, j=0; j<numVectors(); j++) {
      xml += blanks + "   ";
      for (i=0; i<(int)m_Range.steps; i++, n++) {
        snprintf(buf, bufSize, " " icXmlFloatFmt, m_pMatrix[n]);
        xml += buf;
      }
      xml += "\n";
    }
    xml += blanks + "  </MatrixData>\n";
  }

  if (m_pOffset) {
    xml += blanks + "  <OffsetData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pOffset[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </OffsetData>\n";
  }

  xml += blanks + "</InvEmissionMatrixElement>\n";

  return true;
}


bool CIccMpeXmlInvEmissionMatrix::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In MatrixElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (!nSteps) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }

  SetSize(nInputChannels, nOutputChannels, m_Range);
  if (!m_pWhite || !m_pMatrix || !m_pOffset) {
    parseStr += "Unable to SetSize\n";
    return false;
  }

  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing White Data";
  }

  pData = icXmlFindNode(pNode->children, "MatrixData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pMatrix, numVectors()*(int)m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing Matrix Data";
  }

  pData = icXmlFindNode(pNode->children, "OffsetData");
  if (pData) {
    if (!CIccFloatArray::ParseArray(m_pOffset, m_Range.steps, pData->children))
      return false;
  }
  else {
    memset(m_pOffset, 0, (int)m_Range.steps*sizeof(icFloatNumber));
  }
  return true;
}

bool CIccMpeXmlTintArray::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize], line[bufSize];
  snprintf(buf, bufSize, "<TintArrayElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  IIccExtensionTag *pTagEx;
  if (m_Array && (pTagEx=m_Array->GetExtension()) && !strcmp(pTagEx->GetExtClassName(), "CIccTagXml")) {
    CIccTagXml *pTagXml = (CIccTagXml*)pTagEx;
    const icChar* tagSig = icGetTagSigTypeName(m_Array->GetType());

    snprintf(line, bufSize, "  <%s>\n",  tagSig); //parent node is the tag type
    xml += line;

    //convert the rest of the tag to xml
    if (!pTagXml->ToXml(xml, "    ")) {
      printf("Unable to output tag with type %s\n", icGetSigStr(buf, bufSize, m_Array->GetType()));
      return false;
    }
    snprintf(line, bufSize, "  </%s>\n",  tagSig);
    xml += line;
  }

  xml += blanks + "</TintArrayElement>\n";

  return true;
}


bool CIccMpeXmlTintArray::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In TintArrayElement\n";
    return false;
  }
  m_nInputChannels = nInputChannels;
  m_nOutputChannels = nOutputChannels;

  for (pNode = pNode->children; pNode && pNode->type != XML_ELEMENT_NODE; pNode = pNode->next);
  if (pNode) {
    CIccTag *pTag = NULL;

    // get the tag signature
    std::string nodeName = (icChar*) pNode->name;
    icTagTypeSignature sigType = icGetTypeNameTagSig (nodeName.c_str());

    if (sigType==icSigUnknownType){
      xmlAttr *attr = icXmlFindAttr(pNode, "type");
      sigType = (icTagTypeSignature)icGetSigVal((icChar*) icXmlAttrValue(attr));
    }

    CIccInfo info;

    // create a tag based on the signature
    pTag = CIccTag::Create(sigType);

    if (!pTag || !pTag->IsNumArrayType()) {
      parseStr+= "Invalid data type for Tint Array!\n";
      delete pTag;
      return false;
    }

    IIccExtensionTag *pExt;

    if (pTag && (pExt = pTag->GetExtension()) && !strcmp(pExt->GetExtClassName(), "CIccTagXml")) {
      CIccTagXml* pXmlTag = (CIccTagXml*)pExt;
      xmlAttr *attr;

      if (pXmlTag->ParseXml(pNode->children, parseStr)) {
        if ((attr=icXmlFindAttr(pNode, "reserved"))) {
          sscanf(icXmlAttrValue(attr), "%u", &pTag->m_nReserved);
        }
        m_Array = (CIccTagNumArray*)pTag;
      }
      else {
        parseStr += "Unable to Parse \"";
        parseStr += info.GetTagTypeSigName(sigType);
        parseStr += "\" (";
        parseStr += nodeName;
        parseStr += ") Tint Tag\n";
        delete pTag;
        return false;
      }
    }
    else {
      parseStr += "Invalid tag extension for \"";
      parseStr += info.GetTagTypeSigName(sigType);
      parseStr += "\" (";
      parseStr += nodeName;
      parseStr += ") Tint Tag\n";
      if (pTag)
        delete pTag;
      return false;
    }
  }
  else {
    parseStr += "Tint Array missing tint values\n";
    return false;
  }
  return true;
}


CIccToneMapFunc* CIccXmlToneMapFunc::NewCopy() const
{
  CIccToneMapFunc* rv = new CIccXmlToneMapFunc();

  if (rv)
    *rv = *this;

  return rv;
}

bool CIccXmlToneMapFunc::ToXml(std::string& xml, std::string blanks /* = "" */)
{
  const size_t lineSize = 256;
  char line[lineSize];

  snprintf(line, lineSize, "<ToneMapFunction FunctionType=\"%d\"", m_nFunctionType);
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, lineSize, " Reserved=\"%d\"", m_nReserved);
    xml += line;
  }
  if (m_nReserved2) {
    snprintf(line, lineSize, " Reserved2=\"%d\"", m_nReserved2);
    xml += line;
  }
  xml += ">\n";

  CIccFloatArray::DumpArray(xml, blanks + "  ", m_params, m_nParameters, icConvertFloat, 8);

  xml += blanks + "</ToneMapFunction>\n";

  return true;

}

bool CIccXmlToneMapFunc::ParseXml(xmlNode* pNode, std::string& parseStr)
{
  xmlAttr* funcType = icXmlFindAttr(pNode, "FunctionType");

  if (!funcType) {
    parseStr += "Bad FunctionType in Tone Map Function\n";
    return false;
  }

  m_nReserved2 = atoi(icXmlAttrValue(pNode, "Reserved2"));
  m_nFunctionType = atoi(icXmlAttrValue(funcType));

  switch (m_nFunctionType) {
  case 0x0000:
    m_nParameters = 3;
    break;

  default:
    parseStr += "Unsupported FunctionType value in Tone Map Function\n";
    return false;
  }

  CIccFloatArray args;

  if (!args.ParseArray(pNode->children))
    return false;

  if (args.GetSize() < m_nParameters)
    return false;

  if (m_params) {
    free(m_params);
  }

  if (m_nParameters) {
    m_params = (icFloatNumber*)malloc(m_nParameters * sizeof(icFloatNumber));
    if (!m_params)
      return false;

    memcpy(m_params, args.GetBuf(), m_nParameters * sizeof(icFloatNumber));
  }
  else
    m_params = NULL;

  return true;
}


bool CIccMpeXmlToneMap::ToXml(std::string& xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize], line[bufSize];
  snprintf(buf, bufSize, "<ToneMapElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  if (m_pLumCurve) {
    xml += blanks + "  <LuminanceCurve>\n";
    if (!ToXmlCurve(xml, blanks + "  ", m_pLumCurve))
      return false;
    xml += blanks + "  </LuminanceCurve>\n";
  }

  if (m_pToneFuncs) {
    xml += blanks + "  <ToneMapFunctions>\n";
    int i, j;
    for (i = 0; i < NumOutputChannels(); i++) {
      if (m_pToneFuncs[i] && !strcmp(m_pToneFuncs[i]->GetClassName(), "CIccXmlToneMapFunc")) {
        CIccXmlToneMapFunc* pFunc = (CIccXmlToneMapFunc*)m_pToneFuncs[i];

        //check for duplicate curves
        for (j = 0; j < i; j++) {
          if (m_pToneFuncs[i] == m_pToneFuncs[j])
            break;
        }

        //handle case of duplicate curve
        if (j < i) {
          snprintf(line, bufSize, "<DuplicateFunction Index=\"%d\"/>\n", j);
          xml += blanks + "    ";
          xml += line;
        }
        else if (!pFunc->ToXml(xml, blanks + "    "))
          return false;
      }
      else
        return false;
    }
    xml += blanks + "  </ToneMapFunctions>\n";
  }

  xml += blanks + "</ToneMapElement>\n";

  return true;
}


bool CIccMpeXmlToneMap::ParseXml(xmlNode* pNode, std::string& parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  if (!nInputChannels || !nOutputChannels || nInputChannels!=nOutputChannels+1) {
    parseStr += "Invalid InputChannels or OutputChannels In ToneMapElement\n";
    return false;
  }
  m_nInputChannels = nInputChannels;

  SetNumOutputChannels(nOutputChannels);

  if (!m_pToneFuncs) {
    parseStr += "Unable to allocate ToneMapFunctions\n";
    return false;
  }
  
  xmlNode* pSubNode = icXmlFindNode(pNode->children, "LuminanceCurve");

  if (pSubNode) {
    xmlNode* pNode;
    for (pNode = pSubNode->children; pNode && pNode->type != XML_ELEMENT_NODE; pNode = pNode->next);
    m_pLumCurve = ParseXmlCurve(pNode, parseStr);
    if (!m_pLumCurve) {
      parseStr += "Unable to parse Luminance Curve\n";
      return false;
    }
  }
  else {
    parseStr += "Missing Luminance Curve";
    return false;
  }

  pSubNode = icXmlFindNode(pNode->children, "ToneMapFunctions");

  if (pSubNode) {
    xmlNode* pNode;
    int nIndex = 0;
    for (pNode = pSubNode->children, nIndex = 0;
      pNode;
      pNode = pNode->next) {
      if (pNode->type == XML_ELEMENT_NODE) {
        if (nIndex >= nOutputChannels) {
          parseStr += "Too many ToneFunctions";
          return false;
        }
        else if (!strcmp((const char*)pNode->name, "DuplicateFunction")) {
          const char* attr = icXmlAttrValue(pNode, "Index", NULL);

          if (attr) {
            int nCopyIndex = atoi(attr);
            if (nCopyIndex >= 0 && nCopyIndex < nIndex) {
              m_pToneFuncs[nIndex] = m_pToneFuncs[nCopyIndex];
              nIndex++;
            }
            else {
              parseStr += "Invalid index for duplicate ToneMapFunction\n";
              return false;
            }
          }
          else {
            parseStr += "Missing index for duplicate ToneMapFunction\n";
            return false;
          }
        }
        else if (!strcmp((const char*)pNode->name, "ToneMapFunction")) {
          CIccXmlToneMapFunc* pFunc = new CIccXmlToneMapFunc();

          if (!pFunc->ParseXml(pNode, parseStr)) {
            delete pFunc;
            return false;
          }
          if (!Insert(pFunc))
            return false;
          nIndex++;
        }
        else {
          parseStr += std::string("Unknown Tone Map Function '") + (const char*)pNode->name + "'\n";
          return false;
        }
      }
    }
    if (nIndex < m_nOutputChannels) {
      parseStr += "Missing ToneMap Functions\n";
      return false;
    }
  }

  return true;
}

extern CIccCLUT *icCLutFromXml(xmlNode *pNode, int nIn, int nOut, icConvertType nType, std::string &parseStr);

bool CIccMpeXmlCLUT::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char attrs[bufSize];

  if (m_nReserved) {
    snprintf(attrs, bufSize, " InputChannels=\"%d\" OutputChannels=\"%d\" Reserved=\"%u\"", NumInputChannels(), NumOutputChannels(), m_nReserved);
  }
  else {
    snprintf(attrs, bufSize, " InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  }

  return icCLUTToXml(xml, m_pCLUT, icConvertFloat, blanks, true, attrs, "CLutElement");
}

bool CIccMpeXmlCLUT::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In CLutElement\n";
    return false;
  }

  CIccCLUT *pCLut = icCLutFromXml(pNode, m_nInputChannels, m_nOutputChannels, icConvertFloat, parseStr);
  if (pCLut) {
    SetCLUT(pCLut);
    if (m_pCLUT)
      return true;
  }

  return false;
}

bool CIccMpeXmlExtCLUT::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char attrs[bufSize];
  std::string reserved;

  if (m_nReserved) {
    snprintf(attrs, bufSize, " Reserved=\"%u\"", m_nReserved);
    reserved = attrs;
  }

  if (m_nReserved2) {
    snprintf(attrs, bufSize, " Reserved2=\"%u\"", m_nReserved2);
    reserved += attrs;
  }

  snprintf(attrs, bufSize, " InputChannels=\"%d\" OutputChannels=\"%d\"%s StorageType=\"%d\"", NumInputChannels(), NumOutputChannels(), reserved.c_str(), m_storageType);

  return icCLUTToXml(xml, m_pCLUT, icConvertFloat, blanks, true, attrs, "ExtCLutElement");
}

bool CIccMpeXmlExtCLUT::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_storageType = (icUInt16Number)atoi(icXmlAttrValue(pNode, "StorageType", "0"));
  m_nReserved2 = atoi(icXmlAttrValue(pNode, "Reserved2", "0"));

  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In ExtCLutElement\n";
    return false;
  }

  CIccCLUT *pCLut = icCLutFromXml(pNode, m_nInputChannels, m_nOutputChannels, icConvertFloat, parseStr);
  if (pCLut) {
    SetCLUT(pCLut);
    if (m_pCLUT)
      return true;
  }

  return false;
}

bool CIccMpeXmlBAcs::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char line[bufSize];
  char buf[bufSize], fix[bufSize];

  snprintf(line, bufSize, "<BAcsElement InputChannels=\"%d\" OutputChannels=\"%d\" Signature=\"%s\"", NumInputChannels(), NumOutputChannels(),
                icFixXml(fix, icGetSigStr(buf, bufSize, m_signature)));
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }

  if (m_pData && m_nDataSize) {
    xml += ">\n";
  
    icXmlDumpHexData(xml, blanks+"  ", m_pData, m_nDataSize);

    xml += blanks + "</BAcsElement>\n";
  }
  else {
    xml += "/>\n";
  }

  return true;
}


bool CIccMpeXmlBAcs::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In BAcsElement\n";
    return false;
  }

  m_signature = icXmlStrToSig(icXmlAttrValue(pNode, "Signature"));

  if (pNode->children && pNode->children->type == XML_TEXT_NODE && pNode->children->content) {
    icUInt32Number nSize = icXmlGetHexDataSize((const char*)pNode->children->content);

    if (nSize) {
      if (!AllocData(nSize))
        return false;
      icXmlGetHexData(m_pData, (const char *)pNode->children->content, nSize);
    }
  }
  return true;
}


bool CIccMpeXmlEAcs::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char line[bufSize];
  char buf[bufSize], fix[bufSize];

  snprintf(line, bufSize, "<EAcsElement InputChannels=\"%d\" OutputChannels=\"%d\" Signature=\"%s\"", NumInputChannels(), NumOutputChannels(),
    icFixXml(fix, icGetSigStr(buf, bufSize, m_signature)));
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }

  if (m_pData && m_nDataSize) {
    xml += ">\n";
    icXmlDumpHexData(xml, blanks+"  ", m_pData, m_nDataSize);
    xml += blanks + "</EAcsElement>\n";
  }
  else {
    xml += "/>\n";
  }

  return true;
}


bool CIccMpeXmlEAcs::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In EAcsElement\n";
    return false;
  }

  m_signature = icXmlStrToSig(icXmlAttrValue(pNode, "Signature"));

  if (pNode->children && pNode->children->type == XML_TEXT_NODE && pNode->children->content) {
    icUInt32Number nSize = icXmlGetHexDataSize((const char*)pNode->children->content);

    if (nSize) {
      if (!AllocData(nSize))
        return false;
      icXmlGetHexData(m_pData, (const char *)pNode->children->content, nSize);
    }
  }
  return true;
}

static bool icXmlDumpColorAppearanceParams(std::string &xml, std::string blanks, CIccCamConverter *pCam)
{
  const size_t lineSize = 256;
  char line[lineSize];
  icFloatNumber xyz[3];

  xml += blanks + "<ColorAppearanceParams>\n";
  
  pCam->GetParameter_WhitePoint(&xyz[0]);
  snprintf(line, lineSize, "  <XYZNumber X=\"" icXmlFloatFmt "\" Y=\"" icXmlFloatFmt "\" Z=\"" icXmlFloatFmt "\"/>", xyz[0], xyz[1], xyz[2]);

  xml += blanks + " <WhitePoint>\n";
  xml += blanks + line;
  xml += blanks + " </WhitePoint>\n";
  
  snprintf(line, lineSize, " <Luminance>" icXmlFloatFmt "</Luminance>\n", pCam->GetParameter_La());
  xml += blanks + line;

  snprintf(line, lineSize, " <BackgroundLuminance>" icXmlFloatFmt "</BackgroundLuminance>\n", pCam->GetParameter_Yb());
  xml += blanks + line;

  snprintf(line, lineSize, " <ImpactSurround>" icXmlFloatFmt "</ImpactSurround>\n", pCam->GetParameter_C());
  xml += blanks + line;

  snprintf(line, lineSize, " <ChromaticInductionFactor>" icXmlFloatFmt "</ChromaticInductionFactor>\n", pCam->GetParameter_Nc());
  xml += blanks + line;

  snprintf(line, lineSize, " <AdaptationFactor>" icXmlFloatFmt "</AdaptationFactor>\n", pCam->GetParameter_F());
  xml += blanks + line;

  xml += "</ColorAppearanceParams>\n";

  return true;
}

static bool icXmlParseColorAppearanceParams(xmlNode *pNode, std::string &parseStr, CIccCamConverter *pCam)
{
   xmlNode *pChild = icXmlFindNode(pNode, "WhitePoint");
   if (!pChild) {
     parseStr += "Unable to find CAM Whitepoint\n";
     return false;
   }

   xmlNode *xyzNode = icXmlFindNode(pChild->children, "XYZNumber");

   if (!xyzNode) {
     parseStr += "Unable to find valid CAM WhitePoint XYZ\n";
     return false;
   }

   xmlAttr *x = icXmlFindAttr(xyzNode, "X");
   xmlAttr *y = icXmlFindAttr(xyzNode, "Y");
   xmlAttr *z = icXmlFindAttr(xyzNode, "Z");

   icFloatNumber xyz[3];
   if (x && y && z) {
     xyz[0] = (icFloatNumber)atof(icXmlAttrValue(x));
     xyz[1] = (icFloatNumber)atof(icXmlAttrValue(y));
     xyz[2] = (icFloatNumber)atof(icXmlAttrValue(z));
   }
   else {
     parseStr += "Invalid CAM WhitePoint XYZNumber\n";
     return false;
   }
   pCam->SetParameter_WhitePoint(&xyz[0]);

   pChild = icXmlFindNode(pNode, "Luminance");
   if (!pChild || !pChild->children || !pChild->children->content) {
     parseStr += "Invalid CAM Luminance\n";
     return false;
   }
   pCam->SetParameter_La((icFloatNumber)atof((const char*)pChild->children->content));

   pChild = icXmlFindNode(pNode, "BackgroundLuminance");
   if (!pChild || !pChild->children || !pChild->children->content) {
     parseStr += "Invalid CAM Luminance\n";
     return false;
   }
   pCam->SetParameter_Yb((icFloatNumber)atof((const char*)pChild->children->content));

   pChild = icXmlFindNode(pNode, "ImpactSurround");
   if (!pChild || !pChild->children || !pChild->children->content) {
     parseStr += "Invalid CAM ImpactSurround\n";
     return false;
   }
   pCam->SetParameter_C((icFloatNumber)atof((const char*)pChild->children->content));

   pChild = icXmlFindNode(pNode, "ChromaticInductionFactor");
   if (!pChild || !pChild->children || !pChild->children->content) {
     parseStr += "Invalid CAM ChromaticInductionFactor\n";
     return false;
   }
   pCam->SetParameter_Nc((icFloatNumber)atof((const char*)pChild->children->content));

   pChild = icXmlFindNode(pNode, "AdaptationFactor");
   if (!pChild || !pChild->children || !pChild->children->content) {
     parseStr += "Invalid CAM AdaptationFactor\n";
     return false;
   }
   pCam->SetParameter_F((icFloatNumber)atof((const char*)pChild->children->content));

   return true;
}

bool CIccMpeXmlJabToXYZ::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 256;
  char line[lineSize];

  snprintf(line, lineSize, "<JabToXYZElement InputChannels=\"%d\" OutputChannels=\"%d\"" , NumInputChannels(), NumOutputChannels());
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, lineSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }
  xml += ">\n";

  if (m_pCAM) {
    icXmlDumpColorAppearanceParams(xml, blanks+"  ", m_pCAM);
    xml += blanks + "</JabToXYZElement>\n";
  }

  return true;
}


bool CIccMpeXmlJabToXYZ::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (m_nInputChannels!=3 || m_nOutputChannels!=3) {
    parseStr += "Invalid InputChannels or OutputChannels In JabToXYZElement\n";
    return false;
  }
  xmlNode *pChild = icXmlFindNode(pNode->children, "ColorAppearanceParams");

  if (pChild) {
    CIccCamConverter *pCAM = new CIccCamConverter();

    if (!icXmlParseColorAppearanceParams(pChild->children, parseStr, pCAM)) {
      delete pCAM;
      return false;
    }

    SetCAM(pCAM);
  }
  else {
    parseStr += "Unable to find ColorAppearanceParams\n";
    return false;
  }

  return true;
}

bool CIccMpeXmlXYZToJab::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 256;
  char line[lineSize];

  snprintf(line, lineSize, "<XYZToJabElement InputChannels=\"%d\" OutputChannels=\"%d\"" , NumInputChannels(), NumOutputChannels());
    xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, lineSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }
  xml += ">\n";

  if (m_pCAM) {
    icXmlDumpColorAppearanceParams(xml, blanks+"  ", m_pCAM);
    xml += blanks + "</XYZToJabElement>\n";
  }

  return true;
}


bool CIccMpeXmlXYZToJab::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));

  if (m_nInputChannels!=3 || m_nOutputChannels!=3) {
    parseStr += "Invalid InputChannels or OutputChannels In XYZToJabElement\n";
    return false;
  }
  xmlNode *pChild = icXmlFindNode(pNode->children, "ColorAppearanceParams");

  if (pChild) {
    CIccCamConverter *pCAM = new CIccCamConverter();

    if (!icXmlParseColorAppearanceParams(pChild->children, parseStr, pCAM)) {
      delete pCAM;
      return false;
    }

    SetCAM(pCAM);
  }
  else {
    parseStr += "Unable to find ColorAppearanceParams\n";
    return false;
  }

  return true;
}


bool CIccMpeXmlCalculator::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t lineSize = 256;
  char line[lineSize];
  std::string blanks2 = blanks + "  ";

  snprintf(line, lineSize, "<CalculatorElement InputChannels=\"%d\" OutputChannels=\"%d\"", NumInputChannels(), NumOutputChannels());
  xml += blanks + line;

  if (m_nReserved) {
    snprintf(line, lineSize, " Reserved=\"%u\"", m_nReserved);
    xml += line;
  }
  xml += ">\n";

  int i;

  if (m_SubElem && m_nSubElem) {
    xml += blanks2 + "<SubElements>\n";
    for (i=0; i<(int)m_nSubElem; i++) {
      if (m_SubElem[i]) {
        IIccExtensionMpe *pExt = m_SubElem[i]->GetExtension();
        if (pExt && !strcmp(pExt->GetExtClassName(), "CIccMpeXml")) {
          CIccMpeXml *pMpe = (CIccMpeXml*)pExt;
          pMpe->ToXml(xml, blanks2+"  ");
        }
        else {
          return false;
        }
      }
      else {
        return false;
      }
    }
    xml += blanks2 + "</SubElements>\n";
  }

  if (m_calcFunc) {
    std::string desc;

    xml += blanks2 + "<MainFunction>\n";

    m_calcFunc->Describe(desc, 100, (int)blanks2.size()+2);
    xml += desc;

    xml+= blanks2 + "</MainFunction>\n";
  }

  xml += blanks + "</CalculatorElement>\n";
  return true;
}

bool CIccMpeXmlCalculator::validNameChar(char c, bool bFirst)
{
  if (bFirst && !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_')))
    return false;
  else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_')))
    return false;

  return true;
}

bool CIccMpeXmlCalculator::validName(const char *szName)
{
  const char *ptr;
  if (!szName || !*szName) {
    return false;
  }

  for (ptr = szName; *ptr; ptr++) {
    if (!validNameChar(*ptr, ptr == szName))
      return false;
  }

  return true;
}

bool CIccMpeXmlCalculator::ParseImport(xmlNode *pNode, std::string importPath, std::string &parseStr)
{
  xmlNode *pChild, *pNext;
  xmlAttr *attr;

  pChild = icXmlFindNode(pNode->children, "Imports");
  if (pChild) {
    for (pNext = pChild->children; pNext; pNext = pNext->next) {
      if (pNext->type == XML_ELEMENT_NODE) {
        if (!strcmp((icChar*)pNext->name, "Import")) {
          if ((attr = icXmlFindAttr(pNext, "Filename"))) {
            std::string file = icXmlAttrValue(attr);
            xmlDoc *doc = NULL;
            xmlNode *root_element = NULL;

            std::string look = "*";
            look += file;
            look += "*";

            if (strstr(importPath.c_str(), look.c_str())) {
              //Already imported this file
              continue;
            }

            /*parse the file and get the DOM */
            doc = xmlReadFile(file.c_str(), NULL, 0);

            if (doc == NULL) {
              parseStr += "Unable to import '";
              parseStr += file;
              parseStr += '\n';
              return false;
            }

            /*Get the root element node */
            root_element = xmlDocGetRootElement(doc);
            if (strcmp((icChar*)root_element->name, "IccCalcImport")) {
              parseStr += "Invalid calc element import file '" + file + "'\n";
              return false;
            }

            (void)ParseImport(root_element, importPath+file+"*", parseStr); // result unused, but is side effect needed?

            xmlFreeDoc(doc);
          }
          else {
            parseStr += "Missing import file specifier\n";
            return false;
          }
        }
        else {
          parseStr += "Invalid import specifier\n";
          return false;
        }
      }
    }
  }

  pChild = icXmlFindNode(pNode->children, "Variables");
  if (pChild) {
    for (pNext = pChild->children; pNext; pNext = pNext->next) {
      if (pNext->type == XML_ELEMENT_NODE) {
        if (!strcmp((icChar*)pNext->name, "Declare")) {
          if ((attr = icXmlFindAttr(pNext, "Name"))) {
            std::string name = icXmlAttrValue(attr);
            if (!validName(name.c_str())) {
              parseStr += "Invalid calc element variable name '" + name + "'\n'";
              return false;
            }

            TempDeclVarMap::iterator v = m_declVarMap.find(name);
            if (v != m_declVarMap.end()) {
              parseStr += "Calc element variable '" + name + "' was previously declared\n";
              return false;
            }
            int offset = -1;
            icUInt16Number size = 1;

            if ((attr = icXmlFindAttr(pNext, "Position"))) {
              offset = atoi(icXmlAttrValue(attr));
              if (offset && importPath != "*") {
                parseStr += "Position cannot be specified for imported variables";
                return false;
              }
            }
            if ((attr = icXmlFindAttr(pNext, "Size"))) {
              size = (icUInt16Number)atoi(icXmlAttrValue(attr));
            }
            if (size < 1) size = 1;

            CIccTempDeclVar var = CIccTempDeclVar(name, offset, size);
            if (pNext->children && pNext->children->content) {
              CIccFuncTokenizer parse((icChar*)pNext->children->content);
              offset = 0;

              while (parse.GetNext()) {
                icUInt16Number extra = 0;
                std::string member = parse.GetName();
                if (!validName(member.c_str())) {
                  parseStr += "Invalid member name '" + member + "' for calc element variable '" + name + "'\n";
                  return false;
                }
                size = 0; extra = 0;
                parse.GetIndex(size, extra, 1, 0);
                size++;
                if (size < 1) 
                  size = 1;

                var.m_members.push_back(CIccTempVar(member, offset, size));
                offset += size;
              }
              if (var.m_size < offset)
                var.m_size = offset;
            }
            m_declVarMap[name] = var;
          }
          else {
            parseStr += "Missing calc element variable name definition\n";
            return false;
          }
        }
        else {
          parseStr += "Invalid calc element variable declaration\n";
          return false;
        }
      }
    }
  }

  pChild = icXmlFindNode(pNode->children, "Macros");
  if (pChild) {
    for (pNext = pChild->children; pNext; pNext = pNext->next) {
      if (pNext->type == XML_ELEMENT_NODE) {
        if (!strcmp((icChar*)pNext->name, "Macro")) {
          if ((attr = icXmlFindAttr(pNext, "Name"))) {
            std::string name = icXmlAttrValue(attr);
            if (!validName(name.c_str())) {
              parseStr += "Invalid Macro name '" + name + "'\n'";
              return false;
            }
            if (!pNext->children || !pNext->children->content || !pNext->children->content[0]) {
              parseStr += "Missing content for macro '" + name + "'\n'";
              return false;
            }
            MacroMap::iterator m = m_macroMap.find(name);
            if (m != m_macroMap.end()) {
              if (!strcmp(m->second.c_str(), (icChar*)pNext->children->content))
                continue;
              parseStr += "Calc Element macro '" + name + "' was previously defined differently\n";
              return false;
            }
            m_macroMap[name] = (icChar*)(pNext->children->content);

            if ((attr = icXmlFindAttr(pNext, "Local"))) {
              icUInt16Number offset, size;
              CIccTempDeclVar var = CIccTempDeclVar(name, 0, 0);
              std::string locals = icXmlAttrValue(attr);
              CIccFuncTokenizer parse(locals.c_str());
              offset = 0;

              while (parse.GetNext()) {
                icUInt16Number extra = 0;
                std::string member = parse.GetName();
                if (!validName(member.c_str())) {
                  parseStr += "Invalid local name '" + member + "' for calc element macro '" + name + "'\n";
                  return false;
                }
                size = 0; extra = 0;
                parse.GetIndex(size, extra, 1, 0);
                size++;
                if (size < 1)
                  size = 1;

                var.m_members.push_back(CIccTempVar(member, offset, size));
                offset += size;
              }
              if (var.m_size < offset)
                var.m_size = offset;

              m_macroLocalMap[name] = var;
            }
          }
          else {
            parseStr += "Missing macro name\n";
            return false;
          }
        }
        else {
          parseStr += "Invalid macro declaration\n";
          return false;
        }
      }
    }
  }

  pChild = icXmlFindNode(pNode->children, "SubElements");
  if (pChild) {
    for (pNext = pChild->children; pNext; pNext = pNext->next) {
      if (pNext->type == XML_ELEMENT_NODE) {
        std::string name;

        if ((attr = icXmlFindAttr(pNext, "Name"))) {
          name = icXmlAttrValue(attr);
          if (!validName(name.c_str())) {
            parseStr += "Invalid SubElement name '" + name + "'\n'";
            return false;
          }
        }
        if (name.empty()) {
          if (importPath != "*") {
            parseStr += "Imported Calc SubElments must be named.";
            return false;
          }
        }
        else {
          MpePtrMap::iterator se = m_mpeMap.find(name);
          if (se != m_mpeMap.end()) {
            parseStr += "Duplicate declaration of SubElement '" + name + "'\n";
            return false;
          }
        }

        CIccMultiProcessElement *pMpe = CIccTagXmlMultiProcessElement::CreateElement((const icChar*)pNext->name);

        if (!pMpe) {
          parseStr += std::string("Unknown Sub-Element Type (") + (const icChar*)pNext->name + ")\n";
          return false;
        }

        if (!strcmp(pMpe->GetClassName(), "CIccMpeXmlCalculator")) {
          CIccMpeXmlCalculator *pSubCalc = (CIccMpeXmlCalculator*)pMpe;
          pSubCalc->m_sImport = importPath;
        }

        xmlAttr *attr;
        IIccExtensionMpe *pExt = pMpe->GetExtension();

        if (pExt) {
          if (!strcmp(pExt->GetExtClassName(), "CIccMpeXml")) {
            CIccMpeXml* pXmlMpe = (CIccMpeXml*)pExt;

            if (pXmlMpe->ParseXml(pNext, parseStr)) {
              if ((attr = icXmlFindAttr(pNode, "Reserved"))) {
                sscanf(icXmlAttrValue(attr), "%u", &pMpe->m_nReserved);
              }

              if (name.empty()) {
                m_mpeList.push_back(CIccMpePtr(pMpe, m_nNextMpe));
                m_nNextMpe++;
              }
              else {
                m_mpeMap[name] = CIccMpePtr(pMpe);
              }
            }
            else {
              const size_t strSize = 100;
              char str[strSize];
              snprintf(str, strSize, " starting on line %d", pNext->line);
              parseStr += std::string("Unable to parse element of type ") + pMpe->GetClassName() + str + "\n";
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
      }
    }
  }
  return true;
}

bool CIccMpeXmlCalculator::ValidMacroCalls(const char *szMacroText, std::string macroStack, std::string &parseStr) const
{
  const char *ptr;
  for (ptr = strstr(szMacroText, "call{"); ptr; ptr = strstr(ptr, "call{")) {
    CIccFuncTokenizer parse(ptr, true);
    parse.GetNext();

    std::string name = parse.GetReference();
    MacroMap::const_iterator m = m_macroMap.find(name);
    if (m == m_macroMap.end()) {
      parseStr += "Call to undefined macro '" + name + "'\n";
      return false;
    }
    std::string sm = "*";
    sm += name + "*";
    if (strstr(macroStack.c_str(), sm.c_str())) {
      parseStr += "Macro recursion detected in call to '" + name + "'\n";
      return false;
    }
    if (!ValidMacroCalls(m->second.c_str(), macroStack + name + "*", parseStr)) {
      return false;
    }
    ptr++;
  }
  return true;
}

bool CIccMpeXmlCalculator::ValidateMacroCalls(std::string &parseStr) const
{
  MacroMap::const_iterator m;

  for (m = m_macroMap.begin(); m != m_macroMap.end(); m++) {
    if (!ValidMacroCalls(m->second.c_str(), "*", parseStr)) {
      return false;
    }
  }
  return true;
}

bool CIccMpeXmlCalculator::Flatten(std::string &flatStr, std::string macroName, const char *szFunc, std::string &parseStr, icUInt32Number nLocalsOffset)
{
  CIccFuncTokenizer parse(szFunc, true);

  while (parse.GetNext()) {
    std::string token = parse.GetLast();
    const char *tok = token.c_str();
    if (!strncmp(tok, "call{", 5) ||
        tok[0] == '#') {
      std::string nameiter;
      std::string name;

      if (tok[0] == '#')
        nameiter = token.substr(1);
      else
        nameiter = parse.GetReference();

      const char *ptr;
      for (ptr = nameiter.c_str(); *ptr && *ptr != '[' && *ptr != '('; ptr++) name += *ptr;
      int iter = 1;
//       if (*ptr) {
//         iter = atoi(ptr + 1);
//         if (iter < 1)
//           iter = 1;
//         else if (iter>1024)
//           iter = 1024;
//       }

      MacroMap::iterator m = m_macroMap.find(name.c_str());
      if (m != m_macroMap.end()) {
        icUInt16Number nLocalsSize = 0;
        TempDeclVarMap::iterator locals = m_macroLocalMap.find(macroName);
        if (locals != m_macroLocalMap.end()) {
          nLocalsSize = locals->second.m_size;
        }
        
        int i;
        for (i = 0; i < iter; i++) {
          Flatten(flatStr, name, m->second.c_str(), parseStr, nLocalsOffset+nLocalsSize);
        }
      }
      else {
        parseStr += "Call to undefined macro '" + name + "'\n";
        return false;
      }
    }
    else if (!strncmp(tok, "in{", 3) ||
             !strncmp(tok, "out{", 4)) {
      std::string op = parse.GetName();
      std::string ref = parse.GetReference();
      std::string refroot;
      for (const char *ptr = ref.c_str(); *ptr && *ptr != ',' && *ptr != '(' && *ptr != '['; ptr++) refroot += *ptr;

      ChanVarMap *pMap;
      if (op == "in") {
        pMap = &m_inputMap;
      }
      else {
        pMap = &m_outputMap;
      }
      
      const size_t indexSize = 80;
      char index[indexSize];

      ChanVarMap::iterator ci = pMap->find(refroot);
      if (ci == pMap->end()) {
        parseStr += "Invalid '" + op + "' operation channel reference '" + refroot + "'\n";
        return false;
      }
      if (refroot != ref) {
        std::string select = ref.substr(refroot.size());
        int offset = 0;
        int size = 1;

        if (select[0] == '[' || select[1] == '(') {
          const char *ptr;
          offset = atoi(select.c_str() + 1);
          for (ptr = select.c_str() + 1; *ptr && *ptr != ')' && *ptr != ']'; ptr++);
          select = ptr + 1;
        }

        if (select[0]==',')
          size = atoi(select.c_str() + 1);

        if (size < 0 || offset<0 || ci->second.first + offset + size > m_nInputChannels) {
          parseStr += "Invalid '" + op + "' operation channel offset or size '" + refroot + "'\n";
          return false;
        }
        snprintf(index, indexSize, "(%d,%d)", ci->second.first+offset, size);
        flatStr += op + index + " ";
      }
      else if (ci->second.second>1) {
        snprintf(index, indexSize, "(%d,%d)", ci->second.first, ci->second.second);
        flatStr += op + index + " ";
      }
      else {
        snprintf(index, indexSize, "(%d)", ci->second.first);
        flatStr += op + index + " ";
      }
    }
    else if (!strncmp(tok, "tget{", 5) ||
             !strncmp(tok, "tput{", 5) ||
             !strncmp(tok, "tsav{", 5)) {
      std::string op = parse.GetName();
      std::string ref = parse.GetReference();
      std::string refroot;
      for (const char *ptr = ref.c_str(); *ptr && *ptr != '[' && *ptr != '('; ptr++) refroot += *ptr;

      if (macroName.size() && refroot.size() && refroot[0] == '@') {
        std::string localName = refroot.substr(1);
        TempDeclVarMap::iterator locals = m_macroLocalMap.find(macroName);
        if (locals == m_macroLocalMap.end()) {
          parseStr += "Reference to undeclared local variable '" + localName + "' in macro '" + macroName + "'\n";
          return false;
        }

        unsigned long nLocalOffset = 0;
        unsigned long nLocalSize = 0;
        TempVarList::iterator m = locals->second.m_members.begin();
        for (; m != locals->second.m_members.end(); m++) {
          if (localName == m->m_name) {
            nLocalOffset = m->m_pos;
            nLocalSize = m->m_size;
            break;
          }
        }
        if (m == locals->second.m_members.end()) {
          parseStr += "Reference to undeclared local variable '" + localName + "' in macro '" + macroName + "'\n";
          return false;
        }
        int voffset, vsize;

        if (ref != refroot) {
          CIccFuncTokenizer p2(ref.c_str());
          p2.GetNext();
          icUInt16Number _voffset = 0, _vsize = 1;
          p2.GetIndex(_voffset, _vsize, 0, 1);
          voffset = _voffset;
          vsize = _vsize + 1;
        }
        else {
          voffset = 0;
          vsize = (int)nLocalSize;
        }

        if (voffset + vsize > (int)nLocalSize) {
          parseStr += "Out of bounds indexing of local '" + refroot + "' in macro '" + macroName + "'\n";
          return false;
        }

        if (nLocalsOffset + nLocalOffset + voffset + vsize > 65536) {
          parseStr += "Temporary variable addressing out of bounds\n";
          return false;
        }
        const size_t idxSize = 80;
        char idx[idxSize];
        if (vsize == 1) {
          snprintf(idx, idxSize, "[%lu]", nLocalsOffset + nLocalOffset + voffset);
        }
        else {
          snprintf(idx, idxSize, "[%lu,%d]", nLocalsOffset + nLocalOffset + voffset, vsize);
        }
        flatStr += "l";
        flatStr += op.substr(1);
        flatStr += idx;
        flatStr += " ";
      }
      else {
        TempVarMap::iterator var = m_varMap.find(refroot);
        if (var == m_varMap.end()) {
          std::string root;
          for (const char *ptr = refroot.c_str(); *ptr && *ptr != '.'; ptr++) root += *ptr;

          TempVarMap::iterator rootVar = m_varMap.find(root);
          if (refroot != root && rootVar == m_varMap.end()) {
            parseStr += "Reference to undeclared variable '" + ref + "'\n";
            return false;
          }

          TempDeclVarMap::iterator decl = m_declVarMap.find(root);
          if (decl == m_declVarMap.end()) {
            parseStr += "Reference to undeclared variable '" + ref + "'\n";
            return false;
          }

          if (decl->second.m_pos < 0) {
            m_varMap[root] = CIccTempVar(root, m_nNextVar, decl->second.m_size);
            decl->second.m_pos = m_nNextVar;

            if (strchr(refroot.c_str(), '.')) {
              TempVarList::iterator m = decl->second.m_members.begin();
              for (; m != decl->second.m_members.end(); m++) {
                std::string vm = root + "." + m->m_name;
                m_varMap[vm] = CIccTempVar(vm, m_nNextVar + m->m_pos, m->m_size);
              }
            }

            if (m_nNextVar + decl->second.m_size > 65536) {
              parseStr += "Temporary variable addressing out of bounds\n";
              return false;
            }
            m_nNextVar += decl->second.m_size;
          }
          else {
            m_varMap[root] = CIccTempVar(root, decl->second.m_pos, decl->second.m_size);

            if (strchr(refroot.c_str(), '.')) {
              TempVarList::iterator m = decl->second.m_members.begin();
              for (; m != decl->second.m_members.end(); m++) {
                std::string vm = root + "." + m->m_name;
                m_varMap[vm] = CIccTempVar(vm, decl->second.m_pos + m->m_pos, m->m_size);
              }
            }
            if (decl->second.m_pos + decl->second.m_size > 65536) {
              parseStr += "Temporary variable addressing out of bounds\n";
              return false;
            }
          }

          var = m_varMap.find(refroot);
          if (var == m_varMap.end()) {
            parseStr += "Reference to undeclared variable '" + refroot + "'\n";
            return false;
          }
        }
        int voffset, vsize;

        if (ref != refroot) {
          CIccFuncTokenizer p2(ref.c_str());
          p2.GetNext();
          icUInt16Number _voffset = 0, _vsize = 1;
          p2.GetIndex(_voffset, _vsize, 0, 1);
          voffset = _voffset;
          vsize = _vsize + 1;
        }
        else {
          voffset = 0;
          vsize = var->second.m_size;
        }

        if (voffset + vsize > var->second.m_size) {
          parseStr += "Out of bounds indexing of '" + refroot + "'\n";
          return false;
        }
        if (var->second.m_pos + voffset + vsize > 65536) {
          parseStr += "Temporary variable addressing out of bounds\n";
          return false;
        }
        const size_t idxSize = 80;
        char idx[idxSize];
        if (vsize == 1) {
          snprintf(idx, idxSize, "[%d]", var->second.m_pos + voffset);
        }
        else {
          snprintf(idx, idxSize, "[%d,%d]", var->second.m_pos + voffset, vsize);
        }
        flatStr += op + idx + " ";
      }
    }
    else if (!strncmp(tok, "elem{", 5) ||
             !strncmp(tok, "curv{", 5) ||
             !strncmp(tok, "clut{", 5) ||
             !strncmp(tok, "mtx{", 4) ||
             !strncmp(tok, "fJab{", 5) ||
             !strncmp(tok, "tJab{", 5) ||
             !strncmp(tok, "calc{", 5) ||
             !strncmp(tok, "tint{", 5)) {
      std::string op = parse.GetName();
      std::string ref = parse.GetReference();
      MpePtrMap::iterator e = m_mpeMap.find(ref);
      if (e == m_mpeMap.end()) {
        parseStr += "Unknown sub element reference to " + token + "\n";
        return false;
      }
      if (e->second.m_nIndex<0) {
        if (e->second.m_ptr) {
          m_mpeList.push_back(CIccMpePtr(e->second.m_ptr, m_nNextMpe));
          e->second.m_ptr = NULL;
          e->second.m_nIndex = m_nNextMpe;
          m_nNextMpe++;
        }
        else {
          parseStr += "Invalid sub-element reference:  " + token + "\n";
          return false;
        }
      }
      const size_t idxSize = 80;
      char idx[idxSize];
      snprintf(idx, idxSize, "(%d)", e->second.m_nIndex);
      flatStr += op + idx + " ";
    }
    else {
      flatStr += tok;
      flatStr += " ";
    }
  }
 
  return true;
}

bool CIccMpeXmlCalculator::UpdateLocals(std::string &func, std::string sFunc, std::string &parseStr, int nLocalsOffset)
{
  func.clear();
  CIccFuncTokenizer parse(sFunc.c_str(), true);

  while (parse.GetNext()) {
    std::string token = parse.GetLast();
    const char *tok = token.c_str();

    if (!strncmp(tok, "lget[", 5) ||
        !strncmp(tok, "lput[", 5) ||
        !strncmp(tok, "lsav[", 5)) {
      std::string op = parse.GetName();
      int voffset, vsize;

      CIccFuncTokenizer p2(tok+4);
      icUInt16Number _voffset = 0, _vsize = 1;
      p2.GetIndex(_voffset, _vsize, 0, 1);
      voffset = _voffset + nLocalsOffset;
      vsize = _vsize + 1;

      if (voffset + vsize > 65535) {
        parseStr += "Local variable out of bounds - too many variables.\n";
        return false;
      }

      const size_t idxSize = 80;
      char idx[idxSize];
      if (vsize == 1) {
        snprintf(idx, idxSize, "[%d]", voffset);
      }
      else {
        snprintf(idx, idxSize, "[%d,%d]", voffset, vsize);
      }
      func +="t";
      func += op.substr(1);
      func += idx;
      func += " ";
    }
    else {
      func += tok;
      func += " ";
    }
  }

  return true;
}
void CIccMpeXmlCalculator::clean()
{
  m_sImport = "*";
  m_declVarMap.clear();
  m_varMap.clear();
  m_macroMap.clear();

  MpePtrList::iterator ml;
  for (ml = m_mpeList.begin(); ml != m_mpeList.end(); ml++) {
    if (ml->m_ptr)
      delete ml->m_ptr;
  }
  m_mpeList.clear();

  MpePtrMap::iterator mm;
  for (mm = m_mpeMap.begin(); mm != m_mpeMap.end(); mm++) {
    if (mm->second.m_ptr)
      delete mm->second.m_ptr;
  }
  m_mpeMap.clear();
  m_nNextVar = 0;
  m_nNextMpe = 0;
}

bool CIccMpeXmlCalculator::ParseChanMap(ChanVarMap& chanMap, const char *szNames, int nChannels)
{
  chanMap.clear();

  if (!szNames)
    return false;

  int i;
  const char *ptr;
  std::string name;
  IndexSizePair isp;
  int size = 1;

  for (i = 0, ptr = szNames; *ptr && i < nChannels; ptr++) {
    bool bFirst = name.empty();
    if (*ptr == ' ') {
      if (!bFirst) {
        isp.first = i;
        isp.second = size;
        chanMap[name] = isp;
        name.clear();
        i += size;
        size = 1;
      }
    }
    else if (*ptr == '[' || *ptr == '(') {
      size = atoi(ptr + 1);
      if (size<0 || i + size>nChannels)
        return 0;

      for (; *ptr && *ptr != ']' && *ptr != ')'; ptr++);
      if (!*ptr)
        ptr--;
    }
    else if (validNameChar(*ptr, bFirst)) {
      name += *ptr;
    }
    else {
      return false;
    }
  }

  if (!name.empty() && i < nChannels) {
    isp.first = i;
    isp.second = size;
    chanMap[name] = isp;
  }

  return true;
}

bool CIccMpeXmlCalculator::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  xmlNode *pChild;
  
  SetSize(atoi(icXmlAttrValue(pNode, "InputChannels")),
          atoi(icXmlAttrValue(pNode, "OutputChannels")));

  clean();

  if (!ParseChanMap(m_inputMap, icXmlAttrValue(pNode, "InputNames"), m_nInputChannels)) {
    parseStr += "Invalid name for InputChannels";
    return false;
  }

  if (!ParseChanMap(m_outputMap, icXmlAttrValue(pNode, "OutputNames"), m_nOutputChannels)) {
    parseStr += "Invalid name for InputChannels";
    return false;
  }

  if (!ParseImport(pNode, "*", parseStr))
    return false;

  if (!ValidateMacroCalls(parseStr)) {
    return false;
  }

  pChild = icXmlFindNode(pNode->children, "MainFunction");
  if (pChild && pNode->children && pChild->children->content) {
    char *content = (char*)pChild->children->content;
    std::string flatFunc;

    //Move new variables to after allocated variables
    TempDeclVarMap::iterator declVar;
    for (declVar = m_declVarMap.begin(); declVar != m_declVarMap.end(); declVar++) {
      if (declVar->second.m_pos >= 0) {
        int next = declVar->second.m_pos + declVar->second.m_size;
        if (next > m_nNextVar)
          m_nNextVar = next;
      }
    }

    if (!Flatten(flatFunc, "", content, parseStr, 0)) {
      return false;
    }

    if (m_macroLocalMap.size()) {
      std::string localFunc;
      if (!UpdateLocals(localFunc, flatFunc, parseStr, m_nNextVar)) {
        return false;
      }
      flatFunc = localFunc;
    }
    //copy MPE subelmeents used by Main Function to returned object
    int n;
    MpePtrList::iterator m;
    for (m = m_mpeList.begin(), n = 0; m != m_mpeList.end(); m++, n++) {
      this->SetSubElem(n, m->m_ptr);
      m->m_ptr = NULL;
    }

#if 0
    FILE *ff = fopen("flatfunc.txt", "wb");
    fwrite(flatFunc.c_str(), flatFunc.size(), 1, ff);
    fclose(ff);
#endif

    icFuncParseStatus stat = SetCalcFunc(flatFunc.c_str(), parseStr);
    if (stat!=icFuncParseNoError) {
      char buf[65];
      int len = icIntMin(64, (int)strlen(flatFunc.c_str()));
      strncpy(buf, flatFunc.c_str(), len);
      buf[len]=0;

      switch(stat) {
        case icFuncParseSyntaxError:
          parseStr += "Syntax Error occurred while parsing Main Calculator Function from \"";
          break;
        case icFuncParseInvalidOperation:
          parseStr += "Invalid Operation found while parsing Main Calculator Function from \"";
          break;
        case icFuncParseStackUnderflow:
          parseStr += "Stack underflow detected while parsing Main Calculator Function from \"";
          break;
        case icFuncParseInvalidChannel:
          parseStr += "Invalid Channel detected while parsing Main Calculator Function from \"";
          break;
        default:
          parseStr += "Unable to parse Main Calculator Function from \"";
      }
      parseStr += buf;
      parseStr += "\"\n";
      return false;
    }
  }
  clean();

  return true;
}


bool CIccMpeXmlEmissionCLUT::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  std::string reserved;

  xml += blanks + "<EmissionCLutElement";

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml +=  buf;
  }

  snprintf(buf, bufSize, " InputChannels=\"%d\" OutputChannels=\"%d\" Flags=\"%d\" StorageType=\"%d\">\n", NumInputChannels(), NumOutputChannels(), m_flags, m_nStorageType);
  xml += buf;

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += blanks + buf;

  int i;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }
  
  if (!icCLUTDataToXml(xml, m_pCLUT, icConvertFloat, blanks, true))
    return false;

  xml += blanks + "</EmissionCLutElement>\n";

  return true;
}

bool CIccMpeXmlEmissionCLUT::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nStorageType = (icUInt16Number)atoi(icXmlAttrValue(pNode, "StorageType", "0"));

  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  m_flags = atoi(icXmlAttrValue(pNode, "Flags", 0));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In CLutElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (dStart >= dEnd ||!nSteps) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }
  else {
    parseStr += "Missing Spectral Range\n";
    return false;
  }

  if (m_pWhite)
    free(m_pWhite);

  m_pWhite = (icFloatNumber *)malloc(m_Range.steps*sizeof(icFloatNumber));
  if (!m_pWhite) {
    parseStr += "White buffer memory error\n";
    return false;
  }
  
  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing White Data";
  }

  if (m_pCLUT) {
    delete m_pCLUT;
    m_pCLUT = NULL;
  }

  if (m_pApplyCLUT) {
    delete m_pApplyCLUT;
    m_pApplyCLUT = NULL;
  }

  CIccCLUT *pCLut = icCLutFromXml(pNode, m_nInputChannels, m_Range.steps, icConvertFloat, parseStr);
  if (pCLut) {
    m_pCLUT = pCLut;
    return true;
  }

  return false;
}

bool CIccMpeXmlReflectanceCLUT::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 256;
  char buf[bufSize];
  std::string reserved;

  xml += blanks + "<ReflectanceCLutElem";

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml +=  buf;
  }

  snprintf(buf, bufSize, " InputChannels=\"%d\" OutputChannels=\"%d\" Flags=\"%d\">\n", NumInputChannels(), NumOutputChannels(), m_flags);
  xml += buf;

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += buf;

  int i;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }

  if (!icCLUTDataToXml(xml, m_pCLUT, icConvertFloat, blanks, true))
    return false;

  xml += blanks + "</ReflectanceCLutElem>\n";

  return true;
}

bool CIccMpeXmlReflectanceCLUT::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  m_nStorageType = (icUInt16Number)atoi(icXmlAttrValue(pNode, "StorageType", "0"));

  m_nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  m_nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  m_flags = atoi(icXmlAttrValue(pNode, "Flags", 0));

  if (!m_nInputChannels || !m_nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In CLutElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (dStart >= dEnd ||!nSteps) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }
  else {
    parseStr += "Missing Spectral Range\n";
    return false;
  }

  if (m_pWhite)
    free(m_pWhite);

  m_pWhite = (icFloatNumber *)malloc(m_Range.steps*sizeof(icFloatNumber));
  if (!m_pWhite) {
    parseStr += "White buffer memory error\n";
    return false;
  }

  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing WhiteData";
  }

  if (m_pCLUT) {
    delete m_pCLUT;
    m_pCLUT = NULL;
  }

  if (m_pApplyCLUT) {
    delete m_pApplyCLUT;
    m_pApplyCLUT = NULL;
  }

  CIccCLUT *pCLut = icCLutFromXml(pNode, m_nInputChannels, m_Range.steps, icConvertFloat, parseStr);
  if (pCLut) {
    m_pCLUT = pCLut;
    return true;
  }

  return false;
}

bool CIccMpeXmlEmissionObserver::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  m_flags = atoi(icXmlAttrValue(pNode, "Flags"));

  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In EmissionObserverElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (dStart >= dEnd || nSteps != nInputChannels) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }

  SetSize(nInputChannels, nOutputChannels, m_Range);
  if (!m_pWhite) {
    parseStr += "Unable to SetSize\n";
    return false;
  }

  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing White Data";
  }

  return true;
}

bool CIccMpeXmlEmissionObserver::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  snprintf(buf, bufSize, "<EmissionObserverElement InputChannels=\"%d\" OutputChannels=\"%d\" Flags=\"%d\"", NumInputChannels(), NumOutputChannels(), m_flags);
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += buf;

  int i;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }

  xml += blanks + "</EmissionObserverElement>\n";

  return true;
}

bool CIccMpeXmlReflectanceObserver::ParseXml(xmlNode *pNode, std::string &parseStr)
{
  icUInt16Number nInputChannels = atoi(icXmlAttrValue(pNode, "InputChannels"));
  icUInt16Number nOutputChannels = atoi(icXmlAttrValue(pNode, "OutputChannels"));
  m_flags = atoi(icXmlAttrValue(pNode, "Flags"));

  if (!nInputChannels || !nOutputChannels) {
    parseStr += "Invalid InputChannels or OutputChannels In ReflectanceObserverElement\n";
    return false;
  }

  xmlNode *pData;

  pData = icXmlFindNode(pNode->children, "Wavelengths");
  if (pData) {
    icFloatNumber dStart = (icFloatNumber)atof(icXmlAttrValue(pData, "start"));
    icFloatNumber dEnd = (icFloatNumber)atof(icXmlAttrValue(pData, "end"));
    icUInt16Number nSteps = atoi(icXmlAttrValue(pData, "steps"));

    if (dStart >= dEnd || nSteps != nInputChannels) {
      parseStr += "Invalid Spectral Range\n";
      return false;
    }
    m_Range.start = icFtoF16(dStart);
    m_Range.end = icFtoF16(dEnd);
    m_Range.steps = nSteps;
  }

  SetSize(nInputChannels, nOutputChannels, m_Range);
  if (!m_pWhite) {
    parseStr += "Unable to SetSize\n";
    return false;
  }

  pData = icXmlFindNode(pNode->children, "WhiteData");
  if (pData) {

    if (!CIccFloatArray::ParseArray(m_pWhite, m_Range.steps, pData->children))
      return false;
  }
  else {
    parseStr += "Missing White Data";
  }

  return true;
}

bool CIccMpeXmlReflectanceObserver::ToXml(std::string &xml, std::string blanks/* = ""*/)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  snprintf(buf, bufSize, "<ReflectanceObserverElement InputChannels=\"%d\" OutputChannels=\"%d\" Flags=\"%d\"", NumInputChannels(), NumOutputChannels(), m_flags);
  xml += blanks + buf;

  if (m_nReserved) {
    snprintf(buf, bufSize, " Reserved=\"%u\"", m_nReserved);
    xml += buf;
  }
  xml += ">\n";

  snprintf(buf, bufSize, "  <Wavelengths start=\"" icXmlHalfFmt "\" end=\"" icXmlHalfFmt "\" steps=\"%d\"/>\n", icF16toF(m_Range.start), icF16toF(m_Range.end), m_Range.steps);
  xml += buf;

  int i;

  if (m_pWhite) {
    xml += blanks + "  <WhiteData>\n";

    xml += blanks + "   ";
    for (i=0; i<(int)m_Range.steps; i++) {
      snprintf(buf, bufSize, " " icXmlFloatFmt, m_pWhite[i]);
      xml += buf;
    }
    xml += "\n";

    xml += blanks + "  </WhiteData>\n";
  }

  xml += blanks + "</ReflectanceObserverElement>\n";

  return true;
}

#ifdef USEICCDEVNAMESPACE
}
#endif
