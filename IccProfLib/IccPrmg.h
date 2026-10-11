/** @file
File:       IccPrmg.h

Contains:   Header for implementation of CIccPrmg class

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
// -Oct 27, 2007 
// Initial implementation of class CIccPRMG
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCPRMG_H
#define _ICCPRMG_H

#include "IccCmm.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccPRMG
{
public:
  CIccPRMG();

  icFloatNumber GetChroma(icFloatNumber L, icFloatNumber h);

  bool InGamut(icFloatNumber *Lab);
  bool InGamut(icFloatNumber L, icFloatNumber c, icFloatNumber h);

  icStatusCMM EvaluateProfile(CIccProfile *pProfile, icRenderingIntent nIntent=icUnknownIntent, 
                              icXformInterp nInterp=icInterpLinear, bool buseMpeTags=true);
  icStatusCMM EvaluateProfile(const icChar *szProfilePath, icRenderingIntent nIntent=icUnknownIntent, 
                              icXformInterp nInterp=icInterpLinear, bool buseMpeTags=true);

  icUInt32Number m_nDE1, m_nDE2, m_nDE3, m_nDE5, m_nDE10, m_nTotal;

  bool m_bPrmgImplied;
};

#ifdef USEICCDEVNAMESPACE
}
#endif

#endif