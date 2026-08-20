// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: schema validation leaked every libxml2 object it created (#1999).
//
// CIccProfileXml::LoadXml takes an optional RelaxNG schema (iccFromXml -v). The
// validation block built three libxml2 objects -- an xmlRelaxNGParserCtxt, the
// xmlRelaxNG compiled from it, and an xmlRelaxNGValidCtxt -- and called none of
// xmlRelaxNGFreeParserCtxt / xmlRelaxNGFree / xmlRelaxNGFreeValidCtxt on any
// path out of the block. There were four bare "return false" exits and a
// fall-through, and none of them released anything, so the leak was not confined
// to an error case: validating successfully leaked all three as well. Measured
// with iccFromXml -v on Testing/Display/sRGB_D65_MAT.xml under ASan/LSan before
// the fix: 89,714 bytes in 1,094 allocations when validation passed, 8,821 bytes
// in 40 allocations when it failed. Passing no schema leaked nothing, which is
// what localizes it to this block.
//
// The same exits also walked away from the parsed document. icXmlReadFileBounded
// hands ownership of it to LoadXml (IccUtilXml.h) and the function's only
// xmlFreeDoc sits past the tail, after ParseXml. That one is NOT observable as a
// leak -- LSan reports it neither directly nor indirectly, and it stays quiet
// even with use_stacks=0:use_registers=0, so the block remains reachable from
// libxml2 -- but returning without releasing a document this function owns is
// still wrong, and the fix frees it. Do not expect this file to go red on that
// half; the RelaxNG objects above are what it pins.
//
// A sibling of the document half in CIccMpeXmlCalculator::ParseImport does leak
// and is pinned here too: an <Import> whose target parses but whose root element
// is not IccCalcImport was rejected with a bare return, dropping the DOM that had
// just been read. Measured at 6,306 bytes in 14 allocations on the fixture below.
//
// The pass/fail signal for the leaks is the LeakSanitizer at-exit scan, so this
// only goes red under an ASan build with detect_leaks=1 -- the CTest registration
// forces that on, because the CI tool-test job exports detect_leaks=0. The return
// value assertions below run everywhere and pin the behaviour the fix had to keep
// identical: the same accept/reject outcome at each of the four exits.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccXmlConfig.h"

#include <cstdio>
#include <string>

static int g_failures = 0;

// Each case gets its own CIccProfileXml so that a profile populated by a
// successful load is destroyed before the next case runs; a shared instance
// would blur which case any surviving allocation belonged to.
static void check(const char *szCase,
                  const char *szProfileXml,
                  const char *szSchema,
                  bool bExpectLoaded)
{
  CIccProfileXml profile;
  std::string reason;

  bool bLoaded = profile.LoadXml(szProfileXml, szSchema, &reason);

  if (bLoaded != bExpectLoaded) {
    printf("FAIL [%s]: LoadXml returned %s, expected %s (reason: %s)\n",
           szCase, bLoaded ? "true" : "false", bExpectLoaded ? "true" : "false",
           reason.c_str());
    g_failures++;
    return;
  }

  printf("ok   [%s]: LoadXml returned %s\n", szCase, bLoaded ? "true" : "false");
}

int main(int argc, char *argv[])
{
  if (argc < 6) {
    printf("usage: %s <profile.xml> <accept.rng> <reject.rng> <malformed.rng> "
           "<calc-import-main.xml>\n", argv[0]);
    return 1;
  }

  const char *szProfileXml   = argv[1];
  const char *szAcceptRng    = argv[2];
  const char *szRejectRng    = argv[3];
  const char *szMalformedRng = argv[4];
  const char *szCalcImport   = argv[5];

  // Same factory registration iccFromXml performs; without it the XML tag and
  // element handlers this test drives are not reachable.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  // The <Import> case below is gated on this the same way iccFromXml gates it.
  IccXmlSetAllowFileIncludes(true);

  // Validation succeeds. This is the path that leaked all three RelaxNG objects
  // while reporting nothing wrong, so it is the important one.
  check("relaxng-accept", szProfileXml, szAcceptRng, true);

  // Validation runs and rejects the document (xmlRelaxNGValidateDoc != 0).
  check("relaxng-reject", szProfileXml, szRejectRng, false);

  // The schema is well-formed XML but does not compile, so xmlRelaxNGParse
  // returns NULL and the parser context is the only object to release.
  check("relaxng-malformed", szProfileXml, szMalformedRng, false);

  // The schema file does not exist. xmlRelaxNGNewParserCtxt still returns a
  // context for a URL it cannot read, so this exercises the same exit as the
  // malformed case rather than the "no context at all" one; both are covered by
  // a single cleanup block after the fix.
  check("relaxng-missing", szProfileXml,
        "relaxng-this-file-does-not-exist.rng", false);

  // No schema at all -- the validation block is skipped entirely. Present as the
  // control that isolates the leaks above to that block.
  check("relaxng-none", szProfileXml, "", true);

  // CIccMpeXmlCalculator::ParseImport: <Import> target parses but its root
  // element is not IccCalcImport. Resolved relative to the working directory,
  // which the CTest registration points at the fixture directory.
  check("calc-import-badroot", szCalcImport, "", false);

  if (g_failures) {
    printf("%d case(s) regressed\n", g_failures);
    return 1;
  }

  printf("all cases passed\n");
  return 0;
}
