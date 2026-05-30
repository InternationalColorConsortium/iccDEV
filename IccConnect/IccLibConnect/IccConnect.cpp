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
#include <sstream>

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
                                                IccProfilePtrList& pccList,
                                                std::string& sErrorMsg)
{
  CIccProfile* pPcc = nullptr;
  CIccCreateXformHintManager Hint;

  if (pCfg->m_useBPC) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccApplyBPCHint());
    if (stat != icCmmStatOk) {
      sErrorMsg = "failed to attach BPC hint for '" + pCfg->m_iccFile + "'";
      return stat;
    }
  }

  if (pCfg->m_adjustPcsLuminance) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccLuminanceMatchingHint());
    if (stat != icCmmStatOk) {
      sErrorMsg = "failed to attach PCS-luminance hint for '" + pCfg->m_iccFile + "'";
      return stat;
    }
  }

  if (!pCfg->m_pccFile.empty()) {
    // Caller-selected config paths intentionally name ICC/PCC profile files.
    // codeql[cpp/path-injection]
    pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
    if (!pPcc) {
      sErrorMsg = "unable to open PCC profile '" + pCfg->m_pccFile + "'";
      return icCmmStatBad;
    }
    pccList.push_back(pPcc);
  }

  if (!pCfg->m_iccEnvVars.empty()) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
    if (stat != icCmmStatOk) {
      sErrorMsg = "failed to attach ICC env-var hint for '" + pCfg->m_iccFile + "'";
      return stat;
    }
  }

  if (!pCfg->m_pccEnvVars.empty()) {
    icStatusCMM stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
    if (stat != icCmmStatOk) {
      sErrorMsg = "failed to attach PCC env-var hint for '" + pCfg->m_iccFile + "'";
      return stat;
    }
  }

  // Attach a CIccCreateNamedColorXformHint carrying m_nOverprint whenever
  // the caller asked for anything other than the OverWhite default, or
  // whenever the JSON explicitly selected one of the named-color transform
  // variants (so the named-color factory path in CIccXform::Create gets a
  // pre-built hint regardless of profile device class).  Covers:
  //  - JSON "named*OnBlack"/"named*OnGray" (m_nOverprint != OverWhite)
  //  - JSON "named", "namedColorimetric", "namedSpectral" (the m_transform
  //    check below; m_nOverprint is OverWhite)
  //  - the legacy CLI +1000000/+2000000 high-bit (leaves m_transform at
  //    icXformLutColor and relies on the CMM's deviceClass-based
  //    auto-dispatch into the named-color branch).
  // When the profile turns out not to be a NamedColor xform the hint is
  // simply unused.
  if (pCfg->m_nOverprint != icNamedColorOverWhite ||
      pCfg->m_transform == icXformLutNamedColor ||
      pCfg->m_transform == icXformLutNamedColorimetric ||
      pCfg->m_transform == icXformLutNamedSpectral ||
      pCfg->m_transform == icXformLutNamedDevice) {
    CIccCreateNamedColorXformHint* pNCHint =
        new (std::nothrow) CIccCreateNamedColorXformHint();
    if (!pNCHint) {
      sErrorMsg = "failed to allocate NamedColor hint for '" + pCfg->m_iccFile + "'";
      return icCmmStatAllocErr;
    }
    pNCHint->nOverprintType = pCfg->m_nOverprint;
    icStatusCMM stat = AddHintNoThrow(Hint, pNCHint);
    if (stat != icCmmStatOk) {
      sErrorMsg = "failed to attach NamedColor hint for '" + pCfg->m_iccFile + "'";
      return stat;
    }
  }

  icStatusCMM stat = pCmm->AddXform(
    pCfg->m_iccFile.c_str(),
    pCfg->m_intent < 0 ? icUnknownIntent : (icRenderingIntent)pCfg->m_intent,
    pCfg->m_interpolation,
    pPcc,
    pCfg->m_transform,
    pCfg->m_useD2BxB2Dx,
    &Hint,
    pCfg->m_useV5SubProfile
  );
  if (stat == icCmmStatCantOpenProfile) {
    sErrorMsg = "unable to open ICC profile '" + pCfg->m_iccFile + "'";
  }
  else if (stat != icCmmStatOk) {
    std::ostringstream oss;
    oss << "AddXform failed for '" << pCfg->m_iccFile << "' (status " << (int)stat << ")";
    sErrorMsg = oss.str();
  }
  return stat;
}

