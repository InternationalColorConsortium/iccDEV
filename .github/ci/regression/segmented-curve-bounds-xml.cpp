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
    File:       segmented-curve-bounds-xml.cpp

    Contains:   CTest helper for the segment bounds CIccSegmentedCurveXml
                accepts (#2804).

    The binary encoding of a segmented curve holds one break-point between
    neighbouring segments and no outer bounds: ICC.2:2023 11.2.2.4 says the
    first segment "always starts at -infinity" and the last "always ends at
    +infinity".  XML gives every segment a Start and an End of its own, and
    the reader kept both without comparing them.  CIccSegmentedCurve::Write()
    keeps each later segment's Start and drops every End, while Apply() picks a
    segment by its End, so XML whose segments did not join, or whose outer
    bounds were finite, evaluated one way as parsed and another once written
    and read back.  Six tracked fixtures had that form.

    The reader now refuses such a curve.  Checked here: a gap, an overlap, a
    finite first Start and a finite last End are each refused and the message
    names the segment; joined segments are the control, in both spellings of
    the infinities and with "0.0" joining "0"; and a curve that is accepted
    evaluates the same as parsed and after a binary write and read, which is
    the property the refusal exists to keep.

    JSON is iccdev.segmented-curve-bounds-json, since IccMpeXml.h and
    IccMpeJson.h cannot share a translation unit.

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccMpeBasic.h"
#include "IccMpeXml.h"
#include "IccIO.h"

#include <libxml/parser.h>

#include <cmath>
#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const std::string &what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[segmented-curve-bounds-xml] FAIL %s: %s\n", label, what.c_str());
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

/* y = (a*x + b)^g + c, written "g a b c".  Three functions that cannot be
   mistaken for one another: 0, x and 1. */
const char *kZero = "1 0 0 0";
const char *kPass = "1 1 0 0";
const char *kOne  = "1 0 0 1";

std::string seg(const char *start, const char *end, const char *params)
{
  return std::string("<FormulaSegment Start=\"") + start + "\" End=\"" + end +
         "\" FunctionType=\"0\">" + params + "</FormulaSegment>";
}

bool parseCurve(const std::string &segments, CIccSegmentedCurveXml &curve, std::string &parseStr)
{
  std::string doc = "<SegmentedCurve>" + segments + "</SegmentedCurve>";
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "curve.xml", NULL, XML_PARSE_NONET);
  if (!pDoc)
    return false;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool ok = pRoot && curve.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return ok;
}

} // namespace

int main()
{
  /* 1. Curves the binary encoding can hold. */
  {
    static const struct { const char *label; std::string segments; } kAccepted[] = {
      { "three joined segments",
        seg("-infinity", "0", kZero) + seg("0", "1", kPass) + seg("1", "+infinity", kOne) },
      { "one segment",
        seg("-infinity", "+infinity", kPass) },
      /* The comparison is of the values, so a break-point spelled two ways
         still joins, and both infinity spellings are the outer bounds. */
      { "joined, -inf/+inf and 0.0/0 spellings",
        seg("-inf", "0.0", kZero) + seg("0", "+inf", kPass) },
    };
    for (size_t i = 0; i < sizeof(kAccepted) / sizeof(kAccepted[0]); i++) {
      std::string parseStr;
      CIccSegmentedCurveXml curve;
      check(parseCurve(kAccepted[i].segments, curve, parseStr), kAccepted[i].label,
            "refused: " + parseStr);
    }
  }

  /* 2. Curves it cannot hold.  Each row breaks one rule and keeps the others,
     so a reader that enforced only some of them lets a row through. */
  {
    static const struct { const char *label; std::string segments; const char *names; } kRefused[] = {
      /* The form four sRGB fixtures had: End and the next Start differ. */
      { "gap between segments 1 and 2",
        seg("-infinity", "0.04045", kZero) + seg("0.0405", "+infinity", kPass),
        "segment 2 has Start" },
      /* The form the HLG fixture had: the second segment runs past the third. */
      { "overlap between segments 2 and 3",
        seg("-infinity", "0", kZero) + seg("0", "+infinity", kPass) + seg("1", "+infinity", kOne),
        "segment 3 has Start" },
      { "finite first Start",
        seg("0", "0.5", kZero) + seg("0.5", "+infinity", kPass),
        "the first segment starts at -infinity" },
      { "finite last End",
        seg("-infinity", "0.5", kZero) + seg("0.5", "1", kPass),
        "the last segment ends at +infinity" },
      { "one segment with finite bounds",
        seg("-infinity", "1", kPass),
        "the last segment ends at +infinity" },
    };
    for (size_t i = 0; i < sizeof(kRefused) / sizeof(kRefused[0]); i++) {
      std::string parseStr;
      CIccSegmentedCurveXml curve;
      check(!parseCurve(kRefused[i].segments, curve, parseStr), kRefused[i].label,
            "parsed a curve the binary encoding cannot hold");
      check(has(parseStr, kRefused[i].names), kRefused[i].label,
            "the message does not name the segment, parseStr was: " + parseStr);
    }
  }

  /* 3. What the refusal keeps: an accepted curve gives the same values as
     parsed and after CIccSegmentedCurve::Write() and Read(). */
  {
    const char *label = "parsed and written curves agree";
    std::string parseStr;
    CIccSegmentedCurveXml parsed;
    check(parseCurve(seg("-infinity", "0", kZero) + seg("0", "1", kPass) + seg("1", "+infinity", kOne),
                     parsed, parseStr), label, "refused: " + parseStr);

    CIccMemIO io;
    check(io.Alloc(1024, true), label, "no memory for the written curve");
    check(parsed.Write(&io), label, "Write() refused the curve");
    icUInt32Number size = (icUInt32Number)io.Tell();
    io.Seek(0, icSeekSet);

    CIccSegmentedCurve written;
    check(written.Read(size, &io), label, "Read() refused the written curve");
    check(parsed.Begin(icElemInterpLinear, NULL) && written.Begin(icElemInterpLinear, NULL), label,
          "Begin() refused a curve");

    static const struct { icFloatNumber in; icFloatNumber out; } kPoints[] = {
      { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.25f, 0.25f }, { 1.0f, 1.0f }, { 5.0f, 1.0f },
    };
    for (size_t i = 0; i < sizeof(kPoints) / sizeof(kPoints[0]); i++) {
      icFloatNumber p = parsed.Apply(kPoints[i].in);
      icFloatNumber w = written.Apply(kPoints[i].in);
      if (std::fabs(p - kPoints[i].out) > 1e-6f || std::fabs(w - kPoints[i].out) > 1e-6f) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "at %g: parsed gives %g, written gives %g, expected %g",
                      (double)kPoints[i].in, (double)p, (double)w, (double)kPoints[i].out);
        check(false, label, buf);
      }
    }
  }

  if (g_fail) {
    std::fprintf(stderr, "[segmented-curve-bounds-xml] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[segmented-curve-bounds-xml] PASS\n");
  return 0;
}
