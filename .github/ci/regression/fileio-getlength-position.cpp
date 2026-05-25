#include "IccIO.h"
#include "IccProfile.h"

#include <cstdio>

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::printf("usage: fileio-getlength-position <profile> <roundtrip>\n");
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

  CIccFileIO write_io;
  if (!write_io.Open(argv[2], "w+b")) {
    std::printf("write open failed: %s\n", argv[2]);
    return 1;
  }

  const char bytes[] = "12345678";
  if (write_io.Write8((void *)bytes, sizeof(bytes) - 1) != sizeof(bytes) - 1) {
    std::printf("write failed\n");
    return 1;
  }
  if (write_io.Seek(4, icSeekSet) != 4) {
    std::printf("write seek failed\n");
    return 1;
  }

  size_t write_length = write_io.GetLength();
  int64_t write_position = write_io.Tell();
  if (write_length != sizeof(bytes) - 1 || write_position != 4) {
    std::printf("write GetLength changed position: length=%zu position=%lld\n",
                write_length, (long long)write_position);
    return 1;
  }
  write_io.Close();

  CIccProfile *profile = ReadIccProfile(argv[1]);
  if (!profile) {
    std::printf("ReadIccProfile source failed\n");
    return 1;
  }

  const icUInt32Number source_size = profile->m_Header.size;
  const size_t source_tags = profile->m_Tags.size();
  if (!SaveIccProfile(argv[2], profile, icNeverWriteID)) {
    std::printf("SaveIccProfile roundtrip failed\n");
    delete profile;
    return 1;
  }
  delete profile;

  CIccProfile *roundtrip = ReadIccProfile(argv[2]);
  if (!roundtrip) {
    std::printf("ReadIccProfile roundtrip failed\n");
    return 1;
  }

  const icUInt32Number roundtrip_size = roundtrip->m_Header.size;
  const size_t roundtrip_tags = roundtrip->m_Tags.size();
  delete roundtrip;

  if (roundtrip_size == 0 || roundtrip_tags != source_tags) {
    std::printf("roundtrip mismatch: source_size=%u roundtrip_size=%u source_tags=%zu roundtrip_tags=%zu\n",
                source_size, roundtrip_size, source_tags, roundtrip_tags);
    return 1;
  }

  std::printf("GetLength write path and profile roundtrip OK: source_size=%u roundtrip_size=%u tags=%zu\n",
              source_size, roundtrip_size, roundtrip_tags);
  return 0;
}
