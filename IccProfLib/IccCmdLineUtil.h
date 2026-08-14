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
#include <string>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
// The body mirrors the WritePdfTextFile() copies in MiniPDF.cpp, which already
// do exactly this correctly; those are two more members of the duplicated
// open/close helper family #2154 tracks, and they can collapse onto this one.
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
