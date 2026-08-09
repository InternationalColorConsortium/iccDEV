/*
 * Copyright (c) 2026 International Color Consortium.
 *                 All rights reserved.
 *                 https://color.org
 *
 * This source file is licensed under the BSD 3-Clause "New" or "Revised"
 * License used by ICC software projects.
 */

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <new>
#include <string>

#include "IccIO.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"

namespace {

std::unique_ptr<CIccProfile> OpenProfile(const uint8_t *data, size_t size,
                                         uint8_t mode,
                                         std::string &report) {
  const icUInt32Number icc_size = static_cast<icUInt32Number>(size);

  switch (mode) {
    case 0:
      return std::unique_ptr<CIccProfile>(
          OpenIccProfile(data, icc_size, false));
    case 1:
      return std::unique_ptr<CIccProfile>(
          OpenIccProfile(data, icc_size, true));
    case 2:
      return std::unique_ptr<CIccProfile>(
          ReadIccProfile(data, icc_size, false));
    default: {
      CIccMemIO *io = new (std::nothrow) CIccMemIO();
      if (!io)
        return nullptr;
      if (!io->Attach(const_cast<icUInt8Number *>(data), size, false)) {
        delete io;
        return nullptr;
      }

      icValidateStatus status = icValidateOK;
      return std::unique_ptr<CIccProfile>(
          ValidateIccProfile(io, report, status));
    }
  }
}

void ExerciseProfile(CIccProfile &profile, uint8_t mode,
                     std::string &report) {
  profile.Validate(report);

  const int verbosity = static_cast<int>(mode) * 34;
  for (IccTagEntry &entry : profile.m_Tags) {
    CIccTag *tag = profile.FindTag(entry);
    if (!tag)
      continue;

    std::string description;
    std::string tag_report;
    tag->Describe(description, verbosity);
    tag->Validate(icGetSigPath(entry.TagInfo.sig), tag_report, &profile);
    (void)tag->GetType();
    (void)tag->IsArrayType();
    (void)tag->IsSupported();
    (void)tag->GetTagArrayType();
    (void)tag->GetTagStructType();

    std::unique_ptr<CIccMemIO> tag_io(profile.GetTagIO(entry.TagInfo.sig));
    if (tag_io) {
      icUInt8Number byte = 0;
      (void)tag_io->Read8(&byte, 1);
    }
  }

  (void)profile.GetSpaceSamples();
  (void)profile.GetParentSpaceSamples();
  (void)profile.GetParentColorSpace();
  (void)profile.AreTagsUnique();
  (void)profile.ReadPccTags();
  (void)profile.getPccViewingConditions();
  (void)profile.getCustomToStandardPcc();
  (void)profile.getStandardToCustomPcc();
  (void)profile.getPccIlluminant();
  (void)profile.getPccCCT();
  (void)profile.getPccObserver();

  icFloatNumber xyz[3] = {};
  profile.getNormIlluminantXYZ(xyz);
  profile.getLumIlluminantXYZ(xyz);
  (void)profile.getMediaWhiteXYZ(xyz);
  (void)profile.calcNormIlluminantXYZ(xyz, &profile);
  (void)profile.calcLumIlluminantXYZ(xyz, &profile);
  (void)profile.calcMediaWhiteXYZ(xyz, &profile);

  CIccProfile copy(profile);
  CIccNullIO output;
  output.Open();
  (void)copy.Write(&output,
                   static_cast<icProfileIDSaveMethod>(mode % 3));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size > std::numeric_limits<icUInt32Number>::max())
    return 0;

  const uint8_t mode = size ? data[size - 1] & 3 : 0;
  std::string report;
  std::unique_ptr<CIccProfile> profile =
      OpenProfile(data, size, mode, report);
  if (profile)
    ExerciseProfile(*profile, mode, report);

  return 0;
}
