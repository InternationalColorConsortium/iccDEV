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

/** @file In-process libFuzzer target for IccProfLib CMM construction/apply. */

#include "IccCmm.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <vector>

static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxIccInputSize = 1024 * 1024;
static constexpr size_t kMaxChannels = 64;

struct CmmOptions {
  bool first_is_input;
  icRenderingIntent intent;
  icXformInterp interpolation;
};

static icUInt32Number readBigEndian32(const uint8_t *data)
{
  return (static_cast<icUInt32Number>(data[0]) << 24) |
         (static_cast<icUInt32Number>(data[1]) << 16) |
         (static_cast<icUInt32Number>(data[2]) << 8) |
         static_cast<icUInt32Number>(data[3]);
}

static size_t exerciseCmm(const uint8_t *data, size_t profile_size,
                          const CmmOptions &options)
{
  CIccCmm cmm(icSigUnknownData, icSigUnknownData, options.first_is_input);
  icStatusCMM status = cmm.AddXform(
      const_cast<icUInt8Number *>(data),
      static_cast<icUInt32Number>(profile_size), options.intent,
      options.interpolation);
  if (status != icCmmStatOk || cmm.Begin() != icCmmStatOk)
    return static_cast<size_t>(status);

  const size_t source_samples = cmm.GetSourceSamples();
  const size_t dest_samples = cmm.GetDestSamples();
  if (!source_samples || !dest_samples || source_samples > kMaxChannels ||
      dest_samples > kMaxChannels)
    return source_samples + dest_samples;

  std::vector<icFloatNumber> source(source_samples);
  std::vector<icFloatNumber> destination(dest_samples);
  for (size_t i = 0; i < source.size(); ++i) {
    source[i] = static_cast<icFloatNumber>(
        data[(i + 68) % profile_size] / 255.0f);
  }

  status = cmm.Apply(destination.data(), source.data());
  if (status == icCmmStatOk)
    return destination.size();

  return static_cast<size_t>(status);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || size < kMinIccInputSize || size > kMaxIccInputSize)
    return 0;

  const icUInt32Number declared_size = readBigEndian32(data);
  const size_t profile_size = declared_size >= kMinIccInputSize &&
                                      declared_size <= size
                                  ? declared_size
                                  : size;
  const icUInt32Number header_intent = readBigEndian32(data + 64);
  const icRenderingIntent intent = static_cast<icRenderingIntent>(
      header_intent <= static_cast<icUInt32Number>(icAbsoluteColorimetric)
          ? header_intent
          : static_cast<icUInt32Number>(icPerceptual));

  std::array<CmmOptions, 3> options = {{
      {true, intent, icInterpLinear},
      {true, intent, icInterpTetrahedral},
      {true, intent, icInterpLinear},
  }};
  size_t option_count = 2;

  // A valid ICC profile can carry a three-byte fuzzer-only trailer. Keeping
  // controls beyond the header lets libFuzzer mutate direction, intent, and
  // interpolation without corrupting the profile's encoded size.
  if (size - profile_size >= 3) {
    const uint8_t *control = data + profile_size;
    options[option_count++] = {
        (control[0] & 1u) == 0,
        static_cast<icRenderingIntent>(control[1] % 4u),
        (control[2] & 1u) ? icInterpTetrahedral : icInterpLinear,
    };
  }

  size_t observations = 0;
  for (size_t i = 0; i < option_count; ++i)
    observations += exerciseCmm(data, profile_size, options[i]);

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
