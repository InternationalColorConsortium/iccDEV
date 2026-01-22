/** @file
    File:       IccTagDictTag.cpp

    Contains:   Implementation of prototype dictType Tag

    Version:    V1

    Copyright:  � see ICC Software License
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
// -Initial implementation by Max Derhak Jun-26-2009
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
//#include <codecvt>
#include <locale>
#include "IccTagDict.h"
#include "IccUtil.h"
#include "IccIO.h"
#include "IccConvertUTF.h"


//MSVC 6.0 doesn't support std::string correctly so we disable support in this case
#ifndef ICC_UNSUPPORTED_TAG_DICT


/*=============================================================================
* CLASS CIccDictEntry
*=============================================================================*/

/**
******************************************************************************
* Name: CIccDictEntry::CIccDictEntry
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccDictEntry::CIccDictEntry()
{
  m_sName = new std::wstring();
  m_sValue = new std::wstring();
  m_pNameLocalized = NULL;
  m_pValueLocalized = NULL;
  m_bValueSet = false;
}

/**
******************************************************************************
* Name: CIccDictEntry::CIccDictEntry
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccDictEntry::CIccDictEntry(const CIccDictEntry& IDE)
{
  m_sName = new std::wstring();
  m_sValue = new std::wstring();

  *m_sName = *IDE.m_sName;
  m_bValueSet = IDE.m_bValueSet;
  *m_sValue = *IDE.m_sValue;

  if (IDE.m_pNameLocalized) {
    m_pNameLocalized = (CIccTagMultiLocalizedUnicode*)IDE.m_pNameLocalized->NewCopy();
  }
  else
    m_pNameLocalized = NULL;

  if (IDE.m_pValueLocalized) {
    m_pValueLocalized = (CIccTagMultiLocalizedUnicode*)IDE.m_pValueLocalized->NewCopy();
  }
  else
    m_pValueLocalized = NULL;
}

/**
******************************************************************************
* Name: CIccDictEntry::operator=
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccDictEntry &CIccDictEntry::operator=(const CIccDictEntry &IDE)
{
  if (&IDE == this)
    return *this;

  if (m_pNameLocalized)
    delete m_pNameLocalized;

  if (m_pValueLocalized)
    delete m_pValueLocalized;

  *m_sName = *IDE.m_sName;
  m_bValueSet = IDE.m_bValueSet;
  *m_sValue = *IDE.m_sValue;

  if (IDE.m_pNameLocalized) {
    m_pNameLocalized = (CIccTagMultiLocalizedUnicode*)IDE.m_pNameLocalized->NewCopy();
  }
  else
    m_pNameLocalized = NULL;

  if (IDE.m_pValueLocalized) {
    m_pValueLocalized = (CIccTagMultiLocalizedUnicode*)IDE.m_pValueLocalized->NewCopy();
  }
  else
    m_pValueLocalized = NULL;

  return *this;
}

/**
******************************************************************************
* Name: CIccDictEntry::~CIccDictEntry
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccDictEntry::~CIccDictEntry()
{
  delete m_sName;
  delete m_sValue;
  delete m_pNameLocalized;
  delete m_pValueLocalized;
}

/**
******************************************************************************
* Name: wstringToUTF8Converter
*
* Purpose: convert wstring to UTF8 string for display
*
* Args: std::wstring
*
* Return: std::string
******************************************************************************/
static
std::string wstringToUTF8Converter( std::wstring &input )
{

#if 1
  size_t maxBufferSize = 6 * ( input.size() + 1 );  // assume worst case conversion to UTF8
  std::vector<char> outputBuffer ( maxBufferSize );
  UTF8 *output_data( (UTF8 *)outputBuffer.data() );
  UTF8 *output_end = output_data + maxBufferSize;
  UTF8 *output_start = output_data;  // because this will be modified in the conversion routine

  // wstring can be 16 or 32 bits depending on platform - so we need a conditional implementation to convert to UTF8
  static_assert( sizeof(wchar_t) == 4 || sizeof(wchar_t) == 2, "wchar_t has an unexpected size." );
  if (sizeof(wchar_t) == 4) {
    const UTF32 *input_data( (const UTF32 *)input.data() );
    const UTF32 *input_end = input_data + input.size();
    (void) icConvertUTF32toUTF8 ( &input_data, input_end, &output_start, output_end, lenientConversion );
  } else  {
    const UTF16 *input_data( (const UTF16 *)input.data() );
    const UTF16 *input_end = input_data + input.size();
    (void) icConvertUTF16toUTF8 ( &input_data, input_end, &output_start, output_end, lenientConversion );
  }

// output_start has been moved to point to the end of the output, and should have a terminating NULL

  return std::string ( (char *)output_data );     // this makes a copy of the output string data
#else
// deprecated implementation that still works for now, but is marked for removal in C++26
// saving this, just in case the new code fails in testing
  using convert_type = std::codecvt_utf8<wchar_t>;
  std::wstring_convert<convert_type, wchar_t> converter;
  return converter.to_bytes( input );
#endif

}


