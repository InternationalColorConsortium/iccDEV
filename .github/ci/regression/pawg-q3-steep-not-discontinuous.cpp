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

// Regression: iccPawgReport Q3 counted a smooth but steep transform as
// discontinuous.
//
// Q3 samples 65 points along the neutral diagonal and each device axis and
// counted every step over 6.0 CIEDE2000 between neighbours as a
// discontinuity.  A steep stretch of a smooth transform makes steps that large
// as well: L* rises as the cube root of Y, so a linear RGB to XYZ transform
// steps 13 or more at the black end; CIEDE2000 weighs a steady change of a* or
// b* most near neutral; and an extended-range PCS can span more than 64 * 6
// units of L*.  A discontinuity is now one step, or two adjacent steps, with
// the largest over 6.0 and a mean more than four times the steps either side.
//
// Step sequences, given to count_smoothness_discontinuities directly:
//   cube-root       13.68 7.04 5.42 4.66, the near-black steps of a linear
//                   RGB to XYZ transform                        -> 0
//   cube-root-pure  25 times the steps of a cube root from zero,
//                   the first 3.85 times the second             -> 0
//   neutral         5.2 6.4 6.7 5.2, an axis crossing neutral   -> 0
//   clip            7 7 7 0 0, a steep ramp that clips          -> 0
//   small           0.1 3.0 0.1, a jump under 6.0               -> 0
//   bump            3 7 7 3, steeper for two steps              -> 0
//   jump            0.7 90.2 0.7                                -> 1
//   jump-5x         1.5 7.5 1.5                                 -> 1
//   endpoint-jump   124.9 0.8 0.5                               -> 1
//   split           0.4 28.0 12.1 0.2, a jump on a sample       -> 1
//   split-uneven    0.5 7.0 40.0 0.5                            -> 1
//   split-reversed  0.4 7.0 2.0 0.4                             -> 1
//   split-first     25.0 20.0 0.5 0.4                           -> 1
//   split-last      0.4 0.5 20.0 25.0                           -> 1
//   two-jumps       0.4 90 0.4 90 0.4                           -> 2
// cube-root, cube-root-pure, neutral, clip and bump fail on an unfixed build,
// which counts every step over 6.0, as do split, split-uneven, split-first and
// split-last, whose halves it counts apart.  cube-root-pure and jump-5x bound
// the ratio at 3.85 and 5.
//
// Transforms built in memory, one for each of the three ways Q3 measures:
// matrix/TRC, a classic lut16, and the CMM, which the lut16 profile also
// reaches:
//   linear      the device values map linearly to XYZ    -> no discontinuity
//   step        the same with a jump of the curves between table entries
//               127 and 128                               -> one on each of
//               the four sampled paths, the neutral diagonal and three axes
// linear fails on an unfixed build.  step is the control: a jump in a
// 256-entry curve rises over one table interval, which here holds the sample
// at 0.5, so the jump is split between two steps; an unfixed build counts
// both, eight in all.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include "IccQualityMetrics.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace iccquality;

static int g_failures = 0;

static void check(bool cond, const char *szCase, const char *szWhat, const SmoothnessMetrics &m)
{
  std::printf("%s: %-24s %-28s maxStep=%.4f discontinuities=%d\n", cond ? "ok  " : "FAIL",
              szCase, szWhat, m.maxStepDe00, m.discontinuities);
  if (!cond)
    ++g_failures;
}

// 256 entries: the identity, or with nJump > 0 the identity scaled by 0.25
// up to entry nJump - 1 and by 0.5 plus 0.5 from entry nJump on.
static CIccTagCurve *makeCurve(int nJump)
{
  CIccTagCurve *pCurve = new CIccTagCurve();
  pCurve->SetSize(256);
  for (int i = 0; i < 256; ++i) {
    const double x = i / 255.0;
    double y = x;
    if (nJump > 0)
      y = i < nJump ? 0.25 * x : 0.5 + 0.5 * x;
    (*pCurve)[static_cast<icUInt32Number>(i)] = static_cast<icFloatNumber>(y);
  }
  return pCurve;
}

static CIccTagXYZ *makeColumn(double x, double y, double z)
{
  CIccTagXYZ *pXYZ = new CIccTagXYZ();
  pXYZ->SetSize(1);
  (*pXYZ)[0].X = icDtoF(static_cast<icFloatNumber>(x));
  (*pXYZ)[0].Y = icDtoF(static_cast<icFloatNumber>(y));
  (*pXYZ)[0].Z = icDtoF(static_cast<icFloatNumber>(z));
  return pXYZ;
}

static void initRgbXyz(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.deviceClass = icSigDisplayClass;
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigXYZData;
  profile.m_Header.version = icVersionNumberV4;
}

