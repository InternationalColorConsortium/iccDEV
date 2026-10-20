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
/** @file In-process libFuzzer target for bounded CIccCmmSearch application. */

#include "IccCmmSearch.h"
#include "IccProfile.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxInputSize = 1024 * 1024;
static constexpr size_t kMaxChannels = 64;
static constexpr unsigned int kMaxSearchIterations = 64;
static constexpr unsigned int kMaxSearchEvaluations = 128;

struct SearchInput {
  const uint8_t *source;
  size_t source_size;
  const uint8_t *destination;
  size_t destination_size;
  const uint8_t *control;
  size_t control_size;
};

struct SearchOptions {
  icRenderingIntent source_intent;
  icRenderingIntent destination_intent;
  icXformInterp interpolation;
  bool use_pcc;
  bool use_d2b;
  icFloatNumber pcc_weight;
};

static icUInt32Number readBigEndian32(const uint8_t *data)
{
  return (static_cast<icUInt32Number>(data[0]) << 24) |
         (static_cast<icUInt32Number>(data[1]) << 16) |
         (static_cast<icUInt32Number>(data[2]) << 8) |
         static_cast<icUInt32Number>(data[3]);
}

static icRenderingIntent headerIntent(const uint8_t *profile, size_t size)
{
  if (size < 68)
    return icPerceptual;
  const icUInt32Number value = readBigEndian32(profile + 64);
  return static_cast<icRenderingIntent>(
      value <= static_cast<icUInt32Number>(icAbsoluteColorimetric)
          ? value
          : static_cast<icUInt32Number>(icPerceptual));
}

static bool splitInput(const uint8_t *data, size_t size, SearchInput &input)
{
  if (!data || size < 2 * kMinIccInputSize || size > kMaxInputSize)
    return false;

  const size_t source_size = readBigEndian32(data);
  if (source_size < kMinIccInputSize || source_size > size - kMinIccInputSize)
    return false;

  const uint8_t *destination = data + source_size;
  const size_t remaining = size - source_size;
  const size_t destination_size = readBigEndian32(destination);
  if (destination_size < kMinIccInputSize || destination_size > remaining)
    return false;

  input = {data, source_size, destination, destination_size,
           destination + destination_size, remaining - destination_size};
  return true;
}

static size_t exerciseSearch(const SearchInput &input, const SearchOptions &options,
                             uint8_t pixel_seed)
{
  CIccCmmSearch cmm;
  icStatusCMM status = cmm.CIccCmm::AddXform(
      const_cast<icUInt8Number *>(input.source),
      static_cast<icUInt32Number>(input.source_size), options.source_intent,
      options.interpolation, nullptr, icXformLutColor, options.use_d2b);
  if (status != icCmmStatOk)
    return static_cast<size_t>(status);

  status = cmm.CIccCmm::AddXform(
      const_cast<icUInt8Number *>(input.destination),
      static_cast<icUInt32Number>(input.destination_size),
      options.destination_intent, options.interpolation, nullptr,
      icXformLutColor, options.use_d2b);
  if (status != icCmmStatOk)
    return static_cast<size_t>(status);

  if (options.use_pcc) {
    CIccProfile *pcc = OpenIccProfile(
        const_cast<icUInt8Number *>(input.destination),
        static_cast<icUInt32Number>(input.destination_size));
    if (pcc) {
      if (pcc->ReadPccTags()) {
        pcc->Detach();
        if (cmm.AttachPCC(pcc, options.pcc_weight) != icCmmStatOk)
          delete pcc;
      }
      else {
        delete pcc;
      }
    }
  }

  status = cmm.Begin();
  if (status != icCmmStatOk)
    return static_cast<size_t>(status);

  CIccApplyCmmSearch *apply = dynamic_cast<CIccApplyCmmSearch *>(cmm.GetApply());
  if (!apply || !apply->IsReady())
    return 0;
  apply->SetMaxIterations(kMaxSearchIterations);
  apply->SetMaxFuncEvals(kMaxSearchEvaluations);

  const size_t source_samples = cmm.GetSourceSamples();
  const size_t destination_samples = cmm.GetDestSamples();
  if (!source_samples || !destination_samples ||
      source_samples > kMaxChannels || destination_samples > kMaxChannels)
    return source_samples + destination_samples;

  std::vector<icFloatNumber> source(source_samples);
  std::vector<icFloatNumber> destination(destination_samples);
  for (size_t i = 0; i < source_samples; ++i) {
    source[i] = static_cast<icFloatNumber>(
        (pixel_seed + input.source[(i + 68) % input.source_size]) & 0xffu) /
        255.0f;
  }

  (void)cmm.RemoveAllIO();
  status = apply->Apply(destination.data(), source.data());
  if (status != icCmmStatOk)
    return static_cast<size_t>(status);

  icFloatNumber cost = -1.0f;
  status = apply->GetApplyCost(cost, source.data());
  volatile icFloatNumber result_sink =
      cost + (destination.empty() ? 0.0f : destination[0]);
  (void)result_sink;
  return destination.size() + static_cast<size_t>(status);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  SearchInput input{};
  if (!splitInput(data, size, input))
    return 0;

  const uint8_t flags = input.control_size ? input.control[0] : 0;
  const uint8_t pixel_seed = input.control_size > 3 ? input.control[3] : 0;
  static const std::array<icFloatNumber, 6> kWeights = {{
      1.0f, 0.5f, 2.0f, 0.0f, -1.0f, NAN,
  }};

  std::array<SearchOptions, 2> options = {{
      {headerIntent(input.source, input.source_size),
       headerIntent(input.destination, input.destination_size),
       icInterpLinear, false, true, 1.0f},
      {static_cast<icRenderingIntent>(
           input.control_size > 1 ? input.control[1] % 4u : icPerceptual),
       static_cast<icRenderingIntent>(
           input.control_size > 2 ? input.control[2] % 4u : icRelativeColorimetric),
       (flags & 1u) ? icInterpTetrahedral : icInterpLinear,
       true, (flags & 2u) == 0,
       kWeights[(flags >> 2) % kWeights.size()]},
  }};

  size_t observations = 0;
  for (const SearchOptions &option : options)
    observations += exerciseSearch(input, option, pixel_seed);

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
