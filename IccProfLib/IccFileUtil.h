/*
    File:       IccFileUtil.h

    Contains:   Header-only console and regular-file output helpers

    Version:    V1
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
      if (ch < 0x20 || ch >= 0x7f) {
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
