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
 * xml-struct-member-sameas.cpp -- a struct member attached under two
 * signatures round-trips through XML.
 *
 * CIccTagXmlStruct::ToXml() writes a shared member once and each later
 * signature as <... SameAs="first"/>.  The reader resolves that name through
 * the profile tag-name table, which holds no member name, so for every member
 * it needs the SameAsSignature attribute as well.  The writer's guard for that
 * attribute, size() || !strncmp(name, "PrivateSubTag", 13), was true for every
 * name and false only for an empty one: a struct with no handler (CreateStruct
 * allocates it with nothrow) wrote SameAs="" with no signature, which the
 * reader does not take as a SameAs at all (#2770).  The attribute is now
 * always written and an empty name gets the private spelling.
 *
 * This pins the reachable contract, which had no test: a private struct and a
 * colorantInfo struct, each with a member attached under two signatures,
 * serialise one SameAs with its SameAsSignature after a binary write and read,
 * and the colorantInfo document loads back with the shared signatures on one
 * object.  (A privateStruct does not load back on master for an unrelated
 * reason, the struct side of the #1885 mismatch, so its check stops at the
 * serialised side.)
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
    std::fprintf(stderr, "[xml-struct-member-sameas] FAIL %s\n", what);
  }
}

size_t countOf(const std::string &s, const char *szNeedle)
{
  size_t n = 0;
  for (size_t at = s.find(szNeedle); at != std::string::npos; at = s.find(szNeedle, at + 1))
    ++n;
  return n;
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
  std::string name = "/tmp/xml-struct-member-sameas-XXXXXX";
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

CIccTag *newUtf8(const char *szText)
{
  CIccTag *pTag = CIccTag::Create(icSigUtf8TextType);
  if (!pTag)
    return NULL;
  ((CIccTagUtf8Text*)pTag)->SetText(szText);
  return pTag;
}

/* A struct of the given type with members: first under sigA, a distinct one
   under sigB, and the first again under sigC. */
CIccTagStruct *newStruct(icStructSignature sigStruct, icSignature sigA, icSignature sigB, icSignature sigC,
                         const char *szA, const char *szB)
{
  CIccTag *pRaw = CIccTag::Create(icSigTagStructType);
  if (!pRaw)
    return NULL;
  CIccTagStruct *pStruct = (CIccTagStruct*)pRaw;
  CIccTag *pA = newUtf8(szA);
  CIccTag *pB = newUtf8(szB);
  if (!pStruct->SetTagStructType(sigStruct) || !pA || !pB ||
      !pStruct->AttachElem(sigA, pA) || !pStruct->AttachElem(sigB, pB) || !pStruct->AttachElem(sigC, pA)) {
    delete pA;
    delete pB;
    delete pRaw;
    return NULL;
  }
  return pStruct;
}

const char *textOfElem(CIccTagStruct *pStruct, icSignature sig)
{
  CIccTag *pElem = pStruct ? pStruct->FindElem(sig) : NULL;
  if (!pElem || pElem->GetType() != icSigUtf8TextType)
    return NULL;
  return (const char*)((CIccTagUtf8Text*)pElem)->GetText();
}

const icTagSignature kPrivateHolder = (icTagSignature)0x74737430;   /* 'tst0' */
const icTagSignature kCinfHolder = (icTagSignature)0x74737431;      /* 'tst1' */
const icStructSignature kPrivateStruct = (icStructSignature)0x70737430; /* 'pst0' */
const icSignature kMbrA = (icSignature)0x6d627241;  /* 'mbrA' */
const icSignature kMbrB = (icSignature)0x6d627242;  /* 'mbrB' */
const icSignature kMbrC = (icSignature)0x6d627243;  /* 'mbrC' */

/* Builds a profile holding one struct, writes it to a binary and reads it
   back as iccToXml does (so the offsets are real), and returns its XML. */
bool serialise(CIccTagStruct *pStruct, icTagSignature sigHolder, std::string &xml)
{
  CIccProfile built;
  built.InitHeader();
  built.m_Header.version = icVersionNumberV5;
  built.m_Header.deviceClass = icSigDisplayClass;
  built.m_Header.colorSpace = icSigRgbData;
  built.m_Header.pcs = icSigXYZData;
  if (!built.AttachTag(sigHolder, pStruct)) {
    delete pStruct;
    return false;
  }

  std::string binPath;
  if (!writeTempFile(std::string(), binPath))
    return false;
  bool bSaved = SaveIccProfile(binPath.c_str(), &built);

  CIccProfileXml profile;
  CIccFileIO io;
  bool bRead = bSaved && io.Open(binPath.c_str(), "rb") && profile.Read(&io);
  io.Close();
  std::remove(binPath.c_str());
  return bRead && profile.ToXml(xml);
}

/* 1. A private struct: its handler names every member "PrivateSubTag".
      Serialised side only, because a privateStruct does not load back on
      master for an unrelated reason: the writer spells the attribute
      StructSignature attribute and the reader looks for a StructureSignature
      element, the struct side of the mismatch #1885 reports for arrays. */
void testPrivateStruct()
{
  CIccTagStruct *pStruct = newStruct(kPrivateStruct, kMbrA, kMbrB, kMbrC, "private a", "private b");
  check(pStruct != NULL, "the private struct builds with a shared member");
  if (!pStruct)
    return;
  std::string xml;
  check(serialise(pStruct, kPrivateHolder, xml), "the private struct serialises after a binary write and read");
  check(countOf(xml, "SameAs=") == 1, "private struct: one SameAs, for the shared member");
  check(xml.find("<PrivateSubTag TagSignature=\"mbrC\" SameAs=\"PrivateSubTag\" SameAsSignature=\"mbrA\"/>") != std::string::npos,
        "private struct: the shared member names the private spelling and carries SameAsSignature");
  check(countOf(xml, "private a") == 1 && countOf(xml, "private b") == 1,
        "private struct: each distinct member is written once");
}

/* 2. A colorantInfo struct: named members, and the full round trip. */
void testColorantInfoStruct()
{
  CIccTagStruct *pStruct = newStruct(icSigColorantInfoStruct, icSigCinfNameMbr, icSigCinfLocalizedNameMbr,
                                     icSigCinfPcsDataMbr, "cinf name", "cinf localized");
  check(pStruct != NULL, "the colorantInfo struct builds with a shared member");
  if (!pStruct)
    return;
  std::string xml;
  check(serialise(pStruct, kCinfHolder, xml), "the colorantInfo struct serialises after a binary write and read");
  check(countOf(xml, "SameAs=") == 1, "colorantInfo: one SameAs, for the shared member");
  check(xml.find("<cinfPcsDataMbr SameAs=\"cinfNameMbr\" SameAsSignature=\"name\"/>") != std::string::npos,
        "colorantInfo: the shared member names the first member and carries SameAsSignature");
  check(countOf(xml, "cinf name") == 1 && countOf(xml, "cinf localized") == 1,
        "colorantInfo: each distinct member is written once");

  std::string xmlPath;
  if (!writeTempFile(xml, xmlPath)) {
    check(false, "could not write the serialised XML");
    return;
  }
  CIccProfileXml back;
  std::string parseStr;
  bool bLoaded = back.LoadXml(xmlPath.c_str(), "", &parseStr);
  std::remove(xmlPath.c_str());
  check(bLoaded, "the colorantInfo document loads back");
  if (!bLoaded) {
    std::fprintf(stderr, "%s\n", parseStr.c_str());
    return;
  }
  CIccTag *pTag = back.FindTag(kCinfHolder);
  CIccTagStruct *pBack = (pTag && pTag->GetType() == icSigTagStructType) ? (CIccTagStruct*)pTag : NULL;
  check(pBack && pBack->FindElem(icSigCinfNameMbr) &&
        pBack->FindElem(icSigCinfNameMbr) == pBack->FindElem(icSigCinfPcsDataMbr),
        "colorantInfo: both signatures hold one object after the round trip");
  const char *sz = textOfElem(pBack, icSigCinfLocalizedNameMbr);
  check(sz && !std::strcmp(sz, "cinf localized"), "colorantInfo: the distinct member keeps its text");
}

}  // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  testPrivateStruct();
  testColorantInfoStruct();

  if (g_fail) {
    std::fprintf(stderr, "[xml-struct-member-sameas] %d failure(s)\n", g_fail);
    return 1;
  }
  std::printf("[xml-struct-member-sameas] PASS\n");
  return 0;
}
