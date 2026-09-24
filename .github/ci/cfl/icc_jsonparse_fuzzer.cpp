/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file In-process libFuzzer target for IccJSON profile parsing. */

#include "IccMpeJsonFactory.h"
#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"

#include <stddef.h>
#include <stdint.h>

#include <new>
#include <string>

static constexpr size_t kMaxTextInputSize = 1024 * 1024;

static void initializeJsonFactories()
{
  static const bool initialized = []() {
    CIccTagCreator::PushFactory(new (std::nothrow) CIccTagJsonFactory());
    CIccMpeCreator::PushFactory(new (std::nothrow) CIccMpeJsonFactory());
    return true;
  }();
  (void)initialized;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size || size > kMaxTextInputSize)
    return 0;

  initializeJsonFactories();
  IccJson root = IccJson::parse(data, data + size, nullptr, false);
  if (root.is_discarded())
    return 0;

  CIccProfileJson profile;
  std::string report;
  if (profile.ParseJson(root, report))
    profile.Validate(report);

  volatile size_t result_sink = report.size();
  (void)result_sink;
  return 0;
}
