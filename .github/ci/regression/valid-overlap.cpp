/*
 * The ICC Software License, Version 0.2
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// CTest contract for icValidOverlap() (#2731).
//
// Two positions are acceptable if their byte ranges are disjoint, or if
// bAllowSame is set and they start at the same offset, i.e. reference the same
// data. The old test also accepted any overlapping pair of equal size whatever
// the offsets, even with bAllowSame false. CIccMpeToneMap::Read() is the
// caller: it passes false for the luminance curve against each channel
// function and true between channel functions, and shares a function between
// channels whose offsets match.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccUtil.h"

#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "valid-overlap: FAIL  %s\n", label);
    g_failures++;
  }
}

icPositionNumber pos(icUInt32Number offset, icUInt32Number size)
{
  icPositionNumber p;
  p.offset = offset;
  p.size = size;
  return p;
}

// Checks both argument orders: the relation is symmetric.
void expect(icPositionNumber a, icPositionNumber b, bool bAllowSame, bool expected, const char *label)
{
  check(icValidOverlap(a, b, bAllowSame) == expected, label);
  check(icValidOverlap(b, a, bAllowSame) == expected, label);
}

} // namespace

int main()
{
  for (int allow = 0; allow < 2; allow++) {
    bool b = allow != 0;
    expect(pos(0, 10), pos(10, 10), b, true, "adjacent ranges are disjoint");
    expect(pos(0, 10), pos(20, 10), b, true, "separated ranges are disjoint");
    expect(pos(0, 10), pos(5, 10), b, false, "a partial overlap of equal size is refused");
    expect(pos(0, 10), pos(5, 20), b, false, "a partial overlap of different sizes is refused");
    expect(pos(0, 20), pos(5, 10), b, false, "a range inside another is refused");
  }

  // Shared data: positions at the same offset, the case bAllowSame decides.
  expect(pos(8, 12), pos(8, 12), true, true, "identical positions are accepted when bAllowSame");
  expect(pos(8, 12), pos(8, 12), false, false, "identical positions are refused without bAllowSame");
  expect(pos(0, 10), pos(0, 20), true, true, "the same offset with a different size is accepted when bAllowSame");
  expect(pos(0, 10), pos(0, 20), false, false, "the same offset with a different size is refused without bAllowSame");

  // Range ends are computed in 64 bits. In 32 bits 0xFFFFFFF0 + 0x20 wraps to
  // 0x10, and the first range would look as if it ended before the second.
  expect(pos(0xFFFFFFF0u, 0x20), pos(0xFFFFFFF8u, 4), false, false,
         "a range whose end passes 2^32 still overlaps the range it contains");

  if (g_failures)
    std::fprintf(stderr, "valid-overlap: %d failure(s)\n", g_failures);
  else
    std::printf("valid-overlap: all checks passed\n");
  return g_failures;
}
