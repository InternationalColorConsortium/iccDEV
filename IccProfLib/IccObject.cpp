/*
    File:       IccObject.cpp

    Contains:   Implementation of IIccObject

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

#include "IccObject.h"
#include "IccProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


const CIccProfile* IIccObject::GetParentProfile() const
{
  // Two defects met here, and each one alone was enough to lose the profile.
  //
  // The name test compared GetObjectType() -- which CIccProfile forwards to the
  // virtual GetClassName() -- against the literal "CIccProfile".  Every subclass
  // overrides that name, so CIccProfileJson and CIccProfileXml, the two classes
  // the JSON and XML tools actually instantiate, never matched and the walk ran
  // off the top of the tree.
  //
  // The cast was the other half.  IccObject.h only forward-declares CIccProfile
  // (IccObject.h:81), so in this translation unit the type was incomplete and
  // the C-style cast could not be a base-to-derived static_cast -- it degraded
  // to a reinterpret_cast.  CIccProfile derives from IIccProfileConnectionConditions
  // before IIccObject (IccProfile.h:146), so the IIccObject subobject does not sit
  // at offset zero, and the unadjusted pointer was wrong by that offset even on the
  // one path where the name did match.
  //
  // dynamic_cast fixes both at once: it recognises every subclass and performs the
  // required adjustment.  The hierarchy is polymorphic and RTTI is enabled.
  for (const IIccObject* pObj = m_pParentObj; pObj; pObj = pObj->GetParentObject()) {
    if (const CIccProfile* pProfile = dynamic_cast<const CIccProfile*>(pObj))
      return pProfile;
  }
  return nullptr;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
