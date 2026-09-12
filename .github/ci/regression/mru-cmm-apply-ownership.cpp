/** @file
    File:       mru-cmm-apply-ownership.cpp

    Contains:   CTest helper for CIccApplyMruCmm's ownership of the cached
                CMM's apply object, and for the status it returns on a miss.

    CIccMruCmm decorates a Begin()-ed CIccCmm with a small most-recently-used
    cache.  Each CIccApplyMruCmm holds its own CIccMruCacheFloat, which reads as
    thread-ready -- but every cache MISS used to call m_pCachedCmm->Apply(),
    and CIccCmm::Apply dispatches through that CMM's single m_pApply.  So N
    apply objects over one cached CMM -- which is exactly what
    CIccThreadedCmm::GetNewApplyCmm() hands each worker -- shared one set of
    per-pixel scratch buffers while each believed it had private state.  The
    same defect CIccApplyCmmSearch had before it was given per-sub-chain apply
    objects.

    Init() now takes its own CIccApplyCmm from the cached CMM and the miss path
    drives that instead, so nothing is shared.  A miss also propagates the
    inner status now: both Apply() overloads used to discard it and return
    icCmmStatOk, which reported success over an untransformed pixel AND fed
    that pixel to m_pCache->Update(), poisoning every later hit on the same
    source value.

    What this pins:
      1. A cached CMM produces the same values as the same chain uncached --
         so the cache returns transformed colour, not something stale.
      2. Repeated pixels (forcing hits) agree with distinct pixels (forcing
         misses), which is what a poisoned cache entry would break.
      3. Two apply objects over ONE CIccMruCmm hold distinct apply pointers and
         distinct caches, driven interleaved so a shared inner apply would show
         up as cross-contamination.
      4. Concurrent use across threads agrees with the scalar result, with a
         working set far larger than the cache so MISSES dominate -- a
         hit-heavy workload never touches the shared path and passes even
         against the unfixed library.
      5. A cache smaller than the working set still returns correct values, so
         eviction does not lose or corrupt entries.

    Usage:
      mru-cmm-apply-ownership <profile.icc>

    Exit codes:
      0 - cached and uncached results agree everywhere
      1 - unexpected result
      2 - usage error
    Copyright:  See ICC Software License
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
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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

#include "IccCmm.h"
#include "IccProfile.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The cached and uncached paths run the identical xform chain, so a hit must
// return exactly what the miss stored.  This is float equality with room only
// for the copy through the cache, not a tolerance on the transform itself.
static const icFloatNumber kEpsilon = 1.0e-6f;

static int check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "mru-cmm-apply-ownership: PASS  %s\n", label);
    return 0;
  }

  std::fprintf(stderr, "mru-cmm-apply-ownership: FAIL  %s\n", label);
  return 1;
}

// Builds profile -> profile, the simplest chain that still runs real xforms.
static CIccCmm* BuildChain(const char* profilePath)
{
  std::unique_ptr<CIccCmm> cmm(new (std::nothrow) CIccCmm());
  if (!cmm)
    return nullptr;

  if (cmm->AddXform(profilePath, icRelativeColorimetric) != icCmmStatOk ||
      cmm->AddXform(profilePath, icRelativeColorimetric) != icCmmStatOk)
    return nullptr;

  if (cmm->Begin() != icCmmStatOk)
    return nullptr;

  return cmm.release();
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <profile.icc>\n",
                 argv[0] ? argv[0] : "mru-cmm-apply-ownership");
    return 2;
  }

  const char* profilePath = argv[1];
  int failures = 0;

  // A working set deliberately larger than the caches used below, so misses,
  // hits and evictions all occur.
  std::vector<std::vector<icFloatNumber>> samples;
  for (int i = 0; i < 24; i++) {
    icFloatNumber t = (icFloatNumber)i / 23.0f;
    samples.push_back({ t, (icFloatNumber)(1.0f - t), (icFloatNumber)(0.5f * t) });
  }

  // ---- Reference: the same chain with no cache in front of it. ----
  std::unique_ptr<CIccCmm> reference(BuildChain(profilePath));
  if (!reference) {
    std::fprintf(stderr, "mru-cmm-apply-ownership: unable to build chain from '%s'\n",
                 profilePath);
    return 1;
  }
  if (reference->GetSourceSamples() != 3 || reference->GetDestSamples() != 3) {
    std::fprintf(stderr, "mru-cmm-apply-ownership: expected a 3-channel profile\n");
    return 1;
  }

  std::vector<std::vector<icFloatNumber>> expected;
  for (const auto& src : samples) {
    std::vector<icFloatNumber> dst(3, 0.0f);
    if (reference->Apply(&dst[0], &src[0]) != icCmmStatOk) {
      std::fprintf(stderr, "mru-cmm-apply-ownership: reference Apply failed\n");
      return 1;
    }
    expected.push_back(dst);
  }

  // ---- 1 & 2: cached results match uncached, over misses then hits. ----
  {
    CIccCmm* inner = BuildChain(profilePath);
    CIccMruCmm* mru = inner ? CIccMruCmm::Attach(inner, 8, true) : nullptr;
    if (!mru) {
      std::fprintf(stderr, "mru-cmm-apply-ownership: Attach failed\n");
      return 1;
    }
    std::unique_ptr<CIccCmm> owned(mru);

    int mismatches = 0;
    // Two passes: the first fills the cache, the second is mostly hits.
    for (int pass = 0; pass < 2; pass++) {
      for (size_t i = 0; i < samples.size(); i++) {
        std::vector<icFloatNumber> dst(3, 0.0f);
        if (mru->Apply(&dst[0], &samples[i][0]) != icCmmStatOk) {
          mismatches++;
          continue;
        }
        for (int c = 0; c < 3; c++)
          if (std::fabs(dst[c] - expected[i][c]) > kEpsilon)
            mismatches++;
      }
    }
    failures += check(mismatches == 0,
                      "cached apply matches uncached across misses and hits");

    // Repeating one pixel drives the hit path hard; a poisoned entry shows here.
    int repeatMismatches = 0;
    for (int rep = 0; rep < 32; rep++) {
      std::vector<icFloatNumber> dst(3, 0.0f);
      if (mru->Apply(&dst[0], &samples[5][0]) != icCmmStatOk) {
        repeatMismatches++;
        continue;
      }
      for (int c = 0; c < 3; c++)
        if (std::fabs(dst[c] - expected[5][c]) > kEpsilon)
          repeatMismatches++;
    }
    failures += check(repeatMismatches == 0, "repeated hits stay correct");
  }

  // ---- 3: two apply objects over one CIccMruCmm are independent. ----
  {
    CIccCmm* inner = BuildChain(profilePath);
    CIccMruCmm* mru = inner ? CIccMruCmm::Attach(inner, 4, true) : nullptr;
    if (!mru) {
      std::fprintf(stderr, "mru-cmm-apply-ownership: Attach failed (shared)\n");
      return 1;
    }
    std::unique_ptr<CIccCmm> owned(mru);

    icStatusCMM sa = icCmmStatOk, sb = icCmmStatOk;
    std::unique_ptr<CIccApplyCmm> applyA(mru->GetNewApplyCmm(sa));
    std::unique_ptr<CIccApplyCmm> applyB(mru->GetNewApplyCmm(sb));
    failures += check(applyA && applyB && sa == icCmmStatOk && sb == icCmmStatOk &&
                        applyA.get() != applyB.get(),
                      "GetNewApplyCmm yields two distinct apply objects");

    if (applyA && applyB) {
      // Interleave them over different pixels.  With a shared inner apply the
      // two would tread on each other's scratch between these calls.
      int mismatches = 0;
      for (size_t i = 0; i + 1 < samples.size(); i += 2) {
        std::vector<icFloatNumber> dstA(3, 0.0f), dstB(3, 0.0f);
        if (applyA->Apply(&dstA[0], &samples[i][0]) != icCmmStatOk ||
            applyB->Apply(&dstB[0], &samples[i + 1][0]) != icCmmStatOk) {
          mismatches++;
          continue;
        }
        for (int c = 0; c < 3; c++) {
          if (std::fabs(dstA[c] - expected[i][c]) > kEpsilon)
            mismatches++;
          if (std::fabs(dstB[c] - expected[i + 1][c]) > kEpsilon)
            mismatches++;
        }
      }
      failures += check(mismatches == 0,
                        "interleaved apply objects do not contaminate each other");
    }
  }

  // ---- 4: concurrent use with MISSES dominating. ----
  //
  // This is the section that actually exercises the shared-state path, and it
  // has to be built to miss.  A hit is served entirely from the apply object's
  // private cache and never touches the cached CMM at all, so a workload that
  // mostly hits will pass even against the unfixed library -- which is exactly
  // what an earlier version of this test did.  A working set far larger than
  // the cache, with each thread entering at a different offset, keeps every
  // worker in the miss path at the same time, where the pre-fix code drove one
  // shared CIccApplyCmm (and its m_Pixel / m_Pixel2 stage buffers) from all of
  // them at once.
  {
    // Deliberately many distinct pixels against a 2-entry cache: consecutive
    // lookups evict each other, so essentially every access is a miss.
    std::vector<std::vector<icFloatNumber>> stress;
    for (int i = 0; i < 512; i++) {
      icFloatNumber a = (icFloatNumber)(i % 64) / 63.0f;
      icFloatNumber b = (icFloatNumber)((i / 64) % 8) / 7.0f;
      icFloatNumber c = (icFloatNumber)((i * 7) % 251) / 250.0f;
      stress.push_back({ a, b, c });
    }

    std::vector<std::vector<icFloatNumber>> stressExpected;
    for (const auto& src : stress) {
      std::vector<icFloatNumber> dst(3, 0.0f);
      if (reference->Apply(&dst[0], &src[0]) != icCmmStatOk) {
        std::fprintf(stderr, "mru-cmm-apply-ownership: reference Apply failed (stress)\n");
        return 1;
      }
      stressExpected.push_back(dst);
    }

    CIccCmm* inner = BuildChain(profilePath);
    CIccMruCmm* mru = inner ? CIccMruCmm::Attach(inner, 2, true) : nullptr;
    if (!mru) {
      std::fprintf(stderr, "mru-cmm-apply-ownership: Attach failed (threaded)\n");
      return 1;
    }
    std::unique_ptr<CIccCmm> owned(mru);

    unsigned hw = std::thread::hardware_concurrency();
    const int kThreads = (int)(hw > 8 ? 8 : (hw < 4 ? 4 : hw));
    std::vector<int> bad((size_t)kThreads, 0);
    std::vector<std::thread> workers;

    for (int t = 0; t < kThreads; t++) {
      workers.emplace_back([&, t]() {
        icStatusCMM status = icCmmStatOk;
        std::unique_ptr<CIccApplyCmm> apply(mru->GetNewApplyCmm(status));
        if (!apply || status != icCmmStatOk) {
          bad[(size_t)t]++;
          return;
        }
        // Each worker enters the set at its own offset, so at any instant the
        // threads are transforming different pixels through the shared path.
        const size_t n = stress.size();
        const size_t offset = (n / (size_t)kThreads) * (size_t)t;
        for (int rep = 0; rep < 8; rep++) {
          for (size_t k = 0; k < n; k++) {
            const size_t i = (offset + k) % n;
            std::vector<icFloatNumber> dst(3, 0.0f);
            if (apply->Apply(&dst[0], &stress[i][0]) != icCmmStatOk) {
              bad[(size_t)t]++;
              continue;
            }
            for (int c = 0; c < 3; c++)
              if (std::fabs(dst[c] - stressExpected[i][c]) > kEpsilon)
                bad[(size_t)t]++;
          }
        }
      });
    }
    for (auto& w : workers)
      w.join();

    int total = 0;
    for (int b : bad)
      total += b;
    failures += check(total == 0,
                      "concurrent miss-heavy apply agrees with the scalar result");
  }

  // ---- 5: a cache smaller than the working set still returns correct values. ----
  {
    CIccCmm* inner = BuildChain(profilePath);
    CIccMruCmm* mru = inner ? CIccMruCmm::Attach(inner, 1, true) : nullptr;
    if (!mru) {
      std::fprintf(stderr, "mru-cmm-apply-ownership: Attach failed (tiny cache)\n");
      return 1;
    }
    std::unique_ptr<CIccCmm> owned(mru);

    // Multi-pixel overload, so the loop that carries the second copy of the
    // miss path is exercised too.
    std::vector<icFloatNumber> src, dst(samples.size() * 3, 0.0f);
    for (const auto& s : samples)
      src.insert(src.end(), s.begin(), s.end());

    int mismatches = 0;
    if (mru->Apply(&dst[0], &src[0], (icUInt32Number)samples.size()) != icCmmStatOk) {
      mismatches++;
    }
    else {
      for (size_t i = 0; i < samples.size(); i++)
        for (int c = 0; c < 3; c++)
          if (std::fabs(dst[i * 3 + c] - expected[i][c]) > kEpsilon)
            mismatches++;
    }
    failures += check(mismatches == 0,
                      "multi-pixel apply is correct with an evicting cache");
  }

  // ---- Attach contract: a zero cache size must not leak an owned CMM. ----
  {
    CIccCmm* inner = BuildChain(profilePath);
    // bDeleteCmm is true, so Attach owns inner on every path including this
    // rejection.  Under a leak checker this is the case that used to leak.
    CIccMruCmm* rejected = inner ? CIccMruCmm::Attach(inner, 0, true) : nullptr;
    failures += check(rejected == nullptr,
                      "Attach rejects a zero cache size");
  }

  return failures ? 1 : 0;
}
