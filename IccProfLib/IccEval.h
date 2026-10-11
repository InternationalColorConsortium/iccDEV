/** @file
File:       IccEval.h

Contains:   Header file for implementation of the CIccProfile Evaluation utilites.

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
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 9-25-2007
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCEVAL_H)
#define _ICCEVAL_H

#include "IccCmm.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


class CIccEvalCompare {
public:
  virtual ~CIccEvalCompare() {}
  //Create prototype for Compare function that must be implemented by a derived class
  virtual void Compare(icFloatNumber *pPixel, icFloatNumber *deviceLab, icFloatNumber *destLab1, icFloatNumber *destLab2)=0;

  icStatusCMM ICCPROFLIB_API EvaluateProfile(CIccProfile *pProfile, icUInt8Number nGran=0, 
                                             icRenderingIntent nIntent=icUnknownIntent, icXformInterp nInterp=icInterpLinear,
                                             bool buseMpeTags=true);

  icStatusCMM ICCPROFLIB_API EvaluateProfile(const icChar *szProfilePath, icUInt8Number nGran=0, 
                                             icRenderingIntent nIntent=icUnknownIntent, icXformInterp nInterp=icInterpLinear,
                                             bool buseMpeTags=true);
};

#ifdef USEICCDEVNAMESPACE
}; //namespace iccDEV
#endif

#endif // !defined(_ICCEVAL_H)
