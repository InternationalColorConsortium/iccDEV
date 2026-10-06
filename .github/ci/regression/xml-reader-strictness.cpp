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
 * xml-reader-strictness.cpp -- two places where the XML profile reader
 * accepted a malformed document and built a different profile from it, each
 * refused by the JSON reader's twin.
 *
 *   #2675  A tag written in full and then again as SameAs some other tag:
 *          AttachTag() refuses the second attachment because the signature
 *          already holds a different object, but ParseTag() discarded that
 *          result, so the document loaded as the first tag and the SameAs
 *          was silently ignored.  The same discard sat in the struct member
 *          reader.  The JSON reader fails with "Unable to attach sameAs tag".
 *   #2696  A tag element with two type children: ParseTag() took the first
 *          element child and never looked at the second, so a document with
 *          a second signatureType under the same tag loaded as the first and
 *          reported success.  The JSON reader refuses a Tags entry with a
 *          second member.
 *
 * Each case checks that the control document still loads, that the mutated
 * one is refused with the reason the reader now gives, and for #2675 that a
 * SameAs onto a signature not yet attached still works.
 */

#include "IccProfileXml.h"
#include "IccTagXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"

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
    std::fprintf(stderr, "[xml-reader-strictness] FAIL %s\n", what);
  }
}

bool has(const std::string &s, const char *what) { return s.find(what) != std::string::npos; }

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
  std::string name = "/tmp/xml-reader-strictness-XXXXXX";
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

/* A v5 display profile: copyright, description, a colorantInfo struct under a
   private tag, and whatever the case appends after them. */
std::string doc(const std::string &descriptionTypes, const std::string &structExtra, const std::string &tail)
{
  return std::string(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<IccProfile>\n"
    "  <Header>\n"
    "    <ProfileVersion>5.00</ProfileVersion>\n"
    "    <ProfileDeviceClass>mntr</ProfileDeviceClass>\n"
    "    <DataColourSpace>RGB </DataColourSpace>\n"
    "    <PCS>XYZ </PCS>\n"
    "    <CreationDateTime>2026-10-06T00:00:00</CreationDateTime>\n"
    "    <RenderingIntent>Perceptual</RenderingIntent>\n"
    "    <PCSIlluminant><XYZNumber X=\"0.9642\" Y=\"1.0000\" Z=\"0.8249\"/></PCSIlluminant>\n"
    "  </Header>\n"
    "  <Tags>\n"
    "    <copyrightTag> <textType>\n"
    "      <TextData><![CDATA[copyright text]]></TextData>\n"
    "    </textType> </copyrightTag>\n"
    "    <profileDescriptionTag> ") + descriptionTypes + " </profileDescriptionTag>\n"
    "    <PrivateTag TagSignature=\"tst1\"> <tagStructType>\n"
    "        <colorantInfoStructure> <MemberTags>\n"
    "          <cinfNameMbr> <utf8Type>\n"
    "            <TextData><![CDATA[cinf name]]></TextData>\n"
    "          </utf8Type> </cinfNameMbr>\n"
    "          <cinfLocalizedNameMbr> <utf8Type>\n"
    "            <TextData><![CDATA[cinf localized]]></TextData>\n"
    "          </utf8Type> </cinfLocalizedNameMbr>\n" + structExtra +
    "        </MemberTags> </colorantInfoStructure>\n"
    "    </tagStructType> </PrivateTag>\n" + tail +
    "  </Tags>\n"
    "</IccProfile>\n";
}

const char *kDescription =
  "<textDescriptionType>\n      <TextData><![CDATA[profile description]]></TextData>\n    </textDescriptionType>";

bool load(const std::string &text, std::string &parseStr, CIccProfileXml &profile)
{
  std::string path;
  if (!writeTempFile(text, path)) {
    parseStr = "could not write the temporary document";
    return false;
  }
  parseStr.clear();
  bool ok = profile.LoadXml(path.c_str(), "", &parseStr);
  std::remove(path.c_str());
  return ok;
}

