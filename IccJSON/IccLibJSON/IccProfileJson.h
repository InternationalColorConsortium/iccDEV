/** @file
File:       IccProfileJson.h

Contains:   Header for implementation of the CIccProfileJson class.

Version:    V1

Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
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

#if !defined(_ICCPROFILEJSON_H)
#define _ICCPROFILEJSON_H

#include "IccProfile.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>

using IccJson = nlohmann::json;

class CIccProfileJson : public CIccProfile
{
public:
  CIccProfileJson() : CIccProfile() {}
  CIccProfileJson(const CIccProfileJson &profile) : CIccProfile(profile) {}
  virtual CIccProfile* NewCopy() const { return new CIccProfileJson(*this); }
  virtual CIccProfile* NewProfile() const { return new CIccProfileJson(); }
  virtual ~CIccProfileJson() {}

  virtual const char *GetClassName() const { return "CIccProfileJson"; }

  bool ToJson(std::string &jsonString, int indent = 2);
  bool ToJson(IccJson &j);

  bool ParseJson(const IccJson &j, std::string &parseStr);
  bool LoadJson(const char *szFilename, std::string *parseStr = NULL);

protected:
  bool ParseBasic(const IccJson &j, std::string &parseStr);
  bool ParseTag(const std::string &key, const IccJson &tagValue,
                std::map<std::string, icTagSignature> &keyToSig,
                std::string &parseStr);
};

#endif /* _ICCPROFILEJSON_H */
