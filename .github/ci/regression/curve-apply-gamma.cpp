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

// CTest contract for CIccTagCurve gamma application.  A one-entry curve is
// publicly mutable, so Apply() must use the current sample without requiring
// Begin() to refresh hidden state.

#include "IccIO.h"
#include "IccTagLut.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "curve-apply-gamma: FAIL  %s\n", label);
    g_failures++;
  }
}

void check_close(icFloatNumber actual, icFloatNumber expected,
                 const char *label)
{
  check(std::fabs(actual - expected) < 0.00001f, label);
}

icFloatNumber encoded_gamma(icUInt16Number raw)
{
  return static_cast<icFloatNumber>(raw) / 65535.0f;
}

bool build_curve_tag(CIccMemIO &io, icUInt16Number raw)
{
  icTagTypeSignature sig = icSigCurveType;
  icUInt32Number reserved = 0;
  icUInt32Number count = 1;

  if (!io.Alloc(14, true) || !io.Write32(&sig) ||
      !io.Write32(&reserved) || !io.Write32(&count) || !io.Write16(&raw))
    return false;

  return io.Seek(0, icSeekSet) == 0;
}

}  // namespace

int main()
{
  CIccMemIO io;
  check(build_curve_tag(io, 0x0200), "built gamma-2 curve tag");

  CIccTagCurve parsed;
  check(parsed.Read(static_cast<icUInt32Number>(io.GetLength()), &io),
        "read gamma-2 curve tag");
  check_close(parsed.Apply(0.5f), 0.25f,
              "Apply uses a parsed gamma without Begin");

  parsed.Begin();
  parsed[0] = encoded_gamma(0x0100);
  check_close(parsed.Apply(0.5f), 0.5f,
              "Apply observes operator[] mutation after Begin");

  parsed.Begin();
  *parsed.GetData(0) = encoded_gamma(0x0200);
  check_close(parsed.Apply(0.5f), 0.25f,
              "Apply observes GetData mutation after Begin");

  CIccTagCurve copied(parsed);
  check_close(copied.Apply(0.5f), 0.25f,
              "copy constructor preserves gamma application");

  CIccTagCurve assigned;
  assigned = parsed;
  check_close(assigned.Apply(0.5f), 0.25f,
              "assignment preserves gamma application");

  CIccTagCurve set_gamma;
  check(set_gamma.SetGamma(2.0f), "SetGamma accepts gamma 2");
  check_close(set_gamma.Apply(0.5f), 0.25f,
              "Apply uses SetGamma value without Begin");

  CIccTagCurve zero_gamma(1);
  check_close(zero_gamma.Apply(0.5f), 1.0f,
              "size-one constructor applies its zero gamma without Begin");

  if (g_failures) {
    std::fprintf(stderr, "curve-apply-gamma: %d check(s) failed\n", g_failures);
    return 1;
  }

  std::fprintf(stdout, "curve-apply-gamma: all checks passed\n");
  return 0;
}
