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

    #2633 adds two more IccMpeJson.cpp defects, unrelated to each other beyond
    the file.  They live here rather than in a file of their own because they
    need the same IccJSON link and the same reader:

      * CIccJsonToneMapFunc::NewCopy() copy constructed, and the base copy
        constructor was = default over an owning icFloatNumber *m_params, so
        the copy and the original both free()d the same buffer.  Fixed in the
        base, which is why the control below copies through the base type too.

      * icJsonSegPosFromStr() tested for a trailing "inf" BEFORE it looked at
        the sign, so "-inf" came back as the positive maximum; it also wanted
        at least four characters, which "inf" has not, and matched only the
        last three, which "+infinity" -- what icJsonSetSegPos() writes -- does
        not end with.  Anything unmatched reached atof(), which returns a real
        infinity that equals neither sentinel, so the writer emitted null.
        These cases drive CIccSegmentedCurveJson, not a tone map.
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
  /* m_nFunctionType is a uInt16Number and the switch runs on it, so a value
     past 0xFFFF used to wrap onto type 0, the one supported type, and load.
     The XML twin parses the attribute with icXmlParseU16, which refuses it
     (#1954). */
  { 1, "[{\"functionType\": 65536, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "functionType is out of range", "functionType past uInt16" },
  { 1, "[{\"functionType\": -1, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "functionType is out of range", "negative functionType" },
  /* reserved2 is stored in an icUInt16Number and took the same wrap. */
  { 1, "[{\"functionType\": 0, \"reserved2\": 65536, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "reserved2 is out of range", "reserved2 past uInt16" },
  { 1, "[{\"functionType\": 0, \"reserved2\": 1.5, \"parameters\": [1.0, 0.0, 0.0]}]",
                                                               "reserved2 is out of range", "fractional reserved2" },
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

  /* A reserved2 that fits is still read. */
  {
    std::string parseStr;
    std::string funcs = "[{\"functionType\": 0, \"reserved2\": 3, \"parameters\": [1.0, 0.0, 1.0]}]";
    CIccMpeJsonToneMap elem;
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "reserved2 control",
          ("a legal reserved2 was refused: " + parseStr).c_str());
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

  /* #2619: a finite JSON number beyond the float32 range casts to infinity.
     Validate() only counts parameters, so the profile saved clean and
     iccToJson wrote the parameter back as null -- a document this same reader
     refuses with "non-numeric value".  The writer produced input its own
     reader rejects, from a profile the library called valid. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"parameters\": [1e308, 0.0, 0.0]}]";
    check(!parse(elemDoc(1, funcs.c_str()), elem, parseStr), "overflowing parameter",
          "parsed a parameter beyond the float32 range");
    check(has(parseStr, "non-finite"), "overflowing parameter",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"parameters\": [-1e308, 0.0, 0.0]}]";
    check(!parse(elemDoc(1, funcs.c_str()), elem, parseStr), "negative overflow",
          "parsed a parameter below the float32 range");
  }
  {
    /* The largest value that still fits stays acceptable, so the guard is a
       range test and not a magnitude cap. */
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"parameters\": [3.4028234663852886e38, 0.0, 1.0]}]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "float32 maximum",
          ("refused the largest representable float32: " + parseStr).c_str());
  }

  /* #2621: reserved was written by neither side, so a non-zero value could not
     survive an ICC -> JSON -> ICC cycle.  It now round-trips, and is still
     omitted when zero so no corpus document gains a key. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"reserved\": 9, \"parameters\": [1.0, 0.0, 1.0]}]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "reserved round trip",
          ("a legal reserved was refused: " + parseStr).c_str());

    IccJson out;
    check(elem.ToJson(out), "reserved round trip", "ToJson() refused the element");
    std::string text = out.dump();
    check(has(text, "\"reserved\":9") || has(text, "\"reserved\": 9"), "reserved round trip",
          ("reserved was dropped; ToJson() wrote: " + text).c_str());
  }
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = std::string("[") + kFunc1 + "]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "reserved omitted when zero",
          ("refused: " + parseStr).c_str());
    IccJson out;
    check(elem.ToJson(out), "reserved omitted when zero", "ToJson() refused the element");
    check(!has(out.dump(), "\"reserved\""), "reserved omitted when zero",
          ("a zero reserved was written out: " + out.dump()).c_str());
  }

  /* reserved is held to the same rule as functionType and reserved2: a value
     that is not an integer, or does not fit the icUInt32Number it is stored
     in, is refused rather than truncated or wrapped. */
  {
    const char *bad[] = {
      "[{\"functionType\": 0, \"reserved\": 1.5, \"parameters\": [1.0, 0.0, 1.0]}]",
      "[{\"functionType\": 0, \"reserved\": -1, \"parameters\": [1.0, 0.0, 1.0]}]",
      "[{\"functionType\": 0, \"reserved\": 4294967296, \"parameters\": [1.0, 0.0, 1.0]}]",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
      std::string parseStr;
      CIccMpeJsonToneMap elem;
      check(!parse(elemDoc(1, bad[i]), elem, parseStr), "reserved out of range",
            (std::string("parsed a malformed reserved: ") + bad[i]).c_str());
      check(has(parseStr, "reserved is out of range"), "reserved out of range",
            ("no diagnostic, parseStr was: " + parseStr).c_str());
    }
  }
  {
    /* The largest value that does fit is still read. */
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"reserved\": 4294967295, \"parameters\": [1.0, 0.0, 1.0]}]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "reserved uInt32 maximum",
          ("refused the largest representable reserved: " + parseStr).c_str());
  }

  /* #2633: NewCopy() must deep copy.  The copy is released first and then the
     original goes out of scope; on an unfixed build both free the same
     m_params and the process aborts on a double free rather than reporting a
     FAIL, so the red here is the abort. */
  {
    icFloatNumber params[3] = { 1.0f, 2.0f, 3.0f };
    CIccJsonToneMapFunc f;
    check(f.SetFunction(0, 3, params), "NewCopy deep copy", "SetFunction refused three parameters");

    CIccToneMapFunc *pCopy = f.NewCopy();
    check(pCopy != NULL, "NewCopy deep copy", "NewCopy returned NULL");
    if (pCopy) {
      /* The copy must carry the values, not merely a pointer to them. */
      IccJson jCopy;
      CIccJsonToneMapFunc *pJson = dynamic_cast<CIccJsonToneMapFunc *>(pCopy);
      check(pJson != NULL, "NewCopy deep copy", "NewCopy did not return the JSON subclass");
      if (pJson) {
        check(pJson->ToJson(jCopy), "NewCopy deep copy", "ToJson refused the copy");
        check(has(jCopy.dump(), "2.0"), "NewCopy deep copy",
              ("the copy lost its parameters: " + jCopy.dump()).c_str());
      }
      delete pCopy;
    }
    /* ~f runs at the end of this scope and must not free the same buffer. */
  }

  /* Adjacent to #2633, from the review of this change: SetFunction() free()s
     m_params and then only reassigns it inside `if (nArgs && pParams)`, so a
     call that supplies no parameters left the member dangling and the
     destructor freed it again.  On an unfixed build this block ABORTS on the
     double free; the check below only records that the call was reached. */
  {
    icFloatNumber params[3] = { 1.0f, 2.0f, 3.0f };
    CIccJsonToneMapFunc f;
    check(f.SetFunction(0, 3, params), "SetFunction NULL parameters",
          "SetFunction refused three parameters");
    /* Dropping the parameters must clear the member, not dangle it. */
    f.SetFunction(0, 3, NULL);
    IccJson j;
    check(f.ToJson(j), "SetFunction NULL parameters", "ToJson refused the emptied function");
    /* The count is cleared with the pointer, so the object does not claim
       parameters it has no buffer for -- clearing only the pointer would turn
       the double free into a NULL dereference right here. */
    check(has(j.dump(), "\"parameters\":[]"), "SetFunction NULL parameters",
          ("the emptied function still claims parameters: " + j.dump()).c_str());
    /* ~f runs at the end of this scope and must not free the same buffer. */
  }

  /* #2633: the copy must carry EVERY member, not just the parameters.  Driving
     this from a parsed function is what puts non-zero reserved and reserved2 on
     it -- SetFunction leaves both zero, and ToJson omits them when zero, so a
     copy constructor that dropped them would go unnoticed. */
  {
    std::string parseStr;
    CIccMpeJsonToneMap elem;
    std::string funcs = "[{\"functionType\": 0, \"reserved\": 7, \"reserved2\": 5, "
                        "\"parameters\": [1.0, 0.0, 1.0]}]";
    check(parse(elemDoc(1, funcs.c_str()), elem, parseStr), "NewCopy reserved fields",
          ("refused: " + parseStr).c_str());

    IccJson jSrc;
    check(elem.ToJson(jSrc), "NewCopy reserved fields", "ToJson refused the source element");

    /* Copying the element copies its functions through the same NewCopy(). */
    CIccMpeJsonToneMap copy(elem);
    IccJson jCopy;
    check(copy.ToJson(jCopy), "NewCopy reserved fields", "ToJson refused the copy");
    check(jCopy.dump() == jSrc.dump(), "NewCopy reserved fields",
          ("the copy differs from the source.\n  source: " + jSrc.dump() +
           "\n  copy:   " + jCopy.dump()).c_str());
  }

  /* #2633: every spelling that reaches icJsonSegPosFromStr() must round trip to
     the sentinel it names, with the sign read first.  "-inf" returning the
     POSITIVE maximum is the wrong-sign case; "+infinity" and "inf" both used to
     fall through to atof() and come back out as null. */
  {
    static const struct { const char *start; const char *end; } kSpellings[] = {
      { "-infinity", "+infinity" },
      { "-inf",      "+inf"      },
      { "-INFINITY", "+Inf"      },
      { "-infinity", "inf"       },
    };
    for (size_t i = 0; i < sizeof(kSpellings) / sizeof(kSpellings[0]); i++) {
      std::string doc = std::string("{\"segments\":[{\"type\":\"FormulaSegment\",\"start\":\"") +
                        kSpellings[i].start + "\",\"end\":\"" + kSpellings[i].end +
                        "\",\"functionType\":0,\"parameters\":[1.0,0.0,0.0,0.0]}]}";
      std::string label = std::string("segment position ") + kSpellings[i].start + "/" + kSpellings[i].end;

      std::string parseStr;
      CIccSegmentedCurveJson curve;
      check(curve.ParseJson(IccJson::parse(doc), parseStr), label.c_str(),
            ("refused: " + parseStr).c_str());

      IccJson out;
      check(curve.ToJson(out), label.c_str(), "ToJson refused the curve");
      std::string text = out.dump();
      check(has(text, "\"start\":\"-infinity\""), label.c_str(),
            ("start did not round trip: " + text).c_str());
      check(has(text, "\"end\":\"+infinity\""), label.c_str(),
            ("end did not round trip: " + text).c_str());
    }
  }

  /* #2633, the half the issue did not state: the old sign test was
     `s[0] == '-' && s.size() >= 4` with no look at the rest, so EVERY
     string-encoded negative number of four or more characters came back as the
     float32 minimum.  This is silent corruption of ordinary input, not just of
     the infinity spellings, and it is what these rows pin. */
  {
    static const struct { const char *text; const char *expect; } kNegatives[] = {
      { "-0.5",   "-0.5"   },
      { "-1.25",  "-1.25"  },
      { "-123.5", "-123.5" },
      { "-0.125", "-0.125" },
    };
    for (size_t i = 0; i < sizeof(kNegatives) / sizeof(kNegatives[0]); i++) {
      std::string doc = std::string("{\"segments\":[{\"type\":\"FormulaSegment\",\"start\":\"") +
                        kNegatives[i].text + "\",\"end\":0.75,"
                        "\"functionType\":0,\"parameters\":[1.0,0.0,0.0,0.0]}]}";
      std::string label = std::string("negative segment position ") + kNegatives[i].text;

      std::string parseStr;
      CIccSegmentedCurveJson curve;
      check(curve.ParseJson(IccJson::parse(doc), parseStr), label.c_str(),
            ("refused: " + parseStr).c_str());
      IccJson out;
      check(curve.ToJson(out), label.c_str(), "ToJson refused the curve");
      std::string text = out.dump();
      check(!has(text, "infinity"), label.c_str(),
            ("an ordinary negative number became a sentinel: " + text).c_str());
      check(has(text, kNegatives[i].expect), label.c_str(),
            ("the value was not preserved: " + text).c_str());
    }
  }

  /* Whitespace-padded sentinels were accepted before the rewrite and still are.
     icJsonSetSegPos() never emits padding, so this only matters to hand-written
     documents -- but silently turning one into null would be a narrowing. */
  {
    std::string doc = "{\"segments\":[{\"type\":\"FormulaSegment\",\"start\":\" -inf \","
                      "\"end\":\"  +infinity\","
                      "\"functionType\":0,\"parameters\":[1.0,0.0,0.0,0.0]}]}";
    std::string parseStr;
    CIccSegmentedCurveJson curve;
    check(curve.ParseJson(IccJson::parse(doc), parseStr), "padded segment position",
          ("refused: " + parseStr).c_str());
    IccJson out;
    check(curve.ToJson(out), "padded segment position", "ToJson refused the curve");
    std::string text = out.dump();
    check(has(text, "\"start\":\"-infinity\""), "padded segment position",
          ("padded start did not round trip: " + text).c_str());
    check(has(text, "\"end\":\"+infinity\""), "padded segment position",
          ("padded end did not round trip: " + text).c_str());
  }

  /* Control: an ordinary finite endpoint is still written as a number, so the
     fix did not turn every position into a sentinel. */
  {
    std::string doc = "{\"segments\":[{\"type\":\"FormulaSegment\",\"start\":0.25,\"end\":0.75,"
                      "\"functionType\":0,\"parameters\":[1.0,0.0,0.0,0.0]}]}";
    std::string parseStr;
    CIccSegmentedCurveJson curve;
    check(curve.ParseJson(IccJson::parse(doc), parseStr), "finite segment position",
          ("refused: " + parseStr).c_str());
    IccJson out;
    check(curve.ToJson(out), "finite segment position", "ToJson refused the curve");
    std::string text = out.dump();
    check(!has(text, "infinity"), "finite segment position",
          ("a finite position became a sentinel: " + text).c_str());
    check(has(text, "0.25") && has(text, "0.75"), "finite segment position",
          ("the finite positions were lost: " + text).c_str());
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
