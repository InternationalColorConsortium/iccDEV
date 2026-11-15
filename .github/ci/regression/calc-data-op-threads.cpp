/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// CTest contract for calculator data operations under threaded apply (#3069).
//
// A run of consecutive data operations is pushed in one step. Its length used
// to be cached in the first operation's extra field the first time it ran, a
// write into the op array that the calculator shares with every apply object.
// Two threads applying the same transform raced on it, and ThreadSanitizer
// reported the race (IccMpeCalc.cpp:278 at 93eb2792).  The run is now counted
// on every execution.
//
// Four threads, each with its own apply object, start together on a freshly
// begun transform and check every result.  The race itself shows only under a
// ThreadSanitizer build: the old cache always held the right count, so the
// results are the same with or without the fix.  The programs put data runs at
// the end of if, else and case blocks, so that a run-length computed for the
// whole op array, which would merge a block's last run with the data after the
// block, does fail here in every build.
//
// Returns 0 on success; 1 if any check failed (each printed).
#include "IccMpeCalc.h"
#include "IccTagMPE.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

std::atomic<int> g_failures(0);

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "calc-data-op-threads: FAIL  %s\n", label);
    g_failures++;
  }
}

struct Program {
  const char *text;
  icFloatNumber whenTrue[3];
  icFloatNumber whenFalse[3];
};

const Program kPrograms[] = {
  // Each branch ends with a run of two; the run after the block is separate.
  { "{ in(0) if { 0.5 0.25 } else { 0.125 0.0625 } 0.75 out(0,3) }",
    { 0.5f, 0.25f, 0.75f }, { 0.125f, 0.0625f, 0.75f } },
  // An if with no else: the one-op run in the block is followed directly by
  // a run of two in the enclosing sequence.
  { "{ 0.375 in(0) if { pop 0.5 } 0.25 0.75 out(0,3) }",
    { 0.5f, 0.25f, 0.75f }, { 0.375f, 0.25f, 0.75f } },
  // select: in(0) of 0 runs case 0; 1 has no case and runs dflt.
  { "{ in(0) sel case { 0.125 0.0625 } dflt { 0.5 0.25 } 0.75 out(0,3) }",
    { 0.5f, 0.25f, 0.75f }, { 0.125f, 0.0625f, 0.75f } },
};

bool same(const icFloatNumber *a, const icFloatNumber *b)
{
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

void runProgram(const Program &prog)
{
  CIccTagMultiProcessElement mpe(3, 3);
  CIccMpeCalculator *pCalc = new CIccMpeCalculator(3, 3);
  std::string sReport;
  if (pCalc->SetCalcFunc(prog.text, sReport) != icFuncParseNoError) {
    std::fprintf(stderr, "calc-data-op-threads: cannot parse \"%s\": %s\n",
                 prog.text, sReport.c_str());
    delete pCalc;
    g_failures++;
    return;
  }
  mpe.Attach(pCalc);
  if (!mpe.Begin()) {
    check(false, prog.text);
    return;
  }

  const int kThreads = 4;
  std::vector<CIccApplyTagMpe *> applies;
  for (int t = 0; t < kThreads; t++)
    applies.push_back(mpe.GetNewApply());

  std::atomic<int> ready(0);
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; t++) {
    threads.emplace_back([&, t]() {
      ready++;
      while (ready.load() < kThreads)
        std::this_thread::yield();
      bool ok = applies[t] != NULL;
      for (int i = 0; ok && i < 2000; i++) {
        bool bTrue = ((i + t) & 1) != 0;
        icFloatNumber src[3] = { bTrue ? 1.0f : 0.0f, 0, 0 };
        icFloatNumber dst[3] = { -1, -1, -1 };
        mpe.Apply(applies[t], dst, src);
        ok = same(dst, bTrue ? prog.whenTrue : prog.whenFalse);
      }
      check(ok, prog.text);
    });
  }
  for (std::thread &th : threads)
    th.join();
  for (CIccApplyTagMpe *p : applies)
    delete p;
}

} // namespace

int main()
{
  for (const Program &p : kPrograms)
    runProgram(p);

  if (g_failures) {
    std::fprintf(stderr, "calc-data-op-threads: %d check(s) failed\n", g_failures.load());
    return 1;
  }
  std::printf("calc-data-op-threads: all checks passed\n");
  return 0;
}
