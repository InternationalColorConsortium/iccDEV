/** @file
File:       IccIoXml.cpp

Contains:   Implementation of the IIccOpenFileIO interface.

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

#include "IccIoXml.h"
#include "IccXmlConfig.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
****************************************************************************
* Name: CIccStandardFileIO::OpenFile
* 
* Purpose: Open a file 
* 
* Args: 
*  szFilename - path to file to be opened
*  szAttr - attributes of open (identical to attributes in fopen call).
* 
* Return: 
*  Pointer a CIccIO object that IO can be performed with or NULL if file
*  cannot be opened.
*****************************************************************************
*/
CIccIO* CIccStandardFileIO::OpenFile(const icChar *szFilename, const char *szAttr)
{
  CIccFileIO *file = new CIccFileIO();

  if (!file->Open(szFilename, szAttr)) {
    delete file;
    return NULL;
  }

  return file;
}

static CIccStandardFileIO g_IccStandardFileIO;
static IIccOpenFileIO *g_pIccFileIO = &g_IccStandardFileIO;

CIccIO* IccOpenFileIO(const icChar *szFilename, const char *szAttr)
{
  if (g_pIccFileIO) {
    return g_pIccFileIO->OpenFile(szFilename, szAttr);
  }

  return NULL;
}

void IccSetOpenFileIO(IIccOpenFileIO *pOpenIO)
{
  g_pIccFileIO = pOpenIO;
}

// Library-wide flag. Default off; iccFromXml sets it on after argv parse.
// Hidden file-scope global accessed via exported accessors (see
// IccXmlConfig.h) so the DLL interface works on Windows MSVC where
// WINDOWS_EXPORT_ALL_SYMBOLS does not export data symbols.
static bool g_IccXmlAllowFileIncludes = false;

void IccXmlSetAllowFileIncludes(bool allow) { g_IccXmlAllowFileIncludes = allow; }
bool IccXmlGetAllowFileIncludes() { return g_IccXmlAllowFileIncludes; }

bool IccXmlIsPathSafe(const icChar *szFilename)
{
  if (!szFilename || !szFilename[0]) return false;

  // Reject absolute paths: POSIX "/foo", Windows "\foo" or "C:\foo".
  if (szFilename[0] == '/' || szFilename[0] == '\\') return false;
  if (szFilename[1] == ':') return false;

  // Reject any ".." path component (prevents traversal across a leading
  // "../", a middle "/../", and a trailing "/.."; also the bare "..").
  for (const char *p = szFilename; *p; ) {
    if (p[0] == '.' && p[1] == '.' &&
        (p[2] == '\0' || p[2] == '/' || p[2] == '\\')) {
      if (p == szFilename) return false;
      char prev = p[-1];
      if (prev == '/' || prev == '\\') return false;
    }
    ++p;
  }

  return true;
}

CIccIO* IccXmlSafeOpenFileIO(const icChar *szFilename, const char *szAttr)
{
  if (!g_IccXmlAllowFileIncludes) return NULL;
  if (!IccXmlIsPathSafe(szFilename)) return NULL;
  return IccOpenFileIO(szFilename, szAttr);
}

//////////////////////////////////////////////////////////////////////
// Class CIccIO
//////////////////////////////////////////////////////////////////////

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
