// Regression for #2387 findings 1 and 2: CIccProfileXml::ParseBasic must keep every
// digit of a multi-component version, and must encode the sub-class version as BCD.
//
// This is the XML twin of .github/ci/regression/json-bcd-version-parse.cpp (#1830).
//
// ParseBasic walks "<major>.<minor>.<classMajor>.<classMinor>" one component at a
// time. 889db62b repaired the first two loops from
//
//     ver = *szVer;      // assigns -- each pass discards the previous digit
// to
//     ver += *szVer;     // accumulates
//
// but left the third and fourth loops, and both loops of the separate
// <ProfileSubClassVersion> element, in the assigning form. Only the last digit of
// each of those components therefore reached the parser, so a document asking for
// 5.10.12.34 was written to header bytes 8..11 as 05 10 02 04 -- and iccFromXml
// still reported "Profile parsed and saved correctly".
//
// The <ProfileSubClassVersion> block carried two further defects that only cancel
// out together with the first:
//
//   * its major loop never stepped over the separator, so the minor loop ran zero
//     times and atoi("") supplied 0 -- "12.34" became 2.00, not 2.04; and
//   * it used atoi() directly, storing a raw decimal where bytes 10..11 hold BCD.
//
// That last one makes a partial repair worse than no repair. Accumulating the digits
// but keeping atoi() puts "12.34" in the header as 0x0C22, which
// CIccInfo::GetSubClassVersionName rejects outright:
//
//     SubClass Version:   Invalid BCD subclass version 0x0C22
//
// where the unrepaired code at least produced the valid-but-wrong 0x0200. Hence the
// explicit BCD assertions below: they fail for the atoi() form even once the digits
// accumulate correctly. Both components now go through parseVersion, the strict
// helper the <ProfileVersion> path already used, which is what
// icJsonParseBCDVersionStr does on the JSON side.
//
// The test drives the public CIccProfileXml::ParseXml so it exercises the real
// header path rather than the file-static helper.
//
// Red-green: restoring "ver = *szVer" in either pair of loops fails the
// four-component and sub-class cases; keeping atoi() in place of parseVersion fails
// the BCD assertions.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfileXml.h"
#include "IccUtil.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[xml-version] FAIL: %s\n", what);
  }
}

// Build the smallest document ParseXml accepts -- it requires an <IccProfile> root
// carrying both a <Header> and a <Tags> element -- with the supplied version
// elements, parse it, and hand back the resulting packed header version.
// bParsed/parseStr report what ParseBasic told the caller, which is how iccFromXml
// decides whether to print a reason.
icUInt32Number parseVersion(const char *szVersion, const char *szSubClassVersion,
                            bool *pParsed = NULL, std::string *pParseStr = NULL)
{
  std::string doc = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<IccProfile>\n  <Header>\n";

  if (szVersion) {
    doc += "    <ProfileVersion>";
    doc += szVersion;
    doc += "</ProfileVersion>\n";
  }
  if (szSubClassVersion) {
    doc += "    <ProfileSubClassVersion>";
    doc += szSubClassVersion;
    doc += "</ProfileSubClassVersion>\n";
  }
  doc += "  </Header>\n  <Tags/>\n</IccProfile>\n";

  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "version.xml", NULL, 0);
  if (!pDoc) {
    ++g_fail;
    std::fprintf(stderr, "[xml-version] FAIL: libxml2 could not parse the test document\n");
    return 0;
  }

  CIccProfileXml profile;
  std::string parseStr;
  bool bParsed = profile.ParseXml(xmlDocGetRootElement(pDoc), parseStr);

  if (pParsed)
    *pParsed = bParsed;
  if (pParseStr)
    *pParseStr = parseStr;

  icUInt32Number version = profile.m_Header.version;
  xmlFreeDoc(pDoc);
  return version;
}

