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
#include <cstdlib>

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

  // On stderr, like the XML twin: this returned EXIT_FAILURE already but answered
  // on stdout, so a caller that separates the streams saw nothing (#2384).
  if (!profile.LoadJson(argv[1], &reason)) {
    fprintf(stderr, "%s", reason.c_str());
    fprintf(stderr, "Unable to Parse '%s'\n", argv[1]);
    return EXIT_FAILURE;
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
      fprintf(stderr, "Unable to save profile as '%s'\n", argv[2]);
      return EXIT_FAILURE;
    }
  }
  else {
    // The same contract as the XML twin, changed in the same commit rather than
    // left to drift: an above-warning profile is still written, but the report
    // goes to stderr and the status is EXIT_FAILURE instead of EXIT_SUCCESS
    // (#2384).  See IccFromXml.cpp for the reasoning.  Fixing only one of the two
    // parsers is what makes a defect like this outlive its fix -- the JSON side
    // has its own control fixtures (#1901/#1902) and its own callers.
    int i;
    for (i = 0; i < 16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i < 16 ? icAlwaysWriteID : icVersionBasedID))) {
      fprintf(stderr, "Profile parsed. Profile is invalid, but saved correctly\n");
    }
    else {
      fprintf(stderr, "Unable to save profile - profile is invalid!\n");
      return EXIT_FAILURE;
    }
    fprintf(stderr, "%s", valid_report.c_str());
    fprintf(stderr, "\n");
    return EXIT_FAILURE;
  }

  printf("\n");
  return EXIT_SUCCESS;
}