/**
******************************************************************************
* Name: CIccDictEntry::Describe
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
void CIccDictEntry::Describe(std::string &sDescription, int nVerboseness)
{
  std::wstring ws;

  sDescription += "BEGIN DICT_ENTRY\nName=";
  ws.assign(m_sName->begin(), m_sName->end());
  sDescription += wstringToUTF8Converter(ws);
  sDescription += "\nValue=";
  ws.assign(m_sValue->begin(), m_sValue->end());
  sDescription += wstringToUTF8Converter(ws);
  sDescription += "\n";

  if (m_pNameLocalized) {
    sDescription += "BEGIN NAME_LOCALIZATION\n";
    m_pNameLocalized->Describe(sDescription, nVerboseness);
    sDescription += "END NAME_LOCALIZATION\n";
  }
  if (m_pValueLocalized) {
    sDescription += "BEGIN VALUE_LOCALIZATION\n";
    m_pValueLocalized->Describe(sDescription, nVerboseness);
    sDescription += "END VALUE_LOCALIZATION\n";
  }
  sDescription += "END DICT_ENTRY\n";
}


/**
******************************************************************************
* Name: CIccDictEntry::PosRecSize
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icUInt32Number CIccDictEntry::PosRecSize()
{
  if (m_pValueLocalized)
    return 32;
  if (m_pNameLocalized)
    return 24;
  return 16;
}


bool CIccDictEntry::SetValue(std::wstring sValue)
{
  bool rv = m_bValueSet && !m_sValue->empty();

  *m_sValue = sValue;
  m_bValueSet = true;
  return rv;
}

/**
******************************************************************************
* Name: CIccDictEntry::SetNameLocalized
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccDictEntry::SetNameLocalized(CIccTagMultiLocalizedUnicode *pNameLocalized)
{
  bool rv;

  if (m_pNameLocalized) {
    delete m_pNameLocalized;
    rv = true;
  }
  else
    rv = false;

  m_pNameLocalized = pNameLocalized;

  return rv;
}


/**
******************************************************************************
* Name: CIccDictEntry::SetValueLocalized
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccDictEntry::SetValueLocalized(CIccTagMultiLocalizedUnicode *pValueLocalized)
{
  bool rv;

  if (m_pValueLocalized) {
    delete m_pValueLocalized;
    rv = true;
  }
  else
    rv = false;

  m_pValueLocalized = pValueLocalized;

  return rv;
}

/*=============================================================================
 * CLASS CIccTagDict
 *============================================================================*/

