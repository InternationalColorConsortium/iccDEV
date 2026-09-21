// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       measurement-type-icc2.cpp

    Contains:   CTest helper for the ICC.2 extensions to measurementType
                (ICC.2:2023 clause 10.2.15, Tables 56, 60 and 61).

    ICC.2 extends ICC.1's measurementType in two ways:
    - Table 60 adds standard illuminant encodings 9h-16h (black body and
      daylight defined by CCT, B, C, F1, F3-F7, F9-F12) to ICC.1's 0h-8h.
    - Table 56 adds an optional measurement condition in bytes 36-39
      (Table 61: 0 unknown, 1-4 = M0-M3).  A 36-byte tag has none.

    CIccTagMeasurement::Validate() accepted 0h-8h only, at every version.
    Read() ignored bytes 36-39 and Write() never wrote them, so a measurement
    condition was dropped by every binary, XML and JSON round trip.  #2565.

    measurementType has no CCT field (the CCT is carried by
    spectralViewingConditionsType), so 9h and Ah are accepted as encodings with
    nothing further to read.

    Case 1 pins the validator: each encoding at v4 and v5, and the first value
    past each table.  Case 2 pins the binary layout: the condition is read from
    a 40-byte tag, and written only when set, so a tag without one keeps the
    36-byte ICC.1 layout.  Cases 3 and 4 are the XML and JSON round trips,
    including element text with blanks around it.
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagXml.h"
#include "IccTagJson.h"
#include "IccIO.h"
#include "IccUtil.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[measurement-type-icc2] FAIL %s: %s\n", label, what);
  }
}

const icUInt32Number kV4 = 0x04400000;
const icUInt32Number kV5 = icVersionNumberV5;

// The report lines about the measurement tag, and nothing else.
std::string measurementLines(icUInt32Number version, icUInt32Number illuminant, icUInt32Number condition)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = version;
  prof.m_Header.deviceClass = icSigDisplayClass;
  prof.m_Header.colorSpace = icSigRgbData;
  prof.m_Header.pcs = icSigXYZData;

  CIccTagMeasurement *pMeas = new CIccTagMeasurement;
  pMeas->m_Data.stdObserver = icStdObs1931TwoDegrees;
  pMeas->m_Data.geometry = icGeometry045or450;
  pMeas->m_Data.illuminant = (icIlluminant)illuminant;
  pMeas->m_nMeasurementCondition = condition;
  prof.AttachTag(icSigMeasurementTag, pMeas);

  std::string report, lines;
  prof.Validate(report);

  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (line.find("measurementTag") != std::string::npos)
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

void caseValidate()
{
  char label[64];

  for (icUInt32Number ill = 0; ill <= 0x17; ill++) {
    std::string v5 = measurementLines(kV5, ill, 0);
    std::string v4 = measurementLines(kV4, ill, 0);

    std::snprintf(label, sizeof(label), "illuminant %02Xh v5", (unsigned int)ill);
    if (ill <= 0x16)
      check(v5.empty(), label, ("expected clean, got: " + v5).c_str());
    else
      check(has(v5, "Invalid standard illuminant encoding"), label, ("expected invalid, got: " + v5).c_str());

    std::snprintf(label, sizeof(label), "illuminant %02Xh v4", (unsigned int)ill);
    if (ill <= 0x08)
      check(v4.empty(), label, ("expected clean, got: " + v4).c_str());
    else if (ill <= 0x16)
      check(has(v4, "defined by ICC.2 only"), label, ("expected ICC.2-only, got: " + v4).c_str());
    else
      check(has(v4, "Invalid standard illuminant encoding"), label, ("expected invalid, got: " + v4).c_str());
  }

  for (icUInt32Number cond = 0; cond <= 5; cond++) {
    std::string v5 = measurementLines(kV5, icIlluminantD50, cond);
    std::string v4 = measurementLines(kV4, icIlluminantD50, cond);

    std::snprintf(label, sizeof(label), "condition %u v5", (unsigned int)cond);
    if (cond <= 4)
      check(v5.empty(), label, ("expected clean, got: " + v5).c_str());
    else
      check(has(v5, "Invalid measurement condition"), label, ("expected invalid, got: " + v5).c_str());

    std::snprintf(label, sizeof(label), "condition %u v4", (unsigned int)cond);
    if (cond == 0)
      check(v4.empty(), label, ("expected clean, got: " + v4).c_str());
    else
      check(has(v4, "Measurement condition is defined by ICC.2 only"), label, ("expected ICC.2-only, got: " + v4).c_str());
  }
}

