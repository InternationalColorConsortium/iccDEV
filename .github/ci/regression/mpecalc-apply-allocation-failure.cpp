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
/*
  The refined iccdev/unchecked-allocation query found that
  CIccMpeCalculator::GetNewApply() indexed m_SubElem immediately after calloc.
  The GNU linker wrapper below rejects only the one-entry pointer-array
  allocation after the fixture arms it. Before the fix this is a deterministic
  null write; after the fix GetNewApply() must fail closed and clean up.
*/

#include "IccMpeCalc.h"

#include <cstddef>
#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

bool g_rejectSubApplyArray = false;
bool g_rejectedSubApplyArray = false;

const char *kChildFunction = "{ in(0) out(0) }";
const char *kParentFunction = "{ in(0) calc(0) out(0) }";

CIccMpeCalculator *makeCalculator(icUInt16Number channels, const char *function)
{
  CIccMpeCalculator *calculator = new CIccMpeCalculator(channels, channels);
  std::string report;
  if (calculator->SetCalcFunc(function, report) != icFuncParseNoError) {
    std::fprintf(stderr, "calculator fixture parse failed: %s\n", report.c_str());
    delete calculator;
    return NULL;
  }
  return calculator;
}

} // namespace

extern "C" void *__real_calloc(std::size_t count, std::size_t size);

extern "C" void *__wrap_calloc(std::size_t count, std::size_t size)
{
  if (g_rejectSubApplyArray && count == 1 && size == sizeof(void *)) {
    g_rejectSubApplyArray = false;
    g_rejectedSubApplyArray = true;
    return NULL;
  }
  return __real_calloc(count, size);
}

int main()
{
  CIccMpeCalculator *parent = new CIccMpeCalculator(1, 1);
  CIccMpeCalculator *child = makeCalculator(1, kChildFunction);
  if (!parent || !child || !parent->SetSubElem(0, child)) {
    std::fputs("unable to build calculator allocation fixture\n", stderr);
    delete child;
    delete parent;
    return 2;
  }

  std::string report;
  if (parent->SetCalcFunc(kParentFunction, report) != icFuncParseNoError) {
    std::fprintf(stderr, "parent calculator parse failed: %s\n", report.c_str());
    delete parent;
    return 2;
  }

  g_rejectSubApplyArray = true;
  CIccApplyMpe *apply = parent->GetNewApply(NULL);

  const bool passed = g_rejectedSubApplyArray && !apply;
  delete apply;
  delete parent;

  if (!passed) {
    std::fputs("GetNewApply did not fail closed after calloc rejection\n", stderr);
    return 1;
  }

  std::puts("GetNewApply rejected the failed sub-apply allocation");
  return 0;
}
