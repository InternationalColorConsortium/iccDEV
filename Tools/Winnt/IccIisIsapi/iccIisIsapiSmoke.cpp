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
#include <httpext.h>
#include <windows.h>

#include <cstdio>
#include <string>

namespace {

struct MockSession {
  std::string status;
  std::string headers;
  std::string body;
};

using GetExtensionVersionFn = BOOL (WINAPI *)(HSE_VERSION_INFO*);
using HttpExtensionProcFn = DWORD (WINAPI *)(LPEXTENSION_CONTROL_BLOCK);

BOOL WINAPI MockWriteClient(HCONN connId, LPVOID buffer, LPDWORD bytes, DWORD)
{
  if (!connId || !buffer || !bytes) {
    return FALSE;
  }

  auto* session = reinterpret_cast<MockSession*>(connId);
  session->body.append(reinterpret_cast<const char*>(buffer), *bytes);
  return TRUE;
}

BOOL WINAPI MockServerSupportFunction(HCONN connId,
                                      DWORD request,
                                      LPVOID buffer,
                                      LPDWORD,
                                      LPDWORD)
{
  if (!connId) {
    return FALSE;
  }

  auto* session = reinterpret_cast<MockSession*>(connId);
  if (request == HSE_REQ_SEND_RESPONSE_HEADER_EX) {
    auto* headerInfo = reinterpret_cast<HSE_SEND_HEADER_EX_INFO*>(buffer);
    if (!headerInfo) {
      return FALSE;
    }

    session->status.assign(headerInfo->pszStatus, headerInfo->cchStatus);
    session->headers.assign(headerInfo->pszHeader, headerInfo->cchHeader);
    return TRUE;
  }

  return TRUE;
}

int RunSmoke(const char* dllPath)
{
  HMODULE module = LoadLibraryA(dllPath);
  if (!module) {
    std::fprintf(stderr, "LoadLibrary failed for %s (error=%lu)\n", dllPath, GetLastError());
    return 1;
  }

  auto getVersion =
    reinterpret_cast<GetExtensionVersionFn>(GetProcAddress(module, "GetExtensionVersion"));
  auto httpProc =
    reinterpret_cast<HttpExtensionProcFn>(GetProcAddress(module, "HttpExtensionProc"));

  if (!getVersion || !httpProc) {
    std::fprintf(stderr, "Missing expected IIS exports in %s\n", dllPath);
    FreeLibrary(module);
    return 1;
  }

  HSE_VERSION_INFO versionInfo{};
  if (!getVersion(&versionInfo)) {
    std::fprintf(stderr, "GetExtensionVersion failed (error=%lu)\n", GetLastError());
    FreeLibrary(module);
    return 1;
  }

  MockSession session;
  EXTENSION_CONTROL_BLOCK ecb{};
  ecb.cbSize = sizeof(ecb);
  ecb.dwVersion = HSE_VERSION;
  ecb.ConnID = reinterpret_cast<HCONN>(&session);
  ecb.lpszMethod = const_cast<LPSTR>("GET");
  ecb.lpszQueryString = const_cast<LPSTR>("mode=health");
  ecb.ServerSupportFunction = MockServerSupportFunction;
  ecb.WriteClient = MockWriteClient;

  const DWORD status = httpProc(&ecb);
  std::printf("Description: %s\n", versionInfo.lpszExtensionDesc);
  std::printf("Status code: %lu\n", status);
  std::printf("HTTP status: %s\n", session.status.c_str());
  std::printf("Headers:\n%s", session.headers.c_str());
  std::printf("Body:\n%s", session.body.c_str());

  const bool ok = status == HSE_STATUS_SUCCESS &&
                  session.status == "200 OK" &&
                  session.body.find("ok") != std::string::npos;

  FreeLibrary(module);
  return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "Usage: %s <path-to-iccIisIsapi.dll>\n", argv[0]);
    return 2;
  }

  return RunSmoke(argv[1]);
}
