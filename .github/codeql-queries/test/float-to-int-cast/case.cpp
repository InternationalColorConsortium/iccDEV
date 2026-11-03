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
namespace std {
bool isfinite(float value);
bool isnan(float value);
double floor(double value);
}
double round(double value);

unsigned char ClipU8(float input, bool safe)
{
  unsigned char bad = (unsigned char)input; // $ Alert
  if (!safe)
    return bad;
  if (std::isnan(input))
    return 0;
  if (input < 0.0f)
    return 0;
  if (input > 255.0f)
    return 255;
  return (unsigned char)input;
}

int tintPercent(float valueTemp, bool safe)
{
  int bad = (int)round(valueTemp * 100.0f); // $ Alert
  if (!safe)
    return bad;
  if (!std::isfinite(valueTemp))
    valueTemp = 0.0f;
  if (valueTemp < 0.0f)
    valueTemp = 0.0f;
  if (valueTemp > 1.0f)
    valueTemp = 1.0f;
  return (int)round(valueTemp * 100.0f);
}

int resampleCore(double t, bool safe)
{
  int bad = (int)std::floor(t); // $ Alert
  if (!safe)
    return bad;
  if (!std::isfinite((float)t))
    return 0;
  if (t <= 0.0)
    return 0;
  if (t >= 31.0)
    return 30;
  return (int)std::floor(t);
}

class UnsafeInterpolation {
public:
  unsigned int Interpolate(float pos)
  {
    if (pos < 0.0f)
      pos = 0.0f;
    if (pos > 1.0f)
      pos = 1.0f;
    float fpos = 7.0f * pos;
    return (unsigned int)fpos; // $ Alert
  }
};

class SafeInterpolation {
public:
  unsigned int Interpolate(float pos)
  {
    if (!std::isfinite(pos))
      return 0;
    if (pos < 0.0f)
      pos = 0.0f;
    if (pos > 1.0f)
      pos = 1.0f;
    float fpos = 7.0f * pos;
    return (unsigned int)fpos;
  }
};
