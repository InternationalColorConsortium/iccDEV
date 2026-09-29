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

// CTest contract for the PCS a matrix/TRC or monochrome transform accepts
// (#2738).
//
// Both transforms move exactly three PCS values: CIccXformMatrixTRC::Apply()
// writes DstPixel[0..2] (input) or reads SrcPixel[0..2] (output), and
// CIccXformMonochrome::Apply() writes DstPixel[0..2] (input) or reads
// SrcPixel[0] or SrcPixel[1] (output). The CMM sizes the PCS side of the
// caller's buffer from the header PCS, so a header declaring a one-channel PCS
// such as 'gamt' made Apply() run off the end of that buffer. The fuzzer that
// found it (.github/ci/cfl/icc_cmmapply_fuzzer.cpp) allocates the destination
// from CIccCmm::GetDestSamples(), which is the documented contract.
//
// The rule enforced is the specification's, not "three channels":
//   ICC.1:2022 8.3.3, 8.4.3, F.3: "Only the PCSXYZ encoding can be used with
//     matrix/TRC models."
//   ICC.1:2022 F.2: the grayTRC value multiplies "the PCSXYZ or PCSLAB values
//     of the PCS white point".
// So a Lab PCS is refused for matrix/TRC and accepted for monochrome, and a
// three-channel PCS that is neither XYZ nor Lab is refused for both. Those
// cases separate the spec rule from a channel-count check.
//
// Each refusal is asserted as Begin() == icCmmStatBadSpaceLink, the code the
// transform returns, so a refusal by an unrelated check does not count. On an
// unfixed library the input-direction cases return icCmmStatOk, and the
// positive controls keep a fix that refuses everything from passing.
//
// The space checked is the one the transform connects to. Two cases keep an
// XYZ header PCS but reach the transform through CIccXform::Create()'s fallback
// from an unsupported DToB0, which leaves m_bUseSpectralPCS set: the transform
// then connects to a 31-channel spectral PCS and Apply() filled three of them.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagMPE.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "matrixtrc-monochrome-pcs: FAIL  %s\n", label);
    g_failures++;
  }
}

// A two-point 0..1 ramp. It is an identity curve, so Apply() passes device
// values through unchanged and the matrix alone decides the PCS result.
CIccTagCurve *identityCurve()
{
  CIccTagCurve *pCurve = new CIccTagCurve();
  pCurve->SetSize(2);
  (*pCurve)[0] = 0.0f;
  (*pCurve)[1] = 1.0f;
  return pCurve;
}

CIccTagXYZ *xyzColumn(double x, double y, double z)
{
  CIccTagXYZ *pXYZ = new CIccTagXYZ();
  (*pXYZ)[0].X = icDtoF((icFloatNumber)x);
  (*pXYZ)[0].Y = icDtoF((icFloatNumber)y);
  (*pXYZ)[0].Z = icDtoF((icFloatNumber)z);
  return pXYZ;
}

// A v4 RGB display profile with the matrix/TRC tags and no AToB/BToA tags, so
// CIccXform::Create() picks CIccXformMatrixTRC. The columns are the D50-adapted
// sRGB primaries; each row of the matrix sums to the D50 white, so RGB (1,1,1)
// maps to Y = 1.
CIccProfile *matrixTrcProfile(icColorSpaceSignature pcs)
{
  CIccProfile *pProfile = new CIccProfile();
  pProfile->InitHeader();
  pProfile->m_Header.version = icVersionNumberV4;
  pProfile->m_Header.deviceClass = icSigDisplayClass;
  pProfile->m_Header.colorSpace = icSigRgbData;
  pProfile->m_Header.pcs = pcs;

  pProfile->AttachTag(icSigRedMatrixColumnTag, xyzColumn(0.4361, 0.2225, 0.0139));
  pProfile->AttachTag(icSigGreenMatrixColumnTag, xyzColumn(0.3851, 0.7169, 0.0971));
  pProfile->AttachTag(icSigBlueMatrixColumnTag, xyzColumn(0.1431, 0.0606, 0.7141));
  pProfile->AttachTag(icSigRedTRCTag, identityCurve());
  pProfile->AttachTag(icSigGreenTRCTag, identityCurve());
  pProfile->AttachTag(icSigBlueTRCTag, identityCurve());
  return pProfile;
}

