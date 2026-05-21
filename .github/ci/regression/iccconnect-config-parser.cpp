/*
    File:       iccconnect-config-parser.cpp

    Contains:   CTest helper for IccConnect CLI/JSON configuration parsing.

    The helper verifies malformed command-line and JSON fields fail closed
    instead of being silently coerced by atoi/atof or skipped from nested arrays.

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccCmmConfig.h"

#include <cstdio>

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "iccconnect-config-parser: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "iccconnect-config-parser: FAIL  %s\n", label);
  return 1;
}

int main()
{
  int failures = 0;

  {
    CIccCfgProfileSequence profiles;
    const char* args[] = { "1", "src.icc", "1", "dst.icc", "10003" };
    failures += check(profiles.fromArgs(args, 5, true) == 5,
                      "profile sequence accepts valid legacy arguments");
  }

  {
    CIccCfgProfileSequence profiles;
    const char* args[] = { "2", "src.icc", "1" };
    failures += check(profiles.fromArgs(args, 3, true) == 0,
                      "profile sequence rejects invalid interpolation");
  }

  {
    CIccCfgProfileSequence profiles;
    const char* args[] = { "1", "-ENV:abcd", "nan", "src.icc", "1" };
    failures += check(profiles.fromArgs(args, 5, true) == 0,
                      "profile sequence rejects non-finite env value");
  }

  {
    CIccCfgProfileSequence profiles;
    const char* args[] = { "1", "src.icc", "3junk" };
    failures += check(profiles.fromArgs(args, 3, true) == 0,
                      "profile sequence rejects trailing intent junk");
  }

  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1", "pcc.icc", "1.25"
    };
    failures += check(search.fromArgs(args, 9, true) == 9,
                      "search apply accepts valid legacy arguments");
  }

  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1bad"
    };
    failures += check(search.fromArgs(args, 7, true) == 0,
                      "search apply rejects malformed init intent");
  }

  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1", "pcc.icc", "nan"
    };
    failures += check(search.fromArgs(args, 9, true) == 0,
                      "search apply rejects non-finite PCC weight");
  }

  {
    CIccCfgProfile profile;
    failures += check(!profile.fromJson("{\"iccFile\":\"profile.icc\",\"transform\":\"missing\"}", true),
                      "profile JSON rejects unknown transform");
  }

  {
    CIccCfgProfile profile;
    failures += check(!profile.fromJson("{\"iccFile\":\"profile.icc\",\"interpolation\":\"cubic\"}", true),
                      "profile JSON rejects unknown interpolation");
  }

  {
    CIccCfgSearchApply search;
    const char* configJson =
      "{\"profileSequence\":[{\"iccFile\":\"a.icc\",\"intent\":\"relative\"}],"
      "\"initial\":{\"intent\":\"relative\",\"transform\":\"missing\"}}";
    failures += check(!search.fromJson(configJson, true),
                      "search JSON rejects unknown initial transform");
  }

  {
    CIccCfgSearchApply search;
    const char* configJson =
      "{\"profileSequence\":[{\"iccFile\":\"a.icc\",\"transform\":\"missing\"}],"
      "\"initial\":{\"intent\":\"relative\"}}";
    failures += check(!search.fromJson(configJson, true),
                      "search JSON rejects invalid nested profile");
  }

  {
    CIccCfgSearchApply search;
    const char* configJson =
      "{\"profileSequence\":[{\"iccFile\":\"a.icc\",\"intent\":\"relative\"}],"
      "\"initial\":{\"intent\":\"relative\"},"
      "\"pccWeights\":[{\"pccFile\":\"pcc.icc\"}]}";
    failures += check(!search.fromJson(configJson, true),
                      "search JSON rejects incomplete PCC weight");
  }

  return failures ? 1 : 0;
}
