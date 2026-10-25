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
 * A malformed select body used to leak every successfully parsed case that
 * preceded the error.  ParseFuncDef() stored those cases as raw pointers and
 * returned before reaching its success-only cleanup loop when a later case or
 * default body failed to parse.  LeakSanitizer supplies the ownership check;
 * the return-value checks remain useful on platforms without LeakSanitizer.
 * See issue #2881.
 */

#include "IccMpeCalc.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void expectStatus(const char *szProgram, icFuncParseStatus expected,
                  const char *label)
{
  CIccCalculatorFunc fn(NULL);
  std::string report;
  icFuncParseStatus status = fn.SetFunction(szProgram, report);

  if (status != expected) {
    ++g_fail;
    std::fprintf(stderr,
                 "[mpecalc-select-parse-leak] FAIL %s: status %d, expected %d\n",
                 label, (int)status, (int)expected);
  }
}

} // namespace

int main()
{
  expectStatus("{ 2 sel case { 1 } case { 2 } case { 4 } case { d } }",
               icFuncParseSyntaxError, "malformed later case");
  expectStatus("{ 2 sel case { 1 } case { 2 } case { 4 } dflt { d } }",
               icFuncParseSyntaxError, "malformed default");
  expectStatus("{ 2 sel case { 1 } case { 2 } case { 4 } dflt { -1 } }",
               icFuncParseNoError, "valid select control");

  if (g_fail)
    return 1;

  std::printf("[mpecalc-select-parse-leak] all checks passed\n");
  return 0;
}
