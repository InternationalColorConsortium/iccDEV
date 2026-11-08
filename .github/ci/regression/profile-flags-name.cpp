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

#include "IccUtil.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>

static bool CheckFlags(CIccInfo &info, icUInt32Number flags, bool checkMcs,
                       const char *expected)
{
  const char *actual = info.GetProfileFlagsName(flags, checkMcs);
  if (std::strcmp(actual, expected) != 0) {
    std::fprintf(stderr, "flags 0x%08x: expected '%s', got '%s'\n",
                 (unsigned)flags, expected, actual);
    return false;
  }
  return true;
}

int main()
{
  CIccInfo info;
  if (!CheckFlags(info, icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly,
                  false, "EmbeddedProfileTrue | UseWithEmbeddedDataOnly") ||
      !CheckFlags(info, icEmbeddedProfileTrue | icUseWithEmbeddedDataOnly |
                            icExtendedRangePCS,
                  false,
                  "EmbeddedProfileTrue | UseWithEmbeddedDataOnly | ExtendedRangePCS") ||
      !CheckFlags(info, icMCSNeedsSubsetTrue | icExtendedRangePCS, true,
                  "EmbeddedProfileFalse | UseAnywhere | MCSNeedsSubset | ExtendedRangePCS"))
    return 1;

  return 0;
}