// A v4 Gray display profile with only a grayTRC, so CIccXform::Create() picks
// CIccXformMonochrome.
CIccProfile *monochromeProfile(icColorSpaceSignature pcs)
{
  CIccProfile *pProfile = new CIccProfile();
  pProfile->InitHeader();
  pProfile->m_Header.version = icVersionNumberV4;
  pProfile->m_Header.deviceClass = icSigDisplayClass;
  pProfile->m_Header.colorSpace = icSigGrayData;
  pProfile->m_Header.pcs = pcs;

  pProfile->AttachTag(icSigGrayTRCTag, identityCurve());
  return pProfile;
}

// Add one profile and begin. The caller built the CMM the way the fuzzer does:
// no source or destination space given, so the CMM takes both from the profile
// header, and its bFirstInput argument selects device->PCS (true) or
// PCS->device (false). The CMM owns the profile.
icStatusCMM beginOne(CIccCmm &cmm, CIccProfile *pProfile)
{
  icStatusCMM rv = cmm.AddXform(pProfile, icRelativeColorimetric);
  if (rv != icCmmStatOk)
    return rv;
  return cmm.Begin();
}

// A refusal must happen at Begin(). AddXform() has to succeed first, or the
// case never reaches the transform's own Begin() and would pass for the wrong
// reason.
void expectRefused(CIccProfile *pProfile, bool bInput, const char *label,
                   icRenderingIntent nIntent = icRelativeColorimetric)
{
  CIccCmm cmm(icSigUnknownData, icSigUnknownData, bInput);
  icStatusCMM rv = cmm.AddXform(pProfile, nIntent);
  if (rv != icCmmStatOk) {
    std::fprintf(stderr, "matrixtrc-monochrome-pcs: AddXform returned %d\n", (int)rv);
    check(false, label);
    return;
  }
  rv = cmm.Begin();
  if (rv != icCmmStatBadSpaceLink)
    std::fprintf(stderr, "matrixtrc-monochrome-pcs: Begin() returned %d, not icCmmStatBadSpaceLink\n", (int)rv);
  check(rv == icCmmStatBadSpaceLink, label);
}

// Adds a 31-channel reflectance spectral PCS to the header and a DToB0 whose
// only element is unsupported. CIccXform::Create() finds the DToB0, sets
// m_bUseSpectralPCS, then drops the tag as unsupported and falls back to the
// matrix/TRC or monochrome transform with the flag still set, so the transform
// connects to the spectral PCS while Apply() fills three channels. The
// perceptual intent is the one whose DToB0 lookup sets the flag.
CIccProfile *withSpectralFallback(CIccProfile *pProfile, icUInt16Number nDevice)
{
  const icUInt16Number nSteps = 31;
  pProfile->m_Header.version = icVersionNumberV5;
  pProfile->m_Header.spectralPCS =
    (icColorSpaceSignature)(icSigReflectanceSpectralPcsData | nSteps);
  pProfile->m_Header.spectralRange.start = icFtoF16(400.0f);
  pProfile->m_Header.spectralRange.end = icFtoF16(700.0f);
  pProfile->m_Header.spectralRange.steps = nSteps;

  CIccTagMultiProcessElement *pMpe = new CIccTagMultiProcessElement(nDevice, nSteps);
  CIccMpeUnknown *pUnknown = new CIccMpeUnknown();
  pUnknown->SetType((icElemTypeSignature)0x7a7a7a7a);
  pUnknown->SetChannels(nDevice, nSteps);
  pMpe->Attach(pUnknown);
  pProfile->AttachTag(icSigDToB0Tag, pMpe);
  return pProfile;
}

// Apply a pixel through a CMM whose buffers are sized exactly as the CMM
// reports them, which is what the fuzzer does. Returns false if Begin() fails.
bool applySized(CIccProfile *pProfile, bool bInput, icFloatNumber srcValue,
                std::vector<icFloatNumber> &dst)
{
  CIccCmm cmm(icSigUnknownData, icSigUnknownData, bInput);
  if (beginOne(cmm, pProfile) != icCmmStatOk)
    return false;

  std::vector<icFloatNumber> src(cmm.GetSourceSamples(), srcValue);
  dst.assign(cmm.GetDestSamples(), -1.0f);
  return cmm.Apply(dst.data(), src.data()) == icCmmStatOk;
}

