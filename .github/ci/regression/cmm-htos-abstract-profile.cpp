/** @file
    File:       cmm-htos-abstract-profile.cpp

    Contains:   Regression coverage for the ICC.2 abstract-profile H2S rule.

    ICC.2:2023 8.14.3 permits only H2S0 for an abstract profile. This helper
    loads the generated HDR fixture, clones its transforms into in-memory
    abstract-profile variants, and verifies that H2S1 and H2S3 are not selected
    merely because they match the requested rendering intent. A display-profile
    control proves that the ordinary intent-specific lookup remains active.

    Usage:
      cmm-htos-abstract-profile <extended.icc> <standard.icc>

    Exit codes:
      0 - the abstract-profile and display-control matrix passes
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

#include "IccCmm.h"
#include "IccProfile.h"

#include <cstdio>
#include <memory>

static int Check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "cmm-htos-abstract-profile: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "cmm-htos-abstract-profile: FAIL  %s\n", label);
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

static unsigned int CountSourceProfileXforms(CIccCmm& cmm)
{
  const CIccXform* first = cmm.GetFirstXform();
  if (!first)
    return 0;

  ProfileXformCounter counter(first->GetProfile());
  cmm.IterateXforms(&counter);
  return counter.Count();
}

static CIccProfile* MakeProfileVariant(const char* path,
                                       icProfileClassSignature profileClass,
                                       icTagSignature hToSTag)
{
  std::unique_ptr<CIccProfile> profile(OpenIccProfile(path));
  if (!profile || !profile->ReadTags(profile.get()))
    return NULL;

  CIccTag* sourceHToS = profile->FindTag(icSigHToS1Tag);
  CIccTag* sourceAToB = profile->FindTag(icSigAToB1Tag);
  if (!sourceHToS || !sourceAToB)
    return NULL;

  std::unique_ptr<CIccTag> selectedHToS(sourceHToS->NewCopy());
  if (profileClass == icSigAbstractClass) {
    if (!profile->FindTag(icSigAToB0Tag)) {
      std::unique_ptr<CIccTag> abstractAToB(sourceAToB->NewCopy());
      if (!profile->AttachTag(icSigAToB0Tag, abstractAToB.get()))
        return NULL;
      abstractAToB.release();
    }

    // Abstract profiles connect a PCS input to a PCS output.
    profile->m_Header.colorSpace = profile->m_Header.pcs;
  }

  profile->DeleteTag(icSigHToS0Tag);
  profile->DeleteTag(icSigHToS1Tag);
  profile->DeleteTag(icSigHToS2Tag);
  profile->DeleteTag(icSigHToS3Tag);

  if (!profile->AttachTag(hToSTag, selectedHToS.get()))
    return NULL;
  selectedHToS.release();

  profile->m_Header.deviceClass = profileClass;
  profile->Detach();
  return profile.release();
}

static bool CheckVariant(const char* extendedPath,
                         const char* standardPath,
                         icProfileClassSignature profileClass,
                         icTagSignature hToSTag,
                         icRenderingIntent intent,
                         unsigned int expectedSourceXforms)
{
  CIccProfile* profile =
    MakeProfileVariant(extendedPath, profileClass, hToSTag);
  if (!profile)
    return false;

  CIccCmm cmm;
  // AddXform owns profile on both success and failure.
  icStatusCMM status = cmm.AddXform(profile, intent, icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = cmm.AddXform(standardPath, intent, icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = cmm.Begin();

  return status == icCmmStatOk &&
         CountSourceProfileXforms(cmm) == expectedSourceXforms;
}

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr,
                 "usage: %s <extended.icc> <standard.icc>\n",
                 argv[0] ? argv[0] : "cmm-htos-abstract-profile");
    return 2;
  }

  int failures = 0;
  failures += Check(
    CheckVariant(argv[1], argv[2], icSigAbstractClass, icSigHToS0Tag,
                 icSaturation, 2),
    "abstract profile uses H2S0");
  failures += Check(
    CheckVariant(argv[1], argv[2], icSigAbstractClass, icSigHToS1Tag,
                 icRelativeColorimetric, 1),
    "abstract profile ignores intent-specific H2S1");
  failures += Check(
    CheckVariant(argv[1], argv[2], icSigAbstractClass, icSigHToS3Tag,
                 icAbsoluteColorimetric, 1),
    "abstract profile ignores intent-specific H2S3");
  failures += Check(
    CheckVariant(argv[1], argv[2], icSigDisplayClass, icSigHToS1Tag,
                 icRelativeColorimetric, 2),
    "display profile retains intent-specific H2S1");

  return failures ? 1 : 0;
}
