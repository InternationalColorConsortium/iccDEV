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

#include "IccArrayBasic.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

class NamedColorProbe : public CIccArrayNamedColor
{
public:
  icUInt32Number PcsBits() const
  {
    static_assert(sizeof(m_csPcs) == sizeof(icUInt32Number), "PCS signature width");
    icUInt32Number bits;
    std::memcpy(&bits, &m_csPcs, sizeof(bits));
    return bits;
  }
};

int main()
{
  // Fixed pre-construction bytes prove the constructor establishes its own
  // default instead of inheriting an arbitrary value from the allocation.
  alignas(NamedColorProbe) unsigned char storage[sizeof(NamedColorProbe)];
  std::memset(storage, 0xa5, sizeof(storage));
  NamedColorProbe *array = new (storage) NamedColorProbe;
  icUInt32Number pcsBits = array->PcsBits();
  std::unique_ptr<IIccArray> copy(array->NewCopy(NULL));
  array->~NamedColorProbe();

  if (pcsBits != icSigUnknownData || !copy) {
    std::fprintf(stderr, "Named-color array PCS default is 0x%08x, expected 0x%08x\n",
                 (unsigned)pcsBits, (unsigned)icSigUnknownData);
    return 1;
  }
  return 0;
}
