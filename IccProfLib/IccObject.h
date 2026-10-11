/*
    File:       IccObject.h

    Contains:   IIccObject base interface for ICC object ownership tracking

    Version:    V1

    Copyright:  (c) see below
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
// -Initial implementation by Max Derhak 4-2026
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCOBJECT_H
#define _ICCOBJECT_H

#include "IccProfLibConf.h"
#include <string>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccProfile;

/**
 **************************************************************************
 * Class: IIccObject
 *
 * Purpose: Base interface for ICC library objects that participate in an
 *  ownership hierarchy.  Provides GetParentObject() to walk up the object
 *  tree and GetParentProfile() to reach the owning CIccProfile directly.
 *
 *  Ownership pointers are non-owning (raw) references - the parent object
 *  is responsible for the child's lifetime and sets these via the Set*
 *  methods when attaching a child.
 **************************************************************************
 */
class ICCPROFLIB_API IIccObject
{
public:
  IIccObject() : m_pParentObj(nullptr) {}
  virtual ~IIccObject() {}

  virtual const char* GetObjectType() const { return ""; }

  const IIccObject*  GetParentObject()  const { return m_pParentObj; }
  const CIccProfile* GetParentProfile() const;
  std::string GetObjectPath() const {
    if (m_pParentObj) {
      std::string parentPath = m_pParentObj->GetObjectPath();
      if (!parentPath.empty())
        return parentPath + "/" + GetObjectType();
    }
    return std::string();
  }

  // Parent links are non-owning; callers own parent lifetimes.
  // codeql[cpp/stack-address-escape]
  void SetParentObject(IIccObject* pParent)    { m_pParentObj  = pParent;  }

protected:
  const IIccObject*  m_pParentObj;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCOBJECT_H