// Writes the tag, checks the length, and reads it back into a fresh tag.
bool binaryRoundTrip(icUInt32Number condition, size_t wantLength, CIccTagMeasurement &back)
{
  CIccTagMeasurement tag;
  tag.m_Data.stdObserver = icStdObs1931TwoDegrees;
  tag.m_Data.illuminant = icIlluminantF10;
  tag.m_nMeasurementCondition = condition;

  CIccMemIO io;
  if (!io.Alloc(64, true))
    return false;
  if (!tag.Write(&io))
    return false;
  size_t len = (size_t)io.Tell();
  if (len != wantLength) {
    std::fprintf(stderr, "[measurement-type-icc2]   wrote %zu bytes, want %zu\n", len, wantLength);
    return false;
  }
  io.Seek(0, icSeekSet);
  return back.Read((icUInt32Number)len, &io);
}

void caseBinary()
{
  CIccTagMeasurement back;
  check(binaryRoundTrip(0, 36, back) && back.m_nMeasurementCondition == 0,
        "binary none", "a tag without a condition must write 36 bytes and read back 0");
  check(back.m_Data.illuminant == icIlluminantF10, "binary none", "illuminant changed");

  for (icUInt32Number cond = 1; cond <= 4; cond++) {
    CIccTagMeasurement b;
    char label[32];
    std::snprintf(label, sizeof(label), "binary M%u", (unsigned int)(cond - 1));
    check(binaryRoundTrip(cond, 40, b) && b.m_nMeasurementCondition == cond, label,
          "the condition must write 40 bytes and read back unchanged");
  }

  // A 40-byte tag from another writer, read by a tag that had a stale value.
  icUInt8Number raw[40] = { 'm','e','a','s', 0,0,0,0 };
  raw[35] = 0x14;          // illuminant F10
  raw[39] = 0x03;          // M2
  CIccMemIO io;
  io.Attach(raw, sizeof(raw));
  CIccTagMeasurement fromRaw;
  check(fromRaw.Read(sizeof(raw), &io) && fromRaw.m_nMeasurementCondition == 3 &&
          fromRaw.m_Data.illuminant == icIlluminantF10,
        "binary raw", "bytes 36-39 are not read as the measurement condition");

  icUInt8Number raw36[36] = { 'm','e','a','s', 0,0,0,0 };
  CIccMemIO io36;
  io36.Attach(raw36, sizeof(raw36));
  fromRaw.m_nMeasurementCondition = 3;
  check(fromRaw.Read(sizeof(raw36), &io36) && fromRaw.m_nMeasurementCondition == 0,
        "binary raw 36", "a 36-byte tag must read as condition 0");
}

bool parseXml(const std::string &body, icUInt32Number &condition, std::string &parseStr)
{
  std::string doc = "<measurementType>" + body + "</measurementType>";
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "meas.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;
  CIccTagXmlMeasurement tag;
  tag.m_nMeasurementCondition = 99;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool ok = pRoot && tag.ParseXml(pRoot->children, parseStr);
  condition = tag.m_nMeasurementCondition;
  xmlFreeDoc(pDoc);
  return ok;
}

