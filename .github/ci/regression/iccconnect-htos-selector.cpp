/** @file
    File:       iccconnect-htos-selector.cpp

    Contains:   Regression coverage for the IccConnect useHToS control.

    ICC.2:2023 7.2.13 and 8.14 inject the HDR-to-SDR (H2Sx) transform after an
    extended-range source when the next profile is not extended-range, and
    Annex K.2.9 allows a CMM processing control only to disable it. IccConnect
    exposes that control as useHToS: on unless a configuration sets it false.
    This checks the default and both spellings, through the public API only:
    the config object, its JSON and legacy-argument parsers, the transforms a
    standard chain builds, and the pixels a search chain produces.

    Usage:
      iccconnect-htos-selector <extended.icc> <standard.icc>

    Exit codes:
      0 - every check passes
      1 - a check fails
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


#include "IccConnect.h"
#include "IccCmmConfig.h"
#include "IccCmmSearch.h"
#include "IccProfile.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace {

int Check(bool condition, const char* label)
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
  explicit ProfileXformCounter(const CIccProfile* profile)
    : m_profile(profile), m_count(0) {}

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

// Transforms built from the first profile: 2 when the H2Sx transform was
// injected after it, 1 when it was not.
unsigned int CountSourceProfileXforms(CIccCmm* cmm)
{
  if (!cmm || !cmm->GetFirstXform())
    return 0;
  ProfileXformCounter counter(cmm->GetFirstXform()->GetProfile());
  cmm->IterateXforms(&counter);
  return counter.Count();
}

enum StageHToS { kUnset, kOn, kOff };

CIccCfgProfilePtr MakeStage(const char* path, StageHToS hToS)
{
  CIccCfgProfilePtr stage(new CIccCfgProfile());
  stage->m_iccFile = path;
  stage->m_intent = icRelativeColorimetric;
  stage->m_interpolation = icInterpTetrahedral;
  if (hToS != kUnset)
    stage->m_useHToS = (hToS == kOn);
  return stage;
}

// Source stage built with hToS; the SDR destination stage is left at the
// default, which has no effect because only the source is extended-range.
unsigned int StandardChainCount(const char* ext, const char* sdr, StageHToS hToS)
{
  CIccCfgProfileSequence cfg;
  cfg.m_profiles.push_back(MakeStage(ext, hToS));
  cfg.m_profiles.push_back(MakeStage(sdr, kUnset));
  std::string err;
  std::unique_ptr<CIccConnectCmm> cmm(
    CIccConnectCmm::CreateStandard(cfg, nullptr, 0, 1, &err));
  if (!cmm || !cmm->GetCmm()) {
    std::fprintf(stderr, "iccconnect-htos-selector: standard chain: %s\n",
                 err.c_str());
    return 0;
  }
  return CountSourceProfileXforms(cmm->GetCmm());
}

// Applies one bright RGB sample through a two-profile search chain.
bool SearchChainApply(const char* ext, const char* sdr, StageHToS hToS,
                      icFloatNumber out[3])
{
  CIccCfgSearchApply cfg;
  cfg.m_profiles.push_back(MakeStage(ext, hToS));
  cfg.m_profiles.push_back(MakeStage(sdr, kUnset));
  std::string err;
  std::unique_ptr<CIccConnectCmm> cmm(
    CIccConnectCmm::CreateSearch(cfg, nullptr, 0, 1, &err));
  if (!cmm || !cmm->GetSearchCmm()) {
    std::fprintf(stderr, "iccconnect-htos-selector: search chain: %s\n",
                 err.c_str());
    return false;
  }
  const icFloatNumber in[3] = {0.75f, 0.75f, 0.75f};
  return cmm->GetSearchCmm()->Apply(out, in) == icCmmStatOk;
}

bool Same(const icFloatNumber a[3], const icFloatNumber b[3])
{
  for (int i = 0; i < 3; i++) {
    if (std::fabs(a[i] - b[i]) > 1.0e-6f)
      return false;
  }
  return true;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <extended.icc> <standard.icc>\n",
                 argv[0] ? argv[0] : "iccconnect-htos-selector");
    return 2;
  }
  const char* ext = argv[1];
  const char* sdr = argv[2];
  int failures = 0;

  // The default, and what JSON reads and writes.
  {
    CIccCfgProfile stage;
    failures += Check(stage.m_useHToS, "a new stage has useHToS on");

    failures += Check(stage.fromJson("{\"iccFile\":\"a.icc\"}", true) &&
                        stage.m_useHToS,
                      "JSON without useHToS leaves it on");
    json out;
    stage.toJson(out);
    failures += Check(!out.contains("useHToS"),
                      "an on stage writes no useHToS key");

    failures += Check(stage.fromJson("{\"iccFile\":\"a.icc\",\"useHToS\":false}",
                                     true) &&
                        !stage.m_useHToS,
                      "JSON useHToS false turns it off");
    out = json();
    stage.toJson(out);
    failures += Check(out.contains("useHToS") && out["useHToS"].is_boolean() &&
                        !out["useHToS"].get<bool>(),
                      "an off stage writes useHToS false");

    failures += Check(stage.fromJson("{\"iccFile\":\"a.icc\",\"useHToS\":true}",
                                     true) &&
                        stage.m_useHToS,
                      "JSON useHToS true keeps it on");
  }

  // Legacy arguments: with or without +100000 the stage is on.
  {
    CIccCfgProfileSequence seq;
    const char* args[] = {"1", "a.icc", "1", "b.icc", "100001"};
    failures += Check(seq.fromArgs(args, 5, true) == 5 &&
                        seq.m_profiles.size() == 2 &&
                        seq.m_profiles.front()->m_useHToS &&
                        seq.m_profiles.back()->m_useHToS,
                      "legacy intents 1 and 100001 both leave useHToS on");
  }

  // Standard chain: the injected transform follows the stage's control.
  failures += Check(StandardChainCount(ext, sdr, kUnset) == 2,
                    "standard chain: default injects the H2Sx transform");
  failures += Check(StandardChainCount(ext, sdr, kOn) == 2,
                    "standard chain: useHToS true injects it");
  failures += Check(StandardChainCount(ext, sdr, kOff) == 1,
                    "standard chain: useHToS false does not");

  // A CIccCmm built without IccConnect keeps the automatic ICC.2 behaviour.
  {
    CIccCmm raw;
    icStatusCMM status = raw.AddXform(ext, icRelativeColorimetric,
                                      icInterpTetrahedral);
    if (status == icCmmStatOk)
      status = raw.AddXform(sdr, icRelativeColorimetric, icInterpTetrahedral);
    if (status == icCmmStatOk)
      status = raw.Begin();
    failures += Check(status == icCmmStatOk &&
                        CountSourceProfileXforms(&raw) == 2,
                      "direct CIccCmm injects the H2Sx transform");
  }

  // The reference overload copies the profile and drops the copy's IO before
  // Begin(), which is when the H2Sx tag is looked for (IccEval and IccPrmg
  // add their profiles this way, as do the search sub-chains).
  {
    std::unique_ptr<CIccProfile> profile(OpenIccProfile(ext));
    CIccCmm byRef;
    icStatusCMM status = profile
      ? byRef.AddXform(*profile, icRelativeColorimetric, icInterpTetrahedral)
      : icCmmStatCantOpenProfile;
    if (status == icCmmStatOk)
      status = byRef.AddXform(sdr, icRelativeColorimetric, icInterpTetrahedral);
    if (status == icCmmStatOk)
      status = byRef.Begin();
    failures += Check(status == icCmmStatOk &&
                        CountSourceProfileXforms(&byRef) == 2,
                      "CIccCmm::AddXform(CIccProfile&) injects the H2Sx transform");
  }

  // Search chain: its sub-chains add their profiles by reference, so this
  // also depends on the reference overload keeping the H2Sx tags.
  {
    icFloatNumber unset[3] = {0, 0, 0};
    icFloatNumber on[3] = {0, 0, 0};
    icFloatNumber off[3] = {0, 0, 0};
    const bool ok = SearchChainApply(ext, sdr, kUnset, unset) &&
                    SearchChainApply(ext, sdr, kOn, on) &&
                    SearchChainApply(ext, sdr, kOff, off);
    failures += Check(ok, "search chains build and apply");
    failures += Check(ok && !Same(on, off),
                      "search chain: useHToS false changes the result");
    failures += Check(ok && Same(unset, on),
                      "search chain: the default matches useHToS true");
  }

  return failures ? 1 : 0;
}
