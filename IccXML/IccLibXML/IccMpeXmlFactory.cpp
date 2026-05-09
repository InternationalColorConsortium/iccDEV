/** @file
    File:       IccMpeXmlFactory.cpp

    Contains:   Implementation of a CIccTagXmlFactory class and
                creation factories - An extension factory providing ICC
                XML format capabilities

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights 
 * reserved.
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

#include "IccMpeXml.h"
#include "IccMpeXmlFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include <new>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccMultiProcessElement* CIccMpeXmlFactory::CreateElement(icElemTypeSignature elemTypeSig)
{
  switch(elemTypeSig) {
    case icSigMatrixElemType:
      return new(std::nothrow) CIccMpeXmlMatrix();

    case icSigCurveSetElemType:
      return new(std::nothrow) CIccMpeXmlCurveSet();

    case icSigCLutElemType:
      return new(std::nothrow) CIccMpeXmlCLUT();

    case icSigExtCLutElemType:
      return new(std::nothrow) CIccMpeXmlExtCLUT();

    case icSigCalculatorElemType:
      return new(std::nothrow) CIccMpeXmlCalculator();

    case icSigTintArrayElemType:
      return new(std::nothrow) CIccMpeXmlTintArray();

    case icSigToneMapElemType:
      return new(std::nothrow) CIccMpeXmlToneMap();

    case icSigXYZToJabElemType:
      return new(std::nothrow) CIccMpeXmlXYZToJab();

    case icSigJabToXYZElemType:
      return new(std::nothrow) CIccMpeXmlJabToXYZ();

    case icSigEmissionMatrixElemType:
      return new(std::nothrow) CIccMpeXmlEmissionMatrix();

    case icSigInvEmissionMatrixElemType:
      return new(std::nothrow) CIccMpeXmlInvEmissionMatrix();

    case icSigEmissionCLUTElemType:
      return new(std::nothrow) CIccMpeXmlEmissionCLUT();

    case icSigReflectanceCLUTElemType:
      return new(std::nothrow) CIccMpeXmlReflectanceCLUT();

    case icSigEmissionObserverElemType:
      return new(std::nothrow) CIccMpeXmlEmissionObserver();

    case icSigReflectanceObserverElemType:
      return new(std::nothrow) CIccMpeXmlReflectanceObserver();

    case icSigBAcsElemType:
      return new(std::nothrow) CIccMpeXmlBAcs();

    case icSigEAcsElemType:
      return new(std::nothrow) CIccMpeXmlEAcs();

    default:
      return new(std::nothrow) CIccMpeXmlUnknown();
  }
}

bool CIccMpeXmlFactory::GetElementSigName(std::string & /*elemName*/, icElemTypeSignature /*elemTypeSig*/)
{
  return false;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
