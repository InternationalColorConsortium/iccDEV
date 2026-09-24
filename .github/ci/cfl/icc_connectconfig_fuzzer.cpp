/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file In-process libFuzzer target for IccConnect JSON configuration. */

#include "IccCmmConfig.h"

#include <stddef.h>
#include <stdint.h>

static constexpr size_t kMaxTextInputSize = 1024 * 1024;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size || size > kMaxTextInputSize)
    return 0;

  json root = json::parse(data, data + size, nullptr, false);
  if (root.is_discarded())
    return 0;

  CIccCfgDataApply data_apply;
  CIccCfgImageApply image_apply;
  CIccCfgConnectOptions connect_options;
  CIccCfgCreateLink create_link;
  CIccCfgProfileSequence profiles;
  CIccCfgSearchApply search_apply;

  size_t observations = 0;
  observations += data_apply.fromJson(root, true);
  observations += image_apply.fromJson(root, true);
  observations += connect_options.fromJson(root, true);
  observations += create_link.fromJson(root, true);
  observations += profiles.fromJson(root, true);
  observations += search_apply.fromJson(root, true);

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
