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
void consume(unsigned int value);

class BeginGuarded {
public:
  bool Begin()
  {
    if (m_nNodes > 65536)
      return false;
    return true;
  }

  void Apply()
  {
    for (unsigned int i = 0; i < m_nNodes; i++)
      consume(i);
  }

private:
  unsigned int m_nNodes;
};

class Unguarded {
public:
  void Apply()
  {
    for (unsigned int i = 0; i < m_nCount; i++)
      consume(i);
  }

private:
  unsigned int m_nCount;
};

class CIccApplyThreadedCmmPool {
public:
  void Apply()
  {
    for (unsigned int i = 0; i < m_jobCount; i++)
      consume(i);
  }

private:
  unsigned int m_jobCount;
};

class CIccSampledCalculatorCurve {
public:
  void Begin()
  {
    for (unsigned int i = 0; i < m_nCount; i++)
      consume(i);
  }

private:
  unsigned int m_nCount;
};
