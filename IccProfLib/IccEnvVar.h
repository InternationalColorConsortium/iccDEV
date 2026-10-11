/** @file
    File:       IccEnvVar.h

    Contains:   Header for implementation of the IIccCmmEnvVarLookup interface class.

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
// -Initial implementation by Max Derhak 3-1-2016
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCENVVAR_H)
#define _ICCENVVAR_H

#include "IccDefs.h"
#include "IccCmm.h"
#include <map>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif


typedef std::map<icSignature, icFloatNumber> icCmmEnvSigMap;

class ICCPROFLIB_API CIccCmmVarLookup : public IIccCmmEnvVarLookup
{
public:
  CIccCmmVarLookup(icCmmEnvSigMap &map);
  virtual ~CIccCmmVarLookup();

  virtual bool GetEnvVar(icSigCmmEnvVar sig, icFloatNumber &val);
  virtual bool IndexedEnvVar(icUInt32Number nIndex, icSigCmmEnvVar &sig, icFloatNumber &val);
protected:
  icCmmEnvSigMap *m_map;
};

class ICCPROFLIB_API CIccCmmEnvVarHint : public CIccCreateCmmEnvVarXformHint
{
public:
  CIccCmmEnvVarHint(icCmmEnvSigMap &map);
  virtual ~CIccCmmEnvVarHint();

  virtual IIccCmmEnvVarLookup *GetNewCmmEnvVarLookup() const { return new CIccCmmVarLookup(*m_map);}

protected:
  icCmmEnvSigMap *m_map;
};

class ICCPROFLIB_API CIccCmmPccEnvVarHint : public CIccCreateCmmPccEnvVarXformHint
{
public:
  CIccCmmPccEnvVarHint(icCmmEnvSigMap& map);
  virtual ~CIccCmmPccEnvVarHint();

  virtual IIccCmmEnvVarLookup* GetNewCmmEnvVarLookup() const { return new CIccCmmVarLookup(*m_map); }

protected:
  icCmmEnvSigMap* m_map;
};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCENVVAR_H)
