/*
    File:       BenchCases.h

    Contains:   Built-in case table and profile path resolution for iccBenchApply

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
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

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation by Claude Opus 5 8-20-2026
//
//////////////////////////////////////////////////////////////////////

#ifndef _BENCHCASES_H
#define _BENCHCASES_H

#include <string>
#include <vector>

/// One profile in a chain, with the encoded rendering intent it is applied at.
struct BenchChainLink {
  std::string profile;       ///< path relative to Testing/, e.g. "V2/v2RgbLut8.icc"
  int         encodedIntent; ///< as parsed by iccApplyToLink: intent plus modifiers
};

/// A named chain. Cases are chains rather than single profiles because not
/// every profile round-trips -- an input-only profile can appear only first.
struct BenchCase {
  std::string                 name;
  int                         interpolation; ///< 0 = linear, 1 = tetrahedral
  std::vector<BenchChainLink> chain;
};

/// The built-in table run by -suite. Built once on first call.
const std::vector<BenchCase> &icBenchBuiltinCases();

/// Records the two roots profile paths are resolved against. Either may be NULL
/// or empty. Reads ICCDEV_BENCH_SOURCE_ROOT / ICCDEV_BENCH_BUILD_ROOT when not
/// called explicitly.
void icBenchSetRoots(const char *szSourceRoot, const char *szBuildRoot);

/// Resolves a Testing-relative path against both roots. Returns false when the
/// file is not found under either, which is a SKIP rather than an error.
bool icBenchResolveProfile(const std::string &rel, std::string &abs);

#endif // _BENCHCASES_H
