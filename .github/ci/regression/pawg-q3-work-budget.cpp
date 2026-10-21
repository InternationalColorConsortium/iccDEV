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
// Regression: PAWG Q3 sampled the neutral diagonal and every device axis of an
// n-D LUT without accounting for CIccCLUT::InterpND's 2^N corner visits.  The
// 14- and 15-channel CFL slow units therefore took seconds in iccPawgReport and
// tens of seconds under sanitizer/coverage instrumentation.  Keep the 13-channel
// side of the work limit and make a real 14-channel CMM stop before Apply().
#include "IccProfile.h"
#include "IccTagLut.h"

#include "IccQualityMetrics.h"

#include <algorithm>
#include <cstdio>
#include <string>

using namespace iccquality;

static int g_failures = 0;

static void check(bool condition, const char *message)
{
  std::printf("%s: %s\n", condition ? "ok  " : "FAIL", message);
  if (!condition) {
    ++g_failures;
  }
}

static bool make_nd_lut_profile(CIccProfile &profile, int channels,
                                icColorSpaceSignature colorSpace)
{
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV4_3;
  profile.m_Header.deviceClass = icSigOutputClass;
  profile.m_Header.colorSpace = colorSpace;
  profile.m_Header.pcs = icSigLabData;

  CIccTagLutAtoB *lut = new CIccTagLutAtoB();
  if (!lut->Init(static_cast<icUInt8Number>(channels), 3)) {
    delete lut;
    return false;
  }
  lut->SetColorSpaces(colorSpace, icSigLabData);

  CIccCLUT *clut = lut->NewCLUT(2, 2);
  if (!clut) {
    delete lut;
    return false;
  }
  icFloatNumber *data = clut->GetData(0);
  std::fill(data, data + static_cast<size_t>(clut->NumPoints()) *
      clut->GetOutputChannels(), 0.0f);

  profile.AttachTag(icSigAToB0Tag, lut);
  return true;
}

int main()
{
  check(nd_lut_smoothness_within_work_budget(13),
        "13-channel n-D LUT remains inside the Q3 work budget");
  check(!nd_lut_smoothness_within_work_budget(14),
        "14-channel n-D LUT exceeds the Q3 work budget");
  check(!nd_lut_smoothness_within_work_budget(15),
        "15-channel n-D LUT exceeds the Q3 work budget");

  CIccProfile profile;
  check(make_nd_lut_profile(profile, 14, icSig14colorData),
        "construct 14-channel n-D LUT control profile");

  SmoothnessMetrics metrics;
  std::string reason;
  const bool measured = measure_transform_smoothness(&profile, metrics, reason);
  check(!measured, "Q3 dispatcher refuses the 14-channel n-D LUT before sampling");
  check(!metrics.measured, "refused Q3 metric is not marked measured");
  check(reason.find("sample grid exceeds quality metric budget") != std::string::npos,
        "Q3 refusal is classified as not applicable");

  if (g_failures) {
    std::printf("\n%d case(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall cases passed\n");
  return 0;
}
