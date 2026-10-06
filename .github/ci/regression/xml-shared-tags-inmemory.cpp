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
 * xml-shared-tags-inmemory.cpp -- the XML writer shares tags by object, not by
 * tag-table offset.
 *
 * CIccProfileXml::ToXml() and CIccTagXmlStruct::ToXml() emit a tag that is
 * attached under more than one signature once, and every later signature as
 * <... SameAs="first"/>.  Both detected sharing by the tag-table (or element)
 * offset, which only Write() and Read() set: a profile built with AttachTag()
 * or loaded with LoadXml() has every offset at 0, so every tag after the first
 * came out as SameAs the first, and the document was silently wrong.  The
 * binary writer, the SameAs reader and the JSON writer all share on the
 * CIccTag object; the XML writers now do the same.
 *
 * This pins:
 *   1. a profile built in memory with three distinct tags and one signature
 *      that reuses the second object: three tags in full, one SameAs, and the
 *      same <Tags> section as the profile gives after a binary write and read;
 *   2. the written XML loads back with the shared signature on one object;
 *   3. LoadXml() then ToXml(): both tags in full (every offset is 0 here);
 *   4. a struct built in memory: both distinct members in full, one SameAs.
 */

#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccIO.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[xml-shared-tags-inmemory] FAIL %s\n", what);
  }
}

size_t countOf(const std::string &s, const char *szNeedle)
{
  size_t n = 0;
  for (size_t at = s.find(szNeedle); at != std::string::npos; at = s.find(szNeedle, at + 1))
    ++n;
  return n;
}

/* The CDATA payload of the first <TextData> after the given element, or ""
   when the element or its CDATA is absent. */
std::string cdataAfter(const std::string &xml, const char *szElement)
{
  size_t tag = xml.find(szElement);
  if (tag == std::string::npos)
    return std::string();
  size_t a = xml.find("<![CDATA[", tag);
  if (a == std::string::npos)
    return std::string();
  a += 9;
  size_t b = xml.find("]]>", a);
  if (b == std::string::npos)
    return std::string();
  return xml.substr(a, b - a);
}

std::string tagsSection(const std::string &xml)
{
  size_t at = xml.find("<Tags>");
  return at == std::string::npos ? std::string() : xml.substr(at);
}

bool writeTempFile(const std::string &data, std::string &path)
{
#if defined(_WIN32)
  char tempPath[MAX_PATH];
  char tempFile[MAX_PATH];
  DWORD n = GetTempPathA(sizeof(tempPath), tempPath);
  if (!n || n >= sizeof(tempPath) || !GetTempFileNameA(tempPath, "icx", 0, tempFile))
    return false;
  path = tempFile;
  FILE *f = std::fopen(path.c_str(), "wb");
#else
  std::string name = "/tmp/xml-shared-tags-inmemory-XXXXXX";
  std::vector<char> writable(name.begin(), name.end());
  writable.push_back('\0');
  int fd = mkstemp(writable.data());
  if (fd == -1)
    return false;
  path = writable.data();
  FILE *f = fdopen(fd, "wb");
#endif
  if (!f)
    return false;
  bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
  std::fclose(f);
  return ok;
}

/* A text tag of the given type, created through the factory stack so that it
   is the XML-capable subclass, as LoadXml() and Read() would create it. */
CIccTag *newText(icTagTypeSignature type, const char *szText)
{
  CIccTag *pTag = CIccTag::Create(type);
  if (!pTag)
    return NULL;
  if (!pTag->GetExtension()) {
    delete pTag;
    return NULL;
  }
  if (type == icSigTextType)
    ((CIccTagText*)pTag)->SetText(szText);
  else if (type == icSigTextDescriptionType)
    ((CIccTagTextDescription*)pTag)->SetText(szText);
  else if (type == icSigUtf8TextType)
    ((CIccTagUtf8Text*)pTag)->SetText(szText);
  return pTag;
}

void initHeader(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV2_1;
  profile.m_Header.deviceClass = icSigDisplayClass;
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigXYZData;
}

const char *kCopyright = "copyright text";
const char *kDescription = "profile description";
const char *kTarget = "CGATS target data";

