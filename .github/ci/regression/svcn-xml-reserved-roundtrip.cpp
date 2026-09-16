/*
    File:       svcn-xml-reserved-roundtrip.cpp

    Contains:   CTest helper for the reserved fields of spectralViewingConditionsType
                in the XML reader/writer pair (#2560 item 2).

    CIccTagXmlSpectralViewingConditions::ToXml() writes the two reserved fields of
    ObserverFuncs and IlluminantSPD as Reserved="...", but ParseXml() looked only
    for "reserved".  icXmlFindAttr compares with icXmlStrCmp, which is a plain
    strcmp (IccUtilXml.h:116), so the writer's spelling never matched the reader's
    and a non-zero value was dropped on every XML round trip: the tag came back
    with both fields zero, and the writer then omitted the attribute entirely, so
    the loss left no trace in the output document.

    The fields are only written at all when they are non-zero, and no tracked
    document in Testing/ sets either one in either spelling -- which is why the
    whole corpus round-trips cleanly over this defect and nothing caught it.  That
    also means this helper's documents are the only thing that reaches the code:
    every case below has to build its own.

    Case 1 carries the red.  It asserts the value is PRESENT after the parse, not
    merely that the parse succeeded -- the unfixed reader returns true here and
    just leaves the fields at the zero ParseXml() initialised them to, so a case
    that only checked the return value would pass against the defect it is meant
    to catch.

    Case 3 is the discriminator in the other direction: a document with no
    reserved attribute must still come back zero and must NOT acquire one in the
    output.  A "fix" that wrote the attribute unconditionally, or that seeded the
    fields from something other than the document, fails there while passing
    case 1.

    Case 4 pins the lower-case spelling the reader has always accepted.  Nothing
    tracked uses it, but it is the spelling that worked before this change, and a
    reader that moved to "Reserved" alone would silently start ignoring a
    hand-authored document that loads correctly today -- the same failure mode as
    the original defect, pointed the other way.

    Case 5 keeps the existing range refusal: 65536 does not fit the
    icUInt16Number field and must be reported, not wrapped to 0.  Without it a
    reader rewritten to use atoi() would pass every other case here.

    Case 2's reach control (steps="3" in the output) is what proves the fixture
    is being read at all.  If ParseXml() bailed before the attributes for an
    unrelated reason, case 3's absence assertions would still hold vacuously.
*/

#include "IccTagXml.h"

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
    std::fprintf(stderr, "[svcn-xml-reserved-roundtrip] FAIL: %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// A complete spectralViewingConditionsType body at three spectral steps, with
// the two reserved attributes spelled by the caller.  An empty spelling omits
// the attribute altogether.
std::string doc(const char *observerAttr, const char *illuminantAttr)
{
  std::string s;
  s += "<spectralViewingConditionsType>\n";
  s += "  <StdObserver>CIE 1931 standard colorimetric observer</StdObserver>\n";
  s += "  <IlluminantXYZ X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/>\n";
  s += "  <ObserverFuncs start=\"400\" end=\"500\" steps=\"3\"";
  s += observerAttr;
  s += ">\n";
  s += "   0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9\n";
  s += "  </ObserverFuncs>\n";
  s += "  <StdIlluminant>D50</StdIlluminant>\n";
  s += "  <IlluminantSPD start=\"400\" end=\"500\" steps=\"3\"";
  s += illuminantAttr;
  s += ">\n";
  s += "   1.0 2.0 3.0\n";
  s += "  </IlluminantSPD>\n";
  s += "  <SurroundXYZ X=\"0.9642\" Y=\"1.0\" Z=\"0.8249\"/>\n";
  s += "</spectralViewingConditionsType>\n";
  return s;
}

// Run ParseXml over a literal document and hand back the tag.
bool parseFragment(CIccTagXmlSpectralViewingConditions &tag, const std::string &d,
                   std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(d.c_str(), (int)d.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc) {
    std::fprintf(stderr,
                 "[svcn-xml-reserved-roundtrip] test fixture is not valid XML\n");
    return false;
  }
  // CIccProfileXml hands a tag reader the tag element's FIRST CHILD, not the
  // element itself (IccProfileXml.cpp:945), because icXmlFindNode walks the
  // sibling chain from the node it is given and never descends.  Passing the
  // element here would find none of the children, and every assertion about a
  // reserved value would then hold vacuously against any reader at all.
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool rv = pRoot && tag.ParseXml(pRoot->children, parseStr);
  xmlFreeDoc(pDoc);
  return rv;
}

bool wellFormed(const std::string &xml, const char *label)
{
  // The writer emits a bare tag body, so wrap it before parsing.
  const std::string wrapped = "<w>" + xml + "</w>";
  xmlDoc *pDoc = xmlReadMemory(wrapped.c_str(), (int)wrapped.size(), "frag.xml",
                               NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc) {
    std::fprintf(stderr,
                 "[svcn-xml-reserved-roundtrip] not well formed (%s):\n%s\n",
                 label, xml.c_str());
    return false;
  }
  xmlFreeDoc(pDoc);
  return true;
}

} // namespace

