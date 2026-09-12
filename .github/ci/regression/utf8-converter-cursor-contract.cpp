// Coverage for icUtf16ToUtf8() and icWCharToUtf8() (IccProfLib/IccUtil.cpp),
// added with the strict-aliasing fix in #2504.
//
// What this test does NOT do: it cannot observe the aliasing violation it was
// written alongside.  Both converters used to hand icConvertUTF16toUTF8() a
// (UTF8**) aimed at a char* object, so the callee stored a UTF8* through a
// char* lvalue and the caller read it back as char*.  That is undefined, but
// it was measured not to misbehave: gcc 13 and clang 18 at -O2 and at
// -O3 -flto all produce identical output before and after the fix.  A runtime
// test asserting "the bytes are right" therefore passes against the unfixed
// build too, and would be worth nothing on its own.
//
// What it does do is guard the rewrite.  Removing the pun meant re-typing the
// output cursor as UTF8* and recomputing the assigned length against a UTF8*
// base:
//
//     buf.assign(szBuf, (size_t)(szDest - szBuf));          // was
//     buf.assign(szBuf, (size_t)(szDest - (UTF8*)szBuf));   // now
//
// That arithmetic is the part a careless edit breaks, and icWCharToUtf8() had
// NO coverage at all when the fix was written -- deliberately corrupting its
// length by one byte left all 264 tests green, because no tracked XML fixture
// contains a DictEntry and there is no dict CTest, so the one path that reaches
// it (the dictType writers in IccTagXml.cpp and IccTagJson.cpp) is never
// exercised.  icUtf16ToUtf8() was covered only indirectly, by four tests that
// happen to route through it.
//
// The length assertions below are the point: each one pins the exact byte count
// the converter reports, not merely that the text looks right.  A truncated or
// over-long cursor changes size() while c_str() can still compare equal up to
// the NUL.
//
// #2511 added the lone-surrogate cases.  Lenient mode used to encode a UTF-16
// surrogate with no partner as three bytes (U+D83D -> ED A0 BD) -- CESU-8, not
// UTF-8 -- so iccToXml wrote XML that libxml2 refuses and iccFromXml could not
// read back.  It now substitutes U+FFFD.  Each of those cases asserts the exact
// bytes AND runs the output through isLegalUTF8String(), the library's own
// validator, so a fix that produced some other ill-formed sequence would still
// fail.  Every lone-surrogate case below fails against the unfixed converter;
// the strict-mode cases pass on both and guard the branch that #2511
// restructured.
//
// #2526 added the icWCharToUtf8 surrogate cases.  Where wchar_t is 32 bits,
// that function read its input as UTF-32, but the dictType text it converts
// holds UTF-16 code units, one per wchar_t -- so a valid U+1F600 came out as
// six bytes of CESU-8.  Every case that gives a surrogate its own wchar_t
// fails against that converter on Linux and macOS.  On Windows they pass
// before and after: that arm already went through icConvertUTF16toUTF8().
//
// Returns 0 on success; the number of failed assertions otherwise.

#include "IccUtil.h"
#include "IccConvertUTF.h"
#include "icProfileHeader.h"

#include <string>
#include <cstdio>
#include <cstring>
#include <cwchar>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    printf("FAIL: %s\n", what);
  }
}

// #2511: the converter's output must be well-formed UTF-8 by the library's own
// definition.  isLegalUTF8String() rejects any encoded surrogate, so this is
// the check that catches CESU-8 whatever the exact bytes are.
void checkLegal(const std::string &buf, const char *what)
{
  char msg[256];
  snprintf(msg, sizeof(msg), "%s: output is legal UTF-8", what);
  check(isLegalUTF8String((const UTF8*)buf.data(), (int)buf.size()) != 0, msg);
}

