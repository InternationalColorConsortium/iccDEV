/** @file
 *  File:       IccIsapiHttp.cpp
 *
 *  Contains:   HTTP response helpers with security headers for IIS ISAPI.
 *
 *  Version:    V1
 *
 *  Copyright:  (c) see ICC Software License
 *
 *  See IccIsapiHttp.h for design rationale and header list.
 */

/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "IccIsapiHttp.h"
#include "IccIsapiSanitize.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace iccIsapi {

bool SendResponse(LPEXTENSION_CONTROL_BLOCK ecb,
                  LPCSTR status,
                  LPCSTR contentType,
                  const std::string& body)
{
  if (!ecb || !ecb->ServerSupportFunction || !ecb->WriteClient) {
    return false;
  }

  std::string headers("Content-Type: ");
  headers += contentType;
  headers += "\r\nContent-Length: ";
  headers += std::to_string(body.size());
  headers += "\r\nCache-Control: no-store"
             "\r\nX-Content-Type-Options: nosniff"
             "\r\nX-Frame-Options: DENY"
             "\r\nContent-Security-Policy: default-src 'none'"
             "\r\nReferrer-Policy: no-referrer"
             "\r\nX-XSS-Protection: 1; mode=block"
             "\r\n\r\n";

  HSE_SEND_HEADER_EX_INFO headerInfo{};
  headerInfo.pszStatus = const_cast<LPSTR>(status);
  headerInfo.cchStatus = static_cast<DWORD>(std::strlen(status));
  headerInfo.pszHeader = const_cast<LPSTR>(headers.c_str());
  headerInfo.cchHeader = static_cast<DWORD>(headers.size());
  headerInfo.fKeepConn = FALSE;

  if (!ecb->ServerSupportFunction(ecb->ConnID,
                                  HSE_REQ_SEND_RESPONSE_HEADER_EX,
                                  &headerInfo,
                                  nullptr,
                                  nullptr)) {
    return false;
  }

  DWORD bytes = static_cast<DWORD>(body.size());
  return ecb->WriteClient(ecb->ConnID,
                          const_cast<char*>(body.data()),
                          &bytes,
                          0) != FALSE;
}

bool Send405(LPEXTENSION_CONTROL_BLOCK ecb,
             LPCSTR allowedMethod,
             const std::string& message)
{
  return SendResponse(ecb,
                      "405 Method Not Allowed",
                      "application/json; charset=utf-8",
                      std::string("{\"error\":\"") + JsonEscape(message) + "\"}");
}

bool Send400(LPEXTENSION_CONTROL_BLOCK ecb,
             const std::string& rawMessage)
{
  const std::string safe = SanitizeErrorMessage(rawMessage);
  return SendResponse(ecb,
                      "400 Bad Request",
                      "application/json; charset=utf-8",
                      std::string("{\"error\":\"") + JsonEscape(safe) + "\"}");
}

bool Send500(LPEXTENSION_CONTROL_BLOCK ecb)
{
  return SendResponse(ecb,
                      "500 Internal Server Error",
                      "application/json; charset=utf-8",
                      std::string("{\"error\":\"Internal server error.\"}"));
}

std::string GetLastErrorString(DWORD error)
{
  LPSTR message = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD length = FormatMessageA(flags,
                                      nullptr,
                                      error,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPSTR>(&message),
                                      0,
                                      nullptr);
  std::string text;
  if (length && message) {
    text.assign(message, length);
  }
  else {
    text = "Win32 error ";
    text += std::to_string(error);
  }

  if (message) {
    LocalFree(message);
  }

  while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) {
    text.pop_back();
  }

  return text;
}

std::vector<unsigned char> ReadRequestBody(LPEXTENSION_CONTROL_BLOCK ecb,
                                           size_t maxBytes)
{
  if (!ecb) {
    return {};
  }

  const DWORD totalBytes = std::max(ecb->cbTotalBytes, ecb->cbAvailable);

  // Reject oversized uploads BEFORE allocation — CWE-789 prevention.
  if (static_cast<size_t>(totalBytes) > maxBytes) {
    return {};
  }

  std::vector<unsigned char> body(totalBytes);
  DWORD copied = 0;

  if (totalBytes == 0) {
    return body;
  }

  if (ecb->cbAvailable && ecb->lpbData) {
    copied = std::min(ecb->cbAvailable, totalBytes);
    std::memcpy(body.data(), ecb->lpbData, copied);
  }

  while (copied < totalBytes) {
    DWORD chunk = totalBytes - copied;
    if (!ecb->ReadClient ||
        !ecb->ReadClient(ecb->ConnID, body.data() + copied, &chunk) ||
        chunk == 0) {
      body.resize(copied);
      return body;
    }
    copied += chunk;
  }

  return body;
}

}  // namespace iccIsapi
