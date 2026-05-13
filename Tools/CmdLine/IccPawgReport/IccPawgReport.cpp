/** @file
    File:       IccPawgReport.cpp

    Contains:   Console app to report ICC PAWG profile assessment checks

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights
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

#include <cstdio>
#include <cstring>

#include "IccProfLibVer.h"
#include "PawgReport.h"

static void printUsage()
{
  printf("Usage: iccPawgReport {--read} {--json} profile\n");
  printf("\nEmits an ICC PAWG profile assessment checklist report aligned with\n");
  printf("https://www.color.org/profiles/assessment/index.xalter\n");
  printf("  --read   Use ReadIccProfile (eager load) fallback after validation parse failure\n");
  printf("  --json   Emit machine-readable JSON evidence instead of text\n");
  printf("iccPawgReport built with IccProfLib version " ICCPROFLIBVER "\n\n");
}

int main(int argc, char *argv[])
{
  bool bUseRead = false;
  bool bJson = false;

  for (int k = 1; k < argc; k++) {
    if (strcmp(argv[k], "--read") == 0) {
      bUseRead = true;
      for (int m = k; m < argc - 1; m++)
        argv[m] = argv[m + 1];
      argc--;
      k--;
    }
    else if (strcmp(argv[k], "--json") == 0) {
      bJson = true;
      for (int m = k; m < argc - 1; m++)
        argv[m] = argv[m + 1];
      argc--;
      k--;
    }
  }

  if (argc != 2) {
    printUsage();
    return argc <= 1 ? 0 : -1;
  }

  return DumpPawgReport(argv[1], bUseRead, bJson);
}