void checkVersion(const char *szVersion, icUInt32Number expected, const char *what)
{
  icUInt32Number got = parseVersion(szVersion, NULL);
  if (got != expected) {
    ++g_fail;
    std::fprintf(stderr,
                 "[xml-version] FAIL: %s -- \"%s\" gave 0x%08X, expected 0x%08X\n",
                 what, szVersion ? szVersion : "(none)",
                 (unsigned int)got, (unsigned int)expected);
  }
}

void checkSubClass(const char *szSubClassVersion, icUInt32Number expectedLow,
                   const char *what)
{
  icUInt32Number got = parseVersion("5.10", szSubClassVersion);
  if ((got & 0x0000ffffu) != expectedLow) {
    ++g_fail;
    std::fprintf(stderr,
                 "[xml-version] FAIL: %s -- \"%s\" gave low half 0x%04X, expected 0x%04X\n",
                 what, szSubClassVersion ? szSubClassVersion : "(none)",
                 (unsigned int)(got & 0x0000ffffu), (unsigned int)expectedLow);
  }
  // The <ProfileVersion> half must be untouched by the sub-class element, which
  // shares the same header word.
  if ((got & 0xffff0000u) != 0x05100000u) {
    ++g_fail;
    std::fprintf(stderr,
                 "[xml-version] FAIL: %s -- sub-class element disturbed the version half (0x%08X)\n",
                 what, (unsigned int)got);
  }
}

} // namespace