/**
 ******************************************************************************
 * Name: CIccTagDict::CIccTagDict
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagDict::CIccTagDict()
{
    m_tagSize = m_tagStart = 0;
    m_bBadAlignment = false;

  m_Dict = new CIccNameValueDict;
}

/**
 ******************************************************************************
 * Name: CIccTagDict::CIccTagDict
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagDict::CIccTagDict(const CIccTagDict &dict)
{
  m_tagSize = m_tagStart = 0;
  m_bBadAlignment = false;
  m_Dict = new CIccNameValueDict;

  CIccNameValueDict::iterator i;
  CIccDictEntryPtr ptr = {};

  for (i=dict.m_Dict->begin(); i!=dict.m_Dict->end(); i++) {
    ptr.ptr = new CIccDictEntry(*i->ptr);

    m_Dict->push_back(ptr);
  }
}

/**
 ******************************************************************************
 * Name: &operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagDict &CIccTagDict::operator=(const CIccTagDict &dict)
{
  if (&dict == this)
    return *this;

  Cleanup();

  CIccNameValueDict::iterator i;
  CIccDictEntryPtr ptr = {};

  for (i=dict.m_Dict->begin(); i!=dict.m_Dict->end(); i++) {
    ptr.ptr = new CIccDictEntry(*i->ptr);

    m_Dict->push_back(ptr);
  }

  return *this;
}

/**
 ******************************************************************************
 * Name: CIccTagDict::~CIccTagDict
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccTagDict::~CIccTagDict()
{
  Cleanup();
  delete m_Dict;
}


/**
******************************************************************************
* Name: CIccTagDict::MaxPosRecSize;
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icUInt32Number CIccTagDict::MaxPosRecSize()
{

  icUInt32Number rv = 16;

  CIccNameValueDict::iterator i;
  CIccDictEntry ptr;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    if (i->ptr->PosRecSize() > rv)
      rv = i->ptr->PosRecSize();
  }

  return rv;
}


/**
 ******************************************************************************
 * Name: CIccTagDict::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccTagDict::Describe(std::string &sDescription, int nVerboseness)
{
  const size_t bufSize = 128;
  icChar buf[bufSize];

  snprintf(buf, bufSize, "BEGIN DICT_TAG\n");
  sDescription += buf;

  CIccNameValueDict::iterator i;
  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    sDescription += "\n";

    i->ptr->Describe(sDescription, nVerboseness);
  }

  snprintf(buf, bufSize, "\nEND DICT_TAG\n");
  sDescription += buf;
}

typedef struct
{
  icPositionNumber posName;
  icPositionNumber posValue;
  icPositionNumber posNameLocalized;
  icPositionNumber posValueLocalized;
} icDictRecordPos;


/**
 ******************************************************************************
 * Name: CIccTagDict::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccTagDict::Read(icUInt32Number size, CIccIO *pIO)
{
  icTagTypeSignature sig;

  m_tagSize = size;
  
  icUInt32Number headerSize = sizeof(icTagTypeSignature) + 
    sizeof(icUInt32Number) + 
    sizeof(icUInt32Number) +
    sizeof(icUInt32Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  Cleanup();

  m_tagStart = (icUInt32Number) pIO->Tell();

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  icUInt32Number count, reclen, i;

  if (!pIO->Read32(&count))
    return false;

  if (!pIO->Read32(&reclen))
    return false;

  if (reclen!=16 && reclen!=24 && reclen!=32)
    return false;

  if (headerSize + count*reclen > size)
    return false;

  icDictRecordPos *pos = (icDictRecordPos*)calloc(count, sizeof(icDictRecordPos));
  if (!pos)
    return false;

  //Read TagDir
  for (i=0; i<count; i++) {
    if (!pIO->Read32(&pos[i].posName.offset) ||
        !pIO->Read32(&pos[i].posName.size) ||
        !pIO->Read32(&pos[i].posValue.offset) ||
        !pIO->Read32(&pos[i].posValue.size)) {
      free(pos);
      return false;
    }

    if (reclen>=24) {
      if (!pIO->Read32(&pos[i].posNameLocalized.offset) ||
          !pIO->Read32(&pos[i].posNameLocalized.size)) {
        free(pos);
        return false;
      }
      if (reclen>=32) {
        if (!pIO->Read32(&pos[i].posValueLocalized.offset) ||
            !pIO->Read32(&pos[i].posValueLocalized.size)) {
          free(pos);
          return false;
        }
      }
    }

    if ((pos[i].posName.offset & 0x3) ||
        (pos[i].posValue.offset & 0x3) ||
        (pos[i].posNameLocalized.offset & 0x3) ||
        (pos[i].posValueLocalized.offset & 0x3))
      m_bBadAlignment = true;
  }

  icUInt32Number bufsize = 128, num;
  icUnicodeChar *buf = (icUnicodeChar*)malloc(bufsize);
  CIccDictEntryPtr ptr = {};
  std::wstring str;

  for (i=0; i<count; i++) {
    ptr.ptr = new CIccDictEntry();
    if (!ptr.ptr)
      return false;

    //GetName
    if (pos[i].posName.offset) {
      if (!pos[i].posName.size) {
        str.clear();
        ptr.ptr->SetValue(str);
      }
      else {
        if (pos[i].posName.offset + pos[i].posName.size >size ||
            !pos[i].posName.size) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
        }

        //Make sure we have buf large enough for the string
        if (bufsize < pos[i].posName.size) {
          bufsize = pos[i].posName.size;
          buf = (icUnicodeChar*)icRealloc(buf, bufsize);
          if (!buf) {
            free(pos);
            delete ptr.ptr;
            return false;
          }
        }

        if (pIO->Seek(m_tagStart+pos[i].posName.offset, icSeekSet)<0) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
        }

        num = pos[i].posName.size / sizeof(icUnicodeChar);
        if (pIO->Read16(buf, num)!= num) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
        }
        str.assign(&buf[0], &buf[num]);
        ptr.ptr->GetName() = str;
      }
    }

    //GetValue
    if (pos[i].posValue.offset) {
      if (!pos[i].posValue.size) {
        str.clear();
        ptr.ptr->SetValue(str);
      }
      else {
        if (pos[i].posValue.offset + pos[i].posValue.size >size ||
            (pos[i].posValue.size&1)) {
            free(pos);
            free(buf);
            delete ptr.ptr;
            return false;
        }

        //Make sure we have buf large enough for the string
        if (bufsize < pos[i].posValue.size) {
          bufsize = pos[i].posValue.size;
          buf = (icUnicodeChar*)icRealloc(buf, bufsize);
          if (!buf) {
            free(pos);
            delete ptr.ptr;
            return false;
          }
        }

        if (pIO->Seek(m_tagStart+pos[i].posValue.offset, icSeekSet)<0) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
        }

        num = pos[i].posValue.size / sizeof(icUnicodeChar);
        if (pIO->Read16(buf, num)!=num) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
        }
        str.assign(&buf[0], &buf[num]);
        ptr.ptr->SetValue(str);
      }
    }

    //Get NameLocalized
    if (pos[i].posNameLocalized.offset) {
      if (pos[i].posNameLocalized.offset + pos[i].posNameLocalized.size > size ||
          pos[i].posNameLocalized.size < sizeof(icSignature)) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      if (pIO->Seek(m_tagStart+pos[i].posNameLocalized.offset, icSeekSet)<0) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      icTagTypeSignature textSig = (icTagTypeSignature)0;
      if (!pIO->Read32(&textSig, 1)) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      if (textSig != icSigMultiLocalizedUnicodeType) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      if (pIO->Seek(m_tagStart+pos[i].posNameLocalized.offset, icSeekSet)<0) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      CIccTagMultiLocalizedUnicode *pTag = new CIccTagMultiLocalizedUnicode();

      if (!pTag || !pTag->Read(pos[i].posNameLocalized.size, pIO)) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      ptr.ptr->SetNameLocalized(pTag);
    }

    //Get ValueLocalized
    if (pos[i].posValueLocalized.offset) {
      if (pos[i].posValueLocalized.offset + pos[i].posValueLocalized.size > size ||
        pos[i].posValueLocalized.size < sizeof(icSignature)) {
          free(pos);
          free(buf);
          delete ptr.ptr;
          return false;
      }

      if (pIO->Seek(m_tagStart+pos[i].posValueLocalized.offset, icSeekSet)<0) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      icTagTypeSignature textSig = (icTagTypeSignature)0;
      if (!pIO->Read32(&textSig, 1)) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      if (textSig != icSigMultiLocalizedUnicodeType) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      if (pIO->Seek(m_tagStart+pos[i].posValueLocalized.offset, icSeekSet)<0) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      CIccTagMultiLocalizedUnicode *pTag = new CIccTagMultiLocalizedUnicode();

      if (!pTag || !pTag->Read(pos[i].posValueLocalized.size, pIO)) {
        free(pos);
        free(buf);
        delete ptr.ptr;
        return false;
      }

      ptr.ptr->SetValueLocalized(pTag);
    }

    m_Dict->push_back(ptr);
  }

  free(pos);
  free(buf);

  return true;
}

/**
 ******************************************************************************
 * Name: CIccTagDict::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccTagDict::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  m_tagStart = (icUInt32Number) pIO->Tell();

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  CIccNameValueDict::iterator i, j;
  icUInt32Number count;
  icUInt8Number zbuf[32];
  
  memset(zbuf, 0, 32);

  for (count=0, i=m_Dict->begin(); i!= m_Dict->end(); i++) {
    if (i->ptr)
      count++;
  }

  pIO->Write32(&count);

  icUInt32Number recSize = MaxPosRecSize();
  pIO->Write32(&recSize);

  icDictRecordPos *pos = (icDictRecordPos*)calloc(count, sizeof(icDictRecordPos));
  if (!pos)
    return false;

  icUInt32Number n;
  size_t dirpos = pIO->Tell();

  //Write Unintialized Dict rec offset array
  for (i=m_Dict->begin(); i!= m_Dict->end(); i++) {
    if (i->ptr) {
      pIO->Write8(zbuf, recSize);
    }
  }

  icUnicodeChar c;
  std::wstring::const_iterator chrptr;
  //Write Dict records
  for (n=0, i=m_Dict->begin(); i!= m_Dict->end(); i++) {
    if (i->ptr) {
      pos[n].posName.offset = (icUInt32Number)( pIO->Tell()-m_tagStart );

      for(chrptr = i->ptr->GetName().begin(); chrptr!=i->ptr->GetName().end(); chrptr++) {
        c=(icUnicodeChar)*chrptr;
        pIO->Write16(&c, 1);
      }
      pos[n].posName.size = (icUInt32Number)(pIO->Tell()-m_tagStart - pos[n].posName.offset);
      pIO->Align32();

      if (i->ptr->IsValueSet()) {
        pos[n].posValue.offset = (icUInt32Number)(pIO->Tell()-m_tagStart);
        for(chrptr = i->ptr->ValueBegin(); chrptr!=i->ptr->ValueEnd(); chrptr++) {
          c=(icUnicodeChar)*chrptr;
          pIO->Write16(&c, 1);
        }
        pos[n].posValue.size = (icUInt32Number)(pIO->Tell()-m_tagStart - pos[n].posValue.offset);
        pIO->Align32();
      }

      if (recSize>16 && i->ptr->GetNameLocalized()) {
        pos[n].posNameLocalized.offset = (icUInt32Number)(pIO->Tell()-m_tagStart);
        i->ptr->GetNameLocalized()->Write(pIO);
        pos[n].posNameLocalized.size = (icUInt32Number)(pIO->Tell()-m_tagStart - pos[n].posNameLocalized.offset);
        pIO->Align32();
      }

      if (recSize>24 && i->ptr->GetValueLocalized()) {
        pos[n].posValueLocalized.offset = (icUInt32Number)(pIO->Tell()-m_tagStart);
        i->ptr->GetValueLocalized()->Write(pIO);
        pos[n].posValueLocalized.size = (icUInt32Number)(pIO->Tell()-m_tagStart - pos[n].posValueLocalized.offset);
        pIO->Align32();
      }
      n++;
    }
  }
  size_t endpos = pIO->Tell();

  pIO->Seek(dirpos, icSeekSet);

  //Write TagDir with offsets and sizes
  for (n=0, i=m_Dict->begin(); i!= m_Dict->end(); i++) {
    if (i->ptr) {
      pIO->Write32(&pos[n].posName.offset);
      pIO->Write32(&pos[n].posName.size);
      pIO->Write32(&pos[n].posValue.offset);
      pIO->Write32(&pos[n].posValue.size);
      if (recSize>16) {
        pIO->Write32(&pos[n].posNameLocalized.offset);
        pIO->Write32(&pos[n].posNameLocalized.size);
        if (recSize>24) {
          pIO->Write32(&pos[n].posValueLocalized.offset);
          pIO->Write32(&pos[n].posValueLocalized.size);
        }
      }
    }
    n++;
  }
  pIO->Seek(endpos, icSeekSet);

  free(pos);

  return true;
}


/**
 ******************************************************************************
 * Name: CIccTagDict::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccTagDict::Validate(std::string sigPath, std::string &sReport,
                                           const CIccProfile* pProfile /*=NULL*/) const
{ 
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  // Check for duplicate tags
  if (!AreNamesUnique()) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - There are duplicate names.\n";
    rv =icMaxStatus(rv, icValidateWarning);
  }

  if (!AreNamesNonzero()) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - There zero-length names.\n";
    rv =icMaxStatus(rv, icValidateWarning);
  }

  // Check for duplicate tags
  if (m_bBadAlignment) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " - Some Data elements are not aligned correctly\n";
    rv =icMaxStatus(rv, icValidateWarning);
  }

  return rv;
}

