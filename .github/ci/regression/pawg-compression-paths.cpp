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

#include "IccIO.h"
#include "IccProfile.h"

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

// Build a structurally readable profile whose A2B0 tag uses a forbidden text
// type. ValidateIccProfile() returns a CIccProfile plus a critical status, so
// this pins AssessPawgFromMemory() to library status rather than nullness.
static std::vector<uint8_t> makeCriticalParsedProfile()
{
  std::vector<uint8_t> b(156, 0);
  putU32BE(b, 0, (uint32_t)b.size());
  putU32BE(b, 8, 0x04400000u);                     // ICC v4.4
  putU32BE(b, 12, 0x6d6e7472u);                    // 'mntr'
  putU32BE(b, 16, 0x52474220u);                    // 'RGB '
  putU32BE(b, 20, 0x58595a20u);                    // 'XYZ '
  putU32BE(b, 36, 0x61637370u);                    // 'acsp'
  putU32BE(b, 68, 0x0000f6d6u);                    // D50 X
  putU32BE(b, 72, 0x00010000u);                    // D50 Y
  putU32BE(b, 76, 0x0000d32du);                    // D50 Z
  putU32BE(b, 128, 1);                             // tag count
  putU32BE(b, 132, 0x41324230u);                   // 'A2B0'
  putU32BE(b, 136, 144);                           // tag data offset
  putU32BE(b, 140, 12);                            // tag data size
  putU32BE(b, 144, kText);                         // forbidden 'text' type
  b[152] = 'x';
  return b;
}

// Build a v5 DToB0 profile with an mpet tag whose one-element position table
// is truncated. The library must reject it outright, yielding no profile.
static std::vector<uint8_t> makeMalformedMpeProfile()
{
  std::vector<uint8_t> b(160, 0);
  putU32BE(b, 0, (uint32_t)b.size());
  putU32BE(b, 8, 0x05000000u);                     // ICC v5
  putU32BE(b, 12, 0x6d6e7472u);                    // 'mntr'
  putU32BE(b, 16, 0x52474220u);                    // 'RGB '
  putU32BE(b, 20, 0x58595a20u);                    // 'XYZ '
  putU32BE(b, 36, 0x61637370u);                    // 'acsp'
  putU32BE(b, 68, 0x0000f6d6u);                    // D50 X
  putU32BE(b, 72, 0x00010000u);                    // D50 Y
  putU32BE(b, 76, 0x0000d32du);                    // D50 Z
  putU32BE(b, 128, 1);                             // tag count
  putU32BE(b, 132, 0x44324230u);                   // 'D2B0'
  putU32BE(b, 136, 144);                           // tag data offset
  putU32BE(b, 140, 16);                            // tag data size
  putU32BE(b, 144, 0x6d706574u);                   // 'mpet'
  putU32BE(b, 152, 0x00030003u);                   // 3 input, 3 output
  putU32BE(b, 156, 1);                             // one element, no position
  return b;
}

static void initProfileHeader(std::vector<uint8_t> &b, uint32_t version)
{
  putU32BE(b, 0, (uint32_t)b.size());
  putU32BE(b, 8, version);
  putU32BE(b, 12, 0x6d6e7472u);                    // 'mntr'
  putU32BE(b, 16, 0x52474220u);                    // 'RGB '
  putU32BE(b, 20, 0x58595a20u);                    // 'XYZ '
  putU32BE(b, 36, 0x61637370u);                    // 'acsp'
  putU32BE(b, 68, 0x0000f6d6u);                    // D50 X
  putU32BE(b, 72, 0x00010000u);                    // D50 Y
  putU32BE(b, 76, 0x0000d32du);                    // D50 Z
}

// Two individually readable unknown tags partially overlap. Each begins with
// its own type signature; only the nested tag-table layout is non-compliant.
static std::vector<uint8_t> makeEmbeddedOverlapProfile()
{
  std::vector<uint8_t> embedded(172, 0);
  initProfileHeader(embedded, 0x05000000u);
  putU32BE(embedded, 128, 2);
  putU32BE(embedded, 132, 0x70727631u);            // 'prv1'
  putU32BE(embedded, 136, 160);
  putU32BE(embedded, 140, 8);
  putU32BE(embedded, 144, 0x70727632u);            // 'prv2'
  putU32BE(embedded, 148, 164);
  putU32BE(embedded, 152, 8);
  putU32BE(embedded, 160, 0x74413031u);            // 'tA01'
  putU32BE(embedded, 164, 0x74423032u);            // 'tB02'
  putU32BE(embedded, 168, 0);

  std::vector<uint8_t> outer(152 + embedded.size(), 0);
  initProfileHeader(outer, 0x05000000u);
  putU32BE(outer, 128, 1);
  putU32BE(outer, 132, 0x49434335u);               // 'ICC5'
  putU32BE(outer, 136, 144);
  putU32BE(outer, 140, (uint32_t)(8 + embedded.size()));
  putU32BE(outer, 144, 0x49434370u);               // 'ICCp'
  std::copy(embedded.begin(), embedded.end(), outer.begin() + 152);
  return outer;
}

static CIccMemIO *makeMemIo(std::vector<uint8_t> &data)
{
  CIccMemIO *io = new CIccMemIO;
  if (!io->Attach((icUInt8Number *)data.data(), data.size(), false)) {
    delete io;
    return nullptr;
  }
  return io;
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

  // --- Full in-memory assessment must honor critical library status ----------
  {
    std::vector<uint8_t> critical = makeCriticalParsedProfile();
    std::string report;
    icValidateStatus status = icValidateOK;
    CIccProfile *profile = ValidateIccProfile(critical.data(),
                                              (icUInt32Number)critical.size(),
                                              report, status);
    CHECK(profile != nullptr);
    CHECK(status == icValidateCriticalError);
    delete profile;
    CHECK(AssessPawgFromMemory(critical.data(), critical.size()) == 1);
  }

  // A malformed mpet payload has no partially parsed profile to carry a
  // critical validation status. The assessment must still fail closed.
  {
    std::vector<uint8_t> malformed = makeMalformedMpeProfile();
    std::string report;
    icValidateStatus status = icValidateOK;
    CIccProfile *profile = ValidateIccProfile(malformed.data(),
                                              (icUInt32Number)malformed.size(),
                                              report, status);
    CHECK(profile == nullptr);
    CHECK(status == icValidateCriticalError);
    CHECK(AssessPawgFromMemory(malformed.data(), malformed.size()) == 1);
  }

  // Both embedded-profile paths must reject layout aliasing before per-tag
  // validation can obscure the cause: Read() fully loads, Attach() defers.
  {
    std::vector<uint8_t> overlap = makeEmbeddedOverlapProfile();
    CIccProfile fullyRead;
    CIccMemIO *io = makeMemIo(overlap);
    CHECK(io != nullptr);
    if (io) {
      CHECK(!fullyRead.Read(io));
      delete io;
    }
  }
  {
    std::vector<uint8_t> overlap = makeEmbeddedOverlapProfile();
    CIccProfile deferred;
    CIccMemIO *io = makeMemIo(overlap);
    CHECK(io != nullptr);
    if (io) {
      CHECK(deferred.Attach(io));
      CHECK(deferred.FindTag((icSignature)0x49434335u) == nullptr);
    }
  }

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
