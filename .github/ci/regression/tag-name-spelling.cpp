// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: g_icTagNameTable published ten tag names that matched neither the
// signature nor their own siblings (#2012, #2013, #2014).
//
// IccTagFactory.cpp carried, among correctly-named neighbours:
//
//   {icSigBRDFAToB0Tag, "BRDFAToB0Tag"},   // ICC.2-2023 9.2.14-17 -> brdfAToB0Tag
//   {icSigBRDFDToB0Tag, "BRDFDToB0Tag"},   // ICC.2-2023 9.2.26-29 -> brdfDToB0Tag
//   {icSigHToS2Tag,     "HTSB2Tag"},       // siblings are HToS0/1/3Tag
//   {icSigCxFTag,       "CxfTag"},         // ICC.2-2023 9.2.58    -> cxfTag
//
// This is not confined to human-readable dumps. g_icTagNameTable is the single
// source for BOTH directions of the mapping and drives the XML element name and
// the JSON key: IccProfileXml.cpp:259 and IccProfileJson.cpp:220 write through
// GetTagSigName, IccProfileXml.cpp:743 and IccProfileJson.cpp:450 read through
// GetTagNameSig. Because both directions consulted the same wrong entry, the old
// behaviour was self-consistent -- a profile round-tripped fine. What was wrong
// is the name iccDEV published to everyone else.
//
// The uppercase BRDF entries were internally inconsistent quite apart from the
// specification: the brdfMToB/brdfMToS entries immediately above them in the same
// table already map an uppercase icSigBRDFM* enum to a lowercase name, so the
// enum spelling was never the thing being echoed.
//
// HToS2 is the opposite case and is deliberately NOT lowercased: that family is
// uppercase by design, like AToB/BToA/MToB, so the correction is HToS2Tag.
//
// Why the fix needs a second half: renaming alone would make iccDEV unable to
// read any XML or JSON it had previously written for these ten tags. Each old
// name is therefore retained in g_icAltTagNameTable, which only GetTagNameSig
// consults -- read-only, so the legacy name keeps parsing and nothing emits it
// again. That asymmetry is the whole contract, and it is what this test pins:
// for every corrected tag, the NEW name must be what is emitted, and BOTH names
// must still resolve back to the signature.
//
// The controls matter as much as the assertions. The sibling and pre-existing
// alias cases below fail if a careless edit lowercased a whole family or turned
// the alias table into a catch-all, and the negative case fails if an unknown
// name started resolving to something.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccTagFactory.h"

#include <cstdio>
#include <cstring>

static int g_failures = 0;

// szOld is the name this library emitted before the fix; it must remain readable
// but must never be produced again. szNew is the corrected, published name.
struct TagNameFix {
  icTagSignature sig;
  const char *szNew;
  const char *szOld;
  const char *szIssue;
};

static const TagNameFix kCorrected[] = {
  {icSigBRDFAToB0Tag, "brdfAToB0Tag", "BRDFAToB0Tag", "#2013"},
  {icSigBRDFAToB1Tag, "brdfAToB1Tag", "BRDFAToB1Tag", "#2013"},
  {icSigBRDFAToB2Tag, "brdfAToB2Tag", "BRDFAToB2Tag", "#2013"},
  {icSigBRDFAToB3Tag, "brdfAToB3Tag", "BRDFAToB3Tag", "#2013"},
  {icSigBRDFDToB0Tag, "brdfDToB0Tag", "BRDFDToB0Tag", "#2013"},
  {icSigBRDFDToB1Tag, "brdfDToB1Tag", "BRDFDToB1Tag", "#2013"},
  {icSigBRDFDToB2Tag, "brdfDToB2Tag", "BRDFDToB2Tag", "#2013"},
  {icSigBRDFDToB3Tag, "brdfDToB3Tag", "BRDFDToB3Tag", "#2013"},
  {icSigHToS2Tag,     "HToS2Tag",     "HTSB2Tag",     "#2012"},
  {icSigCxFTag,       "cxfTag",       "CxfTag",       "#2014"},
};

// Names that must NOT have moved. The brdfM* pair guards against lowercasing
// having been applied to the whole BRDF block; the HToS siblings guard against
// HToS2 having been "corrected" downwards into lowercase along with it.
static const TagNameFix kUnchanged[] = {
  {icSigBRDFMToB0Tag, "brdfMToB0Tag", NULL, "sibling"},
  {icSigBRDFMToS0Tag, "brdfMToS0Tag", NULL, "sibling"},
  {icSigHToS0Tag,     "HToS0Tag",     NULL, "sibling"},
  {icSigHToS1Tag,     "HToS1Tag",     NULL, "sibling"},
  {icSigHToS3Tag,     "HToS3Tag",     NULL, "sibling"},
  {icSigCicpTag,      "cicpTag",      NULL, "sibling"},
};

