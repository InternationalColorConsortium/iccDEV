/** @file
    File:       IccTagBasic.cpp

    Contains:   Implementation of the CIccTag class and basic inherited classes

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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
  #include <windows.h>
#endif
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "IccTag.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include "IccTagFactory.h"
#include "IccConvertUTF.h"
#include "IccSparseMatrix.h"
#include "IccCmm.h"

#ifdef ICC_USE_ZLIB
#include "zlib.h"
#endif


#ifndef __min
#include <algorithm>
using std::min;
#define __min min
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

// For Describe() nVerboseness levels < 25, maximum length of strings
#define BRIEF_STRING_SIZE (1024)

/**
 ****************************************************************************
 * Name: CIccTag::CIccTag
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTag::CIccTag()
{
  m_nReserved = 0;
}

/**
 ****************************************************************************
 * Name: CIccTag::CIccTag
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTag::~CIccTag()
{

}

/**
 ****************************************************************************
 * Name: CIccTag::Create
 * 
 * Purpose: This is a static tag creator based upon tag signature type
 * 
 * Args: 
 *  sig = tag type signature
 * 
 * Return: Pointer to Allocated tag 
 *****************************************************************************
 */
CIccTag* CIccTag::Create(icTagTypeSignature sig)
{
  return CIccTagCreator::CreateTag(sig);
}


/**
 ******************************************************************************
 * Name: CIccTag::Validate
 * 
 * Purpose: Check tag data validity.  In base class we only look at the
 *  tag's reserved data value
 * 
 * Args: 
 *  sig = signature of tag being validated,
 *  sReport = String to add report information to
 * 
 * Return: 
 *  icValidateStatusOK if valid, or other error status.
 ******************************************************************************
 */
