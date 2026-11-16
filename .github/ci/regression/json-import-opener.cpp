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
// CTest contract for the one opener behind every file a JSON document names
// (#3073): calculator imports, CLUT data and tint array data.
//
// Each loader used to repeat the import gate and the path check. With imports
// off, the calculator import logged a note and carried on, so the conversion
// went ahead without the imported definitions, while a CLUT or tint file
// failed; the XML reader refuses all three. The calculator import also read
// through std::ifstream, so a host's IccJsonSetOpenFileIO() hook never saw it.
//
// For each loader: refused with imports off; refused for a "../", "./",
// "dir\name" or "D:name" path with imports on; loaded from a bare name with
// imports on.
// Then a recording IIccJsonOpenFileIO checks that all three go through the
// hook.
//
// Returns 0 on success; 1 if any check failed (each printed).
#include "IccMpeJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccJsonConfig.h"
#include "IccIoJson.h"
#include "IccUtilJson.h"
#include "IccTagLut.h"

#include <cstdio>
#include <string>
#include <vector>

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

const char *kImport = "iccdev-json-import-opener-import.json";
const char *kClut   = "iccdev-json-import-opener-clut.txt";
const char *kTint   = "iccdev-json-import-opener-tint.txt";

bool writeFile(const char *name, const char *text)
{
  std::FILE *f = std::fopen(name, "wb");
  if (!f)
    return false;
  std::fputs(text, f);
  std::fclose(f);
  return true;
}

bool calcImport(const std::string &file, std::string &err)
{
  IccJson j = {
    { "type", "CalculatorElement" },
    { "inputChannels", 1 },
    { "outputChannels", 1 },
    { "imports", IccJson::array({ { { "file", file } } }) },
    { "mainFunction", "{ in(0) out(0) }" },
  };
  CIccMpeJsonCalculator calc;
  return calc.ParseJson(j, err);
}

bool clutFile(const std::string &file, std::string &err)
{
  IccJson j = {
    { "gridPoints", IccJson::array({ 2 }) },
    { "file", file },
    { "format", "text" },
    { "encoding", "float" },
  };
  CIccCLUT *pCLUT = icCLUTFromJson(j, 1, 1, icConvertFloat, err);
  bool ok = pCLUT != NULL;
  delete pCLUT;
  return ok;
}

bool tintFile(const std::string &file, std::string &err)
{
  IccJson j = {
    { "type", "TintArrayElement" },
    { "inputChannels", 1 },
    { "outputChannels", 3 },
    { "file", file },
    { "format", "text" },
  };
  CIccMpeJsonTintArray tint;
  return tint.ParseJson(j, err);
}

typedef bool (*Loader)(const std::string &, std::string &);

struct Kind {
  const char *name;
  Loader load;
  const char *file;
};

const Kind kKinds[] = {
  { "calculator import", calcImport, kImport },
  { "CLUT file",         clutFile,   kClut },
  { "tint file",         tintFile,   kTint },
};

void expect(const Kind &k, const std::string &file, bool wantOk, const char *wantText,
            const std::string &what)
{
  std::string err;
  bool ok = k.load(file, err);
  std::string label = std::string(k.name) + ": " + what;
  check(ok == wantOk, label);
  if (wantText)
    check(err.find(wantText) != std::string::npos,
          label + " (diagnostic mentions \"" + wantText + "\")");
  if (ok != wantOk && !err.empty())
    std::printf("      %s", err.c_str());
}

class Recorder : public IIccJsonOpenFileIO
{
public:
  std::vector<std::string> names;
  CIccJsonStandardFileIO standard;
  CIccIO *OpenFile(const icChar *szFilename, const char *szAttr) override
  {
    names.push_back(szFilename ? szFilename : "");
    return standard.OpenFile(szFilename, szAttr);
  }
};

} // namespace

int main()
{
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  bool fixtures = writeFile(kImport, "{ }") &&
                  writeFile(kClut, "0 1") &&
                  writeFile(kTint, "0 0 0 1 1 1");
  check(fixtures, "fixtures written");
  if (!fixtures)
    return 1;

  for (const Kind &k : kKinds) {
    IccJsonSetAllowFileImports(false);
    expect(k, k.file, false, "disabled", "refused with imports off");

    IccJsonSetAllowFileImports(true);
    expect(k, k.file, true, NULL, "bare name loads with imports on");
    expect(k, std::string("../") + k.file, false, "Unsafe", "\"../\" path refused");
    expect(k, std::string("./") + k.file, false, "Unsafe", "\"./\" path refused");
    expect(k, std::string(".\\") + k.file, false, "Unsafe", "backslash path refused");
    expect(k, std::string("D:") + k.file, false, "Unsafe", "drive-relative name refused");
  }

  // Every loader opens through the host's hook.
  static Recorder rec;
  static CIccJsonStandardFileIO standard;
  IccJsonSetOpenFileIO(&rec);
  IccJsonSetAllowFileImports(true);
  for (const Kind &k : kKinds) {
    rec.names.clear();
    std::string err;
    k.load(k.file, err);
    check(rec.names.size() == 1 && rec.names[0] == k.file,
          std::string(k.name) + ": opened through IccJsonSetOpenFileIO()");
  }
  IccJsonSetOpenFileIO(&standard);
  IccJsonSetAllowFileImports(false);

  std::remove(kImport);
  std::remove(kClut);
  std::remove(kTint);

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  return g_failures ? 1 : 0;
}
