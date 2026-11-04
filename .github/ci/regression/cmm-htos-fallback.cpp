/** @file
    File:       cmm-htos-fallback.cpp

    Contains:   Regression coverage for ICC.2 HDR-to-SDR tag fallback.

    ICC.2:2023 8.14.2 requires range-conversion lookup in this order:
    intent-specific H2Sx, H2S0, H2S1, and H2S3. The production fixture has an
    H2S1 tag. This helper loads that fixture, clones its transform under one
    selected signature, and removes the original tag directory entries. This
    creates H2S0-only, H2S1-only, H2S3-only, and no-H2S profiles in memory
    without checking generated binary profiles into the repository.

    Usage:
      cmm-htos-fallback <extended.icc> <standard.icc>

    Exit codes:
      0 - the complete fallback and negative-control matrix passes
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
        std::fprintf(stdout, "cmm-htos-fallback: PASS  %s\n", label);
        return 0;
    }

    std::fprintf(stderr, "cmm-htos-fallback: FAIL  %s\n", label);
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
                                       icTagSignature selectedTag)
{
    std::unique_ptr<CIccProfile> profile(OpenIccProfile(path));
    if (!profile || !profile->ReadTags(profile.get()))
        return NULL;

    CIccTag* original = profile->FindTag(icSigHToS1Tag);
    if (!original)
        return NULL;

    std::unique_ptr<CIccTag> selected;
    if (selectedTag)
        selected.reset(original->NewCopy());

    profile->DeleteTag(icSigHToS0Tag);
    profile->DeleteTag(icSigHToS1Tag);
    profile->DeleteTag(icSigHToS2Tag);
    profile->DeleteTag(icSigHToS3Tag);

    if (selected && !profile->AttachTag(selectedTag, selected.get()))
        return NULL;
    selected.release();

    // Every tag was loaded above, so the in-memory profile no longer needs
    // its deferred file IO after it is handed to the CMM.
    profile->Detach();
    return profile.release();
}

static bool CheckVariant(const char* extendedPath,
                         const char* standardPath,
                         icTagSignature selectedTag,
                         unsigned int expectedSourceXforms)
{
    CIccProfile* profile = MakeProfileVariant(extendedPath, selectedTag);
    if (!profile)
        return false;

    CIccCmm cmm;
    // AddXform owns profile on both success and failure.
    // Saturation requests H2S2. Because each variant deliberately lacks H2S2,
    // H2S0, H2S1, and H2S3 exercise fallback steps 2, 3, and 4 respectively.
    icStatusCMM status = cmm.AddXform(profile, icSaturation,
                                     icInterpTetrahedral);
    if (status == icCmmStatOk)
        status = cmm.AddXform(standardPath, icSaturation,
                              icInterpTetrahedral);
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
                     argv[0] ? argv[0] : "cmm-htos-fallback");
        return 2;
    }

    int failures = 0;
    failures += Check(CheckVariant(argv[1], argv[2], icSigHToS0Tag, 2),
                      "H2S0 is the first fallback");
    failures += Check(CheckVariant(argv[1], argv[2], icSigHToS1Tag, 2),
                      "H2S1 remains the second fallback");
    failures += Check(CheckVariant(argv[1], argv[2], icSigHToS3Tag, 2),
                      "H2S3 is the final fallback");
    failures += Check(CheckVariant(argv[1], argv[2],
                                   static_cast<icTagSignature>(0), 1),
                      "a profile without H2S tags injects no range transform");

    return failures ? 1 : 0;
}