void testMatrixTrc()
{
  // #2738: the fuzzer's profile. Input direction, 'gamt' PCS: one destination
  // sample, and Apply() used to write three.
  expectRefused(matrixTrcProfile(icSigGamutData), true,
                "matrix/TRC, input, 'gamt' PCS is refused");

  // Three channels, but not PCSXYZ. Refused by the spec rule; a channel-count
  // check would let it through.
  expectRefused(matrixTrcProfile(icSigLabData), true,
                "matrix/TRC, input, Lab PCS is refused");

  // Output direction. The unfixed library already refused these; they pin
  // that check alongside the new input-direction one.
  expectRefused(matrixTrcProfile(icSigGamutData), false,
                "matrix/TRC, output, 'gamt' PCS is refused");
  expectRefused(matrixTrcProfile(icSigLabData), false,
                "matrix/TRC, output, Lab PCS is refused");

  // XYZ header PCS, but the transform connects to a 31-channel spectral PCS
  // after falling back from an unsupported DToB0.
  expectRefused(withSpectralFallback(matrixTrcProfile(icSigXYZData), 3), true,
                "matrix/TRC, input, spectral fallback is refused", icPerceptual);

  // Positive controls: PCSXYZ works in both directions.
  std::vector<icFloatNumber> dst;
  bool ok = applySized(matrixTrcProfile(icSigXYZData), true, 1.0f, dst);
  check(ok, "matrix/TRC, input, XYZ PCS applies");
  check(dst.size() == 3, "matrix/TRC, input, XYZ PCS has three destination samples");
  // White (1,1,1) maps to Y = 1, stored in the internal XYZ encoding where
  // 1.0 is 32768/65535.
  if (ok && dst.size() == 3)
    check(std::fabs(dst[1] - 32768.0f / 65535.0f) < 0.001f,
          "matrix/TRC, input, XYZ PCS maps white to Y = 1");

  ok = applySized(matrixTrcProfile(icSigXYZData), false, 0.25f, dst);
  check(ok, "matrix/TRC, output, XYZ PCS applies");
  check(dst.size() == 3, "matrix/TRC, output, XYZ PCS has three destination samples");
}

void testMonochrome()
{
  // One-channel PCS in both directions. Input used to write three floats into a
  // one-float destination; output used to read SrcPixel[1] from a one-float
  // source.
  expectRefused(monochromeProfile(icSigGamutData), true,
                "monochrome, input, 'gamt' PCS is refused");
  expectRefused(monochromeProfile(icSigGamutData), false,
                "monochrome, output, 'gamt' PCS is refused");

  // Three channels, neither PCSXYZ nor PCSLAB. Separates the F.2 rule from a
  // channel-count check.
  expectRefused(monochromeProfile(icSigRgbData), true,
                "monochrome, input, RGB PCS is refused");
  expectRefused(monochromeProfile(icSigRgbData), false,
                "monochrome, output, RGB PCS is refused");

  expectRefused(withSpectralFallback(monochromeProfile(icSigXYZData), 1), true,
                "monochrome, input, spectral fallback is refused", icPerceptual);

  // Positive controls: F.2 allows both PCSXYZ and PCSLAB, in both directions.
  std::vector<icFloatNumber> dst;
  bool ok = applySized(monochromeProfile(icSigXYZData), true, 1.0f, dst);
  check(ok, "monochrome, input, XYZ PCS applies");
  check(dst.size() == 3, "monochrome, input, XYZ PCS has three destination samples");
  if (ok && dst.size() == 3)
    check(std::fabs(dst[1] - 32768.0f / 65535.0f) < 0.001f,
          "monochrome, input, XYZ PCS maps device 1.0 to Y = 1");

  ok = applySized(monochromeProfile(icSigLabData), true, 1.0f, dst);
  check(ok, "monochrome, input, Lab PCS applies");
  check(dst.size() == 3, "monochrome, input, Lab PCS has three destination samples");
  // Device 1.0 is the PCS white: L* = 100, stored normalised as 1.0.
  if (ok && dst.size() == 3)
    check(std::fabs(dst[0] - 1.0f) < 0.001f,
          "monochrome, input, Lab PCS maps device 1.0 to L* = 100");

  ok = applySized(monochromeProfile(icSigXYZData), false, 0.25f, dst);
  check(ok, "monochrome, output, XYZ PCS applies");
  check(dst.size() == 1, "monochrome, output, XYZ PCS has one destination sample");

  ok = applySized(monochromeProfile(icSigLabData), false, 0.25f, dst);
  check(ok, "monochrome, output, Lab PCS applies");
  check(dst.size() == 1, "monochrome, output, Lab PCS has one destination sample");
}

} // namespace

int main()
{
  testMatrixTrc();
  testMonochrome();

  if (g_failures)
    std::fprintf(stderr, "matrixtrc-monochrome-pcs: %d failure(s)\n", g_failures);
  else
    std::printf("matrixtrc-monochrome-pcs: all checks passed\n");
  return g_failures;
}
