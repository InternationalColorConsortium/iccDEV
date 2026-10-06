// Regression for #2770: the JSON writer and reader for textType and for the
// ASCII field of textDescriptionType replaced every byte outside 0x20..0x7E
// with '?', which took tab, LF and CR with it. A charTargetTag holds CGATS
// data, CR LF line ends and tab-separated fields, so ICC -> JSON -> ICC
// turned PSO_SNP_Paper_eci.icc's 1,630 CR LF pairs into 3,260 question marks
// and a private textType's every newline into '?'. Both are 7-bit ASCII
// (ICC.1:2022 10.24); the serialiser escapes the three as \t, \n and \r, so
// keeping them puts no raw control byte on any console.
//
// The fix keeps tab, LF and CR and still maps every other control character,
// DEL and every byte above 7 bits to '?'. This test pins both halves on the
// writer and on the reader, and that the emitted document carries no raw
// control byte.
//
// Returns 0 on success; the number of failed checks otherwise (each printed).

#include "IccTagJson.h"
#include "IccUtilJson.h"

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
    std::fprintf(stderr, "[json-text-control-bytes] FAIL: %s\n", what);
  }
}

bool noRawControlByte(const std::string &doc)
{
  for (std::string::size_type i = 0; i < doc.size(); i++) {
    if (static_cast<unsigned char>(doc[i]) < 0x20)
      return false;
  }
  return true;
}

const char kCgats[] = "ISO12642-2\r\nORIGINATOR\t\"Fogra, www.fogra.org\"\r\nEND_DATA\r\n";

// --- textType: writer keeps CR LF and tab, the document is escaped, the reader restores them
void testTextRoundTrip()
{
  CIccTagJsonText src;
  src.SetText(kCgats);

  IccJson j;
  check(src.ToJson(j), "textType ToJson succeeds");
  check(j.contains("text") && j["text"].is_string(), "textType emits a text string");
  check(j.contains("text") && j["text"].get<std::string>() == kCgats,
        "textType writer keeps CR, LF and tab");

  std::string doc = j.dump();
  check(noRawControlByte(doc), "textType document carries no raw control byte");
  check(doc.find("\\r\\n") != std::string::npos && doc.find("\\t") != std::string::npos,
        "textType document escapes CR LF and tab");

  CIccTagJsonText dst;
  std::string parseStr;
  check(dst.ParseJson(IccJson::parse(doc), parseStr), "textType ParseJson succeeds");
  check(dst.GetText() && !std::strcmp(dst.GetText(), kCgats),
        "textType reader restores CR, LF and tab");
}

// --- textDescriptionType: the ASCII field gets the same treatment
void testDescriptionRoundTrip()
{
  const char kDesc[] = "line one\r\nline two\ttabbed";

  CIccTagJsonTextDescription src;
  src.SetText(kDesc);

  IccJson j;
  check(src.ToJson(j), "textDescriptionType ToJson succeeds");
  check(j.contains("description") && j["description"].get<std::string>() == kDesc,
        "textDescriptionType writer keeps CR, LF and tab");

  CIccTagJsonTextDescription dst;
  std::string parseStr;
  check(dst.ParseJson(IccJson::parse(j.dump()), parseStr), "textDescriptionType ParseJson succeeds");
  check(dst.GetText() && !std::strcmp(dst.GetText(), kDesc),
        "textDescriptionType reader restores CR, LF and tab");
}

// --- the hardening stays: ESC, DEL and a byte above 7 bits still become '?'
void testOtherBytesStillReplaced()
{
  const char kRaw[] = "a\x1b[31mb\x7f" "c\xc3\xa9" "d\r\n";
  const char kWant[] = "a?[31mb?c??d\r\n";

  CIccTagJsonText src;
  src.SetText(kRaw);

  IccJson j;
  check(src.ToJson(j), "hardening: ToJson succeeds");
  check(j.contains("text") && j["text"].get<std::string>() == kWant,
        "hardening: writer still maps ESC, DEL and high bytes to '?' and keeps CR LF");

  // The reader applies the same rule to a document that spells the bytes as escapes.
  const char kDoc[] = "{\"text\":\"a\\u001b[31mb\\u007fc\\r\\n\"}";
  CIccTagJsonText dst;
  std::string parseStr;
  check(dst.ParseJson(IccJson::parse(kDoc), parseStr), "hardening: ParseJson succeeds");
  check(dst.GetText() && !std::strcmp(dst.GetText(), "a?[31mb?c\r\n"),
        "hardening: reader maps ESC and DEL to '?' and keeps CR LF");
}

} // namespace

int main()
{
  testTextRoundTrip();
  testDescriptionRoundTrip();
  testOtherBytesStillReplaced();

  if (g_fail)
    std::fprintf(stderr, "[json-text-control-bytes] %d check(s) failed\n", g_fail);
  else
    std::printf("[json-text-control-bytes] all checks passed\n");
  return g_fail;
}
