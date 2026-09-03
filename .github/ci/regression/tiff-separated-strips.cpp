/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the ICC
 *    organization endorses or promotes products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Regression coverage for issue #2228, separated (PLANARCONFIG_SEPARATE) strips.
//
// CTiffImg decoded each plane of a separated strip into the buffer one row apart
// while the deplaning loop -- and WriteLine() -- addressed planes one m_nStripSize
// apart, so every plane after the first was read from unwritten buffer space.
// Independently, the strip buffer was sized from the interleaved row width and
// then multiplied by the sample count again, growing with the square of
// SamplesPerPixel and overflowing the 32-bit size check on wide spectral images.
//
// The fixtures are authored with libtiff directly rather than through CTiffImg so
// that a reader defect cannot be masked by the writer that shares its layout code.

#include "TiffImg.h"

#include <cstdio>
#include <vector>

namespace {

const char *kPlanarPath = "tiff-separated-strips-planar.tif";
const char *kWidePath = "tiff-separated-strips-wide.tif";

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[tiff-separated-strips] FAIL: %s\n", what);
  }
}

unsigned char sampleValue(unsigned int sample, unsigned int row,
                          unsigned int column)
{
  return static_cast<unsigned char>((sample * 3u + row * 11u + column) & 0xffu);
}

bool setCommonFields(TIFF *tif, uint32_t width, uint32_t height,
                     uint16_t samples, uint32_t rowsPerStrip,
                     std::vector<uint16_t> &extras)
{
  extras.assign(samples > 0u ? samples - 1u : 0u, 0);
  if (!TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width) ||
      !TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height) ||
      !TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, static_cast<uint16_t>(8)) ||
      !TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples) ||
      !TIFFSetField(tif, TIFFTAG_PLANARCONFIG,
                    static_cast<uint16_t>(PLANARCONFIG_SEPARATE)) ||
      !TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,
                    static_cast<uint16_t>(PHOTOMETRIC_MINISBLACK)) ||
      !TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT,
                    static_cast<uint16_t>(SAMPLEFORMAT_UINT)) ||
      !TIFFSetField(tif, TIFFTAG_COMPRESSION,
                    static_cast<uint16_t>(COMPRESSION_NONE)) ||
      !TIFFSetField(tif, TIFFTAG_ORIENTATION,
                    static_cast<uint16_t>(ORIENTATION_TOPLEFT)) ||
      !TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip))
    return false;
  return TIFFSetField(tif, TIFFTAG_EXTRASAMPLES,
                      static_cast<uint16_t>(extras.size()),
                      extras.empty() ? NULL : extras.data()) == 1;
}

// 81 bands matches the 380-780 nm image used by the ICC ICS packages.
const unsigned int kBands = 81;
const uint32_t kWidth = 4;
// Keep the height indivisible by the multi-row strip cases so the last strip is
// shorter.  TIFF permits that layout; CTiffImg used to reject it because its
// per-sample strip count used floor division.
const uint32_t kHeight = 5;

bool writePlanar(uint32_t rowsPerStrip)
{
  TIFF *tif = TIFFOpen(kPlanarPath, "w");
  if (!tif)
    return false;

  std::vector<uint16_t> extras;
  bool ok = setCommonFields(tif, kWidth, kHeight,
                            static_cast<uint16_t>(kBands), rowsPerStrip, extras);
  std::vector<unsigned char> strip(kWidth * rowsPerStrip);
  for (unsigned int sample = 0; ok && sample < kBands; ++sample) {
    for (uint32_t row = 0; ok && row < kHeight; row += rowsPerStrip) {
      const uint32_t rowCount =
        rowsPerStrip < kHeight - row ? rowsPerStrip : kHeight - row;
      for (uint32_t stripRow = 0; stripRow < rowCount; ++stripRow)
        for (uint32_t column = 0; column < kWidth; ++column)
          strip[stripRow * kWidth + column] =
              sampleValue(sample, row + stripRow, column);
      ok = TIFFWriteEncodedStrip(tif,
                                 TIFFComputeStrip(tif, row,
                                                  static_cast<uint16_t>(sample)),
                                 strip.data(),
                                 static_cast<tmsize_t>(rowCount * kWidth)) >= 0;
    }
  }

  TIFFClose(tif);
  return ok;
}

