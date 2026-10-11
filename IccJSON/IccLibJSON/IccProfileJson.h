/** @file
File:       IccProfileJson.h

Contains:   Header for implementation of the CIccProfileJson class.

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
#ifndef _ICCPROFILEJSON_H
#define _ICCPROFILEJSON_H

#include "IccProfile.h"
#include "IccJsonTypes.h"
#include <map>
#include <unordered_map>
#include <string>

using IccJson = iccJson::json;

// currently breaking on GCC due to difference bug in basic_string.h line 495
// unordered_map doesn't use a signed comparison
//typedef std::map<std::string, icTagSignature>  KeyToSignatureMap;
typedef std::unordered_map<std::string, icTagSignature>  KeyToSignatureMap;

class CIccProfileJson : public CIccProfile
{
public:
  CIccProfileJson() : CIccProfile() {}
  CIccProfileJson(const CIccProfileJson &profile) : CIccProfile(profile) {}
  virtual CIccProfile* NewCopy() const { return new CIccProfileJson(*this); }
  virtual CIccProfile* NewProfile() const { return new CIccProfileJson(); }
  virtual ~CIccProfileJson();

  virtual const char *GetClassName() const { return "CIccProfileJson"; }

  bool ToJson(std::string &jsonString, int indent = 2);
  bool ToJson(IccJson &j);

  bool ParseJson(const IccJson &j, std::string &parseStr);
  bool LoadJson(const char *szFilename, std::string *parseStr = NULL);

protected:
  bool ParseBasic(const IccJson &j, std::string &parseStr);
  bool ParseTag(const std::string &key, const IccJson &tagValue,
                KeyToSignatureMap &keyToSig,
                std::string &parseStr);
  void SetTagColorSpaces(icTagSignature sig, CIccTag *pTag);
};

#endif /* _ICCPROFILEJSON_H */
