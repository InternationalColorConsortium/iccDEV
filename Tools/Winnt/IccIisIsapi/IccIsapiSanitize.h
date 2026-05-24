/** @file
 *  File:       IccIsapiSanitize.h
 *
 *  Contains:   Defensive sanitization primitives for iccDEV IIS ISAPI.
 *
 *  Version:    V1
 *
 *  Copyright:  (c) see ICC Software License
 *
 *  Mirrors the API surface of .github/scripts/sanitize-sed.sh (V3) and
 *  .github/scripts/sanitize.ps1 (V1), adapted from shell to C++17.
 *
 *  Every function in this header is designed for untrusted input:
 *  - HTML entity encoding  (& < > " ')          - XSS prevention
 *  - JSON string encoding  (RFC 8259 + solidus)  - injection prevention
 *  - URL percent decoding  (null-byte rejection)  - truncation prevention
 *  - Control-char stripping (C0, DEL, ANSI ESC)   - log spoofing prevention
 *  - Filename sanitization (alphanumeric + ._-)    - path traversal prevention
 *  - Query string parsing  (safe tokenization)     - parameter injection prevention
 *
 *  Reference CWEs addressed:
 *    CWE-79   Cross-Site Scripting (XSS)
 *    CWE-116  Improper Encoding or Escaping of Output
 *    CWE-134  Use of Externally-Controlled Format String
 *    CWE-170  Improper Null Termination
 *    CWE-20   Improper Input Validation
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

#ifndef ICC_ISAPI_SANITIZE_H
#define ICC_ISAPI_SANITIZE_H

#include <string>

namespace iccIsapi {

/// Escape all 5 HTML-sensitive characters plus strip C0 control chars and DEL.
/// Mirrors: escape_html() in sanitize-sed.sh, Escape-Html in sanitize.ps1.
/// Entities: ampersand, less-than, greater-than, double quote, single quote.
/// Control chars 0x00-0x1F (except TAB 0x09 and LF 0x0A) and DEL 0x7F are
/// silently dropped to mirror _strip_ctrl_keep_newlines in sanitize-sed.sh.
std::string HtmlEscape(const std::string& value);

/// RFC 8259 JSON string escaping with solidus.
/// Escapes JSON quote, backslash, solidus, BS, FF, LF, CR, TAB, and all C0
/// controls as JSON Unicode escape sequences.
/// The solidus escape prevents closing-script injection when JSON is embedded
/// inside HTML script blocks.
std::string JsonEscape(const std::string& value);

/// Decode percent-encoded URL components (RFC 3986).
/// Rejects %00 (null byte) to prevent C-string truncation attacks (CWE-170).
/// Decodes '+' as space (application/x-www-form-urlencoded).
std::string UrlDecode(const std::string& value);

/// Extract a query-string parameter value by key.
/// Returns empty string if not found. Applies UrlDecode to the value.
std::string GetQueryValue(const char* query, const char* key);

/// Check if a query string contains an exact key=value pair.
/// No URL-decoding - expects literal match (safe for known enum values).
bool QueryHasValue(const char* query, const char* key, const char* value);

/// Produce a filename-safe string: only [A-Za-z0-9._-] are kept.
/// Leading dots/spaces are stripped to prevent hidden files.
/// Mirrors: sanitize_filename() in sanitize-sed.sh, Sanitize-Filename in sanitize.ps1.
std::string SanitizeFilename(const std::string& filename, const char* fallback);

/// Truncate text to maxBytes, appending a "[truncated N bytes]" notice.
std::string TruncateForBrowser(const std::string& text, size_t maxBytes = 65536);

/// Strip path prefixes from error messages to avoid leaking server paths.
/// Truncates to maxLen. Used before sending error text to HTTP clients.
std::string SanitizeErrorMessage(const std::string& message, size_t maxLen = 200);

/// Sanitize a URI by stripping the fragment (#...) and rejecting dangerous
/// schemes.  Only http, https, and relative paths are permitted.
/// Prevents DOM-XSS vectors like:
///   document.location.replace(location.hash.split("#")[1])
///   javascript:alert(1)
///   data:text/html,...
/// Also strips null bytes, newlines, and tabs that confuse URL parsers.
/// CWE-79, CWE-601 (Open Redirect).
std::string SanitizeUri(const std::string& uri);

}  // namespace iccIsapi

#endif  // ICC_ISAPI_SANITIZE_H
