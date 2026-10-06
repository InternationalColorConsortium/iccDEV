// IccFromJson.cpp : Convert a JSON ICC profile to a binary ICC profile file.
//

#include <cstdio>
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccProfileJson.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "IccFileUtil.h"
#include "IccLibJSONVer.h"
#include <cstring>
#include <cstdlib>
#include <string>

#ifdef _WIN32
  #define ICC_STRICMP _stricmp
#else
  #include <strings.h>
  #define ICC_STRICMP strcasecmp
#endif

// The usage screen, shared by the help path and the incomplete-invocation path so
// the two cannot drift.  The STREAM separates them, as in iccFromXml (#2387): a help
// request prints to stdout and succeeds, a malformed invocation prints to stderr and
// fails.
static void Usage(FILE *out)
{
  fprintf(out, "IccFromJson built with IccProfLib Version " ICCPROFLIBVER ", IccLibJSON Version " ICCLIBJSONVER "\n\n");
  fprintf(out, "Usage: IccFromJson json_file saved_profile_file {-noid}\n");
  fprintf(out, "       IccFromJson -h | --help\n");
}

static bool isHelpRequest(const char *szArg)
{
  return !ICC_STRICMP(szArg, "-h") || !ICC_STRICMP(szArg, "--help") ||
         !ICC_STRICMP(szArg, "-help") || !ICC_STRICMP(szArg, "-?");
}

int main(int argc, char* argv[])
{
  // The same contract as iccFromXml (#2387), applied here for #2676: this tool
  // printed its usage and returned 0 for a bare invocation and for one missing
  // its output path, so `iccFromJson base.json` reported success to automation
  // while converting nothing.  Only the lone-argument form is a help request.
  if (argc == 2 && isHelpRequest(argv[1])) {
    Usage(stdout);
    return EXIT_SUCCESS;
  }

  if (argc <= 2) {
    Usage(stderr);
    fprintf(stderr, "\nError: an input JSON file and an output profile path are both required\n");
    return EXIT_FAILURE;
  }

  // Sanitized once here rather than at each print site; the paths handed to
  // LoadJson()/SaveIccProfile() stay untouched (#2406).
  std::string srcName = icSanitizeConsoleText(argv[1]);
  std::string dstName = icSanitizeConsoleText(argv[2]);

  CIccTagCreator::PushFactory(new(std::nothrow) CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new(std::nothrow) CIccMpeJsonFactory());

  CIccProfileJson profile;
  std::string reason;

  bool bNoId = false;
  for (int i = 3; i < argc; i++) {
    if (!ICC_STRICMP(argv[i], "-noid")) {
      bNoId = true;
    }
    else {
      // An unrecognised option used to be skipped in silence, so the "-no-id"
      // typo converted with the ID left in and reported success (#2676).  The
      // XML twin refuses it.
      Usage(stderr);
      fprintf(stderr, "Error: unrecognized option '%s'\n",
              icSanitizeConsoleText(argv[i]).c_str());
      return EXIT_FAILURE;
    }
  }

  // On stderr, like the XML twin: this returned EXIT_FAILURE already but answered
  // on stdout, so a caller that separates the streams saw nothing (#2384).
  if (!profile.LoadJson(argv[1], &reason)) {
    fprintf(stderr, "%s", reason.c_str());
    fprintf(stderr, "Unable to Parse '%s'\n", srcName.c_str());
    return EXIT_FAILURE;
  }

  // -noid has to mean the saved profile carries NO profile ID.  icNeverWriteID
  // alone does not achieve that: CIccProfile::Write() emits m_Header.profileID as
  // part of the header and the switch only decides whether CalcProfileID() then
  // recalculates it, so a document carrying a ProfileID kept it through -noid
  // (#2676).  Cleared here, as iccFromXml clears it (#2387).
  if (bNoId)
    memset(&profile.m_Header.profileID, 0, sizeof(profile.m_Header.profileID));

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
      fprintf(stderr, "Unable to save profile as '%s'\n", dstName.c_str());
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