/**
 ***************************************************************************
 * Name: CIccTagDict::Cleanup
 * 
 * Purpose: Detach from a pending IO object
 ***************************************************************************
 */
void CIccTagDict::Cleanup()
{
  CIccNameValueDict::iterator i;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    if (i->ptr)
      delete i->ptr;
  }
  m_Dict->clear();
}

/**
******************************************************************************
* Name: CIccTagDict::AreNamesUnique
* 
* Purpose: For each tag it checks to see if any other tags have the same
*  signature.
* 
* 
* Return: 
*  true if all tags have unique signatures, or false if there are duplicate
*  tag signatures.
*******************************************************************************
*/
bool CIccTagDict::AreNamesUnique() const
{
  CIccNameValueDict::const_iterator i, j;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    j=i;
    for (j++; j!= m_Dict->end(); j++) {
      if (i->ptr->GetName() == j->ptr->GetName())
        return false;
    }
  }

  return true;
}

/**
******************************************************************************
* Name: CIccTagDict::AreNamesNonzero
* 
* Purpose: For each tag it checks to see if any other tags have the same
*  signature.
* 
* 
* Return: 
*  true if all tags have unique signatures, or false if there are duplicate
*  tag signatures.
*******************************************************************************
*/
bool CIccTagDict::AreNamesNonzero() const
{
  CIccNameValueDict::const_iterator i, j;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    if (i->ptr->GetName().empty())
      return false;
  }

