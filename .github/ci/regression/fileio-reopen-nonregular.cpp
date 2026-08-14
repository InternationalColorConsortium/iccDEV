// Regression test for #1940: a rejected write-mode reopen must leave
// CIccFileIO closed, not holding a FILE* it has already fclose()d.
//
// CIccFileIO::Open closes any stream it is already holding before opening the
// new one.  That fclose() has never nulled m_fFile, which was harmless while
// the very next statement was the fopen() assignment.  #1204 inserted the
// POSIX regular-file check between the two, and its early "return false" hands
// the destructor -- and every other member -- a freed FILE*.
//
// Observed before the fix, on the same source:
//   plain -O2       SIGSEGV inside fread() on the first read after the reopen
//   ASAN Debug      "attempting double-free" in CIccFileIO::CloseFile
//
// The check is compiled out on Windows, so the defect and this test are both
// POSIX-only.  The Windows arm exits 77, which the SKIP_RETURN_CODE on the
// registration turns into a ctest Skipped rather than a green tick over a
// binary that asserted nothing.

#include "IccIO.h"

#include <cstdio>
#include <cstring>

int main()
{
#if defined(_WIN32) || defined(WIN32)
  std::printf("SKIP: the regular-file reopen check is POSIX-only\n");
  return 77;
#else
  const char scratch_file[] = "fileio-reopen-nonregular-scratch.bin";
  const char payload[] = "abcd";
  const size_t payload_len = sizeof(payload) - 1;

  {
    FILE *seed = std::fopen(scratch_file, "wb");
    if (!seed) {
      std::printf("could not create %s\n", scratch_file);
      return 1;
    }
    std::fwrite(payload, 1, payload_len, seed);
    std::fclose(seed);
  }

  CIccFileIO io;
  if (!io.Open(scratch_file, "rb")) {
    std::printf("open failed: %s\n", scratch_file);
    std::remove(scratch_file);
    return 1;
  }

  // Reopen the same object in write mode on a character device.  The
  // regular-file check refuses it, and must not leave the stream it closed on
  // the way in still attached.
  if (io.Open("/dev/null", "wb")) {
    std::printf("write-mode reopen on a character device unexpectedly "
                "succeeded\n");
    std::remove(scratch_file);
    return 1;
  }

  // Both of these run through m_fFile.  With the stale pointer in place the
  // first is a read through freed memory and the second is a second fclose()
  // of the same stream.  The read's return value is not a reliable
  // discriminator on its own -- under ASAN the quarantined FILE still yields 0
  // -- so the failure signal is the crash or the sanitizer report, and the
  // check below only guards against a stream that stayed genuinely readable.
  char buf[8];
  std::memset(buf, 0, sizeof buf);
  size_t nRead = io.Read8(buf, sizeof buf);
  io.Close();

  if (nRead != 0) {
    std::printf("refused reopen left a readable stream: %zu bytes\n", nRead);
    std::remove(scratch_file);
    return 1;
  }

  // A refused Open must leave the object reusable, not merely non-crashing:
  // the same handle has to open and read a regular file afterwards.
  if (!io.Open(scratch_file, "rb")) {
    std::printf("object was not reusable after a refused reopen\n");
    std::remove(scratch_file);
    return 1;
  }

  std::memset(buf, 0, sizeof buf);
  size_t nReopened = io.Read8(buf, payload_len);
  io.Close();
  std::remove(scratch_file);

  if (nReopened != payload_len || std::memcmp(buf, payload, payload_len) != 0) {
    std::printf("reuse after a refused reopen read %zu bytes, expected %zu\n",
                nReopened, payload_len);
    return 1;
  }

  std::printf("CIccFileIO refused reopen left no attached stream, and the "
              "object stayed reusable\n");
  return 0;
#endif
}
