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

// Regression: CIccTagSpectralViewingConditions::isStandardPcc() joined its two
// conditions with || instead of &&.
//
// Standard profile connection conditions are the CIE 1931 two degree observer
// AND illuminant D50; IIccProfileConnectionConditions::isStandardPcc() in
// IccPcc.cpp has always said so. The tag-level copy accepted either one, and
// CIccProfile::getNormIlluminantXYZ() answers icD50XYZ when it returns true.
// So a profile naming the 1931 observer under D65, D93 or A, or naming D50
// under the 1964 or a custom observer, had its Lab PCS referred to the
// standard D50 white instead of its own illuminant XYZ.
//
// Each non-standard case below is paired with the standard one, so the test
// cannot pass by getNormIlluminantXYZ() no longer answering icD50XYZ at all.

#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static bool is_near(icFloatNumber a, icFloatNumber b)
{
  return std::fabs(a - b) < 0.00001f;
}

static bool is_xyz(const icFloatNumber *xyz, icFloatNumber x, icFloatNumber y, icFloatNumber z)
{
  return is_near(xyz[0], x) && is_near(xyz[1], y) && is_near(xyz[2], z);
}

// The illuminant XYZ is deliberately not D50 in every case, so which branch
// getNormIlluminantXYZ() took is visible in what it returns.
static const icFloatNumber kIllumXYZ[3] = {190.0f, 200.0f, 150.0f};

static int check(const char *name, icStandardObserver observer, icIlluminant illuminant, bool expectStandard)
{
  CIccProfile profile;
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV5;

  icSpectralRange emptyRange;
  std::memset(&emptyRange, 0, sizeof(emptyRange));

  CIccTagSpectralViewingConditions *view = new CIccTagSpectralViewingConditions();

  if (!view->setIlluminant(illuminant, emptyRange, nullptr) ||
      !view->setObserver(observer, emptyRange, nullptr)) {
    delete view;
    std::printf("FAIL %s: could not set viewing conditions\n", name);
    return 1;
  }

  view->m_illuminantXYZ.X = kIllumXYZ[0];
  view->m_illuminantXYZ.Y = kIllumXYZ[1];
  view->m_illuminantXYZ.Z = kIllumXYZ[2];

  if (!profile.AttachTag(icSigSpectralViewingConditionsTag, view)) {
    delete view;
    std::printf("FAIL %s: could not attach viewing conditions\n", name);
    return 1;
  }

  int rv = 0;

  if (view->isStandardPcc() != expectStandard) {
    std::printf("FAIL %s: tag isStandardPcc() is %d, expected %d\n", name, view->isStandardPcc(), expectStandard);
    rv = 1;
  }

  // The interface-level answer is the one the CMM uses to decide whether the
  // customToStandardPcc / standardToCustomPcc transforms run; the two must agree.
  if (profile.isStandardPcc() != expectStandard) {
    std::printf("FAIL %s: profile isStandardPcc() is %d, expected %d\n", name, profile.isStandardPcc(), expectStandard);
    rv = 1;
  }

  icFloatNumber xyz[3] = {-1.0f, -1.0f, -1.0f};
  profile.getNormIlluminantXYZ(xyz);

  bool ok = expectStandard ?
    is_xyz(xyz, icD50XYZ[0], icD50XYZ[1], icD50XYZ[2]) :
    is_xyz(xyz, kIllumXYZ[0] / kIllumXYZ[1], 1.0f, kIllumXYZ[2] / kIllumXYZ[1]);

  if (!ok) {
    std::printf("FAIL %s: normalized illuminant is %.6f %.6f %.6f\n", name, xyz[0], xyz[1], xyz[2]);
    rv = 1;
  }

  if (!rv)
    std::printf("ok   %s\n", name);

  return rv;
}

int main()
{
  int rv = 0;

  rv |= check("1931 observer, D50", icStdObs1931TwoDegrees, icIlluminantD50, true);

  rv |= check("1931 observer, D65", icStdObs1931TwoDegrees, icIlluminantD65, false);
  rv |= check("1931 observer, D93", icStdObs1931TwoDegrees, icIlluminantD93, false);
  rv |= check("1931 observer, A", icStdObs1931TwoDegrees, icIlluminantA, false);
  rv |= check("1964 observer, D50", icStdObs1964TenDegrees, icIlluminantD50, false);
  rv |= check("custom observer, D50", icStdObsCustom, icIlluminantD50, false);
  rv |= check("1964 observer, D65", icStdObs1964TenDegrees, icIlluminantD65, false);

  return rv;
}
