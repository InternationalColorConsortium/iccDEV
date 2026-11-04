/** @file
    File:       iccconnect-htos-selector.cpp

    Contains:   Regression coverage for the IccConnect useHToS selector.

    The ICC.2 HDR-to-SDR transform is normally selected when an extended-range
    source profile is followed by a non-extended profile. ICC.2:2023 K.2.9
    permits a CMM processing control to disable that otherwise indicated
    transform. IccConnect exposes that control as useHToS.

    Usage:
      iccconnect-htos-selector <extended.icc> <standard.icc>

    Exit codes:
      0 - selector controls actual standard and search sub-chains
      1 - unexpected result
      2 - usage error
    Copyright:  See ICC Software License
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Inspect the search sub-chain without adding a public test-only accessor.
#define protected public
#include "IccConnect.h"
#undef protected
#include "IccCmmConfig.h"

static int Check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "iccconnect-htos-selector: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "iccconnect-htos-selector: FAIL  %s\n", label);
  return 1;
}

class ProfileXformCounter : public IXformIterator
{
public:
  ProfileXformCounter(const CIccProfile* profile) : m_profile(profile), m_count(0) {}

  virtual void iterate(const CIccXform* xform)
  {
    if (xform && xform->GetProfile() == m_profile)
      m_count++;
  }

  unsigned int Count() const { return m_count; }

private:
  const CIccProfile* m_profile;
  unsigned int m_count;
};

static unsigned int CountSourceProfileXforms(CIccCmm* cmm)
{
  if (!cmm || !cmm->GetFirstXform())
    return 0;

  ProfileXformCounter counter(cmm->GetFirstXform()->GetProfile());
  cmm->IterateXforms(&counter);
  return counter.Count();
}

static CIccCfgProfilePtr MakeStage(const char* path, bool useHToS)
{
  CIccCfgProfilePtr stage(new CIccCfgProfile());
  stage->m_iccFile = path ? path : "";
  stage->m_intent = icRelativeColorimetric;
  stage->m_transform = icXformLutColor;
  stage->m_interpolation = icInterpTetrahedral;
  stage->m_useHToS = useHToS;
  return stage;
}

static CIccCfgProfileSequence MakeStandardSequence(const char* extendedPath,
                                                    const char* standardPath,
                                                    bool useHToS)
{
  CIccCfgProfileSequence sequence;
  sequence.m_profiles.push_back(MakeStage(extendedPath, useHToS));
  sequence.m_profiles.push_back(MakeStage(standardPath, false));
  return sequence;
}

static CIccCfgSearchApply MakeSearchSequence(const char* extendedPath,
                                              const char* standardPath,
                                              bool useHToS)
{
  CIccCfgSearchApply sequence;
  sequence.m_profiles.push_back(MakeStage(extendedPath, useHToS));
  sequence.m_profiles.push_back(MakeStage(standardPath, false));
  return sequence;
}

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <extended.icc> <standard.icc>\n",
                 argv[0] ? argv[0] : "iccconnect-htos-selector");
    return 2;
  }

  const char* extendedPath = argv[1];
  const char* standardPath = argv[2];
  int failures = 0;

  std::string disabledError;
  CIccCfgProfileSequence disabledCfg =
    MakeStandardSequence(extendedPath, standardPath, false);
  std::unique_ptr<CIccConnectCmm> disabled(
    CIccConnectCmm::CreateStandard(disabledCfg, nullptr, 0, 1,
                                   &disabledError));
  if (!disabled || !disabled->GetCmm()) {
    std::fprintf(stderr, "iccconnect-htos-selector: disabled chain: %s\n",
                 disabledError.c_str());
    return 1;
  }

  std::string enabledError;
  CIccCfgProfileSequence enabledCfg =
    MakeStandardSequence(extendedPath, standardPath, true);
  std::unique_ptr<CIccConnectCmm> enabled(
    CIccConnectCmm::CreateStandard(enabledCfg, nullptr, 0, 1,
                                   &enabledError));
  if (!enabled || !enabled->GetCmm()) {
    std::fprintf(stderr, "iccconnect-htos-selector: enabled chain: %s\n",
                 enabledError.c_str());
    return 1;
  }

  failures += Check(CountSourceProfileXforms(disabled->GetCmm()) == 1,
                    "useHToS=false suppresses HDR-to-SDR injection");
  failures += Check(CountSourceProfileXforms(enabled->GetCmm()) == 2,
                    "useHToS=true injects the HDR-to-SDR transform");
  failures += Check(!disabled->GetCmm()->GetFirstXform()->UseHToS() &&
                      enabled->GetCmm()->GetFirstXform()->UseHToS(),
                    "config selector reaches the source xform");

  CIccCmm raw;
  icStatusCMM status = raw.AddXform(extendedPath, icRelativeColorimetric,
                                    icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = raw.AddXform(standardPath, icRelativeColorimetric,
                          icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = raw.Begin();
  failures += Check(status == icCmmStatOk &&
                      CountSourceProfileXforms(&raw) == 2,
                    "non-config CMM retains automatic ICC.2 behavior");

  std::string searchDisabledError;
  CIccCfgSearchApply searchDisabledCfg =
    MakeSearchSequence(extendedPath, standardPath, false);
  std::unique_ptr<CIccConnectCmm> searchDisabled(
    CIccConnectCmm::CreateSearch(searchDisabledCfg, nullptr, 0, 1,
                                 &searchDisabledError));

  std::string searchEnabledError;
  CIccCfgSearchApply searchEnabledCfg =
    MakeSearchSequence(extendedPath, standardPath, true);
  std::unique_ptr<CIccConnectCmm> searchEnabled(
    CIccConnectCmm::CreateSearch(searchEnabledCfg, nullptr, 0, 1,
                                 &searchEnabledError));

  if (!searchDisabled || !searchDisabled->GetSearchCmm() ||
      !searchEnabled || !searchEnabled->GetSearchCmm()) {
    std::fprintf(stderr,
                 "iccconnect-htos-selector: search chains: off='%s' on='%s'\n",
                 searchDisabledError.c_str(), searchEnabledError.c_str());
    return 1;
  }

  CIccCmm* searchOff = searchDisabled->GetSearchCmm()->m_mid_to_dst.get();
  CIccCmm* searchOn = searchEnabled->GetSearchCmm()->m_mid_to_dst.get();
  failures += Check(CountSourceProfileXforms(searchOff) == 1,
                    "search useHToS=false suppresses injection");
  failures += Check(CountSourceProfileXforms(searchOn) == 2,
                    "search useHToS=true injects HDR-to-SDR transform");

  return failures ? 1 : 0;
}