// The plane advance is wrong for every RowsPerStrip, not only for strips holding
// more than one row, so cover both: at one row per strip the planes are packed
// contiguously and read back short, and above that they additionally alias.
void checkPlanarRoundTrip(uint32_t rowsPerStrip)
{
  char what[96];
  std::snprintf(what, sizeof(what),
                "ReadLine() preserves separated samples at %u row(s) per strip",
                rowsPerStrip);

  if (!writePlanar(rowsPerStrip)) {
    check(false, "author the separated fixture");
    return;
  }

  CTiffImg img;
  if (!img.Open(kPlanarPath)) {
    check(false, "Open() accepts the separated spectral fixture");
    return;
  }
  check(img.GetSamples() == kBands, "Open() preserves all 81 bands");
  check(img.GetBytesPerLine() == kWidth * kBands,
        "Open() reports the interleaved row size");

  std::vector<unsigned char> row(img.GetBytesPerLine());
  for (unsigned int y = 0; y < kHeight; ++y) {
    if (!img.ReadLine(row.data())) {
      check(false, "ReadLine() decodes a separated row");
      return;
    }
    for (unsigned int x = 0; x < kWidth; ++x) {
      for (unsigned int sample = 0; sample < kBands; ++sample) {
        if (row[x * kBands + sample] != sampleValue(sample, y, x)) {
          check(false, what);
          return;
        }
      }
    }
  }
  check(true, what);
}

// Sizing the strip buffer from the interleaved width made the allocation grow as
// SamplesPerPixel squared: this 8192-band image needs 512 KiB but was charged
// 4 GiB, which overflowed the 32-bit product and rejected the file outright.
void checkWideChannelCount()
{
  const unsigned int samples = 8192;
  const uint32_t width = 64;

  TIFF *tif = TIFFOpen(kWidePath, "w");
  if (!tif) {
    check(false, "author the wide separated fixture");
    return;
  }
  std::vector<uint16_t> extras;
  bool ok = setCommonFields(tif, width, 1u,
                            static_cast<uint16_t>(samples), 1u, extras);
  std::vector<unsigned char> strip(width);
  for (unsigned int sample = 0; ok && sample < samples; ++sample) {
    for (uint32_t column = 0; column < width; ++column)
      strip[column] = sampleValue(sample, 0u, column);
    ok = TIFFWriteEncodedStrip(tif,
                               TIFFComputeStrip(tif, 0,
                                                static_cast<uint16_t>(sample)),
                               strip.data(),
                               static_cast<tmsize_t>(strip.size())) >= 0;
  }
  TIFFClose(tif);
  if (!ok) {
    check(false, "author the wide separated fixture");
    return;
  }

  CTiffImg img;
  if (!img.Open(kWidePath)) {
    check(false,
          "Open() accepts a wide separated image instead of overflowing its size check");
    return;
  }
  check(true,
        "Open() accepts a wide separated image instead of overflowing its size check");

  // Read the row rather than stopping at a successful Open(): the smaller buffer
  // has to still hold every plane, so this would catch a sizing change that fits
  // the allocation but no longer matches the stride the deplaning loop uses.
  std::vector<unsigned char> row(img.GetBytesPerLine());
  if (!img.ReadLine(row.data())) {
    check(false, "ReadLine() decodes a wide separated row");
    return;
  }
  for (uint32_t column = 0; column < width; ++column) {
    for (unsigned int sample = 0; sample < samples; ++sample) {
      if (row[column * samples + sample] != sampleValue(sample, 0u, column)) {
        check(false, "ReadLine() preserves samples across a wide separated row");
        return;
      }
    }
  }
  check(true, "ReadLine() preserves samples across a wide separated row");
}

void cleanup()
{
  std::remove(kPlanarPath);
  std::remove(kWidePath);
}

} // namespace

int main()
{
  cleanup();

  checkPlanarRoundTrip(1u);
  checkPlanarRoundTrip(2u);
  checkPlanarRoundTrip(3u);
  checkWideChannelCount();

  cleanup();
  if (g_fail)
    std::fprintf(stderr, "[tiff-separated-strips] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[tiff-separated-strips] all assertions passed\n");

  return g_fail;
}
