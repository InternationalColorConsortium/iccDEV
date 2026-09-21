// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       xml-float-significant-digits.cpp

    Contains:   CTest helper for the XML writer's float spelling (#2626).

    icXmlFloatFmt was "%.12f" and icSegPos used "%.8f" -- fixed DECIMAL PLACES,
    not significant digits.  A value smaller than 5e-13 was written as
    0.000000000000 and read back as zero, and a value needing more places than
    the format allowed was rounded to a different float32.  Two measured cases,
    both from real profiles in Testing/:

      * the Rec.2020 matrix coefficient 4.99407085e-17, erased outright
      * the HLG 1/12 breakpoint 0.0833333358, quantized to 0.08333334 and read
        back as 0.0833333433, one float32 ULP away

    Both formats now use nine significant digits, which is
    std::numeric_limits<float>::max_digits10 -- the count that guarantees a
    float32 survives a text round trip.

    The writer and parser changes are COUPLED and cannot be reviewed apart.
    icXmlParseFloat used to refuse a double that compared greater than FLT_MAX,
    but the canonical nine-digit spelling of FLT_MAX is 3.40282347e+38, which
    strtod resolves to a double fractionally ABOVE the exact float maximum -- so
    the old check refused the writer's own output.  It now converts first and
    tests the RESULT for finiteness, which accepts that spelling and still
    refuses a genuine overflow such as 1e39.  Cases 5 and 6 pin both halves; a
    relaxation that simply dropped the range test passes 5 and fails 6.

    Every case asserts the value is RECOVERED, not merely that the round trip
    returned true.  The unfixed writer emits a well-formed document and the
    unfixed reader parses it happily -- the value is just wrong -- so a case that
    only checked the return codes would pass against the defect it is meant to
    catch.

    Case 3 is the control in the other direction: an ordinary value must still
    be spelled plainly and must not acquire an exponent.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccMpeBasic.h"
