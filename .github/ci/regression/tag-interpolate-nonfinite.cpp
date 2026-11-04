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
  Alerts #1086-#1088 were dismissed because Interpolate() clamps pos to [0,1].
  Those comparisons do not clamp NaN: both are false, fpos remains NaN, and the
  following float-to-unsigned conversion is undefined behavior. Each public tag
  implementation must reject a non-finite interpolation position before the cast.
*/

#include "IccTagBasic.h"

#include <cstdio>
#include <limits>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void checkRejected(bool accepted, const char *name)
{
  if (accepted) {
    std::fprintf(stderr, "[tag-interpolate-nonfinite] FAIL: %s accepted NaN\n", name);
    ++g_failures;
  }
}

} // namespace

int main()
{
  const icFloatNumber nan = std::numeric_limits<icFloatNumber>::quiet_NaN();
  icFloatNumber output[4] = {};

  CIccTagSparseMatrixArray sparse(2, 1);
  checkRejected(sparse.Interpolate(output, nan, sparse.GetBytesPerMatrix()),
                "CIccTagSparseMatrixArray::Interpolate");

  CIccTagS15Fixed16 fixed(2);
  checkRejected(fixed.Interpolate(output, nan),
                "CIccTagFixedNum::Interpolate");

  CIccTagUInt8 integer(2);
  checkRejected(integer.Interpolate(output, nan),
                "CIccTagNum::Interpolate");

  if (g_failures)
    return 1;

  std::puts("tag interpolation rejected all non-finite positions");
  return 0;
}
