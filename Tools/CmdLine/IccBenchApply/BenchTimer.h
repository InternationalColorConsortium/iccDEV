/*
    File:       BenchTimer.h

    Contains:   Timing, statistics, checksum, and buffer fill for iccBenchApply

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

#ifndef _BENCHTIMER_H
#define _BENCHTIMER_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <vector>

#include "IccDefs.h"

// icBenchChecksum and icBenchFill below are both defined over modular 32-bit
// arithmetic: the wraparound is the algorithm, not an accident of a too-small
// type. Unsigned overflow is well defined in C++, so this is not undefined
// behaviour -- but UBSan's -fsanitize=integer group flags every wrap as
// suspicious, and the sanitizer presets build with -fno-sanitize-recover=integer,
// so the first LCG step exits the process before a single benchmark row prints.
//
// Annotating the two functions states the intent where it is true, rather than
// laundering the wrap through a 64-bit multiply and a truncating cast: that
// spelling computes the identical value but reads as if the width mattered.
// IccProfLib/IccMD5.cpp:32 already solves the identical problem this way, for
// the same reason -- named narrowly here, so the rest of the integer group
// (implicit truncation and sign change) keeps checking these two functions.
#if defined(__clang__)
#define ICC_BENCH_NO_SANITIZE \
  __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define ICC_BENCH_NO_SANITIZE
#endif

struct BenchStats {
  double medianMpxPerSec;
  double minMpxPerSec;
  double maxMpxPerSec;
};

// One warm-up pass, then nRepeats timed passes; report the median.
//
// Median rather than mean because runner noise is one-sided: interrupts,
// contention, and frequency scaling only ever make a pass slower, never faster.
// A mean is dragged down by any single hostile pass, and a min reports a best
// case the caller will not see again.
inline BenchStats icBenchRun(const std::function<void()> &fn,
                             icUInt32Number nUnits, int nRepeats)
{
  fn();  // warm-up: first-touch page faults and cold caches land here

  std::vector<double> rates;
  rates.reserve((size_t)nRepeats);

  for (int i = 0; i < nRepeats; i++) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();

    const double secs = std::chrono::duration<double>(t1 - t0).count();
    rates.push_back(secs > 0.0 ? ((double)nUnits / secs) / 1.0e6 : 0.0);
  }

  std::sort(rates.begin(), rates.end());

  BenchStats rv;
  rv.minMpxPerSec    = rates.front();
  rv.maxMpxPerSec    = rates.back();
  rv.medianMpxPerSec = rates[rates.size() / 2];
  return rv;
}

// FNV-1a over the raw float bytes.
//
// Hashing the bit patterns, not decimal renderings, makes this exact -- which is
// the point: it is the equivalence oracle for the optimisation branches that
// follow. It also means the harness consumes its own output, so the compiler
// cannot elide the apply loop as dead code.
//
// Exactness cuts both ways: this is also sensitive to legitimate floating-point
// reassociation, so a checksum only compares meaningfully between builds made
// with the same compiler flags. Readme.md states this.
inline icUInt32Number icBenchChecksum(const icFloatNumber *pData, size_t nFloats)
  ICC_BENCH_NO_SANITIZE
{
  const unsigned char *p = (const unsigned char *)pData;
  const size_t nBytes = nFloats * sizeof(icFloatNumber);

  icUInt32Number h = 2166136261u;
  for (size_t i = 0; i < nBytes; i++) {
    h ^= (icUInt32Number)p[i];
    h *= 16777619u;
  }
  return h;
}

/// Number of trailing rows icBenchFill reserves for pathological values.
static const icUInt32Number kBenchPathologicalRows = 5;

// Deterministic fill. The last kBenchPathologicalRows rows are pathological, one
// value per row, filling every channel.
//
// The pathological rows are deliberate. Several of the checks this harness exists
// to measure concern clamping and non-finite handling -- the clamp in the CLUT
// interpolators, the isfinite tests in the sampled curves. A happy-path-only
// buffer would leave them unexercised and let a wrong change pass the checksum
// oracle unnoticed.
//
// One value per row, rather than a single row cycling through the values by
// channel index. The cycling version had a real blind spot: it placed +Inf only
// at channel index 4, so no profile with fewer than five input channels ever saw
// one. That hid a deliberate change to +Inf handling in the CLUT clamp -- the
// only profiles in the corpus that install the affected clip path are
// three-channel, and the only >=5-channel case has no CLUT element at all, so
// every checksum was unchanged and the change looked verified when it was
// simply untested.
inline void icBenchFill(icFloatNumber *pBuf, icUInt32Number nPixels,
                        icUInt16Number nSamples, icUInt32Number nSeed)
  ICC_BENCH_NO_SANITIZE
{
  icUInt32Number state = nSeed ? nSeed : 1u;
  const icUInt32Number nPath   = nPixels < kBenchPathologicalRows
                                   ? nPixels : kBenchPathologicalRows;
  const icUInt32Number nNormal = nPixels - nPath;

  for (icUInt32Number i = 0; i < nNormal; i++) {
    for (icUInt16Number s = 0; s < nSamples; s++) {
      state = state * 1664525u + 1013904223u;  // Numerical Recipes LCG
      pBuf[(size_t)i * nSamples + s] =
        (icFloatNumber)((state >> 8) & 0xFFFFFF) / (icFloatNumber)0xFFFFFF;
    }
  }

  const icFloatNumber inf = std::numeric_limits<icFloatNumber>::infinity();
  const icFloatNumber pathological[kBenchPathologicalRows] = {
    (icFloatNumber)-1.0,                                    // below range
    (icFloatNumber) 2.0,                                    // above range
    inf,
    -inf,
    std::numeric_limits<icFloatNumber>::quiet_NaN(),
  };

  // Fill from the end, so a tiny buffer still gets the most interesting values.
  for (icUInt32Number r = 0; r < nPath; r++) {
    icFloatNumber *pRow = pBuf + (size_t)(nPixels - 1 - r) * nSamples;
    const icFloatNumber v = pathological[kBenchPathologicalRows - 1 - r];
    for (icUInt16Number s = 0; s < nSamples; s++)
      pRow[s] = v;
  }
}

#endif // _BENCHTIMER_H
