/*
    File:       IccFileUtil.h

    Contains:   Header-only console and regular-file output helpers

    Version:    V1
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

#ifndef ICC_FILE_UTIL_H
#define ICC_FILE_UTIL_H

#include <cstdio>
#include <string>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

// Decodes one UTF-8 sequence at p per RFC 3629, rejecting overlong forms, the
// surrogate range and anything above U+10FFFF.  Returns the sequence length and
// stores the codepoint, or 0 when p does not begin a well-formed sequence.  p is
// NUL-terminated, so a truncated sequence fails a continuation-byte test rather
// than reading past the end.
//
// Why this is not icConvertUTF8toUTF32() from IccConvertUTF.h.  Two reasons, the
// second measured:
//
//   1. That entry point is ICCPROFLIB_API, and this header is header-only by
//      design -- a sanitizer that every tool's error path calls should not drag
//      in a link dependency.
//   2. Its strictConversion mode is not strict enough for this use.  isLegalUTF8()
//      (IccConvertUTF.cpp:450) tests the second byte in an inner switch whose
//      0xED and 0xF4 arms check only an upper bound -- `a > 0x9F` and `a > 0x8F`
//      -- and so never reach the `default:` arm's lower-bound test `a < 0x80`.
//      Both leads therefore accept all 127 second bytes in 0x01..0x7F: ED 01 80
//      decodes as U+B040, and F4 01 80 80 as U+81000, which is not a codepoint at
//      all.  Compared exhaustively over every 1-3 byte sequence and every
//      F0..F5-led 4-byte sequence (116,134,905 in total), this function accepts
//      nothing that one rejects and agrees on every codepoint and length where
//      both accept; the converter accepts 528,320 sequences this one refuses.
//      Reusing it would let a malformed path be reported under a fabricated
//      codepoint -- still ASCII, so not an injection, but the wrong name for the
//      file.  The converter's own behaviour is a separate defect and is not
//      changed here.
inline int icDecodeUtf8(const unsigned char* p, unsigned int* pCp)
{
  unsigned char c0 = p[0];
  unsigned char lo, hi;
  unsigned int cp;
  int len, i;

  if (c0 < 0x80) {
    *pCp = c0;
    return 1;
  }

  if (c0 >= 0xc2 && c0 <= 0xdf) {
    // C0 and C1 are excluded above: they can only spell an overlong 2-byte form.
    len = 2; cp = c0 & 0x1fu; lo = 0x80; hi = 0xbf;
  }
  else if (c0 >= 0xe0 && c0 <= 0xef) {
    len = 3; cp = c0 & 0x0fu;
    // E0 A0..BF rejects the overlong 3-byte forms; ED 80..9F rejects
    // U+D800..U+DFFF, which UTF-8 must not encode.
    lo = (c0 == 0xe0) ? 0xa0 : 0x80;
    hi = (c0 == 0xed) ? 0x9f : 0xbf;
  }
  else if (c0 >= 0xf0 && c0 <= 0xf4) {
    len = 4; cp = c0 & 0x07u;
    // F0 90..BF rejects the overlong 4-byte forms; F4 80..8F caps at U+10FFFF.
    lo = (c0 == 0xf0) ? 0x90 : 0x80;
    hi = (c0 == 0xf4) ? 0x8f : 0xbf;
  }
  else {
    // A lone continuation byte (80..BF), an overlong lead (C0/C1), or F5..FF.
    return 0;
  }

  for (i = 1; i < len; i++) {
    // The tighter bound applies to the second byte only; the rest are 80..BF.
    unsigned char ch = p[i];
    if (ch < ((i == 1) ? lo : (unsigned char)0x80) ||
        ch > ((i == 1) ? hi : (unsigned char)0xbf))
      return 0;
    cp = (cp << 6) | (ch & 0x3fu);
  }

  *pCp = cp;
  return len;
}

inline std::string icSanitizeConsoleText(const char* szText)
{
  static const char hex[] = "0123456789ABCDEF";
  std::string result;

  if (!szText)
    return result;

  for (const unsigned char *p = (const unsigned char*)szText; *p; ) {
    unsigned char ch = *p;

    if (ch == '\n') { result += "\\n"; p++; continue; }
    if (ch == '\r') { result += "\\r"; p++; continue; }
    if (ch == '\t') { result += "\\t"; p++; continue; }

    if (ch >= 0x20 && ch < 0x7f) {
      result += (char)ch;
      p++;
      continue;
    }

    if (ch >= 0x80) {
      // #2420: escape non-ASCII by codepoint rather than per byte, so an
      // accented name reads as one U+00E9 escape instead of \xC3\xA9.  The whitelist
      // itself is unchanged -- every non-ASCII codepoint is still escaped,
      // including the C1 controls U+0080..U+009F, which UTF-8 spells as
      // C2 80..C2 9F and a terminal still acts on, and U+202E RIGHT-TO-LEFT
      // OVERRIDE (CVE-2021-42574).  Passing valid UTF-8 through would admit
      // both.  ASCII input is unaffected: only bytes >= 0x80 come through here.
      unsigned int cp = 0;
      int len = icDecodeUtf8(p, &cp);

      if (len > 1) {
        int digits = (cp > 0xffffu) ? 8 : 4;
        int shift;

        result += (digits == 8) ? "\\U" : "\\u";
        for (shift = (digits - 1) * 4; shift >= 0; shift -= 4)
          result += hex[(cp >> shift) & 0xfu];
        p += len;
        continue;
      }
      // Not well-formed UTF-8.  Fall through to the per-byte escape and advance
      // a single byte, so an invalid lead cannot consume the bytes after it.
    }

    result += "\\x";
    result += hex[(ch >> 4) & 0xf];
    result += hex[ch & 0xf];
    p++;
  }

  return result;
}

inline std::string icSanitizeConsoleText(const std::string& text)
{
  return icSanitizeConsoleText(text.c_str());
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

#endif
