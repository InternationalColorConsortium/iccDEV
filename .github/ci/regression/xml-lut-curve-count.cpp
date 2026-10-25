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
// CTest contract: the XML LUT reader refuses curves and blocks it would
// otherwise drop (#2838).
//
// icCurvesFromXml() stopped reading once it had one curve per channel and
// never looked at what followed, so a lutAtoBType declaring three output
// channels with four BCurves loaded the first three and the profile "parsed
// and saved correctly", byte-identical to the three-curve control. An element
// in a curve list that is not a curve was skipped the same way, and so was a
// second ACurves, BCurves, MCurves, Matrix or CLUT block.
//
// The three-curve control must still parse. On an unfixed library each of the
// other cases parses, so the refusal checks fail in any build.
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

void check(bool ok, const char *label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label);
  if (!ok)
    ++g_failures;
}

const char *kCurve = "<Curve IdentitySize=\"2\"/>";

std::string lut(const std::string &body)
{
  return "<lutAtoBType><Channels InputChannels=\"3\" OutputChannels=\"3\"/>" + body +
         "</lutAtoBType>";
}

std::string bcurves(int n, const std::string &extra = "")
{
  std::string s = "<BCurves>";
  for (int i = 0; i < n; i++)
    s += kCurve;
  return s + extra + "</BCurves>";
}

bool parse(const std::string &xml, std::string &err)
{
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "lut.xml", NULL, 0);
  if (!pDoc) {
    err = "libxml2 could not read the fixture";
    return false;
  }
  CIccTagXmlLutAtoB tag;
  bool ok = tag.ParseXml(xmlDocGetRootElement(pDoc)->children, err);
  xmlFreeDoc(pDoc);
  return ok;
}

void expect(bool wantOk, const std::string &xml, const char *label)
{
  std::string err;
  bool ok = parse(xml, err);
  check(ok == wantOk, label);
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

} // namespace

int main()
{
  expect(true, lut(bcurves(3)), "three channels, three BCurves: parses");
  expect(false, lut(bcurves(4)), "three channels, four BCurves: refused");
  expect(false, lut(bcurves(3, "<Bogus/>")), "a non-curve element in BCurves: refused");
  expect(false, lut(bcurves(2, "<Bogus/>") + ""), "a non-curve element among the curves: refused");
  expect(false, lut(bcurves(3) + bcurves(3)), "a second BCurves block: refused");

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
