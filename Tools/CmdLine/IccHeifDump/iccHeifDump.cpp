/** @file
    File:       iccHeifDump

    Contains:   IccProfLib-aware HEIF ICC profile extractor.

    Version:    V1

    Copyright:  (c) see Software License
*/

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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include "IccProfile.h"
#include "HeifIccProperties.h"
#include <libheif/heif.h>

namespace
{
  std::string outputPath(const std::string& directory,
                         const std::uint32_t itemId,
                         const std::uint32_t propertyIndex,
                         const std::string& colourType)
  {
    std::ostringstream path;
    path << directory;
    if (!directory.empty() && directory.back() != '/') {
      path << '/';
    }
    path << "item-" << itemId << "-property-" << propertyIndex << '-'
         << colourType << ".icc";
    return path.str();
  }

  bool writeProfile(const std::string& path, const std::uint8_t* profile,
                    std::size_t profileSize)
  {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
      return false;
    }
    output.write(reinterpret_cast<const char*>(profile),
                 static_cast<std::streamsize>(profileSize));
    return output.good();
  }

  const char* parseProfile(const std::uint8_t* profile, std::size_t profileSize)
  {
    if (profileSize == 0) {
      return "empty";
    }
    if (profileSize > std::numeric_limits<icUInt32Number>::max()) {
      return "too-large";
    }

    CIccProfile* parsedProfile =
        ReadIccProfile(reinterpret_cast<const icUInt8Number*>(profile),
                       static_cast<icUInt32Number>(profileSize));
    if (parsedProfile == nullptr) {
      return "failed";
    }

    delete parsedProfile;
    return "ok";
  }

  void printUsage(const char* executable)
  {
    std::cerr << "Usage: " << executable << " input.heic [existing-output-directory]" << std::endl;
  }
}

int main(int argc, char* argv[])
{
  if (argc < 2 || argc > 3) {
    printUsage(argv[0]);
    return 3;
  }

  std::vector<iccdev_heif::IccProperty> properties;
  std::vector<std::uint8_t> bytes;
  bool collected = false;
  try {
    collected = iccdev_heif::collectIccProperties(argv[1], properties, bytes);
  } catch (const std::bad_alloc&) {
    std::cerr << "Error: insufficient memory for HEIF colour properties" << std::endl;
    return 2;
  }
  if (!collected) {
    std::cerr << "Error: unable to read HEIF colour properties" << std::endl;
    return 2;
  }

  heif_context* context = heif_context_alloc();
  if (context == nullptr) {
    std::cerr << "Error: unable to create HEIF context" << std::endl;
    return 2;
  }

  const heif_error readResult = heif_context_read_from_file(context, argv[1], nullptr);
  if (readResult.code != heif_error_Ok) {
    std::cerr << "Error: unable to read HEIF file: " << readResult.message << std::endl;
    heif_context_free(context);
    return 2;
  }

  std::size_t profileCount = 0;
  for (const auto& property : properties) {
    const std::uint8_t* profile = bytes.data() + property.profileOffset;
    ++profileCount;
    std::cout << "item=" << property.itemId
              << " property=" << property.propertyIndex
              << " essential=" << (property.essential ? 1 : 0)
              << " colour_type=" << property.colourType
              << " profile_bytes=" << property.profileSize;

    if (argc == 3) {
      const std::string path = outputPath(argv[2], property.itemId,
                                          property.propertyIndex, property.colourType);
      if (!writeProfile(path, profile, property.profileSize)) {
        std::cerr << "Error: unable to write extracted ICC profile: " << path << std::endl;
        heif_context_free(context);
        return 2;
      }
      std::cout << " output=" << path;
    }

    std::cout << " proflib_parse=" << parseProfile(profile, property.profileSize)
              << std::endl;
  }

  std::cout << "icc_profiles=" << profileCount << std::endl;
  heif_context_free(context);
  return 0;
}