void caseXml()
{
  const char *kBase =
    "<StandardObserver>CIE 1931 (two degree) standard observer</StandardObserver>"
    "<MeasurementBacking X=\"0\" Y=\"0\" Z=\"0\"/>"
    "<Geometry>Geometry 0-45 or 45-0</Geometry><Flare>0</Flare>"
    "<StandardIlluminant>Illuminant F10</StandardIlluminant>";

  for (icUInt32Number cond = 0; cond <= 5; cond++) {
    CIccTagXmlMeasurement tag;
    tag.m_Data.illuminant = icIlluminantF10;
    tag.m_nMeasurementCondition = cond;
    std::string xml;
    tag.ToXml(xml, "");

    char label[32];
    std::snprintf(label, sizeof(label), "xml condition %u", (unsigned int)cond);
    if (cond == 0) {
      check(!has(xml, "MeasurementCondition"), label, "written although absent");
      continue;
    }
    if (cond <= 4) {
      char want[64];
      std::snprintf(want, sizeof(want), "<MeasurementCondition>M%u</MeasurementCondition>", (unsigned int)(cond - 1));
      check(has(xml, want), label, "not written by name");
    }

    // Round trip: the written element, after the rest of the tag.
    size_t start = xml.find("<MeasurementCondition>");
    size_t end = xml.find("</MeasurementCondition>");
    std::string elem = (start != std::string::npos && end != std::string::npos)
                         ? xml.substr(start, end - start + strlen("</MeasurementCondition>")) : "";
    icUInt32Number back = 0;
    std::string parseStr;
    check(parseXml(std::string(kBase) + elem, back, parseStr) && back == cond, label, "does not read back");
  }

  icUInt32Number back = 0;
  std::string parseStr;
  check(parseXml(std::string(kBase), back, parseStr) && back == 0, "xml absent", "no element must read as 0");
  check(parseXml(std::string(kBase) + "<MeasurementCondition>\n      M3\n    </MeasurementCondition>", back, parseStr) &&
          back == 4,
        "xml pretty", "element text with blanks around it must parse");
  check(!parseXml(std::string(kBase) + "<MeasurementCondition>M4</MeasurementCondition>", back, parseStr),
        "xml bad name", "M4 is not a condition and must be refused");
  check(!parseXml(std::string(kBase) + "<MeasurementCondition>2x</MeasurementCondition>", back, parseStr),
        "xml trailing text", "trailing text must be refused");
  // An element child has NULL content: refused, not a null std::string.
  check(!parseXml(std::string(kBase) + "<MeasurementCondition><x/></MeasurementCondition>", back, parseStr),
        "xml element child", "a child element must be refused");
  check(!parseXml(std::string(kBase) + "<MeasurementCondition/>", back, parseStr),
        "xml empty", "an empty element must be refused");
}

void caseJson()
{
  for (icUInt32Number cond = 0; cond <= 4; cond++) {
    CIccTagJsonMeasurement tag;
    tag.m_Data.illuminant = icIlluminantF10;
    tag.m_nMeasurementCondition = cond;
    IccJson j;
    tag.ToJson(j);

    char label[32];
    std::snprintf(label, sizeof(label), "json condition %u", (unsigned int)cond);
    check(j.contains("measurementCondition") == (cond != 0), label, "key written iff the condition is set");

    CIccTagJsonMeasurement back;
    back.m_nMeasurementCondition = 99;
    std::string parseStr;
    check(back.ParseJson(j, parseStr) && back.m_nMeasurementCondition == cond &&
            back.m_Data.illuminant == icIlluminantF10,
          label, "does not read back");
  }

  // Malformed values are refused, as the XML reader refuses them.
  const char *bad[] = { "{\"measurementCondition\": \"M1\"}", "{\"measurementCondition\": -1}",
                        "{\"measurementCondition\": 1.5}" };
  for (const char *text : bad) {
    CIccTagJsonMeasurement tag;
    std::string parseStr;
    check(!tag.ParseJson(IccJson::parse(text), parseStr), text, "malformed measurementCondition accepted");
  }
}

} // namespace

int main()
{
  caseValidate();
  caseBinary();
  caseXml();
  caseJson();

  if (g_fail) {
    std::fprintf(stderr, "[measurement-type-icc2] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[measurement-type-icc2] all checks passed\n");
  return 0;
}
