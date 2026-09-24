/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
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
