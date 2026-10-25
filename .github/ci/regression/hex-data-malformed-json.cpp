/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
    File:       hex-data-malformed-json.cpp

    Contains:   CTest helper for malformed hex in a JSON blob payload (#2610)
                and in a profileSequenceIdentifierType profileId (#2826).

    The JSON decoder skips whatever it cannot read as a pair of hex digits,
    exactly as the XML one does, so a dataType payload of "0g11" was accepted
    as the single byte 0x11 with no diagnostic.  icJsonDumpHexData() writes
    pairs with nothing between them; whitespace between pairs is still accepted
    so a hand-written document, or one carried over from the XML side, still
    loads.

    Part 1 is the rule, part 2 drives it through the dataType parser that calls
    it.  The two cannot share a translation unit with the XML twin,
    iccdev.hex-data-malformed-xml.

    Part 4 is the header's ProfileID (#2828): whole hex pairs, at most 16
    bytes, with "", "0" and "1" kept legal as "no ID stored", as in XML.

    Part 3 is profileSequenceIdentifierType (#2826).  Each entry carries a fixed
    16-byte profile ID, written as 32 hex digits, and the reader handed whatever
    it was given to the decoder: "0g11" became 0x11 and fifteen zero bytes, and
    the profile was written.  A profileId that is present must now be valid hex
    for exactly 16 bytes, which is what the XML reader requires of its id
    attribute.  An entry with no profileId, or a null one, still loads with an
    all-zero ID; that is unchanged and is pinned so it stays a decision.
*/

#include "IccProfileJson.h"
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

/* One profileSequenceIdentifierType tag with a single entry.  idMember is the
   whole "profileId" member, or empty to leave the key out.  The tag is the
   caller's so that a case which loads can be written back out. */
bool parsePsid(CIccTagJsonProfileSequenceId &tag, const std::string &idMember, std::string &parseStr)
{
  std::string doc = std::string("{\"type\": \"profileSequenceIdentifierType\", \"profileSequenceId\": [{") +
                    idMember + (idMember.empty() ? "" : ", ") +
                    "\"localizedStrings\": [{\"language\": \"en\", \"country\": \"US\", "
                    "\"text\": \"kept description\"}]}]}";
  return tag.ParseJson(IccJson::parse(doc), parseStr);
}

/* A profileId the reader must refuse, with its own diagnostic. */
void refusePsid(const char *idMember, const char *label)
{
  CIccTagJsonProfileSequenceId tag;
  std::string parseStr;
  check(!parsePsid(tag, idMember, parseStr), label, "the tag parsed");
  check(has(parseStr, "Invalid profileId"), label,
        ("no diagnostic, parseStr was: " + parseStr).c_str());
}

/* The entry as ToJson() writes it back, or "" if the tag did not load. */
std::string writtenPsid(const std::string &idMember, std::string &parseStr)
{
  CIccTagJsonProfileSequenceId tag;
  if (!parsePsid(tag, idMember, parseStr))
    return "";
  IccJson j;
  if (!tag.ToJson(j))
    return "";
  return j.dump();
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

  /* 3. profileSequenceIdentifierType.  The first is the reported payload. */
  refusePsid("\"profileId\": \"0g11\"", "psid 0g11");

  /* Two characters the decoder skips, then a complete ID.  The decoder reads
     this as that ID, so only the validity check can refuse it: the byte count
     is 16.  The other malformed cases are also the wrong length. */
  refusePsid("\"profileId\": \"0g00112233445566778899aabbccddeeff\"", "psid junk before a full id");

  refusePsid("\"profileId\": \"00112233\"", "psid short id");

  /* 17 bytes.  The decoder stops at 16, so only the byte count can refuse it. */
  refusePsid("\"profileId\": \"00112233445566778899aabbccddeeff00\"", "psid oversized id");

  /* 33 digits.  The decoder reads 16 pairs and skips the odd one, so this too
     is refused by the validity check alone. */
  refusePsid("\"profileId\": \"00112233445566778899aabbccddeeff0\"", "psid odd trailing digit");

  /* A complete ID, a NUL, then text that is not hex.  The helpers stop at the
     NUL, so without the reader's own test this loads as the 32 digits. */
  refusePsid("\"profileId\": \"00112233445566778899aabbccddeeff\\u0000zz\"", "psid embedded NUL");

  refusePsid("\"profileId\": \"\"", "psid empty id");
  refusePsid("\"profileId\": 11", "psid id that is not a string");

  {
    /* The control: lower-case digits load and come back as the writer spells
       them, with the description intact. */
    std::string parseStr;
    std::string out = writtenPsid("\"profileId\": \"00112233445566778899aabbccddeeff\"", parseStr);
    check(has(out, "\"profileId\":\"00112233445566778899AABBCCDDEEFF\""), "psid control",
          ("a valid id did not survive, parseStr: " + parseStr + " json: " + out).c_str());
    check(has(out, "kept description"), "psid control", "the description was lost");
  }
  {
    /* Whitespace between pairs is accepted, as it is in a blob payload. */
    std::string parseStr;
    std::string out = writtenPsid("\"profileId\": \"00112233 44556677 8899aabb ccddeeff\"", parseStr);
    check(has(out, "\"profileId\":\"00112233445566778899AABBCCDDEEFF\""), "psid spaced id",
          ("a spaced id did not survive, parseStr: " + parseStr + " json: " + out).c_str());
  }
  {
    /* No profileId at all: unchanged, an all-zero ID. */
    std::string parseStr;
    std::string out = writtenPsid("", parseStr);
    check(has(out, "\"profileId\":\"00000000000000000000000000000000\""), "psid absent id",
          ("an entry without a profileId did not load with a zero id, parseStr: " + parseStr + " json: " + out).c_str());
  }
  {
    /* null, which iccDumpProfile's JSON report writes for an ID that was not
       calculated: unchanged as well, the same all-zero ID. */
    std::string parseStr;
    std::string out = writtenPsid("\"profileId\": null", parseStr);
    check(has(out, "\"profileId\":\"00000000000000000000000000000000\""), "psid null id",
          ("an entry with a null profileId did not load with a zero id, parseStr: " + parseStr + " json: " + out).c_str());
  }

  /* 4. The header's ProfileID. */
  {
    struct { const char *id; bool ok; } cases[] = {
      { "1", true }, { "0", true }, { "", true },
      { "34562ABF994CCD066D2C5721D0D68C5D", true },
      { "0g11", false }, { "0g112233445566778899aabbccddeeff", false }, { "001", false },
      { "34562ABF994CCD066D2C5721D0D68C5D00", false },
    };
    for (const auto &c : cases) {
      IccJson doc = { { "IccProfile", {
        { "Header", { { "ProfileVersion", "4.30" }, { "ProfileDeviceClass", "mntr" },
                      { "DataColourSpace", "RGB " }, { "PCS", "Lab " },
                      { "ProfileID", c.id } } },
        { "Tags", IccJson::array() } } } };
      CIccProfileJson profile;
      std::string parseStr;
      bool ok = profile.ParseJson(doc, parseStr);
      std::string label = std::string("header ProfileID '") + c.id + (c.ok ? "' loads" : "' refused");
      check(ok == c.ok, label.c_str(), (c.ok ? "refused: " + parseStr : std::string("the profile loaded")).c_str());
      if (!c.ok)
        check(has(parseStr, "Invalid ProfileID"), label.c_str(), "no diagnostic");
    }
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
