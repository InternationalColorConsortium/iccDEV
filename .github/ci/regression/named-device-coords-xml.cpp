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
// CTest contract: the XML namedColor2 reader refuses device coordinates it
// would otherwise change (#2832).
//
// Each colour's device coordinates went through CIccUInt16Array::ParseArray(),
// which clamped what it read -- "-1 65536" loaded as 0 65535 -- and turned a
// token it could not read into 0; the copy loop then zero-padded a short list
// and dropped the end of a long one. The profile was written and "parsed
// correctly". The binary form is one uInt16Number per coordinate, so a value
// outside the encoding's range has no representation; the reader now refuses
// it, and any count other than CountOfDeviceCoords.
//
// On an unfixed library every refusal case parses, so those checks fail in any
// build.
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

void check(bool ok, const std::string &label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label.c_str());
  if (!ok)
    ++g_failures;
}

bool parse(const char *encoding, const char *coords, std::string &err)
{
  std::string xml = std::string("<NamedColors VendorFlag=\"0\" CountOfDeviceCoords=\"3\" DeviceEncoding=\"") +
                    encoding + "\" Prefix=\"\" Suffix=\"\">"
                    "<LabNamedColor Name=\"c\" L=\"50\" a=\"0\" b=\"0\">" + coords +
                    "</LabNamedColor></NamedColors>";
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "ncl2.xml", NULL, 0);
  if (!pDoc) {
    err = "libxml2 could not read the fixture";
    return false;
  }
  CIccTagXmlNamedColor2 tag;
  bool ok = tag.ParseXml(xmlDocGetRootElement(pDoc), err);
  xmlFreeDoc(pDoc);
  return ok;
}

void expect(bool wantOk, const char *encoding, const char *coords)
{
  std::string err;
  bool ok = parse(encoding, coords, err);
  check(ok == wantOk, std::string(encoding) + " \"" + coords + "\": " + (wantOk ? "parses" : "refused"));
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

} // namespace

int main()
{
  expect(true,  "int16", "0 65535 32768");
  expect(false, "int16", "-1 65535 32768");
  expect(false, "int16", "0 65536 32768");
  expect(false, "int16", "abc 1 2");
  expect(false, "int16", "1 2");
  expect(false, "int16", "1 2 3 4");
  expect(true,  "int8",  "0 255 128");
  expect(false, "int8",  "0 256 128");
  expect(true,  "float", "0 1 0.5");
  expect(false, "float", "0 1.5 0.5");
  expect(false, "float", "0 -0.25 0.5");
  // Split around a comment, or partly in CDATA, as the array reader allows.
  expect(true,  "int16", "0 <!-- c --> 65535 32768");
  expect(true,  "int16", "0 65535 <![CDATA[32768]]>");

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