CIccConnectCmm* CIccConnectCmm::CreateNamed(const CIccCfgProfileSequence& profiles,
                                              icColorSpaceSignature srcSpace,
                                              bool bInputProfile,
                                              std::string* pErrorMsg)
{
  std::string localErr;
  std::string& sErrorMsg = pErrorMsg ? *pErrorMsg : localErr;

  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccNamedColorCmm>(
    new (std::nothrow) CIccNamedColorCmm(srcSpace, icSigUnknownData, bInputProfile));
  if (!pCmm) {
    sErrorMsg = "failed to allocate named-color CMM";
    return nullptr;
  }

  size_t stageIdx = 0;
  for (const auto& profPtr : profiles.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg) {
      ++stageIdx;
      continue;
    }

    std::string sStageErr;
    icStatusCMM stat = AddXformFromConfig(pCmm.get(), pCfg, pccList, sStageErr);
    if (stat != icCmmStatOk) {
      std::ostringstream oss;
      oss << "stage " << stageIdx << ": " << sStageErr;
      sErrorMsg = oss.str();
      ReleasePccList(pccList);
      return nullptr;
    }
    ++stageIdx;
  }

  icStatusCMM beginStat = pCmm->Begin();
  if (beginStat != icCmmStatOk) {
    std::ostringstream oss;
    oss << "Begin() failed (status " << (int)beginStat << "); profile chain is incompatible";
    sErrorMsg = oss.str();
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return Attach(pCmm.release());
}

CIccConnectCmm* CIccConnectCmm::CreateStandard(const CIccCfgProfileSequence& profiles,
                                                 const unsigned char* pEmbeddedData,
                                                 unsigned int nEmbeddedLen,
                                                 int nThreads,
                                                 std::string* pErrorMsg)
{
  std::string localErr;
  std::string& sErrorMsg = pErrorMsg ? *pErrorMsg : localErr;

  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccCmm>(
    new (std::nothrow) CIccCmm(icSigUnknownData, icSigUnknownData, true));
  if (!pCmm) {
    sErrorMsg = "failed to allocate CMM";
    return nullptr;
  }

  bool bFirst = true;
  size_t stageIdx = 0;
  for (const auto& profPtr : profiles.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg) {
      std::ostringstream oss;
      oss << "stage " << stageIdx << ": null profile config entry";
      sErrorMsg = oss.str();
      ReleasePccList(pccList);
      return nullptr;
    }

    icStatusCMM stat;
    std::string sStageErr;

    if (bFirst && pCfg->m_iccFile.empty() && pEmbeddedData && nEmbeddedLen) {
      CIccProfile* pPcc = nullptr;
      CIccCreateXformHintManager Hint;

      if (pCfg->m_useBPC) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccApplyBPCHint());
        if (stat != icCmmStatOk) {
          sStageErr = "failed to attach BPC hint for embedded source profile";
          goto stage_failed;
        }
      }
      if (pCfg->m_adjustPcsLuminance) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccLuminanceMatchingHint());
        if (stat != icCmmStatOk) {
          sStageErr = "failed to attach PCS-luminance hint for embedded source profile";
          goto stage_failed;
        }
      }
      if (!pCfg->m_pccFile.empty()) {
        // Caller-selected config paths intentionally name ICC/PCC profile files.
        // codeql[cpp/path-injection]
        pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
        if (!pPcc) {
          sStageErr = "unable to open PCC profile '" + pCfg->m_pccFile + "' for embedded source profile";
          stat = icCmmStatBad;
          goto stage_failed;
        }
        pccList.push_back(pPcc);
      }
      if (!pCfg->m_iccEnvVars.empty()) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
        if (stat != icCmmStatOk) {
          sStageErr = "failed to attach ICC env-var hint for embedded source profile";
          goto stage_failed;
        }
      }
      if (!pCfg->m_pccEnvVars.empty()) {
        stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
        if (stat != icCmmStatOk) {
          sStageErr = "failed to attach PCC env-var hint for embedded source profile";
          goto stage_failed;
        }
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
      if (stat != icCmmStatOk) {
        std::ostringstream oss;
        oss << "AddXform failed for embedded source profile (status " << (int)stat << ")";
        sStageErr = oss.str();
      }
    }
    else {
      stat = AddXformFromConfig(pCmm.get(), pCfg, pccList, sStageErr);
    }

  stage_failed:
    if (stat != icCmmStatOk) {
      std::ostringstream oss;
      oss << "stage " << stageIdx << ": " << sStageErr;
      sErrorMsg = oss.str();
      ReleasePccList(pccList);
      return nullptr;
    }
    bFirst = false;
    ++stageIdx;
  }

  icStatusCMM beginStat = pCmm->Begin();
  if (beginStat != icCmmStatOk) {
    std::ostringstream oss;
    oss << "Begin() failed (status " << (int)beginStat
        << "); profile chain is incompatible (srcSpace=0x"
        << std::hex << (unsigned)pCmm->GetSourceSpace()
        << " dstSpace=0x" << (unsigned)pCmm->GetDestSpace() << ")";
    sErrorMsg = oss.str();
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return AttachStandardCmm(pCmm, nThreads);
}

