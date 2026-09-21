// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       device-attributes-icc2.cpp

    Contains:   CTest helper for the header device attributes field, bytes 56 to 63
                (ICC.1:2022 clause 7.2.14 Table 22, ICC.2:2023 clause 7.2.16
                Table 19).

    ICC.1 defines bits 0-3, reserves bits 4-31 and leaves bits 32-63 to vendors.
    ICC.2 (since 2019) also defines bits 4-7: non-paper-based, textured,
    non-isotropic and self-luminous media, leaving 8-31 reserved.

    CIccProfile::CheckHeader() tested the field with the masks 0000FFF0h and
    FFFF0000h, which are 32-bit masks applied to the 64-bit value.  So it refused
    ICC.2 bits 4-7 at every version, reported reserved bits 16-31 only as a
    warning labelled "bits 32-63", and never looked at bits 32-63.  #2565.

    Case 1 pins the verdict for each bit that changes meaning between the two
    editions, and for the bits on either side of each boundary.  Bit 8 at v5 and
    bit 4 at v4 are the discriminators for the version gate: a mask of 0xFFFFFFF0
    at v5 passes every other v5 case, and dropping the gate passes every v5 case.

    Cases 2 and 3 cover the XML and JSON names for bits 4-7.  They are written
    only when set, so a profile without them serialises exactly as before, and
    a VendorSpecific value from an older writer still reads back as the same bits.

    Case 4 is bit 3 on the XML path.  The writer spelled it "blackAndwhite" and
    the reader matched only "blackAndWhite", so iccToXml then iccFromXml turned
    black and white media into colour media.  The writer now uses the reader's
    and the JSON spelling, and the reader also accepts the old one, so documents
    already written read back correctly.
*/

#include "IccProfile.h"
#include "IccUtil.h"
#include "IccUtilXml.h"
#include "IccUtilJson.h"

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
    std::fprintf(stderr, "[device-attributes-icc2] FAIL %s: %s\n", label, what);
  }
}

// The report lines CheckHeader() writes about the attributes field, and nothing
// else: an empty profile draws missing-tag errors that are not under test.
std::string attributeLines(icUInt32Number version, icUInt64Number attributes)
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = version;
  prof.m_Header.deviceClass = icSigDisplayClass;
  prof.m_Header.colorSpace = icSigRgbData;
  prof.m_Header.pcs = icSigXYZData;
  prof.m_Header.attributes = attributes;

  std::string report, lines;
  prof.Validate(report);

  size_t pos = 0;
  while (pos < report.size()) {
    size_t end = report.find('\n', pos);
    if (end == std::string::npos)
      end = report.size();
    std::string line = report.substr(pos, end - pos);
    if (line.find("device attributes") != std::string::npos)
      lines += line + "\n";
    pos = end + 1;
  }
  return lines;
}

const icUInt32Number kV4 = 0x04400000;
const icUInt32Number kV5 = icVersionNumberV5;

const char *kReserved4  = "NonCompliant! - Reserved device attributes (bits 4-31) are non-zero.\n";
const char *kReserved8  = "NonCompliant! - Reserved device attributes (bits 8-31) are non-zero.\n";
const char *kVendor     = "Warning! - Vendor-specific device attributes (bits 32-63) are non-zero.\n";

void expectLines(icUInt32Number version, icUInt64Number attributes, const std::string &want)
{
  char label[80];
  std::snprintf(label, sizeof(label), "validate v%x attributes %016llx",
                (unsigned int)(version >> 24), (unsigned long long)attributes);
  std::string got = attributeLines(version, attributes);
  if (got != want) {
    std::string what = "got [" + got + "] want [" + want + "]";
    check(false, label, what.c_str());
  }
}

void caseValidate()
{
  const icUInt64Number icc2Bits[] = { icNonPaperBased, icTextured, icNonIsotropic, icSelfLuminous };

  // Bits 0-3 are defined in both editions.
  expectLines(kV4, 0x0F, "");
  expectLines(kV5, 0x0F, "");

  // Bits 4-7: defined by ICC.2, reserved by ICC.1.
  for (icUInt64Number bit : icc2Bits) {
    expectLines(kV5, bit, "");
    expectLines(kV4, bit, kReserved4);
  }
  expectLines(kV5, 0xF0, "");

  // Bits 8-31 are reserved in both, including the upper half the old mask missed.
  expectLines(kV5, 0x100, kReserved8);
  expectLines(kV5, 0x10000, kReserved8);
  expectLines(kV5, 0x80000000, kReserved8);
  expectLines(kV4, 0x100, kReserved4);
  expectLines(kV4, 0x10000, kReserved4);
  expectLines(kV4, 0x80000000, kReserved4);

  // A later v5 minor version is still ICC.2.
  expectLines(0x05100000, icSelfLuminous, "");

  // Bits 32-63 are the vendor's: a warning only, at either version.
  expectLines(kV5, 0x100000000ULL, kVendor);
  expectLines(kV4, 0x8000000000000000ULL, kVendor);
  expectLines(kV5, 0x80000000000000F0ULL, kVendor);
}