static void expectEmitted(const TagNameFix &fix)
{
  const icChar *szGot = CIccTagCreator::GetTagSigName(fix.sig);

  if (!szGot) {
    printf("FAIL [%s emit %s]: GetTagSigName returned NULL\n",
           fix.szIssue, fix.szNew);
    g_failures++;
    return;
  }
  if (strcmp(szGot, fix.szNew) != 0) {
    printf("FAIL [%s emit]: GetTagSigName gave \"%s\", expected \"%s\"\n",
           fix.szIssue, szGot, fix.szNew);
    g_failures++;
    return;
  }
  printf("ok   [%s emit]: %s\n", fix.szIssue, szGot);
}

// A legacy name must stay readable without ever becoming the published name of the
// tag it resolves to. Resolving the alias and asking what that signature emits is
// the whole check: if the two ever agree, the entry has stopped being an alias.
static void expectNotEmitted(const char *szAlias)
{
  icTagSignature sig = CIccTagCreator::GetTagNameSig(szAlias);
  const icChar *szEmitted = CIccTagCreator::GetTagSigName(sig);

  if (szEmitted && strcmp(szEmitted, szAlias) == 0) {
    printf("FAIL [alias not emitted]: \"%s\" is the published name of 0x%08X\n",
           szAlias, (unsigned)sig);
    g_failures++;
    return;
  }
  printf("ok   [alias not emitted]: \"%s\" emits as \"%s\"\n",
         szAlias, szEmitted ? szEmitted : "(none)");
}

static void expectResolves(const char *szCase, const char *szName,
                           icTagSignature sigExpected)
{
  icTagSignature sigGot = CIccTagCreator::GetTagNameSig(szName);

  if (sigGot != sigExpected) {
    printf("FAIL [%s]: \"%s\" resolved to 0x%08X, expected 0x%08X\n",
           szCase, szName, (unsigned)sigGot, (unsigned)sigExpected);
    g_failures++;
    return;
  }
  printf("ok   [%s]: \"%s\" -> 0x%08X\n", szCase, szName, (unsigned)sigGot);
}

int main()
{
  const size_t nCorrected = sizeof(kCorrected) / sizeof(kCorrected[0]);
  const size_t nUnchanged = sizeof(kUnchanged) / sizeof(kUnchanged[0]);

  // 1 -- the corrected name is what gets published, in XML, JSON and dumps.
  for (size_t i = 0; i < nCorrected; i++)
    expectEmitted(kCorrected[i]);

  // 2 -- the corrected name reads back, so newly written documents load.
  for (size_t i = 0; i < nCorrected; i++)
    expectResolves("new-name reads", kCorrected[i].szNew, kCorrected[i].sig);

  // 3 -- the legacy name still reads, so documents this library wrote before the
  // fix -- including .github/ci/test-data/zxml-plaintext-1853.xml, which carries
  // a literal <CxfTag> element -- keep loading.
  for (size_t i = 0; i < nCorrected; i++)
    expectResolves("legacy alias reads", kCorrected[i].szOld, kCorrected[i].sig);

  // 4 -- controls: neighbouring names must be exactly where they were.
  for (size_t i = 0; i < nUnchanged; i++) {
    expectEmitted(kUnchanged[i]);
    expectResolves("sibling reads", kUnchanged[i].szNew, kUnchanged[i].sig);
  }

  // 5 -- the alias table predates this fix and must still work; if adding ten
  // entries had disturbed it, this is what notices.
  expectResolves("pre-existing alias", "materialTypeArrayTag",
                 icSigMultiplexTypeArrayTag);
  expectResolves("pre-existing alias", "materialDefaultValuesTag",
                 icSigMultiplexDefaultValuesTag);

  // 6 -- a name in neither table must still be unknown. Guards against the alias
  // lookup degrading into something that answers for anything.
  expectResolves("unknown stays unknown", "notATagNameTag", icSigUnknownTag);

  // 7 -- no alias may also be the published name of a tag. GetTagNameSig searches
  // the published names first and the legacy names only on a miss, so a legacy
  // entry that collides with a live name is unreachable: the lookup answers with
  // the published tag and the alias silently does nothing. That is the failure
  // this asserts against, and it is the reason the two tables are searched in
  // sequence rather than merged into one map. Checking it through the public API
  // -- rather than by walking the tables -- is deliberate: g_icTagNameTable and
  // g_icAltTagNameTable do have external linkage, but they appear in no public
  // header and carry no ICCPROFLIB_API marking, so they are not exported from the
  // shared library and an extern declaration here would fail to link on MSVC.
  for (size_t i = 0; i < nCorrected; i++)
    expectNotEmitted(kCorrected[i].szOld);
  expectNotEmitted("materialTypeArrayTag");
  expectNotEmitted("materialDefaultValuesTag");

  if (g_failures) {
    printf("\n%d tag-name case(s) regressed\n", g_failures);
    return 1;
  }

  printf("\nall tag-name cases passed\n");
  return 0;
}
