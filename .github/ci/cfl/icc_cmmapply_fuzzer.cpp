/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file In-process libFuzzer target for IccProfLib CMM construction/apply. */

#include "IccCmm.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxIccInputSize = 1024 * 1024;
static constexpr size_t kMaxChannels = 64;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || size < kMinIccInputSize || size > kMaxIccInputSize)
    return 0;

  const bool first_is_input = (data[0] & 1u) == 0;
  const icRenderingIntent intent =
      static_cast<icRenderingIntent>(data[1] % 4u);
  const icXformInterp interpolation =
      (data[2] & 1u) ? icInterpTetrahedral : icInterpLinear;

  CIccCmm cmm(icSigUnknownData, icSigUnknownData, first_is_input);
  icStatusCMM status = cmm.AddXform(
      const_cast<icUInt8Number *>(data), static_cast<icUInt32Number>(size),
      intent, interpolation);
  if (status != icCmmStatOk || cmm.Begin() != icCmmStatOk)
    return 0;

  const size_t source_samples = cmm.GetSourceSamples();
  const size_t dest_samples = cmm.GetDestSamples();
  if (!source_samples || !dest_samples || source_samples > kMaxChannels ||
      dest_samples > kMaxChannels)
    return 0;

  std::vector<icFloatNumber> source(source_samples);
  std::vector<icFloatNumber> destination(dest_samples);
  for (size_t i = 0; i < source.size(); ++i)
    source[i] = static_cast<icFloatNumber>(data[(i + 3) % size] / 255.0f);

  status = cmm.Apply(destination.data(), source.data());
  volatile icFloatNumber result_sink =
      status == icCmmStatOk ? destination[0] : static_cast<icFloatNumber>(status);
  (void)result_sink;
  return 0;
}
