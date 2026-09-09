/*
    File:       islegalutf8-second-byte-bound.cpp

    Contains:   CTest for the second-byte lower bound isLegalUTF8() omitted for
                the 0xED and 0xF4 leads (#2435).

    isLegalUTF8() validates the second byte in an inner switch.  The outer
    `case 2:` applies only an UPPER bound (a > 0xBF); the generic lower bound
    lives in the switch's `default:` arm, which every named arm bypasses.  The
    0xE0 and 0xF0 arms tighten the lower bound to 0xA0 and 0x90, so they imply
    a >= 0x80 and were correct by accident.  The 0xED and 0xF4 arms tightened
    only the upper bound and then broke -- leaving those two leads with no lower
    bound at all, so a second byte of 0x00..0x7F, which is never a continuation
    byte, was accepted.

    Bisected by xsscx to 1f0a9dd (2015-09-29), the commit that introduced the
    function.  This has been wrong for its entire life in the tree.

    WHY IT MATTERS: this is the validation gate, not a decoder detail.
    CIccUTF16String::FromUtf8() calls icConvertUTF8toUTF16(..., strictConversion)
    and treats anything other than conversionOK as "reject" (IccUtil.cpp:3086),
    and isLegalUTF8String() is what IccTagXml.cpp:209 and IccTagJson.cpp:143 use
    to decide whether a tag's text may be written out.  So malformed input was
    not merely tolerated, it was silently re-encoded as a DIFFERENT character:

        ED 01 80     -> U+B040
        F4 01 80 80  -> U+81000, which is above U+10FFFF and not a Unicode
                        scalar value; the UTF-16 output is a surrogate pair, so
                        the OUTPUT is not well-formed UTF-16 either.

    THE CONTROLS ARE THE POINT.  Four of the eleven cases below are leads whose
    arms DO carry a lower bound -- E0, EC, F0 -- plus the two boundary sequences
    the named arms exist to reject (ED A0 80, the U+D800 surrogate, and
    F4 90 80 80, above U+10FFFF).  Those pass on BOTH builds.  Without them a
    "fix" that simply rejected every ED- and F4-led sequence, or that deleted the
    inner switch outright, would turn every defect case green while breaking
    U+D7FF and U+10FFFF -- the two codepoints the tightened bounds are FOR.

    The exhaustive arm is what actually pins the behaviour.  For each of the 256
    lead bytes it walks every combination of the trailing bytes that lead's RFC
    3629 length implies -- 84,994,816 sequences -- and compares against a
    reference decoder written from the RFC.  Every probe hands over the whole
    buffer as the window, so icIsLegalUTF8Sequence() derives its own length and
    reaches the validator even for the 77 leads that have no RFC length; sizing
    the window to the RFC length instead left `*source > 0xF4` and the 0xC0/0xC1
    overlong guard with no coverage at all.  Measured on 764fc176: 532,480
    sequences accepted that RFC 3629 rejects (128*64 for ED, 128*64*64 for F4)
    and NONE rejected that it accepts.  With the fix, zero in both directions.

    Note the count.  #2435's issue body says 528,320, which came from a harness
    that builds NUL-terminated strings and so could not present 0x00 as a second
    byte; it missed 64 ED and 4096 F4 sequences.  icIsLegalUTF8Sequence() takes a
    pointer and a length and a profile's text tag is a byte range, so the NUL
    cases are reachable and 532,480 is the right figure.

    Both directions are asserted.  "Rejected but legal" must stay zero: a fix
    that over-tightens is as wrong as the bug, and it is the failure this file
    would otherwise miss.
*/

#include "IccConvertUTF.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>

static int g_failures = 0;

/* Reference: does a well-formed UTF-8 sequence begin at b?  Length from the
 * lead byte, bounds from RFC 3629 Table 3-7.  icIsLegalUTF8Sequence() answers
 * the same question -- it derives its own length from the lead -- so the two
 * are directly comparable over a window large enough for any legal sequence. */
static int rfcLen(unsigned char c0)
{
  if (c0 < 0x80)                  return 1;
  if (c0 >= 0xC2 && c0 <= 0xDF)   return 2;
  if (c0 >= 0xE0 && c0 <= 0xEF)   return 3;
  if (c0 >= 0xF0 && c0 <= 0xF4)   return 4;
  return 0;                       /* 0x80..0xC1 and 0xF5..0xFF are never leads */
}

static bool rfcOk(const unsigned char *b)
{
  int n = rfcLen(b[0]);
  if (!n)
    return false;

  unsigned char lo = 0x80, hi = 0xBF;
  if (b[0] == 0xE0) lo = 0xA0;
  if (b[0] == 0xED) hi = 0x9F;
  if (b[0] == 0xF0) lo = 0x90;
  if (b[0] == 0xF4) hi = 0x8F;

  for (int i = 1; i < n; i++) {
    unsigned char a = (i == 1) ? lo : 0x80;
    unsigned char z = (i == 1) ? hi : 0xBF;
    if (b[i] < a || b[i] > z)
      return false;
  }
  return true;
}

