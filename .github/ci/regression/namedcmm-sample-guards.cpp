/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// CTest contract for CIccNamedColorCmm::Begin()'s sample-count guards (#2704).
//
// A caller sizes its pixel buffers from GetSourceSamples() and
// GetDestSamples(), which come from the spaces AddXform() recorded. The
// transforms move GetNumSrcSamples() and GetNumDstSamples(). CIccCmm::Begin()
// refuses a chain where the first transform's input count or the last
// transform's output count disagrees with the CMM's; CIccNamedColorCmm::Begin()
// had neither check.
//
//   destination: AddXform() records a profile's spectral PCS as the
//   destination whenever the header carries one, but CIccXform::Create() falls
//   back to a colorimetric AToBx tag when there is no DToBx. The CMM then
//   advertised the spectral channel count and Apply() wrote three. This is the
//   MemorySanitizer report: iccApplyNamedCmm formatted the unwritten rest.
//
//   source: the mirror image. For a PCS -> device profile, AddXform() records
//   the spectral PCS as the source, and the named CMM adopts the first
//   profile's source space as its own, but Create() falls back to a
//   colorimetric BToAx. The CMM asked the caller for the spectral channel
//   count and the transform read three of them as XYZ.
//
// Each case is checked against CIccCmm as well, which already refuses it, and
// a colorimetric-only version of the profile is the positive control.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagMPE.h"
#include "IccMpeBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "namedcmm-sample-guards: FAIL  %s\n", label);
    g_failures++;
  }
}

const icUInt16Number kSteps = 33;
const icColorSpaceSignature kSpectralPcs =
  (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | kSteps);

// An identity-like matrix element: nIn inputs, nOut outputs, 1 on the diagonal.
CIccTagMultiProcessElement *matrixTag(icUInt16Number nIn, icUInt16Number nOut)
{
  CIccTagMultiProcessElement *pTag = new CIccTagMultiProcessElement(nIn, nOut);
  CIccMpeMatrix *pMatrix = new CIccMpeMatrix();
  pMatrix->SetSize(nIn, nOut);
  std::memset(pMatrix->GetMatrix(), 0, nIn * nOut * sizeof(icFloatNumber));
  std::memset(pMatrix->GetConstants(), 0, nOut * sizeof(icFloatNumber));
  for (icUInt16Number i = 0; i < nIn && i < nOut; i++)
    pMatrix->GetMatrix()[i * nIn + i] = 1.0f;
  pTag->Attach(pMatrix);
  return pTag;
}

// A v5 RGB profile with an XYZ PCS. bSpectral adds a 33-channel reflectance
// spectral PCS to the header. The transform tags are chosen per case.
CIccProfile *v5Profile(icProfileClassSignature cls, bool bSpectral)
{
  CIccProfile *pProfile = new CIccProfile();
  pProfile->InitHeader();
  pProfile->m_Header.version = icVersionNumberV5;
  pProfile->m_Header.deviceClass = cls;
  pProfile->m_Header.colorSpace = icSigRgbData;
  pProfile->m_Header.pcs = icSigXYZData;
  if (bSpectral) {
    pProfile->m_Header.spectralPCS = kSpectralPcs;
    pProfile->m_Header.spectralRange.start = icFtoF16(400.0f);
    pProfile->m_Header.spectralRange.end = icFtoF16(720.0f);
    pProfile->m_Header.spectralRange.steps = kSteps;
  }
  return pProfile;
}

// Device -> PCS with only a colorimetric AToB3 (RGB -> XYZ).
CIccProfile *inputProfile(bool bSpectral)
{
  CIccProfile *pProfile = v5Profile(icSigInputClass, bSpectral);
  pProfile->AttachTag(icSigAToB3Tag, matrixTag(3, 3));
  return pProfile;
}

// PCS -> device with only a colorimetric BToA3 (XYZ -> RGB).
CIccProfile *outputProfile(bool bSpectral)
{
  CIccProfile *pProfile = v5Profile(icSigDisplayClass, bSpectral);
  pProfile->AttachTag(icSigBToA3Tag, matrixTag(3, 3));
  return pProfile;
}

