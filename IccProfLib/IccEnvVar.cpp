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
#include "IccEnvVar.h"

CIccCmmVarLookup::CIccCmmVarLookup(icCmmEnvSigMap &map)
{
  m_map = new icCmmEnvSigMap();
  *m_map = map;
}

CIccCmmVarLookup::~CIccCmmVarLookup()
{
  delete m_map;
}

bool CIccCmmVarLookup::GetEnvVar(icSigCmmEnvVar sig, icFloatNumber &val)
{
  icCmmEnvSigMap::iterator v=m_map->find((icSignature)sig);

  if (v==m_map->end()) {
    val = (icFloatNumber)0.0;
    return false;
  }
  val = v->second;
  return true;
}

bool CIccCmmVarLookup::IndexedEnvVar(icUInt32Number nIndex, icSigCmmEnvVar &sig, icFloatNumber &val)
{
  if (nIndex>=m_map->size()) {
    sig = (icSigCmmEnvVar)0;
    val = (icFloatNumber)0.0;
    return false;
  }
  icUInt32Number i;
  icCmmEnvSigMap::iterator v;

  for (v=m_map->begin(), i=0; i<nIndex; v++, i++);

  sig = (icSigCmmEnvVar)(v->first);
  val = v->second;
  return true;
}

CIccCmmEnvVarHint::CIccCmmEnvVarHint(icCmmEnvSigMap &map)
{
  m_map = new icCmmEnvSigMap();
  *m_map = map;
}

CIccCmmEnvVarHint::~CIccCmmEnvVarHint()
{
  delete m_map;
}

CIccCmmPccEnvVarHint::CIccCmmPccEnvVarHint(icCmmEnvSigMap& map)
{
  m_map = new icCmmEnvSigMap();
  *m_map = map;
}

CIccCmmPccEnvVarHint::~CIccCmmPccEnvVarHint()
{
  delete m_map;
}
