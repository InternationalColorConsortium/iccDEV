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

// Deterministic fill. Rows 0..n-2 are in-gamut; the final row is pathological.
//
// The pathological row is deliberate. Several of the checks this harness exists
// to measure concern clamping and non-finite handling -- ClutUnitClip, the
// isfinite tests in the interpolators and the sampled curves. A happy-path-only
// buffer would leave them unexercised, and would let a wrong hoist pass the
// checksum oracle unnoticed.
inline void icBenchFill(icFloatNumber *pBuf, icUInt32Number nPixels,
                        icUInt16Number nSamples, icUInt32Number nSeed)
{
  icUInt32Number state = nSeed ? nSeed : 1u;
  const icUInt32Number nNormal = nPixels ? nPixels - 1 : 0;

  for (icUInt32Number i = 0; i < nNormal; i++) {
    for (icUInt16Number s = 0; s < nSamples; s++) {
      state = state * 1664525u + 1013904223u;  // Numerical Recipes LCG
      pBuf[(size_t)i * nSamples + s] =
        (icFloatNumber)((state >> 8) & 0xFFFFFF) / (icFloatNumber)0xFFFFFF;
    }
  }

  if (nPixels) {
    static const icFloatNumber pathological[6] = {
      (icFloatNumber)-1.0,
      (icFloatNumber) 2.0,
      (icFloatNumber) 0.0,
      (icFloatNumber) 1.0,
      std::numeric_limits<icFloatNumber>::infinity(),
      -std::numeric_limits<icFloatNumber>::infinity(),
    };
    icFloatNumber *pLast = pBuf + (size_t)nNormal * nSamples;
    for (icUInt16Number s = 0; s < nSamples; s++) {
      pLast[s] = (s == nSamples - 1)
                   ? std::numeric_limits<icFloatNumber>::quiet_NaN()
                   : pathological[s % 6];
    }
  }
}

#endif // _BENCHTIMER_H
