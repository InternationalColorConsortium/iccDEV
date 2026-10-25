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
// CTest contract for the transforms CIccApplyBPC builds to estimate a black
// point (#2868).
//
// CIccApplyBPC::pixelXfm() applied a transform built from the profile into
// fixed buffers its callers own: a 16-sample device pixel and the
// three-sample black point. The profile, not the caller, decides how many
// samples that transform writes. A MultiplexIdentification profile whose data
// colour space is CMYK runs its four-channel AToM0 as the device -> "PCS"
// probe, and Apply() wrote four floats into CalcFactors()' three-float XYZbp:
// a stack buffer overflow under iccApplyProfiles with intent 40 (BPC).
//
// pixelXfm() now refuses a transform wider than either buffer, so
// CalcFactors() fails and Begin() reports icCmmStatCantAdjustPcs. On an
// unfixed library the overflow either aborts under AddressSanitizer or lets
// CalcFactors() succeed and Begin() return icCmmStatOk, so the status check
// below fails in both kinds of build.
//
// The same profile without the BPC hint is a control: it must still begin and
// apply its empty AToM0 as a four-channel identity.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccApplyBPC.h"
#include "IccProfile.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"

#include <cmath>
#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char *label)
{
  if (!ok) {
    std::printf("FAIL: %s\n", label);
    ++g_failures;
  }
  else {
    std::printf("ok:   %s\n", label);
  }
}

const icColorSpaceSignature kMcs4 =
    (icColorSpaceSignature)icNColorSpaceSig(icSigSrcMCSChannelData, 4);

// The #2868 shape: v5 MultiplexIdentification, CMYK data colour space, no
// PCS, a four-channel MCS and an empty 4 -> 4 AToM0 (an identity).
CIccProfile *makeMidCmykProfile(icColorSpaceSignature pcs = icSigUnknownData)
{
  CIccProfile *pICC = new CIccProfile;
  pICC->m_Header.version = icVersionNumberV5;
  pICC->m_Header.deviceClass = icSigMultiplexIdentificationClass;
  pICC->m_Header.colorSpace = icSigCmykData;
  pICC->m_Header.pcs = pcs;
  pICC->m_Header.mcs = (icMultiplexColorSignature)kMcs4;

  CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement;
  pTag->SetChannels(4, 4);
  pICC->AttachTag(icSigAToM0Tag, pTag);
  return pICC;
}

void midWithoutBpcStillApplies()
{
  CIccCmm cmm(icSigCmykData, kMcs4, true);
  check(cmm.AddXform(makeMidCmykProfile(), icPerceptual, icInterpLinear, NULL,
                     icXformLutMCS, false) == icCmmStatOk,
        "control: AddXform without BPC");
  icStatusCMM stat = cmm.Begin();
  check(stat == icCmmStatOk, "control: Begin without BPC");
  if (stat != icCmmStatOk)
    return;

  icFloatNumber src[4] = { 0.20f, 0.40f, 0.60f, 0.80f };
  icFloatNumber dst[4] = { -1.0f, -1.0f, -1.0f, -1.0f };
  check(cmm.Apply(dst, src) == icCmmStatOk, "control: Apply");
  bool same = true;
  for (int i = 0; i < 4; i++)
    same = same && std::fabs(dst[i] - src[i]) < 1e-6f;
  check(same, "control: the empty AToM0 passes all four channels through");
}

// The same profile with a Lab header PCS passes CalcFactors()'s header test,
// but its probe still runs the four-channel AToM0: only pixelXfm()'s width
// check stands between it and the three-sample black point.
void midWithLabHeaderAndBpcIsRefused()
{
  CIccCreateXformHintManager hints;
  hints.AddHint(new CIccApplyBPCHint());

  CIccCmm cmm(icSigCmykData, kMcs4, true);
  check(cmm.AddXform(makeMidCmykProfile(icSigLabData), icPerceptual, icInterpLinear, NULL,
                     icXformLutMCS, false, &hints) == icCmmStatOk,
        "BPC, Lab header: AddXform");
  check(cmm.Begin() == icCmmStatCantAdjustPcs,
        "BPC, Lab header: the four-channel probe is refused by its width");
}

void midWithBpcIsRefused()
{
  CIccCreateXformHintManager hints;
  hints.AddHint(new CIccApplyBPCHint());

  CIccCmm cmm(icSigCmykData, kMcs4, true);
  check(cmm.AddXform(makeMidCmykProfile(), icPerceptual, icInterpLinear, NULL,
                     icXformLutMCS, false, &hints) == icCmmStatOk,
        "BPC: AddXform");
  check(cmm.Begin() == icCmmStatCantAdjustPcs,
        "BPC: the four-channel black point probe is refused, not applied into "
        "a three-sample buffer");
}

// A v5 input profile whose header PCS is not XYZ or Lab: RGB data, an AToB0
// matrix with as many outputs as the header PCS has samples.  The CMM accepts
// the chain; black point compensation, an XYZ affine, has no meaning for it.
CIccProfile *makeNonPcsProfile(icColorSpaceSignature pcs, icUInt16Number nOut)
{
  CIccProfile *pICC = new CIccProfile;
  pICC->m_Header.version = icVersionNumberV5;
  pICC->m_Header.deviceClass = icSigInputClass;
  pICC->m_Header.colorSpace = icSigRgbData;
  pICC->m_Header.pcs = pcs;

  CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(3, nOut);
  CIccMpeMatrix *pMtx = new CIccMpeMatrix;
  pMtx->SetSize(3, nOut, true);
  for (int r = 0; r < nOut; r++) {
    for (int c = 0; c < 3; c++)
      pMtx->GetMatrix()[r * 3 + c] = (r == c) ? 1.0f : 0.0f;
    pMtx->GetConstants()[r] = 0.0f;
  }
  pTag->Attach(pMtx);
  pICC->AttachTag(icSigAToB0Tag, pTag);
  return pICC;
}

// #2886: a header PCS of CMYK with a four-output AToB0 overflowed the black
// point the same way as #2868.  A header PCS narrower than three samples wrote
// two values and left the third uninitialised for the black point arithmetic.
// CalcFactors() now refuses any header PCS that is not XYZ or Lab before the
// probe runs, so both report icCmmStatCantAdjustPcs.
void nonPcsHeaderIsRefused(icColorSpaceSignature pcs, icUInt16Number nOut, const char *label)
{
  CIccCreateXformHintManager hints;
  hints.AddHint(new CIccApplyBPCHint());

  CIccCmm cmm(icSigRgbData, pcs, true);
  std::string what = std::string(label) + ": AddXform";
  check(cmm.AddXform(makeNonPcsProfile(pcs, nOut), icPerceptual, icInterpLinear, NULL,
                     icXformLutColor, false, &hints) == icCmmStatOk, what.c_str());
  what = std::string(label) + ": BPC refused for a header PCS that is not XYZ or Lab";
  check(cmm.Begin() == icCmmStatCantAdjustPcs, what.c_str());
}

} // namespace

int main()
{
  midWithoutBpcStillApplies();
  midWithBpcIsRefused();
  midWithLabHeaderAndBpcIsRefused();
  nonPcsHeaderIsRefused(icSigCmykData, 4, "CMYK header PCS (#2886)");
  nonPcsHeaderIsRefused(icSig2colorData, 2, "two-sample header PCS");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
