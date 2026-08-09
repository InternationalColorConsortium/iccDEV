// Regression for the CTiffImg::Create() integer-overflow gap harvested from ci-qa-flags
// (#1853).
//
// CTiffImg computes bytes-per-line twice, once per direction, and only one of them was
// checked. Open() routes it through calcBytesPerLine() (TiffImg.cpp:471, :489), which
// multiplies in 64 bits and refuses a result that will not fit in unsigned int.
// Create() did the same sums raw:
//
//   m_nBytesPerStripLine = m_nWidth * m_nBytesPerSample;
//   m_nBytesPerLine      = m_nWidth * m_nBytesPerSample * m_nSamples;
//   m_nStripSize         = (unsigned int)TIFFStripSize(m_hTif);   // tmsize_t is int64
//
// Why the wrap is reachable and not theoretical: Create() runs before any pixel data is
// read (iccApplyProfiles.cpp:508), so a hostile source TIFF only has to *declare* a large
// ImageWidth -- no matching pixel data has to exist on disk. The caller then sizes its
// destination row buffer from GetBytesPerLine() (iccApplyProfiles.cpp:539) and writes a
// full row through it.
//
// The large strip allocation in the same branch looks like it would stop this by failing,
// and does not: it asks for the true (untruncated) size, which Linux overcommit happily
// grants, so Create() returned true while reporting a bytes-per-line far smaller than one
// row.
//
// SCOPE, stated plainly: this test asserts the REJECTION contract -- Create() must refuse
// parameters whose bytes-per-line does not fit in unsigned int -- not the heap overflow
// that follows from accepting them. Driving the overflow itself would mean writing a
// multi-gigabyte row buffer, which is not something to do on a CI runner.
//
// On a machine where the 6 GB strip allocation genuinely fails (no overcommit, or a hard
// ulimit), unfixed Create() also returns false, and case 1 below passes without the fix.
// That is why case 2 exists: it overflows bytes-per-line while keeping every allocation
// tiny, so nothing but the range check can reject it.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "TiffImg.h"

#include <cstdio>
#include <cstdlib>

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[tiff-create-overflow] FAIL: %s\n", what);
  }
}

const char *kOutPath = "tiff-create-overflow-should-not-exist.tif";

void cleanup()
{
  std::remove(kOutPath);
}

// Create() with parameters whose bytes-per-line overflows unsigned int must fail.
// bSep = true and nSamples > 1 selects the separated branch, which is where the
// width * bytesPerSample * samples product lives.
bool tryCreate(unsigned int nWidth, unsigned int nBPS, unsigned int nSamples,
               bool bSep)
{
  CTiffImg img;
  const bool ok = img.Create(kOutPath, nWidth, /*nHeight=*/4, nBPS,
                             PHOTO_MINISBLACK, nSamples, /*nExtraSamples=*/0,
                             72.0f, 72.0f, /*bCompress=*/false, bSep);
  img.Close();
  cleanup();
  return ok;
}

} // namespace

int main()
{
  // --- 1. The reported shape: a large declared width and a many-channel separated
  // destination. 3e8 * 1 * 20 = 6e9, which wraps to about 1.7e9 in 32 bits.
  {
    const bool ok = tryCreate(300000000u, 8u, 20u, /*bSep=*/true);
    check(!ok, "Create() rejects width 3e8 x 1 byte x 20 samples (6e9 bytes per line)");
  }

  // --- 2. The same overflow with every allocation kept small, so no allocation failure
  // can stand in for the range check. 2^31 * 2 bytes * 2 samples = 2^33, which wraps to
  // exactly 0 in 32 bits -- and a zero bytes-per-line is precisely the value that makes a
  // caller's row buffer allocation meaningless.
  {
    const bool ok = tryCreate(2147483648u, 16u, 2u, /*bSep=*/true);
    check(!ok, "Create() rejects a bytes-per-line product that wraps to zero");
  }

  // --- 3. A legitimate small image must still be created, so the guards above are not
  // simply refusing everything.
  {
    CTiffImg img;
    const bool ok = img.Create(kOutPath, 64u, 4u, 8u, PHOTO_MINISBLACK, 3u, 0u,
                               72.0f, 72.0f, false, /*bSep=*/false);
    check(ok, "Create() still accepts a 64x4 8-bit 3-channel image");
    if (ok) {
      // 64 px * 1 byte * 3 samples, contiguous.
      check(img.GetBytesPerLine() == 64u * 3u,
            "GetBytesPerLine() is the true row size for the accepted image");
    }
    img.Close();
    cleanup();
  }

  // --- 4. The separated path must also still work for a sane image, since that is the
  // branch the guards were added to.
  {
    CTiffImg img;
    const bool ok = img.Create(kOutPath, 64u, 4u, 8u, PHOTO_MINISBLACK, 3u, 0u,
                               72.0f, 72.0f, false, /*bSep=*/true);
    check(ok, "Create() still accepts a 64x4 8-bit 3-channel separated image");
    if (ok) {
      check(img.GetBytesPerLine() == 64u * 3u,
            "separated GetBytesPerLine() is the true row size");
    }
    img.Close();
    cleanup();
  }

  if (g_fail)
    std::fprintf(stderr, "[tiff-create-overflow] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[tiff-create-overflow] all assertions passed\n");

  return g_fail;
}
