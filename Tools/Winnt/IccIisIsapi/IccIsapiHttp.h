/** @file
 *  File:       IccIsapiHttp.h
 *
 *  Contains:   HTTP response helpers with security headers for IIS ISAPI.
 *
 *  Version:    V1
 *
 *  Copyright:  (c) see ICC Software License
 *
 *  This module centralizes all HTTP response generation so that security
 *  headers, Content-Length, and error formatting are applied uniformly.
 *
 *  Security headers applied to every response:
 *    Cache-Control: no-store             — prevent caching of dynamic content
 *    X-Content-Type-Options: nosniff     — prevent MIME-type sniffing (CWE-79)
 *    X-Frame-Options: DENY               — prevent clickjacking
 *    Content-Security-Policy             — restrict resource loading
 *    Referrer-Policy: no-referrer        — prevent referrer leakage
 *    Content-Length                       — explicit body size
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

#ifndef ICC_ISAPI_HTTP_H
#define ICC_ISAPI_HTTP_H

#include <httpext.h>
#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

namespace iccIsapi {

/// Send an HTTP response with hardened security headers.
/// All responses include: Cache-Control, X-Content-Type-Options, X-Frame-Options,
/// Content-Security-Policy, Referrer-Policy, X-XSS-Protection, and Content-Length.
bool SendResponse(LPEXTENSION_CONTROL_BLOCK ecb,
                  LPCSTR status,
                  LPCSTR contentType,
                  const std::string& body);

/// Send a 405 Method Not Allowed with JSON error body.
/// Includes Allow header indicating the permitted method.
bool Send405(LPEXTENSION_CONTROL_BLOCK ecb,
             LPCSTR allowedMethod,
             const std::string& message);

/// Send a 400 Bad Request with a sanitized JSON error body.
/// The message is stripped of path prefixes and truncated.
bool Send400(LPEXTENSION_CONTROL_BLOCK ecb,
             const std::string& rawMessage);

/// Send a 500 Internal Server Error with a generic JSON body.
/// Never exposes internal details to the client.
bool Send500(LPEXTENSION_CONTROL_BLOCK ecb);

/// Format a Win32 error code as a human-readable string.
std::string GetLastErrorString(DWORD error);

/// Check if the request method matches the expected method.
inline bool IsMethod(LPEXTENSION_CONTROL_BLOCK ecb, const char* method)
{
  return ecb && ecb->lpszMethod && std::strcmp(ecb->lpszMethod, method) == 0;
}

/// Read the full request body, capped at maxBytes.
/// Returns empty vector if maxBytes is exceeded BEFORE allocation (CWE-789).
std::vector<unsigned char> ReadRequestBody(LPEXTENSION_CONTROL_BLOCK ecb,
                                           size_t maxBytes);

}  // namespace iccIsapi

#endif  // ICC_ISAPI_HTTP_H
