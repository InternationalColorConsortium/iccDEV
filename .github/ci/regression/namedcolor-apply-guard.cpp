#include "IccCmm.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <cstring>

static int expect_status(const char *name, icStatusCMM got, icStatusCMM want)
{
  if (got != want) {
    std::printf("%s: got %d, expected %d\n", name, got, want);
    return 1;
  }
  std::printf("%s: status %d\n", name, got);
  return 0;
}

static void init_named_entry(CIccTagNamedColor2 &tag, const char *rootName)
{
  SIccNamedColorEntry *entry = tag.GetEntry(0);
  std::snprintf(entry->rootName, sizeof(entry->rootName), "%s", rootName);

  for (int i = 0; i < 3; ++i) {
    entry->pcsCoords[i] = 0.0f;
  }

  for (icUInt32Number i = 0; i < tag.GetDeviceCoords(); ++i) {
    entry->deviceCoords[i] = 0.05f * static_cast<icFloatNumber>(i + 1);
  }
}

static icStatusCMM run_pixel_to_name(CIccTagNamedColor2 &tag,
                                     icColorSpaceSignature deviceSpace,
                                     icChar *name,
                                     const icFloatNumber *src)
{
  CIccXformNamedColor xform(&tag, icSigLabData, deviceSpace);
  icStatusCMM status = xform.SetSrcSpace(deviceSpace);
  if (status != icCmmStatOk) {
    return status;
  }

  status = xform.SetDestSpace(icSigNamedData);
  if (status != icCmmStatOk) {
    return status;
  }

  return xform.Apply(NULL, name, src);
}

int main()
{
  icFloatNumber src[17] = {};
  icChar name[256] = {};
  int failures = 0;

  CIccTagNamedColor2 valid(1, 3);
  init_named_entry(valid, "valid-device-color");
  failures += expect_status("valid-device-color",
                            run_pixel_to_name(valid, icSigRgbData, name, src),
                            icCmmStatOk);
  if (std::strcmp(name, "valid-device-color")) {
    std::printf("valid-device-color: unexpected name '%s'\n", name);
    failures++;
  }

  CIccTagNamedColor2 noDeviceCoords(1, 0);
  init_named_entry(noDeviceCoords, "no-device-coordinates");
  failures += expect_status("no-device-coordinates",
                            run_pixel_to_name(noDeviceCoords, icSigRgbData, name, src),
                            icCmmStatColorNotFound);

  CIccTagNamedColor2 tooManyDeviceCoords(1, 17);
  init_named_entry(tooManyDeviceCoords, "too-many-device-coordinates");
  failures += expect_status("too-many-device-coordinates",
                            run_pixel_to_name(tooManyDeviceCoords, icSigRgbData, name, src),
                            icCmmStatTooManySamples);

  return failures ? 1 : 0;
}
