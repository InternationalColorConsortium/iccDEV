/*
    File:       xml-embedded-image-empty.cpp

    Contains:   CTest helper for a zero-byte image in the embeddedHeightImageType
                and embeddedNormalImageType XML reader/writer pairs (#2570).

    Both binary Read() methods need at least one image byte after the fixed
    fields (IccTagBasic.cpp), so a zero-byte image has no binary form that
    loads.  The XML side disagreed in both directions:

      - ParseXml() took an <Image> holding only whitespace, or a File= naming
        an empty file, as a zero-byte image and returned true.  iccFromXml then
        saved a tag that Read() refuses, so the profile it wrote could not be
        opened again ("surfaceMapTag - Tag has invalid structure").

      - ToXml() wrote a zero-byte image as a self-closing element with no
        <Image>, which ParseXml() has always refused.

    Cases 1 and 4 carry the red for the reader, case 3 for the writer.  Case 3
    also asserts the writer appended nothing, so a writer that refused only
    after emitting half an element fails.

    Case 2 is the reach control: a one-byte image must still parse, write, and
    parse again with the same byte, and its binary form must load.  Without it
    a reader that refused every <Image> would pass cases 1 and 4.  Case 4's
    one-byte file is the same control for the File= branch, which is gated off
    by default (#863) and would otherwise make the empty-file refusal vacuous.

    Case 5 pins the binary contract the XML side now follows.  If Read() is
    ever changed to load a zero-byte image, this case fails and says to revisit
    the XML refusals rather than leave them stricter than the format.
*/

#include "IccTagXml.h"
#include "IccIO.h"
#include "IccXmlConfig.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
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
    std::fprintf(stderr, "[xml-embedded-image-empty] FAIL: %s\n", what);
  }
}

bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

bool writeFile(const char *name, const std::string &data)
{
  std::ofstream out(name, std::ios::binary | std::ios::trunc);
  out << data;
  return (bool)out;
}

// The element the reader looks for, wrapped in its tag-type element.  An
// empty body writes the element self-closing, as the old writer did.
std::string doc(const char *typeName, const char *element, const char *attrs,
                const std::string &body)
{
  std::string s = std::string("<") + typeName + ">\n  <" + element + " " + attrs;
  if (body.empty())
    s += "/>\n";
  else
    s += ">\n" + body + "\n  </" + element + ">\n";
  s += std::string("</") + typeName + ">\n";
  return s;
}

