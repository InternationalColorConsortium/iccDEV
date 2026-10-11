/** @file
    File:       IccMpeJsonFactory.cpp

    Contains:   Implementation of CIccMpeJsonFactory --
                creates JSON-aware multi-process element objects.

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
  case icSigMatrixElemType:             return new (std::nothrow) CIccMpeJsonMatrix();
  case icSigCurveSetElemType:           return new (std::nothrow) CIccMpeJsonCurveSet();
  case icSigCLutElemType:               return new (std::nothrow) CIccMpeJsonCLUT();
  case icSigExtCLutElemType:            return new (std::nothrow) CIccMpeJsonExtCLUT();
  case icSigCalculatorElemType:         return new (std::nothrow) CIccMpeJsonCalculator();
  case icSigTintArrayElemType:          return new (std::nothrow) CIccMpeJsonTintArray();
  case icSigToneMapElemType:            return new (std::nothrow) CIccMpeJsonToneMap();
  case icSigXYZToJabElemType:           return new (std::nothrow) CIccMpeJsonXYZToJab();
  case icSigJabToXYZElemType:           return new (std::nothrow) CIccMpeJsonJabToXYZ();
  case icSigEmissionMatrixElemType:     return new (std::nothrow) CIccMpeJsonEmissionMatrix();
  case icSigInvEmissionMatrixElemType:  return new (std::nothrow) CIccMpeJsonInvEmissionMatrix();
  case icSigEmissionCLUTElemType:       return new (std::nothrow) CIccMpeJsonEmissionCLUT();
  case icSigReflectanceCLUTElemType:    return new (std::nothrow) CIccMpeJsonReflectanceCLUT();
  case icSigEmissionObserverElemType:   return new (std::nothrow) CIccMpeJsonEmissionObserver();
  case icSigReflectanceObserverElemType: return new (std::nothrow) CIccMpeJsonReflectanceObserver();
  case icSigBAcsElemType:               return new (std::nothrow) CIccMpeJsonBAcs();
  case icSigEAcsElemType:               return new (std::nothrow) CIccMpeJsonEAcs();
  default:                              return new (std::nothrow) CIccMpeJsonUnknown();
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