// Compares both the reported length and the bytes.  size() is checked first and
// separately because that is what the cursor arithmetic decides; a converter
// that writes correct bytes but reports the wrong length still fails here.
void checkUtf16(const icUInt16Number *src, int len,
                const char *expect, size_t expectLen, const char *what)
{
  std::string buf;
  const char *rv = icUtf16ToUtf8(buf, src, len);
  char msg[256];

  snprintf(msg, sizeof(msg), "%s: length (got %zu, want %zu)", what, buf.size(), expectLen);
  check(buf.size() == expectLen, msg);

  snprintf(msg, sizeof(msg), "%s: bytes", what);
  check(buf.size() == expectLen && memcmp(buf.data(), expect, expectLen) == 0, msg);

  // This does NOT pin the cursor: c_str()[size()] is '\0' by std::string's own
  // contract whatever length was assigned, so a short or long cursor still
  // satisfies it -- the length and byte checks above are what catch that.  What
  // it does guard is the return value's provenance: both converters build into
  // a malloc'd scratch buffer, free it, and return buf.c_str().  A future edit
  // that returned the scratch pointer instead would fail here.
  snprintf(msg, sizeof(msg), "%s: returns the string's own storage", what);
  check(rv == buf.c_str() && rv[buf.size()] == '\0', msg);

  // #2511: every case, not only the lone-surrogate ones -- the pre-existing
  // cases are all well-formed, and must stay that way.
  checkLegal(buf, what);
}

// Same pair of assertions for the std::vector overload of
// icConvertUTF16toUTF8().  icUtf16ToUtf8() above reaches the pointer overload
// only; the vector overload is the one CIccTagUtf16Text::GetText() and both
// CIccTagUtf8Text/CIccTagZipUtf8Text::SetText(const icUChar16*) call, and #2511
// changed both overloads.
void checkUtf16Vector(const icUInt16Number *src, size_t len,
                      const char *expect, size_t expectLen, const char *what)
{
  icUtf8Vector out;
  icUtfConversionResult rc = icConvertUTF16toUTF8((const UTF16*)src, (const UTF16*)src + len,
                                                  out, lenientConversion);
  // Copy as bytes, not element by element: std::string(out.begin(), out.end())
  // converts each UTF8 (unsigned char) to char implicitly.  Under clang,
  // ENABLE_SANITIZERS adds -fsanitize=integer, whose implicit sign-change check
  // stops on the first byte >= 0x80 -- measured, on the U+FFFD cases below.
  std::string buf;
  if (!out.empty())
    buf.assign(reinterpret_cast<const char*>(out.data()), out.size());
  char msg[256];

  snprintf(msg, sizeof(msg), "%s: result (got %d, want conversionOK)", what, (int)rc);
  check(rc == conversionOK, msg);

  snprintf(msg, sizeof(msg), "%s: length (got %zu, want %zu)", what, buf.size(), expectLen);
  check(buf.size() == expectLen, msg);

  snprintf(msg, sizeof(msg), "%s: bytes", what);
  check(buf.size() == expectLen && memcmp(buf.data(), expect, expectLen) == 0, msg);

  checkLegal(buf, what);
}

// Strict mode must still stop at a lone surrogate and report it, with the
// source cursor left ON the offending unit and nothing written for it.  #2511
// moved the lone-low test out from under "if (flags == strictConversion)" so
// that lenient mode could reach it; this pins that strict mode kept its old
// behaviour through the move.
void checkStrictStops(const icUInt16Number *src, size_t len, size_t stopAt,
                      size_t bytesBefore, const char *what)
{
  UTF8 out[32];
  const UTF16 *s = (const UTF16*)src;
  UTF8 *d = out;
  icUtfConversionResult rc = icConvertUTF16toUTF8(&s, (const UTF16*)src + len,
                                                  &d, out + sizeof(out), strictConversion);
  char msg[256];

  snprintf(msg, sizeof(msg), "%s: result (got %d, want sourceIllegal)", what, (int)rc);
  check(rc == sourceIllegal, msg);

  snprintf(msg, sizeof(msg), "%s: source cursor (got %td, want %zu)", what,
           s - (const UTF16*)src, stopAt);
  check(s == (const UTF16*)src + stopAt, msg);

  snprintf(msg, sizeof(msg), "%s: bytes written (got %td, want %zu)", what, d - out, bytesBefore);
  check(d == out + bytesBefore, msg);
}

