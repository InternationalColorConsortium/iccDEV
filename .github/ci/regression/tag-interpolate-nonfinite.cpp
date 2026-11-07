/** @file
    File:       tag-interpolate-nonfinite.cpp

    Contains:   Regression coverage for #2975: a NaN position given to an
                array tag's Interpolate().

    CIccTagFixedNum, CIccTagNum and CIccTagSparseMatrixArray clamped the
    position to [0,1] with two comparisons, both false for NaN, and then cast
    it to an unsigned index: undefined behaviour (UBSan float-cast-overflow).
    CIccTagFloatNum already treated NaN as 0. Each type must now give the same
    result for NaN as for position 0, and for +/-infinity as for the ends.
    CIccTagFloatNum is the control. A UBSan build of the old code aborts.

    Exit codes:
      0 - every check passes
      1 - a check fails
    Copyright:  See ICC Software License
*/

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


#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <limits>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void Check(bool ok, const char* type, const char* what)
{
  std::fprintf(ok ? stdout : stderr, "tag-interpolate-nonfinite: %s  %s: %s\n",
               ok ? "PASS" : "FAIL", type, what);
  if (!ok)
    ++g_failures;
}

// Interpolates at pos and at ref and requires the same return and, when they
// succeed, the same first nOutBytes of output. nOutBytes is not always
// nVectorSize floats: a sparse-matrix array takes nVectorSize in BYTES.
template <class TagT>
void Same(const TagT& tag, icFloatNumber pos, icFloatNumber ref,
          icUInt32Number nVectorSize, size_t nOutBytes, const char* type,
          const char* what, bool requireSuccess)
{
  icFloatNumber got[64], want[64];
  std::memset(got, 0, sizeof(got));
  std::memset(want, 0, sizeof(want));
  if (nOutBytes > sizeof(got)) {
    Check(false, type, "output fits the test buffer");
    return;
  }
  const bool rGot = tag.Interpolate(got, pos, nVectorSize);
  const bool rWant = tag.Interpolate(want, ref, nVectorSize);
  const bool ok = rGot == rWant && (!rGot || !std::memcmp(got, want, nOutBytes)) &&
                  (rWant || !requireSuccess);
  Check(ok, type, what);
  if (!ok)
    std::fprintf(stderr, "  returns %d/%d, first values %g/%g\n",
                 (int)rGot, (int)rWant, (double)got[0], (double)want[0]);
}

template <class TagT>
void Positions(const TagT& tag, icUInt32Number nVectorSize, size_t nOutBytes,
               const char* type, bool requireSuccess = true)
{
  const icFloatNumber nan = std::numeric_limits<icFloatNumber>::quiet_NaN();
  const icFloatNumber inf = std::numeric_limits<icFloatNumber>::infinity();
  Same(tag, nan, 0, nVectorSize, nOutBytes, type, "NaN gives the position-0 result", requireSuccess);
  Same(tag, inf, 1, nVectorSize, nOutBytes, type, "+inf gives the position-1 result", requireSuccess);
  Same(tag, -inf, 0, nVectorSize, nOutBytes, type, "-inf gives the position-0 result", requireSuccess);
}

} // namespace

int main()
{
  CIccTagS15Fixed16 fixed(2);
  fixed[0] = icDtoF(0.25);
  fixed[1] = icDtoF(0.75);
  Positions(fixed, 1, sizeof(icFloatNumber), "CIccTagFixedNum (s15Fixed16)");

  CIccTagUInt8 integer(2);
  integer[0] = 64;
  integer[1] = 191;
  Positions(integer, 1, sizeof(icFloatNumber), "CIccTagNum (uInt8)");

  CIccTagFloat32 control(2);
  control[0] = 0.25f;
  control[1] = 0.75f;
  Positions(control, 1, sizeof(icFloatNumber), "CIccTagFloatNum (float32), control");

  // Empty matrices: NaN has to behave exactly as position 0, but the outputs
  // agree for any position, so the undefined cast itself is caught by UBSan.
  CIccTagSparseMatrixArray sparse(2, 1);
  Positions(sparse, sparse.GetBytesPerMatrix(), sparse.GetBytesPerMatrix(),
            "CIccTagSparseMatrixArray");

  return g_failures ? 1 : 0;
}
