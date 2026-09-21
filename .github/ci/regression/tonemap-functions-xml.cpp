// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       tonemap-functions-xml.cpp

    Contains:   CTest helper for the XML reader of a toneMapElement's
                ToneMapFunctions container.  #2609.

    Three things this pins:

    1. An element with no <ToneMapFunctions> container at all used to parse,
       leaving every tone function NULL for Validate() to refuse later, so the
       tools reported an invalid profile rather than the missing container.
       The container is now required, and an empty one is still refused, as it
       always was.

    2. A <ToneMapFunction> with fewer values than its function type takes was
       refused with nothing added to parseStr, so the caller printed only its
       own "Unable to parse" line.  It now says which function is short.

    3. Insert() fills the next slot from m_nFunc, which a <DuplicateFunction>
       used to leave where it was.  A ToneMapFunction following a
       DuplicateFunction therefore overwrote the duplicate's channel and left
       the last channel NULL: with three outputs, F D F parsed and then failed
       to write, while F F F and F D D wrote.  Each of those three orders is
       checked here, by the parameter that comes back on each channel.

    JSON is iccdev.tonemap-functions-json, since IccMpeXml.h and IccMpeJson.h
    cannot share a translation unit.
*/

#include "IccMpeBasic.h"
#include "IccMpeXml.h"
#include "IccIO.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

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
    std::fprintf(stderr, "[tonemap-functions-xml] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

/* A luminance curve that passes its input through, so Apply() shows the tone
   function's own third parameter on each channel. */
const char *kLumCurve =
  "<LuminanceCurve><SegmentedCurve>"
  "<FormulaSegment Start=\"-infinity\" End=\"+infinity\" FunctionType=\"0\">1 1 0 0</FormulaSegment>"
  "</SegmentedCurve></LuminanceCurve>";

std::string elemDoc(int nOut, const std::string &funcs)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf), "<ToneMapElement InputChannels=\"%d\" OutputChannels=\"%d\">", nOut + 1, nOut);
  return std::string(buf) + kLumCurve + funcs + "</ToneMapElement>";
}

bool parseXml(const std::string &doc, CIccMpeXmlToneMap &elem, std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "tonemap.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool ok = pRoot && elem.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return ok;
}

const char *kFunc0 = "<ToneMapFunction FunctionType=\"0\">1 0 0</ToneMapFunction>";
const char *kFunc1 = "<ToneMapFunction FunctionType=\"0\">1 0 1</ToneMapFunction>";
const char *kFunc2 = "<ToneMapFunction FunctionType=\"0\">1 0 2</ToneMapFunction>";

/* Applies the element and checks each channel against the third parameter of
   the function that channel should be using. */
void checkChannels(CIccMpeXmlToneMap &elem, const std::vector<icFloatNumber> &expected, const char *label)
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

} // namespace