return true;
}


/**
 ****************************************************************************
 * Name: CIccTagDict::Get
 * 
 * Purpose: Get a dictionary entry with a given name
 * 
 * Args: 
 *  sName - name to find in dictionary
 * 
 * Return: 
 *  Pointer to desired dictionary entry, or NULL if not found.
 *****************************************************************************
 */
CIccDictEntry* CIccTagDict::Get(std::wstring sName) const
{
  CIccNameValueDict::const_iterator i;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    if (i->ptr->GetName() == sName)
      return i->ptr;
  }

  return NULL;
}

/**
****************************************************************************
* Name: CIccTagDict::Get
* 
* Purpose: Get a dictionary entry with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  Pointer to desired dictionary entry, or NULL if not found.
*****************************************************************************
*/
CIccDictEntry* CIccTagDict::Get(const icUnicodeChar *szName) const
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  return Get(sName);
}


/**
****************************************************************************
* Name: CIccTagDict::Get
* 
* Purpose: Get a dictionary entry with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  Pointer to desired dictionary entry, or NULL if not found.
*****************************************************************************
*/
CIccDictEntry* CIccTagDict::Get(const char *szName) const
{
  std::wstring sName(szName, szName+strlen(szName));

  return Get(sName);
}

/**
****************************************************************************
* Name: CIccTagDict::GetValue
* 
* Purpose: Get a value associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  Pointer to desired dictionary entry, or NULL if not found.
*****************************************************************************
*/
std::wstring CIccTagDict::GetValue(std::wstring sName, bool *bIsSet) const
{
  CIccDictEntry *de = Get(sName);

  if (de) {
  
    if (bIsSet)
      *bIsSet = de->IsValueSet();

    return de->GetValue();
  }

  if (bIsSet)
    *bIsSet = false;

  std::wstring str;
  return str;
}

