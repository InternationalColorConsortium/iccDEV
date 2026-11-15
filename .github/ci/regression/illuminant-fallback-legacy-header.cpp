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
// CTest contract for two uninitialised reads MemorySanitizer reported in #3066.
//
// 1. CIccProfile::calcNormIlluminantXYZ() and calcLumIlluminantXYZ() returned
//    without writing their output when the profile's spectral viewing
//    conditions carried no illuminant data. CIccCombinedConnectionConditions
//    caches both results, so CIccXform::Begin() compared uninitialised
//    illuminant values. A custom illuminant without a spectral power
//    distribution is off-spec (ICC.2:2023 spectralViewingConditionsType
//    specifies the illuminant "both by its illuminant type as well as its
//    power distribution function"), so this pins a chosen fallback, not a
//    spec requirement. Both functions now report what getNormIlluminantXYZ()
//    and getLumIlluminantXYZ() report for the same conditions: the tag's
//    illuminantXYZ, normalised and as stored. The reasons, including ICC.2
//    A.1.10 for luminance and #958's no-D50-substitution contract, are given
//    in IccProfile.cpp. The fixture's D65 illuminantXYZ deliberately differs
//    from the D50 header, so a header fallback would fail here. Checked
//    deterministically: the
//    outputs start as a sentinel, and the cached copy is built in storage
//    filled with NaN, so an unwritten value never compares equal.
// 2. CIccCfgColorData::fromLegacy() read the colour signature from fixed
//    offsets past the end of a short first line, in a buffer from new[], and
//    passed an unwritten sscanf() buffer to strnlen() when the encoding line
//    was blank. Both inputs must be refused cleanly, as must a signature that
//    is not enclosed in matching single or double quotes, or is not four or
//    six characters long, which the fixed offsets used to load regardless. The uninitialised reads themselves only
//    show under MemorySanitizer or valgrind; this checks the verdicts.
//
// Returns 0 on success; 1 if any check failed (each printed).
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccPcc.h"
#include "IccUtil.h"
#include "IccCmmConfig.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool condition, const char *label)
{
  if (!condition) {
    std::fprintf(stderr, "illuminant-fallback-legacy-header: FAIL  %s\n", label);
    g_failures++;
  }
}

