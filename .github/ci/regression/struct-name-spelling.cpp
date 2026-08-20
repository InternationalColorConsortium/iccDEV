// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: g_icStructNames published two struct names that matched neither
// their own siblings nor the name this library actually writes to JSON (#2028).
// It is the struct-name analogue of the g_icTagNameTable defect #2012/#2013/#2014
// corrected, and it was not covered by that sweep.
//
// IccStructFactory.cpp carried two signatures twice, wrong spelling first:
//
//   {icSigBRDFStruct,         "brdfTransfromStructure"},   // <-- "Transfrom"
//   {icSigColorantInfoStruct, "colorantInfoStruct"},       // <-- no "ure"
//   {icSigBRDFStruct,         "brfdfTransformStructure"},  // <-- "brfdf"
//   {icSigColorantInfoStruct, "colorantInfoStructure"},
//
// GetStructSigName returns the FIRST match, so entry one was published and entry
// three was reachable only on read -- a primary/alias split by position rather
// than by design. For both signatures the published entry was the wrong one.
//
// Unlike the tag-table case this was not merely a name iccDEV published to
// everyone else; for brdf it broke both serializations outright. Two unlinked
// sources name the same struct and they have to agree:
//
//   emit: ToJson / ToXml     -> pStruct->GetDisplayName()  (IccStructBasic.h)
//   read: ParseJson/ParseXml -> GetStructSig               (this table)
//
// GetDisplayName has always returned the correct "brdfTransformStructure", which
// matched neither brdf entry, so both writers emitted a string their own readers
// rejected. The two failed differently, and neither was caught:
//
//   JSON  CIccTagJsonStruct::ParseJson left sigStruct 0, never called
//         SetTagStructType and appended nothing to parseStr. The tag came back as
//         "privateStruct" with signature "NULL" -- silent data loss -- and
//         iccFromJson still printed "Profile parsed and saved correctly".
//   XML   CIccTagXmlStruct::ParseXml looks for a <StructureSignature> element on a
//         miss, a form only hand-authored documents carry and never what ToXml
//         writes for a named struct, so the fallback could not fire: "Unable to
//         find StructureSignature" and the whole document refused to load.
//
// Neither had a fixture, because there is no BRDF struct tag anywhere in Testing/
// in either format -- the three <StructureSignature>brdf references all sit inside
// commented-out examples. colorantInfoStruct is the milder case: not a typo, but
// it broke the ...Structure suffix its siblings share, and it read back fine
// either way, so it only ever mis-published a name -- via CIccTagStruct::Describe
// and hence iccDumpProfile.
//
// Why the fix needs a second half: renaming alone would make iccDEV unable to
// read JSON it had previously written. Both old spellings are therefore retained
// in g_icAltStructNames, which only GetStructSig consults -- read-only, so the
// legacy name keeps parsing and nothing emits it again. That asymmetry is the
// contract #2023 established, and it is what section 1-4 below pin.
//
// Section 5 is the one worth having independently of the rename: it asserts that
// the factory table and GetDisplayName agree for EVERY struct signature, not just
// the two corrected here. That invariant is what was broken, nothing else checks
// it, and it is the check that would have caught this defect when it was
// introduced. The controls matter as much as the assertions -- the sibling cases
// fail if a careless edit reworded a whole family, and the negative case fails if
// an unknown name started resolving to something.
//
// Exit code 0 = pass, 1 = a case regressed.
// <string> comes FIRST, before any iccDEV header. IccStructFactory.h declares four
// std::string parameters but includes only IccDefs.h, <memory> and <list>, so it is not
// self-contained: a translation unit that includes it before <string> is on its own. That
// is invisible with libstdc++, which supplies std::string transitively, and fatal on MSVC
// -- 'string': is not a member of 'std' (C2039), after which every GetStructSigName
// declaration is malformed and MSVC reports it as "does not take N arguments" for any N.
// The sibling test extended-device-colorspace.cpp only avoids this by accident of ordering:
// it includes IccTagBasic.h/IccTagComposite.h ahead of IccStructFactory.h. This file adds
// the missing include to the header as well, so the ordering no longer matters; the
// explicit <string> here keeps this test standing on its own regardless.
#include <string>

#include "IccStructFactory.h"
#include "IccStructBasic.h"
#include "IccTagComposite.h"  // CIccTagStruct, which owns the struct handler

#include <cstdio>
#include <cstring>

static int g_failures = 0;

// szOld is the name this library emitted before the fix; it must remain readable
// but must never be produced again. szNew is the corrected, published name.
// szOld2 covers icSigBRDFStruct, which was misspelled in both of its entries.
struct StructNameFix {
  icStructSignature sig;
  const char *szNew;
  const char *szOld;
  const char *szOld2;
};

