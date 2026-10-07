/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

/*
 * xml-tag-reserved-roundtrip.cpp -- the reserved word of a tag array entry and
 * of a struct member survives XML.
 *
 * The binary format keeps the reserved word of every tag a tagArrayType or a
 * tagStructType holds.  Through XML:
 *
 *   - CIccTagXmlArray::ToXml wrote no reserved attribute on an entry's type
 *     node, and CIccTagXmlArray::ParseXml read the entry's reserved, and its
 *     fallback type attribute, from pNode, a node of the enclosing array,
 *     rather than from the entry's own node.
 *   - CIccTagXmlStruct::ToXml wrote no reserved attribute on a member's type
 *     node, which CIccTagXmlStruct::ParseTag has always read.
 *
 * Both now write a lower-case, decimal reserved attribute on the type node
 * when the word is non-zero, the form both readers parse.
 *
 * Cases:
 *   1. A UTF8TextArray whose first entry has reserved 3 writes reserved="3" on
 *      that entry's type node only, and parses back to 3 and 0.
 *   2. A document with reserved="3" on the first entry's type node parses to 3
 *      and 0 (the reader alone).
 *   3. A colorEncodingParamsStructure member with reserved 4 writes
 *      reserved="4" on its type node and parses back to 4.
 *   4. Control: zero reserved words write no attribute, on an array entry and
 *      on a struct member.
 *   5. An array entry written as <PrivateType type="utf8"> parses as a UTF-8
 *      text tag: the reader takes the type from the entry's node.  For a type
 *      the factory does not know, CIccTagXmlUnknown re-reads the type from its
 *      parent node itself, so the wrong node showed only when the attribute
 *      names a registered type.
 *
 * Cases 1, 2, 3 and 5 fail on an unfixed build.
 */

#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagComposite.h"
#include "IccTagBasic.h"
#include "IccUtilXml.h"

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
    std::fprintf(stderr, "[xml-tag-reserved-roundtrip] FAIL %s\n", what);
  }
}

size_t countOf(const std::string &s, const std::string &needle)
{
  size_t n = 0;
  for (size_t at = s.find(needle); at != std::string::npos; at = s.find(needle, at + 1))
    ++n;
  return n;
}