void expectRefused(const std::string &text, const char *needle, const char *label)
{
  CIccProfileXml profile;
  std::string parseStr;
  bool ok = load(text, parseStr, profile);
  check(!ok, label);
  if (ok)
    return;
  std::string what = std::string(label) + ": the reason names the defect";
  check(has(parseStr, needle), what.c_str());
  if (!has(parseStr, needle))
    std::fprintf(stderr, "  got: %s\n", parseStr.c_str());
}

/* #2696 */
void testSecondTypeChild()
{
  CIccProfileXml profile;
  std::string parseStr;
  bool ok = load(doc(kDescription, "", ""), parseStr, profile);
  check(ok, "control: one type element per tag loads");
  if (!ok)
    std::fprintf(stderr, "  %s\n", parseStr.c_str());

  expectRefused(doc(std::string(kDescription) + "\n    <signatureType><Signature>zzzz</Signature></signatureType>", "", ""),
                "More than one type element for profileDescriptionTag: textDescriptionType and signatureType",
                "#2696: a second type element under one tag is refused rather than ignored");
}

/* #2675 */
void testSameAsConflict()
{
  /* Control: SameAs onto a signature not yet attached still works, at profile
     level and for a struct member. */
  CIccProfileXml profile;
  std::string parseStr;
  bool ok = load(doc(kDescription, "          <cinfPcsDataMbr SameAs=\"cinfNameMbr\" SameAsSignature=\"name\"/>\n",
                     "    <deviceModelDescTag SameAs=\"profileDescriptionTag\"/>\n"),
                 parseStr, profile);
  check(ok, "control: SameAs onto a new signature loads");
  if (ok) {
    check(profile.FindTag(icSigDeviceModelDescTag) == profile.FindTag(icSigProfileDescriptionTag),
          "control: the profile-level SameAs shares the object");
    CIccTag *pTag = profile.FindTag((icTagSignature)0x74737431);
    CIccTagStruct *pStruct = (pTag && pTag->GetType() == icSigTagStructType) ? (CIccTagStruct*)pTag : NULL;
    check(pStruct && pStruct->FindElem(icSigCinfPcsDataMbr) == pStruct->FindElem(icSigCinfNameMbr),
          "control: the member-level SameAs shares the object");
  }
  else {
    std::fprintf(stderr, "  %s\n", parseStr.c_str());
  }

  /* A second profileDescriptionTag, as SameAs the copyright: a different
     object than the one the signature already holds. */
  expectRefused(doc(kDescription, "", "    <profileDescriptionTag SameAs=\"copyrightTag\"/>\n"),
                "SameAs tag copyrightTag for profileDescriptionTag conflicts with an earlier profileDescriptionTag",
                "#2675: a SameAs onto a signature that holds a different object is refused rather than ignored");

  /* The same signature twice as SameAs the same object is not a conflict. */
  CIccProfileXml twice;
  ok = load(doc(kDescription, "", "    <deviceModelDescTag SameAs=\"profileDescriptionTag\"/>\n    <deviceModelDescTag SameAs=\"profileDescriptionTag\"/>\n"),
            parseStr, twice);
  check(ok, "#2675: the same SameAs stated twice still loads");

  /* The struct member twin. */
  expectRefused(doc(kDescription, "          <cinfNameMbr SameAs=\"cinfLocalizedNameMbr\" SameAsSignature=\"lcnm\"/>\n", ""),
                "SameAs tag cinfLocalizedNameMbr for cinfNameMbr conflicts with an earlier cinfNameMbr",
                "#2675: a struct member SameAs onto a member holding a different object is refused");
}

}  // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  testSecondTypeChild();
  testSameAsConflict();

  if (g_fail) {
    std::fprintf(stderr, "[xml-reader-strictness] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[xml-reader-strictness] PASS\n");
  return 0;
}
