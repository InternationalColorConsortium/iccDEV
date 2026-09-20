// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       tonemap-functions-json.cpp

    Contains:   CTest helper for the JSON reader of a toneMapElement's
                toneMapFunctions.  #2612 and #2609.

    A missing functionType read as type 0, the one supported type, so a
    function naming none at all loaded as a valid one; the XML twin refuses it,
    as does the formula segment in the same file since #2547.  Found by review
    of this change, same defect class as #2612.

    CIccJsonToneMapFunc::ParseJson() fixed the parameter count from the
    function type, zeroed the parameters, and then treated the array as
    optional, copying min(expected, supplied) values.  A function with no
    parameters at all, or with fewer than its type takes, was accepted and
    saved, and reading the profile back showed the zeroes the parser had
    invented.  The XML twin has always required the full count.  #2612.

    CIccMpeJsonToneMap::ParseJson() ended with "return nIndex == nOut", so an
    array with fewer entries than the element has output channels - an empty
    one included - returned false with nothing added to parseStr, and the CLI
    printed only its own "Unable to Parse" line.  #2609.

    Checked here: a missing, empty, short and non-numeric parameter array are
    each refused and named; a function with the full count is the control, and
    the value it carries is read back rather than a zero; too few and too many
    functions are refused and named; and a duplicate entry still shares the
    function it names.

    XML is iccdev.tonemap-functions-xml, since IccMpeXml.h and IccMpeJson.h
    cannot share a translation unit.
*/

#include "IccMpeBasic.h"
#include "IccMpeJson.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[tonemap-functions-json] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

/* A luminance curve that passes its input through, so Apply() shows the tone
   function's own third parameter on each channel. */
const char *kLumCurve =
  "{\"type\": \"SegmentedCurve\", \"segments\": [{\"type\": \"FormulaSegment\","
  " \"start\": \"-infinity\", \"end\": \"+infinity\", \"functionType\": 0,"
  " \"parameters\": [1.0, 1.0, 0.0, 0.0]}]}";

std::string elemDoc(int nOut, const char *funcs)
{
  char channels[96];
  std::snprintf(channels, sizeof(channels),
                "{\"type\": \"ToneMapElement\", \"inputChannels\": %d, \"outputChannels\": %d, ",
                nOut + 1, nOut);
  return std::string(channels) + "\"luminanceCurve\": " + kLumCurve +
         ", \"toneMapFunctions\": " + funcs + "}";
}

bool parse(const std::string &doc, CIccMpeJsonToneMap &elem, std::string &parseStr)
{
  return elem.ParseJson(IccJson::parse(doc), parseStr);
}

const char *kFunc0 = "{\"functionType\": 0, \"parameters\": [1.0, 0.0, 0.0]}";
const char *kFunc1 = "{\"functionType\": 0, \"parameters\": [1.0, 0.0, 1.0]}";
const char *kFunc2 = "{\"functionType\": 0, \"parameters\": [1.0, 0.0, 2.0]}";

void checkChannels(CIccMpeJsonToneMap &elem, const std::vector<icFloatNumber> &expected, const char *label)
{
  if (!elem.Begin(icElemInterpLinear, NULL)) {
    check(false, label, "Begin() refused the element");
    return;
  }
  const int n = (int)expected.size();
  std::vector<icFloatNumber> src(n + 1, 0.0f), dst(n, -1.0f);
  src[n] = 1.0f;
  elem.Apply(NULL, dst.data(), src.data());
  for (int i = 0; i < n; i++) {
    if (std::fabs(dst[i] - expected[i]) > 1e-4f) {
      char buf[96];
      std::snprintf(buf, sizeof(buf), "channel %d gives %g, expected %g", i, (double)dst[i], (double)expected[i]);
      check(false, label, buf);
    }
  }
}

/* Each entry: the toneMapFunctions array, and the text the refusal must
   carry. */
struct Refusal {
  int nOut;
  const char *funcs;
  const char *report;
  const char *label;
};

const Refusal kRefusals[] = {
  { 1, "[{\"parameters\": [1.0, 0.0, 0.0]}]",                  "integer functionType",    "no functionType" },
  { 1, "[{\"functionType\": 1.5, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "integer functionType",    "fractional functionType" },
  { 1, "[{\"functionType\": 9, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "Unsupported functionType", "unsupported functionType" },
  { 1, "[{\"functionType\": 0}]",                              "Missing parameters",      "no parameters" },
  { 1, "[{\"functionType\": 0, \"parameters\": []}]",          "Too few parameters",      "empty parameters" },
  { 1, "[{\"functionType\": 0, \"parameters\": [1.0, 0.0]}]",  "Too few parameters",      "short parameters" },
  { 1, "[{\"functionType\": 0, \"parameters\": [1.0, 0.0, \"x\"]}]",
                                                               "non-numeric",             "non-numeric parameter" },
  { 1, "[]",                                                   "Too few toneMapFunctions", "empty array" },
  { 2, "[{\"functionType\": 0, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "Too few toneMapFunctions", "one function for two channels" },
};

} // namespace

int main()
{
  for (size_t i = 0; i < sizeof(kRefusals) / sizeof(kRefusals[0]); i++) {
    const Refusal &r = kRefusals[i];
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    check(!parse(elemDoc(r.nOut, r.funcs), elem, parseStr), r.label, "parsed");
    check(has(parseStr, r.report), r.label, ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* More entries than output channels is still refused, as it always was. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = std::string("[") + kFunc0 + ", " + kFunc1 + "]";
    check(!parse(elemDoc(1, funcs.c_str()), elem, parseStr), "too many", "parsed two functions for one channel");
    check(has(parseStr, "Too many"), "too many", ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* The control: a full parameter list is read, and the value that comes back
     is the one in the document rather than the zero the parser used to invent. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = std::string("[") + kFunc1 + "]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "control", ("refused: " + parseStr).c_str());
    checkChannels(elem, std::vector<icFloatNumber>(1, 1.0f), "control");
  }

  /* A duplicate still shares the function it names. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = std::string("[") + kFunc1 +
                        ", {\"type\": \"DuplicateFunction\", \"index\": 0}, " + kFunc2 + "]";
    check(parse(elemDoc(3, funcs.c_str()), elem, parseStr), "F D F", ("refused: " + parseStr).c_str());
    icFloatNumber e[3] = { 1.0f, 1.0f, 2.0f };
    checkChannels(elem, std::vector<icFloatNumber>(e, e + 3), "F D F");
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
