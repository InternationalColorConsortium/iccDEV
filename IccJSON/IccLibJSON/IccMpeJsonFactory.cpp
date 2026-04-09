/** @file
    File:       IccMpeJsonFactory.cpp

    Contains:   Implementation of CIccMpeJsonFactory –
                creates JSON-aware multi-process element objects.

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

#include "IccMpeJson.h"
#include "IccMpeJsonFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccMultiProcessElement* CIccMpeJsonFactory::CreateElement(icElemTypeSignature elemTypeSig)
{
  switch (elemTypeSig) {
  case icSigMatrixElemType:             return new CIccMpeJsonMatrix();
  case icSigCurveSetElemType:           return new CIccMpeJsonCurveSet();
  case icSigCLutElemType:               return new CIccMpeJsonCLUT();
  case icSigExtCLutElemType:            return new CIccMpeJsonExtCLUT();
  case icSigCalculatorElemType:         return new CIccMpeJsonCalculator();
  case icSigTintArrayElemType:          return new CIccMpeJsonTintArray();
  case icSigToneMapElemType:            return new CIccMpeJsonToneMap();
  case icSigXYZToJabElemType:           return new CIccMpeJsonXYZToJab();
  case icSigJabToXYZElemType:           return new CIccMpeJsonJabToXYZ();
  case icSigEmissionMatrixElemType:     return new CIccMpeJsonEmissionMatrix();
  case icSigInvEmissionMatrixElemType:  return new CIccMpeJsonInvEmissionMatrix();
  case icSigEmissionCLUTElemType:       return new CIccMpeJsonEmissionCLUT();
  case icSigReflectanceCLUTElemType:    return new CIccMpeJsonReflectanceCLUT();
  case icSigEmissionObserverElemType:   return new CIccMpeJsonEmissionObserver();
  case icSigReflectanceObserverElemType: return new CIccMpeJsonReflectanceObserver();
  case icSigBAcsElemType:               return new CIccMpeJsonBAcs();
  case icSigEAcsElemType:               return new CIccMpeJsonEAcs();
  default:                              return new CIccMpeJsonUnknown();
  }
}

bool CIccMpeJsonFactory::GetElementSigName(std::string & /*elemName*/,
                                            icElemTypeSignature /*elemTypeSig*/)
{
  return false;
}

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif
