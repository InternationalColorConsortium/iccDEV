/*
 * Copyright (c) 2026 International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING
 * MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file In-process libFuzzer target for IccProfLib parsing and inspection. */

#include "IccProfile.h"
#include "IccTag.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxIccInputSize = 1024 * 1024;
static constexpr size_t kMaxProfileTags = 512;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || size < kMinIccInputSize || size > kMaxIccInputSize)
    return 0;

  std::string report;
  icValidateStatus status = icValidateOK;
  std::unique_ptr<CIccProfile> profile(
      ValidateIccProfile(data, static_cast<icUInt32Number>(size), report,
                         status));
  if (!profile || profile->m_Tags.size() > kMaxProfileTags)
    return 0;

  size_t observations = report.size() + static_cast<size_t>(status);
  for (auto &entry : profile->m_Tags) {
    CIccTag *tag = profile->FindTag(entry);
    if (!tag)
      continue;

    std::string description;
    tag->Describe(description, 0);
    observations += description.size();
  }

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
