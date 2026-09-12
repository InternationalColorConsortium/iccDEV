/*
    File:       colorant-count-narrowing-xml.cpp

    Contains:   CTest helper for the XML twin of the colorantOrder count
                narrowing fixed for JSON in #2536.

    CIccTagXmlColorantOrder::ParseXml() counted its <n> elements into an int and
    passed that to SetSize(), which takes an icUInt16Number, so 65537 elements
    narrowed to a one-entry allocation.  The count then went on UNNARROWED to
    CIccUInt8Array::ParseArray() as its buffer size, and that function's own
    "n > nBufSize" test cannot catch this: the size it is handed is the element
    count, not the size of the allocation, so the two agree and it writes every
    element through a one-byte buffer.

    This is a separate reachable entry point from the JSON readers, not a
    restatement of them -- iccFromXml on a hand-authored document reaches it
    with no JSON involved.  Measured on master 896e9c06:

      WRITE of size 1 in CIccXmlArrayType<unsigned char>::ParseArray at
      IccUtilXml.cpp:1352, called from IccTagXml.cpp:2365, 0 bytes after a
      1-byte region allocated in CIccTagColorantOrder::CIccTagColorantOrder(int)

    It is registered separately from the JSON helper so that neither masks the
    other: this one is skipped where IccXML is not built, and the JSON coverage
    does not disappear with it.

    CIccTagXmlColorantTable::ParseXml() is deliberately NOT asserted as a defect
    here.  It was measured on the same tree with an equivalent 65537-colorant
    document and does not overrun: it casts to icUInt16Number explicitly and
    bounds its own write loop by that same narrowed count, so it silently
    truncates the colorant list instead.

    The last case records that truncation.  Read it as a measurement, NOT as an
    endorsement: its only job is to show that a fix aimed at the neighbouring
    reader did not change this one.  Truncating is very likely the WRONG
    answer -- it accepts a document and silently discards colorants -- and after
    this commit the two front ends disagree about the same shape, because
    CIccTagJsonColorantTable::ParseJson() now refuses a 65537-entry table that
    this reader still accepts as one colorant.  Settling that is a spec and
    compatibility question for the maintainers, not something a memory-safety
    fix should decide by itself.  Whoever settles it should expect to INVERT
    this case rather than to work around it.

    The oversize cases are the memory-safety cases, and the count that carries
    them is 65537, not 65536.  65537 narrows to 1, SetSize(1) is answered "true"
    by the one entry the constructor already allocated, and the unfixed reader
    then writes 65537 elements through it.  65536 narrows to 0, where SetSize(0)
    reallocates to nothing and reports failure, so the unfixed reader ALREADY
    refused it: that case on its own would pass against the very defect it is
    meant to catch.  Both are kept, but 65537 is the one that carries the red.

    Measured against master 896e9c06, the unfixed helper fails on every lane and
    never on an assertion of its own: a plain clang-18 Release build aborts
    inside the first oversize case with glibc's "malloc(): corrupted top size"
    (exit 134), and a sanitizer lane aborts earlier still, at the narrowing
    itself, which -fno-sanitize-recover=integer makes fatal.  The refusal the
    case asks for is the weaker of its two signals -- what a build that survives
    the overrun would report -- and the case does not depend on a sanitizer.

    The 65535 case is the boundary discriminator: that many elements are
    representable and must still load in full, so a guard written ">= 0xFFFF"
    fails it while passing every other case here.

    Args:
      argv[1] - scratch directory to generate fixtures in (created if absent)

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccTagBasic.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static int check(bool condition, const char *label)
{
  if (condition) {
    std::fprintf(stdout, "colorant-count-narrowing-xml: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "colorant-count-narrowing-xml: FAIL  %s\n", label);
  return 1;
}

static const char *kHeader =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<IccProfile>\n"
  "  <Header>\n"
  "    <ProfileVersion>4.30</ProfileVersion>\n"
  "    <ProfileDeviceClass>prtr</ProfileDeviceClass>\n"
  "    <DataColourSpace>CMYK</DataColourSpace>\n"
  "    <PCS>Lab </PCS>\n"
  "    <CreationDateTime>now</CreationDateTime>\n"
  "    <RenderingIntent>Perceptual</RenderingIntent>\n"
  "    <PCSIlluminant>\n"
  "      <XYZNumber X=\"0.964202880859\" Y=\"1.000000000000\" Z=\"0.824905395508\"/>\n"
  "    </PCSIlluminant>\n"
  "  </Header>\n"
  "  <Tags>\n";

static const char *kFooter =
  "  </Tags>\n"
  "</IccProfile>\n";

static bool writeOrderFixture(const std::string &path, size_t nEntries)
{
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;

  f << kHeader;
  f << "    <colorantOrderTag> <colorantOrderType>\n      <ColorantOrder>\n";
  for (size_t i = 0; i < nEntries; i++)
    f << "        <n>" << (unsigned)(i % 256) << "</n>\n";
  f << "      </ColorantOrder>\n    </colorantOrderType> </colorantOrderTag>\n";
  f << kFooter;

  return f.good();
}

static bool writeTableFixture(const std::string &path, size_t nEntries)
{
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;

  f << kHeader;
  f << "    <colorantTableTag> <colorantTableType>\n      <ColorantTable>\n";
  for (size_t i = 0; i < nEntries; i++)
    f << "        <Colorant Name=\"colorant-" << (unsigned)i
      << "\" Channel1=\"50\" Channel2=\"0\" Channel3=\"0\"/>\n";
  f << "      </ColorantTable>\n    </colorantTableType> </colorantTableTag>\n";
  f << kFooter;

  return f.good();
}

// Returns whether the document loaded.  A sanitizer abort inside here IS the
// defect on an instrumented lane: the caller never gets its result back.
static bool loadXml(CIccProfileXml &profile, const std::string &file)
{
  std::string parseStr;
  return profile.LoadXml(file.c_str(), "", &parseStr);
}

static int orderCase(const std::filesystem::path &dir, size_t nEntries,
                     bool bExpectLoaded, const char *file, const char *label)
{
  const std::string path = (dir / file).string();
  if (!writeOrderFixture(path, nEntries)) {
    std::fprintf(stderr, "colorant-count-narrowing-xml: could not write %s\n",
                 path.c_str());
    return 1;
  }

  CIccProfileXml profile;
  const bool bLoaded = loadXml(profile, path);

  if (bLoaded != bExpectLoaded) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-xml: FAIL  %s (LoadXml returned %s, "
                 "expected %s)\n",
                 label, bLoaded ? "true" : "false",
                 bExpectLoaded ? "true" : "false");
    return 1;
  }

  if (!bExpectLoaded)
    return check(true, label);

  // Accepting the count is not enough: the tag must actually carry every
  // position, or a reader that narrowed and then filled a shorter table would
  // pass on the load result alone.
  CIccTagColorantOrder *pTag =
    (CIccTagColorantOrder *)profile.FindTag(icSigColorantOrderTag);
  if (!pTag) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-xml: FAIL  %s (no colorantOrderTag "
                 "on the loaded profile)\n", label);
    return 1;
  }

  if (pTag->GetSize() != nEntries) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-xml: FAIL  %s (GetSize %u, expected "
                 "%u)\n",
                 label, (unsigned)pTag->GetSize(), (unsigned)nEntries);
    return 1;
  }

  const icUInt8Number expect = (icUInt8Number)((nEntries - 1) % 256);
  const icUInt8Number *pData = pTag->GetData(0);
  if (!pData || pData[nEntries - 1] != expect) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-xml: FAIL  %s (last position %d, "
                 "expected %d)\n",
                 label, pData ? (int)pData[nEntries - 1] : -1, (int)expect);
    return 1;
  }

  return check(true, label);
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: colorant-count-narrowing-xml <scratch-directory>\n");
    return 1;
  }

  std::filesystem::path dir(argv[1]);
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    std::fprintf(stderr,
                 "colorant-count-narrowing-xml: could not create %s: %s\n",
                 argv[1], ec.message().c_str());
    return 1;
  }

  // The factory iccFromXml pushes before LoadXml.  Without it no
  // CIccTagXmlColorantOrder is ever constructed, the load fails for want of a
  // tag handler, and case 1's "is refused" assertion would pass without the
  // code under test being entered.  Cases 2 and 3 are what catch that.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());

  int failures = 0;

  failures += orderCase(dir, 65537, false, "colorant-order-65537.xml",
                        "a colorantOrder at 65537, which narrowed to a writable 1, is refused");
  failures += orderCase(dir, 65536, false, "colorant-order-65536.xml",
                        "a colorantOrder at 65536, which narrowed to 0, is refused for the same reason");
  failures += orderCase(dir, 65535, true, "colorant-order-65535.xml",
                        "a colorantOrder at exactly 65535 still loads in full");
  failures += orderCase(dir, 4, true, "colorant-order-4.xml",
                        "an ordinary colorantOrder is unaffected");

  // The sibling reader, measured NOT to overrun.  This records that it still
  // truncates, so the fix above can be shown not to have moved it -- see the
  // header: truncation is being measured here, not endorsed, and it is the case
  // to invert once the front-end divergence is ruled on.
  {
    const std::string path = (dir / "colorant-table-65537.xml").string();
    if (!writeTableFixture(path, 65537)) {
      std::fprintf(stderr, "colorant-count-narrowing-xml: could not write %s\n",
                   path.c_str());
      return 1;
    }

    CIccProfileXml profile;
    const bool bLoaded = loadXml(profile, path);
    CIccTagColorantTable *pTag =
      bLoaded ? (CIccTagColorantTable *)profile.FindTag(icSigColorantTableTag)
              : nullptr;

    failures += check(bLoaded && pTag && pTag->GetSize() == 1,
                      "the colorantTable reader still truncates an oversize list rather than overrunning it (measured, not endorsed)");
  }

  if (failures) {
    std::fprintf(stderr, "colorant-count-narrowing-xml: %d case(s) failed\n",
                 failures);
    return 1;
  }

  std::fprintf(stdout, "colorant-count-narrowing-xml: all cases passed\n");
  return 0;
}
