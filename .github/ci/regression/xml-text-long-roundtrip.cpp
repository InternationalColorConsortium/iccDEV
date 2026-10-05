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
 * xml-text-long-roundtrip.cpp -- text longer than 256 bytes survives XML<->ICC.
 *
 * icAnsiToUtf8() and icUtf8ToAnsi() (IccXML/IccLibXML/IccUtilXml.cpp) bounded
 * their copy with strnlen(szSrc, 256) on every non-Windows build, as the #740
 * fix for a strlen overread.  The overread came from FIXED-SIZE name fields;
 * the constant also truncated every text longer than 256 bytes, silently, in
 * both directions - a charTargetTag's CGATS data, a textType copyright, the
 * ASCII string of a v2 textDescriptionType - while the Windows branch copied
 * the text whole.  A 35,761-byte charTargetTag loaded from XML came back as
 * exactly 256 bytes.
 *
 * This pins three things, each at a length the old cap cannot pass:
 *   1. the bounded overloads read no further than the size they are given, for
 *      a fixed-size field that carries no NUL - the overread #740 guarded;
 *   2. XML -> ICC: long textType and textDescriptionType text loads byte-exact;
 *   3. ICC -> XML: the same text, set through the API rather than parsed from
 *      XML (so this direction is tested on its own), serialises byte-exact and
 *      then round-trips back.
 */

#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccUtilXml.h"
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
    std::fprintf(stderr, "[xml-text-long-roundtrip] FAIL %s\n", what);
  }
}

/* CGATS-shaped, as a charTargetTag carries it: ~35 KB, every row distinct, so
   a truncation anywhere shows as a mismatch rather than a lucky prefix. */
std::string cgatsPayload()
{
  std::string s =
    "CGATS.17\nORIGINATOR\t\"iccDEV regression\"\nNUMBER_OF_FIELDS\t7\n"
    "BEGIN_DATA_FORMAT\nSAMPLE_ID\tRGB_R\tRGB_G\tRGB_B\tXYZ_X\tXYZ_Y\tXYZ_Z\n"
    "END_DATA_FORMAT\nNUMBER_OF_SETS\t1000\nBEGIN_DATA\n";
  char row[128];
  for (int i = 0; i < 1000; i++) {
    std::snprintf(row, sizeof(row), "%d\t%d\t%d\t%d\t%.4f\t%.4f\t%.4f\n", i + 1, i % 256,
                  (i * 7) % 256, (i * 13) % 256, (i % 97) / 97.0, (i % 89) / 89.0,
                  (i % 83) / 83.0);
    s += row;
  }
  s += "END_DATA";
  return s;
}

/* 1,024 bytes of printable ASCII for the v2 textDescriptionType. */
std::string descPayload()
{
  std::string s;
  while (s.size() < 1024)
    s += "iccDEV v2 description longer than 256 bytes; ";
  s.resize(1024);
  return s;
}

bool writeTempDoc(const std::string &doc, std::string &path)
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
  std::string name = "/tmp/xml-text-long-roundtrip-XXXXXX";
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
  bool ok = std::fwrite(doc.data(), 1, doc.size(), f) == doc.size();
  std::fclose(f);
  return ok;
}

/* The CDATA payload of the first <TextData> after the given tag's opening
   element, or "" when absent. */
