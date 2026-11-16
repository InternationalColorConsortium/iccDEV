/** @file
File:       IccIoJson.cpp

Contains:   Pluggable file-IO interface for IccJSON (mirrors IccIoXml.cpp)

Version:    V1

Copyright:  (c) see Software License
*/

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
#include "IccIoJson.h"
#include "IccJsonConfig.h"
#include "IccFileUtil.h"
#include <new>

// Issue #1856: the build sets this from the CMake cache variable
// ICC_JSON_MAX_FILE_MB, as for IccProfileJson.cpp; the fallback keeps other
// builds finite.
#ifndef ICC_JSON_MAX_FILE_BYTES
#define ICC_JSON_MAX_FILE_BYTES (128ULL * 1024 * 1024)
#endif

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccIO *CIccJsonStandardFileIO::OpenFile(const icChar *szFilename, const char *szAttr)
{
  CIccFileIO *file = new (std::nothrow) CIccFileIO();
  if (file && !file->Open(szFilename, szAttr)) {
    delete file;
    return nullptr;
  }
  return file;
}

static CIccJsonStandardFileIO g_IccJsonStandardFileIO;
static IIccJsonOpenFileIO    *g_pIccJsonFileIO = &g_IccJsonStandardFileIO;

CIccIO *IccJsonOpenFileIO(const icChar *szFilename, const char *szAttr)
{
  if (g_pIccJsonFileIO)
    return g_pIccJsonFileIO->OpenFile(szFilename, szAttr);
  return nullptr;
}

void IccJsonSetOpenFileIO(IIccJsonOpenFileIO *pOpenIO)
{
  g_pIccJsonFileIO = pOpenIO;
}

static bool IsImportDeviceName(const std::string &filename)
{
  // Win32 resolves DOS device names even with an extension or trailing spaces.
  // Apply the same rule on every host, before a custom opener sees the name.
  std::string stem = filename.substr(0, filename.find('.'));
  while (!stem.empty() && stem.back() == ' ')
    stem.pop_back();
  for (char &c : stem) {
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 'a' + 'A');
  }
  if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
      stem == "CONIN$" || stem == "CONOUT$")
    return true;
  if (stem.compare(0, 3, "COM") && stem.compare(0, 3, "LPT"))
    return false;

  std::string digit = stem.substr(3);
  // Windows also treats ISO-8859-1 superscript 1, 2 and 3 as device digits.
  // Accept both narrow-code-page and UTF-8 spellings for this rejection.
  return (digit.size() == 1 && digit[0] >= '1' && digit[0] <= '9') ||
         digit == "\xB9" || digit == "\xB2" || digit == "\xB3" ||
         digit == "\xC2\xB9" || digit == "\xC2\xB2" || digit == "\xC2\xB3";
}

CIccIO *IccJsonOpenImportFile(const std::string &filename, const char *szWhat,
                              std::string &parseStr)
{
  // The JSON is attacker-controlled in library, service and WASM use, so file
  // loads are opt-in (IccJsonConfig.h).
  if (!g_IccJsonAllowFileImports) {
    parseStr += std::string(szWhat) + " file imports are disabled; '" +
                icSanitizeConsoleText(filename) + "' not read\n";
    return nullptr;
  }
  // ':' covers a Windows drive-relative name ("D:file") and an alternate data
  // stream ("file:stream").
  if (filename.empty() ||
      filename.find('/')  != std::string::npos ||
      filename.find('\\') != std::string::npos ||
      filename.find(':')  != std::string::npos ||
      filename.find('\0') != std::string::npos ||
      filename.find("..") != std::string::npos ||
      IsImportDeviceName(filename)) {
    parseStr += std::string("Unsafe ") + szWhat + " file path '" +
                icSanitizeConsoleText(filename) + "'\n";
    return nullptr;
  }
  CIccIO *pIO = IccJsonOpenFileIO(filename.c_str(), "rb");
  if (!pIO) {
    parseStr += std::string("Cannot open ") + szWhat + " file '" +
                icSanitizeConsoleText(filename) + "'\n";
    return nullptr;
  }
  if ((icUInt64Number)pIO->GetLength() > (icUInt64Number)IccJsonMaxImportBytes()) {
    delete pIO;
    parseStr += std::string(szWhat) + " file '" + icSanitizeConsoleText(filename) +
                "' is too large\n";
    return nullptr;
  }
  return pIO;
}

size_t IccJsonMaxImportBytes()
{
  return (size_t)ICC_JSON_MAX_FILE_BYTES;
}

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif
