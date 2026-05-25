#include "IccIO.h"

#include <cstdio>

int main(int argc, char **argv)
{
  if (argc != 2) {
    std::printf("usage: fileio-getlength-position <profile>\n");
    return 2;
  }

  CIccFileIO io;
  if (!io.Open(argv[1], "rb")) {
    std::printf("open failed: %s\n", argv[1]);
    return 1;
  }

  if (io.Seek(132, icSeekSet) != 132) {
    std::printf("seek failed\n");
    return 1;
  }

  size_t length = io.GetLength();
  int64_t position = io.Tell();
  if (length == 0 || position != 132) {
    std::printf("GetLength changed position: length=%zu position=%lld\n",
                length, (long long)position);
    return 1;
  }

  std::printf("GetLength preserved position: length=%zu position=%lld\n",
              length, (long long)position);
  return 0;
}
