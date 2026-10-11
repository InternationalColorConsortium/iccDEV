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
    File:       taint-trace-memory-state.cpp

    Contains:   Positive control for the ICC_TAINT_TRACE memory-state query in
                IccSignatureUtils.h, run by iccdev-taint-trace-qa.sh under
                Valgrind Memcheck and by iccdev-msan-taint-qa.sh under
                MemorySanitizer.

    The JSON fixtures those scripts drive are all fail-closed or clean: once
    #2543 and the non-numeric colorant PCS refusal landed, no library input
    leaves poisoned bytes for the tracer to find.  A tracer that never answered
    "poisoned" passed every case -- measured by deleting the Valgrind branch's
    poisoned return, which left the Memcheck script at 5/5.

    This helper poisons bytes itself, so it does not depend on a library bug
    staying unfixed.  One 16-byte buffer is traced twice through
    ICC_TAINT_TRACE_MEMORY: fully written, then with bytes 5..15 marked
    undefined.  The scripts require state=initialized for the first and
    state=poisoned first_bad=5 for the second, so a tracer that is blind to
    poison, or that reports the wrong offset, fails.

    It exits 2 when the build has no memory-state backend (neither MSan nor
    <valgrind/memcheck.h>), where every trace would read state=unknown.
*/

#include "IccSignatureUtils.h"

#include <cstdio>
#include <cstring>

int main()
{
#if !defined(ICC_TAINT_TRACE_ENABLED)
  std::fprintf(stderr,
    "[FAIL] built without ICC_TAINT_TRACE_ENABLED: configure a Debug tree "
    "with -DICCDEV_ENABLE_TAINT_TRACE=ON\n");
  return 2;
#elif !defined(ICC_TAINT_TRACE_HAS_MSAN) && !defined(ICC_TAINT_TRACE_HAS_VALGRIND)
  std::fprintf(stderr,
    "[FAIL] no memory-state backend: built without MemorySanitizer and "
    "<valgrind/memcheck.h> was not on the include path\n");
  return 2;
#else
  unsigned char buffer[16];
  std::memset(buffer, 0x5a, sizeof(buffer));
  ICC_TAINT_TRACE_MEMORY("probe.initialized", "source-read",
                         buffer, sizeof(buffer));

  const size_t firstPoisoned = 5;
#if defined(ICC_TAINT_TRACE_HAS_MSAN)
  __msan_poison(buffer + firstPoisoned, sizeof(buffer) - firstPoisoned);
#else
  (void)VALGRIND_MAKE_MEM_UNDEFINED(buffer + firstPoisoned,
                                    sizeof(buffer) - firstPoisoned);
#endif
  ICC_TAINT_TRACE_MEMORY("probe.poisoned", "source-read",
                         buffer, sizeof(buffer));
  return 0;
#endif
}
