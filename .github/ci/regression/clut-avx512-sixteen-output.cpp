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
#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "IccTagLutAvx512.h"

static bool hostSupportsAvx512()
{
#if defined(_MSC_VER)
  int cpuid[4];
  __cpuidex(cpuid, 0, 0);
  if (cpuid[0] < 7)
    return false;

  __cpuidex(cpuid, 1, 0);
  if (!(cpuid[2] & (1 << 27)) || !(cpuid[2] & (1 << 28)))
    return false;
  if ((_xgetbv(0) & 0xe6) != 0xe6)
    return false;

  __cpuidex(cpuid, 7, 0);
  return (cpuid[1] & (1 << 16)) != 0;
#elif defined(__i386__) || defined(__x86_64__)
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx512f");
#else
  return false;
#endif
}

int main()
{
  if (!hostSupportsAvx512()) {
    std::puts("[SKIP] host does not expose AVX-512F");
    return 77;
  }

  constexpr int outputs = 16;
  icFloatNumber data[8 * outputs];
  icUInt32Number offsets[8];
  const icFloatNumber weights[8] = {
    0.05f, 0.10f, 0.15f, 0.20f, 0.05f, 0.10f, 0.15f, 0.20f
  };
  icFloatNumber actual[outputs] = {};
  icFloatNumber expected[outputs] = {};

  for (int vertex = 0; vertex < 8; vertex++) {
    offsets[vertex] = static_cast<icUInt32Number>(vertex * outputs);
    for (int lane = 0; lane < outputs; lane++) {
      data[vertex * outputs + lane] =
        static_cast<icFloatNumber>(vertex * 0.25f + lane * 0.01f);
    }
  }

  for (int lane = 0; lane < outputs; lane++) {
    expected[lane] = data[offsets[0] + lane] * weights[0];
    for (int vertex = 1; vertex < 8; vertex++)
      expected[lane] += data[offsets[vertex] + lane] * weights[vertex];
  }

  iccCLUTInterp3dAvx512(actual, data, offsets, weights, outputs);
  for (int lane = 0; lane < outputs; lane++) {
    if (std::memcmp(&actual[lane], &expected[lane],
                    sizeof(actual[lane])) != 0) {
      std::fprintf(stderr,
        "[FAIL] AVX-512 16-output lane %d differs: actual=%a expected=%a\n",
        lane, actual[lane], expected[lane]);
      return 1;
    }
  }

  std::puts("[PASS] AVX-512 16-output full-mask parity");
  return 0;
}
