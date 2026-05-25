#include "IccIO.h"

#include <cstdio>

int main(int argc, char **argv)
{
  if (argc != 2) {
    std::printf("usage: fileio-seek-tell <scratch-file>\n");
    return 2;
  }

  CIccFileIO io;
  if (!io.Open(argv[1], "w+b")) {
    std::printf("open failed: %s\n", argv[1]);
    return 1;
  }

  const char bytes[] = "0123456789abcdef";
  const size_t byte_count = sizeof(bytes) - 1;
  if (io.Write8((void *)bytes, byte_count) != byte_count) {
    std::printf("write failed\n");
    return 1;
  }

  if (io.Seek(4, icSeekSet) != 4) {
    std::printf("seek set failed\n");
    return 1;
  }

  if (io.GetLength() != byte_count || io.Tell() != 4) {
    std::printf("GetLength did not preserve position: length=%zu position=%lld\n",
                io.GetLength(), (long long)io.Tell());
    return 1;
  }

  if (io.Seek(-1, icSeekSet) >= 0) {
    std::printf("negative absolute seek unexpectedly succeeded\n");
    return 1;
  }

  if (io.Tell() != 4) {
    std::printf("failed seek changed position: position=%lld\n",
                (long long)io.Tell());
    return 1;
  }

  if (io.Seek(-2, icSeekEnd) != (int64_t)byte_count - 2) {
    std::printf("seek end failed: position=%lld\n", (long long)io.Tell());
    return 1;
  }

  if (io.Seek(1, icSeekCur) != (int64_t)byte_count - 1) {
    std::printf("seek cur failed: position=%lld\n", (long long)io.Tell());
    return 1;
  }

  std::printf("CIccFileIO seek/tell/length OK: length=%zu position=%lld\n",
              io.GetLength(), (long long)io.Tell());
  return 0;
}
