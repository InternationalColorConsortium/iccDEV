/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file In-process libFuzzer target for the PAWG assessment library seam. */

#include "PawgReport.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

static constexpr size_t kMaxIccInputSize = 1024 * 1024;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size || size > kMaxIccInputSize)
    return 0;

  std::string detail;
  const int assessment = AssessPawgFromMemory(data, size);
  const int compression = PawgCompressionVerdict(data, size, &detail);
  volatile size_t result_sink = detail.size() +
      static_cast<size_t>(assessment != 0) +
      static_cast<size_t>(compression);
  (void)result_sink;
  return 0;
}
