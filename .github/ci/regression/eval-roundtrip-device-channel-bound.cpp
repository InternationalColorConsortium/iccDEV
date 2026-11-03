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
/*
 * Issue #2973: CIccEvalCompare::EvaluateProfile() accepts device spaces through 16 channels,
 * but its input sample was historically icFloatNumber[15].  With nGran=2 a
 * valid 16-channel ICC.2 profile reached the sampling loop and wrote sPixel[15]
 * on every node (CWE-121).  The stock iccRoundTrip command did not expose the
 * boundary because its default granularity is rejected by the independent
 * sample-count cap before sampling; the exported library API accepts nGran=2.
 *
 * Build minimal 15-, 16-, and 17-channel profiles entirely through public IccProfLib
 * objects.  Each carries relative-colorimetric calculator MPEs in both
 * directions, matching the transforms EvaluateProfile builds.  The first two
 * must traverse all 2^n binary-grid nodes, and Compare() reads every advertised
 * device channel.  The 17-channel case must retain the explicit refusal above
 * the buffer's limit.  Sanitizer builds therefore make the old 16-channel array
 * mismatch deterministically red while pinning both sides of the boundary.
 */

#include "IccEval.h"
#include "IccMpeCalc.h"
#include "IccProfile.h"
#include "IccTagMPE.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[eval-roundtrip-device-channel-bound] FAIL: %s\n", what);
  }
}

CIccTagMultiProcessElement *makeCalculatorTag(icUInt16Number nInput,
                                               icUInt16Number nOutput,
                                               const std::string &program)
{
  CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(nInput, nOutput);
  CIccMpeCalculator *pCalc = new CIccMpeCalculator(nInput, nOutput);
  std::string report;

  if (pCalc->SetCalcFunc(program.c_str(), report) != icFuncParseNoError) {
    std::fprintf(stderr,
                 "[eval-roundtrip-device-channel-bound] calculator parse failed: %s\n",
                 report.c_str());
    delete pCalc;
    delete pTag;
    return NULL;
  }

  pTag->Attach(pCalc);
  return pTag;
}

bool makeProfile(CIccProfile &profile, icUInt16Number nChannels)
{
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV5;
  profile.m_Header.deviceClass = icSigOutputClass;
  profile.m_Header.colorSpace =
      (icColorSpaceSignature)(icSigNChannelData | nChannels);
  profile.m_Header.pcs = icSigLabData;

  CIccTagMultiProcessElement *pForward =
      makeCalculatorTag(nChannels, 3, "{ in(0,3) out(0,3) }");

  std::string reverse = "{ in(0,3)";
  for (icUInt16Number i = 3; i < nChannels; i++)
    reverse += " 0";
  reverse += " out(0," + std::to_string(nChannels) + ") }";

  CIccTagMultiProcessElement *pReverse =
      makeCalculatorTag(3, nChannels, reverse);
  if (!pForward || !pReverse) {
    delete pForward;
    delete pReverse;
    return false;
  }

  if (!profile.AttachTag(icSigAToB1Tag, pForward)) {
    delete pForward;
    delete pReverse;
    return false;
  }
  if (!profile.AttachTag(icSigBToA1Tag, pReverse)) {
    delete pReverse;
    return false;
  }
  return true;
}

class BoundaryEval : public CIccEvalCompare
{
public:
  explicit BoundaryEval(icUInt16Number nChannels)
      : m_nChannels(nChannels), m_nComparisons(0), m_bSawLastChannelOne(false)
  {
  }

  virtual void Compare(icFloatNumber *pPixel, icFloatNumber *, icFloatNumber *,
                       icFloatNumber *)
  {
    ++m_nComparisons;
    for (icUInt16Number i = 0; i < m_nChannels; i++) {
      if (pPixel[i] != 0.0f && pPixel[i] != 1.0f) {
        check(false, "binary-grid device sample is neither zero nor one");
        break;
      }
    }
    if (pPixel[m_nChannels - 1] == 1.0f)
      m_bSawLastChannelOne = true;
  }

  icUInt16Number m_nChannels;
  icUInt32Number m_nComparisons;
  bool m_bSawLastChannelOne;
};

void runBoundary(icUInt16Number nChannels)
{
  CIccProfile profile;
  char what[192];

  std::snprintf(what, sizeof(what), "%u-channel profile setup succeeds",
                (unsigned)nChannels);
  const bool built = makeProfile(profile, nChannels);
  check(built, what);
  if (!built)
    return;

  BoundaryEval eval(nChannels);
  const icStatusCMM status =
      eval.EvaluateProfile(&profile, 2, icRelativeColorimetric,
                           icInterpLinear, true);

  std::snprintf(what, sizeof(what), "%u-channel nGran=2 evaluation succeeds",
                (unsigned)nChannels);
  check(status == icCmmStatOk, what);

  std::snprintf(what, sizeof(what),
                "%u-channel evaluation visits every 2^n grid node",
                (unsigned)nChannels);
  check(eval.m_nComparisons == (icUInt32Number(1) << nChannels), what);

  std::snprintf(what, sizeof(what),
                "%u-channel evaluation reaches one in its final channel",
                (unsigned)nChannels);
  check(eval.m_bSawLastChannelOne, what);
}

void runAboveBoundary()
{
  const icUInt16Number nChannels = 17;
  CIccProfile profile;
  const bool built = makeProfile(profile, nChannels);
  check(built, "17-channel refusal profile setup succeeds");
  if (!built)
    return;

  BoundaryEval eval(nChannels);
  const icStatusCMM status =
      eval.EvaluateProfile(&profile, 2, icRelativeColorimetric,
                           icInterpLinear, true);
  check(status == icCmmStatTooManySamples,
        "17-channel evaluation is refused above the device-pixel limit");
  check(eval.m_nComparisons == 0,
        "17-channel refusal happens before the sampling loop");
}

} // namespace

int main()
{
  runBoundary(15);
  runBoundary(16);
  runAboveBoundary();

  if (g_fail)
    std::fprintf(stderr, "[eval-roundtrip-device-channel-bound] %d assertion(s) failed\n",
                 g_fail);
  else
    std::printf("[eval-roundtrip-device-channel-bound] 15/16-channel boundaries and 17-channel refusal passed\n");

  return g_fail;
}
