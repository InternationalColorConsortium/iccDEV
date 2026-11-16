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
// hook. #3081 adds reserved-device refusal before that hook and per-file and
// cumulative calculator import limits, including unknown-length streams.
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
#include <cstring>
#include <map>
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

// No test involving a reserved name ever reaches the operating system, even
// when the production path check regresses (CON could otherwise block CTest).
class NameRecorder : public IIccJsonOpenFileIO
{
public:
  std::vector<std::string> names;
  CIccIO *OpenFile(const icChar *name, const char *) override
  {
    names.push_back(name);
    return NULL;
  }
};

struct GeneratedFile {
  std::string prefix;
  size_t length;
  size_t reportedLength;
  size_t bytesRead = 0;
  bool closed = false;
};

// Large inputs are generated a chunk at a time. JSON whitespace exercises the
// actual read budget without allocating a similarly large fixture or DOM.
class GeneratedIO : public CIccIO
{
public:
  explicit GeneratedIO(GeneratedFile &file) : m_file(file), m_pos(0) {}
  ~GeneratedIO() override { m_file.closed = true; }
  size_t GetLength() override { return m_file.reportedLength; }
  size_t Read8(void *buf, size_t count = 1) override
  {
    size_t available = m_file.length - m_pos;
    if (count > available)
      count = available;
    std::memset(buf, ' ', count);
    if (m_pos < m_file.prefix.size()) {
      size_t prefixBytes = m_file.prefix.size() - m_pos;
      if (prefixBytes > count)
        prefixBytes = count;
      std::memcpy(buf, m_file.prefix.data() + m_pos, prefixBytes);
    }
    m_pos += count;
    m_file.bytesRead += count;
    return count;
  }
private:
  GeneratedFile &m_file;
  size_t m_pos;
};

class GeneratedOpener : public IIccJsonOpenFileIO
{
public:
  std::map<std::string, GeneratedFile> files;
  CIccIO *OpenFile(const icChar *name, const char *) override
  {
    auto file = files.find(name);
    return file == files.end() ? NULL : new GeneratedIO(file->second);
  }
};

IccJson imports(const std::vector<std::string> &names)
{
  IccJson j = { { "imports", IccJson::array() } };
  for (const std::string &name : names)
    j["imports"].push_back({ { "file", name } });
  return j;
}

void checkNames()
{
  static NameRecorder recorder;
  IccJsonSetOpenFileIO(&recorder);
  const std::string unsafe[] = {
    "CON", "con.json", "PrN", "aux.txt", "NUL.", "nul ",
    "COM1", "com9.json", "LPT1", "lpt9.txt", "CONIN$", "conout$",
    "CON .json", "nul.txt ", "LPT2. ",
    "COM\xc2\xb9", "LPT\xc2\xb2.json", "COM\xc2\xb3.txt",
    "COM\xb9", "LPT\xb2.json", "COM\xb3.txt",
    std::string("safe\0.json", 10)
  };
  for (const Kind &k : kKinds) {
    for (const std::string &name : unsafe) {
      recorder.names.clear();
      expect(k, name, false, "Unsafe", "reserved or NUL-containing name refused");
      check(recorder.names.empty(), std::string(k.name) + ": refused before host hook");
    }
  }
  for (const char *name : { "console.json", "NULfile", "COM0", "COM10", "LPT10", "auxiliary" }) {
    recorder.names.clear();
    std::string err;
    CIccIO *io = IccJsonOpenImportFile(name, "test", err);
    check(!io && recorder.names.size() == 1 && recorder.names[0] == name,
          std::string("ordinary name reaches host hook: ") + name);
    delete io;
  }
}

void checkBudgets()
{
  static GeneratedOpener opener;
  IccJsonSetOpenFileIO(&opener);
  const size_t limit = IccJsonMaxImportBytes();
  opener.files["large.json"] = { "{}", limit + 1, limit + 1 };
  opener.files["exact.json"] = { "{}", limit, limit };
  std::string err;
  CIccIO *io = IccJsonOpenImportFile("large.json", "test", err);
  check(!io && err.find("too large") != std::string::npos &&
        opener.files["large.json"].bytesRead == 0 && opener.files["large.json"].closed,
        "per-file bound refuses limit + 1 before reading and closes IO");
  delete io;
  err.clear();
  io = IccJsonOpenImportFile("exact.json", "test", err);
  check(io != NULL && err.empty(), "per-file bound accepts exactly the configured limit");
  delete io;

  CIccMpeJsonCalculator calc;
  opener.files["small.json"] = { "{}", 2, 2 };
  err.clear();
  check(!calc.ParseImport(imports({ "small.json", "exact.json" }), "*", err) &&
        err.find("byte budget exceeded") != std::string::npos &&
        opener.files["exact.json"].bytesRead == 0,
        "sibling imports share a byte budget and reject known length before reading");

  std::string parent = imports({ "exact.json" }).dump();
  opener.files["parent.json"] = { parent, parent.size(), parent.size() };
  err.clear();
  check(!calc.ParseImport(imports({ "parent.json" }), "*", err) &&
        err.find("byte budget exceeded") != std::string::npos,
        "nested imports share their parent's byte budget");

  const size_t half = limit / 2 + 1;
  opener.files["stream-a.json"] = { "{}", half, 0 };
  err.clear();
  check(!calc.ParseImport(imports({ "stream-a.json", "stream-a.json" }), "*", err) &&
        err.find("byte budget exceeded") != std::string::npos &&
        opener.files["stream-a.json"].bytesRead > half &&
        opener.files["stream-a.json"].closed,
        "unknown-length repeated siblings count every actual read toward the budget");

  opener.files["half-a.json"] = { "{}", limit / 2, limit / 2 };
  opener.files["half-b.json"] = { "{}", limit - limit / 2, limit - limit / 2 };
  err.clear();
  check(calc.ParseImport(imports({ "half-a.json", "half-b.json" }), "*", err) &&
        opener.files["half-a.json"].bytesRead + opener.files["half-b.json"].bytesRead == limit,
        "sibling imports accept exactly the cumulative byte boundary");

  err.clear();
  check(calc.ParseImport(imports({ "small.json", "small.json" }), "*", err),
        "a new parse resets the budget after failure and permits ordinary siblings");
  err.clear();
  check(calc.ParseImport(imports({ "small.json" }), "*", err),
        "a new parse resets the budget after success");
  parent = imports({ "small.json" }).dump();
  opener.files["parent.json"] = { parent, parent.size(), parent.size() };
  err.clear();
  check(calc.ParseImport(imports({ "parent.json", "small.json" }), "*", err),
        "ordinary nested and sibling imports remain valid");
}

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
  checkNames();
  checkBudgets();
  IccJsonSetOpenFileIO(&standard);
  IccJsonSetAllowFileImports(false);

  std::remove(kImport);
  std::remove(kClut);
  std::remove(kTint);

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  return g_failures ? 1 : 0;
}
