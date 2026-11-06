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
/** @file In-process libFuzzer target for IccConnect JSON configuration. */

#include "IccCmmConfig.h"

#include <stddef.h>
#include <stdint.h>

static constexpr size_t kMaxDirectArrayElements = 16;

static const json &sectionOrRoot(const json &root, const char *name)
{
  if (root.is_object()) {
    const auto section = root.find(name);
    if (section != root.end())
      return *section;
  }

  return root;
}

template <typename T>
static size_t exerciseRoundTrip(const json &value)
{
  T parsed;
  parsed.reset();
  if (!parsed.fromJson(value, false))
    return 0;

  json serialized;
  parsed.toJson(serialized);

  T replayed;
  const bool replayed_ok = replayed.fromJson(serialized, true);
  json replayed_serialized;
  if (replayed_ok)
    replayed.toJson(replayed_serialized);

  const bool updated_ok = replayed_ok && replayed.fromJson(value, false);
  json updated_serialized;
  if (updated_ok)
    replayed.toJson(updated_serialized);

  return serialized.dump().size() + replayed_serialized.dump().size() +
      updated_serialized.dump().size() + static_cast<size_t>(replayed_ok) +
      static_cast<size_t>(updated_ok) +
      static_cast<size_t>(serialized == replayed_serialized);
}

template <typename T>
static size_t exerciseArrayElements(const json &values)
{
  if (!values.is_array())
    return 0;

  size_t observations = 0;
  size_t count = 0;
  for (const auto &value : values) {
    observations += exerciseRoundTrip<T>(value);
    if (++count == kMaxDirectArrayElements)
      break;
  }
  return observations;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size)
    return 0;

  json root;
  try {
    root = json::parse(data, data + size, nullptr, false);
  } catch (...) {
    return 0;
  }

  if (root.is_discarded())
    return 0;

  size_t observations = 0;
  observations += exerciseRoundTrip<CIccCfgDataApply>(
      sectionOrRoot(root, "dataFiles"));
  observations += exerciseRoundTrip<CIccCfgImageApply>(
      sectionOrRoot(root, "imageFiles"));
  observations += exerciseRoundTrip<CIccCfgConnectOptions>(
      sectionOrRoot(root, "connect"));
  observations += exerciseRoundTrip<CIccCfgCreateLink>(
      sectionOrRoot(root, "createLink"));
  observations += exerciseRoundTrip<CIccCfgColorData>(
      sectionOrRoot(root, "colorData"));

  const json &search_apply = sectionOrRoot(root, "searchApply");
  observations += exerciseRoundTrip<CIccCfgSearchApply>(search_apply);

  const json &profiles = sectionOrRoot(root, "profileSequence");
  observations += exerciseRoundTrip<CIccCfgProfileSequence>(profiles);
  observations += exerciseArrayElements<CIccCfgProfile>(profiles);

  const json &search_profiles = sectionOrRoot(search_apply, "profileSequence");
  observations += exerciseRoundTrip<CIccCfgProfileSequence>(search_profiles);
  if (&search_profiles != &profiles)
    observations += exerciseArrayElements<CIccCfgProfile>(search_profiles);

  const json &pcc_weights = sectionOrRoot(search_apply, "pccWeights");
  observations += exerciseArrayElements<CIccCfgPccWeight>(pcc_weights);

  const json &color_data = sectionOrRoot(root, "colorData");
  const json &data_entries = sectionOrRoot(color_data, "data");
  observations += exerciseArrayElements<CIccCfgDataEntry>(data_entries);

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
