/*
    File:       chromaticity-colorant-encodings.cpp

    Contains:   CTest helper for the P3 and ITU-R BT.2020 colorant encodings of
                chromaticityType (ICC.1:2022 clause 10.2, Table 31).

    ICC.1:2022 Table 31 lists six phosphor or colorant encodings: ITU-R BT.709
    (0001h), SMPTE RP145 (0002h), EBU Tech. 3213-E (0003h), P22 (0004h), P3
    (0005h) and ITU-R BT.2020 (0006h).  iccDEV defined only the first four:
    icColorantEncoding stopped at icColorantP22, so CIccTagChromaticity::Validate()
    sent 0005h and 0006h to its default case and reported "Invalid colorant type
    encoding" for a tag carrying exactly the table's values, and iccFromJson
    exited 1 on such a profile.  The XML pair lost them outright:
    CIccInfo::GetColorantEncoding() named both "Customized Encoding", which
    icGetColorantValue() reads back as 0000h (Unknown).

    The BT.2020 red primary.  Table 31 prints it as (0,780, 0,292).  The standard
    the row names, Recommendation ITU-R BT.2020-2 (10/2015), Table 3 "System
    colorimetry", gives x 0.708, y 0.292, and ICC.1:2022 cites that edition as
    reference [17].  Green and blue agree in both documents.  Validate() uses the
    ITU value.  Case 3 pins that choice in both directions: the ITU coordinates
    validate, and the coordinates as printed in Table 31 do not.

    Case 2 perturbs each of the twelve new coordinates by one u16Fixed16 step, one
    at a time.  A transcription error in any single comparison would still let the
    conforming tags of case 1 pass if it happened to match what the test built, so
    this is what shows every comparison is present and bites.

    Case 4 keeps the default case reachable: 0007h is still not an encoding, and
    must still be reported.

    Case 5 records the reading of clause 10.2 for 0000h (Unknown): the channel
    count is fixed at three only for the listed encodings, and no rule ties it to
    the data colour space, so a one- or four-channel Unknown tag validates.  That
    is unchanged here, and pinned so a later count check is a visible decision.

    Cases 6 and 7 cover the names and the XML round trip, which are what actually
    lost the two encodings on the iccToXml / iccFromXml path.
*/

#include "IccTagXml.h"
#include "IccUtilXml.h"
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

const char *kDataMsg     = "Chromaticity data does not match specification.";
const char *kEncodingMsg = "Invalid colorant type encoding.";