static void expectSequence(const char *label, const unsigned char *bytes, int n,
                           bool bExpectLegal)
{
  unsigned char buf[8];
  memset(buf, 0x80, sizeof(buf));
  memcpy(buf, bytes, (size_t)n);

  bool got = icIsLegalUTF8Sequence(buf, buf + n) != 0;
  if (got != bExpectLegal) {
    printf("FAIL [%s]: icIsLegalUTF8Sequence returned %s, expected %s\n",
           label, got ? "legal" : "illegal", bExpectLegal ? "legal" : "illegal");
    g_failures++;
  }
  else {
    printf("  ok [%s]: %s\n", label, bExpectLegal ? "legal" : "illegal");
  }
}

/* The gate a profile's text actually passes through. */
static void expectFromUtf8(const char *label, const char *bytes, size_t n,
                           bool bExpectAccept)
{
  CIccUTF16String str;
  bool got = str.FromUtf8(bytes, n);

  if (got != bExpectAccept) {
    printf("FAIL [%s]: FromUtf8 %s, expected %s\n",
           label, got ? "accepted" : "rejected",
           bExpectAccept ? "accept" : "reject");
    g_failures++;
  }
  else {
    printf("  ok [%s]: FromUtf8 %s\n", label, got ? "accepted" : "rejected");
  }
}