// AddXform() failures are reported as icCmmStatBad, so a refusal the test
// counts has to come from Begin(), where the guards are.
icStatusCMM beginNamed(CIccProfile *pProfile, icColorSpaceSignature src, bool bInput,
                       int *pSrc = NULL, int *pDst = NULL)
{
  CIccNamedColorCmm cmm(src, icSigUnknownData, bInput);
  icStatusCMM rv = cmm.AddXform(pProfile, icPerceptual);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "namedcmm-sample-guards: named AddXform returned %d\n", (int)rv);
    return icCmmStatBad;
  }
  rv = cmm.Begin();
  if (pSrc) *pSrc = cmm.GetSourceSamples();
  if (pDst) *pDst = cmm.GetDestSamples();
  return rv;
}

icStatusCMM beginPlain(CIccProfile *pProfile, icColorSpaceSignature src, bool bInput)
{
  CIccCmm cmm(src, icSigUnknownData, bInput);
  icStatusCMM rv = cmm.AddXform(pProfile, icPerceptual);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "namedcmm-sample-guards: AddXform returned %d\n", (int)rv);
    return icCmmStatBad;
  }
  return cmm.Begin();
}

void testDestination()
{
  int nSrc = 0, nDst = 0;
  icStatusCMM rv = beginNamed(inputProfile(true), icSigRgbData, true, &nSrc, &nDst);
  if (rv == icCmmStatOk)
    std::fprintf(stderr, "namedcmm-sample-guards: named CMM began with %d destination samples\n", nDst);
  check(rv == icCmmStatBadSpaceLink,
        "named CMM refuses a spectral destination that an AToB3 cannot produce");
  check(beginPlain(inputProfile(true), icSigRgbData, true) == icCmmStatBadSpaceLink,
        "CIccCmm refuses the same chain");
}

void testSource()
{
  int nSrc = 0, nDst = 0;
  icStatusCMM rv = beginNamed(outputProfile(true), icSigUnknownData, false, &nSrc, &nDst);
  if (rv == icCmmStatOk)
    std::fprintf(stderr, "namedcmm-sample-guards: named CMM began with %d source samples\n", nSrc);
  check(rv == icCmmStatBadSpaceLink,
        "named CMM refuses a spectral source that a BToA3 cannot read");
  check(beginPlain(outputProfile(true), icSigUnknownData, false) == icCmmStatBadSpaceLink,
        "CIccCmm refuses the same chain");

  // Control: the same profile without the spectral PCS.
  rv = beginNamed(outputProfile(false), icSigUnknownData, false, &nSrc, &nDst);
  check(rv == icCmmStatOk && nSrc == 3 && nDst == 3,
        "control: the named CMM begins the colorimetric-only PCS -> device chain");
}

void testColorimetricControl()
{
  int nSrc = 0, nDst = 0;
  CIccNamedColorCmm cmm(icSigRgbData, icSigUnknownData, true);
  icStatusCMM rv = cmm.AddXform(inputProfile(false), icPerceptual);
  check(rv == icCmmStatOk, "control: the colorimetric-only profile adds");
  if (rv != icCmmStatOk)
    return;
  rv = cmm.Begin();
  check(rv == icCmmStatOk, "control: the named CMM begins a colorimetric-only chain");
  if (rv != icCmmStatOk)
    return;
  nSrc = cmm.GetSourceSamples();
  nDst = cmm.GetDestSamples();
  check(nSrc == 3 && nDst == 3, "control: three samples in and out");

  icFloatNumber src[3] = { 0.25f, 0.5f, 0.75f };
  icFloatNumber dst[3] = { -1, -1, -1 };
  check(cmm.Apply(dst, src) == icCmmStatOk, "control: the chain applies");
  check(dst[0] >= 0 && dst[1] >= 0 && dst[2] >= 0, "control: all three outputs are written");
}

} // namespace

int main()
{
  testDestination();
  testSource();
  testColorimetricControl();

  if (g_failures)
    std::fprintf(stderr, "namedcmm-sample-guards: %d failure(s)\n", g_failures);
  else
    std::printf("namedcmm-sample-guards: all checks passed\n");
  return g_failures;
}
