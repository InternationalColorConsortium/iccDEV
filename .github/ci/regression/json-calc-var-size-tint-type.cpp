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
// CTest contract: the JSON calculator and tint array readers refuse values
// the XML readers refuse (#2852).
//
// A calculator variable's "size" was read into an int whose parse result was
// ignored and then narrowed to 16 bits, so 65539 declared 3 elements and
// "abc", 0 or -7 declared 1; "position" was read the same way. XML reads Size
// with icXmlParseU16 and Position with icXmlParseU32 and refuses the rest.
// A TintArrayElement's unknown "arrayType" quietly became float32ArrayType,
// where XML refuses an unknown type.
//
// Controls: size 3, size 0 (one element, as in XML), an absent arrayType and
// "float32ArrayType" must all parse. On an unfixed library every refusal case
// parses, so those checks fail in any build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccMpeJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"

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

bool parseCalc(const IccJson &var, std::string &err)
{
  IccJson j = {
    { "type", "CalculatorElement" },
    { "inputChannels", 3 },
    { "outputChannels", 3 },
    { "variables", IccJson::array({ var }) },
    { "mainFunction", "{ in(0,3) tput{v} tget{v} out(0,3) }" },
  };
  CIccMpeJsonCalculator calc;
  return calc.ParseJson(j, err);
}

void calcVar(bool wantOk, const char *key, const IccJson &value, const std::string &label)
{
  IccJson var = { { "name", "v" } };
  if (key)
    var[key] = value;
  std::string err;
  bool ok = parseCalc(var, err);
  check(ok == wantOk, label + (wantOk ? ": parses" : ": refused"));
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

bool parseTint(const char *szArrayType, std::string &err)
{
  IccJson j = {
    { "type", "TintArrayElement" },
    { "inputChannels", 1 },
    { "outputChannels", 3 },
    { "values", { 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 } },
  };
  if (szArrayType)
    j["arrayType"] = szArrayType;
  CIccMpeJsonTintArray tint;
  return tint.ParseJson(j, err);
}

void tint(bool wantOk, const char *szArrayType, const std::string &label)
{
  std::string err;
  bool ok = parseTint(szArrayType, err);
  check(ok == wantOk, label + (wantOk ? ": parses" : ": refused"));
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

} // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  calcVar(true,  "size", 3,      "variable size 3");
  calcVar(true,  "size", 0,      "variable size 0 (one element, as in XML)");
  calcVar(false, "size", 65539,  "variable size 65539");
  calcVar(false, "size", 65536,  "variable size 65536");
  calcVar(false, "size", -7,     "variable size -7");
  calcVar(false, "size", 3.9,    "variable size 3.9");
  calcVar(false, "size", "abc",  "variable size \"abc\"");
  calcVar(false, "position", -1,    "variable position -1");
  calcVar(false, "position", "abc", "variable position \"abc\"");

  tint(true,  NULL,                 "tint array with no arrayType");
  tint(true,  "float32ArrayType",   "tint array arrayType float32ArrayType");
  tint(false, "definitelyNotAType", "tint array arrayType definitelyNotAType");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