bool same(const icFloatNumber *a, const icFloatNumber *b)
{
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

void testIlluminantFallback()
{
  CIccProfile prof;
  prof.InitHeader();
  prof.m_Header.version = icVersionNumberV5;
  // A custom illuminant with no spectral data: getIlluminant() returns NULL.
  // (A known standard illuminant, the D50 default included, is synthesised.)
  CIccTagSpectralViewingConditions *pView = new CIccTagSpectralViewingConditions;
  icSpectralRange none;
  std::memset(&none, 0, sizeof(none));
  pView->setIlluminant(icIlluminantCustom, none, NULL);
  // A D65 illuminant in cd/m^2, which differs from the D50 header illuminant.
  pView->m_illuminantXYZ.X = 950.47f;
  pView->m_illuminantXYZ.Y = 1000.0f;
  pView->m_illuminantXYZ.Z = 1088.83f;
  prof.AttachTag(icSigSpectralViewingConditionsTag, pView);
  icSpectralRange range;
  check(pView->getIlluminant(range) == NULL, "fixture: the viewing conditions have no illuminant data");

  // The uncached values for the same conditions.
  icFloatNumber norm[3], lum[3];
  prof.getNormIlluminantXYZ(norm);
  prof.getLumIlluminantXYZ(lum);
  check(std::fabs(norm[0] - 0.95047f) < 1e-5f && norm[1] == 1.0f &&
        std::fabs(norm[2] - 1.08883f) < 1e-5f,
        "fixture: getNormIlluminantXYZ gives the normalised D65 illuminant");
  check(lum[1] == 1000.0f, "fixture: getLumIlluminantXYZ gives the stored luminance");

  icFloatNumber xyz[3] = { -7, -7, -7 };
  prof.calcNormIlluminantXYZ(xyz, NULL);
  check(same(xyz, norm), "calcNormIlluminantXYZ without illuminant data agrees with getNormIlluminantXYZ");

  xyz[0] = xyz[1] = xyz[2] = -7;
  prof.calcLumIlluminantXYZ(xyz, NULL);
  check(same(xyz, lum), "calcLumIlluminantXYZ without illuminant data agrees with getLumIlluminantXYZ");

  // The cached copy, built in storage that starts as NaN.
  alignas(CIccCombinedConnectionConditions) unsigned char storage[sizeof(CIccCombinedConnectionConditions)];
  std::memset(storage, 0xFF, sizeof(storage));
  CIccCombinedConnectionConditions *pPcc =
    new (storage) CIccCombinedConnectionConditions(&prof, &prof, false);
  icFloatNumber got[3];
  pPcc->getNormIlluminantXYZ(got);
  check(same(got, norm), "the combined conditions cache the normalised illuminant");
  pPcc->getLumIlluminantXYZ(got);
  check(same(got, lum), "the combined conditions cache the illuminant luminance");
  pPcc->~CIccCombinedConnectionConditions();
}

enum Verdict { kAccepted, kRefused, kNoFixture };

Verdict legacy(const char *contents)
{
  const char *path = "iccdev-illuminant-fallback-legacy-header.txt";
  std::FILE *f = std::fopen(path, "wb");
  if (!f)
    return kNoFixture;
  std::fputs(contents, f);
  std::fclose(f);
  CIccCfgColorData data;
  bool ok = data.fromLegacy(path, true);
  std::remove(path);
  return ok ? kAccepted : kRefused;
}

void testLegacyHeader()
{
  // A fixture that cannot be written fails its check instead of passing as
  // a refusal.
  check(legacy("'RGB ' ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kAccepted,
        "control: a well-formed legacy RGB file loads");
  // Testing/ApplyDataFiles/WptChecker-400_10_700.txt quotes its signature with
  // double quotes. A four-character one used to be refused: the copy, which
  // ended only at a single quote, ran on to six characters, and icGetSigVal()
  // reads any six-character string as a channel-count signature.
  check(legacy("\"RGB \" ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kAccepted,
        "a double-quoted four-character signature loads");
  check(legacy("'R\n") == kRefused, "a three-character first line is refused");
  check(legacy("") == kRefused, "an empty file is refused");
  // The signature used to be copied from fixed offsets whatever the first
  // character, and the closing quote only ended the copy early. A missing
  // closing quote therefore loaded as RGB; the other three were refused only
  // because the copy ran on into icGetSigVal()'s six-character form.
  check(legacy("XRGB ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kRefused,
        "a signature without an opening quote is refused");
  check(legacy("'RGB \nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kRefused,
        "a signature without a closing quote is refused");
  check(legacy("'RGB \" ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kRefused,
        "a signature closed by the other quote is refused");
  check(legacy("'RG' ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kRefused,
        "a signature shorter than four characters is refused");
  // Five characters used to load as their first four: 'nc01F' (a typo for
  // 'nc001F') became 'nc01', read as 0x3031 = 12337 channels, so the file
  // loaded with every data line skipped.
  check(legacy("'nc01F' ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kRefused,
        "a five-character signature is refused");
  check(legacy("'nc0003' ; Data Format\nicEncodeFloat ; Encoding\n0.25 0.5 0.75\n") == kAccepted,
        "a six-character channel signature loads");
  check(legacy("'RGB ' ; Data Format\n\n0.25 0.5 0.75\n") == kRefused,
        "a blank encoding line is refused");
  check(legacy("'RGB ' ; Data Format\n") == kRefused, "a missing encoding line is refused");
}

} // namespace

int main()
{
  testIlluminantFallback();
  testLegacyHeader();

  if (g_failures) {
    std::fprintf(stderr, "illuminant-fallback-legacy-header: %d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("illuminant-fallback-legacy-header: all checks passed\n");
  return 0;
}