static const StructNameFix kCorrected[] = {
  {icSigBRDFStruct, "brdfTransformStructure", "brdfTransfromStructure",
   "brfdfTransformStructure"},
  {icSigColorantInfoStruct, "colorantInfoStructure", "colorantInfoStruct", NULL},
};

// Names that must NOT have moved. Every one already agreed with GetDisplayName
// before the fix, so these guard against the correction having been applied more
// broadly than the two entries that needed it.
static const StructNameFix kUnchanged[] = {
  {icSigColorEncodingParamsSruct, "colorEncodingParamsStructure", NULL, NULL},
  {icSigMeasurementInfoStruct, "measurementInfoStructure", NULL, NULL},
  {icSigNamedColorStruct, "namedColorStructure", NULL, NULL},
  {icSigProfileConnectionConditionsStruct, "profileConnectionConditionsStructure",
   NULL, NULL},
  {icSigProfileInfoStruct, "profileInfoStructure", NULL, NULL},
  {icSigTintZeroStruct, "tintZeroStructure", NULL, NULL},
};

static void expectEmitted(const StructNameFix &fix)
{
  std::string sGot;

  // Two arguments, letting bFillUnknown default to true, matching every other caller in
  // the tree (IccTagComposite.cpp:359, IccUtil.cpp:1616,
  // extended-device-colorspace.cpp:103). The default is the behaviour wanted here: a
  // missing signature yields an empty string rather than a synthesised
  // "UnknownStruct_xxxx" that would compare unequal for the wrong reason. (Note the flag
  // reads backwards -- "Fill" true means do NOT fill -- pre-existing, not touched here.)
  if (!CIccStructCreator::GetStructSigName(sGot, fix.sig)) {
    printf("FAIL [emit %s]: GetStructSigName found no entry for 0x%08X\n",
           fix.szNew, (unsigned)fix.sig);
    g_failures++;
    return;
  }
  if (strcmp(sGot.c_str(), fix.szNew) != 0) {
    printf("FAIL [emit]: GetStructSigName gave \"%s\", expected \"%s\"\n",
           sGot.c_str(), fix.szNew);
    g_failures++;
    return;
  }
  printf("ok   [emit]: %s\n", sGot.c_str());
}

static void expectResolves(const char *szCase, const char *szName,
                           icStructSignature sigExpected)
{
  icStructSignature sigGot = CIccStructCreator::GetStructSig(szName);

  if (sigGot != sigExpected) {
    printf("FAIL [%s]: \"%s\" resolved to 0x%08X, expected 0x%08X\n",
           szCase, szName, (unsigned)sigGot, (unsigned)sigExpected);
    g_failures++;
    return;
  }
  printf("ok   [%s]: \"%s\" -> 0x%08X\n", szCase, szName, (unsigned)sigGot);
}

// A legacy name must stay readable without ever becoming the published name of the
// struct it resolves to. Resolving the alias and asking what that signature emits
// is the whole check: if the two ever agree, the entry has stopped being an alias.
// Going through the public API rather than walking the tables is deliberate --
// g_icStructNames and g_icAltStructNames are file-static, so no declaration here
// could reach them.
static void expectNotEmitted(const char *szAlias)
{
  icStructSignature sig = CIccStructCreator::GetStructSig(szAlias);
  std::string sEmitted;

  if (!CIccStructCreator::GetStructSigName(sEmitted, sig)) {
    printf("FAIL [alias not emitted]: \"%s\" resolved to 0x%08X, which publishes"
           " no name at all\n", szAlias, (unsigned)sig);
    g_failures++;
    return;
  }
  if (strcmp(sEmitted.c_str(), szAlias) == 0) {
    printf("FAIL [alias not emitted]: \"%s\" is the published name of 0x%08X\n",
           szAlias, (unsigned)sig);
    g_failures++;
    return;
  }
  printf("ok   [alias not emitted]: \"%s\" emits as \"%s\"\n",
         szAlias, sEmitted.c_str());
}

