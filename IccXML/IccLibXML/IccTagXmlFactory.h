/** @file
File:       IccTagXmlFactory.h

Contains:   Header for implementation of a CIccTagXmlFactory class and
creation factories - An extension factory providing ICC XML format
capabilities

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
#ifndef _ICCTAGXMLFACTORY_H
#define _ICCTAGXMLFACTORY_H

#include "IccTagFactory.h"

//CIccTag factory support
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
***********************************************************************
* Class: CIccTagXmlFactory
*
* Purpose:
* CIccSpecTagFactory provides creation of CIccTag's defined by the ICC profile
* specification.  The CIccTagCreator always creates a CIccSpecTagFactory.
***********************************************************************
*/
class ICCPROFLIB_API CIccTagXmlFactory : public IIccTagFactory
{
public:
  /**
  * Function: CreateTag(tagTypeSig)
  *  Create a tag of type tagTypeSig.
  *
  * Parameter(s):
  *  tagTypeSig = signature of the ICC tag type for the tag to be created
  *
  * Returns a new CIccTag object of the given signature type.
  * Unrecognized tagTypeSig's will be created as a CIccTagUnknown object.
  */
  virtual CIccTag* CreateTag(icTagTypeSignature tagSig);

  /**
  * Function: GetTagSigName(tagSig)
  *  Get display name of tagSig.
  *
  * Parameter(s):
  *  tagName = string to put tag name into, 
  *  tagSig = signature of the ICC tag type to get a name for
  *
  * Returns pointer to string containing name of tag if tag is recognized
  * by the factory, NULL if the factory doesn't create tagSig tags.
  */
  virtual const icChar* GetTagSigName(icTagSignature tagSig);

  /**
  * Function: GetTagNameSig(szTagName)
  *  Get signature from tag display name.
  *
  * Parameter(s):
  *  szTagName = name of tag
  *
  * Returns tag signature of tag if tag name is recognized
  * by the factory, icSigUnknownTag if the tag name is not recognized.
  */
  virtual icTagSignature GetTagNameSig(const icChar *szTagName);

  /**
  * Function: GetTagTypeSigName(tagTypeSig)
  *  Get display name of tagTypeSig.
  *
  * Parameter(s):
  *  tagName = string to put tag name into, 
  *  tagTypeSig = signature of the ICC tag type to get a name for
  *
  * Returns pointer to string containing name of tag type if tag is recognized
  * by the factory, NULL if the factory doesn't create tagTypeSig tags.
  */
  virtual const icChar* GetTagTypeSigName(icTagTypeSignature tagTypeSig);

  /**
  * Function: GetTagTypeNameSig(szTypeName)
  *  Get signature based on display name of tag type.
  *
  * Parameter(s):
  *  szTypeName = name of the ICC tag type to get a signature for
  *
  * Returns tag type signature of tag type name if type name is recognized
  * by the factory, icSigUnknownType  if the type name is not recognized.
  */
  virtual icTagTypeSignature GetTagTypeNameSig(const icChar* szTypeName);
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCTAGFACTORY_H
