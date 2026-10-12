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
    File:       segmented-curve-bounds-json.cpp

    Contains:   CTest helper for the segment bounds CIccSegmentedCurveJson
                accepts (#2804).

    The binary encoding of a segmented curve holds one break-point between
    neighbouring segments and no outer bounds: ICC.2:2023 11.2.2.4 says the
    first segment "always starts at -infinity" and the last "always ends at
    +infinity".  JSON gives every segment a start and an end of its own, and
    the reader kept both without comparing them.  CIccSegmentedCurve::Write()
    keeps each later segment's start and drops every end, while Apply() picks a
    segment by its end, so JSON whose segments did not join, or whose outer
    bounds were finite, evaluated one way as parsed and another once written
    and read back.

    The reader now refuses such a curve.  Checked here: a gap, an overlap, a
    finite first start and a finite last end are each refused and the message
    names the segment; so is a segment with no start, or one that is not a
    number or a string, which read as 0 and so joined a previous end of 0;
    joined segments are the control, with
    the infinities as strings and a break-point given once as a string and
    once as a number; and a curve that is accepted evaluates the same as
    parsed and after a binary write and read, which is the property the
    refusal exists to keep.

    XML is iccdev.segmented-curve-bounds-xml, since IccMpeXml.h and
    IccMpeJson.h cannot share a translation unit.

    Exit codes:
      0 - expected result observed
      1 - unexpected result
*/

#include "IccMpeBasic.h"
#include "IccMpeJson.h"
#include "IccIO.h"

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
    std::fprintf(stderr, "[segmented-curve-bounds-json] FAIL %s: %s\n", label, what.c_str());
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

/* y = (a*x + b)^g + c, written [g, a, b, c].  Three functions that cannot be
   mistaken for one another: 0, x and 1. */
const char *kZero = "[1.0,0.0,0.0,0.0]";
const char *kPass = "[1.0,1.0,0.0,0.0]";
const char *kOne  = "[1.0,0.0,0.0,1.0]";

const char *kNegInf = "\"-infinity\"";
const char *kPosInf = "\"+infinity\"";

/* start and end are JSON values, so a caller passes 0.5 or "\"-infinity\"";
   an empty start leaves the member out. */
std::string seg(const char *start, const char *end, const char *params)
{
  std::string s = "{\"type\":\"FormulaSegment\",";
  if (*start)
    s += std::string("\"start\":") + start + ",";
  return s + "\"end\":" + end + ",\"functionType\":0,\"parameters\":" + params + "}";
}

std::string curveDoc(const std::string &segments)
{
  return "{\"segments\":[" + segments + "]}";
}

} // namespace

int main()
{
  /* 1. Curves the binary encoding can hold. */
  {
    static const struct { const char *label; std::string segments; } kAccepted[] = {
      { "three joined segments",
        seg(kNegInf, "0", kZero) + "," + seg("0", "1", kPass) + "," + seg("1", kPosInf, kOne) },
      { "one segment",
        seg(kNegInf, kPosInf, kPass) },
      /* The comparison is of the values, so a break-point given as a string
         joins the same one given as a number. */
      { "joined, break-point as string and as number",
        seg("\"-inf\"", "\"0.5\"", kZero) + "," + seg("0.5", "\"+inf\"", kPass) },
    };
    for (size_t i = 0; i < sizeof(kAccepted) / sizeof(kAccepted[0]); i++) {
      std::string parseStr;
      CIccSegmentedCurveJson curve;
      check(curve.ParseJson(IccJson::parse(curveDoc(kAccepted[i].segments)), parseStr),
            kAccepted[i].label, "refused: " + parseStr);
    }
  }

  /* 2. Curves it cannot hold.  Each row breaks one rule and keeps the others,
     so a reader that enforced only some of them lets a row through. */
  {
    static const struct { const char *label; std::string segments; const char *names; } kRefused[] = {
      { "gap between segments 1 and 2",
        seg(kNegInf, "0.04045", kZero) + "," + seg("0.0405", kPosInf, kPass),
        "segment 2 has start" },
      { "overlap between segments 2 and 3",
        seg(kNegInf, "0", kZero) + "," + seg("0", kPosInf, kPass) + "," + seg("1", kPosInf, kOne),
        "segment 3 has start" },
      { "finite first start",
        seg("0", "0.5", kZero) + "," + seg("0.5", kPosInf, kPass),
        "the first segment starts at -infinity" },
      { "finite last end",
        seg(kNegInf, "0.5", kZero) + "," + seg("0.5", "1", kPass),
        "the last segment ends at +infinity" },
      { "one segment with a finite end",
        seg(kNegInf, "1", kPass),
        "the last segment ends at +infinity" },
      /* A missing start read as 0.  On the first segment that was a finite
         first start; on a later one it joined a previous end of 0. */
      { "first segment without a start",
        seg("", kPosInf, kPass),
        "segment 1 needs a start and an end" },
      { "later segment without a start, after an end of 0",
        seg(kNegInf, "0", kZero) + "," + seg("", kPosInf, kPass),
        "segment 2 needs a start and an end" },
      { "start that is neither a number nor a string",
        seg(kNegInf, "0", kZero) + "," + seg("null", kPosInf, kPass),
        "segment 2 needs a start and an end" },
    };
    for (size_t i = 0; i < sizeof(kRefused) / sizeof(kRefused[0]); i++) {
      std::string parseStr;
      CIccSegmentedCurveJson curve;
      check(!curve.ParseJson(IccJson::parse(curveDoc(kRefused[i].segments)), parseStr),
            kRefused[i].label, "parsed a curve the binary encoding cannot hold");
      check(has(parseStr, kRefused[i].names), kRefused[i].label,
            "the message does not name the segment, parseStr was: " + parseStr);
    }
  }

  /* 3. What the refusal keeps: an accepted curve gives the same values as
     parsed and after CIccSegmentedCurve::Write() and Read(). */
  {
    const char *label = "parsed and written curves agree";
    std::string parseStr;
    CIccSegmentedCurveJson parsed;
    check(parsed.ParseJson(IccJson::parse(curveDoc(
            seg(kNegInf, "0", kZero) + "," + seg("0", "1", kPass) + "," + seg("1", kPosInf, kOne))),
          parseStr), label, "refused: " + parseStr);

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
    std::fprintf(stderr, "[segmented-curve-bounds-json] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[segmented-curve-bounds-json] PASS\n");
  return 0;
}
