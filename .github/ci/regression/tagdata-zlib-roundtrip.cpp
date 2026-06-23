// Regression: CIccTagData zlib compression round-trip (#1521).
//
// Exercises the compressed-dataType read/write path completed in #1521. The
// stub this replaces left CIccTagData::Read() with the on-disk zlib bytes
// uninterpreted and CIccTagData::Write() writing zero payload bytes (silent
// data loss). This test loads plaintext into a compressed-flagged data tag,
// writes it (must deflate to fewer bytes than the plaintext), reads it back
// (must inflate to the exact original), and verifies a second round-trip is
// stable. An uncompressed control confirms the non-compressed path is verbatim.
//
// Built and registered only when ICC_USE_ZLIB is enabled (the option added in
// #1530); without zlib the library cannot deflate and these invariants do not
// hold.
#include "IccTag.h"
#include "IccIO.h"
#include "IccProfile.h"

#include <cstdio>
#include <cstring>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ std::printf("FAIL line %d: %s\n", __LINE__, #c); g_fail=1; } } while(0)

int main()
{
  // Plaintext with redundancy so deflate measurably shrinks it.
  std::string plain;
  for (int i = 0; i < 5000; i++)
    plain += (char)('A' + (i % 26));
  const icUInt32Number nPlain = (icUInt32Number)plain.size();

  // Compressed-binary data tag loaded with the plaintext.
  CIccTagData src;
  src.SetDataType(icCompressedBinaryData);   // icCompressedData | icBinaryData
  CHECK(src.IsTypeCompressed());
  CHECK(src.SetSize(nPlain, false));
  std::memcpy(src.GetData(0), plain.data(), nPlain);

  // Write -> on-disk form is a deflate stream, smaller than the plaintext.
  CIccMemIO io;
  CHECK(io.Alloc(64 + nPlain, true));
  CHECK(src.Write(&io));
  const icUInt32Number nWritten = (icUInt32Number)io.Tell();
  std::printf("plaintext=%u bytes, written(tag incl 12-byte header)=%u bytes\n",
              nPlain, nWritten);
  CHECK(nWritten < nPlain);   // compression must have shrunk the payload
  CHECK(nWritten > 12);       // 3x uint32 header + a non-empty deflate stream

  // Read back -> Read() inflates to the exact original plaintext.
  io.Seek(0, icSeekSet);
  CIccTagData dst;
  CHECK(dst.Read(nWritten, &io));
  CHECK(dst.IsTypeCompressed());
  CHECK(dst.GetSize() == nPlain);
  CHECK(std::memcmp(dst.GetData(0), plain.data(), nPlain) == 0);

  // Second round-trip from the read-back tag must remain byte-exact.
  CIccMemIO io2;
  CHECK(io2.Alloc(64 + nPlain, true));
  CHECK(dst.Write(&io2));
  const icUInt32Number nWritten2 = (icUInt32Number)io2.Tell();
  io2.Seek(0, icSeekSet);
  CIccTagData dst2;
  CHECK(dst2.Read(nWritten2, &io2));
  CHECK(dst2.GetSize() == nPlain);
  CHECK(std::memcmp(dst2.GetData(0), plain.data(), nPlain) == 0);

  // Uncompressed control: a plain ascii tag round-trips verbatim, no compression.
  CIccTagData u;
  u.SetDataType(icAsciiData);
  CHECK(!u.IsTypeCompressed());
  CHECK(u.SetSize(nPlain, false));
  std::memcpy(u.GetData(0), plain.data(), nPlain);
  CIccMemIO io3;
  CHECK(io3.Alloc(64 + nPlain, true));
  CHECK(u.Write(&io3));
  CHECK((icUInt32Number)io3.Tell() == 12 + nPlain);   // header + verbatim payload

  if (g_fail) {
    std::printf("RESULT: FAIL\n");
    return 1;
  }
  std::printf("RESULT: PASS (CIccTagData zlib round-trip invariants held)\n");
  return 0;
}
