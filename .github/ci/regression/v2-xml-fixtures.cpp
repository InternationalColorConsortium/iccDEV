// Coverage for the tracked ICC v2 XML fixtures under Testing/V2 (issue #1883).
//
// The Testing corpus had no v2 profile of any kind: a clean checkout's 210
// profiles are 208 v5 and 2 v4, with 0 'mft2' and 0 'mft1' tags and not one XML
// source declaring a version below 4.00. Testing/V2/*.xml closes that gap, and
// the .icc are generated from them by CreateAllProfiles.sh the same way the rest
// of the corpus is -- Testing/**/*.icc is gitignored, so the XML is the tracked
// artifact and the profile is derived from it.
//
// That makes the XML the thing worth pinning. This test drives the same path
// iccFromXml does -- CIccProfileXml::LoadXml after pushing the XML tag and MPE
// factories -- and asserts of each fixture that:
//
//   1. it parses at all. The v2 side of the XML reader had never been exercised:
//      CIccTagXmlLut16/CIccTagXmlLut8 exist but no tracked XML reached them, and
//      ParseXml for the header version accepts a 2.x BCD nobody had fed it.
//   2. the header really is v2. A fixture that silently became v4 would keep
//      passing every other assertion here while restoring the original gap.
//   3. the LUT tag is the type the fixture exists to provide ('mft2' or 'mft1'),
//      since that is what selects the legacy-PCS path pinned by v2-legacy-pcs.
//   4. it validates with no critical error, so the fixtures stay conformant
//      rather than merely parseable.
//
// Usage:
//   v2-xml-fixtures <fixture.xml> <expected-tag-sig|none>
//
// where expected-tag-sig is the four-character type signature the AToB0Tag must
// carry, or "none" for the matrix/TRC fixture that has no LUT.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagLut.h"
#include "IccUtil.h"

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
    std::fprintf(stderr, "[v2-xml-fixtures] FAIL: %s\n", what);
  }
}

// Render a tag type signature as its four characters, for messages.
std::string sigToText(icTagTypeSignature sig)
{
  const icUInt32Number v = (icUInt32Number)sig;
  char buf[5];
  buf[0] = (char)((v >> 24) & 0xff);
  buf[1] = (char)((v >> 16) & 0xff);
  buf[2] = (char)((v >>  8) & 0xff);
  buf[3] = (char)( v        & 0xff);
  buf[4] = '\0';
  for (int i = 0; i < 4; i++) {
    if (buf[i] < 0x20 || buf[i] > 0x7e)
      buf[i] = '?';
  }
  return std::string(buf);
}

icTagTypeSignature textToSig(const char *szText)
{
  return (icTagTypeSignature)(((icUInt32Number)(icUInt8Number)szText[0] << 24) |
                              ((icUInt32Number)(icUInt8Number)szText[1] << 16) |
                              ((icUInt32Number)(icUInt8Number)szText[2] <<  8) |
                               (icUInt32Number)(icUInt8Number)szText[3]);
}

} // namespace

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::fprintf(stderr,
                 "usage: %s <fixture.xml> <expected-tag-sig|none>\n",
                 argv[0] ? argv[0] : "v2-xml-fixtures");
    return 2;
  }

  const char *szXml      = argv[1];
  const char *szExpected = argv[2];

  // iccFromXml pushes these before parsing; without them the XML tag types
  // resolve to their non-XML base classes and ParseXml is never reached.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  CIccProfileXml profile;
  std::string parseStr;
  if (!profile.LoadXml(szXml, NULL, &parseStr)) {
    std::fprintf(stderr, "[v2-xml-fixtures] FAIL: could not parse %s\n%s\n",
                 szXml, parseStr.c_str());
    return 1;
  }

  // 2. The header must still be v2. The major version is the top BCD byte.
  const icUInt32Number major = (profile.m_Header.version >> 24) & 0xff;
  if (major != 0x02) {
    char msg[160];
    std::snprintf(msg, sizeof(msg),
                  "%s declares version 0x%08x, expected a 2.x profile",
                  szXml, profile.m_Header.version);
    check(false, msg);
  }

  // 3. The LUT tag must be the type this fixture exists to contribute.
  if (std::strcmp(szExpected, "none") != 0) {
    CIccTag *pTag = profile.FindTag(icSigAToB0Tag);
    if (!pTag) {
      check(false, "fixture has no AToB0Tag");
    }
    else {
      const icTagTypeSignature want = textToSig(szExpected);
      if (pTag->GetType() != want) {
        char msg[160];
        std::snprintf(msg, sizeof(msg),
                      "AToB0Tag is '%s', expected '%s'",
                      sigToText(pTag->GetType()).c_str(), szExpected);
        check(false, msg);
      }
    }
  }

  // 4. Conformance, not just parseability. Warnings are tolerated; a critical
  // error means the fixture would poison anything that consumes the corpus.
  std::string report;
  const icValidateStatus status = profile.Validate(report);
  if (status >= icValidateCriticalError) {
    std::fprintf(stderr, "[v2-xml-fixtures] validation report for %s:\n%s\n",
                 szXml, report.c_str());
    check(false, "fixture does not validate -- critical error");
  }

  if (g_fail)
    std::fprintf(stderr, "[v2-xml-fixtures] %d assertion(s) failed for %s\n",
                 g_fail, szXml);
  else
    std::printf("[v2-xml-fixtures] %s: v2 profile parsed, tag type and "
                "validation as expected\n", szXml);

  return g_fail;
}
