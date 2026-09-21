/*
    File:       xml-calculator-curve-writer.cpp

    Contains:   CTest helper: iccToXml wrote an unclosed <SampledCalculatorCurve>
                for Testing/Display/Rec2100HlgFullCC.xml and still exited 0, so
                iccFromXml refused the document iccToXml had just produced.

    Two defects combined.

    1. CIccMpeXmlCalculator did not override NewCopy(), so a copy came back as
       a plain CIccMpeCalculator.  ToXmlCurve() writes a sampled calculator
       curve from a CIccSampledCalculatorCurveXml copy of it, whose copy
       constructor copies the calculator through NewCopy(), and
       CIccSampledCalculatorCurveXml::ToXml() refuses a calculator that is not
       the XML class -- after it has written its opening element.

    2. CIccTagXmlMultiProcessElement::ToXml() and CIccMpeXmlCalculator::ToXml()
       ignored the result of each element's ToXml(), so that half-written
       element was followed by the rest of the document and reported as a
       success.

    Cases:
      - the corpus profile: loaded from XML, saved, read back from the binary
        through the XML factories exactly as iccToXml does, written to XML,
        and read again.  Both curves must be written in full and the second
        binary must equal the first.  Fails on defect 1.
      - NewCopy() of a CIccMpeXmlCalculator keeps the XML class.  Fails on
        defect 1.
      - an MPE tag whose sampled calculator curve holds a plain (non-XML)
        calculator must fail to write; the same tag holding an XML calculator
        is the control.  Fails on defect 2 in IccTagXml.cpp only.
      - a calculator whose sub-element is that failing curve set must fail to
        write; the XML-calculator sub-element is the control.  Fails on
        defect 2 in IccMpeXml.cpp only.

    Args:
      argv[1] - scratch directory to write profiles and documents into
      argv[2] - repository root (the corpus is read from here)

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagXml.h"
#include "IccMpeXml.h"
#include "IccProfile.h"
#include "IccIO.h"
#include "IccXmlConfig.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static const char *kName = "xml-calculator-curve-writer";

static int g_fail = 0;

static void check(bool ok, const char *label)
{
  if (ok) {
    std::fprintf(stdout, "%s: PASS  %s\n", kName, label);
  }
  else {
    std::fprintf(stderr, "%s: FAIL  %s\n", kName, label);
    g_fail++;
  }
}

static size_t countOf(const std::string &text, const char *needle)
{
  size_t n = 0;
  const size_t len = std::strlen(needle);
  for (size_t pos = text.find(needle); pos != std::string::npos;
       pos = text.find(needle, pos + len)) {
    n++;
  }
  return n;
}

static bool readBytes(const std::filesystem::path &path, std::vector<char> &bytes)
{
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

// Loads an XML document with the working directory set to its own directory,
// as CreateAllProfiles.sh runs iccFromXml, then saves it as a binary profile.
static bool xmlToIcc(const std::filesystem::path &xmlPath,
                     const std::filesystem::path &iccPath, std::string &parseStr)
{
  std::error_code ec;
  const std::filesystem::path saved = std::filesystem::current_path(ec);
  std::filesystem::current_path(xmlPath.parent_path(), ec);
  if (ec)
    return false;

  CIccProfileXml profile;
  bool ok = profile.LoadXml(xmlPath.string().c_str(), "", &parseStr);
  if (ok)
    ok = SaveIccProfile(iccPath.string().c_str(), &profile);

  std::filesystem::current_path(saved, ec);
  return ok;
}

static void testCorpusProfile(const std::filesystem::path &scratch,
                              const std::filesystem::path &root)
{
  const std::filesystem::path src = root / "Testing/Display/Rec2100HlgFullCC.xml";
  const std::filesystem::path icc = scratch / "Rec2100HlgFullCC.icc";
  const std::filesystem::path out = scratch / "Rec2100HlgFullCC.out.xml";
  const std::filesystem::path icc2 = scratch / "Rec2100HlgFullCC.2.icc";

  std::string parseStr;
  if (!xmlToIcc(src, icc, parseStr)) {
    check(false, "Rec2100HlgFullCC.xml loads and saves (fixture)");
    std::fprintf(stderr, "%s", parseStr.c_str());
    return;
  }

  // The same sequence as iccToXml: read the binary through the XML factories,
  // so the calculator inside each curve is created by CIccMpeXmlFactory.
  CIccFileIO io;
  CIccProfileXml profile;
  if (!io.Open(icc.string().c_str(), "r") || !profile.Read(&io)) {
    check(false, "the saved binary reads back (fixture)");
    return;
  }

  std::string xml;
  check(profile.ToXml(xml), "Rec2100HlgFullCC writes to XML");
  check(countOf(xml, "<SampledCalculatorCurve ") == 2,
        "both SampledCalculatorCurve elements are written");
  check(countOf(xml, "</SampledCalculatorCurve>") == 2,
        "both SampledCalculatorCurve elements are closed");
  check(countOf(xml, "<CalculatorElement ") == 2,
        "each curve carries its CalculatorElement");
  check(countOf(xml, "unable to serialize") == 0, "no tag is skipped");

  {
    std::ofstream f(out, std::ios::binary);
    if (!f || !(f << xml) || !f.flush()) {
      check(false, "the written document is saved (fixture)");
      return;
    }
  }

  parseStr.clear();
  check(xmlToIcc(out, icc2, parseStr), "the written document loads again");
  if (!parseStr.empty())
    std::fprintf(stderr, "%s", parseStr.c_str());

  std::vector<char> a, b;
  check(readBytes(icc, a) && readBytes(icc2, b) && !a.empty() && a == b,
        "the round-tripped profile is byte-identical to the original");
}

static void testNewCopyKeepsXmlClass()
{
  CIccMpeXmlCalculator calc;
  calc.SetSize(1, 1);

  // Through the base pointer, as CIccSampledCalculatorCurve copies it: a call
  // on the concrete type would bind to the override at compile time.
  const CIccMpeCalculator *pBase = &calc;
  CIccMpeCalculator *pCopy = pBase->NewCopy();
  IIccExtensionMpe *pExt = pCopy ? pCopy->GetExtension() : NULL;

  check(pCopy && !std::strcmp(pCopy->GetClassName(), "CIccMpeXmlCalculator"),
        "NewCopy() of CIccMpeXmlCalculator is a CIccMpeXmlCalculator");
  check(pExt && !std::strcmp(pExt->GetExtClassName(), "CIccMpeXml"),
        "NewCopy() of CIccMpeXmlCalculator keeps its XML extension");

  delete pCopy;
}

// A one-channel curve set whose sampled calculator curve holds either an XML
// calculator (writes) or a plain CIccMpeCalculator (the writer refuses it after
// emitting the curve's opening element).
static CIccMpeXmlCurveSet *newCurveSet(bool bXmlCalculator)
{
  CIccMpeCalculator *pCalc;
  if (bXmlCalculator)
    pCalc = new CIccMpeXmlCalculator();
  else
    pCalc = new CIccMpeCalculator();
  pCalc->SetSize(1, 1);

  CIccSampledCalculatorCurve *pCurve = new CIccSampledCalculatorCurve(0.0f, 1.0f);
  pCurve->SetCalculator(pCalc);

  CIccMpeXmlCurveSet *pSet = new CIccMpeXmlCurveSet();
  if (!pSet->SetSize(1) || !pSet->SetCurve(0, pCurve)) {
    delete pCurve;
    delete pSet;
    return NULL;
  }
  return pSet;
}

static void testMpeTagReportsElementFailure()
{
  for (int bXml = 1; bXml >= 0; bXml--) {
    CIccMpeXmlCurveSet *pSet = newCurveSet(bXml != 0);
    if (!pSet) {
      check(false, "curve set is built (fixture)");
      continue;
    }
    CIccTagXmlMultiProcessElement tag;
    tag.Attach(pSet);

    std::string xml;
    const bool ok = tag.ToXml(xml);
    if (bXml) {
      check(ok, "an MPE tag with an XML calculator curve writes (control)");
      check(countOf(xml, "</SampledCalculatorCurve>") == 1,
            "the control's SampledCalculatorCurve is closed");
    }
    else {
      check(!ok, "an MPE tag whose curve cannot be written fails, "
                 "not a success with an unclosed element");
    }
  }
}

static void testCalculatorReportsSubElementFailure()
{
  for (int bXml = 1; bXml >= 0; bXml--) {
    CIccMpeXmlCurveSet *pSet = newCurveSet(bXml != 0);
    if (!pSet) {
      check(false, "curve set is built (fixture)");
      continue;
    }
    CIccMpeXmlCalculator calc;
    calc.SetSize(1, 1);
    if (!calc.SetSubElem(0, pSet)) {
      delete pSet;
      check(false, "sub-element is attached (fixture)");
      continue;
    }

    std::string xml;
    const bool ok = calc.ToXml(xml);
    if (bXml) {
      check(ok, "a calculator with a writable sub-element writes (control)");
    }
    else {
      check(!ok, "a calculator whose sub-element cannot be written fails, "
                 "not a success with an unclosed element");
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s scratch-dir repo-root\n", kName);
    return 1;
  }

  const std::filesystem::path scratch = std::filesystem::absolute(argv[1]);
  const std::filesystem::path root = std::filesystem::absolute(argv[2]);
  std::error_code ec;
  std::filesystem::create_directories(scratch, ec);
  if (ec) {
    std::fprintf(stderr, "%s: cannot create %s\n", kName, scratch.string().c_str());
    return 1;
  }

  // As iccToXml and iccFromXml set up.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());
  IccXmlSetAllowFileIncludes(true);

  testCorpusProfile(scratch, root);
  testNewCopyKeepsXmlClass();
  testMpeTagReportsElementFailure();
  testCalculatorReportsSubElementFailure();

  if (g_fail) {
    std::fprintf(stderr, "%s: %d check(s) failed\n", kName, g_fail);
    return 1;
  }
  std::fprintf(stdout, "%s: all checks passed\n", kName);
  return 0;
}
