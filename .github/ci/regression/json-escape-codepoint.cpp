/*
    File:       json-escape-codepoint.cpp

    Contains:   CTest for icJsonEscape() appending well-formed UTF-8 unchanged,
                so the --json / --evidence-json sinks emitted live U+202E and
                the C1 controls the text sink escapes (#2454).

    icJsonEscape() escaped the C0 controls and the JSON metacharacters, then
    validated any non-ASCII sequence and appended its BYTES verbatim.  That is
    correct JSON -- the document parses and a machine consumer decodes the same
    string either way -- but it means one filename reached two sinks in the same
    build and got opposite answers:

        iccPawgReport <name>          text  -> \u202E  (escaped since #2419)
        iccPawgReport <name> --json   JSON  -> raw E2 80 AE bytes

    It stops being harmless when a human cat-s the output, which is the threat
    model #2406 and #2437 established for this repo (CVE-2021-42574, Trojan
    Source).  The fix escapes by codepoint using icDecodeUtf8() -- the decoder
    #2437 added -- so both sinks now share ONE whitelist.

    WHY THE SURROGATE-PAIR CASE IS NOT OPTIONAL.  JSON has no \U escape
    (RFC 8259 s7): a codepoint above the BMP must be spelled as a UTF-16
    surrogate pair.  The text sink spells U+1F600 as \U0001F600, which is valid
    there and INVALID JSON here, so an implementation that simply reused the
    text sink's spelling would emit a document that does not parse.  Case 4
    fails against exactly that mistake as well as against master.

    THE CONTROLS ARE THE POINT.  Cases 7-12 pass on BOTH builds.  Without them a
    "fix" that escaped every byte >= 0x80 per byte (re-introducing the
    \xC3\xA9 spelling #2437 removed), or that stopped escaping the JSON
    metacharacters, or that lost the U+FFFD handling for ill-formed input, would
    still satisfy a suite that only checked cases 1-6.  Case 12 in particular
    pins that a sequence truncated by the terminating NUL is not read past.

    WHAT EVERY CASE ACTUALLY ASSERTS, stated precisely because the distinction
    matters: the escaped output equals a hand-written expected spelling, and the
    output is pure ASCII.  It asserts the FIXED form is PRESENT rather than only
    that the raw bytes are absent.  It does NOT link a JSON parser and does not
    round-trip the result -- the surrogate-pair spelling in case 4 is checked
    against a literal, not against a decoder.  That round-trip was verified
    out-of-band when the fix landed (every case parsed with Python's json module
    and decoded back to the input), but it is not what this binary does, and a
    reader should not believe otherwise.
*/

#include <cstdio>
#include <cstring>
#include <string>
#include "IccCmdLineUtil.h"

static int g_pass = 0;
static int g_fail = 0;

static bool isPureAscii(const std::string& s)
{
  for (size_t i = 0; i < s.size(); i++) {
    if ((unsigned char)s[i] >= 0x80)
      return false;
  }
  return true;
}

static void check(const char* label, const std::string& input, const char* expected)
{
  std::string got = icJsonEscape(input);
  bool ok = (got == expected);
  bool ascii = isPureAscii(got);

  if (ok && ascii) {
    // A passing result is ASCII by construction, so it needs no sanitizing.
    printf("PASS  %-28s -> %s\n", label, got.c_str());
    g_pass++;
  }
  else {
    // The failing value is sanitized before it is printed.  On a regressed
    // build `got` is exactly the raw terminal-active bytes this test exists to
    // catch -- printing it verbatim would write a live U+202E RIGHT-TO-LEFT
    // OVERRIDE into the CI log and reverse the rest of the line in any terminal
    // that cats it, which is the CVE-2021-42574 exposure #2406/#2419/#2437
    // closed.  A diagnostic must not reintroduce the defect it reports.
    printf("FAIL  %-28s -> %s\n", label, icSanitizeConsoleText(got).c_str());
    if (!ok)
      printf("        expected: %s\n", expected);
    if (!ascii)
      printf("        output is not pure ASCII\n");
    g_fail++;
  }
}

int main()
{
  printf("== #2454: non-ASCII must be escaped by codepoint, not appended raw ==\n");

  // U+202E RIGHT-TO-LEFT OVERRIDE -- the bidi attack the text sink already blocks.
  check("1. U+202E bidi override", std::string("inv") + "\xE2\x80\xAE" + "cod.icc",
        "inv\\u202ecod.icc");

  // U+0085 NEL, a C1 control; UTF-8 spells it C2 85 and a terminal acts on it.
  check("2. U+0085 C1 control", std::string("a") + "\xC2\x85" + "b.icc",
        "a\\u0085b.icc");

  // A legitimate accented name: escaped as ONE codepoint, not two raw bytes.
  check("3. U+00E9 accented", std::string("\xC3\xA9") + "cran.icc",
        "\\u00e9cran.icc");

  // Above the BMP: MUST be a surrogate pair, not \U0001f600.
  check("4. U+1F600 surrogate pair", std::string("\xF0\x9F\x98\x80") + ".icc",
        "\\ud83d\\ude00.icc");

  // A 3-byte BMP codepoint.
  check("5. U+8272 CJK", std::string("\xE8\x89\xB2") + ".icc",
        "\\u8272.icc");

  // U+007F DEL is terminal-active and icSanitizeConsoleText() escapes it, so
  // leaving it raw here would be the same sink divergence this fix is about,
  // just below 0x80 instead of above it.  Master emits DEL raw, so this fails
  // there -- it belongs with the cases above, not with the controls below.
  check("6. U+007F DEL", std::string("a\x7f" "b.icc"),
        "a\\u007fb.icc");

  printf("== controls: these pass on BOTH builds and must not move ==\n");

  check("7. plain ASCII untouched", std::string("plain-Profile.icc"),
        "plain-Profile.icc");

  check("8. JSON metacharacters", std::string("a\"b\\c"),
        "a\\\"b\\\\c");

  check("9. short-form C0 escapes", std::string("a\tb\nc\rd\be\ff"),
        "a\\tb\\nc\\rd\\be\\ff");

  check("10. other C0 as \\u00XX", std::string("a\x01" "b"),
        "a\\u0001b");

  // ED A0 80 is a UTF-16 surrogate, ill-formed in UTF-8; each byte becomes
  // U+FFFD and the scan advances one byte at a time.
  check("11. ill-formed surrogate", std::string("a") + "\xED\xA0\x80" + "b",
        "a\\ufffd\\ufffd\\ufffdb");

  // A 3-byte lead with only two bytes before the terminator: the decoder must
  // stop at the NUL rather than read past it.
  check("12. truncated at terminator", std::string("a") + "\xE8\x89",
        "a\\ufffd\\ufffd");

  printf("\ncases run: %d  passed: %d  failed: %d\n",
         g_pass + g_fail, g_pass, g_fail);
  return g_fail ? 1 : 0;
}