// Linear TRCs, or stepped ones, and the D50 sRGB matrix columns.
static void makeMatrixTrc(CIccProfile &profile, int nJump)
{
  initRgbXyz(profile);
  profile.AttachTag(icSigRedTRCTag, makeCurve(nJump));
  profile.AttachTag(icSigGreenTRCTag, makeCurve(nJump));
  profile.AttachTag(icSigBlueTRCTag, makeCurve(nJump));
  profile.AttachTag(icSigRedMatrixColumnTag, makeColumn(0.4361, 0.2225, 0.0139));
  profile.AttachTag(icSigGreenMatrixColumnTag, makeColumn(0.3851, 0.7169, 0.0971));
  profile.AttachTag(icSigBlueMatrixColumnTag, makeColumn(0.1431, 0.0606, 0.7141));
}

// A lut16 AToB0Tag whose 2x2x2 CLUT maps each corner to itself, so the PCS XYZ
// is the device value, as in a linear identity transform; the input curves
// carry the step.
static void makeLut16(CIccProfile &profile, int nJump)
{
  initRgbXyz(profile);

  CIccTagLut16 *pLut = new CIccTagLut16();
  pLut->Init(3, 3);
  pLut->SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve *pCurvesA = pLut->NewCurvesA();
  LPIccCurve *pCurvesB = pLut->NewCurvesB();
  for (int i = 0; i < 3; ++i) {
    pCurvesA[i] = makeCurve(nJump);
    pCurvesB[i] = makeCurve(0);
  }

  CIccCLUT *pClut = pLut->NewCLUT(2, 2);
  for (int r = 0; r < 2; ++r) {
    for (int g = 0; g < 2; ++g) {
      for (int b = 0; b < 2; ++b) {
        icFloatNumber *pNode = pClut->GetData(r * pClut->GetDimSize(0) +
                                              g * pClut->GetDimSize(1) +
                                              b * pClut->GetDimSize(2));
        pNode[0] = static_cast<icFloatNumber>(r);
        pNode[1] = static_cast<icFloatNumber>(g);
        pNode[2] = static_cast<icFloatNumber>(b);
      }
    }
  }

  profile.AttachTag(icSigAToB0Tag, pLut);
}

static void checkSequence(const char *szCase, std::vector<double> steps, int nExpected)
{
  const int n = count_smoothness_discontinuities(steps);
  const bool cond = n == nExpected;
  std::printf("%s: %-24s %-28s discontinuities=%d\n", cond ? "ok  " : "FAIL", szCase,
              "step sequence", n);
  if (!cond)
    ++g_failures;
}

int main()
{
  checkSequence("cube-root", {13.68, 7.04, 5.42, 4.66}, 0);
  checkSequence("cube-root-pure", {25.0, 6.4980, 4.5582, 3.6288}, 0);
  checkSequence("neutral", {5.2, 6.4, 6.7, 5.2}, 0);
  checkSequence("clip", {7.0, 7.0, 7.0, 0.0, 0.0}, 0);
  checkSequence("small", {0.1, 3.0, 0.1}, 0);
  checkSequence("bump", {3.0, 7.0, 7.0, 3.0}, 0);
  checkSequence("jump", {0.7, 90.2, 0.7}, 1);
  checkSequence("jump-5x", {1.5, 7.5, 1.5}, 1);
  checkSequence("endpoint-jump", {124.9, 0.8, 0.5}, 1);
  checkSequence("split", {0.4, 28.0, 12.1, 0.2}, 1);
  checkSequence("split-uneven", {0.5, 7.0, 40.0, 0.5}, 1);
  checkSequence("split-reversed", {0.4, 7.0, 2.0, 0.4}, 1);
  checkSequence("split-first", {25.0, 20.0, 0.5, 0.4}, 1);
  checkSequence("split-last", {0.4, 0.5, 20.0, 25.0}, 1);
  checkSequence("two-jumps", {0.4, 90.0, 0.4, 90.0, 0.4}, 2);

  const struct {
    const char *szCase;
    int nJump;
  } cases[] = {
    {"linear", 0},
    {"step", 128},
  };

  for (const auto &c : cases) {
    const char *szCase = c.szCase;
    const int nExpected = c.nJump > 0 ? 4 : 0;

    std::string reason;

    CIccProfile matrixTrc;
    makeMatrixTrc(matrixTrc, c.nJump);
    SmoothnessMetrics m;
    const bool bMatrix = measure_forward_smoothness_matrix_trc(&matrixTrc, m, reason);
    check(bMatrix && m.model == "matrix/TRC multi-axis" &&
            m.discontinuities == nExpected,
          szCase, "matrix/TRC", m);

    CIccProfile lut16;
    makeLut16(lut16, c.nJump);
    m = SmoothnessMetrics();
    const bool bLut = measure_forward_smoothness_classic_lut(&lut16, m, reason);
    check(bLut && m.discontinuities == nExpected,
          szCase, "classic lut16", m);

    m = SmoothnessMetrics();
    const bool bCmm = measure_cmm_forward_smoothness(&lut16, m, reason);
    check(bCmm && m.discontinuities == nExpected,
          szCase, "CMM (lut16 AToB0Tag)", m);
  }

  if (g_failures) {
    std::printf("\n%d case(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall cases passed\n");
  return 0;
}