/* 1 + 2.  Three distinct tags and a fourth signature on the second object:
   deviceModelDescTag takes a textDescriptionType in a v2 profile, as the
   description does, so the shared object keeps the profile on-spec. */
void testProfileBuiltInMemory()
{
  CIccProfileXml profile;
  initHeader(profile);

  CIccTag *pCopyright = newText(icSigTextType, kCopyright);
  CIccTag *pDesc = newText(icSigTextDescriptionType, kDescription);
  CIccTag *pTarget = newText(icSigTextType, kTarget);
  check(pCopyright && pDesc && pTarget, "the factory stack creates XML-capable text tags");
  if (!pCopyright || !pDesc || !pTarget)
    return;

  check(profile.AttachTag(icSigCopyrightTag, pCopyright), "copyrightTag attaches");
  check(profile.AttachTag(icSigProfileDescriptionTag, pDesc), "profileDescriptionTag attaches");
  check(profile.AttachTag(icSigCharTargetTag, pTarget), "charTargetTag attaches");
  check(profile.AttachTag(icSigDeviceModelDescTag, pDesc),
        "deviceModelDescTag attaches the description object a second time");

  std::string xml;
  check(profile.ToXml(xml), "the in-memory profile serialises");

  check(countOf(xml, "SameAs=") == 1,
        "exactly one SameAs: the signature that shares an object (was three)");
  check(xml.find("<deviceModelDescTag SameAs=\"profileDescriptionTag\"") != std::string::npos,
        "the SameAs names the signature the object was first attached under");
  check(cdataAfter(xml, "<copyrightTag>") == kCopyright, "copyrightTag is serialised in full");
  check(cdataAfter(xml, "<profileDescriptionTag>") == kDescription,
        "profileDescriptionTag is serialised in full (was SameAs copyrightTag)");
  check(cdataAfter(xml, "<charTargetTag>") == kTarget,
        "charTargetTag is serialised in full (was SameAs copyrightTag)");

  /* The same profile after a binary write and read, which sets real offsets,
     must give the same <Tags> section: sharing by object and sharing by
     offset agree wherever offsets exist. */
  std::string binPath;
  if (!writeTempFile(std::string(), binPath)) {
    check(false, "could not reserve a temporary binary path");
    return;
  }
  check(SaveIccProfile(binPath.c_str(), &profile), "the in-memory profile writes to a binary");

  CIccProfileXml fromFile;
  CIccFileIO io;
  bool bRead = io.Open(binPath.c_str(), "rb") && fromFile.Read(&io);
  io.Close();
  std::remove(binPath.c_str());
  check(bRead, "the binary reads back");
  if (bRead) {
    std::string fromFileXml;
    check(fromFile.ToXml(fromFileXml), "the read-back profile serialises");
    check(tagsSection(fromFileXml) == tagsSection(xml),
          "the <Tags> section is the same from memory as from the binary");
  }

  /* 2. The in-memory document loads back with one object under both
        signatures and every distinct tag intact. */
  std::string xmlPath;
  if (!writeTempFile(xml, xmlPath)) {
    check(false, "could not write the serialised XML");
    return;
  }
  CIccProfileXml back;
  std::string parseStr;
  bool bLoaded = back.LoadXml(xmlPath.c_str(), "", &parseStr);
  std::remove(xmlPath.c_str());
  check(bLoaded, "the serialised XML loads back");
  if (!bLoaded) {
    std::fprintf(stderr, "%s\n", parseStr.c_str());
    return;
  }
  CIccTag *pBackDesc = back.FindTag(icSigProfileDescriptionTag);
  check(pBackDesc && pBackDesc == back.FindTag(icSigDeviceModelDescTag),
        "after the round trip both signatures hold one object");
  check(pBackDesc && pBackDesc->GetType() == icSigTextDescriptionType &&
        !std::strcmp(((CIccTagTextDescription*)pBackDesc)->GetText(), kDescription),
        "after the round trip profileDescriptionTag keeps its own text");
  CIccTag *pBackCopyright = back.FindTag(icSigCopyrightTag);
  check(pBackCopyright && pBackCopyright->GetType() == icSigTextType &&
        !std::strcmp(((CIccTagText*)pBackCopyright)->GetText(), kCopyright),
        "after the round trip copyrightTag keeps its own text");
  CIccTag *pBackTarget = back.FindTag(icSigCharTargetTag);
  check(pBackTarget && pBackTarget->GetType() == icSigTextType &&
        !std::strcmp(((CIccTagText*)pBackTarget)->GetText(), kTarget),
        "after the round trip charTargetTag keeps its own text");
}

