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

#include "TiffImg.h"
#include "IccCmmConfig.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

namespace {
const uint32_t width = 35, height = 37, tileWidth = 16, tileHeight = 32;
int failures = 0;

void check(bool ok, const char *message)
{
  if (!ok) {
    std::fprintf(stderr, "[FAIL] %s\n", message);
    failures++;
  }
}

void sample(unsigned char *dst, unsigned int bits, unsigned int x,
            unsigned int y, unsigned int s)
{
  const unsigned int value = (x * 7 + y * 13 + s * 31) % 251;
  if (bits == 8)
    *dst = static_cast<unsigned char>(value);
  else if (bits == 16) {
    const uint16_t v = static_cast<uint16_t>(value * 257);
    std::memcpy(dst, &v, sizeof(v));
  }
  else {
    const float v = static_cast<float>(value) / 256.0f;
    std::memcpy(dst, &v, sizeof(v));
  }
}

bool fields(TIFF *tif, unsigned int bits, unsigned int samples, bool separate,
            bool compress, bool tiled, const char *icc = NULL)
{
  std::vector<uint16_t> extras(samples == 81 ? 80 : samples == 4 ? 1 : 0,
                               samples == 4 ? EXTRASAMPLE_UNASSALPHA : EXTRASAMPLE_UNSPECIFIED);
  return TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width) &&
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height) &&
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples) &&
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits) &&
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, bits == 32 ? SAMPLEFORMAT_IEEEFP : SAMPLEFORMAT_UINT) &&
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC,
                 samples == 1 || samples == 81 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB) &&
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, separate ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG) &&
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compress ? COMPRESSION_LZW : COMPRESSION_NONE) &&
    (extras.empty() || TIFFSetField(tif, TIFFTAG_EXTRASAMPLES,
                                   static_cast<uint16_t>(extras.size()), extras.data())) &&
    (tiled ? TIFFSetField(tif, TIFFTAG_TILEWIDTH, tileWidth) &&
             TIFFSetField(tif, TIFFTAG_TILELENGTH, tileHeight) :
             TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1)) &&
    (!icc || TIFFSetField(tif, TIFFTAG_ICCPROFILE, static_cast<uint32_t>(std::strlen(icc)), icc));
}

bool rawTiles(TIFF *tif, unsigned int bits, unsigned int samples, bool separate,
              bool shortTile = false)
{
  const unsigned int bytes = bits / 8;
  const unsigned int planes = separate ? samples : 1;
  std::vector<unsigned char> tile(static_cast<size_t>(TIFFTileSize(tif)));
  for (unsigned int s = 0; s < planes; s++) {
    for (unsigned int y = 0; y < height; y += tileHeight) {
      for (unsigned int x = 0; x < width; x += tileWidth) {
        std::fill(tile.begin(), tile.end(), 0);
        for (unsigned int r = 0; r < tileHeight && y + r < height; r++) {
          for (unsigned int c = 0; c < tileWidth && x + c < width; c++) {
            const unsigned int channels = separate ? 1 : samples;
            for (unsigned int k = 0; k < channels; k++)
              sample(tile.data() + ((r * tileWidth + c) * channels + k) * bytes,
                     bits, x + c, y + r, separate ? s : k);
          }
        }
        const tmsize_t size = shortTile ? 1 : static_cast<tmsize_t>(tile.size());
        if (TIFFWriteEncodedTile(tif, TIFFComputeTile(tif, x, y, 0, static_cast<uint16_t>(s)),
                                 tile.data(), size) != size)
          return false;
      }
    }
  }
  return true;
}

bool createRaw(const char *path, const char *mode, unsigned int bits,
               unsigned int samples, bool separate, bool compress, bool shortTile = false,
               const char *icc = NULL)
{
  TIFF *tif = TIFFOpen(path, mode);
  if (!tif)
    return false;
  const bool ok = fields(tif, bits, samples, separate, compress, true, icc) &&
                  rawTiles(tif, bits, samples, separate, shortTile) && TIFFWriteDirectory(tif);
  TIFFClose(tif);
  return ok;
}

