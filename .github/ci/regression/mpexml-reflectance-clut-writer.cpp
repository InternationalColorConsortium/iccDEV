/*
    File:       mpexml-reflectance-clut-writer.cpp

    Contains:   CTest helper for CIccMpeXmlReflectanceCLUT::ToXml (#2560).

    The writer disagreed with its own reader in two ways:

      - it named the element "ReflectanceCLutElem", open and close tags, while
        the element factory in IccTagXml.cpp dispatches only on
        "ReflectanceCLutElement".  Any profile holding a reflectance CLUT failed
        iccToXml | iccFromXml with "Unknown Element Type (ReflectanceCLutElem)".
        Nothing caught it because no tracked document carried the element until
        Testing/Display/SpectralObserverElements.xml, and the CI XML round trip
        over the generated corpus rejected that profile at once;

      - it never wrote StorageType, which ParseXml reads and
        CIccMpeXmlEmissionCLUT::ToXml writes, so a non-zero storage type was
        reloaded as 0.

    The checks here re-parse the writer's output with the real reader rather
    than grepping for a tag name: a writer that spelled the element right but
    broke its body would pass a substring test.  The fragment uses
    StorageType="1" because 0 is the reader's default, and a writer that dropped
    the attribute would round-trip 0 perfectly.

    The emission element goes through the same steps as a control.  Its writer
    was already correct, so it must pass on the unfixed library too; if it does
    not, the harness -- not the reflectance writer -- is what broke.
*/

#include "IccMpeXml.h"
#include "IccTagXml.h"

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

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[mpexml-reflectance-clut-writer] FAIL: %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

// m_nStorageType is protected on CIccMpeSpectralCLUT.
class TestReflectance : public CIccMpeXmlReflectanceCLUT
{
public:
  icUInt16Number storageType() const { return m_nStorageType; }
};

class TestEmission : public CIccMpeXmlEmissionCLUT
{
public:
  icUInt16Number storageType() const { return m_nStorageType; }
};

// An element body with one input channel, a three-step spectral range and a
// two-point grid, so the table holds 2 x 3 values.
std::string fragment(const char *name)
{
  std::string s = "<";
  s += name;
  s += " InputChannels=\"1\" OutputChannels=\"3\" Flags=\"0\" StorageType=\"1\">\n"
       "  <Wavelengths start=\"400.0\" end=\"420.0\" steps=\"3\"/>\n"
       "  <WhiteData>1.0 1.0 1.0</WhiteData>\n"
       "  <GridPoints>2</GridPoints>\n"
       "  <TableData>0.1 0.2 0.3 0.7 0.8 0.9</TableData>\n"
       "</";
  s += name;
  s += ">\n";
  return s;
}

// MPE element readers take the element node itself: they read its attributes
// and search its children.
template <class T>
bool parse(T &elem, const std::string &doc, std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc)
    return false;
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool rv = pRoot && elem.ParseXml(pRoot, parseStr);
  xmlFreeDoc(pDoc);
  return rv;
}

template <class T>
void roundTrip(const char *name, const char *label)
{
  char msg[200];
  const std::string open = std::string("<") + name + " ";
  const std::string close = std::string("</") + name + ">";

  T src;
  std::string parseStr;
  std::snprintf(msg, sizeof(msg), "%s: the fixture fragment did not parse", label);
  check(parse(src, fragment(name), parseStr), msg);
  std::snprintf(msg, sizeof(msg), "%s: StorageType=\"1\" did not reach the reader", label);
  check(src.storageType() == 1, msg);

  std::string xml;
  std::snprintf(msg, sizeof(msg), "%s: ToXml returned false", label);
  check(src.ToXml(xml, ""), msg);

  std::snprintf(msg, sizeof(msg), "%s: writer did not open the element as <%s", label, name);
  check(has(xml, open.c_str()), msg);
  std::snprintf(msg, sizeof(msg), "%s: writer did not close the element as </%s>", label, name);
  check(has(xml, close.c_str()), msg);
  std::snprintf(msg, sizeof(msg), "%s: writer dropped StorageType", label);
  check(has(xml, "StorageType=\"1\""), msg);

  // Reload the way a profile load does: the element is created from its node
  // NAME by the factory, then parsed.  The element's own ParseXml never looks
  // at that name, so parsing straight into a T would accept a misspelt tag.
  bool bReloaded = false;
  std::string xml2;
  xmlDoc *pDoc = xmlReadMemory(xml.c_str(), (int)xml.size(), "out.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (pDoc) {
    xmlNode *pRoot = xmlDocGetRootElement(pDoc);
    CIccMultiProcessElement *pElem =
      pRoot ? CIccTagXmlMultiProcessElement::CreateElement((const icChar *)pRoot->name) : NULL;
    CIccMpeXml *pXml = pElem ? dynamic_cast<CIccMpeXml *>(pElem) : NULL;
    std::string parseStr2;
    if (pXml && pXml->ParseXml(pRoot, parseStr2)) {
      bReloaded = true;
      pXml->ToXml(xml2, "");
    }
    delete pElem;
    xmlFreeDoc(pDoc);
  }
  std::snprintf(msg, sizeof(msg), "%s: the element factory and reader refused the writer's own output", label);
  check(bReloaded, msg);
  std::snprintf(msg, sizeof(msg), "%s: StorageType did not survive the round trip", label);
  check(has(xml2, "StorageType=\"1\""), msg);

  if (g_fail)
    std::fprintf(stderr, "[mpexml-reflectance-clut-writer] %s writer output:\n%s\n",
                 label, xml.c_str());
}

} // namespace

int main()
{
  // Control first: this writer was already correct.
  roundTrip<TestEmission>("EmissionCLutElement", "emission (control)");
  const int controlFailures = g_fail;

  roundTrip<TestReflectance>("ReflectanceCLutElement", "reflectance");

  if (controlFailures) {
    std::fprintf(stderr,
                 "[mpexml-reflectance-clut-writer] the emission control failed: the "
                 "harness is broken, so the reflectance result means nothing\n");
  }
  if (g_fail) {
    std::fprintf(stderr, "[mpexml-reflectance-clut-writer] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[mpexml-reflectance-clut-writer] all checks passed\n");
  return 0;
}