void check(bool ok, const char *label, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[chromaticity-colorant-encodings] FAIL %s: %s\n", label, what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

struct Encoding {
  icColorantEncoding nType;
  const char *szLabel;
  const char *szName;
  double xy[3][2];
};

// ICC.1:2022 Table 31 for P3; Recommendation ITU-R BT.2020-2 Table 3 for BT.2020.
const Encoding kP3     = { icColorantP3, "P3", "P3",
                           { {0.680, 0.320}, {0.265, 0.690}, {0.150, 0.060} } };
const Encoding kBT2020 = { icColorantBT2020, "BT.2020", "ITU-R BT.2020",
                           { {0.708, 0.292}, {0.170, 0.797}, {0.131, 0.046} } };

void fill(CIccTagChromaticity &tag, icUInt16Number nType, const double xy[3][2])
{
  tag.SetSize(3);
  tag.m_nColorantType = nType;
  for (int i = 0; i < 3; i++) {
    tag[i].x = icDtoUF((icFloatNumber)xy[i][0]);
    tag[i].y = icDtoUF((icFloatNumber)xy[i][1]);
  }
}

icValidateStatus validate(const CIccTagChromaticity &tag, std::string &report)
{
  report.clear();
  return tag.Validate(icGetSigPath(icSigChromaticityTag), report, NULL);
}

// Case 1: the encoding's own values validate, with nothing reported.
void conformingTagValidates(const Encoding &e)
{
  CIccTagChromaticity tag;
  fill(tag, (icUInt16Number)e.nType, e.xy);

  std::string report;
  icValidateStatus rv = validate(tag, report);
  check(rv == icValidateOK, e.szLabel, "1: a tag with the encoding's own values is not icValidateOK");
  check(report.empty(), e.szLabel, "1: a tag with the encoding's own values reported something");
  if (!report.empty())
    std::fprintf(stderr, "%s", report.c_str());
}

// Case 2: each coordinate, one step off, is reported.
void everyCoordinateIsCompared(const Encoding &e)
{
  for (int i = 0; i < 3; i++) {
    for (int c = 0; c < 2; c++) {
      CIccTagChromaticity tag;
      fill(tag, (icUInt16Number)e.nType, e.xy);
      if (c == 0)
        tag[i].x += 1;
      else
        tag[i].y += 1;

      std::string report;
      icValidateStatus rv = validate(tag, report);

      char szWhat[128];
      std::snprintf(szWhat, sizeof(szWhat),
                    "2: channel %d %s one step off was not reported as non-compliant",
                    i + 1, c == 0 ? "x" : "y");
      check(rv == icValidateNonCompliant && has(report, kDataMsg), e.szLabel, szWhat);
    }
  }
}

xmlNode *firstChildOfParsed(xmlDoc *&pDoc, const std::string &xml)
{
  const std::string wrapped = "<w>" + xml + "</w>";
  pDoc = xmlReadMemory(wrapped.c_str(), (int)wrapped.size(), "frag.xml", NULL,
                       XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return NULL;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  return pRoot ? pRoot->children : NULL;
}

// Case 7: ToXml then ParseXml keeps the encoding and the values.
void xmlRoundTripKeepsEncoding(const Encoding &e)
{
  CIccTagXmlChromaticity src;
  fill(src, (icUInt16Number)e.nType, e.xy);

  std::string xml;
  check(src.ToXml(xml, ""), e.szLabel, "7: ToXml returned false");

  std::string want = std::string("<Colorant>") + e.szName + "</Colorant>";
  check(has(xml, want.c_str()), e.szLabel, "7: ToXml did not write the encoding's name");

  xmlDoc *pDoc = NULL;
  xmlNode *pNode = firstChildOfParsed(pDoc, xml);
  CIccTagXmlChromaticity dst;
  std::string parseStr;
  bool bParsed = pNode && dst.ParseXml(pNode, parseStr);
  if (pDoc)
    xmlFreeDoc(pDoc);

  if (!bParsed) {
    check(false, e.szLabel, "7: ParseXml rejected the writer's output");
    return;
  }

  check(dst.m_nColorantType == (icUInt16Number)e.nType, e.szLabel,
        "7: the encoding did not survive ToXml -> ParseXml");
  check(dst.GetSize() == 3, e.szLabel, "7: the channel count did not survive ToXml -> ParseXml");

  std::string report;
  check(validate(dst, report) == icValidateOK, e.szLabel,
        "7: the round-tripped tag no longer validates");
}

} // namespace

int main()
{
  conformingTagValidates(kP3);
  conformingTagValidates(kBT2020);

  everyCoordinateIsCompared(kP3);
  everyCoordinateIsCompared(kBT2020);

  // Case 3: the BT.2020 red primary as printed in ICC.1:2022 Table 31.
  {
    const double printed[3][2] = { {0.780, 0.292}, {0.170, 0.797}, {0.131, 0.046} };
    CIccTagChromaticity tag;
    fill(tag, (icUInt16Number)icColorantBT2020, printed);

    std::string report;
    icValidateStatus rv = validate(tag, report);
    check(rv == icValidateNonCompliant && has(report, kDataMsg), "BT.2020",
          "3: red x 0.780 (Table 31 as printed) was accepted; the check must follow ITU-R BT.2020-2");
  }

  // Case 4: an encoding Table 31 does not define is still reported.
  {
    CIccTagChromaticity tag;
    fill(tag, 0x0007, kBT2020.xy);

    std::string report;
    icValidateStatus rv = validate(tag, report);
    check(rv == icValidateNonCompliant && has(report, kEncodingMsg), "0007h",
          "4: an undefined encoding is no longer reported");
  }

  // Case 5: Unknown (0000h) places no rule on the channel count.
  for (icUInt16Number n = 1; n <= 4; n += 3) {
    CIccTagChromaticity tag;
    tag.SetSize(n);
    tag.m_nColorantType = icColorantUnknown;
    for (icUInt16Number i = 0; i < n; i++) {
      tag[i].x = icDtoUF((icFloatNumber)0.3);
      tag[i].y = icDtoUF((icFloatNumber)0.3);
    }

    std::string report;
    icValidateStatus rv = validate(tag, report);
    check(rv == icValidateOK && report.empty(), n == 1 ? "Unknown, 1 channel" : "Unknown, 4 channels",
          "5: an Unknown-encoding tag was reported");
  }

  // Case 6: every Table 31 name maps back to its value, and an undefined value
  // still reads back as Unknown.
  {
    CIccInfo info;
    for (icUInt16Number n = icColorantITU; n <= icColorantBT2020; n++) {
      const icChar *szName = info.GetColorantEncoding((icColorantEncoding)n);
      char szLabel[32];
      std::snprintf(szLabel, sizeof(szLabel), "encoding %u", (unsigned)n);
      check(icGetColorantValue(szName) == (icColorantEncoding)n, szLabel,
            "6: GetColorantEncoding() and icGetColorantValue() disagree");
    }

    check(!std::strcmp(info.GetColorantEncoding(icColorantP3), "P3"), "P3",
          "6: GetColorantEncoding() does not name 0005h P3");
    check(!std::strcmp(info.GetColorantEncoding(icColorantBT2020), "ITU-R BT.2020"), "BT.2020",
          "6: GetColorantEncoding() does not name 0006h ITU-R BT.2020");
    check(icGetColorantValue(info.GetColorantEncoding((icColorantEncoding)0x0007)) == icColorantUnknown,
          "0007h", "6: an undefined encoding no longer reads back as Unknown");
  }

  xmlRoundTripKeepsEncoding(kP3);
  xmlRoundTripKeepsEncoding(kBT2020);

  if (g_fail) {
    std::fprintf(stderr, "[chromaticity-colorant-encodings] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[chromaticity-colorant-encodings] all checks passed\n");
  return 0;
}