CIccConnectCmm* CIccConnectCmm::CreateSearch(const CIccCfgSearchApply& searchApply,
                                              std::string* pErrorMsg)
{
  std::string localErr;
  std::string& sErrorMsg = pErrorMsg ? *pErrorMsg : localErr;

  IccProfilePtrList pccList;
  auto pCmm = std::unique_ptr<CIccCmmSearch>(new (std::nothrow) CIccCmmSearch());
  if (!pCmm) {
    sErrorMsg = "failed to allocate search CMM";
    return nullptr;
  }

  // Add forward profile chain.  Explicitly scope to CIccCmm::AddXform to bypass
  // CIccCmmSearch's override which is reserved for the reverse/destination profile.
  size_t stageIdx = 0;
  for (const auto& profPtr : searchApply.m_profiles) {
    CIccCfgProfile* pCfg = profPtr.get();
    if (!pCfg) {
      ++stageIdx;
      continue;
    }

    CIccProfile* pPcc = nullptr;
    CIccCreateXformHintManager Hint;
    icStatusCMM stat = icCmmStatOk;
    std::string sStageErr;

    if (!pCfg->m_pccFile.empty()) {
      // Caller-selected config paths intentionally name ICC/PCC profile files.
      // codeql[cpp/path-injection]
      pPcc = OpenIccProfile(pCfg->m_pccFile.c_str());
      if (!pPcc) {
        sStageErr = "unable to open PCC profile '" + pCfg->m_pccFile + "'";
        stat = icCmmStatBad;
        goto search_stage_failed;
      }
      pccList.push_back(pPcc);
    }

    if (!pCfg->m_iccEnvVars.empty()) {
      stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmEnvVarHint(pCfg->m_iccEnvVars));
      if (stat != icCmmStatOk) {
        sStageErr = "failed to attach ICC env-var hint for '" + pCfg->m_iccFile + "'";
        goto search_stage_failed;
      }
    }
    if (!pCfg->m_pccEnvVars.empty()) {
      stat = AddHintNoThrow(Hint, new (std::nothrow) CIccCmmPccEnvVarHint(pCfg->m_pccEnvVars));
      if (stat != icCmmStatOk) {
        sStageErr = "failed to attach PCC env-var hint for '" + pCfg->m_iccFile + "'";
        goto search_stage_failed;
      }
    }

    stat = pCmm->CIccCmm::AddXform(
      pCfg->m_iccFile.c_str(),
      pCfg->m_intent < 0 ? icUnknownIntent : (icRenderingIntent)pCfg->m_intent,
      pCfg->m_interpolation,
      pPcc,
      pCfg->m_transform,
      pCfg->m_useD2BxB2Dx,
      &Hint,
      pCfg->m_useV5SubProfile
    );
    if (stat == icCmmStatCantOpenProfile) {
      sStageErr = "unable to open ICC profile '" + pCfg->m_iccFile + "'";
    }
    else if (stat != icCmmStatOk) {
      std::ostringstream oss;
      oss << "AddXform failed for '" << pCfg->m_iccFile << "' (status " << (int)stat << ")";
      sStageErr = oss.str();
    }

  search_stage_failed:
    if (stat != icCmmStatOk) {
      std::ostringstream oss;
      oss << "stage " << stageIdx << ": " << sStageErr;
      sErrorMsg = oss.str();
      ReleasePccList(pccList);
      return nullptr;
    }
    ++stageIdx;
  }

  // Attach weighted PCC profiles for spectral search computation.
  size_t pccIdx = 0;
  for (const auto& pccPtr : searchApply.m_pccWeights) {
    CIccCfgPccWeight* pPccWeight = pccPtr.get();
    if (!pPccWeight) {
      ++pccIdx;
      continue;
    }

    // Caller-selected config paths intentionally name ICC/PCC profile files.
    // codeql[cpp/path-injection]
    CIccProfile* pPcc = OpenIccProfile(pPccWeight->m_pccPath.c_str());
    if (!pPcc || !pPcc->ReadPccTags()) {
      std::ostringstream oss;
      oss << "weighted PCC " << pccIdx << ": unable to open or read PCC tags from '"
          << pPccWeight->m_pccPath << "'";
      sErrorMsg = oss.str();
      delete pPcc;
      ReleasePccList(pccList);
      return nullptr;
    }
    pPcc->Detach();

    if (pCmm->AttachPCC(pPcc, pPccWeight->m_dWeight) != icCmmStatOk) {
      std::ostringstream oss;
      oss << "weighted PCC " << pccIdx << ": AttachPCC failed for '"
          << pPccWeight->m_pccPath << "'";
      sErrorMsg = oss.str();
      delete pPcc;
      ReleasePccList(pccList);
      return nullptr;
    }
    ++pccIdx;
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
      sErrorMsg = "initial-destination: unable to open ICC profile '" + pLastCfg->m_iccFile + "'";
      ReleasePccList(pccList);
      return nullptr;
    }

    if (!pLastCfg->m_pccFile.empty()) {
      // Caller-selected config paths intentionally name ICC/PCC profile files.
      // codeql[cpp/path-injection]
      pPcc = OpenIccProfile(pLastCfg->m_pccFile.c_str());
      if (!pPcc) {
        sErrorMsg = "initial-destination: unable to open PCC profile '" + pLastCfg->m_pccFile + "'";
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

  icStatusCMM beginStat = pCmm->Begin();
  if (beginStat != icCmmStatOk) {
    std::ostringstream oss;
    oss << "Begin() failed (status " << (int)beginStat
        << "); search-profile chain is incompatible";
    sErrorMsg = oss.str();
    ReleasePccList(pccList);
    return nullptr;
  }

  ReleasePccList(pccList);
  return Attach(pCmm.release());
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
