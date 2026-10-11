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
/*
    File:       iccconnect-search-cost.cpp

    Contains:   CTest helper for CIccCmmSearch::AttachPCC weight validation.

    The weighted-average search cost divides by the accumulated PCC weights.
    This helper verifies invalid weights are rejected before they can produce
    zero, negative, or non-finite denominators.

    Usage:
      iccconnect-search-cost <pcc_profile.icc> <weight> <expect-accept|expect-reject>

    Exit codes:
      0 - expected result observed
      1 - unexpected result or PCC read failure
      2 - usage error
*/

#include "IccCmmSearch.h"
#include "IccProfile.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

int main(int argc, char** argv)
{
  if (argc != 4) {
    std::fprintf(stderr,
      "usage: %s <pcc_profile.icc> <weight> <expect-accept|expect-reject>\n",
      argv[0] ? argv[0] : "iccconnect-search-cost");
    return 2;
  }

  const char* pccPath = argv[1];
  const icFloatNumber pccWeight = static_cast<icFloatNumber>(std::strtod(argv[2], nullptr));
  const bool expectAccept = std::strcmp(argv[3], "expect-accept") == 0;
  const bool expectReject = std::strcmp(argv[3], "expect-reject") == 0;
  if (!expectAccept && !expectReject) {
    std::fprintf(stderr, "iccconnect-search-cost: expected result must be expect-accept or expect-reject\n");
    return 2;
  }

  CIccProfile* pPcc = OpenIccProfile(pccPath);
  if (!pPcc || !pPcc->ReadPccTags()) {
    std::fprintf(stderr, "iccconnect-search-cost: unable to read PCC tags from '%s'\n", pccPath);
    delete pPcc;
    return 1;
  }
  pPcc->Detach();

  std::unique_ptr<CIccCmmSearch> pCmm(new CIccCmmSearch());
  icStatusCMM rv = pCmm->AttachPCC(pPcc, pccWeight);
  if (expectAccept && rv == icCmmStatOk) {
    std::fprintf(stdout, "iccconnect-search-cost: PASS  accepted weight=%g\n",
                 (double)pccWeight);
    return 0;
  }
  if (expectReject && rv != icCmmStatOk) {
    delete pPcc;
    std::fprintf(stdout, "iccconnect-search-cost: PASS  rejected weight=%g status=%d\n",
                 (double)pccWeight, (int)rv);
    return 0;
  }

  if (rv != icCmmStatOk)
    delete pPcc;
  std::fprintf(stderr, "iccconnect-search-cost: unexpected AttachPCC status=%d for weight=%g\n",
               (int)rv, (double)pccWeight);
  return 1;
}
