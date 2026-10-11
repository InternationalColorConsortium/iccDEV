/** @file
File:       IccIoXml.h

Contains:   Implementation of the IIccOpenFileIO interface.

Version:    V1

Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 6-9-2008
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCIOXML_H)
#define _ICCIOXML_H

#include "IccIO.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

#include "IccIO.h"

/**
**************************************************************************
* Type: Interface
* 
* Purpose: Provides access to file IO operations using CIccIO objects
**************************************************************************
*/
class IIccOpenFileIO
{
public:
  virtual ~IIccOpenFileIO() {}
  virtual CIccIO* OpenFile(const icChar *szFilename, const char *szAttr) = 0;
};


/**
**************************************************************************
* Type: class
* 
* Purpose: Provides default IIccOpenFileIO implementation
**************************************************************************
*/
class ICCPROFLIB_API CIccStandardFileIO : public IIccOpenFileIO
{
public:
  CIccStandardFileIO() {}

  virtual CIccIO* OpenFile(const icChar *szFilename, const char *szAttr);
};

ICCPROFLIB_API void IccSetOpenFileIO(IIccOpenFileIO *pOpenIO);
ICCPROFLIB_API CIccIO* IccOpenFileIO(const icChar *szFilename, const char *szAttr);

// Safe wrapper: gated on IccXmlGetAllowFileIncludes() (default false).
// Rejects absolute paths (leading '/' or '\'), Windows drive-prefixed
// paths (second char ':'), and paths with '..' traversal components.
// All ParseXml callers that open a user-supplied filename should use
// this instead of IccOpenFileIO directly.
ICCPROFLIB_API bool    IccXmlIsPathSafe(const icChar *szFilename);
ICCPROFLIB_API CIccIO* IccXmlSafeOpenFileIO(const icChar *szFilename, const char *szAttr);

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCIOXML_H)