int main()
{
  /* 1. The container. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    check(!parseXml(elemDoc(1, ""), elem, parseStr), "absent container", "parsed without a ToneMapFunctions container");
    check(has(parseStr, "Missing ToneMapFunctions"), "absent container",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    check(!parseXml(elemDoc(1, "<ToneMapFunctions/>"), elem, parseStr), "empty container", "parsed with an empty container");
    check(has(parseStr, "Missing ToneMap Functions"), "empty container",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* 2. A short parameter list. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions><ToneMapFunction FunctionType=\"0\">1 0</ToneMapFunction></ToneMapFunctions>");
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "short parameters", "parsed with two of three parameters");
    check(has(parseStr, "Too few parameters"), "short parameters",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* An element with no parameters at all.  CIccFloatArray::ParseArray()
     refuses an empty or whitespace-only node list, which was the one malformed
     spelling still reported by nothing but the caller's generic line. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = "<ToneMapFunctions><ToneMapFunction FunctionType=\"0\"/></ToneMapFunctions>";
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "no parameters", "parsed with no parameters");
    check(has(parseStr, "Missing parameters"), "no parameters",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = "<ToneMapFunctions><ToneMapFunction FunctionType=\"0\"> </ToneMapFunction></ToneMapFunctions>";
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "whitespace parameters",
          "parsed with whitespace for parameters");
    check(has(parseStr, "Missing parameters"), "whitespace parameters",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* The control for both: the same element with its container and a full
     parameter list parses. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions>") + kFunc1 + "</ToneMapFunctions>";
    check(parseXml(elemDoc(1, funcs), elem, parseStr), "control", ("refused a complete element: " + parseStr).c_str());
    checkChannels(elem, std::vector<icFloatNumber>(1, 1.0f), "control");
  }

  /* 3. Three outputs in each of the three orders.  F D F was the broken one. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions>") + kFunc0 + kFunc1 + kFunc2 + "</ToneMapFunctions>";
    check(parseXml(elemDoc(3, funcs), elem, parseStr), "F F F", ("refused: " + parseStr).c_str());
    icFloatNumber e[3] = { 0.0f, 1.0f, 2.0f };
    checkChannels(elem, std::vector<icFloatNumber>(e, e + 3), "F F F");
  }
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions>") + kFunc1 +
                        "<DuplicateFunction Index=\"0\"/><DuplicateFunction Index=\"0\"/></ToneMapFunctions>";
    check(parseXml(elemDoc(3, funcs), elem, parseStr), "F D D", ("refused: " + parseStr).c_str());
    icFloatNumber e[3] = { 1.0f, 1.0f, 1.0f };
    checkChannels(elem, std::vector<icFloatNumber>(e, e + 3), "F D D");
  }
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions>") + kFunc1 +
                        "<DuplicateFunction Index=\"0\"/>" + kFunc2 + "</ToneMapFunctions>";
    check(parseXml(elemDoc(3, funcs), elem, parseStr), "F D F", ("refused: " + parseStr).c_str());
    icFloatNumber e[3] = { 1.0f, 1.0f, 2.0f };
    checkChannels(elem, std::vector<icFloatNumber>(e, e + 3), "F D F");

    /* The element also has to write: the slot the duplicate skipped used to
       leave the last channel NULL, which Write() refuses. */
    CIccMemIO io;
    check(io.Alloc(4096, true) && elem.Write(&io), "F D F", "Write() refused the element");

    std::string xml;
    check(elem.ToXml(xml, ""), "F D F", "ToXml() refused the element");
    check(has(xml, "<DuplicateFunction Index=\"0\"/>"), "F D F",
          "channel 1 was not written as a duplicate of channel 0");
  }

  /* More entries than output channels is still refused. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs = std::string("<ToneMapFunctions>") + kFunc0 + kFunc1 + "</ToneMapFunctions>";
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "too many", "parsed two functions for one channel");
    check(has(parseStr, "Too many"), "too many", ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  /* #2619: icParseArrayValue() returns a real NaN for the literal "nan", and
     Validate() only counts parameters, so the element used to load clean and
     iccToJson then wrote the parameter back as null -- which the JSON reader
     refuses.  An out-of-range magnitude is NOT the vector here: that same
     helper saturates "1e308" to the float32 limit, so it arrives finite and
     must still be accepted. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs =
      std::string("<ToneMapFunctions>")
      + "<ToneMapFunction FunctionType=\"0\">nan 0 0</ToneMapFunction>"
      + "</ToneMapFunctions>";
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "nan parameter",
          "parsed a function whose first parameter is NaN");
    check(has(parseStr, "Non-finite parameter"), "nan parameter",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs =
      std::string("<ToneMapFunctions>")
      + "<ToneMapFunction FunctionType=\"0\">1e308 0 0</ToneMapFunction>"
      + "</ToneMapFunctions>";
    check(parseXml(elemDoc(1, funcs), elem, parseStr), "saturating parameter",
          ("refused a value the array parser saturates to a finite float: " + parseStr).c_str());
  }

  /* #2621: ToXml() wrote Reserved but ParseXml() never read it, so the value
     was dropped on an ICC -> XML -> ICC cycle while Reserved2 survived. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs =
      std::string("<ToneMapFunctions>")
      + "<ToneMapFunction FunctionType=\"0\" Reserved=\"9\" Reserved2=\"3\">1 0 0</ToneMapFunction>"
      + "</ToneMapFunctions>";
    check(parseXml(elemDoc(1, funcs), elem, parseStr), "reserved round trip",
          ("refused a function carrying Reserved: " + parseStr).c_str());

    std::string xml;
    check(elem.ToXml(xml, ""), "reserved round trip", "ToXml() refused the element");
    check(has(xml, "Reserved=\"9\""), "reserved round trip",
          ("Reserved was dropped; ToXml() wrote: " + xml).c_str());
    check(has(xml, "Reserved2=\"3\""), "reserved round trip",
          ("Reserved2 was dropped; ToXml() wrote: " + xml).c_str());
  }

  /* A Reserved that does not fit an icUInt32Number is refused rather than
     wrapped, the rule FunctionType and Reserved2 already follow. */
  {
    std::string parseStr;
    CIccMpeXmlToneMap elem;
    std::string funcs =
      std::string("<ToneMapFunctions>")
      + "<ToneMapFunction FunctionType=\"0\" Reserved=\"4294967296\">1 0 0</ToneMapFunction>"
      + "</ToneMapFunctions>";
    check(!parseXml(elemDoc(1, funcs), elem, parseStr), "reserved out of range",
          "parsed a Reserved past the uInt32 ceiling");
    /* Matched in full: "Invalid Reserved" alone is a prefix of the
       pre-existing "Invalid Reserved2 in Tone Map Function", so the shorter
       needle would also pass if the Reserved2 branch were the one refusing. */
    check(has(parseStr, "Invalid Reserved in Tone Map Function"), "reserved out of range",
          ("no diagnostic, parseStr was: " + parseStr).c_str());
  }

  if (g_fail) {
    std::printf("%d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
