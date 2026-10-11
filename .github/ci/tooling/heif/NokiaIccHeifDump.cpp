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
#include <sstream>
#include <string>

#include "IccProfile.h"
#include "heifreader.h"

namespace
{
  std::string outputPath(const std::string& directory,
                         const std::uint32_t itemId,
                         const std::uint32_t propertyIndex,
                         const char* colourType)
  {
    std::ostringstream path;
    path << directory;
    if (!directory.empty() && directory.back() != '/') {
      path << '/';
    }
    path << "item-" << itemId << "-property-" << propertyIndex << '-' << colourType << ".icc";
    return path.str();
  }

  bool writeProfile(const std::string& path, const HEIF::Array<std::uint8_t>& profile)
  {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
      return false;
    }
    output.write(reinterpret_cast<const char*>(profile.elements), static_cast<std::streamsize>(profile.size));
    return output.good();
  }

  const char* parseProfile(const HEIF::Array<std::uint8_t>& profile)
  {
    if (profile.size == 0) {
      return "empty";
    }
    if (profile.size > std::numeric_limits<icUInt32Number>::max()) {
      return "too-large";
    }

    CIccProfile* parsedProfile =
        ReadIccProfile(reinterpret_cast<const icUInt8Number*>(profile.elements),
                       static_cast<icUInt32Number>(profile.size));
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

  HEIF::Reader* reader = HEIF::Reader::Create();
  if (reader == nullptr) {
    std::cerr << "Error: unable to create HEIF reader" << std::endl;
    return 2;
  }

  const HEIF::ErrorCode initializeResult = reader->initialize(argv[1]);
  if (initializeResult != HEIF::ErrorCode::OK) {
    std::cerr << "Error: unable to initialize HEIF reader: " << static_cast<int>(initializeResult) << std::endl;
    HEIF::Reader::Destroy(reader);
    return 2;
  }

  HEIF::FileInformation fileInformation;
  const HEIF::ErrorCode informationResult = reader->getFileInformation(fileInformation);
  if (informationResult != HEIF::ErrorCode::OK) {
    std::cerr << "Error: unable to read HEIF file information: " << static_cast<int>(informationResult)
              << std::endl;
    HEIF::Reader::Destroy(reader);
    return 2;
  }

  std::size_t profileCount = 0;
  for (const auto& item : fileInformation.rootMetaBoxInformation.itemInformations) {
    HEIF::Array<HEIF::ItemPropertyInfo> properties;
    const HEIF::ErrorCode propertiesResult = reader->getItemProperties(item.itemId, properties);
    if (propertiesResult == HEIF::ErrorCode::INVALID_ITEM_ID) {
      continue;
    }
    if (propertiesResult != HEIF::ErrorCode::OK) {
      std::cerr << "Error: unable to read properties for item " << item.itemId.get() << ": "
                << static_cast<int>(propertiesResult) << std::endl;
      HEIF::Reader::Destroy(reader);
      return 2;
    }

    for (const auto& property : properties) {
      if (property.type != HEIF::ItemPropertyType::COLR) {
        continue;
      }

      HEIF::ColourInformation colourInformation{};
      const HEIF::ErrorCode propertyResult = reader->getProperty(property.index, colourInformation);
      if (propertyResult != HEIF::ErrorCode::OK) {
        std::cerr << "Error: unable to read colr property " << property.index.get() << " for item "
                  << item.itemId.get() << ": " << static_cast<int>(propertyResult) << std::endl;
        HEIF::Reader::Destroy(reader);
        return 2;
      }

      const bool isIcc = colourInformation.colourType == HEIF::FourCC("rICC") ||
                         colourInformation.colourType == HEIF::FourCC("prof");
      if (!isIcc) {
        continue;
      }

      ++profileCount;
      std::cout << "item=" << item.itemId.get() << " property=" << property.index.get()
                << " essential=" << (property.essential ? 1 : 0)
                << " colour_type=" << colourInformation.colourType.value
                << " profile_bytes=" << colourInformation.iccProfile.size;

      if (argc == 3) {
        const std::string path = outputPath(argv[2], item.itemId.get(), property.index.get(),
                                            colourInformation.colourType.value);
        if (!writeProfile(path, colourInformation.iccProfile)) {
          std::cerr << "Error: unable to write extracted ICC profile: " << path << std::endl;
          HEIF::Reader::Destroy(reader);
          return 2;
        }
        std::cout << " output=" << path;
      }

      std::cout << " proflib_parse=" << parseProfile(colourInformation.iccProfile) << std::endl;
    }
  }

  std::cout << "icc_profiles=" << profileCount << std::endl;
  HEIF::Reader::Destroy(reader);
  return 0;
}
