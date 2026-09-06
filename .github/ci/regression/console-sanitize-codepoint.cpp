// #2420 -- icSanitizeConsoleText() escapes by codepoint, not by byte.
//
// #2419 routed the four iccApply* tools' diagnostics through the library helper
// icSanitizeConsoleText() (IccProfLib/IccFileUtil.h).  The pre-merge note on that PR
// raised the cost: the helper escaped every byte >= 0x7f, so a legitimate UTF-8 path
// rendered as '\xC3\xA9cran-Profil.icc' -- two hex escapes for one character, in
// success output as well as errors.
//
// The obvious fix -- pass valid UTF-8 through -- is a net regression, and this test
// exists mostly to keep that from being re-tried:
//
//   * U+202E RIGHT-TO-LEFT OVERRIDE is itself valid UTF-8.  A name carrying it renders
//     reversed from that point, so 'gnp.<RLO>cci' displays as 'icc.png' -- Trojan
//     Source, CVE-2021-42574.  The byte bound blocked that by accident.
//   * U+0080..U+009F are the C1 controls, which a terminal still acts on, and which
//     UTF-8 spells as the perfectly valid two-byte sequences C2 80..C2 9F.  U+009B is
//     CSI: the same escape the #2406 fix was written to neutralise, wearing UTF-8.
//   * Nothing in this tree sets a console code page, so raw UTF-8 is mojibake on the
//     four Windows legs.
//
// So the whitelist is kept -- everything that is not printable ASCII is still escaped --
// and only the SPELLING of the escape changes: one \uXXXX (or \UXXXXXXXX above the BMP)
// per codepoint instead of one \xHH per byte.  Malformed UTF-8 has no codepoint to name
// and keeps the per-byte form, one byte at a time, so an invalid lead byte cannot
// swallow the bytes after it.
//
// Two properties matter more than any single expected string, and both are checked over
// the whole range rather than sampled:
//
//   1. ASCII in, identical ASCII out.  Only bytes >= 0x80 reach the new path, so every
//      existing caller and every existing assertion about ASCII output is untouched --
//      including iccdev.apply-console-injection-regression, which greps for \x1B.
//      Checked for all 255 single-byte inputs.
//   2. The output is always printable ASCII.  That is the security invariant the helper
//      exists for, and it is what a blacklist could never guarantee.  Checked on every
//      case below, including the malformed ones.
//
// Red/green: built against master at 5dfafa80 the ten well-formed-UTF-8 cases fail, each
// reporting the \xHH run the helper used to emit, and the 255 single-byte, malformed and
// edge cases pass on BOTH builds -- which is what makes property 1 a measurement rather
// than a claim.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccFileUtil.h"

#include <string>

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;

void fail(const char *what, const std::string &got, const char *want)
{
  ++g_fail;
  std::fprintf(stderr, "[console-sanitize-codepoint] FAIL: %s (got \"%s\", want \"%s\")\n",
               what, got.c_str(), want);
}

// Every case also has to satisfy property 2, so it is enforced here rather than in a
// separate pass that a new case could be added without.
void checkEscape(const char *what, const char *in, const char *want)
{
  std::string got = icSanitizeConsoleText(in);

  if (got != want) {
    fail(what, got, want);
    return;
  }

  for (std::string::size_type i = 0; i < got.size(); i++) {
    unsigned char ch = (unsigned char)got[i];
    if (ch < 0x20 || ch >= 0x7f) {
      ++g_fail;
      std::fprintf(stderr,
                   "[console-sanitize-codepoint] FAIL: %s left byte 0x%02X at offset %u "
                   "in the sanitized output\n",
                   what, ch, (unsigned)i);
      return;
    }
  }
}

// Property 1: ASCII input is passed through exactly as the byte-wise helper did.  Stated
// as an independent expectation rather than by calling the helper twice, so a change to
// the ASCII branch cannot satisfy this by agreeing with itself.
void testAsciiUnchanged()
{
  for (int b = 0x01; b <= 0xff; b++) {
    char in[2];
    char want[8];

    in[0] = (char)b;
    in[1] = '\0';

    if (b == '\n')      std::snprintf(want, sizeof(want), "\\n");
    else if (b == '\r') std::snprintf(want, sizeof(want), "\\r");
    else if (b == '\t') std::snprintf(want, sizeof(want), "\\t");
    else if (b >= 0x20 && b < 0x7f) { want[0] = (char)b; want[1] = '\0'; }
    else                std::snprintf(want, sizeof(want), "\\x%02X", b);

    // A lone byte >= 0x80 is never a well-formed UTF-8 sequence, so the whole range
    // stays on the per-byte form here -- that is the malformed case, not a counterexample
    // to the codepoint spelling, which needs a complete sequence.
    std::string got = icSanitizeConsoleText(in);
    if (got != want) {
      char what[64];
      std::snprintf(what, sizeof(what), "single byte 0x%02X", b);
      fail(what, got, want);
    }
  }
}

