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

/** @file
    LibFuzzer harness for the data-first iccDEV profile visualization API.

    The harness links IccVizModel.cpp as a separate translation unit and calls
    only its public header. This preserves the compile/link boundary without
    including a command-line implementation file or depending on processLuts().
 */

#include "IccProfile.h"
#include "IccVizModel.hpp"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxIccInputSize = 5 * 1024 * 1024;
static constexpr size_t kMaxProfileTags = 200;
static constexpr size_t kMaxDescriptors = 256;

static size_t observeGraph(const iccviz::GraphResult &result)
{
  size_t observations = result.error.size() + result.diagnostics.size();

  if (!result.ok)
    return observations;

  observations += result.graph.title.size();
  observations += result.graph.description.size();
  for (const auto &series : result.graph.series)
    observations += series.verts.size();

  return observations;
}

static size_t observeRaster(const iccviz::RasterResult &result)
{
  size_t observations = result.error.size() + result.diagnostics.size();

  if (result.ok)
    observations += result.raster.samples.size();

  return observations;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || size < kMinIccInputSize || size > kMaxIccInputSize)
    return 0;

  std::unique_ptr<CIccProfile> profile(
      OpenIccProfile(const_cast<icUInt8Number *>(data), size));
  if (!profile || profile->m_Tags.size() > kMaxProfileTags)
    return 0;

  iccviz::SetSilent(true);
  const std::vector<iccviz::Descriptor> descriptors =
      iccviz::Enumerate(profile.get());
  if (descriptors.size() > kMaxDescriptors)
    return 0;

  size_t observations = descriptors.size();
  for (const auto &descriptor : descriptors) {
    if (descriptor.output == iccviz::Output::Graph) {
      observations += observeGraph(
          iccviz::RenderGraph(profile.get(), descriptor.id,
                              iccviz::Verbosity::Silent));
    } else {
      observations += observeRaster(
          iccviz::RenderRaster(profile.get(), descriptor.id,
                               iccviz::Verbosity::Silent));
    }
  }

  volatile size_t resultSink = observations;
  (void)resultSink;
  return 0;
}