/**
****************************************************************************
* Name: CIccTagDict::GetValue
* 
* Purpose: Get a value associated with a given name
* 
* Args: 
*  szName - name to find in dictionary
* 
* Return: 
*  Pointer to desired dictionary entry, or NULL if not found.
*****************************************************************************
*/
std::wstring CIccTagDict::GetValue(const icUnicodeChar *szName, bool *bIsSet) const
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  return GetValue(sName, bIsSet);
}

/**
****************************************************************************
* Name: CIccTagDict::GetValue
* 
* Purpose: Get a value associated with a given name
* 
* Args: 
*  szName - name to find in dictionary
* 
* Return: 
*  Pointer to desired dictionary entry, or NULL if not found.
*****************************************************************************
*/
std::wstring CIccTagDict::GetValue(const char *szName, bool *bIsSet) const
{
  std::wstring sName(szName, szName+strlen(szName));

  return GetValue(sName, bIsSet);
}

/**
****************************************************************************
* Name: CIccTagDict::GetNameLocalized
* 
* Purpose: Get a localized name information associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name
*****************************************************************************
*/
CIccTagMultiLocalizedUnicode* CIccTagDict::GetNameLocalized(std::wstring sName) const
{
  CIccDictEntry *de = Get(sName);

  if (de)
    return de->GetNameLocalized();

  return NULL;
}