// The invariant the defect actually violated: the name the JSON writer emits must
// be the name the factory table publishes, and must read back to the signature it
// came from. GetDisplayName lives on the struct object, so a handler has to exist.
//
// The handler is obtained through a stack CIccTagStruct rather than by calling
// CIccStructCreator::CreateStruct directly, which matters on Windows and is why
// extended-device-colorspace.cpp:108 does the same. SetTagStructType allocates the
// handler inside the library (IccTagComposite.cpp:311) and the tag both replaces and
// destroys it there, so allocation and release stay on one side of the DLL boundary.
// Calling CreateStruct here and delete-ing the result in the test executable would
// free library-allocated memory against the test's own CRT heap -- undefined on a
// Windows shared build, and invisible on Linux where both sides share an allocator.
// GetStructHandler returns a borrowed pointer; the tag owns it.
static void expectDisplayNameAgrees(icStructSignature sig)
{
  CIccTagStruct tagStruct;

  if (!tagStruct.SetTagStructType(sig)) {
    printf("FAIL [display agrees]: no handler for 0x%08X\n", (unsigned)sig);
    g_failures++;
    return;
  }

  IIccStruct *pStruct = tagStruct.GetStructHandler();
  if (!pStruct) {
    printf("FAIL [display agrees]: handler for 0x%08X is NULL\n", (unsigned)sig);
    g_failures++;
    return;
  }

  const char *szDisplay = pStruct->GetDisplayName();
  std::string sTable;
  bool bFound = CIccStructCreator::GetStructSigName(sTable, sig);

  if (!bFound || !szDisplay || strcmp(sTable.c_str(), szDisplay) != 0) {
    printf("FAIL [display agrees]: 0x%08X publishes \"%s\" but writes \"%s\"\n",
           (unsigned)sig, bFound ? sTable.c_str() : "(none)",
           szDisplay ? szDisplay : "(null)");
    g_failures++;
    return;
  }

  // The round trip that ToJson/ParseJson perform, in the two calls they use.
  icStructSignature sigBack = CIccStructCreator::GetStructSig(szDisplay);
  if (sigBack != sig) {
    printf("FAIL [display round-trips]: \"%s\" read back as 0x%08X, expected"
           " 0x%08X\n", szDisplay, (unsigned)sigBack, (unsigned)sig);
    g_failures++;
    return;
  }

  printf("ok   [display agrees]: 0x%08X <-> \"%s\"\n", (unsigned)sig, szDisplay);
}

int main()
{
  const size_t nCorrected = sizeof(kCorrected) / sizeof(kCorrected[0]);
  const size_t nUnchanged = sizeof(kUnchanged) / sizeof(kUnchanged[0]);
  size_t i;

  // 1 -- the corrected name is what gets published, in JSON and in dumps.
  for (i = 0; i < nCorrected; i++)
    expectEmitted(kCorrected[i]);

  // 2 -- the corrected name reads back, so newly written JSON loads.
  for (i = 0; i < nCorrected; i++)
    expectResolves("new-name reads", kCorrected[i].szNew, kCorrected[i].sig);

  // 3 -- every legacy spelling still reads, so JSON this library wrote before the
  // fix keeps loading. brdf needs both: it was misspelled in each of its entries.
  for (i = 0; i < nCorrected; i++) {
    expectResolves("legacy alias reads", kCorrected[i].szOld, kCorrected[i].sig);
    if (kCorrected[i].szOld2)
      expectResolves("legacy alias reads", kCorrected[i].szOld2, kCorrected[i].sig);
  }

  // 4 -- no legacy spelling may be published again. This is what fails if the
  // alias entries drift back into the primary table, which is how they came to be
  // published in the first place.
  for (i = 0; i < nCorrected; i++) {
    expectNotEmitted(kCorrected[i].szOld);
    if (kCorrected[i].szOld2)
      expectNotEmitted(kCorrected[i].szOld2);
  }

  // 5 -- the invariant that was broken, asserted for every struct signature and
  // not just the two corrected: what the JSON writer emits is what the factory
  // publishes, and it reads back. Two unlinked sources, nothing else checks them.
  for (i = 0; i < nCorrected; i++)
    expectDisplayNameAgrees(kCorrected[i].sig);
  for (i = 0; i < nUnchanged; i++)
    expectDisplayNameAgrees(kUnchanged[i].sig);

  // 6 -- controls: neighbouring names must be exactly where they were.
  for (i = 0; i < nUnchanged; i++) {
    expectEmitted(kUnchanged[i]);
    expectResolves("sibling reads", kUnchanged[i].szNew, kUnchanged[i].sig);
  }

  // 7 -- a name in neither table must stay unresolved. Guards against the added
  // alias lookup degrading into something that answers for anything.
  expectResolves("unknown stays unknown", "notAStructNameStructure",
                 (icStructSignature)0);

  // 8 -- an empty name must not resolve either. DoGetStructSig short-circuits on
  // it before reaching the tables, and the empty terminator entry sits in both.
  expectResolves("empty stays unknown", "", (icStructSignature)0);

  if (g_failures) {
    printf("\n%d struct-name case(s) regressed\n", g_failures);
    return 1;
  }

  printf("\nall struct-name cases passed\n");
  return 0;
}
