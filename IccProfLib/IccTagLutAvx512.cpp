/** @file
    File:       IccTagLutAvx512.cpp

    Contains:   AVX-512 implementation of 3D CLUT interpolation
*/

/*
 * Copyright (c) 2026 International Color Consortium.
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
 * 3. In the absence of prior written permission, the names "ICC" and
 *    "International Color Consortium" must not be used to imply that the
 *    International Color Consortium endorses or promotes products derived
 *    from this software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING
 * MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <immintrin.h>

#include "IccTagLutAvx512.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

void iccCLUTInterp3dAvx512(icFloatNumber *destPixel, const icFloatNumber *data,
                           const icUInt32Number offsets[8], const icFloatNumber weights[8],
                           int nOutput)
{
  const __mmask16 outputMask = (__mmask16)((1u << nOutput) - 1u);
  const __m512 weight0 = _mm512_set1_ps(weights[0]);
  const __m512 weight1 = _mm512_set1_ps(weights[1]);
  const __m512 weight2 = _mm512_set1_ps(weights[2]);
  const __m512 weight3 = _mm512_set1_ps(weights[3]);
  const __m512 weight4 = _mm512_set1_ps(weights[4]);
  const __m512 weight5 = _mm512_set1_ps(weights[5]);
  const __m512 weight6 = _mm512_set1_ps(weights[6]);
  const __m512 weight7 = _mm512_set1_ps(weights[7]);

  __m512 value = _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[0]), weight0);
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[1]), weight1));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[2]), weight2));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[3]), weight3));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[4]), weight4));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[5]), weight5));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[6]), weight6));
  value = _mm512_add_ps(value, _mm512_mul_ps(_mm512_maskz_loadu_ps(outputMask, data + offsets[7]), weight7));
  _mm512_mask_storeu_ps(destPixel, outputMask, value);
}

#ifdef USEICCDEVNAMESPACE
}
#endif