/* 3. A parsed profile has every offset at 0 too. */
void testLoadXmlThenToXml()
{
  std::string doc =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>2.10</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2026-10-05T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0000\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <copyrightTag> <textType>\n"
    "      <TextData><![CDATA[" + std::string(kCopyright) + "]]></TextData>\n"
    "    </textType> </copyrightTag>\n"
    "    <profileDescriptionTag> <textDescriptionType>\n"
    "      <TextData><![CDATA[" + std::string(kDescription) + "]]></TextData>\n"
    "    </textDescriptionType> </profileDescriptionTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";

  std::string path;
  if (!writeTempFile(doc, path)) {
    check(false, "could not write the temporary XML document");
    return;
  }
  CIccProfileXml profile;
  std::string parseStr;
  bool bLoaded = profile.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  check(bLoaded, "the two-tag document loads");
  if (!bLoaded) {
    std::fprintf(stderr, "%s\n", parseStr.c_str());
    return;
  }

  std::string xml;
  check(profile.ToXml(xml), "the loaded profile serialises");
  check(xml.find("SameAs") == std::string::npos,
        "LoadXml then ToXml: no SameAs between two distinct tags (was one)");
  check(cdataAfter(xml, "<copyrightTag>") == kCopyright, "LoadXml then ToXml: copyrightTag in full");
  check(cdataAfter(xml, "<profileDescriptionTag>") == kDescription,
        "LoadXml then ToXml: profileDescriptionTag in full (was SameAs copyrightTag)");
}

/* 4. A struct built in memory: two distinct members, one shared signature. */
void testStructBuiltInMemory()
{
  CIccTag *pRaw = CIccTag::Create(icSigTagStructType);
  check(pRaw && pRaw->GetExtension(), "the factory stack creates an XML-capable struct");
  if (!pRaw || !pRaw->GetExtension()) {
    delete pRaw;
    return;
  }
  CIccTagStruct *pStruct = (CIccTagStruct*)pRaw;
  check(pStruct->SetTagStructType(icSigColorantInfoStruct), "the struct takes the colorantInfo type");

  const char *kName = "colorant name";
  const char *kLocalized = "localized name";
  CIccTag *pName = newText(icSigUtf8TextType, kName);
  CIccTag *pLocalized = newText(icSigUtf8TextType, kLocalized);
  check(pName && pLocalized, "the factory stack creates XML-capable utf8Text members");
  if (!pName || !pLocalized) {
    delete pName;
    delete pLocalized;
    delete pRaw;
    return;
  }
  check(pStruct->AttachElem(icSigCinfNameMbr, pName), "the name member attaches");
  check(pStruct->AttachElem(icSigCinfLocalizedNameMbr, pLocalized), "the localized-name member attaches");
  check(pStruct->AttachElem(icSigCinfPcsDataMbr, pName), "a third member attaches the name object again");

  std::string xml;
  check(((CIccTagXml*)pRaw->GetExtension())->ToXml(xml, ""), "the in-memory struct serialises");
  check(countOf(xml, "SameAs=") == 1, "struct: exactly one SameAs, for the shared member (was two)");
  check(countOf(xml, kName) == 1, "struct: the shared member's text appears once");
  check(countOf(xml, kLocalized) == 1,
        "struct: the second distinct member is serialised in full (was SameAs the first)");

  delete pRaw;
}

}  // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  testProfileBuiltInMemory();
  testLoadXmlThenToXml();
  testStructBuiltInMemory();

  if (g_fail) {
    std::fprintf(stderr, "[xml-shared-tags-inmemory] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[xml-shared-tags-inmemory] PASS\n");
  return 0;
}
