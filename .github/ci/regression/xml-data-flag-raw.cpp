/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// CTest contract: a dataType flag that is neither 0 nor 1 survives XML (#2846).
//
// The dataType flag word is 0 (ASCII) or 1 (binary); other values are
// reserved, but profiles carry them -- Apple writes 0x01000000 in its 'aapy'
// tag. The XML writer emitted "binary" when bit 0 was set and "ASCII"
// otherwise, and the reader took anything but "binary" as ASCII, so that flag
// came back as 0 and an unedited tag changed through iccToXml/iccFromXml. The
// writer now spells any other value as 0x-prefixed hex, the reader accepts
// that spelling, and an unknown Flag string is refused.
//
// On an unfixed library the 0x01000000 round trip comes back as 0 and the
// refusal cases parse, so those checks fail in any build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagXml.h"
#include "IccUtilXml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

// The flag word has no public getter; read it through a subclass.
class DataTag : public CIccTagXmlTagData
{
public:
  icUInt32Number GetDataFlag() const { return (icUInt32Number)m_nDataFlag; }
};

void check(bool ok, const std::string &label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label.c_str());
  if (!ok)
    ++g_failures;
}

bool parse(const std::string &xml, DataTag &tag, std::string &err)
{
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "data.xml", NULL, 0);
  if (!pDoc) {
    err = "libxml2 could not read the fixture";
    return false;
  }
  bool ok = tag.ParseXml(xmlDocGetRootElement(pDoc), err);
  xmlFreeDoc(pDoc);
  return ok;
}

std::string data(const char *flagAttr)
{
  return std::string("<Data") + flagAttr + ">0900</Data>";
}

void flagCase(const char *flagAttr, bool wantOk, icUInt32Number wantFlag)
{
  DataTag tag;
  std::string err;
  bool ok = parse(data(flagAttr), tag, err);
  std::string label = std::string("<Data") + flagAttr + ">";
  check(ok == wantOk, label + (wantOk ? ": parses" : ": refused"));
  if (ok && wantOk) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), ": flag 0x%08x", (unsigned)wantFlag);
    check(tag.GetDataFlag() == wantFlag, label + buf);
  }
}

} // namespace

int main()
{
  flagCase("",                       true,  icAsciiData);
  flagCase(" Flag=\"ASCII\"",        true,  icAsciiData);
  flagCase(" Flag=\"binary\"",       true,  icBinaryData);
  flagCase(" Flag=\"0x01000000\"",   true,  0x01000000);
  flagCase(" Flag=\"bogus\"",        false, 0);
  flagCase(" Flag=\"0x\"",           false, 0);
  flagCase(" Flag=\"0x1FFFFFFFF\"",  false, 0);
  flagCase(" Flag=\"01000000\"",     false, 0);

  // The 'aapy' shape round-trips: write, re-read, same flag.
  {
    DataTag tag;
    std::string err;
    check(parse(data(" Flag=\"0x01000000\""), tag, err), "aapy flag: first parse");
    std::string xml;
    check(tag.ToXml(xml), "aapy flag: written");
    check(xml.find("Flag=\"0x01000000\"") != std::string::npos, "aapy flag: written as 0x01000000");
    DataTag again;
    check(parse(xml, again, err), "aapy flag: re-read");
    check(again.GetDataFlag() == 0x01000000, "aapy flag: unchanged after the round trip");
  }

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
