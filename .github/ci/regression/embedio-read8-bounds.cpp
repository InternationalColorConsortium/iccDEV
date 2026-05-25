#include "IccIO.h"

#include <cstdio>

int main()
{
  icUInt8Number bytes[16];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = (icUInt8Number)('A' + i);
  }

  CIccMemIO backing;
  if (!backing.Attach(bytes, sizeof(bytes))) {
    std::printf("backing attach failed\n");
    return 1;
  }

  if (backing.Seek(8, icSeekSet) != 8) {
    std::printf("initial seek failed\n");
    return 1;
  }

  CIccEmbedIO embed;
  if (!embed.Attach(&backing, 4)) {
    std::printf("embed attach failed\n");
    return 1;
  }

  icUInt8Number out[16] = {};

  if (backing.Seek(0, icSeekSet) != 0) {
    std::printf("backing rewind failed\n");
    return 1;
  }

  size_t read = embed.Read8(out, sizeof(out));
  if (read != 0) {
    std::printf("Read8 read before embedded window: read=%zu\n", read);
    return 1;
  }

  if (embed.Seek(2, icSeekSet) != 2) {
    std::printf("embed seek failed\n");
    return 1;
  }

  read = embed.Read8(out, sizeof(out));
  if (read != 2 || out[0] != 'K' || out[1] != 'L') {
    std::printf("Read8 did not clamp to embedded window: read=%zu first=%u second=%u\n",
                read, (unsigned)out[0], (unsigned)out[1]);
    return 1;
  }

  std::printf("CIccEmbedIO::Read8 bounds OK\n");
  return 0;
}