icValidateStatus CIccTag::Validate(std::string sigPath, std::string &sReport, const CIccProfile* /* pProfile =NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  
  if (m_nReserved != 0) {
    CIccInfo Info;
    sReport += icMsgValidateNonCompliant;
    sReport += Info.GetSigPathName(sigPath);
    sReport += " - Reserved Value must be zero.\n";

    rv = icValidateNonCompliant;
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagUnknown::CIccTagUnknown
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagUnknown::CIccTagUnknown()
{
  m_nType = icSigUnknownType;
  m_pData = NULL;
}

/**
 ****************************************************************************
 * Name: CIccTagUnknown::CIccTagUnknown
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITU = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccTagUnknown::CIccTagUnknown(const CIccTagUnknown &ITU)
{
  m_nSize = ITU.m_nSize;
  m_nType = ITU.m_nType;

  m_pData = new icUInt8Number[m_nSize];
  memcpy(m_pData, ITU.m_pData, sizeof(icUInt8Number)*m_nSize);
}

/**
 ****************************************************************************
 * Name: CIccTagUnknown::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  UnknownTag = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccTagUnknown &CIccTagUnknown::operator=(const CIccTagUnknown &UnknownTag)
{
  if (&UnknownTag == this)
    return *this;

  m_nSize = UnknownTag.m_nSize;
  m_nType = UnknownTag.m_nType;

  if (m_pData)
    delete [] m_pData;
  m_pData = new icUInt8Number[m_nSize];
  memcpy(m_pData, UnknownTag.m_pData, sizeof(icUInt8Number)*m_nSize);

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagUnknown::~CIccTagUnknown
 * 
 * Purpose: Destructor
 *****************************************************************************
 */
CIccTagUnknown::~CIccTagUnknown()
{
  if (m_pData)
    delete [] m_pData;
}


/**
 ****************************************************************************
 * Name: CIccTagUnknown::Read
 * 
 * Purpose: Read in an unknown tag type into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagUnknown::Read(icUInt32Number size, CIccIO *pIO)
{
  if (m_pData) {
    delete [] m_pData;
    m_pData = NULL;
  }

  if (size<sizeof(icTagTypeSignature) || !pIO) {
    return false;
  }

  if (!pIO->Read32(&m_nType))
    return false;

  m_nSize = size - sizeof(icTagTypeSignature);

  if (m_nSize > 0) { // size could be stored as smaller than expected value, therefore the size check

    m_pData = new icUInt8Number[m_nSize];

    if (pIO->Read8(m_pData, m_nSize) != m_nSize) {
      return false;
    }
  } else {
      return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagUnknown::Write
 * 
 * Purpose: Write an unknown tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagUnknown::Write(CIccIO *pIO)
{
  if (!pIO)
   return false;

  if (!pIO->Write32(&m_nType))
   return false;

  if (m_nSize && m_pData) {
   if (pIO->Write8(m_pData, m_nSize) != m_nSize)
     return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagUnknown::Describe
 * 
 * Purpose: Dump data associated with unknown tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagUnknown::Describe(std::string &sDescription, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  sDescription = "Unknown Tag Type of ";
  snprintf(buf, bufSize, "%u Bytes.", m_nSize-4);
  sDescription += buf;

  if (nVerboseness > 50) {
    sDescription += "\n\nData Follows:\n";

    icMemDump(sDescription, m_pData+4, m_nSize-4);
  }
}


/**
 ****************************************************************************
 * Name: CIccTagText::CIccTagText
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagText::CIccTagText()
{
  m_szText = (icChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
}

/**
 ****************************************************************************
 * Name: CIccTagText::CIccTagText
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITT = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagText::CIccTagText(const CIccTagText &ITT)
{
  m_szText = (icChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
  SetText(ITT.m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagText::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  TextTag = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagText &CIccTagText::operator=(const CIccTagText &TextTag)
{
  if (&TextTag == this)
    return *this;

  m_szText = (icChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
  SetText(TextTag.m_szText);

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagText::~CIccTagText
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagText::~CIccTagText()
{
  free(m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagText::Read
 * 
 * Purpose: Read in a text type tag into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagText::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  // Defensive: initialize so error paths all have valid m_szText
  m_szText[0] = '\0';

  if (size < (sizeof(icTagTypeSignature) + sizeof(icUInt32Number)) || !pIO)
    return false;

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  size_t nSize = size - sizeof(icTagTypeSignature) - sizeof(icUInt32Number);

  icChar *pBuf = GetBuffer((icUInt32Number)nSize+1);

  if (nSize) {
    if (pIO->Read8(pBuf, nSize) != nSize) {
      return false;
    }
  }
  
  // always NULL terminate the string
  m_szText[nSize] = '\0';

  Release();

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagText::Write
 * 
 * Purpose: Write a text type tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagText::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!m_szText)
    return false;

  size_t nSize = strlen(m_szText)+1;

  if (pIO->Write8(m_szText, nSize) != nSize)
    return false;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagText::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagText::Describe(std::string &sDescription, int nVerboseness)
{
  sDescription += "Text Length = ";
  if (m_szText && *m_szText) {
    const size_t bufSize = 40;
    char buf[bufSize];
    snprintf(buf, bufSize, "%zu bytes\n", strlen(m_szText));
    sDescription += buf;
  } else
    sDescription += "0 (NULL)";

  if (nVerboseness > 25) {
    sDescription += "\"";
    if (m_szText && *m_szText) {
      if ((nVerboseness > 50) || (strlen(m_szText) < BRIEF_STRING_SIZE)) {
        // output entire string if short or doing verbose
        sDescription += m_szText;  
      }
      else {
        // copy just first part of string to keep nVerboseness under control
        char buf[BRIEF_STRING_SIZE + 1];
        strncpy(buf, m_szText, BRIEF_STRING_SIZE);
        buf[BRIEF_STRING_SIZE] = '\0'; // ensure NULL termination
        sDescription += buf;
        sDescription += "\n... <!truncated!>";
      }
    }
    sDescription += "\"\n";
  }
}


/**
 ****************************************************************************
 * Name: CIccTagText::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagText::SetText(const icChar *szText)
{
  if (!szText) {
    SetText("");
    return;
  }

  icUInt32Number len=(icUInt32Number)strlen(szText) + 1;
  icChar *szBuf = GetBuffer(len);

  strcpy(szBuf, szText);
  Release();
}

/**
 ****************************************************************************
 * Name: *CIccTagText::operator=
 * 
 * Purpose: Define assignment operator to associate text with tag.
 * 
 * Args: 
 *  szText - zero terminated string to put in the tag
 *
 * Return: A pointer to the string assigned to the tag.
 *****************************************************************************
 */
const icChar *CIccTagText::operator=(const icChar *szText)
{
  SetText(szText);
  return m_szText;
}

/**
 ******************************************************************************
 * Name: CIccTagText::GetBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: The character buffer array
 *******************************************************************************
 */
icChar *CIccTagText::GetBuffer(icUInt32Number nSize)
{
  if (m_nBufSize < nSize) {
    m_szText = (icChar*)icRealloc(m_szText, nSize+1);

    if (m_szText) {
      m_szText[nSize] = '\0';
      m_nBufSize = nSize;
    }
    else
      m_nBufSize = 0;
  }

  return m_szText;
}

/**
 ****************************************************************************
 * Name: CIccTagText::Release
 * 
 * Purpose: This will resize the buffer to fit the zero terminated string in
 *  the buffer.
 *****************************************************************************
 */
void CIccTagText::Release()
{
  icUInt32Number nSize = (icUInt32Number)strlen(m_szText)+1;

  if (nSize < m_nBufSize-1) {
    m_szText=(icChar*)icRealloc(m_szText, nSize+1);
    m_nBufSize = nSize+1;
  }
}


/**
******************************************************************************
* Name: CIccTagText::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagText::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (m_nBufSize) {
    switch(sig) {
    case icSigCopyrightTag:
      break;
    case icSigCharTargetTag:
      if (m_nBufSize<7) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - Tag must have at least seven text characters.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
      break;
    default:
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Unknown Tag.\n";
      rv = icMaxStatus(rv, icValidateWarning);
    }
    int i;
    for (i=0; m_szText[i] && i<(int)m_nBufSize; i++) {
      if (m_szText[i]&0x80) {
        sReport += icMsgValidateWarning;
        sReport += sSigPathName;
        sReport += " - Text does not contain just 7-bit data.\n";
        rv = icMaxStatus(rv, icValidateWarning);
      }
    }
  }
  else {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty Tag.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }


  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::CIccTagUtf8Text
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagUtf8Text::CIccTagUtf8Text()
{
  m_szText = (icUChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::CIccTagUtf8Text
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITT = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagUtf8Text::CIccTagUtf8Text(const CIccTagUtf8Text &ITT)
{
  m_szText = (icUChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
  SetText(ITT.m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  TextTag = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagUtf8Text &CIccTagUtf8Text::operator=(const CIccTagUtf8Text &TextTag)
{
  if (&TextTag == this)
    return *this;

  m_szText = (icUChar*)malloc(1);
  m_szText[0] = '\0';
  m_nBufSize = 1;
  SetText(TextTag.m_szText);

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::~CIccTagUtf8Text
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagUtf8Text::~CIccTagUtf8Text()
{
  free(m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::Read
 * 
 * Purpose: Read in a text type tag into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagUtf8Text::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (size<sizeof(icTagTypeSignature) || !pIO) {
    m_szText[0] = '\0';
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (size < sizeof(icTagTypeSignature) + sizeof(icUInt32Number))
    return false;

  size_t nSize = size - sizeof(icTagTypeSignature) - sizeof(icUInt32Number);

  icUChar *pBuf = GetBuffer((icUInt32Number)nSize+1);

  if (nSize) {
    if (pIO->Read8(pBuf, nSize) != nSize) {
      return false;
    }
  }
  
  // always NULL terminate the string
  pBuf[nSize] = '\0';

  Release();

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::Write
 * 
 * Purpose: Write a text type tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagUtf8Text::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!m_szText)
    return false;

  size_t nSize = strlen((icChar*)m_szText)+1;

  if (pIO->Write8(m_szText, nSize) != nSize)
    return false;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagUtf8Text::Describe(std::string &sDescription, int nVerboseness)
{
  sDescription += "UTF8 Length = ";
  if (m_szText && *m_szText) {
    const size_t bufSize = 40;
    char buf[bufSize];
    snprintf(buf, bufSize, "%d bytes\n", m_nBufSize);
    sDescription += buf;
  }
  else
    sDescription += "0 (NULL)";

  if (nVerboseness > 25) {
    sDescription += "\"";
    if (m_szText && *m_szText) {
      if ((nVerboseness > 50) || (m_nBufSize < BRIEF_STRING_SIZE)) {
        // output entire string if short or doing verbose output
        sDescription += (icChar*)m_szText;  
      }
      else {
         // copy just first part of string to keep nVerboseness under control
         char buf[BRIEF_STRING_SIZE + 1];
         strncpy(buf, (icChar*)m_szText, BRIEF_STRING_SIZE);
         buf[BRIEF_STRING_SIZE] = '\0'; // ensure NULL termination
         sDescription += buf;
         sDescription += "\n... <!truncated!>";
      }
    }
    sDescription += "\"\n";
  }
}


/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagUtf8Text::SetText(const icUChar *szText)
{
  if (!szText) {
    SetText("");
    return;
  }

  icUInt32Number len=(icUInt32Number)strlen((icChar*)szText) + 1;
  icUChar *szBuf = GetBuffer(len);

  strcpy((icChar*)szBuf, (icChar*)szText);
  Release();
}


/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagUtf8Text::SetText(const icUChar16 *szText)
{
  if (!szText) {
    SetText("");
    return;
  }

  icUtf8Vector text;
  icUInt32Number len;
  for (len=0; szText[len]; len++);

  icConvertUTF16toUTF8(szText, &szText[len], text, lenientConversion);

  SetText((const icUChar*)&text[0]);
}

/**
 ****************************************************************************
 * Name: *CIccTagUtf8Text::operator=
 * 
 * Purpose: Define assignment operator to associate text with tag.
 * 
 * Args: 
 *  szText - zero terminated string to put in the tag
 *
 * Return: A pointer to the string assigned to the tag.
 *****************************************************************************
 */
const icUChar *CIccTagUtf8Text::operator=(const icUChar *szText)
{
  SetText(szText);
  return (icUChar*)m_szText;
}

/**
 ******************************************************************************
 * Name: CIccTagUtf8Text::GetBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: The character buffer array
 *******************************************************************************
 */
icUChar *CIccTagUtf8Text::GetBuffer(icUInt32Number nSize)
{
  if (m_nBufSize < nSize) {
    m_szText = (icUChar*)icRealloc(m_szText, nSize+1);

    if (m_szText) {
      m_szText[nSize] = '\0';
      m_nBufSize = nSize;
    }
    else {
      m_nBufSize = 0;
    }
  }

  return m_szText;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf8Text::Release
 * 
 * Purpose: This will resize the buffer to fit the zero terminated string in
 *  the buffer.
 *****************************************************************************
 */
void CIccTagUtf8Text::Release()
{
  icUInt32Number nSize = (icUInt32Number)strlen((icChar*)m_szText)+1;

  if (nSize < m_nBufSize-1) {
    m_szText=(icUChar*)icRealloc(m_szText, nSize+1);
    m_nBufSize = nSize+1;
  }
}


/**
******************************************************************************
* Name: CIccTagUtf8Text::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagUtf8Text::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_nBufSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty Tag.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }


  return rv;
}

#ifdef ICC_USE_ZLIB
static const unsigned char icFaultyXmlZipData[4] = { 0,0,0,1 };
#endif

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::CIccTagZipUtf8Text
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagZipUtf8Text::CIccTagZipUtf8Text()
{
  m_pZipBuf = NULL;
  m_nBufSize = 0;
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::CIccTagZipUtf8Text
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITT = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagZipUtf8Text::CIccTagZipUtf8Text(const CIccTagZipUtf8Text &ITT)
{
  m_pZipBuf = NULL;
  m_nBufSize = 0;
  AllocBuffer(ITT.BufferSize());
  if (m_pZipBuf) {
    memcpy(m_pZipBuf, ITT.GetBuffer(), m_nBufSize);
  }
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  TextTag = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagZipUtf8Text &CIccTagZipUtf8Text::operator=(const CIccTagZipUtf8Text &ITT)
{
  if (&ITT == this)
    return *this;

  AllocBuffer(ITT.BufferSize());
  if (m_pZipBuf) {
    memcpy(m_pZipBuf, ITT.GetBuffer(), m_nBufSize);
  }

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::~CIccTagZipUtf8Text
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagZipUtf8Text::~CIccTagZipUtf8Text()
{
  if (m_pZipBuf)
    free(m_pZipBuf);
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::Read
 * 
 * Purpose: Read in a text type tag into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagZipUtf8Text::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (size<sizeof(icTagTypeSignature) || !pIO) {
    AllocBuffer(0);
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nSize = size - sizeof(icTagTypeSignature) - sizeof(icUInt32Number);

  icUChar *pBuf = AllocBuffer(nSize);

  if (m_nBufSize && pBuf) {
    if (pIO->Read8(pBuf, m_nBufSize) != m_nBufSize) {
      return false;
    }
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::Write
 * 
 * Purpose: Write a text type tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagZipUtf8Text::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (m_pZipBuf) {
    if (pIO->Write8(m_pZipBuf, m_nBufSize) != m_nBufSize)
      return false;
  }
  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagZipUtf8Text::Describe(std::string &sDescription, int /* nVerboseness */)
{
  std::string str;
#ifdef ICC_USE_ZLIB
  if (!GetText(str)) {
    sDescription += "Error! - Unable to decompress text data.\n";
  }
  else if (m_nBufSize > 4 && !memcmp(m_pZipBuf, icFaultyXmlZipData, 4)) {
    sDescription += "Warning! - Correcting for improperly encoded ";
    sDescription += CIccTagCreator::GetTagTypeSigName(GetType());
    sDescription += " tag.\n\n";
  }

  sDescription += "ZLib Compressed String=\"";
  sDescription += str;
  sDescription += "\"\n";
#else
  const size_t sizeSize = 30;
  char size[sizeSize];
  snprintf(size, sizeSize, "%d", m_nBufSize);
  sDescription += "BEGIN_COMPESSED_DATA[\"";
  sDescription += size;
  sDescription += "]\n";
  if (m_nBufSize) {
    icMemDump(str, m_pZipBuf, m_nBufSize);
    sDescription += str;
    sDescription += "\n";
  }
  sDescription += "END_COMPRESSED_DATA\n";
#endif
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::GetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  text - string to put uncompressed text into
 *****************************************************************************
 */
bool CIccTagZipUtf8Text::GetText(std::string &str) const
{
#ifndef ICC_USE_ZLIB
  str="";
  return false;
#else
  int zstat;
  z_stream zstr;
  memset(&zstr, 0, sizeof(zstr));
  unsigned char buf[32767] = { 0 };

  zstat = inflateInit(&zstr);

  if (zstat != Z_OK) {
    return false;
  }

  zstat = inflateReset(&zstr);

  if (zstat != Z_OK) {
    return false;
  }

  if (m_nBufSize > 4 && !memcmp(m_pZipBuf, icFaultyXmlZipData, 4)) {
    //xrite creates invalid zipXMLType tags
    zstr.next_in = m_pZipBuf+4;
    zstr.avail_in = m_nBufSize-4;
  }
  else {
    zstr.next_in = m_pZipBuf;
    zstr.avail_in = m_nBufSize;
  }

  do {
    unsigned int i, n;

    zstr.next_out = buf;
    zstr.avail_out = sizeof(buf);

    zstat = inflate(&zstr, Z_SYNC_FLUSH);

    if (zstat != Z_OK && zstat != Z_STREAM_END) {
      inflateEnd(&zstr);
      return false;
    }

    n = sizeof(buf) - zstr.avail_out;

    for (i = 0; i < n; i++) {
      str += buf[i];
    }
  } while (zstat != Z_STREAM_END);

  inflateEnd(&zstr);

  return true;
#endif
}


/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
bool CIccTagZipUtf8Text::SetText(const icUChar *szText)
{
#ifndef ICC_USE_ZLIB
  (void)szText; // dummy to let the compiler know the value is really used below
  return false;
#else
  if (!szText)
    szText = (const icUChar*)"";

  icUInt32Number nSize = (icUInt32Number)strlen((const char*)szText) + 1;
  icUtf8Vector compress;
  int i;

  int zstat;
  z_stream zstr;
  unsigned char buf[32767];
  memset(&zstr, 0, sizeof(zstr));

  zstat = deflateInit(&zstr, Z_DEFAULT_COMPRESSION);

  if (zstat != Z_OK) {
    return false;
  }

  zstat = deflateReset(&zstr);
  zstr.next_in = (Bytef*)szText;
  zstr.avail_in = nSize;

  if (zstat != Z_OK) {
    return false;
  }


  do {
    int n;
    zstr.next_out = buf;
    zstr.avail_out = sizeof(buf);

    zstat = deflate(&zstr, Z_FINISH);

    if (zstat != Z_OK && zstat != Z_STREAM_END) {
      deflateEnd(&zstr);
      return false;
    }

    n = sizeof(buf) - zstr.avail_out;

    for (i = 0; i < n; i++) {
      compress.push_back(buf[i]);
    }
  } while (zstat != Z_STREAM_END);

  deflateEnd(&zstr);

  icUChar *pBuf = AllocBuffer((icUInt32Number)compress.size());

  if (pBuf) {
    for (i = 0; i < (int)m_nBufSize; i++) {
      pBuf[i] = compress[i];
}
  }

  return true;
#endif
}

/**
 ****************************************************************************
 * Name: CIccTagZipUtf8Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
bool CIccTagZipUtf8Text::SetText(const icUChar16 *szText)
{
  if (!szText) {
    return SetText("");
  }

  icUtf8Vector text;
  icUInt32Number len;
  for (len=0; szText[len]; len++);

  icConvertUTF16toUTF8(szText, &szText[len], text, lenientConversion);

  return SetText((const icUChar*)&text[0]);
}



/**
 ******************************************************************************
 * Name: CIccTagZipUtf8Text::AllocBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: The character buffer array
 *******************************************************************************
 */
icUChar *CIccTagZipUtf8Text::AllocBuffer(icUInt32Number nSize)
{
  if (m_nBufSize != nSize) {
    if (!nSize) {
      if (m_pZipBuf) {
        free(m_pZipBuf);
        m_pZipBuf = NULL;
      }

      m_nBufSize = nSize;
      return NULL;
    }

    if (!m_nBufSize)
      m_pZipBuf = (icUChar*)malloc(nSize);
    else
      m_pZipBuf = (icUChar*)icRealloc(m_pZipBuf, nSize);

    m_nBufSize = nSize;

  }

  return m_pZipBuf;
}


/**
******************************************************************************
* Name: CIccTagZipUtf8Text::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagZipUtf8Text::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_nBufSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty Tag.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }
  else {
#ifdef ICC_USE_ZLIB
    std::string sText;
    if (!GetText(sText)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Unable to get text for tag (possibly corrupt compressed data).\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    else if (m_nBufSize > 4 && !memcmp(m_pZipBuf, icFaultyXmlZipData, 4)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Improperly encoded ";
      sReport += CIccTagCreator::GetTagTypeSigName(GetType());
      sReport += " tag.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
#else
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Zip compression not supported by CMM - unable to validate text compression.\n";
    rv = icMaxStatus(rv, icValidateWarning);
#endif
  }
  
  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::CIccTagUtf16Text
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagUtf16Text::CIccTagUtf16Text()
{
  m_szText = (icUChar16*)malloc(1*sizeof(icUChar16));
  m_szText[0] = 0;
  m_nBufSize = 1;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::CIccTagUtf16Text
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITT = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagUtf16Text::CIccTagUtf16Text(const CIccTagUtf16Text &ITT)
{
  m_szText = (icUChar16*)malloc(1*sizeof(icUChar16));
  m_szText[0] = 0;
  m_nBufSize = 1;
  SetText(ITT.m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  TextTag = The CIccTagText object to be copied
 *****************************************************************************
 */
CIccTagUtf16Text &CIccTagUtf16Text::operator=(const CIccTagUtf16Text &TextTag)
{
  if (&TextTag == this)
    return *this;

  m_szText = (icUChar16*)malloc(1*sizeof(icUChar16));
  m_szText[0] = '\0';
  m_nBufSize = 1;
  SetText(TextTag.m_szText);

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::~CIccTagUtf16Text
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagUtf16Text::~CIccTagUtf16Text()
{
  free(m_szText);
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::GetLength
 * 
 * Purpose: Get length of utf16 string
 * 
 *****************************************************************************
 */
icUInt32Number CIccTagUtf16Text::GetLength() const
{
  icUInt32Number n;
  if (!m_szText || !m_nBufSize)
    return 0;

  for (n=0; n<m_nBufSize && m_szText[n]; n++);

  return n;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::Read
 * 
 * Purpose: Read in a text type tag into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagUtf16Text::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (size<sizeof(icTagTypeSignature) || !pIO) {
    m_szText[0] = '\0';
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  size_t nSize = (size - sizeof(icTagTypeSignature) - sizeof(icUInt32Number))/sizeof(icUChar16);

  icUChar16 *pBuf = GetBuffer((icUInt32Number)nSize+1);

  if (nSize) {
    if (pIO->Read16(pBuf, nSize) != nSize) {
      return false;
    }
  }
  
  // always NULL terminate the string
  pBuf[nSize] = 0;

  Release();

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::Write
 * 
 * Purpose: Write a text type tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagUtf16Text::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!m_szText)
    return false;

  size_t nSize = GetLength();

  if (nSize) {
    if (pIO->Write16(m_szText, nSize) != nSize)
      return false;
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagUtf16Text::Describe(std::string &sDescription, int nVerboseness)
{
    sDescription += "UTF16 Length = ";
    if (m_szText && *m_szText) {
        const size_t bufSize = 40;
        char buf[bufSize];
        snprintf(buf, bufSize, "%d bytes\n", m_nBufSize);
        sDescription += buf;
    }
    else
        sDescription += "0 (NULL)";

    if (nVerboseness > 25) {
        sDescription += "\"";
        if (m_szText && *m_szText) {
            std::string   s;
            if ((nVerboseness > 50) || (m_nBufSize < BRIEF_STRING_SIZE)) {
                // output entire string if short or doing verbose
                sDescription += GetText(s);
            }
            else {
                // copy just first part of string to keep nVerboseness under control
                char buf[BRIEF_STRING_SIZE + 1];
                strncpy(buf, GetText(s), BRIEF_STRING_SIZE);
                buf[BRIEF_STRING_SIZE] = '\0'; // ensure NULL termination
                sDescription += buf;
                sDescription += "\n... <!truncated!>";
            }
        }
        sDescription += "\"\n";
    }

}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::GetText
 * 
 * Purpose: Allows text data associated with the tag to be fetched.
 * 
 * Args: 
 *  buf - buffer to put tag data into. 
 * Returns: buf.c_str() 
 *****************************************************************************
 */
const icChar *CIccTagUtf16Text::GetText(std::string &buf) const
{
  icUtf8Vector str;

  icConvertUTF16toUTF8(m_szText, &m_szText[GetLength()], str, lenientConversion);

  buf.clear();
  icUtf8Vector::iterator c;
  c=str.begin();
  if (str.size()>2 && (str[0]==0xff && str[1]==0xfe)) {
    c++; c++;
  }
  for (; c!=str.end(); c++)
    buf.push_back(*c);

  return buf.c_str();
}


/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagUtf16Text::SetText(const icUChar16 *szText)
{
  if (!szText) {
    icUChar16 c=0;
    SetText(&c);
    return;
  }

  icUInt32Number n;
  for (n=0; szText[n]; n++);

  icUInt32Number len=n + 1;
  icUChar16 *szBuf = GetBuffer(len);

  memcpy(szBuf, szText, len*sizeof(icUChar16));
  Release();
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagUtf16Text::SetText(const icUChar *szText)
{
  if (!szText) {
    icUChar16 c=0;
    SetText(&c);
    return;
  }

  icUtf16Vector str;
  icConvertUTF8toUTF16(szText, szText+strlen((icChar*)szText)+1, str, lenientConversion);

  int pos = 0;
  if (str[0]==0xfeff) {
    pos = 1;
  }

  icUInt32Number nSize = (icUInt32Number)(str.size()-pos);
  //icUChar16 *szBuf = GetBuffer(nSize);      // ERROR - value unused! is the memcpy correct?

  if (nSize)
    memcpy(m_szText, &str[pos], nSize*sizeof(icUChar));
  Release();
}

/**
 ****************************************************************************
 * Name: *CIccTagUtf16Text::operator=
 * 
 * Purpose: Define assignment operator to associate text with tag.
 * 
 * Args: 
 *  szText - zero terminated string to put in the tag
 *
 * Return: A pointer to the string assigned to the tag.
 *****************************************************************************
 */
const icUChar16 *CIccTagUtf16Text::operator=(const icUChar16 *szText)
{
  SetText(szText);
  return (icUChar16*)m_szText;
}

/**
 ******************************************************************************
 * Name: CIccTagUtf16Text::GetBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: The utf16 character buffer array
 *******************************************************************************
 */
icUChar16 *CIccTagUtf16Text::GetBuffer(icUInt32Number nSize)
{
  if (m_nBufSize < nSize) {
    m_szText = (icUChar16*)icRealloc(m_szText, (nSize+1)*sizeof(icUChar16));

    m_szText[nSize] = 0;

    m_nBufSize = nSize;
  }

  return m_szText;
}

/**
 ****************************************************************************
 * Name: CIccTagUtf16Text::Release
 * 
 * Purpose: This will resize the buffer to fit the zero terminated string in
 *  the buffer.
 *****************************************************************************
 */
void CIccTagUtf16Text::Release()
{
  icUInt32Number nSize = GetLength()+1;

  if (nSize < m_nBufSize-1) {
    m_szText=(icUChar16*)icRealloc(m_szText, (nSize+1)*sizeof(icUChar16));
    m_nBufSize = nSize+1;
  }
}


/**
******************************************************************************
* Name: CIccTagUtf16Text::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagUtf16Text::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_nBufSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty Tag.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }

  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagTextDescription::CIccTagTextDescription
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagTextDescription::CIccTagTextDescription()
{
  m_szText = (icChar*)malloc(1);
  m_szText[0] = '\0';
  m_nASCIISize = 1;

  m_uzUnicodeText = (icUInt16Number*)malloc(sizeof(icUInt16Number));
  m_uzUnicodeText[0] = 0;
  m_nUnicodeSize = 1;
  m_nUnicodeLanguageCode = 0;

  m_nScriptSize = 0;
  m_nScriptCode = 0;
  memset(m_szScriptText, 0, sizeof(m_szScriptText));

  m_bInvalidScript = false;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::CIccTagTextDescription
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITTD = The CIccTagTextDescription object to be copied
 *****************************************************************************
 */
CIccTagTextDescription::CIccTagTextDescription(const CIccTagTextDescription &ITTD)
{
  m_nASCIISize = ITTD.m_nASCIISize;
  m_nUnicodeSize = ITTD.m_nUnicodeSize;
  m_nUnicodeLanguageCode = ITTD.m_nUnicodeLanguageCode;
  m_nScriptSize = ITTD.m_nScriptSize;
  m_nScriptCode = ITTD.m_nScriptCode;

  if (m_nASCIISize) {
    m_szText = (icChar*)malloc(m_nASCIISize * sizeof(icChar));
    memcpy(m_szText, ITTD.m_szText, m_nASCIISize*sizeof(icChar));
  }
  else {
    m_nASCIISize = 1;
    m_szText = (icChar*)calloc(m_nASCIISize, sizeof(icChar));
    m_szText[0] = '\0';
  }

  if (m_nUnicodeSize) {
    m_uzUnicodeText = (icUInt16Number*)malloc((m_nUnicodeSize) * sizeof(icUInt16Number));
    memcpy(m_uzUnicodeText, ITTD.m_uzUnicodeText, m_nUnicodeSize*sizeof(icUInt16Number));
  }
  else {
    m_nUnicodeSize = 1;
    m_uzUnicodeText = (icUInt16Number*)calloc(m_nUnicodeSize, sizeof(icUInt16Number));
    m_uzUnicodeText[0] = 0;
  }

  memcpy(m_szScriptText, ITTD.m_szScriptText, sizeof(m_szScriptText));

  m_bInvalidScript = ITTD.m_bInvalidScript;
}


/**
 ****************************************************************************
 * Name: CIccTagTextDescription::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  TextDescTag = The CIccTagTextDescription object to be copied
 *****************************************************************************
 */
CIccTagTextDescription &CIccTagTextDescription::operator=(const CIccTagTextDescription& TextDescTag)
{
  if (&TextDescTag == this)
    return *this;

  m_nASCIISize = TextDescTag.m_nASCIISize;
  m_nUnicodeSize = TextDescTag.m_nUnicodeSize;
  m_nUnicodeLanguageCode = TextDescTag.m_nUnicodeLanguageCode;
  m_nScriptSize = TextDescTag.m_nScriptSize;
  m_nScriptCode = TextDescTag.m_nScriptCode;

  if (m_szText)
    free(m_szText);
  if (m_nASCIISize) {
    m_szText = (icChar*)calloc(m_nASCIISize, sizeof(icChar));
    memcpy(m_szText, TextDescTag.m_szText, m_nASCIISize*sizeof(icChar));
  } 
  else {
    m_nASCIISize = 1;
    m_szText = (icChar*)calloc(m_nASCIISize, sizeof(icChar));
    m_szText[0] = '\0';
  }

  if (m_uzUnicodeText)
    free(m_uzUnicodeText);
  if (m_nUnicodeSize) {
    m_uzUnicodeText = (icUInt16Number*)calloc(m_nUnicodeSize, sizeof(icUInt16Number));
    memcpy(m_uzUnicodeText, TextDescTag.m_uzUnicodeText, m_nUnicodeSize*sizeof(icUInt16Number));
  }
  else {
    m_nUnicodeSize = 1;
    m_uzUnicodeText = (icUInt16Number*)calloc(m_nUnicodeSize, sizeof(icUInt16Number));
    m_uzUnicodeText[0] = 0;
  }

  memcpy(m_szScriptText, TextDescTag.m_szScriptText, sizeof(m_szScriptText));

  m_bInvalidScript = TextDescTag.m_bInvalidScript;

  return *this;  
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::~CIccTagTextDescription
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagTextDescription::~CIccTagTextDescription()
{
  free(m_szText);
  free(m_uzUnicodeText);
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagTextDescription::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  m_szText[0] = '\0';
  int64_t nEnd = pIO->Tell() + size;

  if (size<3*sizeof(icUInt32Number) || !pIO)
    return false;

  icUInt32Number nSize;

  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read32(&nSize))
    return false;

  if (3*sizeof(icUInt32Number) + nSize > size)
    return false;

  icChar *pBuf = GetBuffer(nSize+1);

  if (nSize) {
    if (pIO->Read8(pBuf, nSize) != nSize) {
      return false;
    }
  }

  // always NULL terminate the string!
  m_szText[nSize] = '\0';
  
  Release();

  if ( (pIO->Tell() + int64_t(2 * sizeof(icUInt32Number))) > nEnd)
    return false;

  if (!pIO->Read32(&m_nUnicodeLanguageCode) ||
      !pIO->Read32(&nSize))
    return false;

  // Calculations in GetUnicodeBuffer() can cause wrap-around error
  if (nSize == 0xFFFFFFFF) 
    return false;

  icUInt16Number *pBuf16 = GetUnicodeBuffer(nSize);

  if (nSize) {
    if (pIO->Read16(pBuf16, nSize) != nSize) {
      return false;
    }
  }
  else 
    pBuf16[0] = 0;

  ReleaseUnicode();

  if (pIO->Tell()+3 > nEnd)
    return false;

  if (!pIO->Read16(&m_nScriptCode) ||
      !pIO->Read8(&m_nScriptSize))
     return false;
  
  if (pIO->Tell() + m_nScriptSize> nEnd ||
      m_nScriptSize > sizeof(m_szScriptText))
    return false;

  size_t nScriptLen = pIO->Read8(m_szScriptText, 67);

  if (!nScriptLen)
    return false;

  if (nScriptLen<67) {
    memset(&m_szScriptText[0], 0, 67-nScriptLen);
    m_bInvalidScript = true;
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagTextDescription::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt32Number zero = 0;

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write32(&m_nASCIISize))
    return false;

  if (m_nASCIISize) {
    if (pIO->Write8(m_szText, m_nASCIISize) != m_nASCIISize)
      return false;
  }

  if (!pIO->Write32(&m_nUnicodeLanguageCode))
    return false;

  if (m_nUnicodeSize > 1) {
    if (!pIO->Write32(&m_nUnicodeSize) ||
        pIO->Write16(m_uzUnicodeText, m_nUnicodeSize) != m_nUnicodeSize)
      return false;
  }
  else {
    if (!pIO->Write32(&zero))
      return false;
  }

  if (!pIO->Write16(&m_nScriptCode) ||
      !pIO->Write8(&m_nScriptSize) ||
      pIO->Write8(m_szScriptText, 67)!= 67)
    return false;

  m_bInvalidScript = false;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagTextDescription::Describe(std::string &sDescription, int nVerboseness)
{
  sDescription += "TextDescription Length = ";
  if (m_szText && *m_szText) {
    const size_t bufSize = 40;
    char buf[bufSize];
    snprintf(buf, bufSize, "%zu bytes\n", strlen(m_szText));
    sDescription += buf;
  }
  else
    sDescription += "0 (NULL)";

  if (nVerboseness > 25) {
    sDescription += "\"";
    if (m_szText && *m_szText) {
      if ((nVerboseness > 50) || (strlen(m_szText) < BRIEF_STRING_SIZE)) {
        // output entire string if short or doing verbose
         sDescription += m_szText;  
      }
      else {
        // copy just first part of string to keep nVerboseness under control
        char buf[BRIEF_STRING_SIZE+1];
        strncpy(buf, m_szText, BRIEF_STRING_SIZE);
        buf[BRIEF_STRING_SIZE] = '\0'; // ensure NULL termination
        sDescription += buf;
        sDescription += "\n... <!truncated!>";
      } 
    }
    sDescription += "\"\n";
  }
}


/**
 ****************************************************************************
 * Name: CIccTagTextDescription::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText - zero terminated string to put in tag
 *****************************************************************************
 */
void CIccTagTextDescription::SetText(const icChar *szText)
{
  m_bInvalidScript = false;

  if (!szText) {
    SetText("");
    return;
  }

  icUInt32Number len=(icUInt32Number)strlen(szText) + 1;
  icChar *szBuf = GetBuffer(len);

  strcpy(szBuf, szText);
  Release();
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::operator=
 * 
 * Purpose: Define assignment operator to associate text with tag.
 * 
 * Args: 
 *  szText - zero terminated string to put in the tag
 *
 * Return: A pointer to the string assigned to the tag.
 *****************************************************************************
 */
const icChar *CIccTagTextDescription::operator=(const icChar *szText)
{
  SetText(szText);
  return m_szText;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::GetBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: 
 *****************************************************************************
 */
icChar *CIccTagTextDescription::GetBuffer(icUInt32Number nSize)
{
  if (m_nASCIISize < nSize) {
    m_szText = (icChar*)icRealloc(m_szText, nSize+1);

    m_szText[nSize] = '\0';

    m_nASCIISize = nSize;
  }

  return m_szText;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::Release
 * 
 * Purpose: This will resize the buffer to fit the zero terminated string in
 *  the buffer.
 *****************************************************************************
 */
void CIccTagTextDescription::Release()
{
  icUInt32Number nSize = (icUInt32Number)strlen(m_szText);

  if (nSize < m_nASCIISize-1) {
    m_szText=(icChar*)icRealloc(m_szText, nSize+1);
    m_nASCIISize = nSize+1;
  }
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::GetUnicodeBuffer
 * 
 * Purpose: This function allocates room and returns pointer to data buffer to
 *  put string into
 * 
 * Args: 
 *  nSize = Requested size of data buffer.
 * 
 * Return: 
 *****************************************************************************
 */
icUInt16Number *CIccTagTextDescription::GetUnicodeBuffer(icUInt32Number nSize)
{
  if (m_nUnicodeSize < (nSize+2)) { // test for existing size must include the NULL termination!
    m_uzUnicodeText = (icUInt16Number*)icRealloc(m_uzUnicodeText, (nSize+2)*sizeof(icUInt16Number));

    m_uzUnicodeText[nSize] = 0;
    m_uzUnicodeText[nSize+1] = 0;

    m_nUnicodeSize = nSize+2;
  }

  return m_uzUnicodeText;
}

/**
 ****************************************************************************
 * Name: CIccTagTextDescription::ReleaseUnicode
 * 
 * Purpose: This will resize the buffer to fit the zero terminated string in
 *  the buffer.
 *****************************************************************************
 */
void CIccTagTextDescription::ReleaseUnicode()
{
  int i;
  // even if the string is not NULL terminated, don't read over the end!
  for (i=0; i < (int)m_nUnicodeSize && m_uzUnicodeText[i]; i++);

  icUInt32Number nSize = i+1;

  // try to keep 2 NULLs at the end, to deal with malformed unicode
  // but don't reallocate because of the NULLs
  if (nSize < (m_nUnicodeSize-2)) {
    m_uzUnicodeText=(icUInt16Number*)icRealloc(m_uzUnicodeText, (nSize+2)*sizeof(icUInt16Number));
    m_uzUnicodeText[nSize] = 0;
    m_uzUnicodeText[nSize+1] = 0;
    m_nUnicodeSize = nSize+2;
  }
}


/**
******************************************************************************
* Name: CIccTagTextDescription::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagTextDescription::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (m_nScriptSize>67) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - ScriptCode count must not be greater than 67.\n";

    rv =icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_bInvalidScript) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - ScriptCode must contain 67 bytes.\n";

    rv =icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}

/**
 ****************************************************************************
 * Name: CIccTagSignature::CIccTagSignature
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagSignature::CIccTagSignature()
{
  m_nSig = 0x3f3f3f3f; //'????';
}



/**
 ****************************************************************************
 * Name: CIccTagSignature::CIccTagSignature
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITS = The CIccTagSignature object to be copied
 *****************************************************************************
 */
CIccTagSignature::CIccTagSignature(const CIccTagSignature &ITS)
{
  m_nSig = ITS.m_nSig;
}



/**
 ****************************************************************************
 * Name: CIccTagSignature::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  SignatureTag = The CIccTagSignature object to be copied
 *****************************************************************************
 */
CIccTagSignature &CIccTagSignature::operator=(const CIccTagSignature &SignatureTag)
{
  if (&SignatureTag == this)
    return *this;

  m_nSig = SignatureTag.m_nSig;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagSignature::~CIccTagSignature
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagSignature::~CIccTagSignature()
{
}

/**
 ****************************************************************************
 * Name: CIccTagSignature::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagSignature::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 2*sizeof(icUInt32Number) > size)
    return false;

  if (!pIO) {
    m_nSig = 0x3f3f3f3f; //'????';
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nSig))
    return false;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagSignature::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagSignature::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nSig))
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagSignature::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagSignature::Describe(std::string &sDescription, int /* nVerboseness */)
{
  CIccInfo Fmt;

  sDescription += Fmt.GetSigName(m_nSig);
  sDescription += "\n";
}


/**
******************************************************************************
* Name: CIccTagSignature::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagSignature::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);
  const size_t bufSize = 128;
  char buf[bufSize];

  if (sig==icSigTechnologyTag) {
    switch(m_nSig) {
    case icSigFilmScanner:
    case icSigDigitalCamera:
    case icSigReflectiveScanner:
    case icSigInkJetPrinter:
    case icSigThermalWaxPrinter:
    case icSigElectrophotographicPrinter:
    case icSigElectrostaticPrinter:
    case icSigDyeSublimationPrinter:
    case icSigPhotographicPaperPrinter:
    case icSigFilmWriter:
    case icSigVideoMonitor:
    case icSigVideoCamera:
    case icSigProjectionTelevision:
    case icSigCRTDisplay:
    case icSigPMDisplay:
    case icSigAMDisplay:
    case icSigPhotoCD:
    case icSigPhotoImageSetter:
    case icSigGravure:
    case icSigOffsetLithography:
    case icSigSilkscreen:
    case icSigFlexography:
    case icSigMotionPictureFilmScanner:
    case icSigMotionPictureFilmRecorder:
    case icSigDigitalMotionPictureCamera:
    case icSigDigitalCinemaProjector:
      break;

    default:
      {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        snprintf(buf, bufSize, " - %s: Unknown Technology.\n", Info.GetSigName(m_nSig));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
  }
  else if (sig==icSigPerceptualRenderingIntentGamutTag ||
           sig==icSigSaturationRenderingIntentGamutTag) {
    switch(m_nSig) {
    case icSigPerceptualReferenceMediumGamut:
      break;

    default:
      {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        snprintf(buf, bufSize, " - %s: Unknown Reference Medium Gamut.\n", Info.GetSigName(m_nSig));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
  }
  else if (sig==icSigColorimetricIntentImageStateTag) {
    switch(m_nSig) {
      case icSigSceneColorimetryEstimates:
      case icSigSceneAppearanceEstimates:
      case icSigFocalPlaneColorimetryEstimates:
      case icSigReflectionHardcopyOriginalColorimetry:
      case icSigReflectionPrintOutputColorimetry:
        break;

      default:
      {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        snprintf(buf, bufSize, " - %s: Unknown Colorimetric Intent Image State.\n", Info.GetSigName(m_nSig));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
  }


  return rv;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::CIccTagNamedColor2
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nSize = number of named color entries,
 *  nDeviceCoords = number of device channels
 *****************************************************************************
 */
CIccTagNamedColor2::CIccTagNamedColor2(int nSize/*=1*/, int nDeviceCoords/*=0*/)
{
  m_nSize = nSize <1 ? 1 : nSize;
  m_nVendorFlags = 0;
  m_nDeviceCoords = nDeviceCoords;
  if (nDeviceCoords<0)
    m_nDeviceCoords = nDeviceCoords = 0;

  if (nDeviceCoords>0)
    nDeviceCoords--;

  m_szPrefix[0] = '\0';
  m_szSufix[0] = '\0';
  m_csPCS = icSigUnknownData;
  m_csDevice = icSigUnknownData;

  m_nColorEntrySize = 32/*rootName*/ + (3/*PCS*/ + 1/*iAny*/ + nDeviceCoords)*sizeof(icFloatNumber);

  m_NamedColor = (SIccNamedColorEntry*)calloc(nSize, m_nColorEntrySize);

  m_NamedLab = NULL;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::CIccTagNamedColor2
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITNC = The CIccTagNamedColor2 object to be copied
 *****************************************************************************
 */
CIccTagNamedColor2::CIccTagNamedColor2(const CIccTagNamedColor2 &ITNC)
{
  m_nColorEntrySize = ITNC.m_nColorEntrySize;
  m_nVendorFlags = ITNC.m_nVendorFlags;
  m_nDeviceCoords = ITNC.m_nDeviceCoords;
  m_nSize = ITNC.m_nSize;

  m_csPCS = ITNC.m_csPCS;
  m_csDevice = ITNC.m_csDevice;

  memcpy(m_szPrefix, ITNC.m_szPrefix, sizeof(m_szPrefix));
  memcpy(m_szSufix, ITNC.m_szSufix, sizeof(m_szSufix));

  m_NamedColor = (SIccNamedColorEntry*)calloc(m_nSize, m_nColorEntrySize);
  memcpy(m_NamedColor, ITNC.m_NamedColor, m_nColorEntrySize*m_nSize);

  m_NamedLab = NULL;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  NamedColor2Tag = The CIccTagNamedColor2 object to be copied
 *****************************************************************************
 */
CIccTagNamedColor2 &CIccTagNamedColor2::operator=(const CIccTagNamedColor2 &NamedColor2Tag)
{
  if (&NamedColor2Tag == this)
    return *this;

  m_nColorEntrySize = NamedColor2Tag.m_nColorEntrySize;
  m_nVendorFlags = NamedColor2Tag.m_nVendorFlags;
  m_nDeviceCoords = NamedColor2Tag.m_nDeviceCoords;
  m_nSize = NamedColor2Tag.m_nSize;

  m_csPCS = NamedColor2Tag.m_csPCS;
  m_csDevice = NamedColor2Tag.m_csDevice;

  memcpy(m_szPrefix, NamedColor2Tag.m_szPrefix, sizeof(m_szPrefix));
  memcpy(m_szSufix, NamedColor2Tag.m_szSufix, sizeof(m_szSufix));

  if (m_NamedColor)
    free(m_NamedColor);
  m_NamedColor = (SIccNamedColorEntry*)calloc(m_nSize, m_nColorEntrySize);
  memcpy(m_NamedColor, NamedColor2Tag.m_NamedColor, m_nColorEntrySize*m_nSize);

  m_NamedLab = NULL;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::~CIccTagNamedColor2
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagNamedColor2::~CIccTagNamedColor2()
{
  if (m_NamedColor)
    free(m_NamedColor);

  if (m_NamedLab)
    delete [] m_NamedLab;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::SetSize
 * 
 * Purpose: Sets the size of the named color array.
 * 
 * Args: 
 *  nSize - number of named color entries,
 *  nDeviceCoords - number of device channels
 *****************************************************************************
 */
bool CIccTagNamedColor2::SetSize(icUInt32Number nSize, icInt32Number nDeviceCoords/*=-1*/)
{
  if (nSize <1)
    nSize = 1;
  if (nDeviceCoords<0)
    nDeviceCoords = m_nDeviceCoords;

  icInt32Number nNewCoords=nDeviceCoords;

  if (nDeviceCoords>0)
    nDeviceCoords--;

  size_t nColorEntrySize = 32/*rootName*/ + (3/*PCS*/ + 1/*iAny*/ + (size_t)nDeviceCoords)*sizeof(icFloatNumber);

  SIccNamedColorEntry* pNamedColor = (SIccNamedColorEntry*)calloc(nSize, nColorEntrySize);

  if (!pNamedColor)
    return false;

  icUInt32Number i, nCopy = __min(nSize, m_nSize);
  icUInt32Number j, nCoords = __min(nNewCoords, (icInt32Number)m_nDeviceCoords);

  for (i=0; i<nCopy; i++) {
    SIccNamedColorEntry *pFrom = (SIccNamedColorEntry*)((icChar*)m_NamedColor + i*m_nColorEntrySize);
    SIccNamedColorEntry *pTo = (SIccNamedColorEntry*)((icChar*)pNamedColor + i*nColorEntrySize);

    strcpy(pTo->rootName, pFrom->rootName);
    for (j=0; j<3; j++)
      pTo->pcsCoords[j] = pFrom->pcsCoords[j];

    for (j=0; j<nCoords; j++) {
      pTo->deviceCoords[j] = pFrom->deviceCoords[j];
    }
  }
  free(m_NamedColor);

  m_nColorEntrySize = (icUInt32Number)nColorEntrySize;

  m_NamedColor = pNamedColor;
  m_nSize = nSize;
  m_nDeviceCoords = nNewCoords;

  ResetPCSCache();

  return true;
}


/**
****************************************************************************
* Name: CIccTagNamedColor2::SetPrefix
* 
* Purpose: Set contents of suffix member field
* 
* Args:
*  szPrefix - string to set prefix to
*****************************************************************************
*/
void CIccTagNamedColor2::SetPrefix(const icChar *szPrefix)
{
  strncpy(m_szPrefix, szPrefix, sizeof(m_szPrefix));
  m_szPrefix[sizeof(m_szPrefix)-1]='\0';
}


/**
****************************************************************************
* Name: CIccTagNamedColor2::SetSufix
* 
* Purpose: Set contents of suffix member field
* 
* Args:
*  szPrefix - string to set prefix to
*****************************************************************************
*/
void CIccTagNamedColor2::SetSufix(const icChar *szSufix)
{
  strncpy(m_szSufix, szSufix, sizeof(m_szSufix));
  m_szSufix[sizeof(m_szSufix)-1]='\0';
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagNamedColor2::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number nNum, nCoords;

  icUInt32Number nTagHdrSize = sizeof(icTagTypeSignature) + 
                               sizeof(icUInt32Number) + //m_nReserved=0
                               sizeof(icUInt32Number) + //VendorFlags
                               sizeof(icUInt32Number) + //Num Colors
                               sizeof(icUInt32Number) + //Num Device Coords
                               sizeof(m_szPrefix) + 
                               sizeof(m_szSufix); 
  if (nTagHdrSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read32(&m_nVendorFlags) ||
      !pIO->Read32(&nNum) ||
      !pIO->Read32(&nCoords) ||
      pIO->Read8(m_szPrefix, sizeof(m_szPrefix))!=sizeof(m_szPrefix) ||
      pIO->Read8(m_szSufix, sizeof(m_szSufix))!=sizeof(m_szSufix)) {
    return false;
  }

  size -= nTagHdrSize;

  size_t nCount = size / (32+(3+(size_t)nCoords)*sizeof(icUInt16Number));

  if (nCount < (size_t)nNum)
    return false;

  if (!SetSize(nNum, nCoords))
    return false;

  icUInt32Number i;
  SIccNamedColorEntry *pNamedColor=m_NamedColor;

  for (i=0; i<nNum; i++) {
    if (pIO->Read8(&pNamedColor->rootName, sizeof(pNamedColor->rootName))!=sizeof(pNamedColor->rootName) ||
        pIO->ReadUInt16Float(&pNamedColor->pcsCoords, 3)!=3)
      return false;
    if (nCoords) {
      if (pIO->ReadUInt16Float(&pNamedColor->deviceCoords, nCoords)!= nCoords)
        return false;
    }
    pNamedColor = (SIccNamedColorEntry*)((icChar*)pNamedColor + m_nColorEntrySize);
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagNamedColor2::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nVendorFlags))
    return false;

  if (!pIO->Write32(&m_nSize))
    return false;

  if (!pIO->Write32(&m_nDeviceCoords))
    return false;

  if (!pIO->Write8(m_szPrefix, sizeof(m_szPrefix)))
    return false;

  if (!pIO->Write8(m_szSufix, sizeof(m_szSufix)))
    return false;

  icUInt32Number i;
  SIccNamedColorEntry *pNamedColor=m_NamedColor;

  for (i=0; i<m_nSize; i++) {
    if (pIO->Write8(&pNamedColor->rootName, sizeof(pNamedColor->rootName))!=sizeof(pNamedColor->rootName) ||
        pIO->WriteUInt16Float(&pNamedColor->pcsCoords, 3)!=3)
      return false;
    if (m_nDeviceCoords) {
      if (pIO->WriteUInt16Float(&pNamedColor->deviceCoords, m_nDeviceCoords) != m_nDeviceCoords)
        return false;
    }
    pNamedColor = (SIccNamedColorEntry*)((icChar*)pNamedColor + m_nColorEntrySize);
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagNamedColor2::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  const size_t nameSize = 40;
  icChar buf[bufSize], szColorVal[nameSize], szColor[nameSize];

  icUInt32Number i, j;
  SIccNamedColorEntry *pNamedColor=m_NamedColor;

  sDescription.reserve(sDescription.size() + m_nSize*79);

  snprintf(buf, bufSize, "BEGIN_NAMED_COLORS flags=%08x %u %u\n", m_nVendorFlags, m_nSize, m_nDeviceCoords);
  sDescription += buf;

  snprintf(buf, bufSize, "Prefix=\"%s\"\n", m_szPrefix);
  sDescription += buf;

  snprintf(buf, bufSize, "Sufix= \"%s\"\n", m_szSufix);
  sDescription += buf;

  for (i=0; i<m_nSize; i++) {
    snprintf(buf, bufSize, "Color[%u]: %s :", i, pNamedColor->rootName);
    sDescription += buf;
    
    icFloatNumber pcsCoord[3] ={0};
    for (j=0; j<3; j++)
      pcsCoord[j] = pNamedColor->pcsCoords[j];

    if (m_csPCS==icSigLabData) {
      for (j=0; j<3; j++)
        pcsCoord[j] = (icFloatNumber)(pcsCoord[j] * 65535.0 / 65280.0);
    }

    for (j=0; j<3; j++) {
      icColorIndexName(szColor, nameSize, m_csPCS, j, 3, "P");
      icColorValue(szColorVal, nameSize, pcsCoord[j], m_csPCS, j);
      snprintf(buf, bufSize, " %s=%s", szColor, szColorVal);
      sDescription += buf;
    }
    if (m_nDeviceCoords) {
      sDescription += " :";
      for (j=0; j<m_nDeviceCoords; j++) {
        icColorIndexName(szColor, nameSize, m_csDevice, j, m_nDeviceCoords, "D");
        icColorValue(szColorVal, nameSize, pNamedColor->deviceCoords[j], m_csDevice, j);
        snprintf(buf, bufSize, " %s=%s", szColor, szColorVal);
        sDescription += buf;
      }
    }
    sDescription += "\n";

    pNamedColor = (SIccNamedColorEntry*)((icChar*)pNamedColor + m_nColorEntrySize);
  }
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::SetColorSpaces
 * 
 * Purpose: Set the device and PCS color space of the tag
 * 
 * Args: 
 *  csPCS = PCS color space signature,
 *  csDevice = Device color space signature
 * 
 *****************************************************************************
 */
void CIccTagNamedColor2::SetColorSpaces(icColorSpaceSignature csPCS, icColorSpaceSignature csDevice)
{
   m_csPCS = csPCS;
   m_csDevice = csDevice;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::FindRootColor
 * 
 * Purpose: Find the root color name
 * 
 * Args: 
 *  szRootColor = string containing the root color name to be found
 * 
 * Return: Index of the named color array where the root color name was found,
 *  if the color was not found -1 is returned
 *****************************************************************************
 */
icInt32Number CIccTagNamedColor2::FindRootColor(const icChar *szRootColor) const
{
  for (icUInt32Number i=0; i<m_nSize; i++) {
    if (stricmp(m_NamedColor[i].rootName,szRootColor) == 0)
      return i;
  }

  return -1;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::ResetPCSCache
 * 
 * Purpose: This function is called if entry values change between calls
 *  to FindPCSColor()
 * 
 *****************************************************************************
 */
void CIccTagNamedColor2::ResetPCSCache()
{
  if (m_NamedLab) {
    delete [] m_NamedLab;
    m_NamedLab = NULL;
  }
}

/**
****************************************************************************
* Name: CIccTagNamedColor2::InitFindPCSColor
* 
* Purpose: Initialization needed for using FindPCSColor
* 
* Return: 
*  true if successfull, false if failure
*****************************************************************************
*/
bool CIccTagNamedColor2::InitFindCachedPCSColor()
{
  icFloatNumber *pXYZ, *pLab;

  if (!m_NamedLab) {
    m_NamedLab = new SIccNamedLabEntry[m_nSize];
    if (!m_NamedLab)
      return false;

    if (m_csPCS != icSigLabData) {
      for (icUInt32Number i=0; i<m_nSize; i++) {
        pLab = m_NamedLab[i].lab;
        pXYZ = m_NamedColor[i].pcsCoords;
        icXyzFromPcs(pXYZ);
        icXYZtoLab(pLab, pXYZ);
      }
    }
    else {
      for (icUInt32Number i=0; i<m_nSize; i++) {
        pLab = m_NamedLab[i].lab;
        Lab2ToLab4(pLab, m_NamedColor[i].pcsCoords);
        icLabFromPcs(pLab);
      }
    }
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::FindPCSColor
 * 
 * Purpose: Find the PCS color within the specified deltaE
 * 
 * Args: 
 *  pPCS = PCS co-ordinates,
 *  dMinDE = the minimum deltaE (tolerance)
 * 
 * Return: Index of the named color array where the PCS color was found,
 *  if the color was not found within the tolerance -1 is returned
 *****************************************************************************
 */
icInt32Number CIccTagNamedColor2::FindCachedPCSColor(icFloatNumber *pPCS, icFloatNumber dMinDE/*=1000.0*/) const
{
  icFloatNumber dCalcDE, dLeastDE=0.0;
  icFloatNumber pLabIn[3];
  icFloatNumber *pXYZ, *pLab;
  icInt32Number leastDEindex = -1;
  if (m_csPCS != icSigLabData) {
    pXYZ = pPCS;
    icXyzFromPcs(pXYZ);
    icXYZtoLab(pLabIn,pXYZ);
  }
  else {
    Lab2ToLab4(pLabIn, pPCS);
    icLabFromPcs(pLabIn);
  }

  if (!m_NamedLab)
    return -1;

  for (icUInt32Number i=0; i<m_nSize; i++) {
    pLab = m_NamedLab[i].lab;

    dCalcDE = icDeltaE(pLabIn, pLab);

    if (i==0) {
      dLeastDE = dCalcDE;
      leastDEindex = i;
    }

    if (dCalcDE<dMinDE) {
      if (dCalcDE<dLeastDE) {
        dLeastDE = dCalcDE;
        leastDEindex = i;
      }      
    }
  }

  return leastDEindex;
}

/**
****************************************************************************
* Name: CIccTagNamedColor2::FindPCSColor
* 
* Purpose: Find the PCS color within the specified deltaE
* 
* Args: 
*  pPCS = PCS co-ordinates,
*  dMinDE = the minimum deltaE (tolerance)
* 
* Return: Index of the named color array where the PCS color was found,
*  if the color was not found within the tolerance -1 is returned
*****************************************************************************
*/
icInt32Number CIccTagNamedColor2::FindPCSColor(icFloatNumber *pPCS, icFloatNumber dMinDE/*=1000.0*/)
{
  if (!m_NamedLab)
    InitFindCachedPCSColor();

  return FindCachedPCSColor(pPCS, dMinDE);
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::FindColor
 * 
 * Purpose: Find the color with given name
 * 
 * Args: 
 *  szColor = the color name
 * 
 * Return: Index of the named color array where the color name was found,
 *  if the color was not found -1 is returned
 *****************************************************************************
 */
icInt32Number CIccTagNamedColor2::FindColor(const icChar *szColor) const
{
  std::string sColorName;
  icInt32Number i, j;

  j = (icInt32Number)strlen(m_szPrefix);
  if (j != 0) {  
    if (strncmp(szColor, m_szPrefix, j))
      return -1;
  }

  j = (icInt32Number)strlen(m_szSufix);
  i = (icInt32Number)strlen(szColor);
  if (j != 0) {
    if (strncmp(szColor+(i-j), m_szSufix, j))
      return -1;    
  }


  for ( i=0; i<(icInt32Number)m_nSize; i++) {
    sColorName = m_szPrefix;
    sColorName += m_NamedColor[i].rootName;
    sColorName += m_szSufix;

    if (strcmp(sColorName.c_str(),szColor) == 0)
      return i;
  }

  return -1;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::FindDeviceColor
 * 
 * Purpose: Find the device color
 * 
 * Args: 
 *  pDevColor = device color co-ordinates
 * 
 * Return: Index of the named color array where the closest device color
 *  was found, if device representation is absent -1 is returned.
 *****************************************************************************
 */
icInt32Number CIccTagNamedColor2::FindDeviceColor(icFloatNumber *pDevColor) const
{
  if (!m_nDeviceCoords)
    return -1;
  
  icFloatNumber dCalcDiff=0.0, dLeastDiff=0.0;
  icFloatNumber *pDevOut;
  icInt32Number leastDiffindex = -1;


  for (icUInt32Number i=0; i<m_nSize; i++) {
    pDevOut = m_NamedColor[i].deviceCoords;

    for (icUInt32Number j=0; j<m_nDeviceCoords; j++) {
      dCalcDiff += (pDevColor[j]-pDevOut[j])*(pDevColor[j]-pDevOut[j]);
    }
    dCalcDiff = (icFloatNumber)sqrt(dCalcDiff);

    if (i==0) {
      dLeastDiff = dCalcDiff;
      leastDiffindex = i;
    }

    if (dCalcDiff<dLeastDiff) {
      dLeastDiff = dCalcDiff;
      leastDiffindex = i;
    }      

    dCalcDiff = 0.0;
  }

  return leastDiffindex;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::GetColorName
 * 
 * Purpose: Extracts the color name from the named color array
 * 
 * Args: 
 *  sColorName = string where color name will be stored,
 *  index = array index of the color name
 * 
 * Return: 
 *  true = if the index is within range,
 *  false = index out of range
 *****************************************************************************
 */
bool CIccTagNamedColor2::GetColorName(std::string &sColorName, icInt32Number index) const
{
  if (index > (icInt32Number)m_nSize-1)
    return false;

  sColorName += m_szPrefix;
  SIccNamedColorEntry * pNamedColor = (SIccNamedColorEntry*)((icChar*)m_NamedColor + m_nColorEntrySize * index);
  sColorName += pNamedColor->rootName;
  sColorName += m_szSufix;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::UnitClip
 * 
 * Purpose: Clip number so that its between 0-1
 * 
 * Args: 
 *  v = number to be clipped
 * 
 * Return: Clipped number
 *  
 *****************************************************************************
 */
icFloatNumber CIccTagNamedColor2::UnitClip(icFloatNumber v) const
{
  if (v<0)
    v = 0;
  if (v>1.0)
    v = 1.0;

  return v;
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::NegClip
 * 
 * Purpose: Negative numbers are clipped to zero
 * 
 * Args: 
 *  v = number to be clipped
 * 
 * Return: Clipped number
 *  
 *****************************************************************************
 */
icFloatNumber CIccTagNamedColor2::NegClip(icFloatNumber v) const
{
  if (v<0)
    v=0;
  
  return v;
}


/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::Lab2ToLab4
 * 
 * Purpose: Convert version 2 Lab to version 4 Lab
 * 
 * Args: 
 *  Dst = array to store version 4 Lab coordinates,
 *  Src = array containing version 2 Lab coordinates
 * 
 *****************************************************************************
 */
void CIccTagNamedColor2::Lab2ToLab4(icFloatNumber *Dst, const icFloatNumber *Src) const
{
  Dst[0] = UnitClip((icFloatNumber)(Src[0] * 65535.0 / 65280.0));
  Dst[1] = UnitClip((icFloatNumber)(Src[1] * 65535.0 / 65280.0));
  Dst[2] = UnitClip((icFloatNumber)(Src[2] * 65535.0 / 65280.0));
}

/**
 ****************************************************************************
 * Name: CIccTagNamedColor2::Lab4ToLab2
 * 
 * Purpose: Convert version 4 Lab to version 2 Lab
 * 
 * Args: 
 *  Dst = array to store version 2 Lab coordinates,
 *  Src = array containing version 4 Lab coordinates
 * 
 *****************************************************************************
 */
void CIccTagNamedColor2::Lab4ToLab2(icFloatNumber *Dst, const icFloatNumber *Src) const
{
  Dst[0] = (icFloatNumber)(Src[0] * 65280.0 / 65535.0);
  Dst[1] = (icFloatNumber)(Src[1] * 65280.0 / 65535.0);
  Dst[2] = (icFloatNumber)(Src[2] * 65280.0 / 65535.0);
}


/**
******************************************************************************
* Name: CIccTagNamedColor2::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagNamedColor2::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_nSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty tag!\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }

  if (m_nDeviceCoords) {
    if (pProfile) {
      icUInt32Number nCoords = icGetSpaceSamples(pProfile->m_Header.colorSpace);
      if (m_nDeviceCoords != nCoords) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - Incorrect number of device co-ordinates.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
    else {
      sReport += icMsgValidateWarning;
      sReport += sSigPathName;
      sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
      rv = icMaxStatus(rv, icValidateWarning);
    }
  }


  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::CIccTagXYZ
 * 
 * Purpose: Constructor
 *
 * Args:
 *  nSize = number of XYZ entries
 * 
 *****************************************************************************
 */
CIccTagXYZ::CIccTagXYZ(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_XYZ = (icXYZNumber*)calloc(nSize, sizeof(icXYZNumber));
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::CIccTagXYZ
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITXYZ = The CIccTagXYZ object to be copied
 *****************************************************************************
 */
CIccTagXYZ::CIccTagXYZ(const CIccTagXYZ &ITXYZ)
{
  m_nSize = ITXYZ.m_nSize;

  m_XYZ = (icXYZNumber*)calloc(m_nSize, sizeof(icXYZNumber));
  memcpy(m_XYZ, ITXYZ.m_XYZ, sizeof(icXYZNumber)*m_nSize);
}



/**
 ****************************************************************************
 * Name: CIccTagXYZ::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  XYZTag = The CIccTagXYZ object to be copied
 *****************************************************************************
 */
CIccTagXYZ &CIccTagXYZ::operator=(const CIccTagXYZ &XYZTag)
{
  if (&XYZTag == this)
    return *this;

  m_nSize = XYZTag.m_nSize;

  if (m_XYZ)
    free(m_XYZ);
  m_XYZ = (icXYZNumber*)calloc(m_nSize, sizeof(icXYZNumber));
  memcpy(m_XYZ, XYZTag.m_XYZ, sizeof(icXYZNumber)*m_nSize);

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::~CIccTagXYZ
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagXYZ::~CIccTagXYZ()
{
  if (m_XYZ)
    free(m_XYZ);
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagXYZ::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icXYZNumber) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nNum=((size-2*sizeof(icUInt32Number)) / sizeof(icXYZNumber));
  icUInt32Number nNum32 = nNum*sizeof(icXYZNumber)/sizeof(icUInt32Number);

  if (!SetSize(nNum))
    return false;

  if (pIO->Read32(m_XYZ, nNum32) != nNum32 )
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagXYZ::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  icUInt32Number nNum32 = m_nSize * sizeof(icXYZNumber)/sizeof(icUInt32Number);

  if (
    pIO->Write32(m_XYZ, nNum32) != nNum32)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagXYZ::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagXYZ::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  if (m_nSize == 1 ) {
    snprintf(buf, bufSize, "X=%.4lf, Y=%.4lf, Z=%.4lf\n", icFtoD(m_XYZ[0].X), icFtoD(m_XYZ[0].Y), icFtoD(m_XYZ[0].Z));
    sDescription += buf;
  }
  else {
    icUInt32Number i;
    sDescription.reserve(sDescription.size() + m_nSize*79);

    for (i=0; i<m_nSize; i++) {
      snprintf(buf, bufSize, "value[%u]: X=%.4lf, Y=%.4lf, Z=%.4lf\n", i, icFtoD(m_XYZ[i].X), icFtoD(m_XYZ[i].Y), icFtoD(m_XYZ[i].Z));
      sDescription += buf;
    }
  }
}

/**
 ****************************************************************************
 * Name: CIccTagXYZ::SetSize
 * 
 * Purpose: Sets the size of the XYZ array.
 * 
 * Args: 
 *  nSize - number of XYZ entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
bool CIccTagXYZ::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (nSize==m_nSize)
    return true;

  m_XYZ = (icXYZNumber*)icRealloc(m_XYZ, nSize*sizeof(icXYZNumber));

  if (!m_XYZ) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && m_nSize < nSize) {
    memset(&m_XYZ[m_nSize], 0, (nSize-m_nSize)*sizeof(icXYZNumber));
  }
  m_nSize = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagXYZ::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagXYZ::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_nSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty tag.\n";

    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  for (int i=0; i<(int)m_nSize; i++) {
    rv = icMaxStatus(rv, Info.CheckData(sReport, m_XYZ[i], sSigPathName + ":XYZ"));
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::CIccTagChromaticity
 * 
 * Purpose: Constructor
 *
 * Args:
 *  nSize = number of xy entries
 * 
 *****************************************************************************
 */
CIccTagChromaticity::CIccTagChromaticity(int nSize/*=3*/)
{
  m_nChannels = nSize;
  if (m_nChannels <3)
    m_nChannels = 3;
  m_xy = (icChromaticityNumber*)calloc(nSize, sizeof(icChromaticityNumber));
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::CIccTagChromaticity
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITCh = The CIccTagChromaticity object to be copied
 *****************************************************************************
 */
CIccTagChromaticity::CIccTagChromaticity(const CIccTagChromaticity &ITCh)
{
  m_nChannels = ITCh.m_nChannels;

  m_xy = (icChromaticityNumber*)calloc(m_nChannels, sizeof(icChromaticityNumber));
  memcpy(m_xy, ITCh.m_xy, sizeof(icChromaticityNumber)*m_nChannels);
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ChromTag = The CIccTagChromaticity object to be copied
 *****************************************************************************
 */
CIccTagChromaticity &CIccTagChromaticity::operator=(const CIccTagChromaticity &ChromTag)
{
  if (&ChromTag == this)
    return *this;

  m_nChannels = ChromTag.m_nChannels;

  if (m_xy)
    free(m_xy);
  m_xy = (icChromaticityNumber*)calloc(m_nChannels, sizeof(icChromaticityNumber));
  memcpy(m_xy, ChromTag.m_xy, sizeof(icChromaticityNumber)*m_nChannels);

  return *this;  
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::~CIccTagChromaticity
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagChromaticity::~CIccTagChromaticity()
{
  if (m_xy)
    free(m_xy);
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagChromaticity::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt16Number nChannels;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icUInt32Number) +
      sizeof(icChromaticityNumber) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read16(&nChannels) ||
      !pIO->Read16(&m_nColorantType))
    return false;

  icUInt32Number nNum = (size-3*sizeof(icUInt32Number)) / sizeof(icChromaticityNumber);
  icUInt32Number nNum32 = (nNum*sizeof(icChromaticityNumber)) / sizeof(icU16Fixed16Number);

  if (nNum < nChannels)
    return false;

  // SetSize casts from icUInt32Number down to icUInt16Number. Check for overflow
  if (nNum > (icUInt16Number)nNum)
    return false;

  if (!SetSize((icUInt16Number)nNum))
    return false;

  if (pIO->Read32(&m_xy[0], nNum32) != nNum32 )
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagChromaticity::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write16(&m_nChannels))
    return false;

  if (!pIO->Write16(&m_nColorantType))
    return false;

  icUInt32Number nNum32 = m_nChannels*sizeof(icChromaticityNumber)/sizeof(icU16Fixed16Number);

  if (pIO->Write32(&m_xy[0], nNum32) != nNum32)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagChromaticity::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagChromaticity::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo Fmt;

  icUInt32Number i;
  //sDescription.reserve(sDescription.size() + m_nChannels*79);
  snprintf(buf, bufSize, "Number of Channels : %u\n", m_nChannels);
  sDescription += buf;

  snprintf(buf, bufSize, "Colorant Encoding : %s\n", Fmt.GetColorantEncoding((icColorantEncoding)m_nColorantType));
  sDescription += buf;

  for (i=0; i<m_nChannels; i++) {
    snprintf(buf, bufSize, "value[%u]: x=%.3lf, y=%.3lf\n", i, icUFtoD(m_xy[i].x), icUFtoD(m_xy[i].y));
    sDescription += buf;
  }

}

/**
 ****************************************************************************
 * Name: CIccTagChromaticity::SetSize
 * 
 * Purpose: Sets the size of the xy chromaticity array.
 * 
 * Args: 
 *  nSize - number of xy entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
bool CIccTagChromaticity::SetSize(icUInt16Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nChannels == nSize)
    return true;

  m_xy = (icChromaticityNumber*)icRealloc(m_xy, nSize*sizeof(icChromaticityNumber));

  if (!m_xy) {
    m_nChannels = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nChannels) {
    memset(&m_xy[m_nChannels], 0, (nSize - m_nChannels)*sizeof(icChromaticityNumber));
  }

  m_nChannels = nSize;
  return true;
}


/**
******************************************************************************
* Name: CIccTagChromaticity::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagChromaticity::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (m_nColorantType) {

    if (m_nChannels!=3) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Number of device channels must be three.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    switch(m_nColorantType) {
      case icColorantITU:
        {
          if ( (m_xy[0].x != icDtoUF((icFloatNumber)0.640)) || (m_xy[0].y != icDtoUF((icFloatNumber)0.330)) ||
               (m_xy[1].x != icDtoUF((icFloatNumber)0.300)) || (m_xy[1].y != icDtoUF((icFloatNumber)0.600)) ||
               (m_xy[2].x != icDtoUF((icFloatNumber)0.150)) || (m_xy[2].y != icDtoUF((icFloatNumber)0.060)) ) {
                sReport += icMsgValidateNonCompliant;
                sReport += sSigPathName;
                sReport += " - Chromaticity data does not match specification.\n";
                rv = icMaxStatus(rv, icValidateNonCompliant);
              }
          break;
        }

      case icColorantSMPTE:
        {
          if ( (m_xy[0].x != icDtoUF((icFloatNumber)0.630)) || (m_xy[0].y != icDtoUF((icFloatNumber)0.340)) ||
               (m_xy[1].x != icDtoUF((icFloatNumber)0.310)) || (m_xy[1].y != icDtoUF((icFloatNumber)0.595)) ||
               (m_xy[2].x != icDtoUF((icFloatNumber)0.155)) || (m_xy[2].y != icDtoUF((icFloatNumber)0.070)) ) {
              sReport += icMsgValidateNonCompliant;
              sReport += sSigPathName;
              sReport += " - Chromaticity data does not match specification.\n";
              rv = icMaxStatus(rv, icValidateNonCompliant);
            }
            break;
        }

      case icColorantEBU:
        {
          if ( (m_xy[0].x != icDtoUF((icFloatNumber)0.64)) || (m_xy[0].y != icDtoUF((icFloatNumber)0.33)) ||
               (m_xy[1].x != icDtoUF((icFloatNumber)0.29)) || (m_xy[1].y != icDtoUF((icFloatNumber)0.60)) ||
               (m_xy[2].x != icDtoUF((icFloatNumber)0.15)) || (m_xy[2].y != icDtoUF((icFloatNumber)0.06)) ) {
              sReport += icMsgValidateNonCompliant;
              sReport += sSigPathName;
              sReport += " - Chromaticity data does not match specification.\n";
              rv = icMaxStatus(rv, icValidateNonCompliant);
            }
            break;
        }

      case icColorantP22:
        {
          if ( (m_xy[0].x != icDtoUF((icFloatNumber)0.625)) || (m_xy[0].y != icDtoUF((icFloatNumber)0.340)) ||
               (m_xy[1].x != icDtoUF((icFloatNumber)0.280)) || (m_xy[1].y != icDtoUF((icFloatNumber)0.605)) ||
               (m_xy[2].x != icDtoUF((icFloatNumber)0.155)) || (m_xy[2].y != icDtoUF((icFloatNumber)0.070)) ) {
              sReport += icMsgValidateNonCompliant;
              sReport += sSigPathName;
              sReport += " - Chromaticity data does not match specification.\n";
              rv = icMaxStatus(rv, icValidateNonCompliant);
            }
            break;
        }

      default:
        {
          sReport += icMsgValidateNonCompliant;
          sReport += sSigPathName;
          sReport += " - Invalid colorant type encoding.\n";
          rv = icMaxStatus(rv, icValidateNonCompliant);
        }
    }
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::CIccTagCicp
 *
 * Purpose: Constructor
 *
 * Args:
 *  nSize = number of XYZ entries
 *
 *****************************************************************************
 */
CIccTagCicp::CIccTagCicp()
{
  m_nColorPrimaries = 0;
  m_nTransferCharacteristics = 0;
  m_nMatrixCoefficients = 0;
  m_nVideoFullRangeFlag = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::CIccTagCicp
 *
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITCICP = The CIccTagCicp object to be copied
 *****************************************************************************
 */
CIccTagCicp::CIccTagCicp(const CIccTagCicp& /* ITXYZ */)
{
}



/**
 ****************************************************************************
 * Name: CIccTagCicp::operator=
 *
 * Purpose: Copy Operator
 *
 * Args:
 *  cicpTag = The CIccTagCicp object to be copied
 *****************************************************************************
 */
CIccTagCicp& CIccTagCicp::operator=(const CIccTagCicp& cicpTag)
{
  if (&cicpTag == this)
    return *this;

  m_nColorPrimaries = cicpTag.m_nColorPrimaries;
  m_nTransferCharacteristics = cicpTag.m_nTransferCharacteristics;
  m_nMatrixCoefficients = cicpTag.m_nMatrixCoefficients;
  m_nVideoFullRangeFlag = cicpTag.m_nVideoFullRangeFlag;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::~CIccTagCicp
 *
 * Purpose: Destructor
 *
 *****************************************************************************
 */
CIccTagCicp::~CIccTagCicp()
{
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::Read
 *
 * Purpose: Read in the tag contents into a data block
 *
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 *
 * Return:
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagCicp::Read(icUInt32Number size, CIccIO* pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) +
    sizeof(icUInt32Number) +
    sizeof(icUInt8Number)*4 > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read8(&m_nColorPrimaries) ||
      !pIO->Read8(&m_nTransferCharacteristics) ||
      !pIO->Read8(&m_nMatrixCoefficients) ||
      !pIO->Read8(&m_nVideoFullRangeFlag))
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::Write
 *
 * Purpose: Write the tag to a file
 *
 * Args:
 *  pIO - The IO object to write tag to.
 *
 * Return:
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagCicp::Write(CIccIO* pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write8(&m_nColorPrimaries) ||
      !pIO->Write8(&m_nTransferCharacteristics) ||
      !pIO->Write8(&m_nMatrixCoefficients) ||
      !pIO->Write8(&m_nVideoFullRangeFlag))
    return false;

  return true;
}


static struct {
  const char* code;
  const char* interp;
} icExampleCicpCodes[] = {
  { "1-1-0-0",    "RGB narrow range representation specified in Recommendation ITU-R BT.709-6, Item 3.4"},
  { "1-13-0-1",   "RGB full range representation specified in IEC 61966-2-1 sRGB"},
  { "9-14-0-0",   "R’G’B’ narrow range representation specified in Recommendation ITU-R BT.2020-2, Table 5"},
  { "9-16-0-0",   "PQ R’G’B’ narrow range representation specified in Recommendation ITU-R BT.2100-2, Table 9"},
  { "9-16-0-1",   "PQ R’G’B’ full range representation specified in Recommendation ITU-R BT.2100-2, Table 9"},
  { "9-18-0-0 ",  "HLG R’G’B’ narrow range representation specified in Recommendation ITU-R BT.2100-2 "},
  { "9-18-0-1",   "HLG R’G’B’ full range representation specified in Recommendation ITU-R BT.2100-2"},
  { "1-1-1-0 ",   "YCbCr representation specified in Recommendation ITU-R BT.709-6, Item 3.4"},
  { "9-14-9-0",   "Y’Cb’Cr’ narrow range representation specified in Recommendation ITU-R BT.2020-2, Table 5"},
  { "9-16-9-0",   "PQ Y’Cb’Cr’ narrow range representation specified in Recommendation ITU-R BT.2100-2, Table 9"},
  { "9-16-14-0",  "PQ ICtCp narrow range representation specified in Recommendation ITU-R BT.2100-2, Table 9"},
  { "9-18-9-0",   "HLG Y’Cb’Cr’ narrow range representation specified in Recommendation ITU-R BT.2100-2"},
  { "9-18-14-0 ", "HLG ICtCp narrow range representation specified in Recommendation ITU-R BT.2100-2"},
  { NULL, NULL},
};

/**
 ****************************************************************************
 * Name: CIccTagCicp::Describe
 *
 * Purpose: Dump data associated with the tag to a string
 *
 * Args:
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagCicp::Describe(std::string& sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 256;
  const size_t codeSize = 128;
  icChar buf[bufSize], code[codeSize];
  snprintf(code, codeSize, "%u-%u-%u-%u", m_nColorPrimaries, m_nTransferCharacteristics, m_nMatrixCoefficients, m_nVideoFullRangeFlag);

  snprintf(buf, bufSize, "ColorPrimaries=%u\r\n", m_nColorPrimaries);
  sDescription += buf;

  snprintf(buf, bufSize, "TransferCharacteristics=%u\r\n", m_nTransferCharacteristics);
  sDescription += buf;

  snprintf(buf, bufSize, "MatrixCoefficients=%u\r\n", m_nMatrixCoefficients);
  sDescription += buf;

  snprintf(buf, bufSize, "VideoFullRangeFlag=%u\r\n\r\n", m_nVideoFullRangeFlag);
  sDescription += buf;

  snprintf(buf, bufSize, "Combined Code: %s\r\n", code);
  sDescription += buf;

  int i;
  for (i = 0; icExampleCicpCodes[i].code; i++)
    if (!strcmp(code, icExampleCicpCodes[i].code))
      break;

  sDescription += "Interpretation: ";
  if (icExampleCicpCodes[i].code) {
    sDescription += icExampleCicpCodes[i].interp;
  }
  else {
    sDescription += "unknown";
  }
  sDescription += "\r\n";
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::GetFields
 *
 * Purpose: gets the ITU-T H.273 field values
 *
 * Args:
 *  meaning of args as named
 *****************************************************************************
 */
void CIccTagCicp::GetFields(icUInt8Number &colorPrimaries,
                            icUInt8Number &transferCharacteristics,
                            icUInt8Number &matrixCoefficients,
                            icUInt8Number &videoFullRangeFlag)
{
  colorPrimaries = m_nColorPrimaries;
  transferCharacteristics = m_nTransferCharacteristics;
  matrixCoefficients = m_nMatrixCoefficients;
  videoFullRangeFlag = m_nVideoFullRangeFlag;
}


/**
 ****************************************************************************
 * Name: CIccTagCicp::SetFields
 *
 * Purpose: Sets the ITU-T H.273 field values
 *
 * Args:
 *  meaning of args as named
 *****************************************************************************
 */
void CIccTagCicp::SetFields(icUInt8Number colorPrimaries, 
                            icUInt8Number transferCharacteristics,
                            icUInt8Number matrixCoefficients,
                            icUInt8Number videoFullRangeFlag)
{
  m_nColorPrimaries = colorPrimaries;
  m_nTransferCharacteristics = transferCharacteristics;
  m_nMatrixCoefficients =matrixCoefficients;
  m_nVideoFullRangeFlag = videoFullRangeFlag;
}


/**
******************************************************************************
* Name: CIccTagCicp::Validate
*
* Purpose: Check tag data validity.
*
* Args:
*  sig = signature of tag being validated,
*  sReport = String to add report information to
*  pProfile = profile containing tag
*
* Return:
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagCicp::Validate(std::string sigPath, std::string& sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!pProfile)
    return rv;

  if ((pProfile->m_Header.colorSpace == icSigRgbData || pProfile->m_Header.colorSpace == icSigXYZData) && m_nMatrixCoefficients) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - CICP MatrixCoefficients not zero for RGB or XYZ colour space data.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (pProfile->m_Header.colorSpace == icSigYCbCrData && !m_nMatrixCoefficients) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - CICP MatrixCoefficients zero for YCbCr colour space data.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::CIccTagSparseMatrixArray
 * 
 * Purpose: CIccTagSparseMatrixArray Constructor
 * 
 * Args:
 *  nSize = number of data entries
 * 
 *****************************************************************************
 */
CIccTagSparseMatrixArray::CIccTagSparseMatrixArray(int nNumMatrices/* =1 */, int nChannelsPerMatrix/* =4 */)
{
  m_nSize =  nNumMatrices;
  m_nChannelsPerMatrix = nChannelsPerMatrix;
  m_nMatrixType = icSparseMatrixFloat32;

  if (m_nSize <1)
    m_nSize = 1;
  if (nChannelsPerMatrix<4)
    m_nChannelsPerMatrix = 4;

  m_RawData = (icUInt8Number*)calloc(m_nSize, GetBytesPerMatrix());

  m_bNonZeroPadding = false;
}


/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::CIccTagSparseMatrixArray
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITSMA = The CIccTagSparseMatrixArray object to be copied
 *****************************************************************************
 */
CIccTagSparseMatrixArray::CIccTagSparseMatrixArray(const CIccTagSparseMatrixArray &ITSMA)
{
  m_nSize = ITSMA.m_nSize;
  m_nChannelsPerMatrix = ITSMA.m_nChannelsPerMatrix;
  m_nMatrixType = ITSMA.m_nMatrixType;

  m_RawData = (icUInt8Number*)calloc(m_nSize, GetBytesPerMatrix());
  memcpy(m_RawData, ITSMA.m_RawData, m_nSize*GetBytesPerMatrix());

  m_bNonZeroPadding = ITSMA.m_bNonZeroPadding;
}


/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITFN = The CIccTagSparseMatrixArray object to be copied
 *****************************************************************************
 */
CIccTagSparseMatrixArray &CIccTagSparseMatrixArray::operator=(const CIccTagSparseMatrixArray &ITSMA)
{
  if (&ITSMA == this)
    return *this;

  m_nSize = ITSMA.m_nSize;
  m_nChannelsPerMatrix = ITSMA.m_nChannelsPerMatrix;

  if (m_RawData)
    free(m_RawData);
  m_RawData = (icUInt8Number*)calloc(m_nSize, GetBytesPerMatrix());
  memcpy(m_RawData, ITSMA.m_RawData, m_nSize*GetBytesPerMatrix());

  m_bNonZeroPadding = ITSMA.m_bNonZeroPadding;

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::~CIccTagSparseMatrixArray
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagSparseMatrixArray::~CIccTagSparseMatrixArray()
{
  if (m_RawData)
    free(m_RawData);
}


/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagSparseMatrixArray::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt16Number nChannels;
  icUInt16Number nMatrixType;
  icUInt32Number nNumMatrices;

  icUInt32Number nHdrSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    2*sizeof(icUInt16Number) +
    sizeof(icUInt32Number);

  if (nHdrSize +sizeof(icUInt16Number) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read16(&nChannels) ||
      !pIO->Read16(&nMatrixType) ||
      !pIO->Read32(&nNumMatrices))
    return false;
  
  // reject tags with invalid counts of items
  if (nNumMatrices < 1 || nChannels < 1)
    return false;

  m_nMatrixType = (icSparseMatrixType)nMatrixType;

  icUInt32Number nSizeLeft = size - nHdrSize;
  
  // We can't use GetBytesPerMatrix() because the relevant member data isn't set until we call Reset.
  // But this math must match GetBytesPerMatrix()
  icUInt64Number nBytesPerMatrix = nChannels * sizeof(icFloatNumber);

  icUInt64Number nNeededSize = nBytesPerMatrix * nNumMatrices;
  if (nNeededSize > (icUInt64Number)nSizeLeft)
    return false;

  // this sets the sizes, and allocates a huge chunk of memory for matrix storage in m_RawData
  Reset(nNumMatrices, nChannels);
  
  icUInt8Number *matrix_end = m_RawData + (size_t)m_nSize * nBytesPerMatrix;  // overflow detection

  if (m_nSize) {
    icUInt32Number pos;
    icUInt16Number nRows;

    int i, j;
    icUInt32Number n, nAligned;
    CIccSparseMatrix mtx;

    m_bNonZeroPadding = false;

    pos = nHdrSize;
    for (i=0; i<(int)m_nSize; i++) {
      icUInt8Number *pMatrix = m_RawData + (size_t)i * nBytesPerMatrix;

      n=2*sizeof(icUInt16Number);

      if (nSizeLeft<n)
        return false;
      if (pIO->Read16(pMatrix, 2)!=2) {
        return false;
      }

      nSizeLeft -= n;
      pos += n;
      
      nRows = *((icUInt16Number*)pMatrix);

      n=(nRows+1)*sizeof(icUInt16Number);

      if (nSizeLeft<n)
        return false;
      
      size_t rowsRead = nRows+1;
      if (pMatrix+2*sizeof(icUInt16Number)+rowsRead*sizeof(icUInt16Number) >= matrix_end)
        return false;
      if (pIO->Read16(pMatrix+2*sizeof(icUInt16Number), rowsRead)!=rowsRead) {
        return false;
      }

      nSizeLeft -= n;
      pos += n;
      mtx.Reset(pMatrix, nBytesPerMatrix, icSparseMatrixFloatNum, true);

      if (mtx.GetNumEntries()>mtx.MaxEntries(nChannels*sizeof(icFloatNumber), mtx.Rows(), sizeof(icFloatNumber)))
        return false;

      size_t num_entries = mtx.GetNumEntries();
      n = (icUInt32Number) (num_entries*sizeof(icUInt16Number));

      if (nSizeLeft<n)
        return false;

      icUInt16Number *rowPtr = mtx.GetColumnsForRow(0);
      if ( (rowPtr + num_entries) >= (icUInt16Number *)matrix_end)
        return false;
      if (pIO->Read16(rowPtr, num_entries)!=num_entries)
        return false;

      nSizeLeft -= n;
      pos += n;

      nAligned = ((pos+3)/4)*4;
      if (nAligned != pos) {
        n = nAligned - pos;
        if (nSizeLeft < n)
          return false;

        char zbuf[3] = { 0 };
        if (pIO->Read8(&zbuf[0], n)!=n)
          return false;
        for (j=0; j<(int)n; j++) {
          if (zbuf[j])
            m_bNonZeroPadding = true;
        }

        nSizeLeft -= n;
        pos += n;
      }

      switch(m_nMatrixType) {
        case icSparseMatrixUInt8:
          n=mtx.GetNumEntries()*sizeof(icUInt8Number);
          if (nSizeLeft<n)
            return false;

          if (pIO->ReadUInt8Float(mtx.GetData()->getPtr(0), mtx.GetNumEntries())!=mtx.GetNumEntries())
            return false;

          break;

        case icSparseMatrixUInt16:
          n=mtx.GetNumEntries()*sizeof(icUInt16Number);
          if (nSizeLeft<n)
            return false;

          if (pIO->ReadUInt16Float(mtx.GetData()->getPtr(0), mtx.GetNumEntries())!=mtx.GetNumEntries())
            return false;

          break;

        case icSparseMatrixFloat16:
          n=mtx.GetNumEntries()*sizeof(icFloat16Number);
          if (nSizeLeft<n)
            return false;

          if (pIO->ReadFloat16Float(mtx.GetData()->getPtr(0), mtx.GetNumEntries())!=mtx.GetNumEntries())
            return false;

          break;

        case icSparseMatrixFloat32:
          n=mtx.GetNumEntries()*sizeof(icFloat32Number);
          if (nSizeLeft<n)
            return false;

          if (pIO->ReadFloat32Float(mtx.GetData()->getPtr(0), mtx.GetNumEntries())!=mtx.GetNumEntries())
            return false;

          break;

        default:
          return false;
      }
      nSizeLeft -= n;
      pos += n;

      nAligned = ((pos+3)/4)*4;
      if (nAligned != pos) {
        n = nAligned - pos;
        if (nSizeLeft < n)
          return false;

        icUInt8Number zbuf[3] = { 0 };
        if (pIO->Read8(&zbuf[0], n)!=n)
          return false;
        for (j=0; j<(int)n; j++) {
          if (zbuf[j])
            m_bNonZeroPadding = true;
        }

        nSizeLeft -= n;
        pos += n;
      }
    }
    if (nSizeLeft) {
      icUInt8Number b;
      for (i=0; i<(int)nSizeLeft; i++) {
        if (!pIO->Read8(&b))
          return false;
        if (b)
          m_bNonZeroPadding = true;
      }
    }
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagSparseMatrixArray::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt16Number nTemp = m_nMatrixType;

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write16(&m_nChannelsPerMatrix) ||
      !pIO->Write16(&nTemp) ||
      !pIO->Write32(&m_nSize))
      return false;

  icUInt64Number nBytesPerMatrix = m_nChannelsPerMatrix * sizeof(icFloatNumber);
  icUInt64Number nNeededSize = m_nSize * nBytesPerMatrix;
  CIccSparseMatrix mtx;
  icUInt16Number nRows;
  int i;
  
  // current profile format cannot go over 4 Gig
  if (nNeededSize >= (0x100000000ULL - pIO->Tell()))
    return false;

  for (i=0; i<(int)m_nSize; i++) {
    icUInt8Number *pMatrix = m_RawData + (size_t)i * nBytesPerMatrix;
    mtx.Reset(pMatrix, nBytesPerMatrix, icSparseMatrixFloatNum, true);
    nRows = mtx.Rows();

    //int n = (nRows+3)*sizeof(icUInt16Number);

    size_t rowsToWrite = nRows+3;
    if (pIO->Write16(pMatrix, rowsToWrite)!=rowsToWrite ||
        pIO->Write16(mtx.GetColumnsForRow(0), mtx.GetNumEntries())!=mtx.GetNumEntries()) {
      return false;
    }

    if (!pIO->Align32())
      return false;

    switch(m_nMatrixType) {
      case icSparseMatrixUInt8:
        if (pIO->WriteUInt8Float(mtx.GetData()->getPtr(), mtx.GetNumEntries())!=mtx.GetNumEntries()) {
          return false;
        }
        break;
      case icSparseMatrixUInt16:
        if (pIO->WriteUInt16Float(mtx.GetData()->getPtr(), mtx.GetNumEntries())!=mtx.GetNumEntries()) {
          return false;
        }
        break;
      case icSparseMatrixFloat16:
        if (pIO->WriteFloat16Float(mtx.GetData()->getPtr(), mtx.GetNumEntries())!=mtx.GetNumEntries()) {
          return false;
        }
        break;
      case icSparseMatrixFloat32:
        if (pIO->WriteFloat32Float(mtx.GetData()->getPtr(), mtx.GetNumEntries())!=mtx.GetNumEntries()) {
          return false;
        }
        break;
      default:
        return false;
    }

    if (!pIO->Align32())
      return false;
    
  }
 
  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagSparseMatrixArray::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  sDescription += "Begin_SparseMatrix_Array\n";
  snprintf(buf, bufSize, "OutputChannels = %d\n", m_nChannelsPerMatrix);
  sDescription += buf;
  snprintf(buf, bufSize, "MatrixType = %d\n", m_nMatrixType);
  sDescription += buf;

  int i, r, c;
  icUInt32Number nBytesPerMatrix = m_nChannelsPerMatrix * sizeof(icFloatNumber);
  for (i=0; i<(int)m_nSize; i++) {
    CIccSparseMatrix mtx(&m_RawData[i*nBytesPerMatrix], nBytesPerMatrix, icSparseMatrixFloatNum, true);

    snprintf(buf, bufSize, "\nBegin_Matrix_%d\n", i);
    sDescription += buf;

    icUInt16Number *start = mtx.GetRowStart();
    icUInt16Number *cols = mtx.GetColumnsForRow(0);
    for (r=0; r<mtx.Rows(); r++) {
      icUInt16Number rs = start[r];
      icUInt16Number re = start[r+1];
      snprintf(buf, bufSize, "Row%d:", r);
      sDescription += buf;

      for (c=rs; c<re; c++) {
        snprintf(buf, bufSize, " (%d, %.4lf)", cols[c], mtx.GetData()->get(c));
        sDescription += buf;
      }
    }

    snprintf(buf, bufSize, "End_Matrix_%d\n", i);
    sDescription += buf;
  }
  sDescription += "\nEnd_SparseMatrix_Array\n";

}

/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::Validate
 * 
 * Purpose: Validates tag data
 * 
 * Args: 
 *  sigPath - signature path of tag in profile to validate
 *  sReport - string to put validation report results
 *  profile - pointer to profile that contains the tag
 *****************************************************************************
 */
icValidateStatus CIccTagSparseMatrixArray::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = icValidateOK;
  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  switch(m_nMatrixType) {
    case icSparseMatrixUInt8:
    case icSparseMatrixUInt16:
    case icSparseMatrixFloat16:
    case icSparseMatrixFloat32:
      break;
    default:
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Invalid Sparse Matrix Type.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);

  }

  bool bCheckPCS=false;

  icSignature sig1 = icGetFirstSigPathSig(sigPath);
  //icSignature sig2 = icGetSecondSigPathSig(sigPath);    // TODO - unused, but is this call needed for side effects?

  if (sig1==icSigSpectralWhitePointTag) {
    bCheckPCS = true;
  }

  if (bCheckPCS && pProfile) {
    if (!icIsSameColorSpaceType(pProfile->m_Header.spectralPCS, icSigSparseMatrixReflectanceData)) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - SparseMatrix data incompatible with spectral PCS.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
    else if (icGetSpaceSamples((icColorSpaceSignature)pProfile->m_Header.spectralPCS)!=m_nChannelsPerMatrix) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - SparseMatrix Output channels doesn't match spectral PCS channels.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }

  if (m_bNonZeroPadding) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Non zero padding in matrices used.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (!m_nSize) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - No Matrices Defined.\n";
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  if (!m_RawData) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Data dont defined for matrices\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
    return rv;
  }

  icUInt16Number nRows, nCols;
  CIccSparseMatrix mtx;
  int i;

  icUInt16Number nBytesPerMatrix = m_nChannelsPerMatrix * sizeof(icFloatNumber);

  mtx.Reset(m_RawData, nBytesPerMatrix, icSparseMatrixFloatNum, true);
  nRows = mtx.Rows();
  nCols = mtx.Cols();
  icUInt32Number nMaxElements = CIccSparseMatrix::MaxEntries(nBytesPerMatrix, nRows, sizeof(icFloatNumber));
  const size_t bufSize = 128;
  char buf[bufSize];
  icUInt8Number *temp = new icUInt8Number[nBytesPerMatrix];

  for (i=0; i<(int)m_nSize; i++) {
    mtx.Reset(m_RawData+i*nBytesPerMatrix, nBytesPerMatrix, icSparseMatrixFloatNum, true);
    if (mtx.Rows() != nRows || mtx.Cols() != nCols) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      snprintf(buf, bufSize, " - Matrix[%d] doesn't have matching rows and columns.\n", i);
      sReport += buf;
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    if (mtx.GetNumEntries()>nMaxElements) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      snprintf(buf, bufSize, " - Matrix[%d] entries exceeds number supported by Output channels.\n", i);
        sReport += buf;
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    if (!mtx.IsValid()) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      snprintf(buf, bufSize, " - Matrix[%d] has an invalid matrix structure.\n", i);
        sReport += buf;
      rv = icMaxStatus(rv, icValidateCriticalError);
    }

    if (i<(int)(m_nSize-1)) {
      CIccSparseMatrix umtx(temp, nBytesPerMatrix, icSparseMatrixFloatNum, false);
      CIccSparseMatrix mtx2(m_RawData+(i+1)*nBytesPerMatrix, nBytesPerMatrix, icSparseMatrixFloatNum, true);

      umtx.Init(nRows, nCols);
      if (!umtx.Union(mtx, mtx2)) {
        sReport += icMsgValidateCriticalError;
        sReport += sSigPathName;
        snprintf(buf, bufSize, " - Interpolation from Matrix[%d] exceeds number of supported Output channels.\n", i);
        sReport += buf;
        rv = icMaxStatus(rv, icValidateCriticalError);
      }
    }
  }
  delete [] temp;

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagSparseMatrixArray::Reset
 * 
 * Purpose: Sets the size of the data array erasing all previous values
 * 
 * Args: 
 *  nNumMatrices - number of matrices
 *  numChannelsPerMatrix - Equivalent number of output channels that
 *   determines the fixed block size for each matrix
 *****************************************************************************
 */
bool CIccTagSparseMatrixArray::Reset(icUInt32Number nNumMatrices, icUInt16Number nChannelsPerMatrix)
{
  if (nNumMatrices==m_nSize && nChannelsPerMatrix==m_nChannelsPerMatrix)
    return true;

  m_nSize = nNumMatrices;
  m_nChannelsPerMatrix = nChannelsPerMatrix;

  size_t nSize = (size_t)m_nSize * GetBytesPerMatrix();
  
  m_RawData = (icUInt8Number *)icRealloc(m_RawData, nSize);

  if (!m_RawData) {
    m_nSize = 0;
    return false;
  }

  memset(m_RawData, 0, nSize);
  return true;
}


/**
****************************************************************************
* Name: CIccTagSparseMatrixArray::GetValues
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
bool CIccTagSparseMatrixArray::GetSparseMatrix(CIccSparseMatrix &mtx, int nIndex, bool bInitFromData/*=true*/)
{
  if (nIndex<0 || nIndex>(int)m_nSize) {
    mtx.Reset(NULL, 0, icSparseMatrixFloatNum, false);
    return false;
  }

  icUInt32Number nBytesPerMatrix = GetBytesPerMatrix();

  mtx.Reset(m_RawData+nIndex*GetBytesPerMatrix(), nBytesPerMatrix, icSparseMatrixFloatNum, bInitFromData);

  return true;
}

bool CIccTagSparseMatrixArray::GetValues(icFloatNumber *DstVector, icUInt32Number nStart/*=0*/, icUInt32Number nVectorSize/*=1*/) const
{
  icUInt32Number nBytesPerMatrix = GetBytesPerMatrix();
  if (nVectorSize!=nBytesPerMatrix)
    return false;
  if (nStart % nBytesPerMatrix != 0)
    return false;

  if (nStart / nBytesPerMatrix > m_nSize)
    return false;

  memcpy(DstVector, m_RawData+nStart, nVectorSize);
  return true;
}


/**
****************************************************************************
* Name: CIccTagSparseMatrixArray::Interpolate
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
bool CIccTagSparseMatrixArray::Interpolate(icFloatNumber *DstVector, icFloatNumber pos,
                                           icUInt32Number nVectorSize, icFloatNumber *zeroVals) const
{
  icUInt32Number nMatrix = m_nSize;

  if (!nMatrix || nVectorSize != GetBytesPerMatrix())
    return false;

  if (zeroVals)
    nMatrix ++;
  else if (nMatrix<=1)
    return false;

  if (pos<0.0)
    pos=0.0;
  if (pos>1.0)
    pos=1.0;

  icFloatNumber fpos = (icFloatNumber)(nMatrix-1) * pos;
  icUInt32Number iPos = (icUInt32Number)fpos;
  icFloatNumber x = fpos - iPos;

  if (iPos == nMatrix-1) {
    iPos--;
    x = (icFloatNumber)1.0;
  }

  icUInt8Number *lo, *hi;

  if (zeroVals) {
    if (!iPos) {
      lo = (icUInt8Number*)zeroVals;
      hi = m_RawData;
    }
    else {
      lo = &m_RawData[(iPos-1)*nVectorSize];
      hi = &lo[nVectorSize];
    }
  }
  else {
    lo = &m_RawData[iPos*nVectorSize];
    hi = &lo[nVectorSize];
  }

  CIccSparseMatrix mlo(lo, nVectorSize, icSparseMatrixFloatNum, true);
  CIccSparseMatrix mhi(hi, nVectorSize, icSparseMatrixFloatNum, true);
  CIccSparseMatrix dst((icUInt8Number*)DstVector, nVectorSize, icSparseMatrixFloatNum, false);

  if (mlo.Rows()!=mhi.Rows() || mlo.Cols()!=mhi.Cols())
    return false;

  if (!dst.Interp(1.0f-x, mlo, x, mhi))
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagSparseMatrixArray::ValuePos
* 
* Purpose: Gets position of a value int the num array tag
* 
* Args: 
*  DstPos - position of val in array
*  val - value to look for in array.
*  bNoZero - flag indicating whether first entry is zero
*****************************************************************************
*/
bool CIccTagSparseMatrixArray::ValuePos(icFloatNumber & /* DstPos */, icFloatNumber /* val */, bool & /* bNoZero */) const
{
  //ValuePos not supported for Sparse Matrices
  return false;
}


/**
 ****************************************************************************
 * Name: CIccTagFixedNum::CIccTagFixedNum
 * 
 * Purpose: CIccTagFixedNumConstructor
 * 
 * Args:
 *  nSize = number of data entries
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFixedNum<T, Tsig>::CIccTagFixedNum(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_Num = (T*)calloc(nSize, sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagFixedNum::CIccTagFixedNum
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITFN = The CIccTagFixedNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFixedNum<T, Tsig>::CIccTagFixedNum(const CIccTagFixedNum<T, Tsig> &ITFN)
{
  m_nSize = ITFN.m_nSize;
  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITFN.m_Num, m_nSize*sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagFixedNum::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITFN = The CIccTagFixedNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFixedNum<T, Tsig> &CIccTagFixedNum<T, Tsig>::operator=(const CIccTagFixedNum<T, Tsig> &ITFN)
{
  if (&ITFN == this)
    return *this;

  m_nSize = ITFN.m_nSize;

  if (m_Num)
    free(m_Num);
  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITFN.m_Num, m_nSize*sizeof(T));

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagFixedNum::~CIccTagFixedNum
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFixedNum<T, Tsig>::~CIccTagFixedNum()
{
  if (m_Num)
    free(m_Num);
}

/**
 ****************************************************************************
 * Name: CIccTagFixedNum::GetClassName
 * 
 * Purpose: Returns the tag type class name
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
const icChar* CIccTagFixedNum<T, Tsig>::GetClassName() const
{
  if (Tsig==icSigS15Fixed16ArrayType)
    return "CIccTagS15Fixed16";
  else 
    return "CIccTagU16Fixed16";
}


/**
 ****************************************************************************
 * Name: CIccTagFixedNum::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(T) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nSize=((size-2*sizeof(icUInt32Number)) / sizeof(T));

  if (!SetSize(nSize))
    return false;

  if (pIO->Read32(m_Num, nSize) != nSize )
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagFixedNum::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (pIO->Write32(m_Num, m_nSize) != m_nSize)
    return false;
 
  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagFixedNum::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
void CIccTagFixedNum<T, Tsig>::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize] = { 0 };

  if (m_nSize == 1 ) {
    if (Tsig==icSigS15Fixed16ArrayType) 
      snprintf(buf, bufSize, "Value = %.4lf\n", icFtoD(m_Num[0]));
    else
      snprintf(buf, bufSize, "Value = %.4lf\n", icUFtoD(m_Num[0]));
    sDescription += buf;
  }
  else {
    icUInt32Number i;

    if (Tsig==icSigS15Fixed16ArrayType && m_nSize==9) {
      sDescription += "Matrix Form:\n";
      icMatrixDump(sDescription, (icS15Fixed16Number*)m_Num);

      sDescription += "\nArrayForm:\n";
    }
    sDescription.reserve(sDescription.size() + m_nSize*79);

    for (i=0; i<m_nSize; i++) {
      if (Tsig==icSigS15Fixed16ArrayType) 
        snprintf(buf, bufSize, "Value[%u] = %8.4lf\n", i, icFtoD(m_Num[i]));
      else
        snprintf(buf, bufSize, "Value[%u] = %8.4lf\n", i, icUFtoD(m_Num[i]));
      sDescription += buf;
    }
  }
}

/**
 ****************************************************************************
 * Name: CIccTagFixedNum::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of data entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (nSize==m_nSize)
    return true;

  m_Num = (T*)icRealloc(m_Num, nSize*sizeof(T));

  if (!m_Num) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && m_nSize < nSize) {
    memset(&m_Num[m_nSize], 0, (nSize-m_nSize)*sizeof(T));
  }
  m_nSize = nSize;

  return true;
}

/**
****************************************************************************
* Name: CIccTagFixedNum::GetValues
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::GetValues(icFloatNumber *DstVector, icUInt32Number nStart, icUInt32Number nVectorSize) const
{
  if (nVectorSize+nStart >m_nSize)
    return false;

  icUInt32Number i;

  switch (Tsig) {
    case icSigS15Fixed16ArrayType:
      for (i=0; i<m_nSize; i++) {
        DstVector[i] = (icFloatNumber)icFtoD(m_Num[i+nStart]);
      }
      break;
    case icSigU16Fixed16ArrayType:
      for (i=0; i<m_nSize; i++) {
        DstVector[i] = (icFloatNumber)icUFtoD(m_Num[i+nStart]);
      }
      break;
    default:
      return false;
  }
  return true;
}


/**
****************************************************************************
* Name: CIccTagFixedNum::Interpolate
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::Interpolate(icFloatNumber *DstVector, icFloatNumber pos,
                                           icUInt32Number nVectorSize, icFloatNumber *zeroVals) const
{
  icUInt32Number nVector = m_nSize / nVectorSize;

  if (!nVector)
    return false;

  if (zeroVals)
    nVector ++;
  else if (nVector<=1)
    return false;

  if (pos<0.0)
    pos=0.0;
  if (pos>1.0)
    pos=1.0;

  icFloatNumber fpos = (icFloatNumber)(nVector-1) * pos;
  icUInt32Number iPos = (icUInt32Number)fpos;
  icFloatNumber x = fpos - iPos;

  if (iPos == nVector-1) {
    iPos--;
    x = (icFloatNumber)1.0;
  }

  T *lo, *hi;

  if (zeroVals) {
    if (!iPos) {
      lo = NULL;
      hi = m_Num;
    }
    else {
      lo = &m_Num[(iPos-1)*nVectorSize];
      hi = &lo[nVectorSize];
    }
  }
  else {
    lo = &m_Num[iPos*nVectorSize];
    hi = &lo[nVectorSize];
  }

  icUInt32Number i;

  switch (Tsig) {
    case icSigS15Fixed16ArrayType:
      if (!lo) {
        if (zeroVals) {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = (icFloatNumber)(zeroVals[i] * (1.0f - x) + icFtoD(hi[i]) * x);
          }
        }
        else {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = (icFloatNumber)(icFtoD(hi[i]) * x);
          }
        }
      }
      else {
        for (i=0; i<m_nSize; i++) {
          DstVector[i] = (icFloatNumber)(icFtoD(lo[i])*(1.0-x) + icFtoD(hi[i])*x);
        }
      }
      break;
    case icSigU16Fixed16ArrayType:
      if (!lo) {
        if (zeroVals) {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = (icFloatNumber)(zeroVals[i] * (1.0 - x) + icUFtoD(hi[i]) * x);
          }
        }
        else {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = (icFloatNumber)(icUFtoD(hi[i]) * x);
          }
        }
      }
      else {
        for (i=0; i<m_nSize; i++) {
          DstVector[i] = (icFloatNumber)(icUFtoD(lo[i])*(1.0-x) + icUFtoD(hi[i])*x);
        }
      }
      break;
    default:
      return false;
  }
  return true;
}

/**
****************************************************************************
* Name: CIccTagFixedNum::ValuePos
* 
* Purpose: Gets position of a value int the num array tag
* 
* Args: 
*  DstPos - position of val in array
*  val - value to look for in array.
*  bNoZero - flag indicating whether first entry is zero
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFixedNum<T, Tsig>::ValuePos(icFloatNumber &DstPos, icFloatNumber val, bool &bNoZero) const
{
  if(val<0.0)
    return false;

  icFloatNumber nv = 0.0;
  icFloatNumber lv = 0.0;
  switch (Tsig) {
    case icSigS15Fixed16ArrayType:
      lv = icFtoD(m_Num[0]);
      break;
    case icSigU16Fixed16ArrayType:
      lv = icUFtoD(m_Num[0]);
      break;
    default:
      return false;
  }
  bNoZero = (lv!=0.0);

  if (val<lv) {
    DstPos = val/lv - 1.0f;
    return true;
  }

  icUInt32Number i;
  for (i=1; i<m_nSize; i++, lv=nv) {
    switch (Tsig) {
      case icSigS15Fixed16ArrayType:
        nv = icFtoD(m_Num[i]);
        break;
      case icSigU16Fixed16ArrayType:
        nv = icUFtoD(m_Num[i]);
        break;
      default:
        return false;
    }
    if (val<=nv) {
      DstPos = (val-lv)/(nv-lv) + i - 1.0f;
      return true;
    }
  }

  return false;
}


//Make sure typedef classes get built
template class CIccTagFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType>;
template class CIccTagFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType>;


/**
 ****************************************************************************
 * Name: CIccTagNum::CIccTagNum
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nSize = number of data entries
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagNum<T, Tsig>::CIccTagNum(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_Num = (T*)calloc(nSize, sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagNum::CIccTagNum
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITNum = The CIccTagNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagNum<T, Tsig>::CIccTagNum(const CIccTagNum<T, Tsig> &ITNum)
{
  m_nSize = ITNum.m_nSize;

  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITNum.m_Num, m_nSize * sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagNum::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITNum = The CIccTagNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagNum<T, Tsig> &CIccTagNum<T, Tsig>::operator=(const CIccTagNum<T, Tsig> &ITNum)
{
  if (&ITNum == this)
    return *this;

  m_nSize = ITNum.m_nSize;

  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITNum.m_Num, m_nSize * sizeof(T));

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagNum::~CIccTagNum
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagNum<T, Tsig>::~CIccTagNum()
{
  if (m_Num)
    free(m_Num);
}

/**
 ****************************************************************************
 * Name: CIccTagNum::GetClassName
 * 
 * Purpose: Returns the tag type class name
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
const icChar *CIccTagNum<T, Tsig>::GetClassName() const
{
  if (sizeof(T)==sizeof(icUInt8Number))
    return "CIccTagUInt8";
  else if (sizeof(T)==sizeof(icUInt16Number))
    return "CIccTagUInt16";
  else if (sizeof(T)==sizeof(icUInt32Number))
    return "CIccTagUInt32";
  else if (sizeof(T)==sizeof(icUInt64Number))
    return "CIccTagUInt64";
  else
    return "CIccTagNum<>";
}


/**
 ****************************************************************************
 * Name: CIccTagNum::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(T) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nSize=((size-2*sizeof(icUInt32Number)) / sizeof(T));

  if (!SetSize(nSize))
    return false;

  if (sizeof(T)==sizeof(icUInt8Number)) {
    if (pIO->Read8(m_Num, nSize) != nSize )
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt16Number)) {
    if (pIO->Read16(m_Num, nSize) != nSize )
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt32Number)) {
    if (pIO->Read32(m_Num, nSize) != nSize )
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt64Number)) {
    if (pIO->Read64(m_Num, nSize) != nSize )
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagNum::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (sizeof(T)==sizeof(icUInt8Number)) {
    if (pIO->Write8(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt16Number)) {
    if (pIO->Write16(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt32Number)) {
    if (pIO->Write32(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else if (sizeof(T)==sizeof(icUInt64Number)) {
    if (pIO->Write64(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagNum::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
void CIccTagNum<T, Tsig>::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize] = {0};

  if (m_nSize == 1 ) {
    switch (sizeof(T)) {
      case 1:
        snprintf(buf, bufSize, "Value = %u (0x02%x)\n", m_Num[0], m_Num[0]);
        break;
      case 2:
        snprintf(buf, bufSize, "Value = %u (0x04%x)\n", m_Num[0], m_Num[0]);
        break;
      case 4:
        snprintf(buf, bufSize, "Value = %u (0x08%x)\n", m_Num[0], m_Num[0]);
        break;
      case 8:
        snprintf(buf, bufSize, "Value = %u (0x016%x)\n", m_Num[0], m_Num[0]);
        break;
      default:
        snprintf(buf, bufSize, "Value = %u (0x%x)\n", m_Num[0], m_Num[0]);
        break;
    }
    sDescription += buf;
  }
  else {
    icUInt32Number i;
    sDescription.reserve(sDescription.size() + m_nSize*79);

    for (i=0; i<m_nSize; i++) {
      switch (sizeof(T)) {
      case 1:
        snprintf(buf, bufSize, "Value = %u (0x02%x)\n", m_Num[i], m_Num[i]);
        break;
      case 2:
        snprintf(buf, bufSize, "Value = %u (0x04%x)\n", m_Num[i], m_Num[i]);
        break;
      case 4:
        snprintf(buf, bufSize, "Value = %u (0x08%x)\n", m_Num[i], m_Num[i]);
        break;
      case 8:
        snprintf(buf, bufSize, "Value = %u (0x016%x)\n", m_Num[i], m_Num[i]);
        break;
      default:
        snprintf(buf, bufSize, "Value = %u (0x%x)\n", m_Num[i], m_Num[i]);
        break;
      }
      sDescription += buf;
    }
  }
}

template <class T, icTagTypeSignature Tsig>
icValidateStatus CIccTagNum<T, Tsig>::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile) const
{
  icValidateStatus rv = icValidateOK;
  //Check # of channels 
  if (icGetFirstSigPathSig(sigPath) == icSigMaterialDefaultValuesTag && 
      pProfile &&
      m_nSize != icGetMaterialColorSpaceSamples(pProfile->m_Header.mcs)) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(sigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Number of material default values does not match MCS in header.\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  rv = icMaxStatus(rv, CIccTagNumArray::Validate(sigPath, sReport, pProfile));

  return rv;
}

// template function specialization to handle need for %llu and %llx for 64-bit ints
template <>
void CIccTagNum<icUInt64Number, icSigUInt64ArrayType>::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  if (m_nSize == 1 ) {
    snprintf(buf, bufSize, "Value = %llu (0x016%llx)\n", m_Num[0], m_Num[0]);
    sDescription += buf;
  }
  else {
    icUInt32Number i;
    sDescription.reserve(sDescription.size() + m_nSize*79);

    for (i=0; i<m_nSize; i++) {
      snprintf(buf, bufSize, "Value = %llu (0x016%llx)\n", m_Num[i], m_Num[i]);
      sDescription += buf;
    }
  }
}




/**
 ****************************************************************************
 * Name: CIccTagNum::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of data entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (nSize==m_nSize)
    return true;

  m_Num = (T*)icRealloc(m_Num, nSize*sizeof(T));

  if (!m_Num) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && m_nSize < nSize) {
    memset(&m_Num[m_nSize], 0, (nSize-m_nSize)*sizeof(T));
  }
  m_nSize = nSize;

  return true;
}

/**
****************************************************************************
* Name: CIccTagNum::GetValues
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::GetValues(icFloatNumber *DstVector, icUInt32Number nStart, icUInt32Number nVectorSize) const
{
  if (nVectorSize+nStart >m_nSize)
    return false;

  icUInt32Number i;
  
  switch (Tsig) {
    case icSigUInt8ArrayType:
      for (i=0; i<m_nSize; i++) {
        DstVector[i] = icU8toF((icUInt8Number)(m_Num[i+nStart]));
      }
      break;
    case icSigUInt16ArrayType:
      for (i=0; i<m_nSize; i++) {
        DstVector[i] = icU16toF((icUInt16Number)(m_Num[i+nStart]));
      }
      break;
    case icSigUInt32ArrayType: //Not Supported
    case icSigUInt64ArrayType: //Not Supported
    default:
      return false;
  }
  return true;
}

//Avoid casting warnings for 32 and 64 bit numbers
template <>
bool CIccTagNum<icUInt32Number, icSigUInt32ArrayType>::GetValues(icFloatNumber * /* DstVector */, icUInt32Number /* nStart */, icUInt32Number /* nVectorSize */) const
{
  return false;
}
template <>
bool CIccTagNum<icUInt64Number, icSigUInt64ArrayType>::GetValues(icFloatNumber * /* DstVector */, icUInt32Number /* nStart */, icUInt32Number /* nVectorSize */) const
{
  return false;
}

/**
****************************************************************************
* Name: CIccTagNum::Interpolate
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::Interpolate(icFloatNumber *DstVector, icFloatNumber pos,
                                      icUInt32Number nVectorSize, icFloatNumber *zeroVals) const
{
  icUInt32Number nVector = m_nSize / nVectorSize;

  if (!nVector)
    return false;

  if (zeroVals)
    nVector ++;
  else if (nVector<=1)
    return false;

  if (pos<0.0)
    pos=0.0;
  if (pos>1.0)
    pos=1.0;

  icFloatNumber fpos = (icFloatNumber)(nVector-1) * pos;
  icUInt32Number iPos = (icUInt32Number)fpos;
  icFloatNumber x = fpos - iPos;

  if (iPos == nVector-1) {
    iPos--;
    x = (icFloatNumber)1.0;
  }

  T *lo, *hi;

  if (!zeroVals) {
    if (!iPos) {
      lo = NULL;
      hi = m_Num;
    }
    else {
      lo = &m_Num[(iPos-1)*nVectorSize];
      hi = &lo[nVectorSize];
    }
  }
  else {
    lo = &m_Num[iPos*nVectorSize];
    hi = &lo[nVectorSize];
  }
  
  icUInt32Number i;

  switch (Tsig) {
    case icSigUInt8ArrayType:
      if (!lo) {
        if (zeroVals) {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = zeroVals[i] * (1.0f - x) + icU8toF((icUInt8Number)hi[i]) * x;
          }
        }
        else {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = icU8toF((icUInt8Number)hi[i]) * x;
          }
        }
      }
      else {
        for (i=0; i<m_nSize; i++) {
          DstVector[i] = icU8toF((icUInt8Number)lo[i])*(1.0f-x) + icU8toF((icUInt8Number)hi[i])*x;
        }
      }
      break;
    case icSigUInt16ArrayType:
      if (!lo) {
        if (zeroVals) {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = zeroVals[i] * (1.0f - x) + icU16toF((icUInt16Number)hi[i]) * x;
          }
        }
        else {
          for (i = 0; i < m_nSize; i++) {
            DstVector[i] = icU16toF((icUInt16Number)hi[i]) * x;
          }
        }
      }
      else {
        for (i=0; i<m_nSize; i++) {
          DstVector[i] = icU16toF((icUInt16Number)lo[i])*(1.0f-x) + icU16toF((icUInt16Number)hi[i])*x;
        }
      }
      break;
    case icSigUInt32ArrayType:
    case icSigUInt64ArrayType:
    default:
      return false;
  }
  return true;
}


/**
****************************************************************************
* Name: CIccTagNum::ValuePos
* 
* Purpose: Gets position of a value int the num array tag
* 
* Args: 
*  DstPos - position of val in array
*  val - value to look for in array.
*  bNoZero - flag indicating whether first entry is zero
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagNum<T, Tsig>::ValuePos(icFloatNumber &DstPos, icFloatNumber val, bool &bNoZero) const
{
  if(val<0.0)
    return false;

  icFloatNumber nv = 0.0;
  icFloatNumber lv = 0.0;
  switch (Tsig) {
    case icSigUInt8ArrayType:
      lv = icU8toF((icUInt8Number)m_Num[0]);
      break;
    case icSigUInt16ArrayType:
      lv = icU16toF((icUInt16Number)m_Num[0]);
      break;
    case icSigUInt32ArrayType:
    case icSigUInt64ArrayType:
    default:
      return false;
  }
  bNoZero = (lv!=0.0);

  if (val<lv) {
    DstPos = val/lv - 1.0f;
    return true;
  }

  icUInt32Number i;
  for (i=1; i<m_nSize; i++, lv=nv) {
    switch (Tsig) {
      case icSigUInt8ArrayType:
        nv = icU8toF((icUInt8Number)m_Num[i]);
        break;
      case icSigUInt16ArrayType:
        nv = icU16toF((icUInt16Number)m_Num[i]);
        break;
      case icSigUInt32ArrayType:
      case icSigUInt64ArrayType:
      default:
        return false;
    }
    if (val<=nv) {
      DstPos = (val-lv)/(nv-lv) + i - 1.0f;
      return true;
    }
  }

  return false;
}

//Avoid casting warnings for 32 and 64 bit numbers
template <>
bool CIccTagNum<icUInt32Number, icSigUInt32ArrayType>::Interpolate(icFloatNumber * /* DstVector */, icFloatNumber /* pos */,
                                                                   icUInt32Number /* nVectorSize */, icFloatNumber * /* zeroVals */ ) const
{
  return false;
}
template <>
bool CIccTagNum<icUInt64Number, icSigUInt64ArrayType>::Interpolate(icFloatNumber * /* DstVector */, icFloatNumber /* pos */,
                                                                   icUInt32Number /* nVectorSize */, icFloatNumber * /* zeroVals */ ) const
{
  return false;
}

template <>
bool CIccTagNum<icUInt32Number, icSigUInt32ArrayType>::ValuePos(icFloatNumber& /* DstPos */, icFloatNumber /* val */, bool & /* bNoZero */ ) const
{
  return false;
}
template <>
bool CIccTagNum<icUInt64Number, icSigUInt64ArrayType>::ValuePos(icFloatNumber& /* DstPos */, icFloatNumber /* val */, bool & /* bNoZero */ ) const
{
  return false;
}

//Make sure typedef classes get built
template class CIccTagNum<icUInt8Number, icSigUInt8ArrayType>;
template class CIccTagNum<icUInt16Number, icSigUInt16ArrayType>;
template class CIccTagNum<icUInt32Number, icSigUInt32ArrayType>;
template class CIccTagNum<icUInt64Number, icSigUInt64ArrayType>;


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::CIccTagFloatNum
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nSize = number of data entries
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFloatNum<T, Tsig>::CIccTagFloatNum(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_Num = (T*)calloc(nSize, sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::CIccTagFloatNum
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITNum = The CIccTagNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFloatNum<T, Tsig>::CIccTagFloatNum(const CIccTagFloatNum<T, Tsig> &ITNum)
{
  m_nSize = ITNum.m_nSize;

  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITNum.m_Num, m_nSize * sizeof(T));
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ITNum = The CIccTagNum object to be copied
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFloatNum<T, Tsig> &CIccTagFloatNum<T, Tsig>::operator=(const CIccTagFloatNum<T, Tsig> &ITNum)
{
  if (&ITNum == this)
    return *this;

  m_nSize = ITNum.m_nSize;

  m_Num = (T*)calloc(m_nSize, sizeof(T));
  memcpy(m_Num, ITNum.m_Num, m_nSize * sizeof(T));

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagFloatNum::~CIccTagFloatNum
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
CIccTagFloatNum<T, Tsig>::~CIccTagFloatNum()
{
  if (m_Num)
    free(m_Num);
}

/**
 ****************************************************************************
 * Name: CIccTagFloatNum::GetClassName
 * 
 * Purpose: Returns the tag type class name
 * 
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
const icChar *CIccTagFloatNum<T, Tsig>::GetClassName() const
{
  if (Tsig==icSigFloat16ArrayType)
    return "CIccFlaot16";
  if (Tsig==icSigFloat32ArrayType)
    return "CIccFloat32";
  else if (Tsig==icSigFloat64ArrayType)
    return "CIccFloat64";
  else
    return "CIccTagFloatNum<>";
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagFloatNum<T, Tsig>::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (Tsig==icSigFloat16ArrayType) {
    if (sizeof(icTagTypeSignature) + 
        sizeof(icUInt32Number) + 
        sizeof(icFloat16Number) > size)
      return false;
  }
  else {
    if (sizeof(icTagTypeSignature) + 
        sizeof(icUInt32Number) + 
        sizeof(T) > size)
      return false;
  }

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (Tsig==icSigFloat16ArrayType) {
    icUInt32Number nSize=((size-2*sizeof(icUInt32Number)) / sizeof(icFloat16Number));

    if (!SetSize(nSize))
      return false;

    if (pIO->ReadFloat16Float(&m_Num[0], nSize) != nSize)
      return false;
  }
  else if (Tsig==icSigFloat32ArrayType) {
    icUInt32Number nSize=((size-2*sizeof(icUInt32Number)) / sizeof(icFloat32Number));

    if (!SetSize(nSize))
      return false;

    if (pIO->Read32(m_Num, nSize) != nSize )
      return false;
  }
  else if (Tsig==icSigFloat64ArrayType) {
    icUInt32Number nSize=((size-2*sizeof(icUInt32Number)) / sizeof(icFloat64Number));

    if (!SetSize(nSize))
      return false;

    if (pIO->Read64(m_Num, nSize) != nSize )
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool CIccTagFloatNum<T, Tsig>::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (Tsig==icSigFloat16ArrayType) {
    if (pIO->WriteFloat16Float(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else if (Tsig == icSigFloat32ArrayType) {
    if (pIO->Write32(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else if (Tsig == icSigFloat64ArrayType) {
    if (pIO->Write64(m_Num, m_nSize) != m_nSize)
      return false;
  }
  else
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
void CIccTagFloatNum<T, Tsig>::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize] = {0};

  if (m_nSize == 1 ) {
    switch (sizeof(T)) {
      case 4:
        snprintf(buf, bufSize, "Value = %.8f\n", m_Num[0]);
        break;
      case 8:
        snprintf(buf, bufSize, "Value = %.16f\n", m_Num[0]);
        break;
      default:
        snprintf(buf, bufSize, "Value = %.f\n", m_Num[0]);
        break;
    }
    sDescription += buf;
  }
  else {
    icUInt32Number i, n;
    sDescription.reserve(sDescription.size() + m_nSize*79);

    snprintf(buf, bufSize, "Begin_Value_Array[%d]\n", m_nSize);
    sDescription += buf;

    if (sizeof(T)!=8)
      n=8;
    else
      n=4;
    
    for (i=0; i<m_nSize; i++) {
      if (i&& !(i%n))
        sDescription += "\n";
       
      switch (sizeof(T)) {
      case 4:
        snprintf(buf, bufSize, " %.8f", m_Num[i]);
        break;
      case 8:
        snprintf(buf, bufSize, " %.16f", m_Num[i]);
        break;
      default:
        snprintf(buf, bufSize, " %f", m_Num[i]);
      }
      sDescription += buf;
    }
    if ((i%n)!=1)
      sDescription += "\n";
    sDescription += "End_Value_Array\n";
  }
}


/**
 ****************************************************************************
 * Name: CIccTagFloatNum::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of data entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
template <class T, icTagTypeSignature Tsig>
bool  CIccTagFloatNum<T, Tsig>::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (nSize==m_nSize)
    return true;

  m_Num = (T*)icRealloc(m_Num, nSize*sizeof(T));

  if (!m_Num) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && m_nSize < nSize) {
    memset(&m_Num[m_nSize], 0, (nSize-m_nSize)*sizeof(T));
  }
  m_nSize = nSize;

  return true;
}

/**
****************************************************************************
* Name: CIccTagFloatNum::GetValues
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFloatNum<T, Tsig>::GetValues(icFloatNumber *DstVector, icUInt32Number nStart, icUInt32Number nVectorSize) const
{
  if (nVectorSize >m_nSize)
    return false;

  icUInt32Number i;

  for (i=0; i<m_nSize; i++) {
    DstVector[i] = (icFloatNumber)m_Num[i+nStart];
  }
  return true;
}


/**
****************************************************************************
* Name: CIccTagFloatNum::Interpolate
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
icValidateStatus CIccTagFloatNum<T, Tsig>::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile) const
{
  icValidateStatus rv = icValidateOK;
  //Check # of channels 
  if (icGetFirstSigPathSig(sigPath) == icSigMaterialDefaultValuesTag && 
    pProfile &&
    m_nSize != icGetMaterialColorSpaceSamples(pProfile->m_Header.mcs)) {
      CIccInfo Info;
      std::string sSigPathName = Info.GetSigPathName(sigPath);

      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - Number of material default values does not match MCS in header.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
  }

  rv = icMaxStatus(rv, CIccTagNumArray::Validate(sigPath, sReport, pProfile));

  return rv;
}



/**
****************************************************************************
* Name: CIccTagFloatNum::Interpolate
* 
* Purpose: Gets values from the num array tag as floating point numbers
* 
* Args: 
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFloatNum<T, Tsig>::Interpolate(icFloatNumber *DstVector, icFloatNumber pos,
                                      icUInt32Number nVectorSize, icFloatNumber *zeroVals) const
{
  icUInt32Number nVector = m_nSize / nVectorSize;

  if (!nVector)
    return false;

  if (zeroVals)
    nVector ++;
  else if (nVector<=1)
    return false;

  if (pos<0.0)
    pos=0.0;
  if (pos>1.0)
    pos=1.0;
  if (isnan(pos))
    pos=0.0;

  icFloatNumber fpos = (icFloatNumber)(nVector-1) * pos;
  icUInt32Number iPos = (icUInt32Number)fpos;
  icFloatNumber x = fpos - iPos;
  icFloatNumber invx = (1.0f - x);

  if (iPos == nVector-1) {
    iPos--;
    x = (icFloatNumber)1.0f;
    invx = (icFloatNumber)0.0f;
  }

  T *lo, *hi;

  if (zeroVals) {
    if (!iPos) {
      lo = NULL;
      hi = m_Num;
    }
    else {
      lo = &m_Num[(iPos-1)*nVectorSize];
      hi = &lo[nVectorSize];
    }
  }
  else {
    lo = &m_Num[iPos*nVectorSize];
    hi = &lo[nVectorSize];
  }

  icUInt32Number i;

  if (!lo) {
    if (zeroVals) {
      for (i = 0; i < nVectorSize; i++) {
        DstVector[i] = (icFloatNumber)(zeroVals[i] * invx + hi[i] * x);
      }
    }
    else {
      for (i = 0; i < nVectorSize; i++) {
        DstVector[i] = (icFloatNumber)(hi[i] * x);
      }
    }
  }
  else {
    for (i=0; i<nVectorSize; i++) {
      DstVector[i] = (icFloatNumber)(lo[i]*invx + hi[i]*x);
    }
  }
  return true;
}


/**
****************************************************************************
* Name: CIccTagFloatNum::ValuePos
* 
* Purpose: Gets position of a value int the num array tag
* 
* Args: 
*  DstPos - position of val in array
*  val - value to look for in array.
*  bNoZero - flag indicating whether first entry is zero
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
bool CIccTagFloatNum<T, Tsig>::ValuePos(icFloatNumber &DstPos, icFloatNumber val, bool &bNoZero) const
{
  if(val<0.0)
    return false;

  icFloatNumber nv = 0.0;
  icFloatNumber lv = 0.0;
  
  lv = (icFloatNumber)m_Num[0];

  bNoZero = (lv!=0.0);

  if (val<lv) {
    DstPos = val/lv - 1.0f;
    return true;
  }

  icUInt32Number i;
  for (i=1; i<m_nSize; i++, lv=nv) {
    nv = (icFloatNumber)m_Num[i];

    if (val<=nv) {
      DstPos = (val-lv)/(nv-lv) + i - 1.0f;
      return true;
    }
  }

  return false;
}


//Make sure typedef classes get built
template class CIccTagFloatNum<icFloat32Number, icSigFloat16ArrayType>;
template class CIccTagFloatNum<icFloat32Number, icSigFloat32ArrayType>;
template class CIccTagFloatNum<icFloat64Number, icSigFloat64ArrayType>;

/**
 ****************************************************************************
 * Name: CIccTagMeasurement::CIccTagMeasurement
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagMeasurement::CIccTagMeasurement()
{
  memset(&m_Data, 0, sizeof(m_Data));
}


/**
 ****************************************************************************
 * Name: CIccTagMeasurement::CIccTagMeasurement
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITM = The CIccTagMeasurement object to be copied
 *****************************************************************************
 */
CIccTagMeasurement::CIccTagMeasurement(const CIccTagMeasurement &ITM)
{
  memcpy(&m_Data, &ITM.m_Data, sizeof(m_Data));
}


/**
 ****************************************************************************
 * Name: CIccTagMeasurement::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  MeasTag = The CIccTagMeasurement object to be copied
 *****************************************************************************
 */
CIccTagMeasurement &CIccTagMeasurement::operator=(const CIccTagMeasurement &MeasTag)
{
  if (&MeasTag == this)
    return *this;

  memcpy(&m_Data, &MeasTag.m_Data, sizeof(m_Data));

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagMeasurement::~CIccTagMeasurement
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagMeasurement::~CIccTagMeasurement()
{
}


/**
 ****************************************************************************
 * Name: CIccTagMeasurement::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagMeasurement::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(m_Data) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number nSize=sizeof(m_Data)/sizeof(icUInt32Number);

  if (pIO->Read32(&m_Data,nSize) != nSize)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagMeasurement::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagMeasurement::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  icUInt32Number nSize=sizeof(m_Data)/sizeof(icUInt32Number);

  if (pIO->Write32(&m_Data,nSize) != nSize)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagMeasurement::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagMeasurement::Describe(std::string &sDescription, int /* nVerboseness */)
{
  CIccInfo Fmt;
  const size_t bufSize = 128;
  icChar buf[bufSize];

   sDescription += Fmt.GetStandardObserverName(m_Data.stdObserver); sDescription += "\n";
   snprintf(buf, bufSize, "Backing measurement: X=%.4lf, Y=%.4lf, Z=%.4lf\n",
           icFtoD(m_Data.backing.X),
           icFtoD(m_Data.backing.Y),
           icFtoD(m_Data.backing.Z)); 
   sDescription += buf;
   sDescription += Fmt.GetMeasurementGeometryName(m_Data.geometry); sDescription += "\n";
   sDescription += Fmt.GetMeasurementFlareName(m_Data.flare); sDescription += "\n";
   sDescription += Fmt.GetIlluminantName(m_Data.illuminant); sDescription += "\n";
}


/**
******************************************************************************
* Name: CIccTagMeasurement::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagMeasurement::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  switch(m_Data.stdObserver) {
  case icStdObsUnknown:
  case icStdObs1931TwoDegrees:
  case icStdObs1964TenDegrees:
    break;

  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid standard observer encoding.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  switch(m_Data.geometry) {
  case icGeometryUnknown:
  case icGeometry045or450:
  case icGeometry0dord0:
    break;

  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid measurement geometry encoding.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  switch(m_Data.illuminant) {
  case icIlluminantUnknown:
  case icIlluminantD50:
  case icIlluminantD65:
  case icIlluminantD93:
  case icIlluminantF2:
  case icIlluminantD55:
  case icIlluminantA:
  case icIlluminantEquiPowerE:
  case icIlluminantF8:
    break;

  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid standard illuminant encoding.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::CIccLocalizedUnicode
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccLocalizedUnicode::CIccLocalizedUnicode()
{
  m_pBuf = (icUInt16Number*)malloc(1*sizeof(icUInt16Number));
  *m_pBuf = 0;
  m_nLength = 0;
}


/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::CIccLocalizedUnicode
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ILU = The CIccLocalizedUnicode object to be copied
 *****************************************************************************
 */
CIccLocalizedUnicode::CIccLocalizedUnicode(const CIccLocalizedUnicode& ILU)
{
  m_nLength = ILU.GetLength();
  m_pBuf = (icUInt16Number*)malloc((m_nLength+2) * sizeof(icUInt16Number));
  if (m_nLength)
    memcpy(m_pBuf, ILU.GetBuf(), m_nLength*sizeof(icUInt16Number));
  m_pBuf[m_nLength] = 0;    // safety against malformed unicode
  m_pBuf[m_nLength+1] = 0;  // safety against malformed unicode
  m_nLanguageCode = ILU.m_nLanguageCode;
  m_nCountryCode = ILU.m_nCountryCode;
}


/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  UnicodeText = The CIccLocalizedUnicode object to be copied
 *****************************************************************************
 */
CIccLocalizedUnicode &CIccLocalizedUnicode::operator=(const CIccLocalizedUnicode &UnicodeText)
{
  if (&UnicodeText == this)
    return *this;

  if (SetSize(UnicodeText.GetLength())) {
    memcpy(m_pBuf, UnicodeText.GetBuf(), m_nLength*sizeof(icUInt16Number));
  }
  m_nLanguageCode = UnicodeText.m_nLanguageCode;
  m_nCountryCode = UnicodeText.m_nCountryCode;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::~CIccLocalizedUnicode
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccLocalizedUnicode::~CIccLocalizedUnicode()
{
  if (m_pBuf)
    free(m_pBuf);
}

/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::GetUtf8Size
 * 
 * Purpose: Returns the size of the ANSI data buffer
 *
 *****************************************************************************
 */
icUInt32Number CIccLocalizedUnicode::GetUtf8Size()
{
  std::string str;
  GetText(str);

  return (icUInt32Number)str.size()+1;
}

/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::GetUtf8
 * 
 * Purpose: Extracts the ANSI data buffer (from UTF-16BE)
 *
 * Args:
 *  szBuf = pointer where the returned string buffer is to be stored
 *  nBufSize = size of the buffer to be extracted
 *
 * Return:
 *  Pointer to the ANSI data string
 *****************************************************************************
 */
const icChar *CIccLocalizedUnicode::GetUtf8(icChar *szBuf, icUInt32Number nBufSize)
{
  if (!szBuf || !nBufSize)
    return NULL;

  std::string str;
  GetText(str);

  if (nBufSize - 1 < str.size()) {
    memcpy(szBuf, str.c_str(), nBufSize - 1);
    szBuf[nBufSize] = 0;
  }
  else {
    strcpy(szBuf, str.c_str());
  }

  return szBuf;
}

/**
****************************************************************************
* Name: CIccLocalizedUnicode::GetText
*
* Purpose: Gets text as a UTF-8 string
*
* Args:
*  sText - string to put results in
*
*****************************************************************************
*/
bool CIccLocalizedUnicode::GetText(std::string &sText)
{
  sText.clear();

  icUInt16Number* str = m_pBuf;
  icUInt16Number *str_end = m_pBuf + m_nLength;
  while ( (str < str_end) && *str ) {
    icUInt32Number code32 = 0x0;

    //UTF-16 to UTF-32

    if (*str <= 0xD7FF || *str >= 0xE000) {
      code32 = *str;
      str++;
    }
    else if (*str <= 0xDBFF) {
      icUInt16Number high = (*str - 0xD800) * 0x400;
      icUInt16Number low = *(str + 1) - 0xDC00;
      code32 = (low | high) + 0x10000;
      str += 2;
    }
    else {  //range dc00-dfff should only occur after a d800-dbfff
      str++;
      continue;
    }

    //UTF-32 to UTF-8 -------

    if (code32 <= 0x007F) {
      sText += (unsigned char)code32;
    }
    else if (code32 <= 0x07FF) {
      sText += (unsigned char)(((code32 >> 6) & 0x1F) | 0xC0);
      sText += (unsigned char)((code32 & 0x3F) | 0x80);
    }
    else if (code32 <= 0xFFFF) {
      sText += (unsigned char)(((code32 >> 12) & 0x0F) | 0xE0);
      sText += (unsigned char)(((code32 >> 6) & 0x3F) | 0x80);
      sText += (unsigned char)(((code32) & 0x3F) | 0x80);
    }
    else if (code32 <= 0x10FFFF) {
      sText += (unsigned char)(((code32 >> 18) & 0x07) | 0xF0);
      sText += (unsigned char)(((code32 >> 12) & 0x3F) | 0x80);
      sText += (unsigned char)(((code32 >> 6) & 0x3F) | 0x80);
      sText += (unsigned char)(((code32) & 0x3F) | 0x80);
    }
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::SetSize
 * 
 * Purpose: Sets the size of the string buffer.
 * 
 * Args: 
 *  nSize - length of the string
 *
 *****************************************************************************
 */
bool CIccLocalizedUnicode::SetSize(icUInt32Number nSize)
{
  if (nSize == m_nLength)
    return true;

  m_pBuf = (icUInt16Number*)icRealloc(m_pBuf, (nSize+2)*sizeof(icUInt16Number));

  if (!m_pBuf) {
    m_nLength = 0;
    return false;
  }

  m_nLength = nSize;

  m_pBuf[nSize]=0;      // safety against malformed unicode
  m_pBuf[nSize+1]=0;    // safety against malformed unicode

  return true;
}

/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  szText = zero terminated utf8 string to put in tag,
 *  nLanguageCode = the language code type as defined by icLanguageCode,
 *  nRegionCode = the region code type as defined by icCountryCode
 *****************************************************************************
 */
bool CIccLocalizedUnicode::SetText(const icChar *szText,
                                   icLanguageCode nLanguageCode/* = icLanguageCodeEnglish*/,
                                   icCountryCode nRegionCode/* = icCountryCodeUSA*/)
{
  if (!szText)
    szText = "";

  //first convert utf8 to unicode
  std::vector<unsigned long> unicode;
  size_t i = 0;
  while (szText[i])
  {
    unsigned long uni;
    size_t todo;
    unsigned char ch = szText[i++];
    if (ch <= 0x7F)
    {
      uni = ch;
      todo = 0;
    }
    else if (ch <= 0xBF)
    {
      //not a UTF-8 string so use question mark
      uni = '?';
      todo = 0;
    }
    else if (ch <= 0xDF)
    {
      uni = ch & 0x1F;
      todo = 1;
    }
    else if (ch <= 0xEF)
    {
      uni = ch & 0x0F;
      todo = 2;
    }
    else if (ch <= 0xF7)
    {
      uni = ch & 0x07;
      todo = 3;
    }
    else
    {
      //not a UTF-8 string so use question mark
      uni = '?';
      todo = 0;;
    }
    for (size_t j = 0; j < todo; ++j)
    {
      if (!szText[i]) {
        uni = '?';
        break;
      }
      else {
        unsigned char ch2 = szText[i];
        if (ch2 < 0x80 || ch2 > 0xBF) {
          //not a UTF-8 string so use question mark
          uni = '?';
          break;
        }
        else {
          uni <<= 6;
          uni += ch2 & 0x3F;
        }
      }
    }
    if (uni >= 0xD800 && uni <= 0xDFFF) {
      //not a UTF-8 string so use question mark
      uni = '?';
    }
    else if (uni > 0x10FFFF) {
      //not a UTF-8 string so use question mark
      uni = '?';
    }
    unicode.push_back(uni);
  }

  //now convert unicode to utf16
  std::vector<unsigned short> utf16;
  for (i = 0; i < unicode.size(); ++i)
  {
    unsigned long uni = unicode[i];
    if (uni <= 0xFFFF)
    {
      utf16.push_back((unsigned short)uni);
    }
    else
    {
      uni -= 0x10000;
      utf16.push_back((unsigned short)((uni >> 10) + 0xD800));
      utf16.push_back((unsigned short)((uni & 0x3FF) + 0xDC00));
    }
  }

  icUInt16Number *pBuf;

  size_t len = utf16.size();
  if (!SetSize((icUInt32Number)len+1))
    return false;
  
  pBuf = m_pBuf;
  for (i=0; i<len; i++) {
    *pBuf++ = utf16[i];
  }
  *pBuf = 0;

  m_nLanguageCode = nLanguageCode;
  m_nCountryCode = nRegionCode;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccLocalizedUnicode::SetText
 * 
 * Purpose: Allows text data associated with the tag to be set.
 * 
 * Args: 
 *  sszUnicode16Text = Unicode16 text to be set,
 *  nLanguageCode = the language code type as defined by icLanguageCode,
 *  nRegionCode = the region code type as defined by icCountryCode
 *****************************************************************************
 */
bool CIccLocalizedUnicode::SetText(const icUInt16Number *sszUnicode16Text,
                                   icLanguageCode nLanguageCode/* = icLanguageCodeEnglish*/,
                                   icCountryCode nRegionCode/* = icCountryCodeUSA*/)
{
  icUInt16Number empty[1] = { 0 };

  if (!sszUnicode16Text)
    sszUnicode16Text = &empty[0];

  const icUInt16Number *pBuf=sszUnicode16Text;
  int len;

  for (len=0; *pBuf; len++, pBuf++);

  if (!SetSize(len+1))
    return false;
  memcpy(m_pBuf, sszUnicode16Text, (len+1)*sizeof(icUInt16Number));

  m_nLanguageCode = nLanguageCode;
  m_nCountryCode = nRegionCode;

  return true;
}

/**
****************************************************************************
* Name: CIccLocalizedUnicode::SetText
* 
* Purpose: Allows text data associated with the tag to be set.
* 
* Args: 
*  sszUnicode32Text = Unicode32 text to be set,
*  nLanguageCode = the language code type as defined by icLanguageCode,
*  nRegionCode = the region code type as defined by icCountryCode
*****************************************************************************
*/
bool CIccLocalizedUnicode::SetText(const icUInt32Number *sszUnicode32Text,
                                   icLanguageCode nLanguageCode/* = icLanguageCodeEnglish*/,
                                   icCountryCode nRegionCode/* = icCountryCodeUSA*/)
{
  const icUInt32Number empty[1] = { 0 };
  if (!sszUnicode32Text)
    sszUnicode32Text = &empty[0];

  const icUInt32Number *pBuf=sszUnicode32Text;
  int len;

  for (len=0; *pBuf; len++, pBuf++);
  if (*pBuf)
    pBuf--;

  if (!SetSize(len*2))
    return false;

  const icUInt32Number *srcStart = sszUnicode32Text;
  icUInt16Number *dstStart = m_pBuf;
  icConvertUTF32toUTF16(&srcStart, &srcStart[len], &dstStart, &dstStart[len*2], lenientConversion);

  *dstStart=0;
  if (!SetSize((icUInt32Number)(dstStart - m_pBuf)))
    return false;

  m_nLanguageCode = nLanguageCode;
  m_nCountryCode = nRegionCode;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::CIccTagMultiLocalizedUnicode
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagMultiLocalizedUnicode::CIccTagMultiLocalizedUnicode()
{
  m_Strings = new(CIccMultiLocalizedUnicode);
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::CIccTagMultiLocalizedUnicode
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITMLU = The CIccTagMultiLocalizedUnicode object to be copied
 *****************************************************************************
 */
CIccTagMultiLocalizedUnicode::CIccTagMultiLocalizedUnicode(const CIccTagMultiLocalizedUnicode& ITMLU)
{
  m_Strings = new(CIccMultiLocalizedUnicode);
  *m_Strings = *ITMLU.m_Strings;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  MultiLocalizedTag = The CIccTagMultiLocalizedUnicode object to be copied
 *****************************************************************************
 */
CIccTagMultiLocalizedUnicode &CIccTagMultiLocalizedUnicode::operator=(const CIccTagMultiLocalizedUnicode &MultiLocalizedTag)
{
  if (&MultiLocalizedTag == this)
    return *this;

  m_Strings->clear();
  *m_Strings = *MultiLocalizedTag.m_Strings;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::~CIccTagMultiLocalizedUnicode
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagMultiLocalizedUnicode::~CIccTagMultiLocalizedUnicode()
{
  delete m_Strings;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::Read
 * 
 * Purpose: Read in the tag contents into a data block
 *
 * Since MultiLocalizedUnicode tags can be embedded in other tags
 * this function ensures that the current read pointer will be set to the
 * position just after the last name record.
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagMultiLocalizedUnicode::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number nNumRec, nRecSize;
  icLanguageCode nLanguageCode;
  icCountryCode nRegionCode;
  icUInt32Number nLength, nOffset, nNumChar;

  if (!m_Strings->empty())
    m_Strings->clear();

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number)*3 > size)
    return false;

  if (!pIO) {
    return false;
  }

  size_t nTagPos = pIO->Tell();
  
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read32(&nNumRec) ||
      !pIO->Read32(&nRecSize))
    return false;


  if (nRecSize!=12) { //Recognized version name records are 12 bytes each
    return false;
  }

  icUInt32Number i; 
  CIccLocalizedUnicode Unicode;
  icUInt32Number nLastPos = 0;

  for (i=0; i<nNumRec; i++) {
    if (4*sizeof(icUInt32Number) + (i+1)*12 > size)
      return false;

    pIO->Seek(nTagPos+4*sizeof(icUInt32Number) + i*12, icSeekSet);

    if (!pIO->Read16(&nLanguageCode) ||
        !pIO->Read16(&nRegionCode) ||
        !pIO->Read32(&nLength) ||
        !pIO->Read32(&nOffset))
      return false;
    
    if (nOffset+nLength > size)
      return false;

    //Find out position of the end of last named record
    if (nOffset+nLength > nLastPos)
      nLastPos = nOffset + nLength;

    nNumChar = nLength / sizeof(icUInt16Number);

    if (!Unicode.SetSize(nNumChar))
      return false;

    Unicode.m_nLanguageCode = nLanguageCode;
    Unicode.m_nCountryCode = nRegionCode;

    pIO->Seek(nTagPos+nOffset, icSeekSet);

    if (pIO->Read16(Unicode.GetBuf(), nNumChar) != nNumChar)
      return false;

    m_Strings->push_back(Unicode);
  }

  //Now seek past the last named record
  if (nLastPos > 0)
    pIO->Seek(nTagPos+nLastPos, icSeekSet);

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagMultiLocalizedUnicode::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt32Number nNumRec=(icUInt32Number)m_Strings->size(), nRecSize=12;
  icUInt32Number nLength;

  if (!pIO) {
    return false;
  }

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write32(&nNumRec) ||
      !pIO->Write32(&nRecSize))
    return false;


  icUInt32Number nPos = 4*sizeof(icUInt32Number) + nNumRec*12;

  CIccMultiLocalizedUnicode::iterator i;

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    nLength = i->GetLength() * sizeof(icUInt16Number);

    if (!pIO->Write16(&i->m_nLanguageCode) ||
        !pIO->Write16(&i->m_nCountryCode) ||
        !pIO->Write32(&nLength) ||
        !pIO->Write32(&nPos))
      return false;
    nPos += nLength;
  }

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    nLength = i->GetLength();

    if (nLength) {
      if (pIO->Write16(i->GetBuf(), nLength) != nLength)
        return false;
    }
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagMultiLocalizedUnicode::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagMultiLocalizedUnicode::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t szBufSize = 128;
  char szBuf[szBufSize];
  std::string utf8;
  CIccMultiLocalizedUnicode::iterator i;

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    if (i!=m_Strings->begin())
      sDescription += "\n";

    // Bad ICCs can have unprintables...
    sDescription += "Language = ";
    icUInt8Number c0 = i->m_nLanguageCode >> 8;
    icUInt8Number c1 = i->m_nLanguageCode & 0x00FF;
    if (isprint(c0) && isprint(c1) && (c0 <= 126) && (c1 <= 126)) {
        snprintf(szBuf, szBufSize, "'%c%c'", c0, c1);
    }
    else {
      snprintf(szBuf, szBufSize, "'\?\?' (0x%04X)", i->m_nLanguageCode);
    }
    sDescription += szBuf;

    if (i->m_nCountryCode) {
        // Region Codes are optional according to ISO 
        sDescription += ", Region = ";
        c0 = i->m_nCountryCode >> 8;
        c1 = i->m_nCountryCode & 0x00FF;
        if (isprint(c0) && isprint(c1) && (c0 <= 126) && (c1 <= 126)) {
          snprintf(szBuf, szBufSize, "'%c%c'", c0, c1);
        }
        else {
          snprintf(szBuf, szBufSize, "'\?\?' (0x%04X)", i->m_nCountryCode);
        }
        sDescription += szBuf;
    }
    sDescription += "\n";

    i->GetText(utf8);
    sDescription += "\"";
    sDescription += utf8;
    sDescription += "\"\n";
  }
}


/**
******************************************************************************
* Name: CIccTagMultiLocalizedUnicode::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagMultiLocalizedUnicode::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!m_Strings->size()) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty tag!\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }

  CIccMultiLocalizedUnicode::iterator i;
  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    // TODO: Validate ISO-639 Language Codes and and ISO-3166 Country codes.
    // Needs to be done against a full set of codes or many false warnings.
  }

  return rv;
}

/**
****************************************************************************
* Name: refIccMAX::CIccTagMultiLocalizedUnicode::Find
* 
* Purpose: 
* 
* Args:
*  nLanguageCode
*  nRegionCode
*
* Return:
*  Pointer to CIccLocalizedUnicode object associated with the nLanguageCode
*  and nRegionCode or NULL if not found
*****************************************************************************
*/
CIccLocalizedUnicode *CIccTagMultiLocalizedUnicode::Find(icLanguageCode nLanguageCode /* = icLanguageCodeEnglish */,
                                                         icCountryCode nRegionCode /* = icCountryCodeUSA */)
{
  CIccMultiLocalizedUnicode::iterator i;

  for (i=m_Strings->begin(); i!=m_Strings->end(); i++) {
    if (i->m_nLanguageCode == nLanguageCode &&
      i->m_nCountryCode == nRegionCode) {
      return &(*i);
    }
  }

  return NULL;
}

/**
****************************************************************************
* Name: refIccMAX::CIccTagMultiLocalizedUnicode::SetText
* 
* Purpose: 
* 
* Args:
*  sszUnicodeText
*  nLanguageCode
*  RegionCode
*****************************************************************************
*/
void CIccTagMultiLocalizedUnicode::SetText(const icChar *szText, 
                                           icLanguageCode nLanguageCode /* = icLanguageCodeEnglish */,
                                           icCountryCode nRegionCode /* = icCountryCodeUSA */)
{
   CIccLocalizedUnicode *pText = Find(nLanguageCode, nRegionCode);

   if (!pText) {
     CIccLocalizedUnicode newText;
     newText.SetText(szText, nLanguageCode, nRegionCode);
     m_Strings->push_back(newText);
   }
   else {
     pText->SetText(szText, nLanguageCode, nRegionCode);
   }
}


/**
****************************************************************************
* Name: refIccMAX::CIccTagMultiLocalizedUnicode::SetText
* 
* Purpose: 
* 
* Args:
*  sszUnicodeText
*  nLanguageCode
*  RegionCode
*****************************************************************************
*/
void CIccTagMultiLocalizedUnicode::SetText(const icUInt16Number *sszUnicode16Text, 
                                           icLanguageCode nLanguageCode /* = icLanguageCodeEnglish */,
                                           icCountryCode nRegionCode /* = icCountryCodeUSA */)
{
  CIccLocalizedUnicode *pText = Find(nLanguageCode, nRegionCode);

  if (!pText) {
    CIccLocalizedUnicode newText;
    newText.SetText(sszUnicode16Text, nLanguageCode, nRegionCode);
    m_Strings->push_back(newText);
  }
  else {
    pText->SetText(sszUnicode16Text, nLanguageCode, nRegionCode);
  }
}

/**
****************************************************************************
* Name: refIccMAX::CIccTagMultiLocalizedUnicode::SetText
* 
* Purpose: 
* 
* Args:
*  sszUnicodeText
*  nLanguageCode
*  RegionCode
*****************************************************************************
*/
void CIccTagMultiLocalizedUnicode::SetText(const icUInt32Number *sszUnicode32Text, 
                                           icLanguageCode nLanguageCode /* = icLanguageCodeEnglish */,
                                           icCountryCode nRegionCode /* = icCountryCodeUSA */)
{
  CIccLocalizedUnicode *pText = Find(nLanguageCode, nRegionCode);

  if (!pText) {
    CIccLocalizedUnicode newText;
    newText.SetText(sszUnicode32Text, nLanguageCode, nRegionCode);
    m_Strings->push_back(newText);
  }
  else {
    pText->SetText(sszUnicode32Text, nLanguageCode, nRegionCode);
  }
}

//
// MD: Moved Curve and LUT tags to IccTagLut.cpp (4-30-05)
//


/**
 ****************************************************************************
 * Name: CIccTagData::CIccTagData
 * 
 * Purpose: Constructor
 *
 * Args:
 *  nSize = number of data entries
 * 
 *****************************************************************************
 */
CIccTagData::CIccTagData(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_pData = (icUInt8Number*)calloc(nSize, sizeof(icUInt8Number));
}


/**
 ****************************************************************************
 * Name: CIccTagData::CIccTagData
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITD = The CIccTagData object to be copied
 *****************************************************************************
 */
CIccTagData::CIccTagData(const CIccTagData &ITD)
{
  m_nDataFlag = ITD.m_nDataFlag;
  m_nSize = ITD.m_nSize;

  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, ITD.m_pData, sizeof(icUInt8Number)*m_nSize);
}


/**
 ****************************************************************************
 * Name: CIccTagData::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  DataTag = The CIccTagData object to be copied
 *****************************************************************************
 */
CIccTagData &CIccTagData::operator=(const CIccTagData &DataTag)
{
  if (&DataTag == this)
    return *this;

  m_nDataFlag = DataTag.m_nDataFlag;
  m_nSize = DataTag.m_nSize;

  if (m_pData)
    free(m_pData);
  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, DataTag.m_pData, sizeof(icUInt8Number)*m_nSize);

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagData::~CIccTagData
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagData::~CIccTagData()
{
  if (m_pData)
    free(m_pData);
}


/**
 ****************************************************************************
 * Name: CIccTagData::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagData::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icUInt32Number) +
      sizeof(icUInt8Number) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nDataFlag))
    return false;

  icUInt32Number nNum = size-3*sizeof(icUInt32Number);

  if (!SetSize(nNum))
    return false;

  if (pIO->Read8(m_pData, nNum) != nNum)
    return false;

  if (IsTypeCompressed()) {
    //Uncompress data here
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagData::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagData::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nDataFlag))
    return false;

  if (IsTypeCompressed()) {
    icUInt32Number *pData = NULL;
    size_t nSize = 0;

// TODO, UNFINISHED - Compress data here

    if (pIO->Write8(pData, nSize) != nSize)
      return false;
  }
  else {
    if (pIO->Write8(m_pData, m_nSize) != m_nSize)
      return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagData::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagData::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  sDescription = "\n";
  if (IsTypeCompressed())
    sDescription += "Compressed ";  // If data is compressed, prepend appropriate text

  if (IsTypeAscii()) {
    sDescription = "Ascii Data:\n";

    for (int i=0; i<(int)m_nSize; i++)
      sDescription += (icChar)m_pData[i];
  }
  else if (IsTypeUtf()) {
    if (m_nSize>2 && 
        ((m_pData[0]==0xff && m_pData[1]==0xfe) ||  //UTF-16LE
         (m_pData[0]==0xfe && m_pData[1]==0xff))) { //UTF-16BE
      snprintf(buf, bufSize, "UTF-16%s Data:", m_pData[0]==0xff ? "LE" : "BE" );
      sDescription = buf;

      icMemDump(sDescription, m_pData, m_nSize);
    }
    else { //UTF-8
      sDescription = "UTF-8 Data:\n";

      for (int i=0; i<(int)m_nSize; i++)
        sDescription += (icChar)m_pData[i];
    }
  }
  else {
    if (IsTypeBinary()) {
      sDescription = "Binary Data:\n";
    }
    else {
      sDescription = "Other Data:\n";
    }

    icMemDump(sDescription, m_pData, m_nSize);
  }
  sDescription += "\n";
}

/**
 ****************************************************************************
 * Name: CIccTagData::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of data entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
bool CIccTagData::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nSize == nSize)
    return true;

  m_pData = (icUInt8Number*)icRealloc(m_pData, nSize*sizeof(icUInt8Number));

  if (!m_pData) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nSize) {
    memset(&m_pData[m_nSize], 0, (nSize-m_nSize)*sizeof(icUInt8Number));
  }
  m_nSize = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagData::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagData::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  // Mask bits to match processing in Describe() so warnings are appropriate
  switch(m_nDataFlag&(icCompressedData|icDataTypeMask)) {
  case icAsciiData:
    for (int i = 0; i < (int)m_nSize; i++) {
      if (m_pData[i] & 0x80) {
        sReport += icMsgValidateWarning;
        sReport += sSigPathName;
        sReport += " - Ascii Data is not just 7-bit data.\n";
        rv = icMaxStatus(rv, icValidateWarning);
        break;
      }
    }
    break;
  case icBinaryData:
  case icUtfData:
  case icCompressedAsciiData:
  case icCompressedBinaryData:
  case icCompressedUtfData:
    break;
  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid data flag encoding.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }
  
  if (m_nDataFlag & ~(icCompressedData | icDataTypeMask)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Invalid data flag encoding (reserved high order bits).\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagDateTime::CIccTagDateTime
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagDateTime::CIccTagDateTime()
{
  memset(&m_DateTime, 0, sizeof(m_DateTime));
}


/**
 ****************************************************************************
 * Name: CIccTagDateTime::CIccTagDateTime
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITDT = The CIccTagDateTime object to be copied
 *****************************************************************************
 */
CIccTagDateTime::CIccTagDateTime(const CIccTagDateTime &ITDT)
{
  memcpy(&m_DateTime, &ITDT.m_DateTime, sizeof(m_DateTime));
}


/**
 ****************************************************************************
 * Name: CIccTagDateTime::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  DateTimeTag = The CIccTagDateTime object to be copied
 *****************************************************************************
 */
CIccTagDateTime &CIccTagDateTime::operator=(const CIccTagDateTime &DateTimeTag)
{
  if (&DateTimeTag == this)
    return *this;

  memcpy(&m_DateTime, &DateTimeTag.m_DateTime, sizeof(m_DateTime));

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagDateTime::~CIccTagDateTime
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagDateTime::~CIccTagDateTime()
{
}



/**
 ****************************************************************************
 * Name: CIccTagDateTime::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagDateTime::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icDateTimeNumber) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;


  size_t nsize = (size-2*sizeof(icUInt32Number))/sizeof(icUInt16Number);

  if (nsize > sizeof(m_DateTime) / sizeof(icUInt16Number))
    return false;

  if (pIO->Read16(&m_DateTime,nsize) != nsize)
    return false;

  return true;
}



/**
 ****************************************************************************
 * Name: CIccTagDateTime::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagDateTime::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (pIO->Write16(&m_DateTime,6) != 6)
    return false;
  
  return true;
}



/**
 ****************************************************************************
 * Name: CIccTagDateTime::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagDateTime::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  sDescription = "Date = ";
  snprintf(buf, bufSize, "%u-%u-%u\n", m_DateTime.month, m_DateTime.day, m_DateTime.year);
  sDescription += buf;
  
  sDescription += "Time = ";
  snprintf(buf, bufSize, "%u:%u:%u\n", m_DateTime.hours, m_DateTime.minutes, m_DateTime.seconds);
  sDescription += buf;
}


/**
******************************************************************************
* Name: CIccTagDateTime::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagDateTime::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);
  CIccInfo Info;

  rv = icMaxStatus(rv, Info.CheckData(sReport, m_DateTime));
  
  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::CIccTagColorantOrder
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nSize = number of channels
 * 
 *****************************************************************************
 */
CIccTagColorantOrder::CIccTagColorantOrder(int nsize/*=1*/)
{
  m_nCount = nsize;
  if (m_nCount <1)
    m_nCount = 1;
  m_pData = (icUInt8Number*)calloc(nsize, sizeof(icUInt8Number));
}



/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::CIccTagColorantOrder
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITCO = The CIccTagColorantOrder object to be copied
 *****************************************************************************
 */
CIccTagColorantOrder::CIccTagColorantOrder(const CIccTagColorantOrder &ITCO)
{
  m_nCount = ITCO.m_nCount;

  m_pData = (icUInt8Number*)calloc(m_nCount, sizeof(icUInt8Number));
  memcpy(m_pData, ITCO.m_pData, sizeof(icUInt8Number)*m_nCount);
}


/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ColorantOrderTag = The CIccTagColorantOrder object to be copied
 *****************************************************************************
 */
CIccTagColorantOrder &CIccTagColorantOrder::operator=(const CIccTagColorantOrder &ColorantOrderTag)
{
  if (&ColorantOrderTag == this)
    return *this;

  m_nCount = ColorantOrderTag.m_nCount;

  if (m_pData)
    free(m_pData);
  m_pData = (icUInt8Number*)calloc(m_nCount, sizeof(icUInt8Number));
  memcpy(m_pData, ColorantOrderTag.m_pData, sizeof(icUInt8Number)*m_nCount);

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::~CIccTagColorantOrder
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagColorantOrder::~CIccTagColorantOrder()
{
  if (m_pData)
    free(m_pData);
}



/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagColorantOrder::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number nCount;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icUInt32Number) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&nCount))
    return false;

  icUInt32Number nNum = (size - 3*sizeof(icUInt32Number))/sizeof(icUInt8Number);

  if (nNum < nCount)
    return false;

  if (!SetSize((icUInt16Number)nCount))
    return false;

  if (pIO->Read8(&m_pData[0], m_nCount) != m_nCount)
    return false;

  return true;
}



/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagColorantOrder::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nCount))
    return false;

  if (pIO->Write8(&m_pData[0], m_nCount) != m_nCount)
    return false;
  
  return true;
}



/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagColorantOrder::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "Colorant Count : %u\n", m_nCount);
  sDescription += buf;
  sDescription += "Order of Colorants:\n";
  
  for (int i=0; i<(int)m_nCount; i++) {
    snprintf(buf, bufSize, "%u\n", m_pData[i]);
    sDescription += buf;
  }
}


/**
 ****************************************************************************
 * Name: CIccTagColorantOrder::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of channels,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
bool CIccTagColorantOrder::SetSize(icUInt16Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nCount == nSize)
    return true;

  m_pData = (icUInt8Number*)icRealloc(m_pData, nSize*sizeof(icUInt8Number));

  if (!m_pData) {
    m_nCount = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nCount) {
    memset(&m_pData[m_nCount], 0, (nSize - m_nCount)*sizeof(icUInt8Number));
  }

  m_nCount = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagColorantOrder::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagColorantOrder::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!pProfile) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  if (sigPath==icGetSigPath(icSigColorantTableTag)) {
    if (m_nCount != icGetSpaceSamples(pProfile->m_Header.colorSpace)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Incorrect number of colorants.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else if (sigPath==icGetSigPath(icSigColorantTableOutTag)) {
    if (m_nCount != icGetSpaceSamples(pProfile->m_Header.pcs)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Incorrect number of colorants.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Unknown number of required colorants.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }

  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagColorantTable::CIccTagColorantTable
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nSize = number of entries
 * 
 *****************************************************************************
 */
CIccTagColorantTable::CIccTagColorantTable(int nSize/*=1*/)
{
  m_nCount = nSize;
  if (m_nCount<1)
    m_nCount = 1;

  m_pData = (icColorantTableEntry*)calloc(nSize, sizeof(icColorantTableEntry));
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::CIccTagColorantTable
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITCT = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccTagColorantTable::CIccTagColorantTable(const CIccTagColorantTable &ITCT)
{
  m_PCS = ITCT.m_PCS;
  m_nCount = ITCT.m_nCount;

  m_pData = (icColorantTableEntry*)calloc(m_nCount, sizeof(icColorantTableEntry));
  memcpy(m_pData, ITCT.m_pData, m_nCount*sizeof(icColorantTableEntry));
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ColorantTableTag = The CIccTagColorantTable object to be copied
 *****************************************************************************
 */
CIccTagColorantTable &CIccTagColorantTable::operator=(const CIccTagColorantTable &ColorantTableTag)
{
  if (&ColorantTableTag == this)
    return *this;

  m_PCS = ColorantTableTag.m_PCS;
  m_nCount = ColorantTableTag.m_nCount;

  if (m_pData)
    free(m_pData);
  m_pData = (icColorantTableEntry*)calloc(m_nCount, sizeof(icColorantTableEntry));
  memcpy(m_pData, ColorantTableTag.m_pData, m_nCount*sizeof(icColorantTableEntry));

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::~CIccTagColorantTable
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagColorantTable::~CIccTagColorantTable()
{
  if (m_pData)
    free(m_pData);
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagColorantTable::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number nCount;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number) + 
      sizeof(icUInt32Number) +
      sizeof(icColorantTableEntry) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&nCount))
    return false;

  icUInt32Number nNum = (size - 3*sizeof(icUInt32Number))/sizeof(icColorantTableEntry);
  size_t nNum8 = sizeof(m_pData->name);
  size_t nNum16 = sizeof(m_pData->data)/sizeof(icUInt16Number);

  if (nNum < nCount || nCount > 0xffff)
    return false;
  
  if (!SetSize((icUInt16Number)nCount))
    return false;

  for (icUInt32Number i=0; i<nCount; i++) {
    if (pIO->Read8(&m_pData[i].name[0], nNum8) != nNum8)
      return false;

    if (pIO->Read16(&m_pData[i].data[0], nNum16) != nNum16)
      return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagColorantTable::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nCount))
    return false;

  size_t nNum8 = sizeof(m_pData->name);
  size_t nNum16 = sizeof(m_pData->data)/sizeof(icUInt16Number);

  for (icUInt32Number i=0; i<m_nCount; i++) {
    if (pIO->Write8(&m_pData[i].name[0],nNum8) != nNum8)
      return false;

    if (pIO->Write16(&m_pData[i].data[0],nNum16) != nNum16)
      return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagColorantTable::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagColorantTable::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  icUInt32Number i, nLen, nMaxLen=0;
  icFloatNumber Lab[3] = {0};

  snprintf(buf, bufSize, "BEGIN_COLORANTS %u\n", m_nCount);
  sDescription += buf;

  for (i=0; i<m_nCount; i++) {
    nLen = (icUInt32Number)strlen(m_pData[i].name);
    if (nLen>nMaxLen)
      nMaxLen =nLen;
  }
  sDescription += "# NAME ";

  if (m_PCS == icSigXYZData) {
    snprintf(buf, bufSize, "XYZ_X XYZ_Y XYZ_Z\n");
    sDescription += buf;
  }
  else {
    snprintf(buf, bufSize, "Lab_L Lab_a Lab_b\n");
    sDescription += buf;
  }
  for (i=0; i<m_nCount; i++) {
    snprintf(buf, bufSize, "%2u \"%s\"", i, m_pData[i].name);
    sDescription += buf;
    memset(buf, ' ', 128);
    buf[nMaxLen + 1 - strlen(m_pData[i].name)] ='\0';
    sDescription += buf;

    if (m_PCS == icSigXYZData) {
      snprintf(buf, bufSize, "%7.4lf %7.4lf %7.4lf\n", icUSFtoD(m_pData[i].data[0]), icUSFtoD(m_pData[i].data[1]), icUSFtoD(m_pData[i].data[2]));
      sDescription += buf;
    }
    else {
      Lab[0] = icU16toF(m_pData[i].data[0]);
      Lab[1] = icU16toF(m_pData[i].data[1]);
      Lab[2] = icU16toF(m_pData[i].data[2]);
      icLabFromPcs(Lab);
      snprintf(buf, bufSize, "%7.4lf %8.4lf %8.4lf\n", Lab[0], Lab[1], Lab[2]);
      sDescription += buf;
    }
  }

}

/**
 ****************************************************************************
 * Name: CIccTagColorantTable::SetSize
 * 
 * Purpose: Sets the size of the data array.
 * 
 * Args: 
 *  nSize - number of entries,
 *  bZeroNew - flag to zero newly formed values
 *****************************************************************************
 */
bool CIccTagColorantTable::SetSize(icUInt16Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nCount == nSize)
    return true;

  m_pData = (icColorantTableEntry*)icRealloc(m_pData, nSize*sizeof(icColorantTableEntry));

  if (!m_pData) {
    m_nCount = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nCount) {
    memset(&m_pData[m_nCount], 0, (nSize-m_nCount)*sizeof(icColorantTableEntry));
  }
  m_nCount = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagColorantTable::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagColorantTable::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (!pProfile) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }


  if (sig==icSigColorantTableOutTag) {
    if (pProfile->m_Header.deviceClass!=icSigLinkClass) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Use of this tag is allowed only in DeviceLink Profiles.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
    if (m_nCount != icGetSpaceSamples(pProfile->m_Header.pcs)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Incorrect number of colorants.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else {
    if (m_nCount != icGetSpaceSamples(pProfile->m_Header.colorSpace)) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Incorrect number of colorants.\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::CIccTagViewingConditions
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagViewingConditions::CIccTagViewingConditions()
{
  m_XYZIllum.X = 0;
  m_XYZIllum.Y = 0;
  m_XYZIllum.Z = 0;

  m_XYZSurround.X = 0;
  m_XYZSurround.Y = 0;
  m_XYZSurround.Z = 0;

  m_illumType = icIlluminantUnknown;
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::CIccTagViewingConditions
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITVC = The CIccTagViewingConditions object to be copied
 *****************************************************************************
 */
CIccTagViewingConditions::CIccTagViewingConditions(const CIccTagViewingConditions &ITVC)
{
  m_illumType = ITVC.m_illumType;

  memcpy(&m_XYZIllum, &ITVC.m_XYZIllum, sizeof(icXYZNumber));
  memcpy(&m_XYZSurround, &ITVC.m_XYZSurround, sizeof(icXYZNumber));
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ViewCondTag = The CIccTagViewingConditions object to be copied
 *****************************************************************************
 */
CIccTagViewingConditions &CIccTagViewingConditions::operator=(const CIccTagViewingConditions &ViewCondTag)
{
  if (&ViewCondTag == this)
    return *this;

  m_illumType = ViewCondTag.m_illumType;

  memcpy(&m_XYZIllum, &ViewCondTag.m_XYZIllum, sizeof(icXYZNumber));
  memcpy(&m_XYZSurround, &ViewCondTag.m_XYZSurround, sizeof(icXYZNumber));

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::~CIccTagViewingConditions
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagViewingConditions::~CIccTagViewingConditions()
{
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagViewingConditions::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 
      2*sizeof(icUInt32Number) + 
      2*sizeof(icXYZNumber) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (pIO->Read32(&m_XYZIllum.X, 3) != 3)
    return false;

  if (pIO->Read32(&m_XYZSurround.X, 3) != 3)
    return false;

  if (!pIO->Read32(&m_illumType))
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagViewingConditions::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (pIO->Write32(&m_XYZIllum.X, 3) !=3)
    return false;

  if (pIO->Write32(&m_XYZSurround.X, 3) !=3)
    return false;

  if (!pIO->Write32(&m_illumType))
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagViewingConditions::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagViewingConditions::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo Fmt;

  snprintf(buf, bufSize, "Illuminant Tristimulus values: X = %.4lf, Y = %.4lf, Z = %.4lf\n",
               icFtoD(m_XYZIllum.X),
               icFtoD(m_XYZIllum.Y),
               icFtoD(m_XYZIllum.Z));
  sDescription += buf;

  snprintf(buf, bufSize, "Surround Tristimulus values: X = %.4lf, Y = %.4lf, Z = %.4lf\n",
               icFtoD(m_XYZSurround.X),
               icFtoD(m_XYZSurround.Y),
               icFtoD(m_XYZSurround.Z));
  sDescription += buf;

  sDescription += "Illuminant Type: ";

  sDescription += Fmt.GetIlluminantName(m_illumType);
  sDescription += "\n";

}


/**
******************************************************************************
* Name: CIccTagViewingConditions::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagViewingConditions::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  rv = icMaxStatus(rv, Info.CheckData(sReport, m_XYZIllum, "XYZ Illuminant"));
  rv = icMaxStatus(rv, Info.CheckData(sReport, m_XYZSurround, "XYZ Surround"));

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::CIccProfileDescText
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccProfileDescText::CIccProfileDescText()
{
  m_pTag = NULL;
  m_bNeedsPading = false;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::CIccProfileDescText
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  IPDC = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccProfileDescText::CIccProfileDescText(const CIccProfileDescText &IPDC)
{
  if (IPDC.m_pTag) {
    m_pTag = IPDC.m_pTag->NewCopy();
    m_bNeedsPading = IPDC.m_bNeedsPading;
  }
  else {
    m_pTag = NULL;
    m_bNeedsPading = false;
  }
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ProfDescText = The CIccProfileDescText object to be copied
 *****************************************************************************
 */
CIccProfileDescText &CIccProfileDescText::operator=(const CIccProfileDescText &ProfDescText)
{
  if (&ProfDescText == this)
    return *this;

  if (m_pTag)
    delete m_pTag;

  if (ProfDescText.m_pTag) {
    m_pTag = ProfDescText.m_pTag->NewCopy();
    m_bNeedsPading = ProfDescText.m_bNeedsPading;
  }
  else {
    m_pTag = NULL;
    m_bNeedsPading = false;
  }

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::~CIccProfileDescText
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccProfileDescText::~CIccProfileDescText()
{
  if (m_pTag)
    delete m_pTag;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::SetType
 * 
 * Purpose: Sets the type of the profile description text. Could be either 
 *  a MultiLocalizedUnicodeType or a TextDescriptionType.
 * 
 * Args:
 *  nType = the tag type signature
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccProfileDescText::SetType(icTagTypeSignature nType)
{
  if (m_pTag) {
    if (m_pTag->GetType() == nType)
      return true;

    delete m_pTag;
  }

  if (nType == icSigMultiLocalizedUnicodeType ||
      nType == icSigTextDescriptionType)
    m_pTag = CIccTag::Create(nType);
  else
    m_pTag = NULL;

  return(m_pTag != NULL);
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::SetType
 * 
 * Purpose: Gets the type of the profile description text. Could be either 
 *  a MultiLocalizedUnicodeType or a TextDescriptionType.
 * 
 *****************************************************************************
 */
icTagTypeSignature CIccProfileDescText::GetType() const
{
  if (m_pTag)
    return m_pTag->GetType();

  return icSigUnknownType;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccProfileDescText::Describe(std::string &sDescription, int nVerboseness)
{
  if (m_pTag)
    m_pTag->Describe(sDescription, nVerboseness);
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccProfileDescText::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  //Check for description tag type signature
  size_t nPos = pIO->Tell();

  if ((nPos&0x03) != 0)
    m_bNeedsPading = true;

  if (!pIO->Read32(&sig))
    return false;
  pIO->Seek(nPos, icSeekSet);

  if (sig==icSigTextDescriptionType)
    m_bNeedsPading = false;

  if (!SetType(sig)) {
    //We couldn't find it, but we may be looking in the wrong place
    //Re-Sync to a 4 byte boundary
    pIO->Sync32();

    nPos = pIO->Tell();
    if (!pIO->Read32(&sig))
      return false;
    pIO->Seek(nPos, icSeekSet);

    if (!SetType(sig)) {
      return false;
    }
  }

  if (m_pTag) {
    return m_pTag->Read(size, pIO);
  }
 
  return false;
}


/**
 ****************************************************************************
 * Name: CIccProfileDescText::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccProfileDescText::Write(CIccIO *pIO)
{
  if (!m_pTag)
    return false;

  if (m_pTag->Write(pIO)) {
    if (m_pTag->GetType() != icSigTextDescriptionType)
      return pIO->Align32();
    else
      return true;
  }
  
  return false;
}



/**
 ****************************************************************************
 * Name: CIccProfileDescStruct::CIccProfileDescStruct
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccProfileDescStruct::CIccProfileDescStruct()
{
}


/**
 ****************************************************************************
 * Name: CIccProfileDescStruct::CIccProfileDescStruct
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  IPDS = The CIccProfileDescStruct object to be copied
 *****************************************************************************
 */
CIccProfileDescStruct::CIccProfileDescStruct(const CIccProfileDescStruct &IPDS)
{
  m_deviceMfg = IPDS.m_deviceMfg;
  m_deviceModel = IPDS.m_deviceModel;
  m_attributes = IPDS.m_attributes;
  m_technology = IPDS.m_technology;
  m_deviceMfgDesc = IPDS.m_deviceMfgDesc;
  m_deviceModelDesc = IPDS.m_deviceModelDesc;
}



/**
 ****************************************************************************
 * Name: CIccProfileDescStruct::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ProfDescStruct = The CIccProfileDescStruct object to be copied
 *****************************************************************************
 */
CIccProfileDescStruct &CIccProfileDescStruct::operator=(const CIccProfileDescStruct &ProfDescStruct)
{
  if (&ProfDescStruct == this)
    return *this;

  m_deviceMfg = ProfDescStruct.m_deviceMfg;
  m_deviceModel = ProfDescStruct.m_deviceModel;
  m_attributes = ProfDescStruct.m_attributes;
  m_technology = ProfDescStruct.m_technology;
  m_deviceMfgDesc = ProfDescStruct.m_deviceMfgDesc;
  m_deviceModelDesc = ProfDescStruct.m_deviceModelDesc;

  return *this;
}



/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::CIccTagProfileSeqDesc
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagProfileSeqDesc::CIccTagProfileSeqDesc()
{
  m_Descriptions = new(CIccProfileSeqDesc);
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::CIccTagProfileSeqDesc
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITPSD = The CIccTagProfileSeqDesc object to be copied
 *****************************************************************************
 */
CIccTagProfileSeqDesc::CIccTagProfileSeqDesc(const CIccTagProfileSeqDesc &ITPSD)
{
  m_Descriptions = new(CIccProfileSeqDesc);
  *m_Descriptions = *ITPSD.m_Descriptions;
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  ProfSeqDescTag = The CIccTagProfileSeqDesc object to be copied
 *****************************************************************************
 */
CIccTagProfileSeqDesc &CIccTagProfileSeqDesc::operator=(const CIccTagProfileSeqDesc &ProfSeqDescTag)
{
  if (&ProfSeqDescTag == this)
    return *this;

  *m_Descriptions = *ProfSeqDescTag.m_Descriptions;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::~CIccTagProfileSeqDesc
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagProfileSeqDesc::~CIccTagProfileSeqDesc()
{
  delete m_Descriptions;
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagProfileSeqDesc::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;
  icUInt32Number nCount;

  size_t nEnd = pIO->Tell() + size;

  if (sizeof(icTagTypeSignature) + 
      sizeof(icUInt32Number)*2 > size)
    return false;

  if (!pIO) {
    return false;
  }
 
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved) ||
      !pIO->Read32(&nCount))
    return false;

  if (!nCount)
    return true;

  if (sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number)*2 +
    sizeof(CIccProfileDescStruct) > size)
    return false;

  icUInt32Number i;
  size_t nPos;
  CIccProfileDescStruct ProfileDescStruct;

  for (i=0; i<nCount; i++) {

    if (!pIO->Read32(&ProfileDescStruct.m_deviceMfg) ||
        !pIO->Read32(&ProfileDescStruct.m_deviceModel) ||
        !pIO->Read64(&ProfileDescStruct.m_attributes) ||
        !pIO->Read32(&ProfileDescStruct.m_technology))
      return false;

    nPos = pIO->Tell();

    if (!ProfileDescStruct.m_deviceMfgDesc.Read((icUInt32Number)(nEnd - nPos), pIO))
      return false;
    
    nPos = pIO->Tell();
    if (!ProfileDescStruct.m_deviceModelDesc.Read((icUInt32Number)(nEnd - nPos), pIO))
      return false;

    m_Descriptions->push_back(ProfileDescStruct);
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagProfileSeqDesc::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt32Number nCount=(icUInt32Number)m_Descriptions->size();

  if (!pIO) {
    return false;
  }

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved) ||
      !pIO->Write32(&nCount))
    return false;

  CIccProfileSeqDesc::iterator i;

  for (i=m_Descriptions->begin(); i!=m_Descriptions->end(); i++) {

    if (!pIO->Write32(&i->m_deviceMfg) ||
        !pIO->Write32(&i->m_deviceModel) ||
        !pIO->Write64(&i->m_attributes) ||
        !pIO->Write32(&i->m_technology))
      return false;

    if (!i->m_deviceMfgDesc.Write(pIO) ||
        !i->m_deviceModelDesc.Write(pIO))
      return false;
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagProfileSeqDesc::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagProfileSeqDesc::Describe(std::string &sDescription, int nVerboseness)
{
  CIccProfileSeqDesc::iterator i;
  const size_t bufSize = 128;
  icChar buf[bufSize], buf2[28];
  icUInt32Number count=0;

  snprintf(buf, bufSize, "Number of Profile Description Structures: %u\n", (icUInt32Number)m_Descriptions->size());
  sDescription += buf;

  for (i=m_Descriptions->begin(); i!=m_Descriptions->end(); i++, count++) {
    sDescription += "\n";

    snprintf(buf, bufSize, "Profile Description Structure Number [%u] follows:\n", count+1);
    sDescription += buf;

    snprintf(buf, bufSize, "Device Manufacturer Signature: %s\n", icGetSig(buf2, i->m_deviceMfg, false));
    sDescription += buf;

    snprintf(buf, bufSize, "Device Model Signature: %s\n", icGetSig(buf2, i->m_deviceModel, false));
    sDescription += buf;

    snprintf(buf, bufSize, "Device Attributes: %08x%08x\n", (icUInt32Number)(i->m_attributes >> 32), (icUInt32Number)(i->m_attributes));
    sDescription += buf;

    snprintf(buf, bufSize, "Device Technology Signature: %s\n", icGetSig(buf2, i->m_technology, false));
    sDescription += buf;

    snprintf(buf, bufSize, "Description of device manufacturer: \n");
    sDescription += buf;
    i->m_deviceMfgDesc.Describe(sDescription, nVerboseness);

    snprintf(buf, bufSize, "Description of device model: \n");
    sDescription += buf;
    i->m_deviceModelDesc.Describe(sDescription, nVerboseness);
  }
}


/**
******************************************************************************
* Name: CIccTagProfileSeqDesc::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagProfileSeqDesc::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  const size_t bufSize = 128;
  icChar buf[bufSize];
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  CIccProfileSeqDesc::iterator i;
  for (i=m_Descriptions->begin(); i!=m_Descriptions->end(); i++) {
    switch(i->m_technology) {
    case icSigUndefined:  //Technology not defined
    case icSigFilmScanner:
    case icSigDigitalCamera:
    case icSigReflectiveScanner:
    case icSigInkJetPrinter:
    case icSigThermalWaxPrinter:
    case icSigElectrophotographicPrinter:
    case icSigElectrostaticPrinter:
    case icSigDyeSublimationPrinter:
    case icSigPhotographicPaperPrinter:
    case icSigFilmWriter:
    case icSigVideoMonitor:
    case icSigVideoCamera:
    case icSigProjectionTelevision:
    case icSigCRTDisplay:
    case icSigPMDisplay:
    case icSigAMDisplay:
    case icSigPhotoCD:
    case icSigPhotoImageSetter:
    case icSigGravure:
    case icSigOffsetLithography:
    case icSigSilkscreen:
    case icSigFlexography:
      break;

    default:
      {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        snprintf(buf, bufSize, " - %s: Unknown Technology.\n", Info.GetSigName(i->m_technology));
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }

    if (i->m_deviceMfgDesc.m_bNeedsPading) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;

      sReport += " Contains non-aligned deviceMfgDesc text tag information\n";

      rv = icMaxStatus(rv, icValidateNonCompliant);
    }

    if (i->m_deviceModelDesc.m_bNeedsPading) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;

      sReport += " Contains non-aligned deviceModelDesc text tag information\n";

      rv = icMaxStatus(rv, icValidateNonCompliant);
    }

    rv = icMaxStatus(rv, i->m_deviceMfgDesc.GetTag()->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
    rv = icMaxStatus(rv, i->m_deviceModelDesc.GetTag()->Validate(sigPath+icGetSigPath(GetType()), sReport, pProfile));
  }  

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::CIccResponseCurveStruct
 * 
 * Purpose: Constructor
 * 
 * Args:
 *  nChannels = number of channels
 * 
 *****************************************************************************
 */
CIccResponseCurveStruct::CIccResponseCurveStruct(icUInt16Number nChannels/*=0*/)
{
  m_nChannels = nChannels;
  m_maxColorantXYZ = (icXYZNumber*)calloc(nChannels, sizeof(icXYZNumber));
  m_Response16ListArray = new CIccResponse16List[nChannels];
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::CIccResponseCurveStruct
 * 
 * Purpose: Constructor
 *
 * Args:
 *  sig = measurement unit signature indicating the type of measurement data,
 *  nChannels = number of channels
 *****************************************************************************
 */
CIccResponseCurveStruct::CIccResponseCurveStruct(icMeasurementUnitSig sig,icUInt16Number nChannels/*=0*/)
{
  m_nChannels = nChannels;
  m_measurementUnitSig = sig;
  m_maxColorantXYZ = (icXYZNumber*)calloc(nChannels, sizeof(icXYZNumber));
  m_Response16ListArray = new CIccResponse16List[nChannels];
}

/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::CIccResponseCurveStruct
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  IRCS = The CIccTagUnknown object to be copied
 *****************************************************************************
 */
CIccResponseCurveStruct::CIccResponseCurveStruct(const CIccResponseCurveStruct &IRCS)
{
  m_nChannels = IRCS.m_nChannels;
  m_measurementUnitSig = IRCS.m_measurementUnitSig;

  m_maxColorantXYZ = (icXYZNumber*)calloc(m_nChannels, sizeof(icXYZNumber));
  memcpy(m_maxColorantXYZ, IRCS.m_maxColorantXYZ, m_nChannels*sizeof(icXYZNumber));

  m_Response16ListArray = new CIccResponse16List[m_nChannels];
  for (icUInt32Number i=0; i<m_nChannels; i++)
    m_Response16ListArray[i] = IRCS.m_Response16ListArray[i];
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  RespCurveStruct = The CIccResponseCurveStruct object to be copied
 *****************************************************************************
 */
CIccResponseCurveStruct &CIccResponseCurveStruct::operator=(const CIccResponseCurveStruct &RespCurveStruct)
{
  if (&RespCurveStruct == this)
    return *this;

  m_nChannels = RespCurveStruct.m_nChannels;
  m_measurementUnitSig = RespCurveStruct.m_measurementUnitSig;

  if (m_maxColorantXYZ)
    free(m_maxColorantXYZ);

  m_maxColorantXYZ = (icXYZNumber*)calloc(m_nChannels, sizeof(icXYZNumber));
  memcpy(m_maxColorantXYZ, RespCurveStruct.m_maxColorantXYZ, m_nChannels*sizeof(icXYZNumber));

  if (m_Response16ListArray)
    delete [] m_Response16ListArray;
  m_Response16ListArray = new CIccResponse16List[m_nChannels];
  for (icUInt32Number i=0; i<m_nChannels; i++)
    m_Response16ListArray[i] = RespCurveStruct.m_Response16ListArray[i];

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::~CIccResponseCurveStruct
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccResponseCurveStruct::~CIccResponseCurveStruct()
{
  if (m_maxColorantXYZ)
    free(m_maxColorantXYZ);

  if (m_Response16ListArray)
    delete [] m_Response16ListArray;
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccResponseCurveStruct::Read(icUInt32Number size, CIccIO *pIO)
{
  if (!m_nChannels)
    return false;

  if (sizeof(icTagTypeSignature) + 
      m_nChannels * (sizeof(icUInt32Number) +
        sizeof(icXYZNumber) + 
        sizeof(icResponse16Number)) > size)
    return false;

  if (!pIO) {
    return false;
  }
 
  if (!pIO->Read32(&m_measurementUnitSig))
    return false;

  icUInt32Number* nMeasurements = new icUInt32Number[m_nChannels];

  if (pIO->Read32(&nMeasurements[0],m_nChannels) != m_nChannels) {
    delete[] nMeasurements;
    return false;
  }

  icUInt32Number nNum32 = m_nChannels*sizeof(icXYZNumber)/sizeof(icS15Fixed16Number);
  if (pIO->Read32(&m_maxColorantXYZ[0], nNum32) != nNum32) {
    delete[] nMeasurements;
    return false;
  }

  icResponse16Number nResponse16 = {};
  CIccResponse16List nResponseList;

  for (int i = 0; i<m_nChannels; i++) {
    if (!nResponseList.empty())
      nResponseList.clear();
    for (int j=0; j<(int)nMeasurements[i]; j++) {
      if (!pIO->Read16(&nResponse16.deviceCode) ||
         !pIO->Read16(&nResponse16.reserved)   ||
         !pIO->Read32(&nResponse16.measurementValue)) {
        delete[] nMeasurements;
        return false;
      }
      nResponseList.push_back(nResponse16);
    }
    m_Response16ListArray[i] = nResponseList;
  }

  delete [] nMeasurements;
  return true;
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccResponseCurveStruct::Write(CIccIO *pIO)
{
  if (!m_nChannels)
    return false;
  
  icMeasurementUnitSig sig = GetMeasurementType();

  if (!pIO) {
    return false;
  }

  if (!pIO->Write32(&sig))
    return false;

  if (m_nChannels) {

    icUInt32Number* nMeasurements = new icUInt32Number[m_nChannels];
    for (int i=0; i<m_nChannels; i++)
      nMeasurements[i] = (icUInt32Number)m_Response16ListArray[i].size();

    if (pIO->Write32(&nMeasurements[0],m_nChannels) != m_nChannels)
      return false;
    delete [] nMeasurements;

    icUInt32Number nNum32 = m_nChannels*sizeof(icXYZNumber)/sizeof(icS15Fixed16Number);
    if (pIO->Write32(&m_maxColorantXYZ[0], nNum32) != nNum32)
      return false;
  }
  else
    return false;

  CIccResponse16List nResponseList;
  CIccResponse16List::iterator j;

  for (int i = 0; i<m_nChannels; i++) {
    nResponseList = m_Response16ListArray[i];
    for (j=nResponseList.begin(); j!=nResponseList.end(); j++) {
      if (!pIO->Write16(&j->deviceCode) ||
         !pIO->Write16(&j->reserved)   ||
         !pIO->Write32(&j->measurementValue))
        return false;
    }
    nResponseList.clear();
  }

  return true;
}


/**
 ****************************************************************************
 * Name: CIccResponseCurveStruct::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccResponseCurveStruct::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo Fmt;
  CIccResponse16List nResponseList;
  CIccResponse16List::iterator j;

  sDescription += "Measurement Unit: ";
  sDescription += Fmt.GetMeasurementUnit((icSignature)GetMeasurementType()); sDescription += "\n";

  
  for (int i=0; i<m_nChannels; i++) {
    nResponseList = m_Response16ListArray[i];

    sDescription += "\n";
    snprintf(buf, bufSize, "Maximum Colorant XYZ Measurement for Channel-%u : X=%.4lf, Y=%.4lf, Z=%.4lf\n", i+1,
      icFtoD(m_maxColorantXYZ[i].X), icFtoD(m_maxColorantXYZ[i].Y), icFtoD(m_maxColorantXYZ[i].Z));
    sDescription += buf;

    snprintf(buf, bufSize, "Number of Measurements for Channel-%u : %u\n", i+1, (icUInt32Number)nResponseList.size());
    sDescription += buf;

    snprintf(buf, bufSize, "Measurement Data for Channel-%u follows:\n", i+1);
    sDescription += buf;

    for (j=nResponseList.begin(); j!=nResponseList.end(); j++) {
      snprintf(buf, bufSize, "Device Value= %u : Measurement Value= %.4lf\n", j->deviceCode, icFtoD(j->measurementValue));
      sDescription += buf;
    }
  }
}


/**
******************************************************************************
* Name: CIccResponseCurveStruct::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccResponseCurveStruct::Validate(std::string &sReport)
{
  icValidateStatus rv = icValidateOK;

  CIccInfo Info;
  std::string sSigName = Info.GetSigName(m_measurementUnitSig);
  switch(m_measurementUnitSig) {
  case icSigStatusA:
  case icSigStatusE:
  case icSigStatusI:
  case icSigStatusT:
  case icSigStatusM:
  case icSigDN:
  case icSigDNP:
  case icSigDNN:
  case icSigDNNP:
    break;

  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigName;
    sReport += " - Unknown measurement unit signature.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (!m_nChannels) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigName;
    sReport += " - Incorrect number of channels.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
    return rv;
  }
  for (int i=0; i<m_nChannels; i++) {
    rv = icMaxStatus(rv, Info.CheckData(sReport, m_maxColorantXYZ[i], "Max Colorant XYZ"));
  }

  return rv;
}



/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::CIccTagResponseCurveSet16
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagResponseCurveSet16::CIccTagResponseCurveSet16()
{
  m_nChannels = 0;

  m_ResponseCurves = new(CIccResponseCurveSet);
  m_Curve = new(CIccResponseCurveSetIter);
  m_Curve->inited = false;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::CIccTagResponseCurveSet16
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITRCS = The CIccTagResponseCurveSet16 object to be copied
 *****************************************************************************
 */
CIccTagResponseCurveSet16::CIccTagResponseCurveSet16(const CIccTagResponseCurveSet16 &ITRCS)
{
  m_nChannels = ITRCS.m_nChannels;
  m_ResponseCurves = new(CIccResponseCurveSet);
  *m_ResponseCurves = *ITRCS.m_ResponseCurves;
  m_Curve = new(CIccResponseCurveSetIter);
  *m_Curve = *ITRCS.m_Curve;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  RespCurveSet16Tag = The CIccTagResponseCurveSet16 object to be copied
 *****************************************************************************
 */
CIccTagResponseCurveSet16 &CIccTagResponseCurveSet16::operator=(const CIccTagResponseCurveSet16 &RespCurveSet16Tag)
{
  if (&RespCurveSet16Tag == this)
    return *this;

  if (!m_ResponseCurves->empty())
    m_ResponseCurves->clear();

  m_nChannels = RespCurveSet16Tag.m_nChannels;
  *m_ResponseCurves = *RespCurveSet16Tag.m_ResponseCurves;
  *m_Curve = *RespCurveSet16Tag.m_Curve;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::~CIccTagResponseCurveSet16
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagResponseCurveSet16::~CIccTagResponseCurveSet16()
{
  if (!m_ResponseCurves->empty())
    m_ResponseCurves->clear();

  delete m_ResponseCurves;
  delete m_Curve;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagResponseCurveSet16::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  size_t startPos = pIO->Tell();

  unsigned long headerSize = sizeof(icTagTypeSignature) +
                             sizeof(icUInt32Number) +
                             sizeof(icUInt16Number) * 2;
  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }
 
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved))
    return false;

  icUInt16Number nCountMeasmntTypes;
  
  if (!pIO->Read16(&m_nChannels) ||
      !pIO->Read16(&nCountMeasmntTypes))
    return false;
  
  if ((icUInt32Number)nCountMeasmntTypes * sizeof(icUInt32Number) > size - headerSize)
    return false;

  icUInt32Number* nOffset = new icUInt32Number[nCountMeasmntTypes];

  if (pIO->Read32(&nOffset[0], nCountMeasmntTypes) != nCountMeasmntTypes) {
    delete[] nOffset;
    return false;
  }

  CIccResponseCurveStruct entry;

  for (icUInt16Number i=0; i<nCountMeasmntTypes; i++) {
    if (nOffset[i] + 4 > size) {
      delete[] nOffset;
      return false;
    }
    pIO->Seek(startPos + nOffset[i], icSeekSet);
    entry = CIccResponseCurveStruct(m_nChannels);
    if (!entry.Read(size-nOffset[i], pIO)) {
      delete[] nOffset;
      return false;
    }

    m_ResponseCurves->push_back(entry);
  }
  delete[] nOffset;


  m_Curve->inited = false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagResponseCurveSet16::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();
  icUInt16Number nCountMeasmntTypes = (icUInt16Number)m_ResponseCurves->size();

  if (!pIO) {
    return false;
  }

  size_t startPos = pIO->GetLength();

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved))
    return false;


  if (!pIO->Write16(&m_nChannels) ||
      !pIO->Write16(&nCountMeasmntTypes))
    return false;

  size_t offsetPos = pIO->GetLength();
  icUInt32Number* nOffset = new icUInt32Number[nCountMeasmntTypes];


  int j;
  for (j=0; j<nCountMeasmntTypes; j++) {
    nOffset[j] = 0;
    if (!pIO->Write32(&nOffset[j]))
      return false;
  }

  CIccResponseCurveSet::iterator i;

  for (i=m_ResponseCurves->begin(), j=0; i!=m_ResponseCurves->end(); i++, j++) {
    nOffset[j] = (icUInt32Number)(pIO->GetLength() - startPos);
    if (!i->Write(pIO))
      return false;
  }

  size_t curPOs = pIO->GetLength();

  pIO->Seek(offsetPos,icSeekSet);

  for (j=0; j<nCountMeasmntTypes; j++) {
    if (!pIO->Write32(&nOffset[j]))
      return false;
  }

  pIO->Seek(curPOs,icSeekSet);
  delete [] nOffset;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagResponseCurveSet16::Describe(std::string &sDescription, int nVerboseness)
{
  CIccResponseCurveSet::iterator i;
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "Number of Channels: %u\n", m_nChannels);
  sDescription += buf;

  snprintf(buf, bufSize, "Number of Measurement Types used: %u\n", (icUInt32Number)m_ResponseCurves->size());
  sDescription += buf;

  int count = 0;
  for (i=m_ResponseCurves->begin(); i!=m_ResponseCurves->end(); i++, count++) {
     sDescription += "\n";

    snprintf(buf, bufSize, "Response Curve for measurement type [%u] follows:\n", count+1);
    sDescription += buf;

    i->Describe(sDescription, nVerboseness);
  }
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::SetNumChannels
 * 
 * Purpose: Sets the number of channels. This will delete any prior Response
 *  curves from the set.
 * 
 * Args: 
 *  nChannels = number of channels
 *****************************************************************************
 */
void CIccTagResponseCurveSet16::SetNumChannels(icUInt16Number nChannels)
{
  m_nChannels = nChannels;

  if (!m_ResponseCurves->empty())
    m_ResponseCurves->clear();
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::NewResponseCurves
 * 
 * Purpose: Creates and adds a new set of response curves to the list. 
 *  SetNumChannels() must be called before calling this function.
 * 
 * Args: 
 *  sig = measurement unit signature
 *****************************************************************************
 */
CIccResponseCurveStruct *CIccTagResponseCurveSet16::NewResponseCurves(icMeasurementUnitSig sig)
{
  if (!m_nChannels)
    return NULL;

  CIccResponseCurveStruct *pResponseCurveStruct;
  pResponseCurveStruct = GetResponseCurves(sig);

  if (pResponseCurveStruct)
    return pResponseCurveStruct;

  CIccResponseCurveStruct entry;
  entry = CIccResponseCurveStruct(sig, m_nChannels);
  m_ResponseCurves->push_back(entry);
  m_Curve->inited = false;

  return GetResponseCurves(sig);
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::GetResponseCurves
 * 
 * Purpose: Returns pointer to the requested set of response curves
 * 
 * Args: 
 *  sig = measurement unit signature of the response curve set desired
 *****************************************************************************
 */
CIccResponseCurveStruct *CIccTagResponseCurveSet16::GetResponseCurves(icMeasurementUnitSig sig)
{
  if (!m_nChannels)
    return NULL;

  CIccResponseCurveSet::iterator i;

  for (i=m_ResponseCurves->begin(); i!=m_ResponseCurves->end(); i++) {
    if (i->GetMeasurementType() == sig)
      return (i->GetThis());
  }

  return NULL;
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::GetFirstCurves
 * 
 * Purpose: Returns pointer to the first set of response curves in the list.
 * 
 *****************************************************************************
 */
CIccResponseCurveStruct *CIccTagResponseCurveSet16::GetFirstCurves()
{
  if (!m_Curve)
    return NULL;

  m_Curve->item = m_ResponseCurves->begin();
  if (m_Curve->item == m_ResponseCurves->end()) {
    m_Curve->inited = false;
    return NULL;
  }
  m_Curve->inited = true;
  return m_Curve->item->GetThis();
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::GetNextCurves
 * 
 * Purpose: Serves as an iterator for the list containing response curves.
 *   GetFirstCurves() must be called before calling this function.
 * 
 *****************************************************************************
 */
CIccResponseCurveStruct *CIccTagResponseCurveSet16::GetNextCurves()
{
  if (!m_Curve || !m_Curve->inited)
    return NULL;

  m_Curve->item++;
  if (m_Curve->item==m_ResponseCurves->end()) {
    m_Curve->inited = false;
    return NULL;
  }
  return m_Curve->item->GetThis();
}


/**
 ****************************************************************************
 * Name: CIccTagResponseCurveSet16::GetNumResponseCurveTypes
 * 
 * Purpose: Get the number of response curve types.
 *   
 *****************************************************************************
 */
icUInt16Number CIccTagResponseCurveSet16::GetNumResponseCurveTypes() const
{
  return(icUInt16Number) m_ResponseCurves->size();
}


/**
******************************************************************************
* Name: CIccTagResponseCurveSet16::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagResponseCurveSet16::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  if (!pProfile) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Tag validation incomplete: Pointer to profile unavailable.\n";
    rv = icMaxStatus(rv, icValidateWarning);
    return rv;
  }

  if (m_nChannels!=icGetSpaceSamples(pProfile->m_Header.colorSpace)) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Incorrect number of channels.\n";
  }

  if (!GetNumResponseCurveTypes()) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Empty Tag!.\n";
    rv = icMaxStatus(rv, icValidateWarning);
  }
  else {
    CIccResponseCurveSet::iterator i;
    for (i=m_ResponseCurves->begin(); i!=m_ResponseCurves->end(); i++) {
      rv = icMaxStatus(rv, i->Validate(sReport));
    }
  }

  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::CIccTagSpectralDataInfo
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagSpectralDataInfo::CIccTagSpectralDataInfo()
{
  m_nSig = 0;
  memset(&m_spectralRange, 0, sizeof(m_spectralRange));
  memset(&m_biSpectralRange, 0, sizeof(m_biSpectralRange));
}



/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::CIccTagSpectralDataInfo
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITS = The CIccTagSpectralDataInfo object to be copied
 *****************************************************************************
 */
CIccTagSpectralDataInfo::CIccTagSpectralDataInfo(const CIccTagSpectralDataInfo &ITS)
{
  m_nSig = ITS.m_nSig;
  m_spectralRange = ITS.m_spectralRange;
  m_biSpectralRange = ITS.m_biSpectralRange;
}



/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  SignatureTag = The CIccTagSignature object to be copied
 *****************************************************************************
 */
CIccTagSpectralDataInfo &CIccTagSpectralDataInfo::operator=(const CIccTagSpectralDataInfo &ITS)
{
  if (&ITS == this)
    return *this;

  m_nSig = ITS.m_nSig;
  m_spectralRange = ITS.m_spectralRange;
  m_biSpectralRange = ITS.m_biSpectralRange;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::~CIccTagSpectralDataInfo
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagSpectralDataInfo::~CIccTagSpectralDataInfo()
{
}

/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagSpectralDataInfo::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) + 2*sizeof(icUInt32Number) + 6*sizeof(icUInt16Number) > size)
    return false;

  if (!pIO) {
    m_nSig = 0; 
    memset(&m_spectralRange, 0, sizeof(m_spectralRange));
    memset(&m_biSpectralRange, 0, sizeof(m_biSpectralRange));
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nSig))
    return false;

  if (!pIO->Read16(&m_spectralRange.start))
    return false;

  if (!pIO->Read16(&m_spectralRange.end))
    return false;

  if (!pIO->Read16(&m_spectralRange.steps))
    return false;

  if (!pIO->Read16(&m_biSpectralRange.start))
    return false;

  if (!pIO->Read16(&m_biSpectralRange.end))
    return false;

  if (!pIO->Read16(&m_biSpectralRange.steps))
    return false;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagSpectralDataInfo::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
   return false;

  if (!pIO->Write32(&sig))
   return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nSig))
    return false;

  if (!pIO->Write16(&m_spectralRange.start))
    return false;

  if (!pIO->Write16(&m_spectralRange.end))
    return false;

  if (!pIO->Write16(&m_spectralRange.steps))
    return false;

  if (!pIO->Write16(&m_biSpectralRange.start))
    return false;

  if (!pIO->Write16(&m_biSpectralRange.end))
    return false;

  if (!pIO->Write16(&m_biSpectralRange.steps))
    return false;
  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralDataInfo::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagSpectralDataInfo::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 256;
  icChar buf[bufSize];

  sDescription += "ColorSignature: ";
  sDescription += icGetColorSigStr(buf, bufSize, m_nSig);
  sDescription += "\n";

  snprintf(buf, bufSize, "SpectralRange: start %fnm end %fnm with %d steps\n", icF16toF(m_spectralRange.start), icF16toF(m_spectralRange.end), m_spectralRange.steps);
  sDescription += buf;
  if (m_biSpectralRange.steps) {
    snprintf(buf, bufSize, "BiSpectralRange: start %fnm end %fnm with %d steps\n", icF16toF(m_spectralRange.start), icF16toF(m_spectralRange.end), m_spectralRange.steps);
    sDescription += buf;
  }
}


/**
******************************************************************************
* Name: CIccTagSpectralDataInfo::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagSpectralDataInfo::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  icSignature sig = icGetFirstSigPathSig(sigPath);

  if (sig==icSigSpectralDataInfoTag && pProfile->m_Header.spectralPCS) {
    const icHeader *pHdr = &pProfile->m_Header;

    if (m_nSig != pHdr->spectralPCS ||
        memcmp(&m_spectralRange, &pHdr->spectralRange, sizeof(m_spectralRange)) ||
        memcmp(&m_biSpectralRange, &pHdr->biSpectralRange, sizeof(m_biSpectralRange))) {
      sReport += icMsgValidateCriticalError;
      sReport += sSigPathName;
      sReport += " - SpectralDataInfo should be the same as the profile spectralPCS.\n";
      rv = icMaxStatus(rv, icValidateCriticalError);
    }
  }
  rv = icMaxStatus(rv, Info.CheckData(sReport, m_spectralRange, "Spectral Range"));
  if (m_biSpectralRange.steps)
    rv = icMaxStatus(rv, Info.CheckData(sReport, m_biSpectralRange, "Bispectral Range"));
  return rv;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::CIccTagSpectralViewingConditions
 * 
 * Purpose: Constructor
 * 
 *****************************************************************************
 */
CIccTagSpectralViewingConditions::CIccTagSpectralViewingConditions()
{
  m_stdObserver = icStdObs1931TwoDegrees;
  m_observerRange.start = 0;
  m_observerRange.end = 0;
  m_observerRange.steps = 0;
  m_observer = NULL;

  m_stdIlluminant = icIlluminantD50;
  m_colorTemperature = 5000.0f;

  m_illuminantRange.start = 0;
  m_illuminantRange.end = 0;
  m_illuminantRange.steps = 0;

  m_illuminant = 0;

  m_illuminantXYZ.X = 0;
  m_illuminantXYZ.Y = 0;
  m_illuminantXYZ.Z = 0;

  m_surroundXYZ.X = 0;
  m_surroundXYZ.Y = 0;
  m_surroundXYZ.Z = 0;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::CIccTagSpectralViewingConditions
 * 
 * Purpose: Copy Constructor
 *
 * Args:
 *  SVCT = The CIccTagSpectralViewingConditions object to be copied
 *****************************************************************************
 */
CIccTagSpectralViewingConditions::CIccTagSpectralViewingConditions(const CIccTagSpectralViewingConditions &SVCT)
{
  m_stdObserver = SVCT.m_stdObserver;
  m_observerRange = SVCT.m_observerRange;

  if (SVCT.m_observer && SVCT.m_observerRange.steps) {
    m_observer = new icFloat32Number[SVCT.m_observerRange.steps*3];
    if (m_observer) {
      memcpy(m_observer, SVCT.m_observer, SVCT.m_observerRange.steps*3*sizeof(icFloat32Number));
    }
  }
  else {
    m_observer = NULL;
  }

  m_stdIlluminant = SVCT.m_stdIlluminant;
  m_colorTemperature = SVCT.m_colorTemperature;

  if (SVCT.m_illuminant && SVCT.m_illuminantRange.steps) {
    m_illuminant = new icFloat32Number[SVCT.m_illuminantRange.steps];
    if (m_illuminant) {
      memcpy(m_illuminant, SVCT.m_illuminant, SVCT.m_illuminantRange.steps*sizeof(icFloat32Number));
    }
  }
  else {
    m_illuminant = NULL;
  }

  m_illuminantXYZ = SVCT.m_illuminantXYZ;
  m_surroundXYZ = SVCT.m_surroundXYZ;
  m_illuminantRange = SVCT.m_illuminantRange;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::operator=
 * 
 * Purpose: Copy Operator
 *
 * Args:
 *  specViewCondTag = The CIccTagSpectralViewingConditions object to be copied
 *****************************************************************************
 */
CIccTagSpectralViewingConditions &CIccTagSpectralViewingConditions::operator=(const CIccTagSpectralViewingConditions &SVCT)
{
  m_stdObserver = SVCT.m_stdObserver;
  m_observerRange = SVCT.m_observerRange;

  if (SVCT.m_observer && SVCT.m_observerRange.steps) {
    m_observer = new icFloat32Number[SVCT.m_observerRange.steps*3];
    if (m_observer) {
      memcpy(m_observer, SVCT.m_observer, SVCT.m_observerRange.steps*3*sizeof(icFloat32Number));
    }
  }
  else {
    m_observer = NULL;
  }

  m_stdIlluminant = SVCT.m_stdIlluminant;
  m_colorTemperature = SVCT.m_colorTemperature;

  if (SVCT.m_illuminant && SVCT.m_illuminantRange.steps) {
    m_illuminant = new icFloat32Number[SVCT.m_illuminantRange.steps];
    if (m_illuminant) {
      memcpy(m_illuminant, SVCT.m_illuminant, SVCT.m_illuminantRange.steps*sizeof(icFloat32Number));
    }
  }
  else {
    m_illuminant = NULL;
  }

  m_illuminantXYZ = SVCT.m_illuminantXYZ;
  m_surroundXYZ = SVCT.m_surroundXYZ;
  m_illuminantRange = SVCT.m_illuminantRange;

  return *this;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::~CIccTagResponseCurveSet16
 * 
 * Purpose: Destructor
 * 
 *****************************************************************************
 */
CIccTagSpectralViewingConditions::~CIccTagSpectralViewingConditions()
{
  if (m_observer)
    delete [] m_observer;

  if (m_illuminant)
    delete [] m_illuminant;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::Read
 * 
 * Purpose: Read in the tag contents into a data block
 * 
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 * 
 * Return: 
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagSpectralViewingConditions::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  //check size properly
  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
                              sizeof(icUInt32Number)*4;
  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }
 
  if (!pIO->Read32(&sig) ||
      !pIO->Read32(&m_nReserved))
    return false;


  if (!pIO->Read32(&m_stdObserver) ||
      !pIO->Read16(&m_observerRange.start) ||
      !pIO->Read16(&m_observerRange.end) ||
      !pIO->Read16(&m_observerRange.steps) ||
      !pIO->Read16(&m_reserved2))
    return false;

  icUInt32Number vals;

  if (m_observer) {
    delete [] m_observer;
    m_observer = NULL;
  }

  icUInt32Number observerSize = 0;

  if (m_observerRange.steps) {
    vals = m_observerRange.steps * 3;

    observerSize = vals * sizeof(icFloat32Number);

    if (headerSize + observerSize > size)
      return false;
    
    m_observer = new icFloat32Number[vals];
    if (!m_observer)
      return false;

    if (pIO->ReadFloat32Float(&m_observer[0], vals) != vals)
      return false;
  }

  icUInt32Number illumInfoSize = 2 * sizeof(icUInt32Number) + 4 * sizeof(icUInt16Number);
  if (headerSize + observerSize +illumInfoSize > size)
    return false;

  if (!pIO->Read32(&m_stdIlluminant) ||
      !pIO->ReadFloat32Float(&m_colorTemperature) ||
      !pIO->Read16(&m_illuminantRange.start) ||
      !pIO->Read16(&m_illuminantRange.end) ||
      !pIO->Read16(&m_illuminantRange.steps) ||
      !pIO->Read16(&m_reserved3))
    return false;


  if (m_illuminant) {
    delete [] m_illuminant;
    m_illuminant = NULL;
  }

  icUInt32Number illuminantSize = 0;

  if (m_illuminantRange.steps) {
    vals = m_illuminantRange.steps;

    illuminantSize = vals * sizeof(icFloat32Number);

    if (headerSize + observerSize + illumInfoSize + illuminantSize > size)
      return false;

    m_illuminant = new icFloat32Number[vals];
    if (!m_illuminant)
      return false;

    if (pIO->ReadFloat32Float(&m_illuminant[0], vals) != vals)
      return false;
  }
  else {
    setIlluminant(m_stdIlluminant, m_illuminantRange, NULL, m_colorTemperature);
  }

  if (headerSize + observerSize + illumInfoSize + illuminantSize + 6*sizeof(icFloat32Number) > size)
    return false;

  if (pIO->ReadFloat32Float(&m_illuminantXYZ, 3)!=3 ||
      pIO->ReadFloat32Float(&m_surroundXYZ, 3)!=3)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::Write
 * 
 * Purpose: Write the tag to a file
 * 
 * Args: 
 *  pIO - The IO object to write tag to.
 * 
 * Return: 
 *  true = succesful, false = failure
 *****************************************************************************
 */
bool CIccTagSpectralViewingConditions::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO) {
    return false;
  }

  if (!pIO->Write32(&sig) ||
      !pIO->Write32(&m_nReserved))
    return false;


  if (!pIO->Write32(&m_stdObserver) ||
      !pIO->Write16(&m_observerRange.start) ||
      !pIO->Write16(&m_observerRange.end) ||
      !pIO->Write16(&m_observerRange.steps) ||
      !pIO->Write16(&m_reserved2))
    return false;

  icUInt32Number vals = m_observerRange.steps*3;

  if (vals && m_observer)
    if (pIO->WriteFloat32Float(&m_observer[0], vals) != vals)
      return false;

  if (!pIO->Write32(&m_stdIlluminant) ||
    !pIO->WriteFloat32Float(&m_colorTemperature) ||
    !pIO->Write16(&m_illuminantRange.start) ||
    !pIO->Write16(&m_illuminantRange.end) ||
    !pIO->Write16(&m_illuminantRange.steps) ||
    !pIO->Write16(&m_reserved3))
    return false;

  vals = m_illuminantRange.steps;

  if (vals && m_illuminant)
    if (pIO->WriteFloat32Float(&m_illuminant[0], vals) != vals)
      return false;

  if (pIO->WriteFloat32Float(&m_illuminantXYZ, 3)!=3 ||
      pIO->WriteFloat32Float(&m_surroundXYZ, 3)!=3)
    return false;

  return true;
}


/**
 ****************************************************************************
 * Name: CIccTagSpectralViewingConditions::Describe
 * 
 * Purpose: Dump data associated with the tag to a string
 * 
 * Args: 
 *  sDescription - string to concatenate tag dump to
 *****************************************************************************
 */
void CIccTagSpectralViewingConditions::Describe(std::string &sDescription, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];
  CIccInfo info;

  snprintf(buf, bufSize, "StdObserver: %s\n\n", info.GetStandardObserverName(m_stdObserver));
  sDescription += buf;

  snprintf(buf, bufSize, "Illuminant Tristimulus values: X = %.4lf, Y = %.4lf, Z = %.4lf\n\n",
    m_illuminantXYZ.X, m_illuminantXYZ.Y,m_illuminantXYZ.Z);
  sDescription += buf;

  if (m_observer) {
    snprintf(buf, bufSize, "Observer Functions: start=%.1fnm end=%.1fnm, steps=%d\n",
            icF16toF(m_observerRange.start), icF16toF(m_observerRange.end), m_observerRange.steps);
    sDescription += buf;

    if (nVerboseness > 75) {
        icFloat32Number *ptr = m_observer;
        int i, j;
        for (j=0; j<3; j++) {
          for (i=0; i<m_observerRange.steps; i++) {
            snprintf(buf, bufSize, " %.4f", *ptr);
            sDescription += buf;
            ptr++;
          }
          sDescription += "\n\n";
        }
    }
  }
  else {
    sDescription += "No custom Observer defined\n\n";
  }

  sDescription += "StdIlluminant Type: ";

  sDescription += info.GetIlluminantName(m_stdIlluminant);
  sDescription += "\n";

  snprintf(buf, bufSize, "Color Temperature: %.1fK\n\n", m_colorTemperature);
  sDescription += buf;

  if (m_illuminant) {
    snprintf(buf, bufSize, "Illuminant SPD: start=%.1fm end=%.1fnm, steps=%d\n",
      icF16toF(m_illuminantRange.start), icF16toF(m_illuminantRange.end), m_illuminantRange.steps);
    sDescription += buf;
    if (nVerboseness > 75) {
        icFloat32Number *ptr = m_illuminant;
        int i;
        for (i=0; i<m_illuminantRange.steps; i++) {
          snprintf(buf, bufSize, " %.4f", *ptr);
          sDescription += buf;
          ptr++;
        }
    }
    sDescription += "\n\n";
  }
  else {
    sDescription += "No custom Observer defined\n\n";
  }

  snprintf(buf, bufSize, "Surround Tristimulus values: X = %.4lf, Y = %.4lf, Z = %.4lf\n",
    m_surroundXYZ.X, m_surroundXYZ.Y, m_surroundXYZ.Z);
  sDescription += buf;
}



/**
******************************************************************************
* Name: CIccTagSpectralViewingConditions::Validate
* 
* Purpose: Check tag data validity.
* 
* Args: 
*  sig = signature of tag being validated,
*  sReport = String to add report information to
* 
* Return: 
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagSpectralViewingConditions::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  rv = icMaxStatus(rv, Info.CheckData(sReport, m_illuminantXYZ, sSigPathName + ":>illuminantXYZ"));
  rv = icMaxStatus(rv, Info.CheckData(sReport, m_surroundXYZ, sSigPathName + ":>surroundXYZ"));

  rv = icMaxStatus(rv, Info.CheckLuminance(sReport, m_illuminantXYZ, sSigPathName + ":>illuminantXYZ"));
  rv = icMaxStatus(rv, Info.CheckLuminance(sReport, m_surroundXYZ, sSigPathName + ":>surroundXYZ"));

  icSpectralRange range;

  if (getObserver(range)) {
    rv = icMaxStatus(rv, Info.CheckData(sReport, range, sSigPathName + ":>observerRange"));

    if (!m_observer && pProfile && pProfile->m_Header.version < icVersionNumberV5_1) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Missing Observer CMF not supported by profile version!\r\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else {
    rv = icMaxStatus(rv, Info.CheckData(sReport, m_observerRange, sSigPathName + ":>observerRange"));

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Missing Observer CMF!\r\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  if (getIlluminant(range)) {
    rv = icMaxStatus(rv, Info.CheckData(sReport, range, sSigPathName + ":>illuminantRange"));

    if (!m_illuminant && pProfile && pProfile->m_Header.version < icVersionNumberV5_1) {
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += " - Missing illuminant SPD not supported by profile version!\r\n";
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }
  else {
    rv = icMaxStatus(rv, Info.CheckData(sReport, m_illuminantRange, sSigPathName + ":>illuminantRange"));
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Missing Illuminant SPD!\r\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}

icFloatNumber *CIccTagSpectralViewingConditions::applyRangeToObserver(const icSpectralRange &newRange) const
{
  int n = newRange.steps*3;
  icFloatNumber *rv = (icFloatNumber*)malloc(n*sizeof(icFloatNumber));

  CIccMatrixMath *range = CIccMatrixMath::rangeMap(m_observerRange, newRange);
  if (range) {
    range->VectorMult(rv, m_observer);
    range->VectorMult(&rv[newRange.steps], &m_observer[m_observerRange.steps]);
    range->VectorMult(&rv[newRange.steps*2], &m_observer[m_observerRange.steps*2]);
    delete range;
  }
  else {
    memcpy(rv, m_observer, m_observerRange.steps*3*sizeof(icFloatNumber));
  }

  return rv;
}

CIccMatrixMath *CIccTagSpectralViewingConditions::getObserverMatrix(const icSpectralRange &newRange) const
{
  CIccMatrixMath *pMtx=new CIccMatrixMath(3, newRange.steps);

  CIccMatrixMath *range = CIccPcsXform::rangeMap(m_observerRange, newRange);
  if (range) {
    range->VectorMult(pMtx->entry(0), m_observer);
    range->VectorMult(pMtx->entry(1), &m_observer[m_observerRange.steps]);
    range->VectorMult(pMtx->entry(2), &m_observer[m_observerRange.steps*2]);
    delete range;
  }
  else {
    memcpy(pMtx->entry(0), m_observer, m_observerRange.steps*3*sizeof(icFloatNumber));
  }

  return pMtx;
}

typedef struct {
  icIlluminant illum;
  icFloatNumber spd[81];
} icIllumDef;

static icIllumDef icKnownIllums[] = {
  { icIlluminantD50,
    { 24.457147f,   27.147280f,   29.837414f,   39.547443f,   49.257472f,   52.859364f,   56.461255f,
      58.222678f,   59.984100f,   58.878685f,   57.773270f,   66.274622f,   74.775973f,   80.987003f,
      87.198034f,   88.882488f,   90.566943f,   90.947782f,   91.328622f,   93.200755f,   95.072887f,
      93.503707f,   91.934527f,   93.817670f,   95.700813f,   96.147589f,   96.594366f,   96.854869f,
      97.115372f,   99.602130f,  102.088888f,  101.418624f,  100.748359f,  101.531110f,  102.313861f,
     101.156931f,  100.000000f,   98.868725f,   97.737450f,   98.330520f,   98.923589f,   96.216919f,
      93.510248f,   95.607997f,   97.705746f,   98.498795f,   99.291845f,   99.179895f,   99.067945f,
      97.409311f,   95.750676f,   97.322057f,   98.893438f,   97.299430f,   95.705421f,   96.969799f,
      98.234177f,  100.644248f,  103.054319f,  101.119636f,   99.184954f,   93.304566f,   87.424178f,
      89.538557f,   91.652935f,   92.293256f,   92.933577f,   84.912496f,   76.891415f,   81.721450f,
      86.551486f,   89.586840f,   92.622195f,   85.443791f,   78.265387f,   67.992302f,   57.719217f,
      70.340670f,   82.962124f,   80.636096f,   78.310068f }
  },
  { icIlluminantD65,
    { 49.975500f,   52.311800f,   54.648200f,   68.701500f,   82.754900f,   87.120400f,   91.486000f,
      92.458900f,   93.431800f,   90.057000f,   86.682300f,   95.773600f,  104.865000f,  110.936000f,
     117.008000f,  117.410000f,  117.812000f,  116.336000f,  114.861000f,  115.392000f,  115.923000f,
     112.367000f,  108.811000f,  109.082000f,  109.354000f,  108.578000f,  107.802000f,  106.296000f,
     104.790000f,  106.239000f,  107.689000f,  106.047000f,  104.405000f,  104.225000f,  104.046000f,
     102.023000f,  100.000000f,   98.167100f,   96.334200f,   96.061100f,   95.788000f,   92.236800f,
      88.685600f,   89.345900f,   90.006200f,   89.802600f,   89.599100f,   88.648900f,   87.698700f,
      85.493600f,   83.288600f,   83.493900f,   83.699200f,   81.863000f,   80.026800f,   80.120700f,
      80.214600f,   81.246200f,   82.277800f,   80.281000f,   78.284200f,   74.002700f,   69.721300f,
      70.665200f,   71.609100f,   72.979000f,   74.349000f,   67.976500f,   61.604000f,   65.744800f,
      69.885600f,   72.486300f,   75.087000f,   69.339800f,   63.592700f,   55.005400f,   46.418200f,
      56.611800f,   66.805400f,   65.094100f,   63.382800f }
  },
  { icIlluminantD93,
    { 92.424314f,   92.694368f,   92.964422f,  111.383899f,  129.803377f,  135.801152f,  141.798926f,
     141.447583f,  141.096240f,  133.975439f,  126.854638f,  135.602812f,  144.350987f,  149.960021f,
     155.569056f,  154.184112f,  152.799168f,  148.783207f,  144.767246f,  143.338753f,  141.910261f,
     136.139746f,  130.369231f,  128.545220f,  126.721209f,  124.329813f,  121.938418f,  117.937629f,
     113.936841f,  114.043847f,  114.150853f,  111.519464f,  108.888075f,  107.474441f,  106.060808f,
     103.030404f,  100.000000f,   97.314129f,   94.628259f,   93.347855f,   92.067450f,   88.659570f,
      85.251689f,   84.758623f,   84.265557f,   83.380599f,   82.495642f,   80.837162f,   79.178681f,
      76.519588f,   73.860495f,   73.447089f,   73.033684f,   71.360089f,   69.686495f,   69.189247f,
      68.692000f,   68.742482f,   68.792965f,   67.049716f,   65.306467f,   61.725711f,   58.144955f,
      58.497707f,   58.850460f,   60.339599f,   61.828738f,   56.573774f,   51.318811f,   54.969052f,
      58.619293f,   60.929241f,   63.239189f,   58.464199f,   53.689209f,   46.273925f,   38.858642f,
      47.398674f,   55.938706f,   54.609104f,   53.279502f,
    }
  },
  { icIlluminantA,
    { 9.7951f,   10.8996f,   12.0853f,   13.3543f,   14.7080f,   16.1480f,   17.6753f,   19.2907f,
     20.9950f,   22.7883f,   24.6709f,   26.6425f,   28.7027f,   30.8508f,   33.0859f,   35.4068f,
     37.8121f,   40.3002f,   42.8693f,   45.5174f,   48.2423f,   51.0418f,   53.9132f,   56.8539f,
     59.8611f,   62.9320f,   66.0635f,   69.2525f,   72.4959f,   75.7903f,   79.1326f,   82.5193f,
     85.9470f,   89.4124f,   92.9120f,   96.4423f,  100.0000f,  103.5820f,  107.1840f,  110.8030f,
    114.4360f,  118.0800f,  121.7310f,  125.3860f,  129.0430f,  132.6970f,  136.3460f,  139.9880f,
    143.6180f,  147.2350f,  150.8360f,  154.4180f,  157.9790f,  161.5160f,  165.0280f,  168.5100f,
    171.9630f,  175.3830f,  178.7690f,  182.1180f,  185.4290f,  188.7010f,  191.9310f,  195.1180f,
    198.2610f,  201.3590f,  204.4090f,  207.4110f,  210.3650f,  213.2680f,  216.1200f,  218.9200f,
    221.6670f,  224.3610f,  227.0000f,  229.5850f,  232.1150f,  234.5890f,  237.0080f,  239.3700f,
    241.6750f, }
  },
};
#define NUM_KNOWN_ILLUM (sizeof(icKnownIllums)/sizeof(icKnownIllums[0]))

static icSpectralRange icKnownIllumObsRange = { icRange380nm, icRange780nm, 81 };

const icFloatNumber *CIccTagSpectralViewingConditions::getIlluminant(icSpectralRange &illumRange) const
{
  if (!m_illuminant || !m_illuminantRange.steps) {
    for (size_t i = 0; i < NUM_KNOWN_ILLUM; i++) {
      if (m_stdIlluminant == icKnownIllums[i].illum) {
        illumRange = icKnownIllumObsRange;
        return &icKnownIllums[i].spd[0];
      }
    }
  }

  illumRange = m_illuminantRange;
  return m_illuminant;
}

bool CIccTagSpectralViewingConditions::setIlluminant(icIlluminant illumId, const icSpectralRange &illumRange, const icFloatNumber *illum, icFloatNumber illumCCT/* = 0.0f*/)
{
  m_stdIlluminant = illumId;
  m_colorTemperature = illumCCT;

  if (m_illuminant) {
    delete[] m_illuminant;
    m_illuminant = nullptr;
  }

  m_illuminantRange = illumRange;

  if (illumRange.steps && illum) {
    icUInt32Number size = illumRange.steps;
    m_illuminant = new icFloatNumber[size]; // Replaced malloc with new[]
    if (m_illuminant)
      memcpy(m_illuminant, illum, size * sizeof(icFloatNumber)); // Retained memcpy
    else {
      memset(&m_illuminantRange, 0, sizeof(m_illuminantRange));
      return false;
    }
  } else {
    if (!illum)
      memset(&m_illuminantRange, 0, sizeof(m_illuminantRange));

    m_illuminant = nullptr;
  }

  return true;
}

bool CIccTagSpectralViewingConditions::setIlluminant(icFloatNumber *pWhiteXYZ)
{
  icSpectralRange zeroRange;
  memset(&zeroRange, 0, sizeof(zeroRange));
  icFloatNumber white[3] = {0};
  white[0] = pWhiteXYZ[0] / pWhiteXYZ[1];
  white[1] = 1.0;
  white[2] = pWhiteXYZ[2] / pWhiteXYZ[1];

  if (!pWhiteXYZ || !icNotZero(pWhiteXYZ[1])) {
    setIlluminant(icIlluminantUnknown, zeroRange, NULL);
    return false;
  }

  if (m_stdObserver == icStdObs1931TwoDegrees) {
    if ((icIsNear(white[0], icD50XYZ[0]) && icIsNear(white[2], icD50XYZ[2])) ||
        (icIsNear(white[0], 0.9642f, 0.0001f) && icIsNear(white[2], 0.8251f, 0.0001f))) {
      return setIlluminant(icIlluminantD50, zeroRange, NULL, 5000);
    }
    else if (icIsNear(white[0], 0.9505f, 0.0001f) && icIsNear(white[2], 1.0888f, 0.0001f)) {
      return setIlluminant(icIlluminantD65, zeroRange, NULL, 6504);
    }
  }
//   else if (m_stdObserver == icStdObs1964TenDegrees) {
//     setIlluminant(icIlluminantUnknown, zeroRange, NULL);
//     return false;
//   }

  setIlluminant(icIlluminantUnknown, zeroRange, NULL);
  return false;
}

typedef struct {
  icStandardObserver obs;
  icFloatNumber cmf[81*3];
} icObserverDef;

static icObserverDef icKnownObservers[] = {
  { icStdObs1931TwoDegrees,
    { 0.001368f,    0.002236f,    0.004243f,    0.007650f,    0.014310f,    0.023190f,    0.043510f,
      0.077630f,    0.134380f,    0.214770f,    0.283900f,    0.328500f,    0.348280f,    0.348060f,
      0.336200f,    0.318700f,    0.290800f,    0.251100f,    0.195360f,    0.142100f,    0.095640f,
      0.057950f,    0.032010f,    0.014700f,    0.004900f,    0.002400f,    0.009300f,    0.029100f,
      0.063270f,    0.109600f,    0.165500f,    0.225750f,    0.290400f,    0.359700f,    0.433450f,
      0.512050f,    0.594500f,    0.678400f,    0.762100f,    0.842500f,    0.916300f,    0.978600f,
      1.026300f,    1.056700f,    1.062200f,    1.045600f,    1.002600f,    0.938400f,    0.854450f,
      0.751400f,    0.642400f,    0.541900f,    0.447900f,    0.360800f,    0.283500f,    0.218700f,
      0.164900f,    0.121200f,    0.087400f,    0.063600f,    0.046770f,    0.032900f,    0.022700f,
      0.015840f,    0.011359f,    0.008111f,    0.005790f,    0.004109f,    0.002899f,    0.002049f,
      0.001440f,    0.001000f,    0.000690f,    0.000476f,    0.000332f,    0.000235f,    0.000166f,
      0.000117f,    0.000083f,    0.000059f,    0.000042f,
      0.000039f,    0.000064f,    0.000120f,    0.000217f,    0.000396f,    0.000640f,    0.001210f,
      0.002180f,    0.004000f,    0.007300f,    0.011600f,    0.016840f,    0.023000f,    0.029800f,
      0.038000f,    0.048000f,    0.060000f,    0.073900f,    0.090980f,    0.112600f,    0.139020f,
      0.169300f,    0.208020f,    0.258600f,    0.323000f,    0.407300f,    0.503000f,    0.608200f,
      0.710000f,    0.793200f,    0.862000f,    0.914850f,    0.954000f,    0.980300f,    0.994950f,
      1.000000f,    0.995000f,    0.978600f,    0.952000f,    0.915400f,    0.870000f,    0.816300f,
      0.757000f,    0.694900f,    0.631000f,    0.566800f,    0.503000f,    0.441200f,    0.381000f,
      0.321000f,    0.265000f,    0.217000f,    0.175000f,    0.138200f,    0.107000f,    0.081600f,
      0.061000f,    0.044580f,    0.032000f,    0.023200f,    0.017000f,    0.011920f,    0.008210f,
      0.005723f,    0.004102f,    0.002929f,    0.002091f,    0.001484f,    0.001047f,    0.000740f,
      0.000520f,    0.000361f,    0.000249f,    0.000172f,    0.000120f,    0.000085f,    0.000060f,
      0.000042f,    0.000030f,    0.000021f,    0.000015f,
      0.006450f,    0.010550f,    0.020050f,    0.036210f,    0.067850f,    0.110200f,    0.207400f,
      0.371300f,    0.645600f,    1.039050f,    1.385600f,    1.622960f,    1.747060f,    1.782600f,
      1.772110f,    1.744100f,    1.669200f,    1.528100f,    1.287640f,    1.041900f,    0.812950f,
      0.616200f,    0.465180f,    0.353300f,    0.272000f,    0.212300f,    0.158200f,    0.111700f,
      0.078250f,    0.057250f,    0.042160f,    0.029840f,    0.020300f,    0.013400f,    0.008750f,
      0.005750f,    0.003900f,    0.002750f,    0.002100f,    0.001800f,    0.001650f,    0.001400f,
      0.001100f,    0.001000f,    0.000800f,    0.000600f,    0.000340f,    0.000240f,    0.000190f,
      0.000100f,    0.000050f,    0.000030f,    0.000020f,    0.000010f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f }
    },
  { icStdObs1964TenDegrees,
    { 0.000160f,    0.000662f,    0.002362f,    0.007242f,    0.019110f,    0.043400f,    0.084736f,
      0.140638f,    0.204492f,    0.264737f,    0.314679f,    0.357719f,    0.383734f,    0.386726f,
      0.370702f,    0.342957f,    0.302273f,    0.254085f,    0.195618f,    0.132349f,    0.080507f,
      0.041072f,    0.016172f,    0.005132f,    0.003816f,    0.015444f,    0.037465f,    0.071358f,
      0.117749f,    0.172953f,    0.236491f,    0.304213f,    0.376772f,    0.451584f,    0.529826f,
      0.616053f,    0.705224f,    0.793832f,    0.878655f,    0.951162f,    1.014160f,    1.074300f,
      1.118520f,    1.134300f,    1.123990f,    1.089100f,    1.030480f,    0.950740f,    0.856297f,
      0.754930f,    0.647467f,    0.535110f,    0.431567f,    0.343690f,    0.268329f,    0.204300f,
      0.152568f,    0.112210f,    0.081261f,    0.057930f,    0.040851f,    0.028623f,    0.019941f,
      0.013842f,    0.009577f,    0.006605f,    0.004553f,    0.003145f,    0.002175f,    0.001506f,
      0.001045f,    0.000727f,    0.000508f,    0.000356f,    0.000251f,    0.000178f,    0.000126f,
      0.000090f,    0.000065f,    0.000046f,    0.000033f,
      0.000017f,    0.000072f,    0.000253f,    0.000769f,    0.002004f,    0.004509f,    0.008756f,
      0.014456f,    0.021391f,    0.029497f,    0.038676f,    0.049602f,    0.062077f,    0.074704f,
      0.089456f,    0.106256f,    0.128201f,    0.152761f,    0.185190f,    0.219940f,    0.253589f,
      0.297665f,    0.339133f,    0.395379f,    0.460777f,    0.531360f,    0.606741f,    0.685660f,
      0.761757f,    0.823330f,    0.875211f,    0.923810f,    0.961988f,    0.982200f,    0.991761f,
      0.999110f,    0.997340f,    0.982380f,    0.955552f,    0.915175f,    0.868934f,    0.825623f,
      0.777405f,    0.720353f,    0.658341f,    0.593878f,    0.527963f,    0.461834f,    0.398057f,
      0.339554f,    0.283493f,    0.228254f,    0.179828f,    0.140211f,    0.107633f,    0.081187f,
      0.060281f,    0.044096f,    0.031800f,    0.022602f,    0.015905f,    0.011130f,    0.007749f,
      0.005375f,    0.003718f,    0.002565f,    0.001768f,    0.001222f,    0.000846f,    0.000586f,
      0.000407f,    0.000284f,    0.000199f,    0.000140f,    0.000098f,    0.000070f,    0.000050f,
      0.000036f,    0.000025f,    0.000018f,    0.000013f,
      0.000705f,    0.002928f,    0.010482f,    0.032344f,    0.086011f,    0.197120f,    0.389366f,
      0.656760f,    0.972542f,    1.282500f,    1.553480f,    1.798500f,    1.967280f,    2.027300f,
      1.994800f,    1.900700f,    1.745370f,    1.554900f,    1.317560f,    1.030200f,    0.772125f,
      0.570060f,    0.415254f,    0.302356f,    0.218502f,    0.159249f,    0.112044f,    0.082248f,
      0.060709f,    0.043050f,    0.030451f,    0.020584f,    0.013676f,    0.007918f,    0.003988f,
      0.001091f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,    0.000000f,
      0.000000f,    0.000000f,    0.000000f,    0.000000f, }
  },
};
#define NUM_KNOWN_OBSERVERS (sizeof(icKnownObservers)/sizeof(icKnownObservers[0]))


const icFloatNumber *CIccTagSpectralViewingConditions::getObserver(icSpectralRange &observerRange) const
{
  if (!m_observer || !m_observerRange.steps) {
    for (size_t i = 0; i < NUM_KNOWN_OBSERVERS; i++) {
      if (m_stdObserver == icKnownObservers[i].obs) {
        observerRange = icKnownIllumObsRange;
        return &icKnownObservers[i].cmf[0];
      }
    }
  }

  observerRange = m_observerRange;
  return m_observer;
}

bool CIccTagSpectralViewingConditions::setObserver(icStandardObserver observerId, const icSpectralRange &observerRange, const icFloatNumber *observer)
{
  m_stdObserver = observerId;
  m_observerRange = observerRange;
  if (m_observer) {
    delete [] m_observer;
  }

  if (observerRange.steps && observer) {
    icUInt32Number size = observerRange.steps * 3;
    m_observer = new icFloatNumber[size];
    if (m_observer)
      memcpy(m_observer, observer, size*sizeof(icFloatNumber));
    else {
      memset(&m_observerRange, 0, sizeof(m_observerRange));
      return false;
    }
  }
  else {
    if (!observer)
      memset(&m_observerRange, 0, sizeof(m_illuminantRange));

    m_observer = NULL;
  }

  return true;
}


bool icGetTagText(const CIccTag *pTag, std::string &text)
{
  if (!pTag) {
    text = "";
    return false;
  }
  switch (pTag->GetType()) {
    case icSigTextType:
    {
      CIccTagText *pTextTag = (CIccTagText*)pTag;
      const char *szText = pTextTag->GetText();
      if (szText) {
        text = szText;
        return true;
      }
    }
    text = "";
    return false;

    case icSigUtf8TextType:
    {
      CIccTagUtf8Text *pTextTag = (CIccTagUtf8Text*)pTag;
      const char *szText = (const char*)pTextTag->GetText();
      if (szText) {
        text = szText;
        return true;
      }
    }
    text = "";
    return false;

    case icSigZipUtf8TextType:
    {
      CIccTagZipUtf8Text *pTextTag = (CIccTagZipUtf8Text*)pTag;
      bool rv = pTextTag->GetText(text);
      return rv;
    }

    case icSigUtf16TextType:
    {
      CIccTagUtf16Text *pTextTag = (CIccTagUtf16Text*)pTag;
      const icChar *szText = pTextTag->GetText(text);
      if (szText)
        return true;
    }
    return false;

    case icSigTextDescriptionType:
    {
      CIccTagTextDescription *pTextTag = (CIccTagTextDescription*)pTag;
      const char *szText = pTextTag->GetText();
      if (szText) {
        text = szText;
        return true;
      }
    }
    text = "";
    return false;

    case icSigMultiLocalizedUnicodeType:
    {
      CIccTagMultiLocalizedUnicode *pTextTag = (CIccTagMultiLocalizedUnicode*)pTag;
      CIccLocalizedUnicode *pText = pTextTag->Find();
      if (pText) {
        bool rv = pText->GetText(text);
        return rv;
      }
    }
    text = "";
    return false;

    default:
      text = "";
      return false;
  }
}


/**
****************************************************************************
* Name: CIccEmbeddedHeightImage::CIccEmbeddedHeightImage
*
* Purpose: Constructor
*
* Args:
*  nSize =size of embedded image
*
*****************************************************************************
*/
CIccTagEmbeddedHeightImage::CIccTagEmbeddedHeightImage(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize <1)
    m_nSize = 1;
  m_pData = (icUInt8Number*)calloc(nSize, sizeof(icUInt8Number));
  m_nSeamlesIndicator = 0;
  m_nEncodingFormat = icPngImageType;
  m_fMetersMinPixelValue = 0;
  m_fMetersMaxPixelValue = 0;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::CIccTagEmbeddedHeightImage
*
* Purpose: Copy Constructor
*
* Args:
*  ITD = The CIccEmbeddedHeightImage object to be copied
*****************************************************************************
*/
CIccTagEmbeddedHeightImage::CIccTagEmbeddedHeightImage(const CIccTagEmbeddedHeightImage &IEHI)
{
  m_nSeamlesIndicator = IEHI.m_nSeamlesIndicator;
  m_nEncodingFormat = IEHI.m_nEncodingFormat;
  m_fMetersMinPixelValue = IEHI.m_fMetersMinPixelValue;
  m_fMetersMaxPixelValue = IEHI.m_fMetersMaxPixelValue;
  m_nSize = IEHI.m_nSize;

  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, IEHI.m_pData, sizeof(icUInt8Number)*m_nSize);
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::operator=
*
* Purpose: Copy Operator
*
* Args:
*  DataTag = The CIccTagData object to be copied
*****************************************************************************
*/
CIccTagEmbeddedHeightImage &CIccTagEmbeddedHeightImage::operator=(const CIccTagEmbeddedHeightImage &HeightImageTag)
{
  if (&HeightImageTag == this)
    return *this;

  m_nSeamlesIndicator = HeightImageTag.m_nSeamlesIndicator;
  m_nEncodingFormat = HeightImageTag.m_nEncodingFormat;
  m_fMetersMinPixelValue = HeightImageTag.m_fMetersMinPixelValue;
  m_fMetersMaxPixelValue = HeightImageTag.m_fMetersMaxPixelValue;
  m_nSize = HeightImageTag.m_nSize;

  if (m_pData)
    free(m_pData);
  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, HeightImageTag.m_pData, sizeof(icUInt8Number)*m_nSize);

  return *this;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::~CIccTagEmbeddedHeightImage
*
* Purpose: Destructor
*
*****************************************************************************
*/
CIccTagEmbeddedHeightImage::~CIccTagEmbeddedHeightImage()
{
  if (m_pData)
    free(m_pData);
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::Read
*
* Purpose: Read in the tag contents into a data block
*
* Args:
*  size - # of bytes in tag,
*  pIO - IO object to read tag from
*
* Return:
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedHeightImage::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) +
    sizeof(icUInt32Number) +
    sizeof(icUInt32Number) +
    sizeof(icUInt32Number) +
    sizeof(icFloat32Number) +
    sizeof(icFloat32Number) +
    sizeof(icUInt8Number) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nSeamlesIndicator))
    return false;

  if (!pIO->Read32(&m_nEncodingFormat))
    return false;

  if (!pIO->ReadFloat32Float(&m_fMetersMinPixelValue))
    return false;

  if (!pIO->ReadFloat32Float(&m_fMetersMaxPixelValue))
    return false;

  icUInt32Number nNum = size - 6 * sizeof(icUInt32Number);

  if (!SetSize(nNum))
    return false;

  if (pIO->Read8(m_pData, nNum) != nNum)
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::Write
*
* Purpose: Write the tag to a file
*
* Args:
*  pIO - The IO object to write tag to.
*
* Return:
*  true = succesful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedHeightImage::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nSeamlesIndicator))
    return false;

  if (!pIO->Write32(&m_nEncodingFormat))
    return false;

  if (!pIO->WriteFloat32Float(&m_fMetersMinPixelValue))
    return false;

  if (!pIO->WriteFloat32Float(&m_fMetersMaxPixelValue))
    return false;

  if (pIO->Write8(m_pData, m_nSize) != m_nSize)
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::Describe
*
* Purpose: Dump data associated with the tag to a string
*
* Args:
*  sDescription - string to concatenate tag dump to
*****************************************************************************
*/
void CIccTagEmbeddedHeightImage::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "\nSeamlessIndicater: %d\n", m_nSeamlesIndicator);
  sDescription += buf;

  switch (m_nEncodingFormat) {
  case icPngImageType:
    strcpy(buf, "EncodingFormat: PNG\n");
    break;
  case icTiffImageType:
    strcpy(buf, "EncodingFormat: TIFF\n");
    break;
  default:
    snprintf(buf, bufSize, "EncodingFormat: %d", m_nEncodingFormat);
    break;
  }
  sDescription += buf;

  snprintf(buf, bufSize, "Height minimum pixel: %.4fmeters\n", m_fMetersMinPixelValue);
  snprintf(buf, bufSize, "Height maximum pixel: %.4fmeters\n", m_fMetersMaxPixelValue);

  sDescription += "\nImage Data:\n";

  for (int i = 0; i<(int)m_nSize; i++) {
    if (!(i & 0x1f))
      sDescription += "\n";
    snprintf(buf, bufSize, "%02X", m_pData[i]);
    sDescription += buf;
  }
  sDescription += "\n";
}

/**
****************************************************************************
* Name: CIccTagEmbeddedHeightImage::SetSize
*
* Purpose: Sets the size of the data array.
*
* Args:
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
bool CIccTagEmbeddedHeightImage::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nSize == nSize)
    return true;

  m_pData = (icUInt8Number*)icRealloc(m_pData, nSize * sizeof(icUInt8Number));

  if (!m_pData) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nSize) {
    memset(&m_pData[m_nSize], 0, (nSize - m_nSize) * sizeof(icUInt8Number));
  }
  m_nSize = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagEmbeddedHeightImage::Validate
*
* Purpose: Check tag data validity.
*
* Args:
*  sig = signature of tag being validated,
*  sReport = String to add report information to
*
* Return:
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagEmbeddedHeightImage::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  switch (m_nSeamlesIndicator) {
  case 0:
  case 1:
    break;
  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid data flag Seamless Indicator.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  switch (m_nEncodingFormat) {
  case icPngImageType:
  case icTiffImageType:
    break;
  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid data flag Encoding Image Type.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}



/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::CIccTagEmbeddedNormalImage
*
* Purpose: Constructor
*
* Args:
*  nSize =size of embedded image
*
*****************************************************************************
*/
CIccTagEmbeddedNormalImage::CIccTagEmbeddedNormalImage(int nSize/*=1*/)
{
  m_nSize = nSize;
  if (m_nSize < 1)
    m_nSize = 1;
  m_pData = (icUInt8Number*)calloc(nSize, sizeof(icUInt8Number));
  m_nSeamlesIndicator = 0;
  m_nEncodingFormat = icPngImageType;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::CIccTagEmbeddedNormalImage
*
* Purpose: Copy Constructor
*
* Args:
*  ITD = The CIccEmbeddedNormalImage object to be copied
*****************************************************************************
*/
CIccTagEmbeddedNormalImage::CIccTagEmbeddedNormalImage(const CIccTagEmbeddedNormalImage &IENI)
{
  m_nSeamlesIndicator = IENI.m_nSeamlesIndicator;
  m_nEncodingFormat = IENI.m_nEncodingFormat;
  m_nSize = IENI.m_nSize;

  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, IENI.m_pData, sizeof(icUInt8Number)*m_nSize);
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::operator=
*
* Purpose: Copy Operator
*
* Args:
*  DataTag = The CIccTagData object to be copied
*****************************************************************************
*/
CIccTagEmbeddedNormalImage &CIccTagEmbeddedNormalImage::operator=(const CIccTagEmbeddedNormalImage &NormalImageTag)
{
  if (&NormalImageTag == this)
    return *this;

  m_nSeamlesIndicator = NormalImageTag.m_nSeamlesIndicator;
  m_nEncodingFormat = NormalImageTag.m_nEncodingFormat;
  m_nSize = NormalImageTag.m_nSize;

  if (m_pData)
    free(m_pData);
  m_pData = (icUInt8Number*)calloc(m_nSize, sizeof(icUInt8Number));
  memcpy(m_pData, NormalImageTag.m_pData, sizeof(icUInt8Number)*m_nSize);

  return *this;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::~CIccTagEmbeddedNormalImage
*
* Purpose: Destructor
*
*****************************************************************************
*/
CIccTagEmbeddedNormalImage::~CIccTagEmbeddedNormalImage()
{
  if (m_pData)
    free(m_pData);
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::Read
*
* Purpose: Read in the tag contents into a data block
*
* Args:
*  size - # of bytes in tag,
*  pIO - IO object to read tag from
*
* Return:
*  true = successful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedNormalImage::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  if (sizeof(icTagTypeSignature) +
    sizeof(icUInt32Number) +
    sizeof(icUInt32Number) +
    sizeof(icUInt32Number) +
    sizeof(icUInt8Number) > size)
    return false;

  if (!pIO) {
    return false;
  }

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nSeamlesIndicator))
    return false;

  if (!pIO->Read32(&m_nEncodingFormat))
    return false;

  icUInt32Number nNum = size - 4 * sizeof(icUInt32Number);

  if (!SetSize(nNum))
    return false;

  if (pIO->Read8(m_pData, nNum) != nNum)
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::Write
*
* Purpose: Write the tag to a file
*
* Args:
*  pIO - The IO object to write tag to.
*
* Return:
*  true = succesful, false = failure
*****************************************************************************
*/
bool CIccTagEmbeddedNormalImage::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nSeamlesIndicator))
    return false;

  if (!pIO->Write32(&m_nEncodingFormat))
    return false;

  if (pIO->Write8(m_pData, m_nSize) != m_nSize)
    return false;

  return true;
}


/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::Describe
*
* Purpose: Dump data associated with the tag to a string
*
* Args:
*  sDescription - string to concatenate tag dump to
*****************************************************************************
*/
void CIccTagEmbeddedNormalImage::Describe(std::string &sDescription, int /* nVerboseness */)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "\nSeamlessIndicater: %d\n", m_nSeamlesIndicator);
  sDescription += buf;

  switch (m_nEncodingFormat) {
  case icPngImageType:
    strcpy(buf, "EncodingFormat: PNG\n");
    break;
  case icTiffImageType:
    strcpy(buf, "EncodingFormat: TIFF\n");
    break;
  default:
    snprintf(buf, bufSize, "EncodingFormat: %d", m_nEncodingFormat);
    break;
  }
  sDescription += buf;

  sDescription += "\nImage Data:\n";

  for (int i = 0; i < (int)m_nSize; i++) {
    if (!(i & 0x1f))
      sDescription += "\n";
    snprintf(buf, bufSize, "%02X", m_pData[i]);
    sDescription += buf;
  }
  sDescription += "\n";
}

/**
****************************************************************************
* Name: CIccTagEmbeddedNormalImage::SetSize
*
* Purpose: Sets the size of the data array.
*
* Args:
*  nSize - number of data entries,
*  bZeroNew - flag to zero newly formed values
*****************************************************************************
*/
bool CIccTagEmbeddedNormalImage::SetSize(icUInt32Number nSize, bool bZeroNew/*=true*/)
{
  if (m_nSize == nSize)
    return true;

  m_pData = (icUInt8Number*)icRealloc(m_pData, nSize * sizeof(icUInt8Number));

  if (!m_pData) {
    m_nSize = 0;
    return false;
  }

  if (bZeroNew && nSize > m_nSize) {
    memset(&m_pData[m_nSize], 0, (nSize - m_nSize) * sizeof(icUInt8Number));
  }
  m_nSize = nSize;

  return true;
}


/**
******************************************************************************
* Name: CIccTagEmbeddedNormalImage::Validate
*
* Purpose: Check tag data validity.
*
* Args:
*  sig = signature of tag being validated,
*  sReport = String to add report information to
*
* Return:
*  icValidateStatusOK if valid, or other error status.
******************************************************************************
*/
icValidateStatus CIccTagEmbeddedNormalImage::Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile/*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  switch (m_nSeamlesIndicator) {
  case 0:
  case 1:
    break;
  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid data flag Seamless Indicator.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  switch (m_nEncodingFormat) {
  case icPngImageType:
    if (m_nSize > 4 && m_pData) {
      unsigned char PNGhdr[] = { 'P', 'N', 'G' };
      if (memcmp(m_pData+1, PNGhdr, 3)) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - Non-PNG file stored as Embedded image.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
    break;
  case icTiffImageType:
    if (m_nSize > 4 && m_pData) {
      unsigned char IIhdr[] = { 'I', 'I', 42, 0 };
      unsigned char MMhdr[] = { 'M', 'M', 0, 42 };

      if (memcmp(m_pData, IIhdr, 4) && memcmp(m_pData, MMhdr, 4)) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - Non-TIFF file stored as Embedded image.\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
      }
    }
    break;
  default:
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - Invalid data flag Encoding Image Type.\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  return rv;
}



#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
