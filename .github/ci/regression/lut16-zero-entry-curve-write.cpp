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
#include "IccIO.h"
#include "IccTagLut.h"

static bool set_lut_curves(CIccTagLut16 &lut, icUInt32Number nEntries)
{
  LPIccCurve *bCurves = lut.NewCurvesB();
  LPIccCurve *aCurves = lut.NewCurvesA();
  if (!bCurves || !aCurves)
    return false;

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(nEntries))
      return false;
    bCurves[i] = curve;
  }

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(nEntries))
      return false;
    aCurves[i] = curve;
  }

  return true;
}

static int run_lut16_write_case(icUInt32Number nEntries, bool bExpectedWrite)
{
  CIccTagLut16 lut;
  lut.Init(3, 3);
  lut.SetColorSpaces(icSigRgbData, icSigXYZData);

  if (!set_lut_curves(lut, nEntries))
    return 2;

  icUInt8Number grid[16] = {2, 2, 2};
  if (!lut.NewCLUT(grid, 2))
    return 3;

  CIccMemIO io;
  if (!io.Alloc(131072, true))
    return 4;

  bool bWrote = lut.Write(&io);
  if (bWrote != bExpectedWrite)
    return bExpectedWrite ? 5 : 6;

  if (bExpectedWrite && io.Tell() == 0)
    return 7;

  return 0;
}

int main()
{
  if (run_lut16_write_case(0, false))
    return 10;
  if (run_lut16_write_case(1, false))
    return 11;
  if (run_lut16_write_case(2, true))
    return 12;
  if (run_lut16_write_case(4096, true))
    return 13;
  if (run_lut16_write_case(4097, false))
    return 14;

  return 0;
}
