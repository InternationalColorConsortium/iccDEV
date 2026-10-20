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
class CIccCmm {
public:
  int Apply(float *dst, const float *src);
  int GetDestSamples() const;
};

class BlackPointEstimator {
public:
  void bad()
  {
    float pixel[16];
    float pcs[3] = {};
    pixelXfm(pixel, pcs); // $ Alert
  }

  void fixedBufferControl()
  {
    float pixel[16];
    clear(pixel);
  }

  void guardedFixedBufferControl()
  {
    float pixel[16];
    float pcs[3] = {};
    guardedPixelXfm(pixel, pcs);
  }

  void mismatchedGuardControl()
  {
    float pixel[16];
    float pcs[3] = {};
    mismatchedGuardPixelXfm(pixel, pcs); // $ Alert
  }

  void differentTransformGuardControl()
  {
    float pixel[16];
    float pcs[3] = {};
    differentTransformGuardPixelXfm(pixel, pcs); // $ Alert
  }

  void arithmeticGuardControl()
  {
    float pixel[16];
    float pcs[3] = {};
    arithmeticGuardPixelXfm(pixel, pcs); // $ Alert
  }

  void dynamicBufferControl(float *pixel)
  {
    float pcs[3] = {};
    pixelXfm(pixel, pcs);
  }

private:
  bool pixelXfm(float *dst, const float *src)
  {
    CIccCmm cmm;
    return cmm.Apply(dst, src) == 0;
  }

  bool guardedPixelXfm(float *dst, const float *src)
  {
    CIccCmm cmm;
    if (cmm.GetDestSamples() > 16)
      return false;
    return cmm.Apply(dst, src) == 0;
  }

  bool arithmeticGuardPixelXfm(float *dst, const float *src)
  {
    CIccCmm cmm;
    if (cmm.GetDestSamples() - 1 > 16)
      return false;
    return cmm.Apply(dst, src) == 0;
  }

  bool mismatchedGuardPixelXfm(float *dst, const float *src)
  {
    CIccCmm cmm;
    if (cmm.GetDestSamples() > 17)
      return false;
    return cmm.Apply(dst, src) == 0;
  }

  bool differentTransformGuardPixelXfm(float *dst, const float *src)
  {
    CIccCmm guardedCmm;
    if (guardedCmm.GetDestSamples() > 16)
      return false;
    CIccCmm appliedCmm;
    return appliedCmm.Apply(dst, src) == 0;
  }

  void clear(float *pixel)
  {
    pixel[0] = 0.0f;
  }
};