void checkWChar(const wchar_t *src, size_t len,
                const char *expect, size_t expectLen, const char *what)
{
  std::string buf;
  const char *rv = icWCharToUtf8(buf, src, len);
  char msg[256];

  snprintf(msg, sizeof(msg), "%s: length (got %zu, want %zu)", what, buf.size(), expectLen);
  check(buf.size() == expectLen, msg);

  snprintf(msg, sizeof(msg), "%s: bytes", what);
  check(buf.size() == expectLen && memcmp(buf.data(), expect, expectLen) == 0, msg);

  // See checkUtf16: provenance, not cursor position.
  snprintf(msg, sizeof(msg), "%s: returns the string's own storage", what);
  check(rv == buf.c_str() && rv[buf.size()] == '\0', msg);

  // #2526: as in checkUtf16, every case.
  checkLegal(buf, what);
}

} // namespace

int main()
{
  // --- icUtf16ToUtf8 -------------------------------------------------------
  {
    // ASCII: one UTF-16 unit per byte out.  The simplest case that still moves
    // the cursor, so a base-pointer mistake shows up immediately.
    const icUInt16Number ascii[] = { 'i', 'c', 'c', 'D', 'E', 'V', 0 };
    checkUtf16(ascii, 6, "iccDEV", 6, "utf16 ascii, explicit length");

    // sizeSrc = 0 means "measure it" (CIccUTF16String::WStrlen), a separate
    // entry path into the same cursor code.
    checkUtf16(ascii, 0, "iccDEV", 6, "utf16 ascii, measured length");

    // U+00E9 -> 2 bytes, U+20AC -> 3 bytes.  Output length now differs from
    // input length, which is what makes the cursor arithmetic load-bearing.
    const icUInt16Number mixed[] = { 'a', 0x00E9, 0x20AC, 'z', 0 };
    checkUtf16(mixed, 4, "a\xC3\xA9\xE2\x82\xACz", 7, "utf16 2- and 3-byte sequences");

    // U+1F600 as a surrogate pair: 2 input units collapse to 4 output bytes.
    const icUInt16Number surrogate[] = { 0xD83D, 0xDE00, 0 };
    checkUtf16(surrogate, 2, "\xF0\x9F\x98\x80", 4, "utf16 surrogate pair");

    // A trailing unpaired high surrogate is DROPPED: the converter stops at the
    // incomplete pair and never emits it, so two input units yield one byte.
    // This is the case that actually pins the cursor -- the reported length
    // reflects where the converter stopped, not how much input it was given, so
    // a length derived from sizeSrc rather than from the cursor fails here.
    const icUInt16Number truncated[] = { 'a', 0xD83D, 0 };
    checkUtf16(truncated, 2, "a", 1, "utf16 trailing unpaired surrogate dropped");

    // #2511: an unpaired high surrogate NOT at the end becomes U+FFFD (EF BF BD).
    // This case used to pin the CESU-8 output (ED A0 BD) that #2510 found, so
    // the fix would be a visible change -- this is that change.  Same length
    // as before (3 bytes for the surrogate, 1 for 'a'), so only the bytes and
    // the legality check tell the two behaviours apart.
    const icUInt16Number loneHigh[] = { 0xD83D, 'a', 0 };
    checkUtf16(loneHigh, 2, "\xEF\xBF\xBD" "a", 4, "utf16 interior unpaired high surrogate -> U+FFFD");

    // #2511: a lone LOW surrogate leaked the same way (U+DE00 -> ED B8 80).
    // It takes a different arm of the converter from the high case above.
    const icUInt16Number loneLow[] = { 'a', 0xDE00, 'b', 0 };
    checkUtf16(loneLow, 3, "a" "\xEF\xBF\xBD" "b", 5, "utf16 lone low surrogate -> U+FFFD");

    // #2511: the substitution must not swallow the unit after a lone high
    // surrogate.  Here that unit is itself the start of a valid pair, so the
    // right answer is U+FFFD followed by an intact U+1F600.  A fix that
    // advanced past the unit it peeked at would lose the emoji.
    const icUInt16Number highThenPair[] = { 0xD83D, 0xD83D, 0xDE00, 0 };
    checkUtf16(highThenPair, 3, "\xEF\xBF\xBD" "\xF0\x9F\x98\x80", 7,
               "utf16 lone high surrogate then a valid pair");

    // #2511: a low surrogate before a high one is not a pair.  Both are lone,
    // and the trailing 'a' is what makes the high one interior.
    const icUInt16Number reversed[] = { 0xDE00, 0xD83D, 'a', 0 };
    checkUtf16(reversed, 3, "\xEF\xBF\xBD" "\xEF\xBF\xBD" "a", 7, "utf16 reversed pair -> two U+FFFD");

    // Interior NUL: the converter is length-driven, so it must not stop early.
    // buf.assign(ptr, len) preserves it; a c_str()-based length would not.
    const icUInt16Number embedded[] = { 'a', 0x0000, 'b', 0 };
    checkUtf16(embedded, 3, "a\0b", 3, "utf16 interior NUL kept");

    // Degenerate inputs take the early-return arms, which never touch the
    // cursor at all.
    {
      std::string buf("stale");
      icUtf16ToUtf8(buf, NULL, 4);
      check(buf.empty(), "utf16 NULL source clears the buffer");
    }
    {
      std::string buf("stale");
      const icUInt16Number empty[] = { 0 };
      icUtf16ToUtf8(buf, empty, 0);
      check(buf.empty(), "utf16 empty source clears the buffer");
    }
  }

  // --- icConvertUTF16toUTF8, std::vector overload (#2511) ------------------
  {
    const icUInt16Number loneHigh[] = { 0xD83D, 'a' };
    checkUtf16Vector(loneHigh, 2, "\xEF\xBF\xBD" "a", 4, "vector interior unpaired high surrogate -> U+FFFD");

    const icUInt16Number loneLow[] = { 'a', 0xDE00, 'b' };
    checkUtf16Vector(loneLow, 3, "a" "\xEF\xBF\xBD" "b", 5, "vector lone low surrogate -> U+FFFD");

    const icUInt16Number highThenPair[] = { 0xD83D, 0xD83D, 0xDE00 };
    checkUtf16Vector(highThenPair, 3, "\xEF\xBF\xBD" "\xF0\x9F\x98\x80", 7,
                     "vector lone high surrogate then a valid pair");

    const icUInt16Number reversed[] = { 0xDE00, 0xD83D, 'a' };
    checkUtf16Vector(reversed, 3, "\xEF\xBF\xBD" "\xEF\xBF\xBD" "a", 7, "vector reversed pair -> two U+FFFD");

    // A valid pair is untouched -- the control for the three cases above.
    const icUInt16Number pair[] = { 0xD83D, 0xDE00 };
    checkUtf16Vector(pair, 2, "\xF0\x9F\x98\x80", 4, "vector surrogate pair");
  }

  // --- icConvertUTF16toUTF8, strict mode (#2511 guard) ---------------------
  {
    // Stops on the lone high surrogate at index 1, after writing "a".
    const icUInt16Number loneHigh[] = { 'a', 0xD83D, 'b' };
    checkStrictStops(loneHigh, 3, 1, 1, "strict lone high surrogate");

    // Stops on the lone low surrogate at index 1, after writing "a".
    const icUInt16Number loneLow[] = { 'a', 0xDE00, 'b' };
    checkStrictStops(loneLow, 3, 1, 1, "strict lone low surrogate");
  }

  // --- icWCharToUtf8 -------------------------------------------------------
  // Reached in production only through the dictType writers, whose text holds
  // UTF-16 code units one per wchar_t on every platform.  Which arm of the
  // WCHAR_MAX branch runs depends on the platform, but since #2526 both end in
  // icConvertUTF16toUTF8(), so the cases in this block give the same bytes on
  // both.  The block after it runs only where wchar_t is 32 bits.
  {
    const wchar_t ascii[] = L"iccDEV";
    checkWChar(ascii, 6, "iccDEV", 6, "wchar ascii, explicit length");

    // sizeSrc = 0 means wcslen().
    checkWChar(ascii, 0, "iccDEV", 6, "wchar ascii, measured length");

    const wchar_t mixed[] = { L'a', 0x00E9, 0x20AC, L'z', 0 };
    checkWChar(mixed, 4, "a\xC3\xA9\xE2\x82\xACz", 7, "wchar 2- and 3-byte sequences");

    const wchar_t embedded[] = { L'a', 0, L'b', 0 };
    checkWChar(embedded, 3, "a\0b", 3, "wchar interior NUL kept");

    // #2526: U+1F600 as a surrogate pair, one unit per wchar_t -- the form a
    // dict entry holds.  Read as UTF-32 it came out as ED A0 BD ED B8 80.
    const wchar_t pairUnits[] = { L'a', 0xD83D, 0xDE00, L'b', 0 };
    checkWChar(pairUnits, 4, "a\xF0\x9F\x98\x80" "b", 6, "wchar surrogate pair in two units");

    // #2526 takes the #2511 lone-surrogate cases along with it.  The UTF-32
    // arm passed them through as CESU-8; they now match icUtf16ToUtf8 above.
    const wchar_t loneHigh[] = { 0xD83D, L'a', 0 };
    checkWChar(loneHigh, 2, "\xEF\xBF\xBD" "a", 4, "wchar interior unpaired high surrogate -> U+FFFD");

    const wchar_t loneLow[] = { L'a', 0xDE00, L'b', 0 };
    checkWChar(loneLow, 3, "a" "\xEF\xBF\xBD" "b", 5, "wchar lone low surrogate -> U+FFFD");

    const wchar_t reversed[] = { 0xDE00, 0xD83D, L'a', 0 };
    checkWChar(reversed, 3, "\xEF\xBF\xBD" "\xEF\xBF\xBD" "a", 7, "wchar reversed pair -> two U+FFFD");

    // A trailing high surrogate is dropped, as in icUtf16ToUtf8.
    const wchar_t truncated[] = { L'a', 0xD83D, 0 };
    checkWChar(truncated, 2, "a", 1, "wchar trailing unpaired surrogate dropped");

    {
      std::string buf("stale");
      icWCharToUtf8(buf, NULL, 4);
      check(buf.empty(), "wchar NULL source clears the buffer");
    }
    {
      std::string buf("stale");
      const wchar_t empty[] = L"";
      icWCharToUtf8(buf, empty, 0);
      check(buf.empty(), "wchar empty source clears the buffer");
    }
  }

#if WCHAR_MAX > 65535
  // --- icWCharToUtf8, 32-bit wchar_t only (#2526) ---------------------------
  // Real UTF-32, a form a 16-bit wchar_t cannot hold.  These passed before
  // #2526 as well; they guard the step it added, which splits a wchar_t above
  // U+FFFF into a surrogate pair before the UTF-16 converter joins it again.
  {
    const wchar_t oneUnit[] = { L'a', (wchar_t)0x1F600, L'b', 0 };
    checkWChar(oneUnit, 3, "a\xF0\x9F\x98\x80" "b", 6, "wchar U+1F600 as one UTF-32 unit");

    // U+10FFFF is the last code point; one past it has no encoding.
    const wchar_t last[] = { (wchar_t)0x10FFFF, 0 };
    checkWChar(last, 1, "\xF4\x8F\xBF\xBF", 4, "wchar U+10FFFF as one UTF-32 unit");

    const wchar_t beyond[] = { L'a', (wchar_t)0x110000, 0 };
    checkWChar(beyond, 2, "a\xEF\xBF\xBD", 4, "wchar past U+10FFFF -> U+FFFD");

    // Both forms in one string: a UTF-32 unit, then the same character as
    // a pair of UTF-16 units.
    const wchar_t bothForms[] = { (wchar_t)0x1F600, 0xD83D, 0xDE00, 0 };
    checkWChar(bothForms, 3, "\xF0\x9F\x98\x80" "\xF0\x9F\x98\x80", 8,
               "wchar U+1F600 as one unit, then as two");
  }
#endif

  if (g_fail)
    printf("%d assertion(s) failed\n", g_fail);
  else
    printf("utf8 converter cursor contract: all assertions passed\n");

  return g_fail;
}
