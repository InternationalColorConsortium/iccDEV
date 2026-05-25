#include "IccIO.h"
#include "IccProfile.h"

#include <cstdio>

class FailingWriteIO : public CIccIO
{
public:
  size_t Write8(void *, size_t) override { return 0; }
  int64_t Seek(int64_t, icSeekVal) override { return 0; }
  int64_t Tell() override { return 0; }
};

int main()
{
  CIccProfile profile;
  profile.InitHeader();

  FailingWriteIO failing;
  if (profile.Write(&failing, icNeverWriteID)) {
    std::printf("CIccProfile::Write succeeded despite failing IO\n");
    return 1;
  }

  CIccMemIO memory;
  if (!memory.Alloc(4096, true)) {
    std::printf("memory allocation failed\n");
    return 1;
  }

  if (!profile.Write(&memory, icNeverWriteID)) {
    std::printf("CIccProfile::Write failed on writable memory IO\n");
    return 1;
  }

  std::printf("CIccProfile::Write failure handling OK\n");
  return 0;
}
