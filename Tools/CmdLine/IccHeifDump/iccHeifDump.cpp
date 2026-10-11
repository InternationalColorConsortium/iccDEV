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
#include <libheif/heif.h>

namespace
{
  std::string outputPath(const std::string& directory,
                         const heif_item_id itemId,
                         const heif_color_profile_type colourType)
  {
    std::ostringstream path;
    path << directory;
    if (!directory.empty() && directory.back() != '/') {
      path << '/';
    }
    path << "item-" << itemId << '-'
         << (colourType == heif_color_profile_type_rICC ? "rICC" : "prof") << ".icc";
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

  const int imageCount = heif_context_get_number_of_top_level_images(context);
  std::vector<heif_item_id> imageIds(imageCount);
  heif_context_get_list_of_top_level_image_IDs(context, imageIds.data(), imageCount);

  std::size_t profileCount = 0;
  for (const heif_item_id imageId : imageIds) {
    heif_image_handle* imageHandle = nullptr;
    const heif_error handleResult = heif_context_get_image_handle(context, imageId, &imageHandle);
    if (handleResult.code != heif_error_Ok) {
      std::cerr << "Error: unable to read image " << imageId << ": " << handleResult.message << std::endl;
      heif_context_free(context);
      return 2;
    }

    const heif_color_profile_type colourType =
        heif_image_handle_get_color_profile_type(imageHandle);
    if (colourType != heif_color_profile_type_rICC &&
        colourType != heif_color_profile_type_prof) {
      heif_image_handle_release(imageHandle);
      continue;
    }

    std::vector<std::uint8_t> profile(heif_image_handle_get_raw_color_profile_size(imageHandle));
    const heif_error profileResult = profile.empty()
        ? heif_error{heif_error_Ok, heif_suberror_Unspecified, "success"}
        : heif_image_handle_get_raw_color_profile(imageHandle, profile.data());
    if (profileResult.code != heif_error_Ok) {
      std::cerr << "Error: unable to read ICC profile from image " << imageId << ": "
                << profileResult.message << std::endl;
      heif_image_handle_release(imageHandle);
      heif_context_free(context);
      return 2;
    }

    ++profileCount;
    std::cout << "item=" << imageId
              << " colour_type=" << (colourType == heif_color_profile_type_rICC ? "rICC" : "prof")
              << " profile_bytes=" << profile.size();

    if (argc == 3) {
      const std::string path = outputPath(argv[2], imageId, colourType);
      if (!writeProfile(path, profile)) {
        std::cerr << "Error: unable to write extracted ICC profile: " << path << std::endl;
        heif_image_handle_release(imageHandle);
        heif_context_free(context);
        return 2;
      }
      std::cout << " output=" << path;
    }

    std::cout << " proflib_parse=" << parseProfile(profile) << std::endl;
    heif_image_handle_release(imageHandle);
  }

  std::cout << "icc_profiles=" << profileCount << std::endl;
  heif_context_free(context);
  return 0;
}
