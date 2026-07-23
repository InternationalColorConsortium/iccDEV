// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: heap-use-after-free in CIccTagJsonSparseMatrixArray::ToJson (#1791).
//
// A fuzz-crash profile carries a 'swp4' tag of type sparseMatrixArrayType whose
// sparse-matrix header is corrupt. CIccTagJsonSparseMatrixArray::ToJson() called
// CIccSparseMatrix::Reset() without checking its result and then walked the matrix
// via GetColumnsForRow()/GetRowOffset()/getPtr() -- all of which return pointers
// derived from the (corrupt) m_RowStart array. The row/value reads then
// dereferenced wild pointers: under ASAN the float read tripped heap-use-after-free
// (nlohmann to_json<float>), and in a plain build it silently emitted garbage.
//
// The XML serializer (CIccTagXmlSparseMatrixArray::ToXml) already guards this with
// `!Reset() || GetNumEntries() > GetMaxEntries() || !IsValid()` and refuses the tag
// ("Unable to output tag with type swp4"). The fix mirrors that guard in ToJson, so
// the corrupt tag is refused (ToJson returns false -> CIccProfileJson skips it)
// rather than walked.
//
// This test drives the same path the icc_tojson fuzzer hit -- Read() the profile
// from an in-memory buffer into a CIccProfileJson, then ToJson() -- and asserts the
// corrupt sparseMatrixArrayType tag is NOT serialized (refused). Before the fix the
// tag is walked and appears in the output (and trips ASAN under a sanitizer build);
// after the fix it is absent. Runs clean under ASAN+UBSAN.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccProfileJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccSparseMatrix.h"
#include "IccIO.h"

#include <cstdio>
#include <string>
#include <vector>

// The #1791 PoC (crash-a109c0deae961cbdd3d5d33c629c8b9550d8fcb5),
// sha256 57ea75a13c78cf1c08886c4314506f8d0fcf95a021dc5cdab92e8be5caeebe1b.
static const unsigned char kPoc[] = {
  0x00, 0x30, 0x2f, 0xfd, 0x54, 0x42, 0xd8, 0xcc, 0x04, 0xff, 0xff, 0xff,
  0x14, 0x00, 0x07, 0x25, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0x00, 0x4c,
  0x40, 0x00, 0x00, 0x00, 0x29, 0x00, 0xff, 0x39, 0x40, 0x60, 0x00, 0xfd,
  0x61, 0x63, 0x73, 0x70, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
  0x62, 0x62, 0x62, 0x4c, 0x62, 0x61, 0x61, 0xc5, 0xff, 0x00, 0x00, 0x00,
  0x85, 0x00, 0x00, 0xfe, 0x9d, 0x9d, 0xff, 0xff, 0x8a, 0x8d, 0x0d, 0x14,
  0x53, 0xf6, 0xf6, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6e, 0x6c, 0xc3, 0x78,
  0x3a, 0xb0, 0x03, 0xff, 0x00, 0x00, 0x63, 0x01, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x07, 0x17, 0x52, 0x4c, 0x43, 0x32, 0x00, 0x00, 0x40, 0xad,
  0x62, 0x62, 0x62, 0x62, 0x62, 0xcc, 0x00, 0x00, 0x61, 0x4f, 0xfb, 0x00,
  0x00, 0x00, 0x86, 0x00, 0x00, 0x02, 0x62, 0x62, 0x00, 0x00, 0x00, 0x01,
  0x73, 0x77, 0x70, 0x34, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x00, 0x00, 0x5d,
  0x74, 0x00, 0x73, 0x63, 0xc2, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6,
  0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xf9, 0xff,
  0x28, 0xff, 0xff, 0xd5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xfb, 0xf9, 0xff, 0x28, 0xff, 0xff, 0xd2, 0xff, 0xff,
  0x00, 0x35, 0x44, 0x61, 0x74, 0x61, 0x43, 0x6f, 0x6c, 0x6f, 0x75, 0x72,
  0x53, 0x70, 0x61, 0x63, 0x65, 0x00, 0x01, 0xdf, 0x00, 0x34, 0x43, 0x00,
  0x50, 0x69, 0x43, 0x6c, 0x66, 0x6f, 0x49, 0x72, 0x00, 0x7f, 0x00, 0x00,
  0x26, 0x00, 0x00, 0x29, 0x00, 0x10, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x30,
  0xff, 0x63, 0x6c, 0x75, 0xfd, 0x00, 0x70, 0x73, 0x6d, 0x61, 0x74, 0x63,
  0x00, 0x68, 0x6d, 0x87, 0x72, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x01, 0x00, 0xff, 0xff, 0x14, 0x00, 0x07, 0x25, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xf7, 0x00, 0x4c, 0x40, 0x00, 0x00, 0x00, 0x29, 0x00, 0xff, 0x39,
  0x40, 0x60, 0x00, 0xfd, 0x61, 0x63, 0x73, 0x70, 0xcd, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x20, 0x62, 0x62, 0x62, 0x4c, 0x62, 0x61, 0x61, 0xc5,
  0xff, 0x00, 0x00, 0x00, 0x85, 0x00, 0x00, 0xfe, 0x9d, 0x9d, 0xff, 0xff,
  0x8a, 0x8d, 0x0d, 0x14, 0x53, 0xf6, 0xf6, 0xfb, 0x00, 0x00, 0x50, 0xff,
  0xff, 0x00, 0xf7, 0x00, 0xa2, 0x30,
};