int main()
{
  // --- two-component versions are unchanged ------------------------------------
  // CIccInfo::GetVersionName writes "%.2lf", so "4.30" is the canonical round-trip
  // form. These pin the repair as behaviour-neutral for input that already parsed.

  checkVersion("4.30", 0x04300000u, "\"4.30\" encodes as 0x0430");
  checkVersion("5.10", 0x05100000u, "\"5.10\" encodes as 0x0510");
  checkVersion("2.20", 0x02200000u, "\"2.20\" encodes as 0x0220");

  // A missing minor component keeps its previous meaning ("4" == "4.0"), and a
  // single-digit minor keeps its previous encoding: parseVersion maps 3 to 0x03
  // rather than 0x30, matching icJsonParseBCDByte on the JSON side.
  checkVersion("4", 0x04000000u, "\"4\" encodes as 0x0400");
  checkVersion("4.3", 0x04030000u, "\"4.3\" encodes as 0x0403 (unchanged quirk)");

  // --- finding 1: the third and fourth components lost their leading digit ------
  // The issue's own fixture. Before the repair this gave 0x05100204.

  checkVersion("5.10.12.34", 0x05101234u,
               "four-component version keeps every digit (#2387 finding 1)");
  checkVersion("5.10.12", 0x05101200u,
               "three-component version keeps the class major's leading digit");
  checkVersion("5.10.1.2", 0x05100102u,
               "single-digit class components are unaffected");

  // The strictness of the shared helper reaches the later components too: a
  // malformed one rejects the whole version rather than landing partly parsed.
  checkVersion("5.10.99.99", 0x05109999u, "the BCD ceiling of 99 is accepted");
  checkVersion("5.10.100.0", 0u, "a three-digit class major rejects the version");
  checkVersion("5.10.12.-1", 0u, "a negative class minor rejects the version");
  checkVersion("5.10.ab.34", 0u, "a non-numeric class major rejects the version");

  // --- finding 2: the explicit <ProfileSubClassVersion> element -----------------
  // Before the repair "12.34" gave 0x0200: the major loop kept only '2', and
  // because it never stepped over the '.', the minor loop ran zero times.

  checkSubClass("12.34", 0x1234u,
                "explicit sub-class version keeps both components (#2387 finding 2)");
  checkSubClass("1.20", 0x0120u, "single-digit sub-class major is unaffected");
  checkSubClass("12", 0x1200u, "a missing sub-class minor stays 0 (\"12\" == \"12.00\")");

  // These are the assertions a digits-only repair fails. atoi() would store "12" as
  // decimal 12 == 0x0C and "34" as 0x22, giving 0x0C22 -- a value
  // GetSubClassVersionName reports as "Invalid BCD subclass version".
  {
    icUInt32Number v = parseVersion("5.10", "12.34");
    check(((v >> 8) & 0xf0u) == 0x10u && ((v >> 8) & 0x0fu) == 0x02u,
          "sub-class major is BCD 0x12, not decimal 0x0C");
    check((v & 0xf0u) == 0x30u && (v & 0x0fu) == 0x04u,
          "sub-class minor is BCD 0x34, not decimal 0x22");

    // The user-visible consequence, asserted through the reader the writer uses.
    CIccInfo info;
    const char *szName = info.GetSubClassVersionName(v);
    check(szName && !std::strstr(szName, "Invalid"),
          "GetSubClassVersionName accepts the encoded sub-class version");
    check(szName && !std::strcmp(szName, "12.34"),
          "GetSubClassVersionName renders the sub-class version back as \"12.34\"");
  }

  // The sub-class element now shares <ProfileVersion>'s strictness. Each of these
  // previously reached the header as a silent 0 via atoi() with no diagnostic.
  checkSubClass("-1", 0u, "a negative sub-class version is rejected");
  checkSubClass("999", 0u, "a three-digit sub-class major is rejected");
  checkSubClass("abc", 0u, "a non-numeric sub-class version is rejected");
  checkSubClass("", 0u, "an empty sub-class version is rejected");

  // --- whitespace around a component ------------------------------------------
  // A component carries the element's literal text, so a pretty-printed or
  // hand-edited document indents it or leaves a trailing space. icXmlParseU32
  // requires the whole string to be consumed and strtoull skips leading blanks but
  // not trailing ones, so "5.10 " reached the header as 0 -- true of
  // <ProfileVersion> since #1845, and routing <ProfileSubClassVersion> through the
  // same helper would have spread it to a field whose atoi() had tolerated it.
  // parseVersion now trims, so both elements accept surrounding whitespace.

  checkVersion("5.10 ", 0x05100000u, "a trailing space does not zero the version");
  checkVersion(" 5.10", 0x05100000u, "a leading space does not zero the version");
  checkVersion("\n    5.10\n  ", 0x05100000u,
               "an indented, pretty-printed version parses");
  checkVersion("5 . 10", 0x05100000u, "whitespace around the separator is trimmed");
  checkVersion("5.10.12.34 ", 0x05101234u,
               "a trailing space on the four-component form is trimmed");
  checkSubClass("2.00 ", 0x0200u,
                "a trailing space does not zero the sub-class version");
  checkSubClass(" 12.34 ", 0x1234u,
                "surrounding whitespace on the sub-class version is trimmed");

  // Trimming must not turn an all-blank component into a zero.
  checkVersion("5. ", 0u, "an all-blank minor component is still rejected");
  checkSubClass("   ", 0u, "an all-blank sub-class version is still rejected");

  // --- a component too many -----------------------------------------------------
  // The header holds four components; the sub-class version holds two. Anything
  // beyond that was silently truncated, which diverged from
  // icJsonParseBCDVersionStr -- it hands the whole remainder to a helper that
  // refuses more than two digits.

  checkVersion("5.10.12.34.56", 0u, "a fifth version component is rejected");
  checkSubClass("1.2.3", 0u, "a third sub-class component is rejected");

  // A rejected component must say so. iccFromXml only prints the reason when
  // ParseXml fails, but the string is what carries the explanation either way.
  {
    bool bParsed = false;
    std::string parseStr;
    parseVersion("5.10", "abc", &bParsed, &parseStr);
    check(parseStr.find("Cannot parse ProfileSubClassVersion") != std::string::npos,
          "a rejected sub-class version records a reason");
  }

  // --- the two elements must not fight over the same header word ---------------
  // <ProfileVersion>'s four-component form writes all four bytes; a later
  // <ProfileSubClassVersion> overwrites only the low half.
  {
    icUInt32Number v = parseVersion("5.10.12.34", "56.78");
    check(v == 0x05105678u,
          "an explicit sub-class version overrides the four-component form");
  }

  if (g_fail)
    std::fprintf(stderr, "[xml-version] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[xml-version] all assertions passed\n");

  return g_fail;
}
