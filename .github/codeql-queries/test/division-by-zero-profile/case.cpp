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
float fabs(float value);
}

float pushXyzLumToXyz(float XYZLum[3], bool safe)
{
  float bad = 1.0f / XYZLum[1]; // $ Alert
  if (!safe)
    return bad;
  if (!std::isfinite(XYZLum[1]) || XYZLum[1] <= 0.0f)
    return 1.0f;
  return 1.0f / XYZLum[1];
}

float icYxy2XYZVector(float xy[2], bool safe)
{
  float bad = 2.0f / xy[1]; // $ Alert
  if (!safe)
    return bad;
  if (!(std::isfinite(xy[1]) && xy[1] > 0.0f))
    return 0.0f;
  return 2.0f / xy[1];
}

float calcQuadraticVertex(float denom, float t, bool safe)
{
  float badDenom = 1.0f / denom; // $ Alert
  float badT = 1.0f / (2.0f * t); // $ Alert
  if (!safe)
    return badDenom + badT;
  if (std::fabs(denom) <= 0.0f)
    return 0.0f;
  float coefficient = 1.0f / denom;
  if (t == 0.0f || !std::isfinite(t))
    return 0.0f;
  return coefficient / (2.0f * t);
}

float calcDstBlackPoint(float MaxL, float MinL, bool safe)
{
  float range = MaxL - MinL;
  float bad = 1.0f / range; // $ Alert
  if (!safe)
    return bad;
  if (std::fabs(range) < 1e-8f)
    return 0.0f;
  return 1.0f / range;
}

float CalcCoefficients(float m_La, bool safe)
{
  float bad = 1.0f / (5.0f * m_La + 1.0f); // $ Alert
  if (!safe)
    return bad;
  if (!std::isfinite(m_La) || m_La < 0.0f)
    return 0.0f;
  return 1.0f / (5.0f * m_La + 1.0f);
}
