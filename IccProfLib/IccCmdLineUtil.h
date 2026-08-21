/*
    File:       IccCmdLineUtil.h

    Contains:   Shared helpers for command line tools

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2010 The International Color Consortium. All rights
 * reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

#ifndef ICC_CMD_LINE_UTIL_H
#define ICC_CMD_LINE_UTIL_H

#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(__clang__)
#define ICC_CMDLINE_NO_SANITIZE_INTEGER __attribute__((no_sanitize("integer")))
#else
#define ICC_CMDLINE_NO_SANITIZE_INTEGER
#endif

/******************************************************************************/

inline std::string icSanitizeConsoleText(const char* szText)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string result;

  if (!szText)
    return result;

  for (const unsigned char *p = (const unsigned char*)szText; *p; p++) {
    unsigned char ch = *p;

    switch (ch) {
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (ch < 0x20 || ch >= 0x7f) {  // && <= 0xFF implied by data type
        result += "\\x";
        result += hex[(ch >> 4) & 0xf];
        result += hex[ch & 0xf];
      }
      else {
        result += (char)ch;
      }
      break;
    }
  }

  return result;
}

inline std::string icSanitizeConsoleText(const std::string& text)
{
  return icSanitizeConsoleText(text.c_str());
}

/******************************************************************************/

// we want to preserve CRLF and tab, but not control characters
inline std::string icSanitizeTagText(const char* szText)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string result;

  if (!szText)
    return result;

  for (const unsigned char *p = (const unsigned char*)szText; *p; p++) {
    unsigned char ch = *p;

    switch (ch) {
    case '\n':
    case '\r':
    case '\t':
      result += (char)ch;
      break;
    default:
      if (ch < 0x20 || ch >= 0x7f) {  // && <= 0xFF implied by data type
        result += "\\x";
        result += hex[(ch >> 4) & 0xf];
        result += hex[ch & 0xf];
      }
      else {
        result += (char)ch;
      }
      break;
    }
  }

  return result;
}

inline std::string icSanitizeTagText(const std::string& text)
{
  return icSanitizeTagText(text.c_str());
}

/******************************************************************************/

// NOTE - we cannot filter out path characters /\ without breaking output
// But we can remove non-ASCII and high ASCII that cause problems
inline std::string icSanitizeFileName(const char* szText)
{
  std::string result;

  if (!szText)
    return result;

  for (const unsigned char *p = (const unsigned char*)szText; *p; p++) {
    unsigned char ch = *p;

    switch (ch) {
    case '\n':
      result += "N";
      break;
    case '\r':
      result += "R";
      break;
    case '\t':
      result += "T";
      break;
    default:
      if (ch < 0x20 || ch >= 0x7f) {  // && <= 0xFF implied by data type
        result += "_";
      }
      else {
        result += (char)ch;
      }
      break;
    }
  }

  return result;
}

inline std::string icSanitizeFileName(const std::string& text)
{
  return icSanitizeFileName(text.c_str());
}

/******************************************************************************/

inline std::string icJsonEscape(const char* szText)
{
  static const char hex[] = "0123456789abcdef";
  std::string result;

  if (!szText)
    return result;

  for (const unsigned char *p = (const unsigned char*)szText; *p; p++) {
    unsigned char ch = *p;
    switch (ch) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (ch < 0x20) {
        result += "\\u00";
        result += hex[(ch >> 4) & 0xf];
        result += hex[ch & 0xf];
      }
      else {
        result += (char)ch;
      }
      break;
    }
  }

  return result;
}

inline std::string icJsonEscape(const std::string& text)
{
  return icJsonEscape(text.c_str());
}

inline ICC_CMDLINE_NO_SANITIZE_INTEGER unsigned int icSha256Rotr(unsigned int x, unsigned int n)
{
  return (x >> n) | (x << (32 - n));
}

