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
class CountedBuffer {
public:
  void badArray(unsigned int nIndex)
  {
    if (nIndex > m_nSize) // $ Alert
      return;
    m_values[nIndex] = 0;
  }

  void safeArray(unsigned int nIndex)
  {
    if (nIndex >= m_nSize)
      return;
    m_values[nIndex] = 0;
  }

  void adjustedCountControl(unsigned int nIndex)
  {
    if (!m_nSize || nIndex > m_nSize - 1)
      return;
    m_values[nIndex] = 0;
  }

  void laterRejectingGuardControl(unsigned int nIndex)
  {
    m_seenTooLarge = nIndex > m_nSize;
    if (nIndex >= m_nSize)
      return;
    m_values[nIndex] = 0;
  }

  unsigned char *badPointer(int nIndex)
  {
    if (nIndex < 0 || nIndex > (int)m_nSize) // $ Alert
      return 0;
    return m_raw + nIndex * 4;
  }

private:
  unsigned int m_nSize;
  int *m_values;
  unsigned char *m_raw;
  bool m_seenTooLarge;
};