// ToXml writes the array or struct body without the enclosing tag type node,
// which the profile reader strips before calling ParseXml; wrap it the same way.
template <class T>
bool parseBody(T &tag, const std::string &body)
{
  const std::string doc = "<w>" + body + "</w>";
  xmlDoc *pDoc = xmlReadMemory(doc.c_str(), (int)doc.size(), "frag.xml", NULL,
                               XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
  xmlNode *pRoot = pDoc ? xmlDocGetRootElement(pDoc) : NULL;
  std::string parseStr;
  bool ok = pRoot && tag.ParseXml(pRoot->children, parseStr);
  if (!ok)
    std::fprintf(stderr, "%s", parseStr.c_str());
  if (pDoc)
    xmlFreeDoc(pDoc);
  return ok;
}

CIccTagXmlUtf8Text *newText(const char *szText, icUInt32Number nReserved)
{
  CIccTagXmlUtf8Text *pText = new CIccTagXmlUtf8Text();
  pText->SetText(szText);
  pText->m_nReserved = nReserved;
  return pText;
}

bool buildArray(CIccTagXmlArray &arr, icUInt32Number nReserved0)
{
  return arr.SetTagArrayType(icSigUtf8TextTypeArray) && arr.SetSize(2) &&
         arr.AttachTag(0, newText("first", nReserved0)) &&
         arr.AttachTag(1, newText("second", 0));
}

void checkParsedArray(const std::string &body, const char *szCase)
{
  CIccTagXmlArray dst;
  std::string what;
  if (!parseBody(dst, body)) {
    what = std::string(szCase) + ": the array did not parse";
    check(false, what.c_str());
    return;
  }
  CIccTag *p0 = dst.GetIndex(0), *p1 = dst.GetIndex(1);
  what = std::string(szCase) + ": entry 0 did not read back reserved 3";
  check(p0 && p0->m_nReserved == 3, what.c_str());
  what = std::string(szCase) + ": entry 1 did not read back reserved 0";
  check(p1 && p1->m_nReserved == 0, what.c_str());
}

const std::string kTextType = icGetTagSigTypeName(icSigUtf8TextType);

// Case 1.
void arrayEntryRoundTrips()
{
  CIccTagXmlArray src;
  if (!buildArray(src, 3)) {
    check(false, "1: could not build the array");
    return;
  }
  std::string xml;
  check(src.ToXml(xml, ""), "1: ToXml returned false");
  check(countOf(xml, "<" + kTextType + " reserved=\"3\">") == 1 &&
        countOf(xml, "reserved=") == 1,
        "1: ToXml did not write reserved=\"3\" on the first entry's type node alone");
  checkParsedArray(xml, "1");
}

// Case 2.
void arrayReaderReadsEntryNode()
{
  CIccTagXmlArray src;
  if (!buildArray(src, 0)) {
    check(false, "2: could not build the array");
    return;
  }
  std::string xml;
  check(src.ToXml(xml, ""), "2: ToXml returned false");
  const std::string plain = "<" + kTextType + ">";
  size_t at = xml.find(plain);
  if (at == std::string::npos) {
    check(false, "2: no entry type node in the written XML");
    return;
  }
  xml.replace(at, plain.size(), "<" + kTextType + " reserved=\"3\">");
  checkParsedArray(xml, "2");
}

// Case 3.
void structMemberRoundTrips()
{
  CIccTagXmlStruct src;
  if (!src.SetTagStructType(icSigColorEncodingParamsSruct)) {
    check(false, "3: could not set the struct type");
    return;
  }
  CIccTagXmlFloat32 *pXYZ = new CIccTagXmlFloat32();
  pXYZ->SetSize(3);
  (*pXYZ)[0] = 0.64f; (*pXYZ)[1] = 0.33f; (*pXYZ)[2] = 0.03f;
  pXYZ->m_nReserved = 4;
  if (!src.AttachElem(icSigCeptRedPrimaryXYZMbr, pXYZ)) {
    delete pXYZ;
    check(false, "3: could not attach the member");
    return;
  }

  std::string xml;
  check(src.ToXml(xml, ""), "3: ToXml returned false");
  const std::string typeName = icGetTagSigTypeName(icSigFloat32ArrayType);
  check(countOf(xml, "<" + typeName + " reserved=\"4\">") == 1,
        "3: ToXml did not write reserved=\"4\" on the member's type node");

  CIccTagXmlStruct dst;
  if (!parseBody(dst, xml)) {
    check(false, "3: the struct did not parse");
    return;
  }
  CIccTag *pMember = dst.FindElem(icSigCeptRedPrimaryXYZMbr);
  check(pMember && pMember->m_nReserved == 4, "3: the member did not read back reserved 4");
}

// Case 4.
void zeroReservedWritesNothing()
{
  CIccTagXmlArray arr;
  std::string xml;
  check(buildArray(arr, 0) && arr.ToXml(xml, "") && xml.find("reserved=") == std::string::npos,
        "4: a zero reserved word was written on an array entry");

  CIccTagXmlStruct st;
  CIccTagXmlFloat32 *pXYZ = new CIccTagXmlFloat32();
  pXYZ->SetSize(3);
  xml.clear();
  if (!st.SetTagStructType(icSigColorEncodingParamsSruct) ||
      !st.AttachElem(icSigCeptRedPrimaryXYZMbr, pXYZ)) {
    delete pXYZ;
    check(false, "4: could not build the struct");
    return;
  }
  check(st.ToXml(xml, "") && xml.find("reserved=") == std::string::npos,
        "4: a zero reserved word was written on a struct member");
}

// Case 5.
void arrayPrivateTypeFromEntryNode()
{
  CIccTagXmlArray src;
  if (!buildArray(src, 0)) {
    check(false, "5: could not build the array");
    return;
  }
  std::string xml;
  check(src.ToXml(xml, ""), "5: ToXml returned false");
  const std::string open = "<" + kTextType + ">", close = "</" + kTextType + ">";
  size_t at = xml.find(open), end = xml.find(close);
  if (at == std::string::npos || end == std::string::npos) {
    check(false, "5: no entry type node in the written XML");
    return;
  }
  xml.replace(end, close.size(), "</PrivateType>");
  xml.replace(at, open.size(), "<PrivateType type=\"utf8\">");

  CIccTagXmlArray dst;
  if (!parseBody(dst, xml)) {
    check(false, "5: an entry naming its type in a PrivateType attribute did not parse");
    return;
  }
  CIccTag *p0 = dst.GetIndex(0);
  check(p0 && p0->GetType() == icSigUtf8TextType,
        "5: the PrivateType entry was not read as a UTF-8 text tag");
}

} // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  arrayEntryRoundTrips();
  arrayReaderReadsEntryNode();
  structMemberRoundTrips();
  zeroReservedWritesNothing();
  arrayPrivateTypeFromEntryNode();

  xmlCleanupParser();

  if (g_fail) {
    std::fprintf(stderr, "[xml-tag-reserved-roundtrip] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::printf("[xml-tag-reserved-roundtrip] all checks passed\n");
  return 0;
}