bool xmlAttrValue(const std::string &fragment, icUInt64Number &value)
{
  xmlDoc *pDoc = xmlReadMemory(fragment.c_str(), (int)fragment.size(), "attr.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  if (pRoot)
    value = icGetDeviceAttrValue(pRoot);
  xmlFreeDoc(pDoc);
  return pRoot != NULL;
}

struct NamedBit {
  icUInt64Number bit;
  const char *xml;
  const char *jsonKey;
};

const NamedBit kNamed[] = {
  { icNonPaperBased, " MediaBase=\"nonPaper\"",         "MediaBase" },
  { icTextured,      " MediaTexture=\"textured\"",      "MediaTexture" },
  { icNonIsotropic,  " MediaIsotropy=\"nonIsotropic\"", "MediaIsotropy" },
  { icSelfLuminous,  " SelfLuminous=\"true\"",          "SelfLuminous" },
};

const char *kLegacyXml =
  "<DeviceAttributes ReflectiveOrTransparency=\"reflective\" GlossyOrMatte=\"glossy\""
  " MediaPolarity=\"positive\" MediaColour=\"colour\"/>\n";

void caseXml()
{
  check(icGetDeviceAttrName(0) == kLegacyXml, "xml zero", "a profile without bits 4-7 must write the four legacy attributes only");

  for (const NamedBit &n : kNamed) {
    std::string xml = icGetDeviceAttrName(n.bit);
    check(xml.find(n.xml) != std::string::npos, n.jsonKey, "xml name not written");
    check(xml.find("VendorSpecific") == std::string::npos, n.jsonKey, "xml bit also written as VendorSpecific");

    icUInt64Number back = 0;
    check(xmlAttrValue(xml, back) && back == n.bit, n.jsonKey, "xml name does not read back as its bit");
  }

  const icUInt64Number mixed = 0xABCD1234000001F5ULL;
  icUInt64Number back = 0;
  check(xmlAttrValue(icGetDeviceAttrName(mixed), back) && back == mixed, "xml mixed", "round trip changed the value");

  // An older writer put bits 4-7 in VendorSpecific; that still reads the same.
  check(xmlAttrValue("<DeviceAttributes VendorSpecific=\"00000000000000f0\"/>", back) && back == 0xF0,
        "xml legacy vendor", "VendorSpecific f0 does not read back as bits 4-7");
}

void caseJson()
{
  IccJson zero = icJsonGetDeviceAttr(0);
  check(zero.size() == 4, "json zero", "a profile without bits 4-7 must write the four legacy keys only");

  for (const NamedBit &n : kNamed) {
    IccJson j = icJsonGetDeviceAttr(n.bit);
    check(j.contains(n.jsonKey), n.jsonKey, "json key not written");
    check(!j.contains("VendorSpecific"), n.jsonKey, "json bit also written as VendorSpecific");
    check(icJsonParseDeviceAttr(j) == n.bit, n.jsonKey, "json key does not read back as its bit");
  }
  check(icJsonGetDeviceAttr(icSelfLuminous)["SelfLuminous"].is_boolean(), "json SelfLuminous", "not a boolean");

  const icUInt64Number mixed = 0xABCD1234000001F5ULL;
  check(icJsonParseDeviceAttr(icJsonGetDeviceAttr(mixed)) == mixed, "json mixed", "round trip changed the value");

  IccJson legacy;
  legacy["VendorSpecific"] = "00000000000000f0";
  check(icJsonParseDeviceAttr(legacy) == 0xF0, "json legacy vendor", "VendorSpecific f0 does not read back as bits 4-7");
}

void caseBlackAndWhite()
{
  std::string xml = icGetDeviceAttrName(icMediaBlackAndWhite);
  check(xml.find(" MediaColour=\"blackAndWhite\"") != std::string::npos, "xml bit 3", "not written as blackAndWhite");
  check(icJsonGetDeviceAttr(icMediaBlackAndWhite)["MediaColour"] == "blackAndWhite", "json bit 3", "XML and JSON spell bit 3 differently");

  icUInt64Number back = 0;
  check(xmlAttrValue(xml, back) && back == icMediaBlackAndWhite, "xml bit 3", "does not read back as bit 3");

  back = 0;
  check(xmlAttrValue("<DeviceAttributes MediaColour=\"blackAndwhite\"/>", back) && back == icMediaBlackAndWhite,
        "xml bit 3 legacy", "the spelling older builds wrote does not read back as bit 3");

  back = 1;
  check(xmlAttrValue("<DeviceAttributes MediaColour=\"colour\"/>", back) && back == 0, "xml colour", "colour does not read back as 0");
}

} // namespace

int main()
{
  caseValidate();
  caseXml();
  caseJson();
  caseBlackAndWhite();

  if (g_fail) {
    std::fprintf(stderr, "[device-attributes-icc2] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[device-attributes-icc2] all checks passed\n");
  return 0;
}
