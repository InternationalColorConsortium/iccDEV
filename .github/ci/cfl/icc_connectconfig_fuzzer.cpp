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

/** @file In-process libFuzzer target for IccConnect JSON configuration. */

#include "IccCmmConfig.h"

#include <stddef.h>
#include <stdint.h>

static constexpr size_t kMaxTextInputSize = 1024 * 1024;

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
  if (!parsed.fromJson(value, true))
    return 0;

  json serialized;
  parsed.toJson(serialized);

  T replayed;
  const bool replayed_ok = replayed.fromJson(serialized, true);
  return serialized.dump().size() + static_cast<size_t>(replayed_ok);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size || size > kMaxTextInputSize)
    return 0;

  json root = json::parse(data, data + size, nullptr, false);
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
  if (profiles.is_array() && !profiles.empty())
    observations += exerciseRoundTrip<CIccCfgProfile>(profiles.front());

  const json &search_profiles = sectionOrRoot(search_apply, "profileSequence");
  observations += exerciseRoundTrip<CIccCfgProfileSequence>(search_profiles);
  if (search_profiles.is_array() && !search_profiles.empty()) {
    observations += exerciseRoundTrip<CIccCfgProfile>(
        search_profiles.front());
  }

  const json &pcc_weights = sectionOrRoot(search_apply, "pccWeights");
  if (pcc_weights.is_array() && !pcc_weights.empty())
    observations += exerciseRoundTrip<CIccCfgPccWeight>(pcc_weights.front());

  const json &color_data = sectionOrRoot(root, "colorData");
  const json &data_entries = sectionOrRoot(color_data, "data");
  if (data_entries.is_array() && !data_entries.empty())
    observations += exerciseRoundTrip<CIccCfgDataEntry>(data_entries.front());

  volatile size_t result_sink = observations;
  (void)result_sink;
  return 0;
}
