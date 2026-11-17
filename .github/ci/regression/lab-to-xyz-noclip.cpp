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

// IccProfLibConf.h defines REFICCMAX_NOCLIPLABTOXYZ so that the Lab to XYZ
// inverse companding function icICubeth() follows the CIE linear segment all
// the way down instead of clamping to zero. The define was renamed from
// SAMPLEICC_NOCLIPLABTOXYZ in 2016, but icICubeth() kept testing the old name,
// so the clamp was silently compiled back in. With the clamp, any Lab whose
// f(X), f(Y) or f(Z) falls below 16/116 (for example L*=0 with b*>0) produced
// a zero XYZ component, and XYZ -> Lab could not recover the original value.

#include "IccProfLibConf.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>

static bool is_near(icFloatNumber a, icFloatNumber b, icFloatNumber tol)
{
  return std::fabs(a - b) < tol;
}

int main()
{
#ifndef REFICCMAX_NOCLIPLABTOXYZ
  // A build that opts into clipping has nothing for this test to check.
  std::printf("REFICCMAX_NOCLIPLABTOXYZ not defined; skipping\n");
  return 77;
#else
  // Below the linear-segment knee the inverse must stay linear (and negative).
  const icFloatNumber expected = (icFloatNumber)(-(16.0 / 116.0) / 7.787037037037037);
  icFloatNumber v = icICubeth(0.0f);
  if (!(v < 0.0f) || !is_near(v, expected, 1e-6f)) {
    std::printf("icICubeth(0) = %g, expected %g\n", (double)v, (double)expected);
    return 1;
  }

  // L*=0, b*=+50 puts f(Z) below 16/116, so Z must come out negative.
  icFloatNumber Lab[3] = {0.0f, 0.0f, 50.0f};
  icFloatNumber XYZ[3];
  icLabtoXYZ(XYZ, Lab);
  if (!(XYZ[2] < 0.0f)) {
    std::printf("icLabtoXYZ(0,0,50) Z = %g, expected negative\n", (double)XYZ[2]);
    return 2;
  }

  // Without the clamp the conversion round-trips.
  icFloatNumber Lab2[3];
  icXYZtoLab(Lab2, XYZ);
  for (int i = 0; i < 3; i++) {
    if (!is_near(Lab2[i], Lab[i], 1e-3f)) {
      std::printf("round trip Lab[%d] = %g, expected %g\n", i, (double)Lab2[i], (double)Lab[i]);
      return 3;
    }
  }

  return 0;
#endif
}
