/*
    File:       xml-numeric-values.cpp

    Contains:   CTest helper for #2548: the XML readers converted numbers with
                bare atof(), atoi() and atol(), so a malformed value loaded as
                whatever prefix of it parsed -- "50abc" as 50, "not-a-number"
                as 0 -- with no diagnostic.

    PR 1 fixed the colorantTable channels (colorant-count-narrowing-xml.cpp
    pins those).  This helper covers the class: every other reader that took a
    number from an attribute or element text now goes through
    icXmlParseFloat() or icXmlParseU8/U16/U32 and refuses the document.

    Most cases mutate a tracked corpus document in memory -- one value, one
    edit -- so the fixture is a real profile rather than a hand-built
    approximation.  A refusal on its own would pass if the edit never reached
    the reader, so each case asserts the reader's OWN refusal message, which
    only the changed code emits, and asserts that the unmodified document does
    not carry it.  Where the unmodified document loads, the edited one must
    also fail to load.  A case whose anchor has gone from the corpus fails
    rather than skipping, so the fixture cannot go vacuous silently.

    Two readers take element text rather than attributes (CAM parameters and
    ColorTemperature).  Their whitespace cases pin the other direction:
    pretty-printed "<Luminance>\n    500.0\n  </Luminance>" must still load,
    because strtod skips leading whitespace but a whole-string check would
    otherwise see the trailing newline as garbage.

    Readers no tracked document reaches (chromaticityType, measurementType,
    viewingConditionsType, responseCurveSet16Type, the NamedColor and
    XYZNamedColor forms of namedColor2Type, parametricCurveType's Reserved,
    and a lutAtoBType CLUT's TableData Precision)
    get small generated documents with the same control-then-edit structure.

    chromaticityType has one more check.  Its writer printed x as
    icXmlFloatFmt "f", so every chromaticity document iccToXml had written
    carries x="0.640000000000f", which a strict reader would refuse.  The
    writer no longer emits the 'f', and the reader still accepts that one
    trailing 'f' -- loading to the same value as "0.64", not to 0.

    Documents are loaded with file includes enabled, as iccFromXml enables
    them, and with the working directory set to the source document's own
    directory, as CreateAllProfiles.sh runs iccFromXml, so a relative include
    or <Import> still resolves; the edited copies are written only into the
    scratch directory.

    Args:
      argv[1] - scratch directory to write edited documents into
      argv[2] - repository root (the corpus is read from here)

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccTagXml.h"
#include "IccTagBasic.h"
#include "IccUtil.h"
#include "IccXmlConfig.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static const char *kName = "xml-numeric-values";

static int pass(const char *label)
{
  std::fprintf(stdout, "%s: PASS  %s\n", kName, label);
  return 0;
}

static int fail(const char *label, const std::string &why)
{
  std::fprintf(stderr, "%s: FAIL  %s (%s)\n", kName, label, why.c_str());
  return 1;
}

enum OpKind {
  kAppend,       // append text to the value (attribute value or element text)
  kWrap,         // put text on both sides of the value
  kInsertAfter,  // insert text directly after the anchor
  kReplace       // replace the anchor itself with text
};

struct Op {
  OpKind kind;
  const char *anchor;  // first occurrence in the document
  const char *attr;    // attribute name after the anchor; NULL = element text
  const char *text;
};

// Finds `name` = "..." at or after `from`, allowing spaces around '=' (the
// corpus has FirstEntry = "0.0"), and returns the value's [start, end).
static bool findAttrValue(const std::string &doc, size_t from, const char *name,
                          size_t &start, size_t &end)
{
  const std::string key = std::string(" ") + name;
  for (size_t a = doc.find(key, from); a != std::string::npos;
       a = doc.find(key, a + 1)) {
    size_t p = a + key.size();
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\t'))
      p++;
    if (p >= doc.size() || doc[p] != '=')
      continue;
    p++;
    while (p < doc.size() && (doc[p] == ' ' || doc[p] == '\t'))
      p++;
    if (p >= doc.size() || doc[p] != '"')
      continue;
    start = p + 1;
    end = doc.find('"', start);
    return end != std::string::npos;
  }
  return false;
}

static bool applyOp(std::string &doc, const Op &op)
{
  size_t pos = doc.find(op.anchor);
  if (pos == std::string::npos)
    return false;
  const size_t anchorEnd = pos + std::strlen(op.anchor);

  if (op.kind == kReplace) {
    doc.replace(pos, std::strlen(op.anchor), op.text);
    return true;
  }
  if (op.kind == kInsertAfter) {
    doc.insert(anchorEnd, op.text);
    return true;
  }

  size_t start = 0, end = 0;
  if (op.attr) {
    if (!findAttrValue(doc, anchorEnd, op.attr, start, end))
      return false;
  }
  else {
    start = anchorEnd;
    end = doc.find('<', start);
    if (end == std::string::npos)
      return false;
  }

  doc.insert(end, op.text);
  if (op.kind == kWrap)
    doc.insert(start, op.text);
  return true;
}

struct LoadResult {
  bool bLoaded;
  std::string parseStr;
};

// Writes doc into the scratch directory and loads it with the working
// directory set to cwd, restoring the working directory afterwards.
static bool loadDoc(const std::string &doc, const std::filesystem::path &scratch,
                    const std::string &name, const std::filesystem::path &cwd,
                    LoadResult &result)
{
  const std::filesystem::path path = scratch / name;
  {
    std::ofstream f(path, std::ios::binary);
    if (!f || !(f << doc) || !f.flush())
      return false;
  }

  std::error_code ec;
  const std::filesystem::path saved = std::filesystem::current_path(ec);
  std::filesystem::current_path(cwd, ec);
  if (ec)
    return false;

  CIccProfileXml profile;
  result.parseStr.clear();
  result.bLoaded = profile.LoadXml(path.string().c_str(), "", &result.parseStr);

  std::filesystem::current_path(saved, ec);
  std::filesystem::remove(path, ec);
  return true;
}

struct Case {
  const char *label;
  const char *file;     // repo-relative corpus document, or NULL for inlineDoc
  std::vector<Op> ops;  // edits that turn the control into the case
  const char *message;  // refusal message expected; NULL = must load
};

// kReplace edits prepare the control -- e.g. undoing the unrelated defect a
// ub-* fixture was written to carry -- so they apply to both documents; every
// other edit applies to the edited document only.
static int runCase(const Case &c, const std::string &source,
                   const std::filesystem::path &cwd,
                   const std::filesystem::path &scratch, int index)
{
  std::string controlDoc = source;
  for (const Op &op : c.ops) {
    if (op.kind == kReplace && !applyOp(controlDoc, op))
      return fail(c.label, std::string("anchor \"") + op.anchor +
                             "\" not found: the corpus changed under this case");
  }

  std::string edited = controlDoc;
  for (const Op &op : c.ops) {
    if (op.kind != kReplace && !applyOp(edited, op))
      return fail(c.label, std::string("anchor \"") + op.anchor +
                             "\" not found: the corpus changed under this case");
  }
  if (edited == controlDoc)
    return fail(c.label, "the edit did not change the document");

  char name[64];
  LoadResult control, result;
  std::snprintf(name, sizeof(name), "case-%02d-control.xml", index);
  if (!loadDoc(controlDoc, scratch, name, cwd, control))
    return fail(c.label, "could not write or load the control document");
  std::snprintf(name, sizeof(name), "case-%02d-edited.xml", index);
  if (!loadDoc(edited, scratch, name, cwd, result))
    return fail(c.label, "could not write or load the edited document");

  if (!c.message) {
    if (!control.bLoaded)
      return fail(c.label, "the unmodified document does not load: " + control.parseStr);
    if (!result.bLoaded)
      return fail(c.label, "the edited document was refused: " + result.parseStr);
    return pass(c.label);
  }

  if (control.parseStr.find(c.message) != std::string::npos)
    return fail(c.label, std::string("the unmodified document already reports \"") +
                           c.message + "\", so the case cannot discriminate");
  if (result.parseStr.find(c.message) == std::string::npos)
    return fail(c.label, std::string("expected \"") + c.message +
                           "\"; LoadXml reported: " +
                           (result.parseStr.empty() ? "nothing" : result.parseStr));
  if (control.bLoaded && result.bLoaded)
    return fail(c.label, "the edited document still loaded");

  return pass(c.label);
}

static bool readFile(const std::filesystem::path &path, std::string &out)
{
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

// The header colorant-count-narrowing-xml.cpp uses for its generated documents.
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

static std::string generated(const char *tags)
{
  return std::string(kHeader) + tags + kFooter;
}

static const char *kChromaticity =
  "    <chromaticityTag> <chromaticityType>\n"
  "      <Colorant>Unknown</Colorant>\n"
  "      <Channel x=\"0.64\" y=\"0.33\"/>\n"
  "      <Channel x=\"0.30\" y=\"0.60\"/>\n"
  "      <Channel x=\"0.15\" y=\"0.06\"/>\n"
  "    </chromaticityType> </chromaticityTag>\n";

static const char *kMeasurement =
  "    <measurementTag> <measurementType>\n"
  "      <StandardObserver>Unknown observer</StandardObserver>\n"
  "      <MeasurementBacking X=\"0.0\" Y=\"0.0\" Z=\"0.0\"/>\n"
  "      <Geometry>Geometry Unknown</Geometry>\n"
  "      <Flare>Flare 0</Flare>\n"
  "      <StandardIlluminant>Illuminant D50</StandardIlluminant>\n"
  "    </measurementType> </measurementTag>\n";

static const char *kViewingConditions =
  "    <viewingConditionsTag> <viewingConditionsType>\n"
  "      <IlluminantXYZ X=\"96.42\" Y=\"100.0\" Z=\"82.49\"/>\n"
  "      <SurroundXYZ X=\"19.28\" Y=\"20.0\" Z=\"16.50\"/>\n"
  "      <IllumType>Illuminant D50</IllumType>\n"
  "    </viewingConditionsType> </viewingConditionsTag>\n";

static const char *kResponseCurve =
  "    <outputResponseTag> <responseCurveSet16Type>\n"
  "      <CountOfChannels>1</CountOfChannels>\n"
  "      <ResponseCurve MeasUnitSignature=\"Status A\">\n"
  "        <ChannelResponses X=\"1.0\" Y=\"1.0\" Z=\"1.0\">\n"
  "          <Measurement DeviceCode=\"0\" MeasValue=\"0.5\"/>\n"
  "        </ChannelResponses>\n"
  "      </ResponseCurve>\n"
  "    </responseCurveSet16Type> </outputResponseTag>\n";

static const char *kNamedColor =
  "    <namedColor2Tag> <namedColor2Type>\n"
  "      <NamedColors VendorFlag=\"00000000\" CountOfDeviceCoords=\"0\" DeviceEncoding=\"int16\" Prefix=\"\" Suffix=\"\">\n"
  "        <NamedColor Name=\"red\" L=\"46\" a=\"68\" b=\"38\"/>\n"
  "        <XYZNamedColor Name=\"white\" X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/>\n"
  "      </NamedColors>\n"
  "    </namedColor2Type> </namedColor2Tag>\n";

// A lutAtoBType CLUT is the variable-precision path, where TableData's
// Precision attribute selects 1- or 2-byte entries.  The only tracked documents
// that reach it are the multi-megabyte Testing/hybrid profiles.
static const char *kLutAtoB =
  "    <AToB0Tag> <lutAtoBType>\n"
  "      <Channels InputChannels=\"1\" OutputChannels=\"3\"/>\n"
  "      <ACurves>\n"
  "        <Curve IdentitySize=\"2\"/>\n"
  "      </ACurves>\n"
  "      <CLUT>\n"
  "        <GridPoints>2</GridPoints>\n"
  "        <TableData Precision=\"1\">0 128 128 255 128 128</TableData>\n"
  "      </CLUT>\n"
  "      <BCurves>\n"
  "        <Curve IdentitySize=\"2\"/>\n"
  "        <Curve IdentitySize=\"2\"/>\n"
  "        <Curve IdentitySize=\"2\"/>\n"
  "      </BCurves>\n"
  "    </lutAtoBType> </AToB0Tag>\n";

static const char *kParametric =
  "    <redTRCTag> <parametricCurveType>\n"
  "      <ParametricCurve FunctionType=\"0\" Reserved=\"7\">\n"
  "        2.2\n"
  "      </ParametricCurve>\n"
  "    </parametricCurveType> </redTRCTag>\n";

// Loads a generated chromaticityType whose first channel's x is szX and
// returns that x as loaded, plus the tag's own XML serialization.
static bool loadChromaticityX(const char *szX, const std::filesystem::path &scratch,
                              icFloatNumber &x, std::string &xml)
{
  std::string doc = generated(kChromaticity);
  const std::string from = "x=\"0.64\"";
  doc.replace(doc.find(from), from.size(), std::string("x=\"") + szX + "\"");

  const std::filesystem::path path = scratch / "chromaticity-x.xml";
  {
    std::ofstream f(path, std::ios::binary);
    if (!f || !(f << doc) || !f.flush())
      return false;
  }

  CIccProfileXml profile;
  std::string parseStr;
  const bool bLoaded = profile.LoadXml(path.string().c_str(), "", &parseStr);
  std::error_code ec;
  std::filesystem::remove(path, ec);
  if (!bLoaded)
    return false;

  CIccTagChromaticity *pTag = (CIccTagChromaticity *)profile.FindTag(icSigChromaticityTag);
  if (!pTag || pTag->GetSize() != 3)
    return false;
  x = (icFloatNumber)icUFtoD(pTag->Getxy(0)->x);

  IIccExtensionTag *pExt = pTag->GetExtension();
  if (!pExt || std::strcmp(pExt->GetExtClassName(), "CIccTagXml"))
    return false;
  return ((CIccTagXml *)pExt)->ToXml(xml);
}

static int chromaticityLegacyCase(const std::filesystem::path &scratch)
{
  const char *label =
    "a chromaticity x written by the old writer as \"0.640000000000f\" loads as 0.64, "
    "and the writer no longer emits the 'f'";

  icFloatNumber control = 0, legacy = -1;
  std::string controlXml, legacyXml;
  if (!loadChromaticityX("0.64", scratch, control, controlXml))
    return fail(label, "the control chromaticity (x=\"0.64\") did not load");
  if (!loadChromaticityX("0.640000000000f", scratch, legacy, legacyXml))
    return fail(label, "x=\"0.640000000000f\" was refused");
  if (legacy != control)
    return fail(label, "x=\"0.640000000000f\" loaded as a different value from \"0.64\"");
  if (controlXml.find("f\"") != std::string::npos)
    return fail(label, "the writer still emits a trailing 'f': " + controlXml);
  return pass(label);
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <scratch-directory> <repository-root>\n", kName);
    return 1;
  }

  const std::filesystem::path scratch = std::filesystem::absolute(argv[1]);
  const std::filesystem::path root = std::filesystem::absolute(argv[2]);
  std::error_code ec;
  std::filesystem::create_directories(scratch, ec);
  if (ec) {
    std::fprintf(stderr, "%s: could not create %s: %s\n", kName, argv[1],
                 ec.message().c_str());
    return 1;
  }

  // Without the factory no CIccTagXml* reader is constructed and every tag
  // fails for want of a handler; the message assertions catch that too.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());

  // iccFromXml does the same; several corpus documents <Import> a calculator
  // or read CLUT data from a file beside them, and the gate defaults to off.
  IccXmlSetAllowFileIncludes(true);

  const std::vector<Case> corpus = {
    // IccProfileXml.cpp -- the header.
    { "a PCSIlluminant X of \"0.96420288abc\" is refused",
      ".github/ci/test-data/mpe-channels-wrapped-control-1901.xml",
      { { kAppend, "<PCSIlluminant>", "X", "abc" } },
      "Invalid PCSIlluminant XYZNumber" },
    { "a header SpectralRange start of \"380abc\" is refused",
      ".github/ci/test-data/mpe-channels-wrapped-control-1901.xml",
      { { kAppend, "<SpectralRange>", "start", "abc" } },
      "Invalid SpectralRange Wavelengths start or end" },
    { "a header BiSpectralRange start with trailing text is refused",
      ".github/ci/test-data/ub-normalimage-parsexml-1343.xml",
      { { kAppend, "<BiSpectralRange>", "start", "abc" } },
      "Invalid BiSpectralRange Wavelengths start or end" },

    // IccTagXml.cpp -- tag readers.
    { "an XYZArrayType X with trailing text is refused",
      "Testing/V2/v2GrayTRC.xml",
      { { kAppend, "<XYZArrayType>", "X", "abc" } },
      "Invalid number for XYZNumber X" },
    { "a LabNamedColor L with trailing text is refused",
      "Testing/Named/NamedColorV4.xml",
      { { kAppend, "<LabNamedColor", "L", "abc" } },
      "Invalid number for LabNamedColor L" },
    { "a spectralDataInfoType Wavelengths start with trailing text is refused",
      "Testing/Display/Rec2020rgbSpectral.xml",
      { { kAppend, "<spectralDataInfoType>", "start", "abc" } },
      "Invalid number for SpectralRange Wavelengths start" },
    { "a spectralViewingConditions IlluminantXYZ X with trailing text is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kAppend, "<IlluminantXYZ", "X", "abc" } },
      "Invalid number for IlluminantXYZ X" },
    { "an ObserverFuncs start with trailing text is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kAppend, "<ObserverFuncs", "start", "abc" } },
      "Invalid number for ObserverFuncs start" },
    { "an ObserverFuncs reserved of \"1abc\" is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kInsertAfter, "<ObserverFuncs", nullptr, " reserved=\"1abc\"" } },
      "Invalid ObserverFuncs reserved" },
    { "an IlluminantSPD end with trailing text is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kAppend, "<IlluminantSPD", "end", "abc" } },
      "Invalid number for IlluminantSPD end" },
    { "an IlluminantSPD reserved of \"1abc\" is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kInsertAfter, "<IlluminantSPD", nullptr, " reserved=\"1abc\"" } },
      "Invalid IlluminantSPD reserved" },
    { "a spectralViewingConditions SurroundXYZ Z with trailing text is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-Abs.xml",
      { { kAppend, "<SurroundXYZ", "Z", "abc" } },
      "Invalid number for SurroundXYZ Z" },
    { "a ColorTemperature of \"6500abc\" is refused",
      "Testing/SpecRef/RefDecC.xml",
      { { kAppend, "<ColorTemperature>", nullptr, "abc" } },
      "Invalid number for ColorTemperature" },
    { "a pretty-printed ColorTemperature still loads",
      "Testing/SpecRef/RefDecC.xml",
      { { kWrap, "<ColorTemperature>", nullptr, "\n        " } },
      nullptr },
    { "a lutAtoB Matrix e1 with trailing text is refused",
      "Testing/hybrid/MultSpectralRGB.xml",
      { { kAppend, "<Matrix", "e1", "abc" } },
      "Error! - Failed to parse Matrix." },

    // IccMpeXml.cpp -- multiProcessElement readers.
    { "a FormulaSegment End with trailing text is refused",
      "Testing/Encoding/ISO22028-Encoded-sRGB.xml",
      { { kAppend, "<FormulaSegment", "End", "abc" } },
      "Invalid Start or End in SegmentedCurve segment" },
    { "a SampledCalculatorCurve FirstEntry with trailing text is refused",
      "Testing/Display/Rec2100HlgFullCC.xml",
      { { kAppend, "<SampledCalculatorCurve", "FirstEntry", "abc" } },
      "Invalid FirstEntry in Sampled Calculator Curve" },
    { "a SingleSampledCurve LastEntry with trailing text is refused",
      "Testing/hybrid/SC-Mid_Overprint.xml",
      { { kAppend, "<SingleSampledCurve", "LastEntry", "abc" } },
      "Invalid LastEntry in Simple Sampled Segment" },
    { "a DuplicateCurve Index of \"0abc\" is refused, not read as 0",
      "Testing/HDR/BT2100HlgSceneToDisplayLink.xml",
      { { kAppend, "<DuplicateCurve", "Index", "abc" } },
      "Invalid index for duplicate CurveSet Curve" },
    { "a DuplicateFunction Index of \"0abc\" is refused, not read as 0",
      "Testing/HDR/BT2100HlgSceneToDisplayLink.xml",
      { { kAppend, "<DuplicateFunction", "Index", "abc" } },
      "Invalid index for duplicate ToneMapFunction" },
    { "an EmissionMatrixElement Wavelengths start with trailing text is refused",
      "Testing/hybrid/LCDDisplay.xml",
      { { kAppend, "<EmissionMatrixElement", "start", "abc" } },
      "Invalid Spectral Range" },
    { "an EmissionObserverElement Wavelengths start with trailing text is refused",
      ".github/ci/test-data/ub-mpe-steps-emission-1900.xml",
      { { kReplace, "steps=\"360200\"", nullptr, "steps=\"11\"" },
        { kAppend, "<EmissionObserverElement", "start", "abc" } },
      "Invalid Spectral Range" },
    { "a ReflectanceObserverElement Wavelengths start with trailing text is refused",
      ".github/ci/test-data/ub-mpe-steps-reflectance-1903.xml",
      { { kReplace, "steps=\"360200\"", nullptr, "steps=\"11\"" },
        { kAppend, "<ReflectanceObserverElement", "start", "abc" } },
      "Invalid Spectral Range" },
    { "a CAM WhitePoint X with trailing text is refused",
      "Testing/PCC/Spec400_10_700-B_2deg-CAM.xml",
      { { kAppend, "<WhitePoint>", "X", "abc" } },
      "Invalid CAM WhitePoint XYZNumber" },
    { "a CAM Luminance of \"500.0abc\" is refused",
      "Testing/Calc/srgbCalc++Test.xml",
      { { kAppend, "<Luminance>", nullptr, "abc" } },
      "Invalid CAM Luminance" },
    { "a pretty-printed CAM Luminance still loads",
      "Testing/Calc/srgbCalc++Test.xml",
      { { kWrap, "<Luminance>", nullptr, "\n            " } },
      nullptr },
    { "a CAM ImpactSurround of \"0.69abc\" is refused",
      "Testing/Calc/srgbCalc++Test.xml",
      { { kAppend, "<ImpactSurround>", nullptr, "abc" } },
      "CAM ImpactSurround must be in [0.0, 1.0]" },
    { "a calculator Declare Position of \"0abc\" is refused",
      "Testing/Calc/srgbCalc++Test.xml",
      { { kAppend, "<Declare Name=\"rv\"", "Position", "abc" } },
      "Invalid Position in calculator variable declaration" },
  };

  // Generated documents: readers no tracked document reaches.
  struct GeneratedCase {
    const char *label;
    const char *tags;
    Op op;
    const char *message;
  };
  const std::vector<GeneratedCase> gen = {
    { "a chromaticity x of \"0.64abc\" is refused",
      kChromaticity, { kAppend, "<Channel", "x", "abc" },
      "Invalid number for chromaticityType Channel x or y" },
    { "a MeasurementBacking X with trailing text is refused",
      kMeasurement, { kAppend, "<MeasurementBacking", "X", "abc" },
      "Invalid number for MeasurementBacking X" },
    { "a viewingConditions SurroundXYZ Y with trailing text is refused",
      kViewingConditions, { kAppend, "<SurroundXYZ", "Y", "abc" },
      "Invalid number for SurroundXYZ Y" },
    { "a ChannelResponses X with trailing text is refused",
      kResponseCurve, { kAppend, "<ChannelResponses", "X", "abc" },
      "Invalid number for ChannelResponses X" },
    { "a Measurement MeasValue with trailing text is refused",
      kResponseCurve, { kAppend, "<Measurement ", "MeasValue", "abc" },
      "Invalid number for Measurement MeasValue" },
    { "a NamedColor a with trailing text is refused",
      kNamedColor, { kAppend, "<NamedColor ", "a", "abc" },
      "Invalid number for NamedColor a" },
    { "an XYZNamedColor Y with trailing text is refused",
      kNamedColor, { kAppend, "<XYZNamedColor", "Y", "abc" },
      "Invalid number for XYZNamedColor Y" },
    { "a lutAtoB TableData Precision of \"1abc\" is refused, not read as 1",
      kLutAtoB, { kAppend, "<TableData", "Precision", "abc" },
      "Invalid TableData Precision." },
  };

  int failures = 0;
  int index = 0;

  for (const Case &c : corpus) {
    const std::filesystem::path src = root / c.file;
    std::string source;
    if (!readFile(src, source)) {
      failures += fail(c.label, "could not read " + src.string());
      continue;
    }
    failures += runCase(c, source, src.parent_path(), scratch, index++);
  }

  for (const GeneratedCase &g : gen) {
    Case c = { g.label, nullptr, { g.op }, g.message };
    failures += runCase(c, generated(g.tags), scratch, scratch, index++);
  }

  // Refusals with no diagnostic of their own: the control must load and the
  // edited document must not.
  struct SilentCase {
    const char *label;
    const char *file;     // NULL = generated from tags
    const char *tags;
    Op op;
  };
  const std::vector<SilentCase> silent = {
    { "an 8-bit curve IdentitySize of \"256abc\" is refused",
      "Testing/V2/v2RgbLut8.xml", nullptr,
      { kAppend, "<Curve", "IdentitySize", "abc" } },
    { "a 16-bit curve IdentitySize of \"2abc\" is refused",
      "Testing/PCC/CustomLab_int-D65_2deg.xml", nullptr,
      { kAppend, "<Curve", "IdentitySize", "abc" } },
    { "a parametricCurve Reserved of \"7abc\" is refused",
      nullptr, kParametric,
      { kAppend, "<ParametricCurve", "Reserved", "abc" } },
  };

  for (const SilentCase &sc : silent) {
    std::string source;
    std::filesystem::path cwd = scratch;
    if (sc.file) {
      const std::filesystem::path src = root / sc.file;
      if (!readFile(src, source)) {
        failures += fail(sc.label, "could not read " + src.string());
        continue;
      }
      cwd = src.parent_path();
    }
    else {
      source = generated(sc.tags);
    }

    std::string edited = source;
    if (!applyOp(edited, sc.op)) {
      failures += fail(sc.label, std::string("anchor \"") + sc.op.anchor + "\" not found");
      continue;
    }

    LoadResult control, result;
    char name[64];
    std::snprintf(name, sizeof(name), "case-%02d-control.xml", index);
    const bool bWroteControl = loadDoc(source, scratch, name, cwd, control);
    std::snprintf(name, sizeof(name), "case-%02d-edited.xml", index);
    const bool bWroteEdited = loadDoc(edited, scratch, name, cwd, result);
    index++;

    if (!bWroteControl || !bWroteEdited)
      failures += fail(sc.label, "could not write or load a document");
    else if (!control.bLoaded)
      failures += fail(sc.label, "the unmodified document does not load: " + control.parseStr);
    else if (result.bLoaded)
      failures += fail(sc.label, "the edited document still loaded");
    else
      failures += pass(sc.label);
  }

  failures += chromaticityLegacyCase(scratch);

  if (failures) {
    std::fprintf(stderr, "%s: %d case(s) failed\n", kName, failures);
    return 1;
  }

  std::fprintf(stdout, "%s: all cases passed\n", kName);
  return 0;
}
