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
  const char scratch_file[] = "fileio-seek-tell-scratch.bin";

  CIccFileIO io;
  if (!io.Open(scratch_file, "w+b")) {
    std::printf("open failed: %s\n", scratch_file);
    return 1;
  }

  const char bytes[] = "0123456789abcdef";
  const size_t byte_count = sizeof(bytes) - 1;
  if (io.Write8((void *)bytes, byte_count) != byte_count) {
    std::printf("write failed\n");
    return 1;
  }

  if (io.Seek(4, icSeekSet) != 4) {
    std::printf("seek set failed\n");
    return 1;
  }

  if (io.GetLength() != byte_count || io.Tell() != 4) {
    std::printf("GetLength did not preserve position: length=%zu position=%lld\n",
                io.GetLength(), (long long)io.Tell());
    return 1;
  }

  if (io.Seek(-1, icSeekSet) >= 0) {
    std::printf("negative absolute seek unexpectedly succeeded\n");
    return 1;
  }

  if (io.Tell() != 4) {
    std::printf("failed seek changed position: position=%lld\n",
                (long long)io.Tell());
    return 1;
  }

  if (io.Seek(-2, icSeekEnd) != (int64_t)byte_count - 2) {
    std::printf("seek end failed: position=%lld\n", (long long)io.Tell());
    return 1;
  }

  if (io.Seek(1, icSeekCur) != (int64_t)byte_count - 1) {
    std::printf("seek cur failed: position=%lld\n", (long long)io.Tell());
    return 1;
  }

  std::printf("CIccFileIO seek/tell/length OK: length=%zu position=%lld\n",
              io.GetLength(), (long long)io.Tell());
  io.Close();
  std::remove(scratch_file);
  return 0;
}