inline ICC_CMDLINE_NO_SANITIZE_INTEGER std::string icSha256Bytes(const unsigned char* data, size_t len)
{
  static const unsigned int k[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
  };
  unsigned int h[8] = {
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
  };
  std::vector<unsigned char> msg(data, data + len);
  unsigned long long bitLen = (unsigned long long)len * 8ULL;
  msg.push_back(0x80);
  while ((msg.size() % 64) != 56)
    msg.push_back(0);
  for (int i = 7; i >= 0; i--)
    msg.push_back((unsigned char)((bitLen >> (i * 8)) & 0xff));

  for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
    unsigned int w[64];
    for (int i = 0; i < 16; i++) {
      size_t j = chunk + (size_t)i * 4;
      w[i] = ((unsigned int)msg[j] << 24) |
             ((unsigned int)msg[j + 1] << 16) |
             ((unsigned int)msg[j + 2] << 8) |
             ((unsigned int)msg[j + 3]);
    }
    for (int i = 16; i < 64; i++) {
      unsigned int s0 = icSha256Rotr(w[i - 15], 7) ^
                        icSha256Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      unsigned int s1 = icSha256Rotr(w[i - 2], 17) ^
                        icSha256Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    unsigned int a = h[0], b = h[1], c = h[2], d = h[3];
    unsigned int e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      unsigned int s1 = icSha256Rotr(e, 6) ^ icSha256Rotr(e, 11) ^
                        icSha256Rotr(e, 25);
      unsigned int ch = (e & f) ^ ((~e) & g);
      unsigned int temp1 = hh + s1 + ch + k[i] + w[i];
      unsigned int s0 = icSha256Rotr(a, 2) ^ icSha256Rotr(a, 13) ^
                        icSha256Rotr(a, 22);
      unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
      unsigned int temp2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  std::ostringstream out;
  out << "sha256:";
  for (int i = 0; i < 8; i++)
    out << std::hex << std::setfill('0') << std::setw(8) << h[i];
  return out.str();
}

inline bool icReadFileBytes(const char* path, std::vector<unsigned char>& bytes)
{
  if (!path || !path[0])
    return false;

  FILE* f = fopen(path, "rb");
  if (!f)
    return false;

  bytes.clear();
  unsigned char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    bytes.insert(bytes.end(), buf, buf + n);

  bool ok = ferror(f) == 0;
  fclose(f);
  return ok;
}

inline bool icSha256File(const char* path, std::string& digest)
{
  std::vector<unsigned char> bytes;
  if (!icReadFileBytes(path, bytes))
    return false;

  digest = icSha256Bytes(bytes.empty() ? (const unsigned char*)"" : &bytes[0],
                         bytes.size());
  return true;
}

inline FILE* icOpenRegularWriteFile(const char* szFname, const char* szMode)
{
  if (!szFname || !szFname[0])
    return stdout;

#if defined(_WIN32)
  return fopen(szFname, szMode);
#else
  struct stat st;
  if (stat(szFname, &st) == 0 && !S_ISREG(st.st_mode))
    return NULL;

  int fd = open(szFname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd < 0)
    return NULL;

  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    return NULL;
  }

  FILE* f = fdopen(fd, szMode);
  if (!f)
    close(fd);

  return f;
#endif
}

inline FILE* icOpenRegularWriteBinaryFile(const char* szFname)
{
  return icOpenRegularWriteFile(szFname, "wb");
}

inline FILE* icOpenRegularWriteTextFile(const char* szFname)
{
  return icOpenRegularWriteFile(szFname, "wt");
}

/******************************************************************************/

inline bool icFlushAndClose(FILE* f)
{
  if (!f)
    return false;

  bool failed = (fflush(f) != 0) || (ferror(f) != 0);

  if (f == stdout)
    return !failed;

  if (fclose(f) != 0)
    failed = true;

  return !failed;
}

/******************************************************************************/

// Write a whole document through an ALREADY-VALIDATED handle, then close it.
//
// Why this exists (#2154): icOpenRegularWriteFile() above earns its guarantee by
// fstat()ing the descriptor it actually opened, not the path it was given. That
// guarantee lives in the returned FILE* and nowhere else -- so a caller that
// closes the handle and then reopens the same *path* through another stream has
// thrown the check away and reintroduced the race it just paid for. Passing the
// handle straight to this function is what keeps the validated object and the
// written object the same object.
//
// The body came from the WritePdfTextFile() copies in MiniPDF.cpp, which already
// did exactly this correctly. Both of those were members of the duplicated
// open/close helper family #2154 tracks, and both now call this function instead.
inline bool icWriteAndClose(FILE* f, const std::string& text)
{
  bool failed = false;

  if (!f)
    return false;

  if (!text.empty() && fwrite(text.data(), 1, text.size(), f) != text.size())
    failed = true;

  // icFlushAndClose() reports the deferred write errors that fwrite() can hide
  // behind a full stdio buffer, so its verdict is part of the result.
  if (!icFlushAndClose(f))
    failed = true;

  return !failed;
}

/******************************************************************************/

#endif
