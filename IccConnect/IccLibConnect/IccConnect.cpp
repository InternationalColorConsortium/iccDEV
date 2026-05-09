/*
    File:       IccConnect.cpp

    Contains:   Factory class for constructing initialized ICC CMM transforms
                from JSON-based configuration objects.

    Version:    V1

    Copyright:  (c) see below
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
 // -Initial implementation by Max Derhak 4-2026
 //
 //////////////////////////////////////////////////////////////////////

#include "IccConnect.h"
#include "IccApplyBPC.h"
#include "IccCmmThread.h"
#include "IccEnvVar.h"
#include <memory>
#include <new>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccConnectCmm::CIccConnectCmm()
  : m_pCmm(nullptr)
{
}

CIccConnectCmm::~CIccConnectCmm()
{
  delete m_pCmm;
}

CIccConnectCmm* CIccConnectCmm::Attach(CIccCmm* pCmm)
{
  auto* pConn = new (std::nothrow) CIccConnectCmm();
  if (!pConn) {
    delete pCmm;
    return nullptr;
  }
  pConn->m_pCmm = pCmm;
  return pConn;
}

CIccNamedColorCmm* CIccConnectCmm::GetNamedCmm() const
{
  return dynamic_cast<CIccNamedColorCmm*>(m_pCmm);
}

CIccCmmSearch* CIccConnectCmm::GetSearchCmm() const
{
  return dynamic_cast<CIccCmmSearch*>(m_pCmm);
}

CIccThreadedCmm* CIccConnectCmm::GetThreadedCmm() const
{
  return dynamic_cast<CIccThreadedCmm*>(m_pCmm);
}

icColorSpaceSignature CIccConnectCmm::GetSourceSpace() const
{
  return m_pCmm ? m_pCmm->GetSourceSpace() : icSigUnknownData;
}

icColorSpaceSignature CIccConnectCmm::GetDestSpace() const
{
  return m_pCmm ? m_pCmm->GetDestSpace() : icSigUnknownData;
}

void CIccConnectCmm::ReleasePccList(IccProfilePtrList& pccList)
{
  for (auto* p : pccList)
    delete p;
  pccList.clear();
}

static icStatusCMM AddHintNoThrow(CIccCreateXformHintManager& Hint,
                                  IIccCreateXformHint* pHint)
{
  if (!pHint)
    return icCmmStatAllocErr;

  return Hint.AddHint(pHint) ? icCmmStatOk : icCmmStatBad;
}

static CIccConnectCmm* AttachStandardCmm(std::unique_ptr<CIccCmm>& pCmm,
                                         int nThreads)
{
  if (nThreads == 0 || nThreads > 1) {
    CIccCmm* pRawCmm = pCmm.release();
    CIccThreadedCmm* pThreadedCmm = CIccThreadedCmm::Attach(pRawCmm, nThreads);
    if (!pThreadedCmm)
      return nullptr;
    return CIccConnectCmm::Attach(pThreadedCmm);
  }

  return CIccConnectCmm::Attach(pCmm.release());
}

icStatusCMM CIccConnectCmm::AddXformFromConfig(CIccCmm* pCmm,
                                                CIccCfgProfile* pCfg,
                                                IccProfilePtrList& pccList)
{
  CIccProfile* pPcc = nullptr;
  CIccCreateXformHintManager Hint;

  if (pCfg->m_useBPC) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccApplyBPCHint());
    if (stat != icCmmStatOk)
      return stat;
  }

  if (pCfg->m_adjustPcsLuminance) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccLuminanceMatchingHint());
    if (stat != icCmmStatOk)
      return stat;
  }

  if (!pCfg->m_pccFile.empty()) {
    // Caller-selected config paths intentionally name ICC/PCC profile files.
    // codeql[cpp/path-injection]
    pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
    if (!pPcc)
      return icCmmStatBad;
    pccList.push_back(pPcc);
  }

  if (!pCfg->m_iccEnvVars.empty()) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
    if (stat != icCmmStatOk)
      return stat;
  }

  if (!pCfg->m_pccEnvVars.empty()) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
    if (stat != icCmmStatOk)
      return stat;
  }

  return pCmm->AddXform(
    pCfg->m_iccFile.c_str(),
    pCfg->m_intent < 0 ? icUnknownIntent : (icRenderingIntent)pCfg->m_intent,
    pCfg->m_interpolation,
    pPcc,
    pCfg->m_transform,
    pCfg->m_useD2BxB2Dx,
    &Hint,
    pCfg->m_useV5SubProfile
  );
}

CIccConnectCmm* CIccConnectCmm::CreateNamed(const CIccCfgProfileSequence& profiles,
                                              icColorSpaceSignature srcSpace,
                                              bool bInputProfile)
{
  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccNamedColorCmm>(
    new (std::nothrow) CIccNamedColorCmm(srcSpace, icSigUnknownData, bInputProfile));
  if (!pCmm)
    return nullptr;

  for (const auto& profPtr : profiles.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg)
      continue;

    icStatusCMM stat = AddXformFromConfig(pCmm.get(), pCfg, pccList);
    if (stat != icCmmStatOk) {
      ReleasePccList(pccList);
      return nullptr;
    }
  }

  if (pCmm->Begin() != icCmmStatOk) {
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return Attach(pCmm.release());
}

CIccConnectCmm* CIccConnectCmm::CreateStandard(const CIccCfgProfileSequence& profiles,
                                                 const unsigned char* pEmbeddedData,
                                                 unsigned int nEmbeddedLen,
                                                 int nThreads)
{
  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccCmm>(
    new (std::nothrow) CIccCmm(icSigUnknownData, icSigUnknownData, true));
  if (!pCmm)
    return nullptr;

  bool bFirst = true;
  for (const auto& profPtr : profiles.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg) {
      ReleasePccList(pccList);
      return nullptr;
    }

    icStatusCMM stat;

    if (bFirst && pCfg->m_iccFile.empty() && pEmbeddedData && nEmbeddedLen) {
      CIccProfile* pPcc = nullptr;
      CIccCreateXformHintManager Hint;

      if (pCfg->m_useBPC) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccApplyBPCHint());
        if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
      }
      if (pCfg->m_adjustPcsLuminance) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccLuminanceMatchingHint());
        if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
      }
      if (!pCfg->m_pccFile.empty()) {
        // Caller-selected config paths intentionally name ICC/PCC profile files.
        // codeql[cpp/path-injection]
        pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
        if (!pPcc) { ReleasePccList(pccList); return nullptr; }
        pccList.push_back(pPcc);
      }
      if (!pCfg->m_iccEnvVars.empty()) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
        if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
      }
      if (!pCfg->m_pccEnvVars.empty()) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
        if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
      }

      stat = pCmm->AddXform(
        const_cast<unsigned char*>(pEmbeddedData),
        (icUInt32Number)nEmbeddedLen,
        pCfg->m_intent < 0 ? icUnknownIntent : (icRenderingIntent)pCfg->m_intent,
        pCfg->m_interpolation,
        pPcc,
        pCfg->m_transform,
        pCfg->m_useD2BxB2Dx,
        &Hint,
        pCfg->m_useV5SubProfile
      );
    }
    else {
      stat = AddXformFromConfig(pCmm.get(), pCfg, pccList);
    }

    if (stat != icCmmStatOk) {
      ReleasePccList(pccList);
      return nullptr;
    }
    bFirst = false;
  }

  if (pCmm->Begin() != icCmmStatOk) {
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return AttachStandardCmm(pCmm, nThreads);
}

CIccConnectCmm* CIccConnectCmm::CreateSearch(const CIccCfgSearchApply& searchApply)
{
  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccCmmSearch>(new (std::nothrow) CIccCmmSearch());
  if (!pCmm)
    return nullptr;

  // Add forward profile chain.  Explicitly scope to CIccCmm::AddXform to bypass
  // CIccCmmSearch's override which is reserved for the reverse/destination profile.
  for (const auto& profPtr : searchApply.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg)
      continue;

    CIccProfile* pPcc = nullptr;
    CIccCreateXformHintManager Hint;

    if (!pCfg->m_pccFile.empty()) {
      // Caller-selected config paths intentionally name ICC/PCC profile files.
      // codeql[cpp/path-injection]
      pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
      if (!pPcc) { ReleasePccList(pccList); return nullptr; }
      pccList.push_back(pPcc);
    }

    if (!pCfg->m_iccEnvVars.empty()) {
      icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
      if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
    }
    if (!pCfg->m_pccEnvVars.empty()) {
      icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
      if (stat != icCmmStatOk) { ReleasePccList(pccList); return nullptr; }
    }

    icStatusCMM stat = pCmm->CIccCmm::AddXform(
      pCfg->m_iccFile.c_str(),
      pCfg->m_intent < 0 ? icUnknownIntent : (icRenderingIntent)pCfg->m_intent,
      pCfg->m_interpolation,
      pPcc,
      pCfg->m_transform,
      pCfg->m_useD2BxB2Dx,
      &Hint,
      pCfg->m_useV5SubProfile
    );
    if (stat != icCmmStatOk) {
      ReleasePccList(pccList);
      return nullptr;
    }
  }

  // Attach weighted PCC profiles for spectral search computation.
  for (const auto& pccPtr : searchApply.m_pccWeights) {
    CIccCfgPccWeight* pPccWeight = pccPtr.get();
    if (!pPccWeight)
      continue;

    // Caller-selected config paths intentionally name ICC/PCC profile files.
    // codeql[cpp/path-injection]
    CIccProfile* pPcc = OpenIccProfile(pPccWeight->m_pccPath.c_str());
    if (!pPcc || !pPcc->ReadPccTags()) {
      ReleasePccList(pccList);
      return nullptr;
    }
    pPcc->Detach();

    if (pCmm->AttachPCC(pPcc, pPccWeight->m_dWeight) != icCmmStatOk) {
      ReleasePccList(pccList);
      return nullptr;
    }
  }

  // Set the destination initial profile for the reverse search.
  if (searchApply.isInitialized()) {
    CIccCfgProfile* pLastCfg = searchApply.m_profiles.rbegin()->get();
    // Caller-selected config paths intentionally name ICC/PCC profile files.
    // codeql[cpp/path-injection]
    CIccProfile* pProfile = OpenIccProfile(pLastCfg->m_iccFile.c_str(),
                                            searchApply.m_useV5SubProfileInitial);
    CIccProfile* pPcc = nullptr;
    if (!pProfile) {
      ReleasePccList(pccList);
      return nullptr;
    }

    if (!pLastCfg->m_pccFile.empty()) {
      // Caller-selected config paths intentionally name ICC/PCC profile files.
      // codeql[cpp/path-injection]
      pPcc = OpenIccProfile(pLastCfg->m_pccFile.c_str());
      if (!pPcc) {
        delete pProfile;
        ReleasePccList(pccList);
        return nullptr;
      }
      pccList.push_back(pPcc);
    }

    pCmm->SetDstInitProfile(
      pProfile,
      searchApply.m_intentInitial,
      pLastCfg->m_interpolation,
      pPcc,
      searchApply.m_transformInitial,
      searchApply.m_useD2BxB2DxInitial
    );
  }

  if (pCmm->Begin() != icCmmStatOk) {
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return Attach(pCmm.release());
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
