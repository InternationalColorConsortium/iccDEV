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
// CTest contract: the JSON namedColor2 reader refuses device coordinates it
// would otherwise change (#2832).
//
// A colour's deviceCoords were read into an int and stored divided by 65535,
// so -1 and 65536 were written back clamped to 0 and 65535; a short list was
// zero-padded and a long one cut short. Each coordinate is a uInt16Number in
// the binary form, so the reader now requires exactly countOfDeviceCoords
// integers in 0..65535, as the XML reader does.
//
// On an unfixed library every refusal case parses, so those checks fail in any
// build.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagJson.h"

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

void expect(bool wantOk, const IccJson &coords, const char *label)
{
  IccJson colour = { { "name", "c" }, { "pcsCoords", { 0.5, 0.5, 0.5 } } };
  if (!coords.is_null())
    colour["deviceCoords"] = coords;
  IccJson j = { { "vendorFlag", 0 }, { "countOfDeviceCoords", 3 },
                { "colors", IccJson::array({ colour }) } };
  CIccTagJsonNamedColor2 tag;
  std::string err;
  bool ok = tag.ParseJson(j, err);
  check(ok == wantOk, std::string(label) + ": " + (wantOk ? "parses" : "refused"));
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

} // namespace

int main()
{
  expect(true,  { 0, 65535, 32768 },       "[0, 65535, 32768]");
  expect(false, { -1, 65536, 32768 },      "[-1, 65536, 32768]");
  expect(false, { 0, 65536, 32768 },       "[0, 65536, 32768]");
  expect(false, { 0, 1.5, 2 },             "[0, 1.5, 2]");
  expect(false, { "abc", 1, 2 },           "[\"abc\", 1, 2]");
  expect(false, { 1, 2 },                  "two coordinates for three");
  expect(false, { 1, 2, 3, 4 },            "four coordinates for three");
  expect(false, IccJson(),                 "no deviceCoords for three");

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
