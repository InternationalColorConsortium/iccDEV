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
// CTest contract: XML float arrays refuse values a float cannot hold (#2884).
//
// ICC.1:2022 4.3 excludes infinities and NaN from float32Number. The XML array
// reader had no way to refuse a value: its tokenizer split on any character
// that is not part of a number, so "abc" read as "a" and "inf" as "n", both 0;
// "nan"/"-nan" became a real NaN, which iccFromXml wrote into the profile; and
// 1e39 was clipped to FLT_MAX. The JSON reader refuses null CLUT samples, so
// the two readers disagreed. Every token of a float array must now be a
// complete, finite number within the array type's range.
//
// Integer arrays keep their behaviour (#2401). On an unfixed library every
// refusal case parses, so those checks fail in any build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

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

template <class A>
void text(const char *kind, bool wantOk, const char *body)
{
  A arr;
  bool ok = arr.ParseTextArray(body);
  check(ok == wantOk, std::string(kind) + " text \"" + body + "\": " + (wantOk ? "parses" : "refused"));
}

// <Data><f>..</f>...</Data> through CIccFloatArray::ParseArray, the form the
// CLUT reader takes.
void elements(bool wantOk, const char *a, const char *b)
{
  std::string xml = std::string("<Data><f>") + a + "</f><f>" + b + "</f></Data>";
  xmlDocPtr pDoc = xmlReadMemory(xml.data(), (int)xml.size(), "f.xml", NULL, 0);
  if (!pDoc) {
    check(false, "libxml2 could not read the fixture");
    return;
  }
  CIccFloatArray arr;
  bool ok = arr.ParseArray(xmlDocGetRootElement(pDoc)->children);
  xmlFreeDoc(pDoc);
  check(ok == wantOk, std::string("float <f>") + a + "</f><f>" + b + "</f>: " +
                      (wantOk ? "parses" : "refused"));
}

} // namespace

int main()
{
  text<CIccFloatArray>("float", true,  "0 0.5 1 -2.25e-3");
  text<CIccFloatArray>("float", false, "0 nan 1");
  text<CIccFloatArray>("float", false, "0 -nan 1");
  text<CIccFloatArray>("float", false, "0 inf 1");
  text<CIccFloatArray>("float", false, "0 abc 1");
  text<CIccFloatArray>("float", false, "0 1e39 1");
  text<CIccFloatArray>("float", false, "0 0.5x 1");
  text<CIccFloatArray>("float", false, "0.1 0.2 0.3 xyz");
  text<CIccFloatArray>("float", false, "0.1 0.2 0.3 ;");
  text<CIccFloat32Array>("float32", true,  "3.40282347e+38");
  text<CIccFloat32Array>("float32", false, "1e39");
  text<CIccFloat64Array>("float64", true,  "1e39 -1e300");
  text<CIccFloat64Array>("float64", false, "1e400");
  text<CIccFloat64Array>("float64", false, "nan");

  elements(true,  "0.25", "0.75");
  elements(false, "0.25", "nan");
  elements(false, "inf", "0.75");

  // Integer arrays are unchanged: out-of-range values still clip (#2401).
  text<CIccUInt8Array>("uInt8", true, "0 255 300");

  xmlCleanupParser();
  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
