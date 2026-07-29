#include "IccIO.h"
#include "IccTagBasic.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static int fail(const char *msg)
{
  std::cerr << msg << "\n";
  return 1;
}

static bool matches_text(const std::string &decoded, const char *expected)
{
  return !std::strcmp(decoded.c_str(), expected);
}

int main()
{
#ifndef ICC_USE_ZLIB
  return fail("ICC_USE_ZLIB was not defined for the zlib regression test");
#else
  const char *text =
      "zipUtf8Text zlib regression: alpha alpha alpha alpha alpha alpha "
      "alpha alpha alpha alpha alpha alpha alpha alpha alpha alpha "
      "beta beta beta beta beta beta beta beta beta beta beta beta";

  CIccTagZipUtf8Text tag;
  if (!tag.SetText(text))
    return fail("SetText failed");
  if (!tag.BufferSize())
    return fail("SetText produced an empty compressed buffer");
  if (tag.BufferSize() >= std::strlen(text) + 1)
    return fail("SetText did not compress this repetitive text fixture");

  std::string decoded;
  if (!tag.GetText(decoded))
    return fail("GetText failed");
  if (!matches_text(decoded, text))
    return fail("GetText returned the wrong string");

  CIccMemIO write_io;
  if (!write_io.Alloc(4096, true))
    return fail("Failed to allocate write buffer");
  if (!tag.Write(&write_io))
    return fail("Write failed");

  const size_t written = write_io.GetLength();
  if (written <= 8)
    return fail("Write produced only the tag header");

  std::vector<icUInt8Number> bytes(write_io.GetData(),
                                   write_io.GetData() + written);
  CIccMemIO read_io;
  if (!read_io.Attach(bytes.data(), bytes.size(), false))
    return fail("Failed to attach read buffer");

  CIccTagZipUtf8Text roundtrip;
  if (!roundtrip.Read((icUInt32Number)bytes.size(), &read_io))
    return fail("Read failed");
  decoded.clear();
  if (!roundtrip.GetText(decoded))
    return fail("Round-trip GetText failed");
  if (!matches_text(decoded, text))
    return fail("Round-trip text mismatch");

  std::string report;
  if (roundtrip.Validate("zip ", report, NULL) != icValidateOK)
    return fail("Valid compressed text did not validate cleanly");

  CIccTagZipUtf8Text corrupt;
  icUChar *corrupt_buf = corrupt.AllocBuffer(4);
  if (!corrupt_buf)
    return fail("Failed to allocate corrupt buffer");
  corrupt_buf[0] = 0x41;
  corrupt_buf[1] = 0x41;
  corrupt_buf[2] = 0x41;
  corrupt_buf[3] = 0x41;

  report.clear();
  if (corrupt.Validate("zip ", report, NULL) < icValidateNonCompliant)
    return fail("Corrupt compressed text validated successfully");
  if (report.find("corrupt compressed data") == std::string::npos)
    return fail("Corrupt compressed text did not report the zlib failure");

  return 0;
#endif
}
