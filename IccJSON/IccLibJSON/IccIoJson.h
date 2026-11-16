/** @file
File:       IccIoJson.h

Contains:   Pluggable file-IO interface for IccJSON (mirrors IccIoXml.h)

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
#ifndef _ICCIOJSON_H
#define _ICCIOJSON_H

#include "IccIO.h"
#include <string>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 **************************************************************************
 * Type: Interface
 *
 * Purpose: Provides pluggable file-IO for IccJSON (same contract as
 *          IIccOpenFileIO in IccIoXml.h but independent state).
 **************************************************************************
 */
class IIccJsonOpenFileIO
{
public:
  virtual ~IIccJsonOpenFileIO() {}
  virtual CIccIO *OpenFile(const icChar *szFilename, const char *szAttr) = 0;
};


/**
 **************************************************************************
 * Type: Class
 *
 * Purpose: Default IIccJsonOpenFileIO implementation using CIccFileIO.
 **************************************************************************
 */
class ICCPROFLIB_API CIccJsonStandardFileIO : public IIccJsonOpenFileIO
{
public:
  CIccJsonStandardFileIO() {}
  virtual CIccIO *OpenFile(const icChar *szFilename, const char *szAttr);
};

void    ICCPROFLIB_API IccJsonSetOpenFileIO(IIccJsonOpenFileIO *pOpenIO);
CIccIO* ICCPROFLIB_API IccJsonOpenFileIO(const icChar *szFilename, const char *szAttr);

// Opens a file that a JSON document names: a calculator import, CLUT data or
// tint array data.  Every such loader goes through here, so they share one
// rule: the read needs IccJsonSetAllowFileImports(true), and even then only a
// bare name -- no '/', '\', ':' or ".." -- is opened, through
// IccJsonOpenFileIO(). Embedded NULs and Windows device names (including their
// extensions and aliases) are refused on every platform before the opener.
// A file longer than IccJsonMaxImportBytes() is
// refused.  On refusal or failure returns NULL and appends a diagnostic that
// names szWhat (for example "CLUT") to parseStr.
CIccIO* ICCPROFLIB_API IccJsonOpenImportFile(const std::string &filename, const char *szWhat,
                                             std::string &parseStr);

// The size bound for such a file: the build's ICC_JSON_MAX_FILE_BYTES, the
// same bound as the top-level JSON file.
size_t ICCPROFLIB_API IccJsonMaxImportBytes();

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif

#endif // _ICCIOJSON_H
