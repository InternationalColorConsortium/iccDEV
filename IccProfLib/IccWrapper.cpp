/** @file
File:       IccWrapper.h

Contains:   Header file for C Wrapper interface functions for CIccProfile
and CIccCmm classes.

Version:    V1

Copyright:  (c) see ICC Software License
*/

/*
* The ICC Software License, Version 0.2
*
*
* Copyright (c) 2003-2016 The International Color Consortium. All rights
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

#include "IccWrapper.h"
#include "IccApplyBPC.h"

class CIccCmmEnvVarsWrapper : public IIccCmmEnvVarLookup
{
public:
  CIccCmmEnvVarsWrapper(SIccCmmEnvVars *pVars) { m_pVars = pVars; }

  virtual bool GetEnvVar(icSigCmmEnvVar sig, icFloatNumber &val);
  virtual bool IndexedEnvVar(icUInt32Number nIndex, icSigCmmEnvVar &sig, icFloatNumber &val);
protected:
  SIccCmmEnvVars *m_pVars;
};

bool CIccCmmEnvVarsWrapper::GetEnvVar(icSigCmmEnvVar sig, icFloatNumber &val)
{
  if (!m_pVars || !m_pVars->sigs || !m_pVars->vals) {
    val = 0;
    return false;
  }

  icUInt32Number i;
  for (i = 0; i < m_pVars->nVars; i++) {
    if (sig == m_pVars->sigs[i]) {
      val = m_pVars->vals[i];
      return true;
    }
  }
  val = 0;
  return false;
}

bool CIccCmmEnvVarsWrapper::IndexedEnvVar(icUInt32Number nIndex, icSigCmmEnvVar &sig, icFloatNumber &val)
{
  if (!m_pVars || !m_pVars->sigs || !m_pVars->vals) {
    sig = (icSigCmmEnvVar)0;
    val = 0;
    return false;
  }

  if (nIndex < m_pVars->nVars) {
    sig = m_pVars->sigs[nIndex];
    val = m_pVars->vals[nIndex];
    return true;
  }

  sig = (icSigCmmEnvVar)0;
  val = 0;
  return false;
}

class CIccCmmEnvVarWrapperHint : public CIccCreateCmmEnvVarXformHint
{
public:
  CIccCmmEnvVarWrapperHint(SIccCmmEnvVars *pVars) { m_pVars = pVars; }

  virtual IIccCmmEnvVarLookup *GetNewCmmEnvVarLookup() const { return new CIccCmmEnvVarsWrapper(m_pVars); }
protected:
  SIccCmmEnvVars *m_pVars;
};

CIccCmmHandle* IccCmmCreate(icColorSpaceSignature srcSpace, icColorSpaceSignature dstSpace, unsigned char bFirstIsInput)
{
  return (CIccCmmHandle*)new CIccCmm(srcSpace, dstSpace, bFirstIsInput != 0);
}

icStatusCMM ICCPROFLIB_API IccCmmAttachProfileFile(CIccCmmHandle *pCmm,
  const char *szFname,
  icRenderingIntent nIntent,
  icXformInterp nInterp,
  CIccProfileHandle *pPcc, //may be NULL (ownership not taken)
  icXformLutType nLutType,
  unsigned char bUseDtoBTags,
  unsigned char bUseBPC,
  SIccCmmEnvVars *pVars) //may be NULL (ownership not taken)
{
  if (!pCmm)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;

  CIccCreateXformHintManager hint;
  bool bUseHint = false;

  if (bUseBPC) {
    hint.AddHint(new CIccApplyBPCHint());
    bUseHint = true;
  }

  if (pVars) {
    hint.AddHint(new CIccCmmEnvVarWrapperHint(pVars));
    bUseHint = true;
  }

  return pCmmPtr->AddXform(szFname, nIntent, nInterp, (CIccProfile*)pPcc, nLutType, bUseDtoBTags != 0, bUseHint ? &hint : NULL);
}

icStatusCMM IccCmmAttachProfileHandle(CIccCmmHandle *pCmm,
  CIccProfileHandle *pProfile, // CMM assumes ownership of profile
  icRenderingIntent nIntent,
  icXformInterp nInterp,
  CIccProfileHandle *pPcc,
  icXformLutType nLutType,
  unsigned char bUseDtoBTags,
  unsigned char bUseBPC,
  SIccCmmEnvVars *pVars)
{
  if (!pCmm)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;
  CIccProfile *pProfilePtr = (CIccProfile*)pProfile;

  CIccCreateXformHintManager hint;
  bool bUseHint = false;

  if (bUseBPC) {
    hint.AddHint(new CIccApplyBPCHint());
    bUseHint = true;
  }

  if (pVars) {
    hint.AddHint(new CIccCmmEnvVarWrapperHint(pVars));
    bUseHint = true;
  }

  return pCmmPtr->AddXform(pProfilePtr, nIntent, nInterp, (CIccProfile*)pPcc, nLutType, bUseDtoBTags != 0, bUseHint ? &hint : NULL);
}

icStatusCMM IccCmmBegin(CIccCmmHandle *pCmm)
{
  if (!pCmm)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;

  return pCmmPtr->Begin();
}

CIccApplyHandle* IccCmmGetApply(CIccCmmHandle *pCmm)
{
  if (!pCmm)
    return NULL;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;
  icStatusCMM status;

  // GetNewApplyCmm allocates a new apply object the caller must delete.
  // GetApply() returns the CMM's internal m_pApply - deleting it corrupts the CMM.
  return (CIccApplyHandle*)pCmmPtr->GetNewApplyCmm(status);
}

icStatusCMM IccCmmGetInfo(CIccCmmHandle *pCmm, SIccCmmStruct *pCmmInfo)
{
  if (!pCmm || !pCmmInfo)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;

  pCmmInfo->srcSpace = pCmmPtr->GetSourceSpace();
  pCmmInfo->srcSamples = pCmmPtr->GetSourceSamples();
  pCmmInfo->dstSpace = pCmmPtr->GetDestSpace();
  pCmmInfo->dstSamples = pCmmPtr->GetDestSamples();

  return icCmmStatOk;
}

icStatusCMM IccCmmApplyFloat(CIccCmmHandle *pCmm, icFloatNumber *pTo, icFloatNumber *pFrom)
{
  if (!pCmm || !pTo || !pFrom)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;

  return pCmmPtr->Apply(pTo, pFrom);
}

icStatusCMM IccCmmApplyFloatMulti(CIccCmmHandle *pCmm, icFloatNumber *pTo, icFloatNumber *pFrom, icUInt32Number nPixels)
{
  if (!pCmm || !pTo || !pFrom)
    return icCmmStatBad;

  CIccCmm *pCmmPtr = (CIccCmm*)pCmm;

  return pCmmPtr->Apply(pTo, pFrom, nPixels);
}

void IccCmmFree(CIccCmmHandle *pCmm)
{
  if (pCmm) {
    CIccCmm *pCmmPtr = (CIccCmm*)pCmm;
    delete pCmmPtr;
  }
}

icStatusCMM IccApplyApplyFloat(CIccApplyHandle *pApply, icFloatNumber *pTo, icFloatNumber *pFrom)
{
  if (!pApply || !pTo || !pFrom)
    return icCmmStatBad;

  CIccApplyCmm *pApplyPtr = (CIccApplyCmm*)pApply;

  return pApplyPtr->Apply(pTo, pFrom);
}

icStatusCMM IccApplyApplyFloatMulti(CIccApplyHandle *pApply, icFloatNumber *pTo, icFloatNumber *pFrom, icUInt32Number nPixels)
{
  if (!pApply || !pTo || !pFrom)
    return icCmmStatBad;

  CIccApplyCmm *pApplyPtr = (CIccApplyCmm*)pApply;

  return pApplyPtr->Apply(pTo, pFrom, nPixels);
}

void IccApplyFree(CIccApplyHandle *pApply)
{
  if (pApply) {
    CIccApplyCmm *pApplyPtr = (CIccApplyCmm*)pApply;
    delete pApplyPtr;
  }
}

CIccProfileHandle* IccProfileReadHandle(const char *szFname)
{
  return (CIccProfileHandle*)ReadIccProfile(szFname);
}

CIccProfileHandle* IccProfileOpenHandle(const char *szFname)
{
  return (CIccProfileHandle*)OpenIccProfile(szFname);
}
icBoolean IccProfileGetHeader(CIccProfileHandle *pProfile, icHeader *pHeader)
{
  if (!pProfile || !pHeader)
    return 0;

  CIccProfile *pProfilePtr = (CIccProfile*)pProfile;

  *pHeader = pProfilePtr->m_Header;

  return 1;
}

void IccProfileFree(CIccProfileHandle *pProfile)
{
  if (pProfile) {
    CIccProfile *pProfilePtr = (CIccProfile*)pProfile;
    delete pProfilePtr;
  }
}