/**
****************************************************************************
* Name: CIccTagDict::GetNameLocalized
* 
* Purpose: Get a localized name information associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name
*****************************************************************************
*/
CIccTagMultiLocalizedUnicode* CIccTagDict::GetNameLocalized(const icUnicodeChar *szName) const
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  return GetNameLocalized(sName);
}

/**
****************************************************************************
* Name: CIccTagDict::GetNameLocalized
* 
* Purpose: Get a localized name information associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name
*****************************************************************************
*/

CIccTagMultiLocalizedUnicode* CIccTagDict::GetNameLocalized(const char *szName) const
{
  std::wstring sName(szName, szName+strlen(szName));

  return GetNameLocalized(sName);
}


/**
****************************************************************************
* Name: CIccTagDict::GetValueLocalized
* 
* Purpose: Get a localized name information for value associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name's value
*****************************************************************************
*/
CIccTagMultiLocalizedUnicode* CIccTagDict::GetValueLocalized(std::wstring sName) const
{
  CIccDictEntry *de = Get(sName);

  if (de)
    return de->GetValueLocalized();

  return NULL;
}

/**
****************************************************************************
* Name: CIccTagDict::GetValueLocalized
* 
* Purpose: Get a localized name information for value associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name's value
*****************************************************************************
*/
CIccTagMultiLocalizedUnicode* CIccTagDict::GetValueLocalized(const icUnicodeChar *szName) const
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  return GetValueLocalized(sName);
}

/**
****************************************************************************
* Name: CIccTagDict::GetValueLocalized
* 
* Purpose: Get a localized name information for value associated with a given name
* 
* Args: 
*  sName - name to find in dictionary
* 
* Return: 
*  localized information for name's value
*****************************************************************************
*/

CIccTagMultiLocalizedUnicode* CIccTagDict::GetValueLocalized(const char *szName) const
{
  std::wstring sName(szName, szName+strlen(szName));

  return GetValueLocalized(sName);
}


/**
******************************************************************************
* Name: CIccTagDict::Remove
* 
* Purpose: Remove a name value pair from the dictionary
*
* Args:
*  sName = the name to look for in the dictionary
* 
* Return: 
*  true if sName exists and was removed, or false otherwise
*******************************************************************************
*/
bool CIccTagDict::Remove(std::wstring sName)
{
  CIccNameValueDict::iterator i;

  for (i=m_Dict->begin(); i!=m_Dict->end(); i++) {
    if (i->ptr->GetName() == sName) {
      delete i->ptr;

      m_Dict->erase(i);
      return true;
    }
  }

  return false;
}


/**
******************************************************************************
* Name: CIccTagDict::Remove
* 
* Purpose: Remove a name value pair from the dictionary
*
* Args:
*  sName = the name to look for in the dictionary
* 
* Return: 
*  true if sName exists and was remove , or false otherwise
*******************************************************************************
*/
bool CIccTagDict::Remove(const icUnicodeChar *szName)
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  return Remove(sName);

}