std::string cdataAfter(const std::string &xml, const char *szTagElement)
{
  size_t tag = xml.find(szTagElement);
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

std::string profileDoc(const std::string &target, const std::string &desc)
{
  return std::string(
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
    "    <charTargetTag> <textType>\n"
    "      <TextData><![CDATA[") + target + "]]></TextData>\n"
    "    </textType> </charTargetTag>\n"
    "    <profileDescriptionTag> <textDescriptionType>\n"
    "      <TextData><![CDATA[" + desc + "]]></TextData>\n"
    "    </textDescriptionType> </profileDescriptionTag>\n"
    "  </Tags>\n"
    "</IccProfile>\n";
}

/* 1. The bounded overloads: a full 32-byte field, no NUL, followed by bytes
      that are not part of it.  Reading them is the overread #740 guarded;
      the old constant of 256 would have read 224 bytes past this field. */
void testFixedFieldBound()
{
  struct {
    char field[32];
    char after[32];
  } s;
  std::memset(s.field, 'A', sizeof(s.field));
  std::memset(s.after, 'Z', sizeof(s.after) - 1);
  s.after[sizeof(s.after) - 1] = '\0';

  std::string buf;
  icAnsiToUtf8(buf, s.field, sizeof(s.field));
  check(buf.size() == 32, "icAnsiToUtf8 bounded: a full 32-byte field converts to exactly 32 bytes");
  check(buf.find('Z') == std::string::npos, "icAnsiToUtf8 bounded: nothing past the field is read");

  icUtf8ToAnsi(buf, s.field, sizeof(s.field));
  check(buf.size() == 32, "icUtf8ToAnsi bounded: a full 32-byte field converts to exactly 32 bytes");
  check(buf.find('Z') == std::string::npos, "icUtf8ToAnsi bounded: nothing past the field is read");

  s.field[5] = '\0';
  icAnsiToUtf8(buf, s.field, sizeof(s.field));
  check(buf == "AAAAA", "icAnsiToUtf8 bounded: stops at a NUL inside the field");

  icAnsiToUtf8(buf, NULL, 32);
  check(buf.empty(), "icAnsiToUtf8 bounded: a NULL source gives an empty string");

  /* And the unbounded form converts a NUL-terminated string whole. */
  std::string longText(5000, 'q');
  icAnsiToUtf8(buf, longText.c_str());
  check(buf.size() == 5000, "icAnsiToUtf8: a 5,000-byte NUL-terminated string converts whole (was 256)");
  icUtf8ToAnsi(buf, longText.c_str());
  check(buf.size() == 5000, "icUtf8ToAnsi: a 5,000-byte NUL-terminated string converts whole (was 256)");
}

const char *textOf(CIccProfile &profile, icTagSignature sig, icTagTypeSignature type)
{
  CIccTag *pTag = profile.FindTag(sig);
  if (!pTag || pTag->GetType() != type)
    return NULL;
  if (type == icSigTextType)
    return ((CIccTagText*)pTag)->GetText();
  return ((CIccTagTextDescription*)pTag)->GetText();
}

/* 2. XML -> ICC, then back to XML. */
void testXmlToIcc(const std::string &target, const std::string &desc)
{
  std::string path;
  if (!writeTempDoc(profileDoc(target, desc), path)) {
    check(false, "could not write the temporary XML document");
    return;
  }

  CIccProfileXml profile;
  std::string parseStr;
  bool bLoaded = profile.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  check(bLoaded, "the long-text XML loads");
  if (!bLoaded) {
    std::fprintf(stderr, "%s\n", parseStr.c_str());
    return;
  }

  const char *szTarget = textOf(profile, icSigCharTargetTag, icSigTextType);
  check(szTarget && std::strlen(szTarget) == target.size(),
        "XML->ICC: the textType charTargetTag keeps its full length (was 256)");
  check(szTarget && target == szTarget, "XML->ICC: the textType charTargetTag is byte-exact");

  const char *szDesc = textOf(profile, icSigProfileDescriptionTag, icSigTextDescriptionType);
  check(szDesc && std::strlen(szDesc) == desc.size(),
        "XML->ICC: the textDescriptionType ASCII string keeps its full length (was 256)");
  check(szDesc && desc == szDesc, "XML->ICC: the textDescriptionType ASCII string is byte-exact");

  /* Back to XML from a binary written and read back, as iccFromXml then
     iccToXml would do it, rather than from the parsed profile: a parsed
     profile has every tag-table offset at 0, and CIccProfileXml::ToXml()
     keyed shared-tag detection on the offset (#2767), so it wrote the second
     tag as SameAs the first and left nothing to compare. */
  std::string binPath;
  if (!writeTempDoc(std::string(), binPath)) {
    check(false, "could not reserve a temporary binary path");
    return;
  }
  check(SaveIccProfile(binPath.c_str(), &profile), "the loaded profile writes to a binary");

  CIccProfileXml reread;
  CIccFileIO io;
  bool bRead = io.Open(binPath.c_str(), "rb") && reread.Read(&io);
  io.Close();
  std::remove(binPath.c_str());
  check(bRead, "the binary reads back");
  if (!bRead)
    return;

  std::string xml;
  check(reread.ToXml(xml), "the re-read profile serialises back to XML");
  check(cdataAfter(xml, "<charTargetTag>") == target,
        "XML->ICC->XML: the charTargetTag CDATA is byte-exact");
  check(cdataAfter(xml, "<profileDescriptionTag>") == desc,
        "XML->ICC->XML: the textDescriptionType ASCII CDATA is byte-exact");
}

/* 3. ICC -> XML from a real binary, then back to ICC.

   The profile is built through the library API (so this direction does not
   depend on the XML parser), WRITTEN to a binary file, and read back into a
   CIccProfileXml exactly as iccToXml does.  Writing first matters: a profile
   that exists only in memory has every tag-table offset at 0, and
   CIccProfileXml::ToXml() detected shared tags by offset (#2767, fixed
   separately), so it would emit every tag after the first as SameAs the
   first; a binary read never triggers that. */
void testIccToXml(const std::string &target, const std::string &desc)
{
  std::string binPath;
  if (!writeTempDoc(std::string(), binPath)) {
    check(false, "could not reserve a temporary binary path");
    return;
  }

  {
    CIccProfile built;
    built.InitHeader();
    built.m_Header.version = icVersionNumberV2_1;
    built.m_Header.deviceClass = icSigDisplayClass;
    built.m_Header.colorSpace = icSigRgbData;
    built.m_Header.pcs = icSigXYZData;

    CIccTagText *pTarget = new CIccTagText();
    pTarget->SetText(target.c_str());
    check(built.AttachTag(icSigCharTargetTag, pTarget), "the API-built charTargetTag attaches");

    CIccTagTextDescription *pDesc = new CIccTagTextDescription();
    pDesc->SetText(desc.c_str());
    check(built.AttachTag(icSigProfileDescriptionTag, pDesc), "the API-built description attaches");

    check(SaveIccProfile(binPath.c_str(), &built), "the API-built profile writes to a binary");
  }

  CIccProfileXml profile;
  CIccFileIO io;
  bool bRead = io.Open(binPath.c_str(), "rb") && profile.Read(&io);
  io.Close();
  std::remove(binPath.c_str());
  check(bRead, "the binary reads back as an XML-capable profile, as iccToXml reads it");
  if (!bRead)
    return;

  std::string xml;
  check(profile.ToXml(xml), "the binary serialises to XML");
  check(xml.find("SameAs") == std::string::npos,
        "the two distinct text tags are serialised separately, not as SameAs");

  std::string got = cdataAfter(xml, "<charTargetTag>");
  check(got.size() == target.size(), "ICC->XML: the charTargetTag CDATA keeps its full length (was 256)");
  check(got == target, "ICC->XML: the charTargetTag CDATA is byte-exact");
  check(cdataAfter(xml, "<profileDescriptionTag>") == desc,
        "ICC->XML: the textDescriptionType ASCII CDATA is byte-exact");

  std::string path;
  if (!writeTempDoc(xml, path)) {
    check(false, "could not write the serialised XML");
    return;
  }
  CIccProfileXml back;
  std::string parseStr;
  bool bLoaded = back.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  check(bLoaded, "the serialised XML loads back");
  if (!bLoaded) {
    std::fprintf(stderr, "%s\n", parseStr.c_str());
    return;
  }

  const char *szTarget = textOf(back, icSigCharTargetTag, icSigTextType);
  check(szTarget && target == szTarget, "ICC->XML->ICC: the charTargetTag is byte-exact");
  const char *szDesc = textOf(back, icSigProfileDescriptionTag, icSigTextDescriptionType);
  check(szDesc && desc == szDesc, "ICC->XML->ICC: the textDescriptionType ASCII string is byte-exact");
}

}  // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  const std::string target = cgatsPayload();
  const std::string desc = descPayload();

  /* The guard on the guard: every assertion below is only meaningful because
     these payloads are longer than the old 256-byte cap. */
  check(target.size() > 256 && desc.size() > 256, "the test payloads exceed the old 256-byte cap");

  testFixedFieldBound();
  testXmlToIcc(target, desc);
  testIccToXml(target, desc);

  if (g_fail) {
    std::fprintf(stderr, "[xml-text-long-roundtrip] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[xml-text-long-roundtrip] PASS (charTargetTag %u bytes, description %u bytes)\n",
              (unsigned)target.size(), (unsigned)desc.size());
  return 0;
}