template <class T>
bool parseFragment(T &tag, const std::string &d, std::string &parseStr)
{
  xmlDoc *pDoc = xmlReadMemory(d.c_str(), (int)d.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  if (!pDoc) {
    std::fprintf(stderr,
                 "[xml-embedded-image-empty] test fixture is not valid XML\n");
    return false;
  }
  // CIccProfileXml hands a tag reader the tag element's first child
  // (IccProfileXml.cpp), and icXmlFindNode walks siblings without descending.
  xmlNode *pRoot = xmlDocGetRootElement(pDoc);
  bool rv = pRoot && tag.ParseXml(pRoot->children, parseStr);
  xmlFreeDoc(pDoc);
  return rv;
}

// Write the tag through the binary writer and read it back with Read().
bool binaryRoundTrip(CIccTag &tag, CIccTag &back)
{
  CIccMemIO io;
  if (!io.Alloc(1024, true) || !tag.Write(&io))
    return false;
  icUInt32Number size = (icUInt32Number)io.Tell();
  io.Seek(0, icSeekSet);
  return back.Read(size, &io);
}

void fail(const std::string &label, const char *what)
{
  check(false, (label + " " + what).c_str());
}

template <class T>
void runKind(const char *typeName, const char *element, const char *attrs)
{
  const std::string label(element);
  auto expect = [&](bool ok, const char *what) {
    if (!ok)
      fail(label, what);
  };

  // 1. Whitespace-only, empty and missing <Image> are refused.
  for (const char *image : {"    <Image>\n    </Image>", "    <Image></Image>",
                            "    <Image/>", ""}) {
    T tag;
    std::string parseStr;
    expect(!parseFragment(tag, doc(typeName, element, attrs, image), parseStr),
           "1: an image with no data was accepted");
  }

  const std::string oneByte = "    <Image>\n      A5\n    </Image>";

  // 2. Reach control: a one-byte image parses, writes, re-parses with the
  //    same byte, and loads from its binary form.
  {
    T tag;
    std::string parseStr;
    expect(parseFragment(tag, doc(typeName, element, attrs, oneByte), parseStr),
           "2: a one-byte image was refused");
    expect(tag.GetSize() == 1 && tag.GetData(0) && *tag.GetData(0) == 0xA5,
           "2: the one-byte image did not load as 0xA5");

    std::string xml;
    expect(tag.ToXml(xml, ""), "2: ToXml refused a one-byte image");
    // icXmlDumpHexData writes lower-case hex; the re-parse below checks the value.
    expect(has(xml, "<Image>") && (has(xml, "a5") || has(xml, "A5")),
           "2: ToXml output has no <Image> holding the byte");

    T back;
    parseStr.clear();
    expect(parseFragment(back, std::string("<") + typeName + ">" + xml + "</" +
                                   typeName + ">",
                         parseStr) &&
               back.GetSize() == 1 && *back.GetData(0) == 0xA5,
           "2: the writer's output did not parse back to the same byte");

    T bin;
    expect(binaryRoundTrip(tag, bin) && bin.GetSize() == 1,
           "2: the one-byte image did not load from its binary form");
  }

  // 3. The writer refuses a zero-byte image and appends nothing.
  {
    T tag;
    std::string parseStr;
    expect(parseFragment(tag, doc(typeName, element, attrs, oneByte), parseStr),
           "3: setup parse failed");
    tag.SetSize(0);

    std::string xml = "unchanged";
    expect(!tag.ToXml(xml, ""), "3: ToXml wrote a zero-byte image");
    expect(xml == "unchanged", "3: ToXml appended output before refusing");
  }

  // 4. File=: an empty file is refused, a one-byte file is not.
  {
    const std::string emptyName = label + "-empty.bin";
    const std::string oneName = label + "-one.bin";
    expect(writeFile(emptyName.c_str(), "") &&
               writeFile(oneName.c_str(), "\xA5"),
           "4: could not write the image files");

    T empty;
    std::string parseStr;
    expect(!parseFragment(empty,
                          doc(typeName, element, attrs,
                              "    <Image File=\"" + emptyName + "\"/>"),
                          parseStr),
           "4: an empty image file was accepted");
    expect(has(parseStr, "is empty"),
           "4: the empty-file refusal gave no message");

    T one;
    parseStr.clear();
    expect(parseFragment(one,
                         doc(typeName, element, attrs,
                             "    <Image File=\"" + oneName + "\"/>"),
                         parseStr) &&
               one.GetSize() == 1,
           "4: a one-byte image file was refused (File= not reached)");
  }

  // 5. The binary contract: Read() refuses a zero-byte image.
  {
    T tag;
    tag.SetSize(0);
    T back;
    expect(!binaryRoundTrip(tag, back),
           "5: Read() now loads a zero-byte image; revisit the XML refusals "
           "in ParseXml/ToXml (#2570)");
  }
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
    return 1;
  }

  std::error_code ec;
  std::filesystem::create_directories(argv[1], ec);
  std::filesystem::current_path(argv[1], ec);
  if (ec) {
    std::fprintf(stderr, "[xml-embedded-image-empty] cannot use scratch dir '%s'\n",
                 argv[1]);
    return 1;
  }

  // File= is resolved relative to the working directory and gated off by
  // default (#863); iccFromXml opts in, so the test does too.
  IccXmlSetAllowFileIncludes(true);

  runKind<CIccTagXmlEmbeddedHeightImage>(
      "embeddedHeightImageType", "HeightImage",
      "SeamlessIndicator=\"0\" EncodingFormat=\"0\" "
      "MetersMinPixelValue=\"0\" MetersMaxPixelValue=\"0.001\"");
  runKind<CIccTagXmlEmbeddedNormalImage>(
      "embeddedNormalImageType", "NormalImage",
      "SeamlessIndicator=\"0\" EncodingFormat=\"0\"");

  if (g_fail) {
    std::fprintf(stderr, "[xml-embedded-image-empty] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[xml-embedded-image-empty] all checks passed\n");
  return 0;
}
