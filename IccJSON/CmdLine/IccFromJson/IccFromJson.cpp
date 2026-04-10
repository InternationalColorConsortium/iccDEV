// IccFromJson.cpp : Convert a JSON ICC profile to a binary ICC profile file.
//

#include <cstdio>
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccProfileJson.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "IccLibJSONVer.h"
#include <cstring>

#ifdef _WIN32
  #define ICC_STRICMP _stricmp
#else
  #include <strings.h>
  #define ICC_STRICMP strcasecmp
#endif

int main(int argc, char* argv[])
{
  if (argc <= 2) {
    printf("IccFromJson built with IccProfLib Version " ICCPROFLIBVER ", IccLibJSON Version " ICCLIBJSONVER "\n\n");
    printf("Usage: IccFromJson json_file saved_profile_file {-noid}\n");
    return 0;
  }

  CIccTagCreator::PushFactory(new(std::nothrow) CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new(std::nothrow) CIccMpeJsonFactory());

  CIccProfileJson profile;
  std::string reason;

  bool bNoId = false;
  for (int i = 3; i < argc; i++) {
    if (!ICC_STRICMP(argv[i], "-noid"))
      bNoId = true;
  }

  if (!profile.LoadJson(argv[1], &reason)) {
    printf("%s", reason.c_str());
    printf("Unable to Parse '%s'\n", argv[1]);
    return -1;
  }

  std::string valid_report;

  if (profile.Validate(valid_report) <= icValidateWarning) {
    int i;
    for (i = 0; i < 16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i < 16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed and saved correctly\n");
    }
    else {
      printf("Unable to save profile as '%s'\n", argv[2]);
      return -1;
    }
  }
  else {
    int i;
    for (i = 0; i < 16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i < 16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed. Profile is invalid, but saved correctly\n");
    }
    else {
      printf("Unable to save profile - profile is invalid!\n");
      return -1;
    }
    printf("%s", valid_report.c_str());
  }

  printf("\n");
  return 0;
}
