/*
    File:       xml-curve-oversize-file.cpp

    Contains:   CTest helper for CIccTagXmlCurve::ParseXml's File=/Format="text"
                curve branches (#2006).

    CIccTagCurve::SetSize() answers a request above its 65536-entry cap by
    freeing the table, setting m_nSize to 0 and returning true.  The three
    File=/Format="text" branches of CIccTagXmlCurve::ParseXml called it without
    checking either the return or the resulting size, then wrote the parsed
    samples through GetData(0), which is NULL once the table has been freed.
    A hand-authored XML profile whose curve table has 65537 entries therefore
    took iccFromXml down with SIGSEGV (UBSan: "store to null pointer of type
    'icFloatNumber'" at IccTagXml.cpp).

    The equivalent inline <Curve> branches in the same function have guarded
    this since #1158, so the same values authored inline are refused cleanly.
    This test pins both halves of that contrast, and the 65536 boundary.

    The File= attribute is resolved relative to the process working directory
    and IccXmlIsPathSafe() rejects absolute paths, so the helper generates its
    fixtures in a scratch directory and runs from inside it rather than writing
    anything into the source tree.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccXmlConfig.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "xml-curve-oversize-file: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "xml-curve-oversize-file: FAIL  %s\n", label);
  return 1;
}

// A plain v4 RGB matrix/TRC display profile.  Only redTRCTag varies: the caller
// supplies the <Curve> element so the same profile can carry a File= reference
// or the identical values inline.
static std::string profileXml(const std::string& redCurve)
{
  return
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <PreferredCMMType>ICCD</PreferredCMMType>\n"
    "    <ProfileVersion>4.40</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2026-08-06T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Relative Colorimetric</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <redColorantTag><XYZArrayType><XYZNumber X=\"0.4360\" Y=\"0.2225\" Z=\"0.0139\"/></XYZArrayType></redColorantTag>\n"
    "    <greenColorantTag><XYZArrayType><XYZNumber X=\"0.3851\" Y=\"0.7169\" Z=\"0.0971\"/></XYZArrayType></greenColorantTag>\n"
    "    <blueColorantTag><XYZArrayType><XYZNumber X=\"0.1431\" Y=\"0.0606\" Z=\"0.7139\"/></XYZArrayType></blueColorantTag>\n"
    "    <redTRCTag><curveType>\n      " + redCurve + "\n    </curveType></redTRCTag>\n"
    "    <greenTRCTag><curveType><Curve>1.0</Curve></curveType></greenTRCTag>\n"
    "    <blueTRCTag><curveType><Curve>1.0</Curve></curveType></blueTRCTag>\n"
    "    <mediaWhitePointTag><XYZArrayType><XYZNumber X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/></XYZArrayType></mediaWhitePointTag>\n"
    "    <profileDescriptionTag><multiLocalizedUnicodeType>"
    "<LocalizedText LanguageCountry=\"enUS\"><![CDATA[curve oversize regression]]></LocalizedText>"
    "</multiLocalizedUnicodeType></profileDescriptionTag>\n"
    "    <copyrightTag><multiLocalizedUnicodeType>"
    "<LocalizedText LanguageCountry=\"enUS\"><![CDATA[none]]></LocalizedText>"
    "</multiLocalizedUnicodeType></copyrightTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

// A ramp of n 16-bit sample values, space separated, as the text curve format
// expects.  Values are not what is under test; the count is.
static std::string rampText(unsigned n)
{
  std::string out;
  out.reserve(n * 6);
  for (unsigned i = 0; i < n; i++) {
    if (i)
      out += ' ';
    out += std::to_string((unsigned)((unsigned long long)i * 65535ull / (n - 1)));
  }
  return out;
}

static bool writeFile(const std::string& path, const std::string& text)
{
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  f << text;
  return f.good();
}

// Load a generated profile and report only whether the parse succeeded.  A
// SIGSEGV inside here is the defect: the caller never gets its result back.
//
// The empty RelaxNG directory matches what iccFromXml passes when -v is absent.
// Note the factories the caller must have pushed first: without them no
// CIccTagXmlCurve is ever constructed, every load fails for want of a tag
// handler, and the two "is refused" assertions below pass without the branch
// under test being entered at all.  The 65536 boundary case is what catches
// that, which is why it is here and not just for completeness.
static bool loadXml(const char* file, std::string& parseStr)
{
  CIccProfileXml profile;
  return profile.LoadXml(file, "", &parseStr);
}

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
    return 1;
  }

  std::error_code ec;
  std::filesystem::create_directories(argv[1], ec);
  std::filesystem::current_path(argv[1], ec);
  if (ec) {
    std::fprintf(stderr, "xml-curve-oversize-file: cannot use scratch dir '%s'\n", argv[1]);
    return 1;
  }

  // The File= loaders are gated off by default (#863).  iccFromXml opts in
  // unconditionally, which is what makes the crash reachable from a tool, so
  // opt in here too -- without this the branch under test is never entered and
  // the test would pass for the wrong reason.
  IccXmlSetAllowFileIncludes(true);

  // Same two factories iccFromXml pushes before LoadXml.  These are what make
  // <curveType> resolve to CIccTagXmlCurve; see the note on loadXml() above.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  int failures = 0;

  const unsigned nCap = 65536;   // the largest curve a curveType may hold
  const unsigned nOver = 65537;  // one past it

  if (!writeFile("curve-oversize.txt", rampText(nOver)) ||
      !writeFile("curve-atcap.txt", rampText(nCap)) ||
      !writeFile("curve-oversize.xml",
                 profileXml("<Curve File=\"curve-oversize.txt\" Format=\"text\"/>")) ||
      !writeFile("curve-atcap.xml",
                 profileXml("<Curve File=\"curve-atcap.txt\" Format=\"text\"/>")) ||
      !writeFile("curve-oversize-inline.xml",
                 profileXml("<Curve>" + rampText(nOver) + "</Curve>"))) {
    std::fprintf(stderr, "xml-curve-oversize-file: could not generate fixtures\n");
    return 1;
  }

  // The case that used to crash.  Before the fix this call does not return: the
  // parser writes 65537 floats through a NULL table.
  {
    std::string parseStr;
    bool loaded = loadXml("curve-oversize.xml", parseStr);
    failures += check(!loaded,
                      "over-cap curve from File= is refused instead of crashing");
    failures += check(!parseStr.empty(),
                      "over-cap curve from File= reports a parse message");
  }

  // The boundary, so the guard cannot be satisfied by refusing everything.
  {
    std::string parseStr;
    failures += check(loadXml("curve-atcap.xml", parseStr),
                      "curve of exactly 65536 entries from File= still parses");
  }

  // The contrast that shows this was one branch away from correct all along:
  // identical values, identical count, authored inline instead of referenced.
  // This branch has been guarded since #1158 and passes before and after.
  {
    std::string parseStr;
    failures += check(!loadXml("curve-oversize-inline.xml", parseStr),
                      "over-cap curve authored inline is refused");
  }

  return failures ? 1 : 0;
}