int main()
{
  /* ---- the defect: no lower bound on the second byte after ED or F4 ---- */
  {
    static const unsigned char ed00[] = { 0xED, 0x00, 0x80 };
    static const unsigned char ed01[] = { 0xED, 0x01, 0x80 };
    static const unsigned char ed7f[] = { 0xED, 0x7F, 0x80 };
    static const unsigned char f400[] = { 0xF4, 0x00, 0x80, 0x80 };
    static const unsigned char f401[] = { 0xF4, 0x01, 0x80, 0x80 };
    static const unsigned char f47f[] = { 0xF4, 0x7F, 0x80, 0x80 };

    expectSequence("ED 00 80",    ed00, 3, false);
    expectSequence("ED 01 80",    ed01, 3, false);
    expectSequence("ED 7F 80",    ed7f, 3, false);
    expectSequence("F4 00 80 80", f400, 4, false);
    expectSequence("F4 01 80 80", f401, 4, false);
    expectSequence("F4 7F 80 80", f47f, 4, false);
  }

  /* ---- CONTROLS: pass on BOTH builds ----
   * The three sibling leads whose arms already carried a lower bound, and the
   * two boundary sequences the ED and F4 arms exist to reject.  A fix that
   * threw away the inner switch would break the two boundaries; a fix that
   * blanket-rejected ED and F4 would break the two valid cases below. */
  {
    static const unsigned char e0[]   = { 0xE0, 0x01, 0x80 };
    static const unsigned char ec[]   = { 0xEC, 0x01, 0x80 };
    static const unsigned char f0[]   = { 0xF0, 0x01, 0x80, 0x80 };
    static const unsigned char edsur[]= { 0xED, 0xA0, 0x80 };        /* U+D800 */
    static const unsigned char f4big[]= { 0xF4, 0x90, 0x80, 0x80 };  /* >10FFFF */

    expectSequence("ctrl E0 01 80",     e0,    3, false);
    expectSequence("ctrl EC 01 80",     ec,    3, false);
    expectSequence("ctrl F0 01 80 80",  f0,    4, false);
    expectSequence("ctrl ED A0 80",     edsur, 3, false);
    expectSequence("ctrl F4 90 80 80",  f4big, 4, false);
  }

  /* ---- CONTROLS: the codepoints the tightened bounds are FOR ---- */
  {
    static const unsigned char d7ff[]  = { 0xED, 0x9F, 0xBF };        /* U+D7FF */
    static const unsigned char maxcp[] = { 0xF4, 0x8F, 0xBF, 0xBF };  /* U+10FFFF */
    static const unsigned char eacute[]= { 0xC3, 0xA9 };              /* U+00E9 */

    expectSequence("ctrl U+D7FF",   d7ff,   3, true);
    expectSequence("ctrl U+10FFFF", maxcp,  4, true);
    expectSequence("ctrl U+00E9",   eacute, 2, true);
  }

  /* ---- the gate a profile's text passes through ---- */
  expectFromUtf8("FromUtf8 ED 01 80",    "\xED\x01\x80",     3, false);
  expectFromUtf8("FromUtf8 F4 01 80 80", "\xF4\x01\x80\x80", 4, false);
  expectFromUtf8("FromUtf8 U+D7FF",      "\xED\x9F\xBF",     3, true);
  expectFromUtf8("FromUtf8 U+10FFFF",    "\xF4\x8F\xBF\xBF", 4, true);
  expectFromUtf8("FromUtf8 U+00E9",      "\xC3\xA9",         2, true);

  /* ---- isLegalUTF8String(): the gate the XML and JSON writers actually call
   * (IccTagXml.cpp:209, IccTagJson.cpp:143).  icIsLegalUTF8Sequence() is
   * documented in IccConvertUTF.cpp as "not used here; it's just exported", so
   * without these the motivation above rests on a function no writer calls.
   * It walks a whole string and does its own bounds arithmetic, so it is a
   * separate path, not a wrapper.  The leading 'A' makes the malformed sequence
   * start mid-string rather than at offset 0. ---- */
  {
    struct { const char *label; const char *text; int len; bool legal; } cases[] = {
      { "string A+ED 01 80",       "A\xED\x01\x80",         4, false },
      { "string A+ED 9F BF",       "A\xED\x9F\xBF",         4, true  },
      { "string A+F4 01 80 80",    "A\xF4\x01\x80\x80",     5, false },
      { "string A+F4 8F BF BF",    "A\xF4\x8F\xBF\xBF",     5, true  },
      { "string A+F5 80 80 80",    "A\xF5\x80\x80\x80",     5, false },
      { "string plain ASCII",      "ABC",                   3, true  },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      bool got = isLegalUTF8String((const UTF8 *)cases[i].text, cases[i].len) != 0;
      if (got != cases[i].legal) {
        printf("FAIL [%s]: isLegalUTF8String returned %s, expected %s\n",
               cases[i].label, got ? "legal" : "illegal",
               cases[i].legal ? "legal" : "illegal");
        g_failures++;
      }
      else {
        printf("  ok [%s]: %s\n", cases[i].label, got ? "legal" : "illegal");
      }
    }
  }

  /* ---- exhaustive: every sequence the function can be asked about ---- */
  {
    long scanned = 0, acceptedIllegal = 0, rejectedLegal = 0;
    unsigned char b[8];
    unsigned char firstBad[4] = { 0, 0, 0, 0 };
    bool haveBad = false;

    memset(b, 0x80, sizeof(b));

    for (int lead = 0; lead < 256; lead++) {
      b[0] = (unsigned char)lead;
      int n = rfcLen(b[0]);
      /* The whole buffer is the window, every time.  An earlier version sized
       * the window to the RFC length and passed 1 byte for a non-RFC lead --
       * 0xC0, 0xC1 and 0xF5..0xFF, 77 of the 256.  icIsLegalUTF8Sequence()
       * derives its OWN length from trailingBytesForUTF8 (4 for 0xF5, up to 6
       * for 0xF8..0xFD), so a 1-byte window tripped its
       * `source + length > sourceEnd` guard and isLegalUTF8() was never entered
       * for those leads at all.  The answer came out the same, so the arm stayed
       * green -- but `if (*source > 0xF4) return false;` and the 0xC0/0xC1
       * overlong guard in `case 1:` had NO coverage: deleting the former left
       * this test fully green while F5 80 80 80 became legal, which is exactly
       * the call shape isLegalUTF8String() makes on real profile text.  A window
       * of sizeof(b) is large enough for every length the table can produce, so
       * the function always reaches the validator. */
      int trailing = (n >= 2) ? (n - 1) : 1;
      long combos = 1;
      for (int i = 0; i < trailing; i++)
        combos *= 256;

      for (long c = 0; c < combos; c++) {
        long v = c;
        /* Anything past the enumerated bytes stays a valid continuation, so a
         * lead that is rejected is rejected for the LEAD, not for its tail. */
        memset(b + 1, 0x80, sizeof(b) - 1);
        for (int i = trailing; i >= 1; i--) {
          b[i] = (unsigned char)(v & 0xFF);
          v >>= 8;
        }

        scanned++;
        bool got  = icIsLegalUTF8Sequence(b, b + sizeof(b)) != 0;
        bool want = rfcOk(b);

        if (got && !want) {
          acceptedIllegal++;
          if (!haveBad) {
            memcpy(firstBad, b, 4);
            haveBad = true;
          }
        }
        if (!got && want)
          rejectedLegal++;
      }
    }

    printf("  exhaustive: scanned %ld sequences\n", scanned);

    if (acceptedIllegal != 0) {
      printf("FAIL [exhaustive-accepts]: %ld sequences accepted that RFC 3629 "
             "rejects; first is %02X %02X %02X %02X\n",
             acceptedIllegal, firstBad[0], firstBad[1], firstBad[2], firstBad[3]);
      g_failures++;
    }
    else {
      printf("  ok [exhaustive-accepts]: nothing malformed is accepted\n");
    }

    /* The over-tightening direction.  A fix that rejects too much passes every
     * case above and fails only here. */
    if (rejectedLegal != 0) {
      printf("FAIL [exhaustive-rejects]: %ld well-formed sequences rejected\n",
             rejectedLegal);
      g_failures++;
    }
    else {
      printf("  ok [exhaustive-rejects]: nothing well-formed is rejected\n");
    }
  }

  if (g_failures) {
    printf("islegalutf8-second-byte-bound: %d failure(s)\n", g_failures);
    return 1;
  }

  printf("islegalutf8-second-byte-bound: all checks passed\n");
  return 0;
}
