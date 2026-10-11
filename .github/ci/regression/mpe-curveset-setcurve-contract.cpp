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
    File:       mpe-curveset-setcurve-contract.cpp

    Contains:   CTest helper for CIccMpeCurveSet::SetCurve() bounds and
                ownership contracts (issue #2607).

    SetCurve() used to accept index == InputChannels(), then read and write one
    pointer beyond m_curve.  The public API also deleted a slot's unique curve
    when asked to reinstall that same pointer, leaving the slot dangling.

    This helper deliberately drives the exported library API.  XML and JSON
    reject an extra CurveSet child before calling SetCurve(), and binary reads
    populate the table directly, so no serialized profile honestly reaches the
    invalid setter index.  The separate XML fixture pins that parser boundary.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccMpeBasic.h"

#include <cstdio>

static int g_failures = 0;

static void check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "mpe-curveset-setcurve-contract: PASS  %s\n", label);
    return;
  }

  std::fprintf(stderr, "mpe-curveset-setcurve-contract: FAIL  %s\n", label);
  g_failures++;
}

int main()
{
  CIccMpeCurveSet curves(3);
  CIccSegmentedCurve *curve0 = new CIccSegmentedCurve;
  CIccSegmentedCurve *curve1 = new CIccSegmentedCurve;
  CIccSegmentedCurve *curve2 = new CIccSegmentedCurve;

  check(curves.SetCurve(0, curve0), "index zero is accepted");
  check(curves.SetCurve(1, curve1), "interior index is accepted");
  check(curves.SetCurve(2, curve2), "last valid index is accepted");
  check(curves.SetCurve(0, curve0), "reinstalling the owned pointer is a no-op");

  CIccSegmentedCurve *below = new CIccSegmentedCurve;
  check(!curves.SetCurve(-1, below), "negative index is rejected");
  delete below;

  CIccSegmentedCurve *atEnd = new CIccSegmentedCurve;
  check(!curves.SetCurve(3, atEnd), "index equal to channel count is rejected");
  delete atEnd;

  CIccMpeCurveSet empty;
  CIccSegmentedCurve *noStorage = new CIccSegmentedCurve;
  check(!empty.SetCurve(0, noStorage), "empty curve set rejects index zero");
  delete noStorage;

  return g_failures ? 1 : 0;
}