// The issue's own case, plus the widths either side of it.
void testCodepointSpelling()
{
  checkEscape("U+00E9 in a filename",  "\xC3\xA9" "cran-Profil.icc", "\\u00E9cran-Profil.icc");
  checkEscape("U+00A0 no-break space", "\xC2\xA0",                   "\\u00A0");
  checkEscape("U+8272 three-byte",     "\xE8\x89\xB2",               "\\u8272");
  checkEscape("U+1F600 four-byte",     "\xF0\x9F\x98\x80",           "\\U0001F600");
  checkEscape("U+10FFFF upper bound",  "\xF4\x8F\xBF\xBF",           "\\U0010FFFF");
  checkEscape("mixed run",             "a\xC3\xA9" "b\xE8\x89\xB2" "c",
                                       "a\\u00E9b\\u8272c");
}

// The reason passthrough was rejected.  Each of these is well-formed UTF-8 and must
// still come out escaped.
void testTerminalActiveCodepointsStillEscaped()
{
  checkEscape("U+009B CSI as UTF-8",   "x\xC2\x9B" "[31my",          "x\\u009B[31my");
  checkEscape("U+202E RLO",            "gnp.\xE2\x80\xAE" "cci",     "gnp.\\u202Ecci");
  checkEscape("U+FEFF BOM",            "\xEF\xBB\xBF" "a",           "\\uFEFFa");
  checkEscape("U+200B zero width",     "a\xE2\x80\x8B" "b",          "a\\u200Bb");
  // The ASCII ESC the #2406 fix targeted is unchanged by all of this.
  checkEscape("ASCII ESC unchanged",   "a\x1b" "[31mb",              "a\\x1B[31mb");
}

// RFC 3629 rejects these, so there is no codepoint to name and the per-byte form stands.
// Each also proves the invalid lead advanced by exactly one byte.
void testMalformedFallsBackPerByte()
{
  checkEscape("lone continuation",     "a\x80" "b",                  "a\\x80b");
  checkEscape("overlong two-byte",     "\xC0\xAF",                   "\\xC0\\xAF");
  checkEscape("overlong three-byte",   "\xE0\x80\xAF",               "\\xE0\\x80\\xAF");
  checkEscape("overlong four-byte",    "\xF0\x80\x80\xAF",           "\\xF0\\x80\\x80\\xAF");
  checkEscape("UTF-8 surrogate D800",  "\xED\xA0\x80",               "\\xED\\xA0\\x80");
  checkEscape("above U+10FFFF",        "\xF4\x90\x80\x80",           "\\xF4\\x90\\x80\\x80");
  checkEscape("F5 lead",               "\xF5\x80\x80\x80",           "\\xF5\\x80\\x80\\x80");
  checkEscape("FF is never a lead",    "\xFF",                       "\\xFF");
  checkEscape("truncated two-byte",    "\xC3",                       "\\xC3");
  checkEscape("truncated three-byte",  "\xE8\x89",                   "\\xE8\\x89");
  // The lead is escaped alone and the ASCII after it survives: a greedy fallback would
  // have consumed the 'A' as though it were a continuation byte.
  checkEscape("bad lead then ASCII",   "\xC3" "A",                   "\\xC3A");
}

// The escape names the codepoint, so two encodings that a terminal renders identically
// stay distinguishable -- the concrete cost of rendering raw UTF-8.
void testDistinctFormsStayDistinct()
{
  std::string precomposed = icSanitizeConsoleText("\xC3\xA9");         // U+00E9
  std::string decomposed  = icSanitizeConsoleText("e\xCC\x81");        // e + U+0301

  if (precomposed == decomposed) {
    ++g_fail;
    std::fprintf(stderr,
                 "[console-sanitize-codepoint] FAIL: precomposed and decomposed forms "
                 "both rendered as \"%s\"\n", precomposed.c_str());
  }
}

void testEdges()
{
  checkEscape("empty string", "", "");

  if (!icSanitizeConsoleText((const char *)0).empty()) {
    ++g_fail;
    std::fprintf(stderr, "[console-sanitize-codepoint] FAIL: null input was not empty\n");
  }

  // The std::string overload must agree with the char* one.
  std::string s = "\xC3\xA9" "cran";
  if (icSanitizeConsoleText(s) != icSanitizeConsoleText(s.c_str())) {
    ++g_fail;
    std::fprintf(stderr,
                 "[console-sanitize-codepoint] FAIL: std::string overload disagrees\n");
  }
}

} // namespace

int main()
{
  testAsciiUnchanged();
  testCodepointSpelling();
  testTerminalActiveCodepointsStillEscaped();
  testMalformedFallsBackPerByte();
  testDistinctFormsStayDistinct();
  testEdges();

  if (g_fail)
    std::fprintf(stderr, "[console-sanitize-codepoint] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[console-sanitize-codepoint] all assertions passed\n");

  // Not `return g_fail`: the exit status is truncated to 8 bits, and
  // testAsciiUnchanged() alone can contribute 255 failures, so a count landing on
  // a multiple of 256 would exit 0 and report this test green.
  return g_fail ? 1 : 0;
}
