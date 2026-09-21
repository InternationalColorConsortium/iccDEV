// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       hex-data-malformed-json.cpp

    Contains:   CTest helper for malformed hex in a JSON blob payload.  #2610.

    The JSON decoder skips whatever it cannot read as a pair of hex digits,
    exactly as the XML one does, so a dataType payload of "0g11" was accepted
    as the single byte 0x11 with no diagnostic.  icJsonDumpHexData() writes
    pairs with nothing between them; whitespace between pairs is still accepted
    so a hand-written document, or one carried over from the XML side, still
    loads.

    Part 1 is the rule, part 2 drives it through the dataType parser that calls
    it.  The header's profileID is deliberately left unchecked, for the reason
    given in the XML twin, iccdev.hex-data-malformed-xml; the two cannot share
    a translation unit.
*/

#include "IccTagJson.h"
#include "IccUtilJson.h"

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
    std::fprintf(stderr, "[hex-data-malformed-json] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

struct HexCase {
  const char *text;
  bool bValid;
  const char *label;
};

const HexCase kCases[] = {
  { "",             true,  "empty" },
  { "0011",         true,  "pairs" },
  { "00112233aabb", true,  "the writer's own spelling" },
  { "00 11",        true,  "whitespace between pairs" },
  { "  0011  ",     true,  "leading and trailing whitespace" },
  { "0g11",         false, "the reported payload" },
  { "001",          false, "odd trailing digit" },
  { "1",            false, "a single digit" },
  { "0011zz",       false, "letters past the end of the alphabet" },
  { "00,11",        false, "a separator that is not whitespace" },
  { "0 011",        false, "whitespace inside a pair" },
};

/* One dataType tag with the given payload. */
bool parseData(const char *payload, std::string &parseStr)
{
  std::string doc = std::string("{\"type\": \"dataType\", \"dataFlag\": 0, \"data\": \"") + payload + "\"}";
  CIccTagJsonTagData tag;
  return tag.ParseJson(IccJson::parse(doc), parseStr);
}

} // namespace

int main()
{
  /* 1. The rule. */
  for (size_t i = 0; i < sizeof(kCases) / sizeof(kCases[0]); i++) {
    const HexCase &c = kCases[i];
    bool bValid = icJsonValidHexData(c.text);
    if (bValid != c.bValid) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "icJsonValidHexData() said %s", bValid ? "valid" : "invalid");
      check(false, c.label, buf);
    }
  }
  check(!icJsonValidHexData(NULL), "null", "a null pointer was called valid");

  /* 2. Through the dataType parser. */
  {
    std::string parseStr;
    check(!parseData("0g11", parseStr), "dataType 0g11", "the tag parsed");
    check(has(parseStr, "Malformed hex"), "dataType 0g11",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(!parseData("001", parseStr), "dataType odd digit", "the tag parsed");
    check(has(parseStr, "Malformed hex"), "dataType odd digit",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    check(parseData("00112233", parseStr), "dataType control",
          ("a well formed payload was refused: " + parseStr).c_str());
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