bool createBilevel(const char *path)
{
  TIFF *tif = TIFFOpen(path, "wl");
  if (!tif)
    return false;
  bool ok = fields(tif, 1, 1, false, false, true) &&
            TIFFUnsetField(tif, TIFFTAG_BITSPERSAMPLE);
  std::vector<unsigned char> tile(static_cast<size_t>(TIFFTileSize(tif)), 0);
  for (ttile_t index = 0; ok && index < TIFFNumberOfTiles(tif); index++)
    ok = TIFFWriteEncodedTile(tif, index, tile.data(), tile.size()) ==
         static_cast<tmsize_t>(tile.size());
  ok = ok && TIFFWriteDirectory(tif);
  TIFFClose(tif);
  return ok;
}

bool verifyRaw(const char *path, unsigned int bits, unsigned int samples)
{
  TIFF *tif = TIFFOpen(path, "r");
  if (!tif)
    return false;
  uint16_t planar = 0;
  TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planar);
  const bool separate = planar == PLANARCONFIG_SEPARATE;
  const unsigned int channels = separate ? 1 : samples, bytes = bits / 8;
  std::vector<unsigned char> tile(static_cast<size_t>(TIFFTileSize(tif)));
  unsigned char expected[4] = {};
  bool ok = true;
  for (unsigned int s = 0; s < (separate ? samples : 1); s++) {
    for (unsigned int y = 0; y < height; y += tileHeight) {
      for (unsigned int x = 0; x < width; x += tileWidth) {
        if (TIFFReadEncodedTile(tif, TIFFComputeTile(tif, x, y, 0, static_cast<uint16_t>(s)),
                                tile.data(), static_cast<tmsize_t>(tile.size())) !=
            static_cast<tmsize_t>(tile.size())) {
          ok = false;
          continue;
        }
        for (unsigned int r = 0; r < tileHeight; r++) {
          for (unsigned int c = 0; c < tileWidth; c++) {
            for (unsigned int k = 0; k < channels; k++) {
              std::memset(expected, 0, sizeof(expected));
              if (y + r < height && x + c < width)
                sample(expected, bits, x + c, y + r, separate ? s : k);
              ok &= std::memcmp(expected,
                tile.data() + ((r * tileWidth + c) * channels + k) * bytes, bytes) == 0;
            }
          }
        }
      }
    }
  }
  TIFFClose(tif);
  return ok;
}

bool multiFixture(const char *path)
{
  TIFF *tif = TIFFOpen(path, "w8l");
  if (!tif)
    return false;
  uint64_t subOffset = 0;
  bool ok = fields(tif, 8, 3, false, false, true, "malformed-first-icc") &&
            TIFFSetField(tif, TIFFTAG_SUBIFD, 1, &subOffset) &&
            rawTiles(tif, 8, 3, false) && TIFFWriteDirectory(tif) &&
            fields(tif, 8, 3, false, false, true, "malformed-sub-icc") &&
            rawTiles(tif, 8, 3, false) && TIFFWriteDirectory(tif) &&
            fields(tif, 16, 4, false, false, false, "malformed-second-icc") &&
            TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_BOTLEFT);
  std::vector<unsigned char> line(width * 4 * 2, 0);
  for (unsigned int y = 0; ok && y < height; y++)
    ok = TIFFWriteScanline(tif, line.data(), y, 0) == 1;
  ok &= TIFFWriteDirectory(tif) == 1;
  TIFFClose(tif);
  return ok;
}
}

