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
#include "IccCmm.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <cstring>

static int expect_status(const char *name, icStatusCMM got, icStatusCMM want)
{
  if (got != want) {
    std::printf("%s: got %d, expected %d\n", name, got, want);
    return 1;
  }
  std::printf("%s: status %d\n", name, got);
  return 0;
}

static void init_named_entry(CIccTagNamedColor2 &tag, const char *rootName)
{
  SIccNamedColorEntry *entry = tag.GetEntry(0);
  std::snprintf(entry->rootName, sizeof(entry->rootName), "%s", rootName);

  for (int i = 0; i < 3; ++i) {
    entry->pcsCoords[i] = 0.0f;
  }

  for (icUInt32Number i = 0; i < tag.GetDeviceCoords(); ++i) {
    entry->deviceCoords[i] = 0.05f * static_cast<icFloatNumber>(i + 1);
  }
}

static icStatusCMM run_pixel_to_name(CIccTagNamedColor2 &tag,
                                     icColorSpaceSignature deviceSpace,
                                     icChar *name,
                                     const icFloatNumber *src)
{
  CIccXformNamedColor xform(&tag, icSigLabData, deviceSpace);
  icStatusCMM status = xform.SetSrcSpace(deviceSpace);
  if (status != icCmmStatOk) {
    return status;
  }

  status = xform.SetDestSpace(icSigNamedData);
  if (status != icCmmStatOk) {
    return status;
  }

  return xform.Apply(NULL, name, src);
}

int main()
{
  icFloatNumber src[17] = {};
  icChar name[256] = {};
  int failures = 0;

  CIccTagNamedColor2 valid(1, 3);
  init_named_entry(valid, "valid-device-color");
  failures += expect_status("valid-device-color",
                            run_pixel_to_name(valid, icSigRgbData, name, src),
                            icCmmStatOk);
  if (std::strcmp(name, "valid-device-color")) {
    std::printf("valid-device-color: unexpected name '%s'\n", name);
    failures++;
  }

  CIccTagNamedColor2 noDeviceCoords(1, 0);
  init_named_entry(noDeviceCoords, "no-device-coordinates");
  failures += expect_status("no-device-coordinates",
                            run_pixel_to_name(noDeviceCoords, icSigRgbData, name, src),
                            icCmmStatColorNotFound);

  CIccTagNamedColor2 tooManyDeviceCoords(1, 17);
  init_named_entry(tooManyDeviceCoords, "too-many-device-coordinates");
  failures += expect_status("too-many-device-coordinates",
                            run_pixel_to_name(tooManyDeviceCoords, icSigRgbData, name, src),
                            icCmmStatTooManySamples);

  return failures ? 1 : 0;
}
