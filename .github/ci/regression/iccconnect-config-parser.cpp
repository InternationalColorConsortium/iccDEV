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

  // #1860: -INIT was matched with stricmp where the profile loop breaks out of
  // the sequence but with strcmp where the initializer is consumed.  A lowercase
  // "-init" therefore ended the profile loop and then fell through to the
  // weighted-PCC loop, where it was taken as a PCC path and its intent as the
  // weight.  Both tests are stricmp now, so either spelling parses identically.
  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-init", "1"
    };
    failures += check(search.fromArgs(args, 7, true) == 7,
                      "search apply accepts lowercase -init");
  }

  {
    CIccCfgSearchApply search;
    const char* argsUpper[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1"
    };
    CIccCfgSearchApply searchLower;
    const char* argsLower[] = {
      "1", "target.icc", "1", "src.icc", "101", "-init", "1"
    };
    failures += check(search.fromArgs(argsUpper, 7, true) ==
                        searchLower.fromArgs(argsLower, 7, true) &&
                      search.isInitialized() && searchLower.isInitialized(),
                      "search apply treats -INIT and -init identically");
  }

  // #1860: every loop in CIccCfgSearchApply::fromArgs consumes tokens in pairs,
  // so a leftover odd token is one the caller wrote and the parser never read.
  // It used to be dropped silently and the run exited 0 as if fully honoured.
  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1", "TRAILING"
    };
    failures += check(search.fromArgs(args, 8, true) == 0,
                      "search apply rejects unconsumed trailing argument");
  }

  {
    CIccCfgSearchApply search;
    const char* args[] = {
      "1", "target.icc", "1", "src.icc", "101", "-INIT", "1", "pcc.icc"
    };
    failures += check(search.fromArgs(args, 8, true) == 0,
                      "search apply rejects PCC path with no weight");
  }

  // #1860: the digits field of encoding[:precision[:digits]] was read with the
  // cursor still on its ':' separator, so atoi(":8") silently yielded 0 and the
  // requested total width was discarded.
  {
    CIccCfgDataApply data;
    const char* args[] = { "data.txt", "0:2:8" };
    failures += check(data.fromArgs(args, 2, true) == 2 &&
                        data.m_dstPrecision == 2 && data.m_dstDigits == 8,
                      "data apply decodes encoding:precision:digits");
  }

  {
    CIccCfgDataApply data;
    const char* args[] = { "data.txt", "3:6" };
    failures += check(data.fromArgs(args, 2, true) == 2 &&
                        data.m_dstPrecision == 6 && data.m_dstDigits == 11,
                      "data apply defaults digits to 5 + precision");
  }

  // #1860: both fields used bare atoi(), which cannot fail -- malformed text
  // became 0 and an out-of-range value was truncated by the icUInt8Number cast.
  {
    CIccCfgDataApply data;
    const char* args[] = { "data.txt", "0:x:y" };
    failures += check(data.fromArgs(args, 2, true) == 0,
                      "data apply rejects malformed precision and digits");
  }

  {
    CIccCfgDataApply data;
    const char* args[] = { "data.txt", "0:2:999" };
    failures += check(data.fromArgs(args, 2, true) == 0,
                      "data apply rejects out-of-range digits");
  }

  // #1996: the destination sample encoding is a positional selector where 0 means
  // "same as source" and 1..3 are 8 bit / 16 bit / float.  All four must survive
  // the round trip, because the selector numbering is what the usage text quotes.
  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "0", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 6 &&
                        image.m_dstEncoding == icEncodeUnknown,
                      "image apply maps encoding 0 to same-as-source");
  }

  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "1", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 6 &&
                        image.m_dstEncoding == icEncode8Bit,
                      "image apply maps encoding 1 to 8 bit");
  }

  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "2", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 6 &&
                        image.m_dstEncoding == icEncode16Bit,
                      "image apply maps encoding 2 to 16 bit");
  }

  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "3", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 6 &&
                        image.m_dstEncoding == icEncodeFloat,
                      "image apply maps encoding 3 to float");
  }

  // #1996: the switch shared its default label with case 1, so 4 -- the value the
  // usage text wrongly advertised for float -- was accepted as 8 bit and the run
  // exited 0.  This is the case that fails on the unfixed parser: it returned 6
  // with m_dstEncoding == icEncode8Bit rather than refusing the argument.
  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "4", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 0,
                      "image apply rejects out-of-range encoding selector");
  }

  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "3junk", "1", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 0,
                      "image apply rejects trailing encoding junk");
  }

  // #1996: the three flags were read with bare atoi() as well, so a typo parsed
  // as 0 and was indistinguishable from an explicit "off".  Any non-zero integer
  // still means true, so only unparseable text is refused.
  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "1", "yes", "0", "1" };
    failures += check(image.fromArgs(args, 6, true) == 0,
                      "image apply rejects non-numeric compression flag");
  }

  {
    CIccCfgImageApply image;
    const char* args[] = { "src.tif", "dst.tif", "1", "1", "0", "" };
    failures += check(image.fromArgs(args, 6, true) == 0,
                      "image apply rejects empty embed-icc flag");
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