/**
******************************************************************************
* Name: CIccTagDict::Remove
* 
* Purpose: Remove a name value pair from the dictionary
*
* Args:
*  sName = the name to look for in the dictionary
* 
* Return: 
*  true if sName exists and was remove , or false otherwise
*******************************************************************************
*/
bool CIccTagDict::Remove(const char *szName)
{
  std::wstring sName(szName, szName+strlen(szName));

  return Remove(sName);
}


/**
******************************************************************************
* Name: CIccTagDict::Set
* 
* Purpose: Associate a value with a name in the dictionary
*
* Args:
*  sName = the name to look for in the dictionary
*  sValue = value to associate with sName
*  bUnset = flag to indicate that value has not been set (no data for value in tag)
* 
* Return: 
*  true if value associate with sName was changed, or false if no change
*******************************************************************************
*/
bool CIccTagDict::Set(std::wstring sName, std::wstring sValue, bool bUnSet)
{
  CIccDictEntry *de = Get(sName);

  if (de) {
    if (de->GetValue()==sValue && de->IsValueSet() && !bUnSet)
      return false;
  }
  else {
    de = new CIccDictEntry;
    de->GetName() = sName;

    CIccDictEntryPtr ptr = {};
    ptr.ptr = de;
    m_Dict->push_back(ptr);
  }

  if (sValue.empty() && bUnSet)
    de->UnsetValue();
  else
    de->SetValue(sValue);

  return true;
}

bool CIccTagDict::Set(const icUnicodeChar *szName, const icUnicodeChar *szValue)
{
  std::wstring sName;
  while(*szName)
    sName += *szName;

  std::wstring sValue;

  if (szValue) {
    while(*szValue)
      sValue += *szValue;

    return Set(sName, sValue, false);
  }

  return Set(sName, sValue, true);
}

bool CIccTagDict::Set(const char *szName, const char *szValue)
{
  std::wstring sName(szName, szName+strlen(szName));
  std::wstring sValue;

  if (szValue) {
    sValue.assign(szValue, szValue+strlen(szValue));

    return Set(sName, sValue, false);
  }

  return Set(sName, sValue, true);
}

bool CIccTagDict::SetNameLocalized(std::wstring sName, CIccTagMultiLocalizedUnicode *pTag)
{
  CIccDictEntry *de = Get(sName);

  if (!de) {
    de = new CIccDictEntry;
    de->GetName() = sName;

    CIccDictEntryPtr ptr = {};
    ptr.ptr = de;
    m_Dict->push_back(ptr);
  }

  return de->SetNameLocalized(pTag);
}

bool CIccTagDict::SetNameLocalized(const icUnicodeChar *szName, CIccTagMultiLocalizedUnicode *pTag)
{
  std::wstring sName;
  while(*szName)
    sName += *szName++;

  return SetNameLocalized(sName, pTag);
}

bool CIccTagDict::SetNameLocalized(const char *szName, CIccTagMultiLocalizedUnicode *pTag)
{
  std::wstring sName;
  while(*szName)
    sName += *szName++;

  return SetNameLocalized(sName, pTag);
}

bool CIccTagDict::SetValueLocalized(std::wstring sName, CIccTagMultiLocalizedUnicode *pTag)
{
  CIccDictEntry *de = Get(sName);

  if (!de) {
    de = new CIccDictEntry;
    de->GetName() = sName;

    CIccDictEntryPtr ptr = {};
    ptr.ptr = de;
    m_Dict->push_back(ptr);
  }

  return de->SetValueLocalized(pTag);
}

bool CIccTagDict::SetValueLocalized(const icUnicodeChar *szName, CIccTagMultiLocalizedUnicode *pTag)
{
  std::wstring sName;
  while(*szName)
    sName += *szName++;

  return SetValueLocalized(sName, pTag);
}

bool CIccTagDict::SetValueLocalized(const char *szName, CIccTagMultiLocalizedUnicode *pTag)
{
  std::wstring sName(szName, szName+strlen(szName));

  return SetValueLocalized(sName, pTag);
}

#endif //ICC_UNSUPPORTED_TAG_DICT
