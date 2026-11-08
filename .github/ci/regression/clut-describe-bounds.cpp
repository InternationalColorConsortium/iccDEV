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

#include "IccTagLut.h"

#include <algorithm>
#include <cstdio>
#include <string>

class TestClut : public CIccCLUT
{
public:
  TestClut() : CIccCLUT(1, 1) {}

  // Iterate(std::string&, ...) is a protected, source-compatible entry point.
  void DumpWithBuffer(std::string &description, size_t size)
  {
    Iterate(description, 0, 0, size);
  }
};

int main()
{
  TestClut clut;
  if (!clut.Init(2)) {
    std::fputs("CLUT initialization failed\n", stderr);
    return 1;
  }
  clut[0] = 0.0f;
  clut[1] = 1.0f;

  // For an unknown color space, each value is eight characters. The first
  // channel consumes nine including its prefix. Size 10 leaves one byte for
  // the three-byte separator, and size 21 leaves one for the two-byte newline.
  // The old strcpy calls wrote beyond each vector's allocation.
  const size_t smallSizes[] = {0, 1, 10, 11, 20, 21, 22};
  for (size_t size : smallSizes) {
    std::string description;
    clut.DumpWithBuffer(description, size);
  }

  std::string description;
  clut.DumpWithBuffer(description, 64);
  if (std::count(description.begin(), description.end(), '\n') != 2 ||
      description.find("0.00000") == std::string::npos ||
      description.find("1.00000") == std::string::npos) {
    std::fputs("Complete CLUT rows were not preserved\n", stderr);
    return 1;
  }

  return 0;
}
