/*
    File:       IccSameFile.h

    Contains:   Whether an output path names the same file as an input path

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

#ifndef ICC_SAME_FILE_H
#define ICC_SAME_FILE_H

// Kept apart from IccCmdLineUtil.h on purpose: the Windows identity lookup needs
// <windows.h>, and only the tools that refuse to overwrite an input should pay
// for it (#2692).

#include <cstdio>
#include "IccFileUtil.h"

#if defined(_WIN32)
// stat() leaves st_ino zero on Windows, so it cannot compare file identity
// there.  NOMINMAX and WIN32_LEAN_AND_MEAN as in
// Tools/CmdLine/IccApplyProfiles/TiffImg.cpp.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// Whether two paths name the same existing file (#2692).
//
// A tool that reads one path and writes another destroys its input when the two
// are the same file: iccApplyProfiles and iccSpecSepToTiff truncate the TIFF they
// are still reading and fault inside libtiff, and the dump tools replace the image
// with the profile they extracted from it and exit 0.
//
// The test is file IDENTITY, not spelling.  "./a.tif" and "a.tif", a hard link to
// the input, and a symlink to it all name the same file, and a string compare
// misses every one of them.  POSIX compares st_dev and st_ino; stat() follows a
// symlink, so a link is judged by its target.  Windows compares the volume serial
// number and file index, the identity GetFileInformationByHandle() reports;
// CreateFile() follows reparse points unless told not to, so the same holds.
//
// A path that does not exist yet -- the usual state of an output -- cannot be the
// input, so it answers false.  So does a path that cannot be examined: this guards
// against clobbering, and the open that follows reports any real error.
inline bool icIsSameFile(const char* szA, const char* szB)
{
  if (!szA || !szA[0] || !szB || !szB[0])
    return false;

#if defined(_WIN32)
  HANDLE hA = CreateFileA(szA, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hA == INVALID_HANDLE_VALUE)
    return false;

  HANDLE hB = CreateFileA(szB, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (hB == INVALID_HANDLE_VALUE) {
    CloseHandle(hA);
    return false;
  }

  BY_HANDLE_FILE_INFORMATION infoA, infoB;
  bool bSame = GetFileInformationByHandle(hA, &infoA) &&
               GetFileInformationByHandle(hB, &infoB) &&
               infoA.dwVolumeSerialNumber == infoB.dwVolumeSerialNumber &&
               infoA.nFileIndexHigh == infoB.nFileIndexHigh &&
               infoA.nFileIndexLow == infoB.nFileIndexLow;

  CloseHandle(hB);
  CloseHandle(hA);
  return bSame;
#else
  struct stat stA, stB;
  if (stat(szA, &stA) != 0 || stat(szB, &stB) != 0)
    return false;

  return stA.st_dev == stB.st_dev && stA.st_ino == stB.st_ino;
#endif
}

// When szOutput names the same file as szInput, say so on stderr and return true,
// so the caller refuses BEFORE it opens -- and truncates -- the output.  One message
// for every tool, with both paths sanitised for the console as #2414 requires of
// any echoed operand.
inline bool icOutputIsInput(const char* szOutput, const char* szInput)
{
  if (!icIsSameFile(szOutput, szInput))
    return false;

  fprintf(stderr, "Error! - Output '%s' is the same file as input '%s'; "
                  "refusing to overwrite the input.\n",
          icSanitizeConsoleText(szOutput).c_str(),
          icSanitizeConsoleText(szInput).c_str());
  return true;
}

#endif
