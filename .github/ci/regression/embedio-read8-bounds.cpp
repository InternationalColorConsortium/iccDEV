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
#include "IccIO.h"

#include <cstdio>

int main()
{
  icUInt8Number bytes[16];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = (icUInt8Number)('A' + i);
  }

  CIccMemIO backing;
  if (!backing.Attach(bytes, sizeof(bytes))) {
    std::printf("backing attach failed\n");
    return 1;
  }

  if (backing.Seek(8, icSeekSet) != 8) {
    std::printf("initial seek failed\n");
    return 1;
  }

  CIccEmbedIO embed;
  if (!embed.Attach(&backing, 4)) {
    std::printf("embed attach failed\n");
    return 1;
  }

  icUInt8Number out[16] = {};

  if (backing.Seek(0, icSeekSet) != 0) {
    std::printf("backing rewind failed\n");
    return 1;
  }

  size_t read = embed.Read8(out, sizeof(out));
  if (read != 0) {
    std::printf("Read8 read before embedded window: read=%zu\n", read);
    return 1;
  }

  if (embed.Seek(2, icSeekSet) != 2) {
    std::printf("embed seek failed\n");
    return 1;
  }

  read = embed.Read8(out, sizeof(out));
  if (read != 2 || out[0] != 'K' || out[1] != 'L') {
    std::printf("Read8 did not clamp to embedded window: read=%zu first=%u second=%u\n",
                read, (unsigned)out[0], (unsigned)out[1]);
    return 1;
  }

  std::printf("CIccEmbedIO::Read8 bounds OK\n");
  return 0;
}
