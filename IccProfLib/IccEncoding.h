/** @file
File:       IccEncode.h

Contains:   Header for implementation of Encoded profile class utiltities

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
// -Separated tags to separate files and created this single header
//  file to declare them all
//
////////////////////////////////////////////////////////////////////// 

#ifndef _ICCENCODING_H
#define _ICCENCODING_H

#include "IccProfile.h"
#include "IccDefs.h"

typedef enum {
  icEncConvertOk = 0,
  icEncConvertBadProfile,
  icEncConvertNoBaseProfile,
  icEncConvertBadBaseProfile,
  icEncConvertBadParams,
  icEncConvertMemoryError,
} icStatusEncConvert;

class ICCPROFLIB_API IIccEncProfileCacheHandler
{
public:
  virtual ~IIccEncProfileCacheHandler() {}
  virtual CIccProfile *GetEncodingProfile(const icUChar *szColorSpaceName)=0;

  static IIccEncProfileCacheHandler* GetHandler();
  static void SetEncCacheHandler(IIccEncProfileCacheHandler *pHandler);
};

class CIccTagStruct;

class ICCPROFLIB_API IIccEncProfileConverter
{
public:
  virtual ~IIccEncProfileConverter() {}
  virtual icStatusEncConvert ConvertFromParams(CIccProfilePtr &newIcc, CIccTagStruct *pParams, icHeader *pHeader)=0;

  static IIccEncProfileConverter* GetHandler();
  static void SetEncProfileConverter(IIccEncProfileConverter *pConverter);
};

icStatusEncConvert ICCPROFLIB_API icConvertEncodingProfile(CIccProfilePtr &newIcc, CIccProfile *pEncodeIcc);

#endif  // _ICCENCODING_H