static bool has(const std::string &haystack, const char *needle)
{
  return haystack.find(needle) != std::string::npos;
}

int main()
{
  int fail = 0;

  // Register the JSON tag/MPE factories, exactly as the IccToJson CLI and the
  // icc_tojson fuzzer do -- without them a loaded tag has no CIccTagJson
  // extension and CIccProfileJson::ToJson silently skips it, so the corrupt
  // sparse-matrix tag would never be walked (the bug would not reproduce).
  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  // Read the PoC into a CIccProfileJson from memory (Attach wants a non-const,
  // long-lived buffer), mirroring the icc_tojson fuzzer / IccToJson CLI path.
  std::vector<icUInt8Number> bytes(kPoc, kPoc + sizeof(kPoc));
  CIccMemIO io;
  if (!io.Attach(bytes.data(), bytes.size())) {
    std::printf("FAIL: could not attach PoC buffer\n");
    return 1;
  }

  CIccProfileJson profile;
  if (!profile.Read(&io)) {
    // The corrupt profile still parses far enough for the fuzzer to reach ToJson;
    // if a future hardening rejects it earlier that is fine (the tag can't be
    // walked at all), so treat a read failure as a pass.
    std::printf("PASS: profile rejected at Read (corrupt tag never reaches ToJson)\n");
    return 0;
  }

  std::string json;
  const bool ok = profile.ToJson(json, 2);

  // ToJson must complete (it skips the un-serializable tag, it does not abort the
  // whole document), and the corrupt sparse-matrix tag must be REFUSED, not walked.
  if (!ok) {
    std::printf("FAIL: profile ToJson returned false (should skip the bad tag, not abort)\n");
    fail = 1;
  }
  if (has(json, "sparseMatrixArrayType")) {
    std::printf("FAIL: corrupt sparseMatrixArrayType tag was walked/serialized (pre-fix behavior)\n");
    fail = 1;
  }

  // ---- IsValid() must not itself over-read on an oversized intermediate row-start
  // #1792 review: the terminal row-start (GetNumEntries()) is checked against
  // GetMaxEntries(), but an *intermediate* row-start was not -- and IsValid()
  // used it directly as the column-walk loop bound. So a row-start like {0, 20, 5}
  // (oversized intermediate 20, small terminal 5) with strictly-increasing,
  // in-range column indices filling the buffer drives the m_ColumnIndices[] read
  // past the allocation on row 0, before the descending-offset check on row 1 can
  // reject it -- meaning the guard added to ToJson would still over-read *inside*
  // IsValid(). The hardened IsValid() bounds each row-start against m_nMaxEntries
  // first. Build that matrix directly in an exact-size heap buffer (so ASAN's
  // redzone catches any over-read) and require IsValid() to reject it cleanly.
  // Reverting the IsValid() hardening makes this over-read m_ColumnIndices
  // (heap-buffer-overflow at IccSparseMatrix.cpp IsValid()).
  {
    std::vector<unsigned char> m(48, 0);       // exact-size heap alloc -> ASAN redzone past end
    m[0] = 2; m[1] = 0;                         // nRows = 2 (Reset reads these native-endian)
    m[2] = 0xA0; m[3] = 0x0F;                   // nCols = 4000 (>= every column index below)
    m[4] = 0; m[5] = 0;                         // rowStart[0] = 0
    m[6] = 20; m[7] = 0;                        // rowStart[1] = 20  (oversized intermediate)
    m[8] = 5; m[9] = 0;                         // rowStart[2] = 5   (small terminal)
    for (size_t k = 0; 10 + k * 2 + 1 < m.size(); ++k) {
      m[10 + k * 2] = (unsigned char)k;         // column indices 0,1,2,... (strictly increasing, in range)
      m[10 + k * 2 + 1] = 0;
    }

    CIccSparseMatrix mtx;
    const bool reset = mtx.Reset(m.data(), m.size(), icSparseMatrixFloatNum, true);
    if (!reset) {
      std::printf("FAIL: Reset should succeed for in-range dims\n");
      fail = 1;
    }
    if (mtx.IsValid()) {
      std::printf("FAIL: IsValid must reject oversized intermediate row-start {0,20,5}\n");
      fail = 1;
    }
    std::printf("  IsValid {0,20,5}: reset=%d valid=%d (expect reset=1 valid=0; revert over-reads)\n",
                (int)reset, (int)mtx.IsValid());
  }

  if (fail) {
    std::printf("RESULT: FAIL\n");
    return 1;
  }
  std::printf("RESULT: PASS (corrupt sparse-matrix tag refused; no walk)\n");
  return 0;
}
