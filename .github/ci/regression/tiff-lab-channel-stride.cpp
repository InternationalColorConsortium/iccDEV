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
// CTest contract for the CIELAB sample shift in WriteTIFF (#2880).
//
// TIFF stores CIELAB a* and b* signed; the tools hold them unsigned, so
// WriteTIFF() shifts them in the caller's buffer before writing.
// shiftTIFFLAB() stepped 3 samples per pixel whatever the raster's channel
// count. iccProfileVisualizePlot hands it a CLUT raster whose photometric comes
// from the LUT's output colour space and whose channel count comes from the
// CLUT, so a malformed Lab LUT with one output channel made it read and rewrite
// past the end of the buffer (a heap-buffer-overflow under the fuzzer).
//
// The shift now steps by the raster's own channel count and shifts only the
// second and third samples, and does nothing to an L*-only raster.
//
// Because the shift is applied in place, the test inspects the buffer after
// the call: an L*-only raster must come back unchanged, and in an L*a*b*+alpha
// raster only a* and b* may move. An unfixed WriteTIFF rewrites the second and
// third L* samples of the first raster and the alpha of the second, so both
// checks fail without a sanitizer.
//
// The same source is built against each of the two MiniTIFF.cpp copies
// (Tools/CmdLine/IccProfilePlot and Tools/CmdLine/IccProfileVisualize).
//
// usage: <test> <output directory>
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "MiniTIFF.hpp"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#ifdef ICCDEV_TIFF_LAB_VISUALIZE_COPY
// The IccProfileVisualize copy reports errors through the tool's own logger,
// which lives in iccProfileVisualize.cpp; this stands in for it.
void LogAnError(FILE *stream, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stream, format, args);
  va_end(args);
}
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char *label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label);
  if (!ok)
    ++g_failures;
}

void lOnly(const std::string &dir)
{
  // 4 x 2 pixels, one sample each: L* only.
  std::vector<uint8_t> px = { 10, 20, 30, 40, 50, 60, 70, 80 };
  const std::vector<uint8_t> before = px;
  bool ok = WriteTIFF(dir + "/lab-l-only.tif", 100.0f, TIFF_MODE_CIELAB, px.data(),
                      4, 2, 1, 8);
  check(ok, "L*-only: written");
  check(px == before, "L*-only: no sample is shifted");
}

void labAlpha(const std::string &dir)
{
  // 2 x 1 pixels of L* a* b* alpha.
  std::vector<uint8_t> px = { 50, 128, 138, 255,
                              60, 118, 128, 7 };
  bool ok = WriteTIFF(dir + "/lab-alpha.tif", 100.0f, TIFF_MODE_CIELAB, px.data(),
                      2, 1, 4, 8);
  check(ok, "L*a*b*+alpha: written");
  const std::vector<uint8_t> expect = { 50, 0, 10, 255,
                                        60, (uint8_t)(118 - 128), 0, 7 };
  check(px == expect, "L*a*b*+alpha: only a* and b* are shifted");
}

void lab16(const std::string &dir)
{
  // 1 x 1 pixel, 16-bit L* a* b*: the three-sample case is unchanged.
  std::vector<uint16_t> px = { 1000, 0x8000 + 5, 0x8000 - 5 };
  bool ok = WriteTIFF(dir + "/lab16.tif", 100.0f, TIFF_MODE_CIELAB,
                      reinterpret_cast<uint8_t *>(px.data()), 1, 1, 3, 16);
  check(ok, "16-bit L*a*b*: written");
  check(px[0] == 1000 && px[1] == 5 && px[2] == (uint16_t)(-5),
        "16-bit L*a*b*: a* and b* shifted as before");
}

} // namespace

int main(int argc, char **argv)
{
  if (argc < 2) {
    std::printf("usage: %s <output directory>\n", argv[0]);
    return 2;
  }
  const std::string dir = argv[1];

  lOnly(dir);
  labAlpha(dir);
  lab16(dir);

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
