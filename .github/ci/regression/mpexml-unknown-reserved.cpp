// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: CIccMpeXmlUnknown lost the Reserved field in both directions (#1886).
//
// The writer formatted the attribute into `line` and then appended `buf`
// (IccMpeXml.cpp). `buf` still held the element's type signature from the
// icGetSigStr call a few lines earlier, so two things happened at once: the
// Reserved value never reached the document, and the raw signature was injected
// into the middle of the attribute list -- before the '>' that closes the start
// tag. The result was not merely a missing attribute but an unparseable
// document:
//
//   <UnknownElement Type="FF515112h" InputChannels="0" OutputChannels="0"FF5...
//
// Correcting the writer alone would have been a half fix. CIccMpeXmlUnknown's
// reader never parsed Reserved either -- the omission was invisible while the
// writer could not emit it -- so a value would have been written and then
// dropped on the way back in. Both sides are covered here so they cannot drift
// apart again.
//
// The assertions parse the writer's output with libxml2 rather than only
// grepping it, because the original defect produced text that still contained
// every substring a naive check would look for while being malformed overall.
//
// Red-green on master: assertion 1 fails on well-formedness AND on the missing
// attribute, assertion 2 fails because the reader drops the value, assertion 3
// fails because the reader accepted anything. Assertion 4 passes either way and
// is the control.
//
// Deliberately NOT asserted here: that InputChannels/OutputChannels round-trip.
// They do not, for an unrelated reason in IccProfLib rather than in this writer
// -- CIccMpeUnknown redeclares m_nInputChannels/m_nOutputChannels, shadowing the
// members of CIccMultiProcessElement, and SetChannels() writes the derived pair
// while the inherited NumInputChannels()/NumOutputChannels() still read the base
// pair that nothing ever populates. An unknown element therefore always
// serializes as InputChannels="0" OutputChannels="0". Reserved is unaffected
// because Read(), ParseXml() and ToXml() all resolve to the same shadowing copy.
// Pinning the channel behaviour belongs with that fix, not this one.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeXml.h"
#include "IccTagMPE.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
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
    std::fprintf(stderr, "[mpexml-unknown-reserved] FAIL: %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// Parse the emitted fragment to prove it is well formed. The writer defect
// produced a start tag that was never terminated, which only a real parser
// reliably rejects.
bool wellFormed(const std::string &xml, const char *label)
{
  xmlDoc *doc = xmlReadMemory(xml.c_str(), (int)xml.size(), "frag.xml", NULL,
                              XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!doc) {
    std::fprintf(stderr, "[mpexml-unknown-reserved] not well formed (%s):\n%s\n",
                 label, xml.c_str());
    return false;
  }
  xmlFreeDoc(doc);
  return true;
}

// m_nReserved is protected on CIccMpeUnknown. Deriving is how a test reaches it
// without widening the library's interface for a test's benefit.
class TestUnknown : public CIccMpeXmlUnknown
{
public:
  void setReserved(icUInt32Number n) { m_nReserved = n; }
  icUInt32Number reserved() const { return m_nReserved; }
};

// Run ParseXml over a literal document and hand back the element.
bool parseFragment(TestUnknown &elem, const char *doc, std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(doc, (int)std::char_traits<char>::length(doc),
                               "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc) {
    std::fprintf(stderr, "[mpexml-unknown-reserved] test fixture is not valid XML\n");
    return false;
  }
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool rv = pRoot && elem.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return rv;
}

const icUInt32Number kReserved = 305419896u;  // 0x12345678

} // namespace

int main()
{
  // ---------------------------------------------------------------------
  // 1. The writer emits Reserved and the document stays parseable.
  // ---------------------------------------------------------------------
  {
    TestUnknown elem;
    elem.SetType((icElemTypeSignature)0xFF515112u);
    elem.SetChannels(3, 3);
    elem.SetDataSize(4);
    elem.setReserved(kReserved);

    std::string xml;
    check(elem.ToXml(xml, ""), "1: ToXml returned false");
    check(wellFormed(xml, "writer output"),
          "1: writer output is not well-formed XML");
    check(has(xml, "Reserved=\"305419896\""),
          "1: Reserved attribute missing from writer output");
    // The signature belongs in the Type attribute and nowhere else. Two
    // occurrences means buf was appended into the attribute list again.
    {
      size_t first = xml.find("FF515112");
      size_t second = (first == std::string::npos)
                        ? std::string::npos
                        : xml.find("FF515112", first + 1);
      check(second == std::string::npos,
            "1: type signature appears twice -- buf appended into attributes");
    }
  }

  // ---------------------------------------------------------------------
  // 2. Reserved survives a full parse -> write round-trip.
  // ---------------------------------------------------------------------
  {
    TestUnknown elem;
    std::string parseStr;
    const char *doc =
      "<UnknownElement Type=\"FF515112h\" InputChannels=\"3\" "
      "OutputChannels=\"3\" Reserved=\"305419896\">00010203</UnknownElement>";

    check(parseFragment(elem, doc, parseStr), "2: ParseXml rejected a valid document");
    check(elem.reserved() == kReserved, "2: reader dropped the Reserved value");

    std::string xml;
    check(elem.ToXml(xml, ""), "2: ToXml returned false after parse");
    check(wellFormed(xml, "round-trip output"),
          "2: round-trip output is not well-formed XML");
    check(has(xml, "Reserved=\"305419896\""),
          "2: Reserved did not survive the round-trip");
  }

  // ---------------------------------------------------------------------
  // 3. A Reserved value that cannot fit icUInt32Number is refused rather
  //    than silently wrapped, matching the channel-count parsing above it.
  // ---------------------------------------------------------------------
  {
    TestUnknown elem;
    std::string parseStr;
    const char *doc =
      "<UnknownElement Type=\"FF515112h\" InputChannels=\"3\" "
      "OutputChannels=\"3\" Reserved=\"4294967296\"></UnknownElement>";

    check(!parseFragment(elem, doc, parseStr),
          "3: out-of-range Reserved was accepted");
    check(has(parseStr, "Invalid Reserved attribute"),
          "3: no diagnostic recorded for out-of-range Reserved");
  }

  // ---------------------------------------------------------------------
  // 4. Control: a document with no Reserved attribute -- every document
  //    written before this fix -- still reads as zero, and the writer
  //    omits the attribute entirely rather than emitting Reserved="0".
  // ---------------------------------------------------------------------
  {
    TestUnknown elem;
    std::string parseStr;
    const char *doc =
      "<UnknownElement Type=\"FF515112h\" InputChannels=\"3\" "
      "OutputChannels=\"3\"></UnknownElement>";

    check(parseFragment(elem, doc, parseStr),
          "4: document without Reserved was rejected");
    check(elem.reserved() == 0u, "4: absent Reserved did not default to zero");

    std::string xml;
    check(elem.ToXml(xml, ""), "4: ToXml returned false");
    check(wellFormed(xml, "no-reserved output"),
          "4: no-reserved output is not well-formed XML");
    check(!has(xml, "Reserved="),
          "4: Reserved attribute emitted for a zero value");
  }

  if (!g_fail)
    std::printf("[mpexml-unknown-reserved] all assertions passed\n");

  xmlCleanupParser();
  return g_fail;
}