#include "IccMpeXml.h"
#include "IccUtilXml.h"
#include "IccXmlConfig.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
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
    std::fprintf(stderr, "[xml-float-significant-digits] FAIL: %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// Write a formula segment carrying the given breakpoints, read the document
// back, and write it a second time.  The breakpoints travel as attributes
// written by icSegPos(), the "%.8f" site, so this exercises that format rather
// than icXmlFloatFmt.
//
// The segment list has no public accessor, so the caller asserts on the two
// documents instead.  Note what that comparison does and does not catch: it
// catches a reader that dropped or defaulted the attribute, but NOT the
// quantization this issue is about -- "%.8f" writes 0.08333334, reads back a
// different float, and then writes 0.08333334 again, so the two documents
// MATCH while the value has moved.  Verified by mutating the format back: this
// comparison stays green and only the spelling assertion in the caller fires.
// Both checks are needed; neither is redundant.
bool segmentReWrite(icFloatNumber start, icFloatNumber end,
                    std::string &firstXml, std::string &secondXml)
{
  CIccSegmentedCurveXml curve;
  CIccFormulaCurveSegment *pSeg = new CIccFormulaCurveSegment(start, end);
  icFloatNumber params[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
  pSeg->SetFunction(0, 4, params);
  curve.Insert(pSeg);

  if (!curve.ToXml(firstXml, ""))
    return false;

  const std::string wrapped = "<w>" + firstXml + "</w>";
  xmlDoc *pDoc = xmlReadMemory(wrapped.c_str(), (int)wrapped.size(), "frag.xml",
                               NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;

  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  CIccSegmentedCurveXml back;
  std::string parseStr;
  // As in the other XML helpers: a reader is handed the element's FIRST CHILD,
  // because icXmlFindNode walks the sibling chain and never descends.
  bool rv = pRoot && back.ParseXml(pRoot->children, parseStr);
  xmlFreeDoc(pDoc);
  if (!rv)
    return false;

  return back.ToXml(secondXml, "");
}

} // namespace

int main()
{
  // 1. The Rec.2020 coefficient the issue names, straight through the format.
  //    "%.12f" wrote twelve zeroes for it.
  {
    const icFloatNumber tiny = 4.99407085e-17f;
    char buf[64];
    std::snprintf(buf, sizeof(buf), icXmlFloatFmt, tiny);
    const std::string written(buf);

    check(!has(written, "0.000000000000"),
          ("a 4.99407085e-17 coefficient was erased, written as: " + written).c_str());

    icFloatNumber back = 0.0f;
    check(icXmlParseFloat(written.c_str(), back),
          ("the writer's own spelling was refused by the parser: " + written).c_str());
    check(back == tiny,
          ("4.99407085e-17 did not survive the round trip, written as: " + written).c_str());
  }

  // 2. The HLG 1/12 breakpoint, through icSegPos rather than icXmlFloatFmt.
  //    "%.8f" moved it one float32 ULP.
  {
    const icFloatNumber hlg = 0.0833333358f;
    std::string first, second;
    check(segmentReWrite(hlg, 1.0f, first, second),
          "a formula segment carrying the HLG breakpoint did not round trip");
    check(first == second,
          ("the HLG 1/12 breakpoint moved on a round trip.\n  wrote:   " + first +
           "  re-wrote: " + second).c_str());
    // And the spelling itself must carry the value, not a truncation of it.
    check(!has(first, "0.08333334\""),
          ("the breakpoint was quantized to eight places: " + first).c_str());
  }

  // 2b. The old "%.8f" also OVERRAN icSegPos's 20-byte buffer for any finite
  //     breakpoint of about 1e10 or more: -1.23456786e+38 needs 49 characters
  //     in fixed notation, so snprintf truncated it to "-123456786051166514",
  //     a number twenty orders of magnitude from the value, written into the
  //     document without a diagnostic.  GCC flags the same call with
  //     -Wformat-truncation.  Nine significant digits never exceeds 15
  //     characters, so the truncation is gone rather than merely unlikely.
  {
    const icFloatNumber big = -1.23456786e+38f;
    std::string first, second;
    check(segmentReWrite(big, 1.0f, first, second),
          "a formula segment with a large finite breakpoint did not round trip");
    check(!has(first, "-123456786051166514"),
          ("a large breakpoint was truncated into the document: " + first).c_str());
    check(first == second,
          ("a large breakpoint moved on a round trip.\n  wrote:   " + first +
           "  re-wrote: " + second).c_str());
  }

  // 3. Control: an ordinary value must still be spelled plainly, so the change
  //    is significant digits and not a blanket switch to exponent notation.
  {
    char buf[64];
    std::snprintf(buf, sizeof(buf), icXmlFloatFmt, (icFloatNumber)0.5f);
    check(!std::strchr(buf, 'e') && !std::strchr(buf, 'E'),
          ("0.5 acquired an exponent: " + std::string(buf)).c_str());
    icFloatNumber back = 0.0f;
    check(icXmlParseFloat(buf, back) && back == 0.5f, "0.5 did not round trip");
  }

  // 4. A spread of ordinary float32 values must all survive exactly.  Without
  //    this, a format with too FEW significant digits would pass cases 1 and 3.
  {
    const icFloatNumber vals[] = {
      1.0f, 0.1f, 0.2f, 1.0f / 3.0f, 0.0833333358f, 0.636953533f,
      1.06082726f, 2.22222233f, 0.0280731358f, 1e-30f, 1e30f, 0.0f, -0.0f,
    };
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), icXmlFloatFmt, vals[i]);
      icFloatNumber back = 0.0f;
      char label[160];
      std::snprintf(label, sizeof(label), "value %d did not survive, written as %s",
                    (int)i, buf);
      check(icXmlParseFloat(buf, back) && back == vals[i], label);
    }
  }

  // 4b. Most of the ~52 icXmlFloatFmt sites are ARRAY dumps read back through
  //     CIccXmlArrayType::ParseText, not through icXmlParseFloat -- and the
  //     Rec.2020 matrix coefficient this issue names travels that path.  Cases
  //     1 and 4 would pass while the array tokenizer mangled every exponent, so
  //     this drives DumpArray/ParseText directly.
  //
  //     The tokenizer is the reason to pin it: icIsNumChar (IccUtilXml.cpp)
  //     accepts 'e' but NOT 'E'.  "%.9g" only ever emits lower case, so this is
  //     correct today, but a future "%.9G" would split every exponent token in
  //     two and silently change the array's length.
  {
    icFloatNumber vals[] = {
      4.99407085e-17f, 1.00000002e+30f, -1.40129846e-45f,
      0.0833333358f, 1.0f, 0.0f, -0.0f, 0.636953533f,
    };
    const icUInt32Number n = (icUInt32Number)(sizeof(vals) / sizeof(vals[0]));

    std::string dumped;
    check(CIccFloatArray::DumpArray(dumped, "", vals, n, icConvertFloat, 8),
          "DumpArray refused a float row");
    check(!has(dumped, "0.000000000000"),
          ("an array value was erased: " + dumped).c_str());

    check(CIccFloatArray::ParseTextCount(dumped.c_str()) == n,
          ("the array tokenizer split or merged tokens: " + dumped).c_str());

    icFloatNumber back[8];
    for (icUInt32Number i = 0; i < n; i++) back[i] = 12345.0f;
    check(CIccFloatArray::ParseText(back, n, dumped.c_str()) == n,
          ("ParseText did not return every value: " + dumped).c_str());
    for (icUInt32Number i = 0; i < n; i++) {
      char label[192];
      std::snprintf(label, sizeof(label),
                    "array value %u did not survive the dump/parse round trip; row was:%s",
                    (unsigned)i, dumped.c_str());
      check(back[i] == vals[i], label);
    }
  }

  // 5. The coupling: the writer's own spelling of FLT_MAX must be accepted.
  //    strtod resolves it to a double just above the exact float maximum, which
  //    is precisely what the old range test refused.
  {
    const icFloatNumber fmax = std::numeric_limits<icFloatNumber>::max();
    char buf[64];
    std::snprintf(buf, sizeof(buf), icXmlFloatFmt, fmax);
    icFloatNumber back = 0.0f;
    check(icXmlParseFloat(buf, back),
          ("the canonical spelling of FLT_MAX was refused: " + std::string(buf)).c_str());
    check(back == fmax,
          ("FLT_MAX did not round trip, written as: " + std::string(buf)).c_str());

    const icFloatNumber fmin = -fmax;
    std::snprintf(buf, sizeof(buf), icXmlFloatFmt, fmin);
    check(icXmlParseFloat(buf, back) && back == fmin,
          ("-FLT_MAX did not round trip, written as: " + std::string(buf)).c_str());
  }

  // 6. ...and the guard must not have been weakened while relaxing it.  A
  //    relaxation that simply deleted the range test passes case 5 and fails
  //    here.
  {
    icFloatNumber back = 0.0f;
    check(!icXmlParseFloat("1e39", back), "1e39 overflows float32 and must be refused");
    // The bound is 2^128 - 2^103, the round-to-nearest overflow point, not
    // 2^128 and not FLT_MAX.  These two straddle it: the first rounds back to
    // FLT_MAX and must be accepted, the second rounds to infinity and must not.
    check(icXmlParseFloat("3.4028235e+38", back) && back == std::numeric_limits<icFloatNumber>::max(),
          "a double just below the overflow point must be accepted and round to FLT_MAX");
    check(!icXmlParseFloat("3.4028236e+38", back),
          "a double at or above the overflow point must be refused");
    check(!icXmlParseFloat("1e300", back),
          "1e300 must be refused, and refused WITHOUT an out-of-range cast");
    check(!icXmlParseFloat("-1e39", back), "-1e39 overflows float32 and must be refused");
    check(!icXmlParseFloat("1e400", back), "1e400 overflows double and must be refused");
    check(!icXmlParseFloat("inf", back), "an infinity must be refused");
    check(!icXmlParseFloat("nan", back), "a NaN must be refused");
    check(!icXmlParseFloat("", back), "an empty string must be refused");
    check(!icXmlParseFloat("0.5abc", back), "trailing garbage must be refused");
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