int main(int argc, char **argv)
{
  if (argc == 4 && std::strcmp(argv[1], "--compare") == 0) {
    CTiffImg a, b;
    if (!a.Open(argv[2]) || !b.Open(argv[3]) ||
        a.GetWidth() != b.GetWidth() || a.GetHeight() != b.GetHeight() ||
        a.GetSamples() != b.GetSamples() || a.GetBitsPerSample() != b.GetBitsPerSample())
      return 1;
    std::vector<unsigned char> x(a.GetBytesPerLine()), y(b.GetBytesPerLine());
    for (unsigned int row = 0; row < a.GetHeight(); row++) {
      if (!a.ReadLine(x.data()) || !b.ReadLine(y.data()) || x != y)
        return 1;
    }
    return 0;
  }
  if (argc == 3 && std::strcmp(argv[1], "--fixtures") == 0) {
    const std::filesystem::path dir(argv[2]);
    bool ok = createRaw((dir / "tiles.tif").string().c_str(), "wl", 8, 3, false, false) &&
              createRaw((dir / "malformed-classic.tif").string().c_str(), "wl", 8, 3,
                        false, false, false, "malformed-classic-icc") &&
              multiFixture((dir / "multi.tif").string().c_str()) &&
              createBilevel((dir / "bilevel.tif").string().c_str());
    for (unsigned int bits : {8u, 16u, 32u}) {
      for (unsigned int channel : {1u, 2u, 3u}) {
        const std::string name = "gray" + std::to_string(bits) + "_" + std::to_string(channel);
        ok &= createRaw((dir / name).string().c_str(), "wl", bits, 1, false, false);
      }
    }
    return ok ? 0 : 1;
  }
  for (unsigned int bits : {8u, 16u, 32u}) {
    for (unsigned int samples : {1u, 3u, 4u, 81u}) {
      for (bool separate : {false, true}) {
        for (bool compress : {false, true}) {
          for (const char *mode : {"wl", "wb", "w8l", "w8b"}) {
            check(createRaw("tile-input.tif", mode, bits, samples, separate, compress),
                  "independent libtiff tile fixture");
            CTiffImg input, output;
            if (!input.Open("tile-input.tif")) {
              check(false, "tiled input open");
              continue;
            }
            check(output.Create("tile-output.tif", width, height, bits,
                  samples == 1 || samples == 81 ? PHOTO_MINISBLACK : PHOTO_RGB, samples,
                  samples == 81 ? 80 : samples == 4 ? 1 : 0, 96, 96, compress, separate, RESUNIT_INCH,
                  tileWidth, tileHeight, mode[1] == '8'), "tiled output create");
            std::vector<unsigned char> line(input.GetBytesPerLine());
            unsigned char expected[4] = {};
            for (unsigned int y = 0; y < height; y++) {
              check(input.ReadLine(line.data()), "tile row read");
              for (unsigned int x = 0; x < width; x++) {
                for (unsigned int s = 0; s < samples; s++) {
                  sample(expected, bits, x, y, s);
                  check(std::memcmp(expected, line.data() + (x * samples + s) * (bits / 8),
                                    bits / 8) == 0, "interleaved sample parity");
                }
              }
              check(output.WriteLine(line.data()), "tile row write");
            }
            check(!input.ReadLine(line.data()), "read past image fails");
            check(!output.WriteLine(line.data()), "write past image fails");
            check(output.Close(), "tiled output finalize");
            check(verifyRaw("tile-output.tif", bits, samples), "independent decode and zero edge padding");
          }
        }
      }
    }
  }
  CTiffImg image;
  check(!image.Create("invalid-tile.tif", width, height, 8, PHOTO_RGB, 3, 0,
                     96, 96, false, false, RESUNIT_INCH, 17, 16), "invalid tile geometry rejected");
  check(!image.Create("oversized-tile.tif", 262144, 1, 8, PHOTO_RGB, 3, 0,
                     96, 96, false, false, RESUNIT_INCH, 16, 4096), "tile band memory cap");
  check(!image.WasOutputOpened() && !std::filesystem::exists("oversized-tile.tif"),
        "oversized tile band rejected before output creation");
  check(!image.Create("oversized-tile.tif", 16, UINT32_MAX, 8, PHOTO_MINISBLACK, 81, 80,
                     96, 96, false, true, RESUNIT_INCH, 16, 16), "tile count overflow rejected");
  check(!image.WasOutputOpened() && !std::filesystem::exists("oversized-tile.tif"),
        "tile count overflow rejected before output creation");
  check(createRaw("short-tile.tif", "wl", 8, 3, false, false, true), "short tile fixture");
  check(image.Open("short-tile.tif"), "short payload opens directory");
  std::vector<unsigned char> line(width * 3);
  check(!image.ReadLine(line.data()), "short tile payload fails read");
  check(image.Open("short-tile.tif", true), "metadata does not decode short tile");
  check(!image.ReadLine(line.data()), "metadata mode cannot read pixels");
  check(multiFixture("multi.tif"), "mixed BigTIFF fixture");
  check(image.Open("multi.tif", true, 1) && image.IsBigTiff() &&
        !image.IsTiled() && image.GetDirectoryCount() == 2 &&
        image.HasStoredExtraSamplesCount() && image.GetStoredExtraSamplesCount() == 1,
        "selected BigTIFF metadata and exact ExtraSamples");
  check(!image.Open("multi.tif", true, 2), "missing IFD rejected");
  image.Close();
  check(!image.IsTiled() && !image.GetTileWidth(), "close resets tile state");
  check(image.Create("incomplete.tif", width, height, 8, PHOTO_RGB, 3, 0,
                     96, 96, false, false, RESUNIT_INCH, 16, 32), "incomplete output create");
  check(!image.Close(), "incomplete tiled output fails finalize");
  CIccCfgImageApply cfg;
  check(cfg.fromJson("{\"dstTileWidth\":16,\"dstTileHeight\":32,\"dstBigTiff\":true}", true),
        "tile config accepts complete geometry");
  json encoded;
  cfg.toJson(encoded);
  CIccCfgImageApply roundtrip;
  check(roundtrip.fromJson(encoded, true) && roundtrip.m_dstTileWidth == 16 &&
        roundtrip.m_dstTileHeight == 32 && roundtrip.m_dstBigTiff, "tile config roundtrip");
  check(cfg.fromJson("{\"dstTileWidth\":16.0,\"dstTileHeight\":3.2e1}", true) &&
        cfg.m_dstTileWidth == 16 && cfg.m_dstTileHeight == 32,
        "schema integral numbers accepted");
  cfg.toJson(encoded);
  check(encoded["dstTileWidth"].is_number_unsigned() &&
        encoded["dstTileHeight"].is_number_unsigned(), "tile dimensions serialize as integers");
  check(cfg.fromJson("{\"dstTileWidth\":4294967280.0,\"dstTileHeight\":16}", true) &&
        cfg.m_dstTileWidth == 4294967280u, "largest uint32 tile multiple accepted");
  check(cfg.fromJson("{\"dstTileWidth\":0.0,\"dstTileHeight\":0.0}", true) &&
        !cfg.m_dstTileWidth && !cfg.m_dstTileHeight, "integral zero selects strips");
  for (const char *bad : {"{\"dstTileWidth\":16}", "{\"dstTileWidth\":-16,\"dstTileHeight\":16}",
       "{\"dstTileWidth\":16.5,\"dstTileHeight\":16}", "{\"dstTileWidth\":4294967296}",
       "{\"dstTileWidth\":4294967296.0,\"dstTileHeight\":16}",
       "{\"dstTileWidth\":true,\"dstTileHeight\":16}",
       "{\"dstTileWidth\":null,\"dstTileHeight\":16}",
       "{\"dstTileWidth\":\"16\",\"dstTileHeight\":16}",
       "{\"dstTileWidth\":17,\"dstTileHeight\":16}", "{\"dstBigTiff\":\"true\"}"})
    check(!cfg.fromJson(bad, true), "invalid config fails closed");
  for (double invalid : {std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
    json nonfinite = {{"dstTileWidth", invalid}, {"dstTileHeight", 16}};
    check(!cfg.fromJson(nonfinite, true), "nonfinite dimensions rejected before cast");
  }
  cfg.reset();
  check(!cfg.m_dstTileWidth && !cfg.m_dstTileHeight && !cfg.m_dstBigTiff, "config reset");
  for (const char *path : {"tile-input.tif", "tile-output.tif", "invalid-tile.tif",
                          "short-tile.tif", "multi.tif", "incomplete.tif", "oversized-tile.tif"})
    std::remove(path);
  std::printf("%s TIFF tile regression (%d failures)\n", failures ? "[FAIL]" : "[OK]", failures);
  return failures ? 1 : 0;
}
