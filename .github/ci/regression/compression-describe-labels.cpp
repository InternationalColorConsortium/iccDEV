// Regression: compressed-tag Describe() labelling (#1723 Compression Paths).
//
// Two output-only defects on the compression dump path, both in IccTagBasic.cpp:
//
//  1. CIccTagData::Describe() appended a "Compressed " marker for a
//     compressed-flagged data tag, then the very next line reassigned
//     sDescription with `=` (the data-kind label), silently discarding the
//     marker. So a compressed data tag was never labelled "Compressed" in any
//     dump, on either an ICC_USE_ZLIB or a no-zlib build (Describe does not
//     inflate; it only formats m_pData). The fix appends the label instead of
//     overwriting. This assertion is zlib-agnostic.
//
//  2. CIccTagZipUtf8Text::Describe() on a *no-zlib* build emitted the marker
//     "BEGIN_COMPESSED_DATA[\"" (missing the R, and an unbalanced ["...]
//     delimiter) while the closing line spelled "END_COMPRESSED_DATA". The fix
//     spells it "BEGIN_COMPRESSED_DATA" and balances the quotes. That branch is
//     compiled out when ICC_USE_ZLIB is defined, so the string assertion below
//     is guarded to the no-zlib configuration; in a zlib build only defect (1)
//     is exercised.
//
// Building the tags in memory (SetDataType/SetSize/GetData) needs no zlib, so
// this test runs in every build configuration.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "IccTag.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ std::printf("FAIL line %d: %s\n", __LINE__, #c); g_fail=1; } } while(0)

// True iff `haystack` contains `needle`.
static bool has(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

int main()
{
  // ---- Defect 1: the "Compressed " marker must survive into the dump ----------
  // A compressed *binary* data tag: IsTypeCompressed() and IsTypeBinary() are both
  // true (icCompressedData is a bit above icDataTypeMask).
  {
    CIccTagData tag;
    tag.SetDataType(icCompressedBinaryData);   // icCompressedData | icBinaryData
    CHECK(tag.IsTypeCompressed());
    CHECK(tag.IsTypeBinary());
    CHECK(tag.SetSize(4, false));
    tag[0] = 0xde; tag[1] = 0xad; tag[2] = 0xbe; tag[3] = 0xef;

    std::string d;
    tag.Describe(d, 100);
    CHECK(has(d, "Compressed"));       // the marker that used to be clobbered
    CHECK(has(d, "Binary Data:"));     // the data-kind label still present
  }

  // A compressed *ascii* data tag must likewise be labelled "Compressed".
  {
    CIccTagData tag;
    tag.SetDataType(icCompressedAsciiData);    // icCompressedData | icAsciiData
    CHECK(tag.IsTypeCompressed());
    CHECK(tag.IsTypeAscii());
    CHECK(tag.SetSize(3, false));
    tag[0] = 'a'; tag[1] = 'b'; tag[2] = 'c';

    std::string d;
    tag.Describe(d, 100);
    CHECK(has(d, "Compressed"));
    CHECK(has(d, "Ascii Data:"));
  }

  // Control: an *uncompressed* data tag must NOT be labelled "Compressed".
  {
    CIccTagData tag;
    tag.SetDataType(icBinaryData);
    CHECK(!tag.IsTypeCompressed());
    CHECK(tag.SetSize(2, false));
    tag[0] = 0x00; tag[1] = 0x01;

    std::string d;
    tag.Describe(d, 100);
    CHECK(!has(d, "Compressed"));
    CHECK(has(d, "Binary Data:"));
  }

  // ---- Defect 2: no-zlib zut8/zxml marker is spelled/balanced correctly --------
#ifndef ICC_USE_ZLIB
  {
    // On a no-zlib build, Describe() dumps the raw compressed bytes wrapped in a
    // BEGIN_COMPRESSED_DATA["<n>"] ... END_COMPRESSED_DATA marker.
    CIccTagZipUtf8Text tag;
    std::string d;
    tag.Describe(d, 100);
    CHECK(has(d, "BEGIN_COMPRESSED_DATA"));   // was the typo "BEGIN_COMPESSED_DATA"
    CHECK(has(d, "END_COMPRESSED_DATA"));
    CHECK(!has(d, "COMPESSED"));              // the misspelling must be gone
  }
#else
  std::printf("note: ICC_USE_ZLIB set; defect-2 (no-zlib marker) branch not exercised\n");
#endif

  if (g_fail) {
    std::printf("RESULT: FAIL\n");
    return 1;
  }
  std::printf("RESULT: PASS (compressed-tag Describe labelling invariants held)\n");
  return 0;
}
