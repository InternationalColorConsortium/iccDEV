/** @file
File:       IccProfileXml.h

Contains:   Header for implementation of the CIccProfileXml class.

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
#if !defined(_ICCPROFILEXML_H)
#define _ICCPROFILEXML_H

#include "IccProfile.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/relaxng.h>


class CIccProfileXml :
  public CIccProfile
{
public:
  CIccProfileXml() : CIccProfile() {}
  CIccProfileXml(const CIccProfileXml &profile) : CIccProfile(profile) {}
  virtual CIccProfile* NewCopy() const { return new CIccProfileXml(*this); }
  virtual CIccProfile* NewProfile() const { return new CIccProfileXml(); }
  virtual ~CIccProfileXml() {}

  virtual const char *GetClassName() const { return "CIccProfileXml"; }

  bool ToXml(std::string &xmlString);
  bool ToXmlWithBlanks(std::string &xmlString, std::string blanks);

  bool ParseXml(xmlNode *pNode, std::string &parseStr);
  bool LoadXml(const char *szFilename, const char *szRelaxNGDir, std::string *parseStr=NULL);

protected:
  bool ParseBasic(xmlNode *pNode, std::string &parseStr);
  bool ParseTag(xmlNode *pNode, std::string &parseStr);
};

#endif /* _ICCPROFILEXML_H */
