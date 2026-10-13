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

#include "IccWrapper.h"

#include <cstdio>

static bool CheckAdditionalApply(const char *profilePath, icBoolean useBpc)
{
  CIccCmmHandle *cmm = IccCmmCreate(icSigUnknownData, icSigUnknownData, false);
  if (!cmm) {
    std::fprintf(stderr, "failed to create output CMM\n");
    return false;
  }

  icStatusCMM attached = IccCmmAttachProfileFile(
    cmm, profilePath, icRelativeColorimetric, icInterpLinear, nullptr,
    icXformLutColor, false, useBpc, nullptr);
  icStatusCMM begun = attached == icCmmStatOk ? IccCmmBegin(cmm) : attached;

  icFloatNumber src[3] = { 0.25f, 0.25f, 0.25f };
  icFloatNumber baseDst[3] = { 0.0f, 0.0f, 0.0f };
  icStatusCMM baseApplied = begun == icCmmStatOk
    ? IccCmmApplyFloat(cmm, baseDst, src) : begun;

  CIccApplyHandle *apply = baseApplied == icCmmStatOk
    ? IccCmmGetApply(cmm) : nullptr;
  icFloatNumber extraDst[3] = { 0.0f, 0.0f, 0.0f };
  icStatusCMM extraApplied = apply
    ? IccApplyApplyFloat(apply, extraDst, src) : icCmmStatBad;

  bool ok = attached == icCmmStatOk && begun == icCmmStatOk &&
            baseApplied == icCmmStatOk && apply &&
            extraApplied == icCmmStatOk;
  if (!ok) {
    std::fprintf(stderr,
                 "BPC=%u: attach=%d begin=%d base_apply=%d get_apply=%s extra_apply=%d\n",
                 (unsigned)useBpc, (int)attached, (int)begun,
                 (int)baseApplied, apply ? "ok" : "null", (int)extraApplied);
  }

  if (apply)
    IccApplyFree(apply);
  IccCmmFree(cmm);
  return ok;
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s profile.icc\n", argv[0]);
    return 2;
  }

  return CheckAdditionalApply(argv[1], false) &&
         CheckAdditionalApply(argv[1], true) ? 0 : 1;
}
