/** @file
    File:       PawgReport.h

    Contains:   ICC PAWG profile assessment report support for IccPawgReport

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

#ifndef ICC_PAWG_REPORT_H
#define ICC_PAWG_REPORT_H

#include <cstddef>
#include <string>

int DumpPawgReport(const char *szFilename, bool bUseRead, bool bJson);

// Assessment-only entry points (issue #1775): run the PAWG evaluation "up to the
// point of Output, not for Output" over an in-memory profile image, so the
// overnight CI fuzzing harness can drive the code path with no filesystem
// round-trip and no report emission.

// Runs the full PAWG evaluation (all S/C/Q checks, including the S14 compression
// measurement) over the buffer and returns 1 if any check FAILs, else 0.
// Intended as a libFuzzer target:
//   extern "C" int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n)
//   { AssessPawgFromMemory(d, n); return 0; }
int AssessPawgFromMemory(const unsigned char *data, size_t size);

// Runs ONLY the S14 compression-path measurement over the buffer, returning one
// of the kPawg* codes below and (if outDetail != nullptr) the human-readable
// detail string.  Detection is raw-bytes based, so it needs no parsed profile.
int PawgCompressionVerdict(const unsigned char *data, size_t size, std::string *outDetail);

// Verdict codes returned by PawgCompressionVerdict.  Kept as plain ints (not the
// internal PawgVerdict enum) so consumers need not include the report internals;
// PawgReport.cpp static_asserts they match PawgVerdict's ordering.
enum {
  kPawgOk            = 0,
  kPawgWarn          = 1,
  kPawgFail          = 2,
  kPawgNotApplicable = 3,
  kPawgGap           = 4,
  kPawgNotRun        = 5
};

#endif