int main()
{
  // ---------------------------------------------------------------------
  // 1. The spelling the writer emits is the spelling the reader takes.
  //    This is the case the unfixed reader fails.
  // ---------------------------------------------------------------------
  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(parseFragment(tag, doc(" Reserved=\"7\"", " Reserved=\"9\""), parseStr),
          "1: ParseXml rejected a valid document");
    check(tag.m_reserved2 == 7,
          "1: ObserverFuncs Reserved was dropped by the reader");
    check(tag.m_reserved3 == 9,
          "1: IlluminantSPD Reserved was dropped by the reader");
  }

  // ---------------------------------------------------------------------
  // 2. Both values survive a parse -> write round trip.
  // ---------------------------------------------------------------------
  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(parseFragment(tag, doc(" Reserved=\"7\"", " Reserved=\"9\""), parseStr),
          "2: ParseXml rejected a valid document");

    std::string xml;
    check(tag.ToXml(xml, ""), "2: ToXml returned false");
    check(wellFormed(xml, "round-trip output"),
          "2: round-trip output is not well formed");
    // Reach control: proves the fixture got as far as the two elements at all.
    check(has(xml, "steps=\"3\""),
          "2: fixture never reached the reader (no steps in output)");
    check(has(xml, "Reserved=\"7\""),
          "2: ObserverFuncs Reserved did not survive the round trip");
    check(has(xml, "Reserved=\"9\""),
          "2: IlluminantSPD Reserved did not survive the round trip");
  }

  // ---------------------------------------------------------------------
  // 3. A document without the attribute stays without it.
  // ---------------------------------------------------------------------
  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(parseFragment(tag, doc("", ""), parseStr),
          "3: ParseXml rejected a document with no reserved attribute");
    check(tag.m_reserved2 == 0, "3: ObserverFuncs reserved invented a value");
    check(tag.m_reserved3 == 0, "3: IlluminantSPD reserved invented a value");

    std::string xml;
    check(tag.ToXml(xml, ""), "3: ToXml returned false");
    check(!has(xml, "Reserved="),
          "3: writer emitted a Reserved attribute for a zero field");
  }

  // ---------------------------------------------------------------------
  // 4. The lower-case spelling the reader has always taken still works.
  // ---------------------------------------------------------------------
  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(parseFragment(tag, doc(" reserved=\"7\"", " reserved=\"9\""), parseStr),
          "4: ParseXml rejected the historical lower-case spelling");
    check(tag.m_reserved2 == 7,
          "4: ObserverFuncs lower-case reserved stopped being read");
    check(tag.m_reserved3 == 9,
          "4: IlluminantSPD lower-case reserved stopped being read");
  }

  // ---------------------------------------------------------------------
  // 5. A value too wide for the field is still refused, not wrapped.
  // ---------------------------------------------------------------------
  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(!parseFragment(tag, doc(" Reserved=\"65536\"", ""), parseStr),
          "5: an over-wide ObserverFuncs Reserved was accepted");
    check(tag.m_reserved2 == 0,
          "5: an over-wide ObserverFuncs Reserved was wrapped into the field");
  }

  {
    CIccTagXmlSpectralViewingConditions tag;
    std::string parseStr;
    check(!parseFragment(tag, doc("", " Reserved=\"65536\""), parseStr),
          "5: an over-wide IlluminantSPD Reserved was accepted");
    check(tag.m_reserved3 == 0,
          "5: an over-wide IlluminantSPD Reserved was wrapped into the field");
  }

  if (g_fail) {
    std::fprintf(stderr, "[svcn-xml-reserved-roundtrip] %d check(s) failed\n",
                 g_fail);
    return 1;
  }
  std::fprintf(stdout, "[svcn-xml-reserved-roundtrip] all checks passed\n");
  return 0;
}
