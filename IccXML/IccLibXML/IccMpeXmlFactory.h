/** @file
    File:       IccMpeXmlFactory.h

    Contains:   Header for implementation of CIccMpeXmlFactory class and
                creation factories

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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Feb 4, 2006 
//  A CIccMpeCreator singleton class has been added to provide general
//  support for dynamically creating element classes using a element signature.
//  Prototype and private element type support can be added to the system
//  by pushing additional IIccMpeFactory based objects to the 
//  singleton CIccMpeCreator object.
//
// -Nov 6, 2006
//  Merged into release
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCMPEXMLFACTORY_H
#define _ICCMPEXMLFACTORY_H

#include "IccMpeFactory.h"
#include "IccDefs.h"
#include <memory>
#include <list>

//CIccProcessElement factory support
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif



/**
 ***********************************************************************
 * Class: CIccMpeXmlFactory
 *
 * Purpose:
 * CIccMpeXmlFactory provides creation of CIccProcessElement's 
 * defined by the ICC profile specification.  The CIccMpeCreator always
 * creates a CIccBasicElemFactory.
 ***********************************************************************
 */
class CIccMpeXmlFactory : public IIccMpeFactory
{
public:
  /**
  * Function: CreateElement(elemTypeSig)
  *  Create a element of type elemTypeSig.
  *
  * Parameter(s):
  *  elemTypeSig = signature of the ICC element type for the element to be created
  *
  * Returns a new CIccProcessElement object of the given signature type.
  * Unrecognized elemTypeSig's will be created as a CIccProcessElementUnknown object.
  */
  virtual CIccMultiProcessElement* CreateElement(icElemTypeSignature elementSig);

  /**
  * Function: GetElementSigName(elemTypeSig)
  *  Get display name of elemTypeSig.
  *
  * Parameter(s):
  *  elemName = string to put element name into, 
  *  elemTypeSig = signature of the ICC element type to get a name for
  *
  * Returns true if element type is recognized by the factory, false if the
  * factory doesn't create elemTypeSig elements.
  */
  virtual bool GetElementSigName(std::string &elemName, icElemTypeSignature elemTypeSig);
};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCMPEXMLFACTORY_H
