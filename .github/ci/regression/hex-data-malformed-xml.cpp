// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       hex-data-malformed-xml.cpp

    Contains:   CTest helper for malformed hex in an XML blob payload.  #2610.

    icXmlGetHexData() and icXmlGetHexDataSize() skip whatever they cannot read
    as a pair of hex digits, one character at a time.  A dataType payload of
    "0g11" was therefore accepted as the single byte 0x11 with no diagnostic,
    and an odd trailing digit was dropped, so after one conversion a malformed
    payload could not be told from a shorter intentional one.

    The skipping is not gratuitous: icXmlDumpHexData() writes the digits in
    pairs, 32 bytes to a line, so the decoder has to pass over the line breaks
    and indentation of its own output.  icXmlValidHexData() keeps exactly that
    and refuses the rest, and the blob parsers call it before decoding.

    Part 1 is the rule itself, including what must stay legal.  Part 2 drives
    it through the tag parsers that call it, by loading a profile document.
    Part 3 pins the deliberate exclusion: the profile header's ProfileID is
    NOT checked, because 176 tracked Testing profiles carry
    <ProfileID>1</ProfileID>, a single digit that decodes to no bytes at all.
    Whether that spelling should stay legal is a question for the maintainers
    (#2610), and this test freezes today's answer so a later change to it is
    deliberate rather than accidental.
*/

#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccUtilXml.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[hex-data-malformed-xml] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

struct HexCase {
  const char *text;
  bool bValid;
  const char *label;
};

const HexCase kCases[] = {
  { "",                          true,  "empty" },
  { "0011",                      true,  "one line of pairs" },
  { "00112233\n       44556677", true,  "the writer's own line break and indent" },
  { "  0011  ",                  true,  "leading and trailing whitespace" },
  { "00 11",                     true,  "whitespace between pairs" },
  { "0g11",                      false, "the reported payload" },
  { "001",                       false, "odd trailing digit" },
  { "1",                         false, "a single digit" },
  { "0011zz",                    false, "letters past the end of the alphabet" },
  { "00,11",                     false, "a separator that is not whitespace" },
  { "0 011",                     false, "whitespace inside a pair" },
};

/* A profile whose profileDescriptionTag carries the payload as HexTextData.
   icXmlParseLocalizedText() reports "no text" and "malformed" through the same
   false, and every caller turns "no text" into an empty string, so a refusal
   here has to be told apart or the payload silently vanishes. */
std::string mlucDoc(const char *payload)
{
  return std::string(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>4.30</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>Lab </PCS>\n"
    "    <CreationDateTime>now</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0000\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <profileDescriptionTag><multiLocalizedUnicodeType>\n"
    "      <LocalizedText LanguageCountry=\"enUS\"><HexTextData>") + payload +
    "</HexTextData></LocalizedText>\n"
    "    </multiLocalizedUnicodeType></profileDescriptionTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

/* A profile carrying one private dataType tag with the given payload. */
std::string profileDoc(const char *payload, const char *profileId)
{
  std::string id;
  if (profileId) {
    id = std::string("    <ProfileID>") + profileId + "</ProfileID>\n";
  }
  return std::string(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>4.30</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>Lab </PCS>\n"
    "    <CreationDateTime>now</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0000\" Z=\"0.8249\"/></PCSIlluminant>\n") +
    id +
    "  </Header>\n"
    "  <Tags>\n"
    "    <PrivateTag TagSignature=\"priv\">\n"
    "      <dataType><Data Flag=\"binary\">" + payload + "</Data></dataType>\n"
    "    </PrivateTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

/* Writes the document to a temporary file and loads it, the way iccFromXml
   does.  Returns whether it loaded, with the parser's report. */
bool loadDoc(const std::string &doc, std::string &parseStr, const char *label)
{
  std::string path = std::string("hex-data-malformed-xml-") + label + ".xml";
  for (size_t i = 0; i < path.size(); i++) {
    if (path[i] == ' ')
      path[i] = '-';
  }

  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) {
    check(false, label, "could not write the document");
    return false;
  }
  std::fwrite(doc.c_str(), 1, doc.size(), f);
  std::fclose(f);

  CIccProfileXml profile;
  bool bLoaded = profile.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  return bLoaded;
}

} // namespace

int main()
{
  // The XML tag and element extensions have to be registered, as iccFromXml
  // does, or every tag loads as its plain library form.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  /* 1. The rule. */
  for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
    const HexCase &c = kCases[i];
    bool bValid = icXmlValidHexData(c.text);
    if (bValid != c.bValid) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "icXmlValidHexData() said %s", bValid ? "valid" : "invalid");
      check(false, c.label, buf);
    }
  }
  check(!icXmlValidHexData(NULL), "null", "a null pointer was called valid");

  /* 2. Through the dataType parser. */
  {
    std::string parseStr;
    check(!loadDoc(profileDoc("0g11", NULL), parseStr, "malformed"), "dataType 0g11",
          "the profile loaded");
    check(has(parseStr, "Malformed hex"), "dataType 0g11",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(profileDoc("001", NULL), parseStr, "odd"), "dataType odd digit",
          "the profile loaded");
    check(has(parseStr, "Malformed hex"), "dataType odd digit",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  /* The control: the same document with a well formed payload, and one
     spelled the way the writer spells a payload of more than 32 bytes. */
  {
    std::string parseStr;
    check(loadDoc(profileDoc("0011", NULL), parseStr, "control"), "dataType control",
          ("a well formed payload was refused: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    const char *wrapped =
      "\n       00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"
      "\n       00112233445566778899aabbccddeeff\n      ";
    check(loadDoc(profileDoc(wrapped, NULL), parseStr, "wrapped"), "dataType wrapped",
          ("the writer's own spelling was refused: " + parseStr).c_str());
  }

  /* 2b. Through the localized text parser, whose callers treat a false return
     as "no text" and substitute an empty string. */
  {
    std::string parseStr;
    check(!loadDoc(mlucDoc("0g11"), parseStr, "mluc-malformed"), "HexTextData 0g11",
          "the profile loaded, so the payload was dropped silently");
    check(has(parseStr, "Malformed hex"), "HexTextData 0g11",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(loadDoc(mlucDoc("0011"), parseStr, "mluc-control"), "HexTextData control",
          ("a well formed payload was refused: " + parseStr).c_str());
  }

  /* 3. The header's ProfileID is deliberately not checked: 176 tracked
     Testing profiles carry a single digit there. */
  {
    std::string parseStr;
    check(loadDoc(profileDoc("0011", "1"), parseStr, "profileid-short"), "ProfileID single digit",
          ("a corpus spelling was refused: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(loadDoc(profileDoc("0011", "34562ABF994CCD066D2C5721D0D68C5D"), parseStr, "profileid-full"),
          "ProfileID full", ("a full profile ID was refused: " + parseStr).c_str());
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
