/** @file
    File:       cmm-htos-selection.cpp

    Contains:   Regression coverage for the ICC.2 HDR-to-SDR tag selection
                in CIccCmm::CheckPCSRangeConversions().

    ICC.2:2023 8.14.2 (input, display, output and colour space profiles) and
    8.14.7 (MultiplexVisualization) select the range conversion as: the
    HdrToSdrXTag for the rendering intent, then HdrToSdr0Tag, HdrToSdr1Tag
    and HdrToSdr3Tag. 8.14.3 gives abstract profiles HdrToSdr0Tag only.

    The generated HDR fixture carries one H2S1 tag. Each case copies that
    transform under a single H2Sx signature (removing the others), optionally
    restamps the profile as abstract, connects it to an SDR profile and counts
    the transforms the CMM builds from the source profile: 2 means a range
    conversion was injected, 1 means none was. The precedence cases attach
    the real transform under one signature and constant-output decoys under
    the others, and require the converted pixel to match the real transform's.
    Every variant is built in memory, so no binary fixture is added.

    Usage:
      cmm-htos-selection <extended.icc> <standard.icc>

    Exit codes:
      0 - every case matches
      1 - a case does not match
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
#include "IccMpeBasic.h"
#include "IccProfile.h"
#include "IccTagMPE.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <memory>

namespace {

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

unsigned int CountSourceProfileXforms(CIccCmm& cmm)
{
  const CIccXform* first = cmm.GetFirstXform();
  if (!first)
    return 0;

  ProfileXformCounter counter(first->GetProfile());
  cmm.IterateXforms(&counter);
  return counter.Count();
}

// A 3-channel HDR-to-SDR stand-in whose output is a constant, so a pixel
// converted through it cannot match one converted through the real tag.
CIccTag* NewDecoy()
{
  std::unique_ptr<CIccTagMultiProcessElement> tag(
    new CIccTagMultiProcessElement(3, 3));
  CIccMpeMatrix* matrix = new CIccMpeMatrix();
  if (!matrix->SetSize(3, 3, true)) {
    delete matrix;
    return NULL;
  }
  for (int i = 0; i < 9; i++)
    matrix->GetMatrix()[i] = 0;
  matrix->GetConstants()[0] = 0.3f;
  matrix->GetConstants()[1] = 0.2f;
  matrix->GetConstants()[2] = 0.1f;
  tag->Attach(matrix);
  return tag.release();
}

// Returns the fixture stamped with profileClass, its H2S1 transform copied to
// realTag (0 = no real tag) and a decoy under each of decoyTags.
CIccProfile* MakeProfileVariant(const char* path,
                                icProfileClassSignature profileClass,
                                icTagSignature realTag,
                                std::initializer_list<icTagSignature> decoyTags = {})
{
  std::unique_ptr<CIccProfile> profile(OpenIccProfile(path));
  if (!profile || !profile->ReadTags(profile.get()))
    return NULL;

  CIccTag* sourceHToS = profile->FindTag(icSigHToS1Tag);
  if (!sourceHToS)
    return NULL;

  std::unique_ptr<CIccTag> selected;
  if (realTag)
    selected.reset(sourceHToS->NewCopy());

  if (profileClass == icSigAbstractClass) {
    // An abstract profile's only colorimetric transform is AToB0, and it
    // connects PCS to PCS.
    if (!profile->FindTag(icSigAToB0Tag)) {
      CIccTag* sourceAToB = profile->FindTag(icSigAToB1Tag);
      if (!sourceAToB)
        return NULL;
      std::unique_ptr<CIccTag> abstractAToB(sourceAToB->NewCopy());
      if (!profile->AttachTag(icSigAToB0Tag, abstractAToB.get()))
        return NULL;
      abstractAToB.release();
    }
    profile->m_Header.colorSpace = profile->m_Header.pcs;
  }
  profile->m_Header.deviceClass = profileClass;

  profile->DeleteTag(icSigHToS0Tag);
  profile->DeleteTag(icSigHToS1Tag);
  profile->DeleteTag(icSigHToS2Tag);
  profile->DeleteTag(icSigHToS3Tag);

  if (selected) {
    if (!profile->AttachTag(realTag, selected.get()))
      return NULL;
    selected.release();
  }
  for (icTagSignature sig : decoyTags) {
    std::unique_ptr<CIccTag> decoy(NewDecoy());
    if (!decoy || !profile->AttachTag(sig, decoy.get()))
      return NULL;
    decoy.release();
  }

  // Every tag is in memory now; the CMM must not read the file again.
  profile->Detach();
  return profile.release();
}

// Connects a variant to the SDR profile; reports the source-profile transform
// count and, when pixel is given, converts one bright grey into it.
icStatusCMM Connect(CIccProfile* profile, const char* standardPath,
                    icRenderingIntent intent, unsigned int& count,
                    icFloatNumber* pixel = NULL)
{
  count = 0;
  if (!profile)
    return icCmmStatAllocErr;
  CIccCmm cmm;
  // AddXform owns profile on success and on failure.
  icStatusCMM status = cmm.AddXform(profile, intent, icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = cmm.AddXform(standardPath, intent, icInterpTetrahedral);
  if (status == icCmmStatOk)
    status = cmm.Begin();
  if (status != icCmmStatOk)
    return status;
  count = CountSourceProfileXforms(cmm);
  if (pixel) {
    const icFloatNumber in[3] = {0.75f, 0.75f, 0.75f};
    status = cmm.Apply(pixel, in);
  }
  return status;
}

int Case(const char* extendedPath, const char* standardPath,
         icProfileClassSignature profileClass, icTagSignature hToSTag,
         icRenderingIntent intent, unsigned int expected, const char* label)
{
  unsigned int got = 0;
  icStatusCMM status = Connect(
    MakeProfileVariant(extendedPath, profileClass, hToSTag), standardPath,
    intent, got);

  if (status == icCmmStatOk && got == expected) {
    std::fprintf(stdout, "cmm-htos-selection: PASS  %s\n", label);
    return 0;
  }
  std::fprintf(stderr,
               "cmm-htos-selection: FAIL  %s (status %d, %u source "
               "transforms, expected %u)\n",
               label, (int)status, got, expected);
  return 1;
}

bool SamePixel(const icFloatNumber a[3], const icFloatNumber b[3])
{
  for (int i = 0; i < 3; i++) {
    if (std::fabs(a[i] - b[i]) > 1.0e-5f)
      return false;
  }
  return true;
}

// With the real transform under realTag and decoys under decoyTags, the CMM
// has to pick realTag: the pixel must match the one the real tag gives alone,
// and differ from the one a decoy gives alone.
int Precedence(const char* extendedPath, const char* standardPath,
               icProfileClassSignature profileClass, icRenderingIntent intent,
               icTagSignature realTag,
               std::initializer_list<icTagSignature> decoyTags,
               const char* label)
{
  icFloatNumber realOnly[3] = {0, 0, 0};
  icFloatNumber decoyOnly[3] = {0, 0, 0};
  icFloatNumber mixed[3] = {0, 0, 0};
  unsigned int nReal = 0, nDecoy = 0, nMixed = 0;

  icStatusCMM status = Connect(
    MakeProfileVariant(extendedPath, profileClass, realTag), standardPath,
    intent, nReal, realOnly);
  if (status == icCmmStatOk)
    status = Connect(
      MakeProfileVariant(extendedPath, profileClass,
                         static_cast<icTagSignature>(0), {realTag}),
      standardPath, intent, nDecoy, decoyOnly);
  if (status == icCmmStatOk)
    status = Connect(
      MakeProfileVariant(extendedPath, profileClass, realTag, decoyTags),
      standardPath, intent, nMixed, mixed);

  const bool ok = status == icCmmStatOk && nReal == 2 && nDecoy == 2 &&
                  nMixed == 2 && !SamePixel(realOnly, decoyOnly) &&
                  SamePixel(mixed, realOnly);
  if (ok) {
    std::fprintf(stdout, "cmm-htos-selection: PASS  %s\n", label);
    return 0;
  }
  std::fprintf(stderr,
               "cmm-htos-selection: FAIL  %s (status %d, transforms %u/%u/%u, "
               "real %g %g %g, decoy %g %g %g, mixed %g %g %g)\n",
               label, (int)status, nReal, nDecoy, nMixed,
               realOnly[0], realOnly[1], realOnly[2],
               decoyOnly[0], decoyOnly[1], decoyOnly[2],
               mixed[0], mixed[1], mixed[2]);
  return 1;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <extended.icc> <standard.icc>\n",
                 argv[0] ? argv[0] : "cmm-htos-selection");
    return 2;
  }
  const char* ext = argv[1];
  const char* sdr = argv[2];
  const icProfileClassSignature disp = icSigDisplayClass;
  const icProfileClassSignature abst = icSigAbstractClass;
  const icTagSignature none = static_cast<icTagSignature>(0);

  int failures = 0;

  // 8.14.2. A saturation request with no H2S2 walks the fallbacks.
  failures += Case(ext, sdr, disp, icSigHToS1Tag, icRelativeColorimetric, 2,
                   "display: the tag for the intent (H2S1, relative)");
  failures += Case(ext, sdr, disp, icSigHToS0Tag, icSaturation, 2,
                   "display: fallback 2 is H2S0");
  failures += Case(ext, sdr, disp, icSigHToS1Tag, icSaturation, 2,
                   "display: fallback 3 is H2S1");
  failures += Case(ext, sdr, disp, icSigHToS3Tag, icSaturation, 2,
                   "display: fallback 4 is H2S3");
  failures += Case(ext, sdr, disp, icSigHToS2Tag, icPerceptual, 1,
                   "display: H2S2 is never a fallback");
  failures += Case(ext, sdr, disp, none, icSaturation, 1,
                   "display: no H2S tag, no range conversion");

  // 8.14.3. Abstract: H2S0 whatever the intent, and nothing else.
  failures += Case(ext, sdr, abst, icSigHToS0Tag, icSaturation, 2,
                   "abstract: H2S0 under saturation");
  failures += Case(ext, sdr, abst, icSigHToS1Tag, icRelativeColorimetric, 1,
                   "abstract: ignores H2S1 for its own intent");
  failures += Case(ext, sdr, abst, icSigHToS1Tag, icPerceptual, 1,
                   "abstract: ignores H2S1 as a fallback");
  failures += Case(ext, sdr, abst, icSigHToS2Tag, icSaturation, 1,
                   "abstract: ignores H2S2");
  failures += Case(ext, sdr, abst, icSigHToS3Tag, icAbsoluteColorimetric, 1,
                   "abstract: ignores H2S3");

  // Precedence with several tags present: the real transform has to win.
  failures += Precedence(ext, sdr, disp, icRelativeColorimetric,
                         icSigHToS1Tag, {icSigHToS0Tag, icSigHToS3Tag},
                         "display: the intent's tag beats H2S0 and H2S3");
  failures += Precedence(ext, sdr, disp, icSaturation,
                         icSigHToS0Tag, {icSigHToS1Tag, icSigHToS3Tag},
                         "display: H2S0 beats H2S1 and H2S3");
  failures += Precedence(ext, sdr, disp, icSaturation,
                         icSigHToS1Tag, {icSigHToS3Tag},
                         "display: H2S1 beats H2S3");
  failures += Precedence(ext, sdr, abst, icRelativeColorimetric,
                         icSigHToS0Tag,
                         {icSigHToS1Tag, icSigHToS2Tag, icSigHToS3Tag},
                         "abstract: H2S0 beats the intent's tag");

  return failures ? 1 : 0;
}
