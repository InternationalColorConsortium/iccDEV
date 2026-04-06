/** @file
 *  File:       IccIsapiSanitize.cpp
 *
 *  Contains:   Defensive sanitization primitives for iccDEV IIS ISAPI.
 *
 *  Version:    V1
 *
 *  Copyright:  (c) see ICC Software License
 *
 *  See IccIsapiSanitize.h for design rationale and CWE references.
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

#include "IccIsapiSanitize.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace iccIsapi {

std::string HtmlEscape(const std::string& value)
{
  std::string escaped;
  escaped.reserve(value.size() + 16);

  for (unsigned char ch : value) {
    switch (ch) {
    case '&':
      escaped += "&amp;";
      break;
    case '<':
      escaped += "&lt;";
      break;
    case '>':
      escaped += "&gt;";
      break;
    case '\"':
      escaped += "&quot;";
      break;
    case '\'':
      escaped += "&#39;";
      break;
    default:
      // Strip C0 control chars except TAB (0x09) and LF (0x0A).
      // Mirrors _strip_ctrl_keep_newlines in sanitize-sed.sh.
      if (ch < 0x20 && ch != '\t' && ch != '\n') {
        break;
      }
      // Strip DEL (0x7F).
      if (ch == 0x7F) {
        break;
      }
      escaped.push_back(static_cast<char>(ch));
      break;
    }
  }

  return escaped;
}

std::string JsonEscape(const std::string& value)
{
  std::string escaped;
  escaped.reserve(value.size() + 16);

  for (unsigned char ch : value) {
    switch (ch) {
    case '\"':
      escaped += "\\\"";
      break;
    case '\\':
      escaped += "\\\\";
      break;
    case '/':
      // Prevents </script> injection in HTML-embedded JSON.
      escaped += "\\/";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (ch < 0x20) {
        char buffer[8]{};
        std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
        escaped += buffer;
      }
      else {
        escaped.push_back(static_cast<char>(ch));
      }
      break;
    }
  }

  return escaped;
}

std::string UrlDecode(const std::string& value)
{
  std::string decoded;
  decoded.reserve(value.size());

  size_t i = 0;
  while (i < value.size()) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const char hi = value[i + 1];
      const char lo = value[i + 2];
      if (std::isxdigit(static_cast<unsigned char>(hi)) &&
          std::isxdigit(static_cast<unsigned char>(lo))) {
        // Manual hex conversion avoids std::stoi which can throw
        // std::invalid_argument/std::out_of_range (CWE-248 DoS).
        auto hexVal = [](char c) -> int {
          if (c >= '0' && c <= '9') return c - '0';
          if (c >= 'A' && c <= 'F') return c - 'A' + 10;
          if (c >= 'a' && c <= 'f') return c - 'a' + 10;
          return -1;
        };
        const int hex = (hexVal(hi) << 4) | hexVal(lo);
        i += 3;
        // Reject null bytes — CWE-170 string truncation.
        if (hex == 0) {
          continue;
        }
        decoded.push_back(static_cast<char>(hex));
        continue;
      }
    }

    decoded.push_back(value[i] == '+' ? ' ' : value[i]);
    ++i;
  }

  // Strip any raw null bytes that may appear in malformed input.
  // UrlDecode already rejects %00, but raw 0x00 can appear in binary
  // data submitted via query strings or form fields.
  decoded.erase(std::remove(decoded.begin(), decoded.end(), '\0'), decoded.end());

  return decoded;
}

std::string GetQueryValue(const char* query, const char* key)
{
  if (!query || !*query || !key || !*key) {
    return std::string();
  }

  const std::string target(key);
  const char* cursor = query;

  while (*cursor) {
    while (*cursor == '&') {
      ++cursor;
    }

    const char* tokenEnd = std::strchr(cursor, '&');
    const size_t tokenLen = tokenEnd ? static_cast<size_t>(tokenEnd - cursor) : std::strlen(cursor);
    const std::string token(cursor, tokenLen);
    const size_t equalsPos = token.find('=');
    const std::string currentKey = token.substr(0, equalsPos);

    if (currentKey == target) {
      if (equalsPos == std::string::npos) {
        return std::string();
      }
      return UrlDecode(token.substr(equalsPos + 1));
    }

    if (!tokenEnd) {
      break;
    }
    cursor = tokenEnd + 1;
  }

  return std::string();
}

bool QueryHasValue(const char* query, const char* key, const char* value)
{
  if (!query || !*query || !key || !*key || !value) {
    return false;
  }

  std::string needle(key);
  needle += '=';
  needle += value;

  const char* cursor = query;
  while (*cursor) {
    while (*cursor == '&') {
      ++cursor;
    }

    const char* tokenEnd = std::strchr(cursor, '&');
    const size_t tokenLen = tokenEnd ? static_cast<size_t>(tokenEnd - cursor) : std::strlen(cursor);
    if (tokenLen == needle.size() && std::strncmp(cursor, needle.c_str(), tokenLen) == 0) {
      return true;
    }

    if (!tokenEnd) {
      break;
    }
    cursor = tokenEnd + 1;
  }

  return false;
}

std::string SanitizeFilename(const std::string& filename, const char* fallback)
{
  std::string safe = filename;
  if (safe.empty()) {
    safe = fallback;
  }

  // Keep only alphanumeric, dot, hyphen, underscore.
  // Mirrors sanitize_filename() in sanitize-sed.sh.
  std::replace_if(safe.begin(),
                  safe.end(),
                  [](unsigned char ch) {
                    return !(std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_');
                  },
                  '_');

  // Strip leading dots/spaces — prevent hidden files and path tricks.
  while (!safe.empty() && (safe.front() == '.' || safe.front() == ' ')) {
    safe.erase(safe.begin());
  }

  if (safe.empty()) {
    safe = fallback;
  }

  return safe;
}

std::string TruncateForBrowser(const std::string& text, size_t maxBytes)
{
  if (text.size() <= maxBytes) {
    return text;
  }

  std::string truncated = text.substr(0, maxBytes);
  truncated += "\n\n[truncated ";
  truncated += std::to_string(text.size() - maxBytes);
  truncated += " bytes]";
  return truncated;
}

std::string SanitizeErrorMessage(const std::string& message, size_t maxLen)
{
  // Strip any content that looks like a filesystem path to avoid leaking
  // internal directory layout (CWE-209).  Walk the string and replace
  // sequences that start with a drive letter or leading slash.
  std::string safe;
  safe.reserve(message.size());
  size_t i = 0;
  while (i < message.size()) {
    // Detect Windows drive path: C:\ or C:/
    if (i + 2 < message.size() && std::isalpha(static_cast<unsigned char>(message[i]))
        && message[i + 1] == ':' && (message[i + 2] == '\\' || message[i + 2] == '/')) {
      safe += "[path]";
      while (i < message.size() && message[i] != ' ' && message[i] != '"'
             && message[i] != '\'' && message[i] != ':' && message[i] != '\n')
        ++i;
      // Also skip trailing colon from "path: error" patterns
      continue;
    }
    // Detect Unix-style absolute path: /something
    if (message[i] == '/' && i + 1 < message.size()
        && std::isalpha(static_cast<unsigned char>(message[i + 1]))) {
      safe += "[path]";
      while (i < message.size() && message[i] != ' ' && message[i] != '"'
             && message[i] != '\'' && message[i] != ':' && message[i] != '\n')
        ++i;
      continue;
    }
    safe.push_back(message[i]);
    ++i;
  }

  if (safe.size() > maxLen) {
    safe = safe.substr(0, maxLen);
  }

  return safe;
}

std::string SanitizeUri(const std::string& uri)
{
  if (uri.empty()) {
    return std::string();
  }

  // Strip characters that confuse URL parsers: null, newline, tab.
  // Attackers use these to bypass scheme checks and inject headers.
  std::string clean;
  clean.reserve(uri.size());
  for (unsigned char ch : uri) {
    if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\t') {
      continue;
    }
    clean.push_back(static_cast<char>(ch));
  }

  // Strip fragment (#...) — fragment data is never sent to the server but
  // can be used for DOM-XSS if reflected into client-side code.
  const size_t hashPos = clean.find('#');
  if (hashPos != std::string::npos) {
    clean = clean.substr(0, hashPos);
  }

  // Block dangerous URI schemes.  Only allow relative paths, http, and https.
  // Attackers inject javascript:, data:, vbscript:, etc. through parameters
  // that end up in href/src attributes or location assignments.
  //
  // Classic vector: ?redirect=javascript:alert(document.cookie)
  // Parser-confusing variant from the user's DOM XSS research:
  //   ?q=<a"'\x0A`= +%20>;path/hostname?url=https://attacker:8889
  if (clean.find(':') != std::string::npos) {
    // Has a colon — check if it's a scheme prefix.
    // Normalize to lowercase for scheme comparison.
    std::string lower = clean;
    for (auto& c : lower) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Relative paths start with ., /, or have no scheme at all.
    const bool isRelative = clean[0] == '.' || clean[0] == '/';
    const bool isHttp = lower.rfind("http://", 0) == 0;
    const bool isHttps = lower.rfind("https://", 0) == 0;

    if (!isRelative && !isHttp && !isHttps) {
      return std::string();
    }
  }

  return clean;
}

}  // namespace iccIsapi
