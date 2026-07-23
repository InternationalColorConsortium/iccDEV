// Regression: IccPawgReport compression-path measurement (S14, issue #1775).
//
// Follow-up to #1774/#1723: the PAWG report classified tags by *signature* and
// never noticed when a tag's *type* was DEFLATE-compressed (zut8/zxml/compressed
// data). PawgCompressionVerdict() is the new measurement, exercised here through
// its in-memory entry point -- the same "up to the point of Output, not for
// Output" boundary the overnight fuzzing harness uses.
//
// Detection is raw-bytes based (a tag's 4-byte TYPE signature at its data offset,
// plus the icCompressedData flag word at data+8 for a 'data' tag), so this test
// hand-builds minimal profile images rather than constructing full profiles.
//
// The measurement is build-independent (compression On/Off, "not gzip"); only the
// VERDICT differs: a compressed tag is Ok when zlib is linked (content
// assessable) and Gap otherwise (content retained but not decodable here). The
// detection assertions below hold in every configuration; the verdict-value
// assertions self-gate on ICC_USE_ZLIB.
//
// Exit code 0 = pass, 1 = a case regressed.
#include "PawgReport.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ std::printf("FAIL line %d: %s\n", __LINE__, #c); g_fail=1; } } while(0)

// Tag TYPE signatures (byte-for-byte, per icProfileHeader.h) exercised here.
static const uint32_t kZut8 = 0x7a757438u;  // zipUtf8Text
static const uint32_t kZxml = 0x7a786d6cu;  // zipXml
static const uint32_t kData = 0x64617461u;  // 'data'
static const uint32_t kText = 0x74657874u;  // 'text' (uncompressed control)
static const uint32_t kCompressedDataFlag = 0x00010000u;  // icCompressedData

static void putU32BE(std::vector<uint8_t> &b, size_t off, uint32_t v)
{
  b[off + 0] = (uint8_t)(v >> 24);
  b[off + 1] = (uint8_t)(v >> 16);
  b[off + 2] = (uint8_t)(v >> 8);
  b[off + 3] = (uint8_t)(v);
}

// Build a minimal single-tag ICC image: 128-byte header ('acsp' magic), a
// one-entry tag table, and tag data beginning with `typeSig`. When `dataFlag` is
// nonzero the tag data also carries a flags word at +8 (for the 'data' case).
static std::vector<uint8_t> makeProfile(uint32_t tagSig, uint32_t typeSig, uint32_t dataFlag)
{
  const size_t tagOffset = 144;                      // 132 + 12 (one table entry)
  const size_t tagDataLen = dataFlag ? 12 : 4;       // need +8 flags word only for 'data'
  const size_t total = tagOffset + tagDataLen;

  std::vector<uint8_t> b(total, 0);
  putU32BE(b, 0, (uint32_t)total);                   // profile size
  b[36] = 'a'; b[37] = 'c'; b[38] = 's'; b[39] = 'p'; // header magic
  putU32BE(b, 128, 1);                               // tag count = 1
  putU32BE(b, 132, tagSig);                          // tag signature
  putU32BE(b, 136, (uint32_t)tagOffset);             // tag data offset
  putU32BE(b, 140, (uint32_t)tagDataLen);            // tag data size
  putU32BE(b, tagOffset, typeSig);                   // tag TYPE signature
  if (dataFlag)
    putU32BE(b, tagOffset + 8, dataFlag);            // [type][reserved][flags]
  return b;
}

static bool contains(const std::string &s, const char *needle)
{
  return s.find(needle) != std::string::npos;
}

// A compressed tag: Ok when zlib is linked, Gap otherwise.
static int expectedCompressedVerdict()
{
#ifdef ICC_USE_ZLIB
  return kPawgOk;
#else
  return kPawgGap;
#endif
}

// Assert the measurement flags a buffer as carrying a compressed tag.
static void checkCompressed(const std::vector<uint8_t> &buf, const char *label, const char *sigText)
{
  std::string detail;
  int v = PawgCompressionVerdict(buf.data(), buf.size(), &detail);
  CHECK(v == expectedCompressedVerdict());
  CHECK(contains(detail, "compressed tag"));   // "... N compressed tag(s): ..."
  CHECK(contains(detail, sigText));            // names the offending type
  CHECK(!contains(detail, "no DEFLATE"));      // must NOT read as the empty case
  std::printf("  %-18s v=%d detail=\"%s\"\n", label, v, detail.c_str());
}

// Assert the measurement reports no compressed tags (Ok, the empty case).
static void checkUncompressed(const std::vector<uint8_t> &buf, const char *label)
{
  std::string detail;
  int v = PawgCompressionVerdict(buf.data(), buf.size(), &detail);
  CHECK(v == kPawgOk);
  CHECK(contains(detail, "no DEFLATE-compressed tags"));
  std::printf("  %-18s v=%d detail=\"%s\"\n", label, v, detail.c_str());
}

int main()
{
  // --- Compressed tag TYPES are detected regardless of build ------------------
  checkCompressed(makeProfile(0x74617267u /*'targ'*/, kZut8, 0), "zut8 type", "zut8");
  checkCompressed(makeProfile(0x4d533130u /*'MS10'*/, kZxml, 0), "zxml type", "zxml");

  // --- A 'data' tag is compressed iff the icCompressedData flag is set --------
  checkCompressed(makeProfile(0x64657363u /*'desc'*/, kData, kCompressedDataFlag),
                  "compressed data", "data");

  // --- Controls: uncompressed tags must read as the empty case ----------------
  checkUncompressed(makeProfile(0x63707274u /*'cprt'*/, kText, 0), "text control");
  checkUncompressed(makeProfile(0x64657363u /*'desc'*/, kData, 0), "plain data ctrl");

  // --- Robustness: a too-small / header-only buffer must not crash ------------
  {
    std::string detail;
    std::vector<uint8_t> tiny(64, 0);
    int v = PawgCompressionVerdict(tiny.data(), tiny.size(), &detail);
    CHECK(v == kPawgOk);   // no tag table -> nothing to measure, no crash
    std::printf("  %-18s v=%d detail=\"%s\"\n", "tiny buffer", v, detail.c_str());
  }

  if (g_fail) {
    std::printf("RESULT: FAIL\n");
    return 1;
  }
  std::printf("RESULT: PASS (S14 compression measurement holds)\n");
  return 0;
}
