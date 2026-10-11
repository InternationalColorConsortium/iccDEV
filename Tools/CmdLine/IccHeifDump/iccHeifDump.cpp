/** @file
    File:       iccHeifDump

    Contains:   IccProfLib-aware HEIF ICC profile extractor.

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
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
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
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

  bool writeProfile(const std::string& path, const std::vector<std::uint8_t>& profile)
  {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
      return false;
    }
    output.write(reinterpret_cast<const char*>(profile.data()),
                 static_cast<std::streamsize>(profile.size()));
    return output.good();
  }

  const char* parseProfile(const std::vector<std::uint8_t>& profile)
  {
    if (profile.empty()) {
      return "empty";
    }
    if (profile.size() > std::numeric_limits<icUInt32Number>::max()) {
      return "too-large";
    }

    CIccProfile* parsedProfile =
        ReadIccProfile(reinterpret_cast<const icUInt8Number*>(profile.data()),
                       static_cast<icUInt32Number>(profile.size()));
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

  std::vector<iccdev_heif::IccProperty> properties;
  if (!iccdev_heif::collectIccProperties(argv[1], properties)) {
    std::cerr << "Error: unable to read HEIF colour properties" << std::endl;
    heif_context_free(context);
    return 2;
  }
  std::size_t profileCount = 0;
  for (const auto& property : properties) {
    const std::vector<std::uint8_t>& profile = property.profile;
    ++profileCount;
    std::cout << "item=" << property.itemId
              << " property=" << property.propertyIndex
              << " essential=" << (property.essential ? 1 : 0)
              << " colour_type=" << property.colourType
              << " profile_bytes=" << profile.size();

    if (argc == 3) {
      const std::string path = outputPath(argv[2], property.itemId,
                                          property.propertyIndex, property.colourType);
      if (!writeProfile(path, profile)) {
        std::cerr << "Error: unable to write extracted ICC profile: " << path << std::endl;
        heif_context_free(context);
        return 2;
      }
      std::cout << " output=" << path;
    }

    std::cout << " proflib_parse=" << parseProfile(profile) << std::endl;
  }

  std::cout << "icc_profiles=" << profileCount << std::endl;
  heif_context_free(context);
  return 0;
}
