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

    profileSequenceIdentifierType had two more reader/writer asymmetries.  Its
    fixed 16-byte ProfileIdDesc id ignored the decoder's byte count and skipped
    malformed characters, while its LocalizedText search started at the parent
    instead of its children and therefore discarded every description.

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
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

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

/* profileSequenceDescType embeds complete manufacturer and model description
   tags. An empty mluc is the placeholder ICC.1:2022 Table 70 permits, but a
   present malformed record must not be silently converted into one. */
std::string pseqMlucDoc(const char *payload, bool testModel, bool unknown = false,
                        const char *directContent = NULL)
{
  const std::string tested = directContent
    ? std::string("<multiLocalizedUnicodeType>") + directContent +
      "</multiLocalizedUnicodeType>"
    : unknown
    ? "<multiLocalizedUnicodeType><Unknown/></multiLocalizedUnicodeType>"
    : payload
    ? std::string("<multiLocalizedUnicodeType><LocalizedText LanguageCountry=\"enUS\">"
                  "<HexTextData>") + payload +
                  "</HexTextData></LocalizedText></multiLocalizedUnicodeType>"
    : "<multiLocalizedUnicodeType></multiLocalizedUnicodeType>";
  const std::string valid =
    "<multiLocalizedUnicodeType><LocalizedText LanguageCountry=\"enUS\">valid"
    "</LocalizedText></multiLocalizedUnicodeType>";
  const std::string manufacturer = testModel ? valid : tested;
  const std::string model = testModel ? tested : valid;

  return std::string(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>4.30</ProfileVersion>\n"
    "    <ProfileDeviceClass>link</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>RGB </PCS>\n"
    "    <CreationDateTime>now</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0000\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <profileSequenceDescTag><profileSequenceDescType><ProfileSequence><ProfileDesc>\n"
    "      <DeviceManufacturerSignature>test</DeviceManufacturerSignature>\n"
    "      <DeviceModelSignature>test</DeviceModelSignature>\n"
    "      <DeviceAttributes ReflectiveOrTransparency=\"reflective\" GlossyOrMatte=\"glossy\""
    " MediaPolarity=\"positive\" MediaColour=\"colour\"/>\n"
    "      <Technology></Technology>\n"
    "      <DeviceManufacturer>") + manufacturer + "</DeviceManufacturer>\n" +
    "      <DeviceModel>" + model + "</DeviceModel>\n" +
    "    </ProfileDesc></ProfileSequence></profileSequenceDescType></profileSequenceDescTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

std::string pseqMlucEntityDoc(bool testModel)
{
  std::string doc = pseqMlucDoc(NULL, testModel, false, "&placeholder;");
  size_t root = doc.find("<IccProfile>");

  return doc.insert(root, "<!DOCTYPE IccProfile [<!ENTITY placeholder \"entity\">]>\n");
}

/* profileSequenceIdentifierType carries a fixed 16-byte hexadecimal ID and a
   nested mluc description for each entry. The writer always emits 32 digits
   and places LocalizedText below ProfileIdDesc. */
std::string psidDoc(const char *id, const char *text)
{
  const std::string idAttribute = id ? std::string(" id=\"") + id + "\"" : "";

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
    "    <profileSequenceIdentifierTag><profileSequenceIdentifierType>\n"
    "      <ProfileSequenceId><ProfileIdDesc") + idAttribute + ">\n"
    "        <LocalizedText LanguageCountry=\"enUS\">" + text + "</LocalizedText>\n"
    "      </ProfileIdDesc></ProfileSequenceId>\n"
    "    </profileSequenceIdentifierType></profileSequenceIdentifierTag>\n"
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

bool writeTempDoc(const std::string &doc, std::string &path, const char *label)
{
#if defined(_WIN32)
  char tempPath[MAX_PATH];
  char tempFile[MAX_PATH];
  DWORD tempPathLength = GetTempPathA(sizeof(tempPath), tempPath);
  if (!tempPathLength || tempPathLength >= sizeof(tempPath) ||
      !GetTempFileNameA(tempPath, "icx", 0, tempFile)) {
    check(false, label, "could not create a temporary document");
    return false;
  }

  path = tempFile;
  FILE *f = std::fopen(path.c_str(), "wb");
#else
  std::string name = std::string("/tmp/hex-data-malformed-xml-") + label + "-XXXXXX";
  std::vector<char> writable(name.begin(), name.end());
  writable.push_back('\0');

  int fd = mkstemp(writable.data());
  if (fd == -1) {
    check(false, label, "could not create a temporary document");
    return false;
  }

  path = writable.data();
  FILE *f = fdopen(fd, "wb");
#endif
  if (!f) {
#if !defined(_WIN32)
    close(fd);
#endif
    std::remove(path.c_str());
    check(false, label, "could not open the temporary document");
    return false;
  }
  bool writeOk = std::fwrite(doc.c_str(), 1, doc.size(), f) == doc.size();
  bool closeOk = std::fclose(f) == 0;
  if (!writeOk || !closeOk) {
    std::remove(path.c_str());
    check(false, label, "could not write the complete document");
    return false;
  }

  return true;
}

/* Writes the document to a temporary file and loads it, the way iccFromXml
   does.  Returns whether it loaded, with the parser's report. */
bool loadDoc(const std::string &doc, std::string &parseStr, const char *label)
{
  std::string path;
  if (!writeTempDoc(doc, path, label))
    return false;

  CIccProfileXml profile;
  bool bLoaded = profile.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  return bLoaded;
}

bool roundTripDoc(const std::string &doc, std::string &xml, std::string &parseStr, const char *label)
{
  std::string path;
  if (!writeTempDoc(doc, path, label))
    return false;

  CIccProfileXml profile;
  bool bLoaded = profile.LoadXml(path.c_str(), "", &parseStr);
  bool bSerialized = bLoaded && profile.ToXml(xml);
  std::remove(path.c_str());
  return bSerialized;
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

  /* 2c. The profile-sequence parser must propagate those nested failures for
     both embedded descriptions while retaining the spec-defined placeholder. */
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucDoc("0g11", false), parseStr, "pseq-mfg-malformed"),
          "pseq manufacturer HexTextData 0g11", "the profile loaded and lost the payload");
    check(has(parseStr, "Malformed hex"), "pseq manufacturer HexTextData 0g11",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucDoc("0g11", true), parseStr, "pseq-model-malformed"),
          "pseq model HexTextData 0g11", "the profile loaded and lost the payload");
    check(has(parseStr, "Malformed hex"), "pseq model HexTextData 0g11",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(loadDoc(pseqMlucDoc(NULL, false), parseStr, "pseq-placeholder"),
          "pseq empty mluc placeholder", ("the legal placeholder was refused: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(loadDoc(pseqMlucDoc(NULL, false, false, " \n\t "), parseStr, "pseq-placeholder-whitespace"),
          "pseq whitespace mluc placeholder",
          ("formatting whitespace was treated as malformed: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucDoc(NULL, false, true), parseStr, "pseq-unknown-child"),
          "pseq mluc unknown child", "a present malformed child was treated as an empty placeholder");
  }
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucEntityDoc(false), parseStr, "pseq-mfg-entity"),
          "pseq manufacturer entity", "an entity reference was treated as an empty placeholder");
  }
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucDoc(NULL, false, false, "garbage"), parseStr, "pseq-mfg-text"),
          "pseq manufacturer direct text", "nonblank text was treated as an empty placeholder");
  }
  {
    std::string parseStr;
    check(!loadDoc(pseqMlucDoc(NULL, true, false, "<![CDATA[garbage]]>"), parseStr, "pseq-model-cdata"),
          "pseq model direct CDATA", "nonblank CDATA was treated as an empty placeholder");
  }

  /* 2d. The profile-sequence identifier parser must validate the complete
     fixed-width ID and start its LocalizedText search at the entry's children. */
  {
    std::string parseStr;
    check(!loadDoc(psidDoc("0g112233445566778899aabbccddeeff", "malformed id"), parseStr,
                   "psid-malformed-id"),
          "psid malformed id", "the profile loaded and changed the identifier");
    check(has(parseStr, "Invalid ProfileIdDesc id"), "psid malformed id",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(psidDoc(NULL, "missing id"), parseStr, "psid-missing-id"),
          "psid missing id", "the profile loaded a missing fixed-width identifier");
    check(has(parseStr, "Invalid ProfileIdDesc id"), "psid missing id",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(psidDoc("", "empty id"), parseStr, "psid-empty-id"),
          "psid empty id", "the profile loaded an empty fixed-width identifier");
    check(has(parseStr, "Invalid ProfileIdDesc id"), "psid empty id",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(psidDoc("00112233", "short id"), parseStr, "psid-short-id"),
          "psid short id", "the profile loaded a short fixed-width identifier");
    check(has(parseStr, "Invalid ProfileIdDesc id"), "psid short id",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!loadDoc(psidDoc("00112233445566778899aabbccddeeff00", "oversized id"), parseStr,
                   "psid-oversized-id"),
          "psid oversized id", "the profile loaded a truncated fixed-width identifier");
    check(has(parseStr, "Invalid ProfileIdDesc id"), "psid oversized id",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr, xml;
    check(roundTripDoc(psidDoc("00112233445566778899aabbccddeeff", "preserved description"),
                       xml, parseStr, "psid-control"),
          "psid control", ("a valid entry was refused: " + parseStr).c_str());
    check(has(xml, "id=\"00112233445566778899AABBCCDDEEFF\""), "psid control",
          "the fixed-width identifier did not round-trip");
    check(has(xml, "preserved description"), "psid control",
          "the nested LocalizedText was discarded");
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
