#include "IccProfile.h"
#include "IccIO.h"

#include <cstdio>

class ExposedProfile : public CIccProfile
{
public:
  using CIccProfile::ConnectSubProfile;

  void SetAttachedIO(CIccIO *io)
  {
    m_pAttachIO = io;
    m_bSharedIO = true;
  }
};

class RestoreFailIO : public CIccIO
{
public:
  int64_t Tell() override { return 5; }

  int64_t Seek(int64_t offset, icSeekVal) override
  {
    return offset == 5 ? -1 : offset;
  }
};

class FailedSeekReadIO : public CIccIO
{
public:
  int read_count = 0;

  int64_t Tell() override { return 0; }
  int64_t Seek(int64_t, icSeekVal) override { return -1; }

  size_t Read8(void *pBuf, size_t nNum = 1) override
  {
    ++read_count;
    if (nNum >= sizeof(icUInt32Number)) {
      *((icUInt32Number *)pBuf) = read_count == 1 ? (icUInt32Number)icSigEmbeddedProfileType : 0u;
      return sizeof(icUInt32Number);
    }
    return 0;
  }
};

int main()
{
  ExposedProfile read_tags_profile;
  RestoreFailIO restore_fail;
  read_tags_profile.SetAttachedIO(&restore_fail);

  if (read_tags_profile.ReadTags(NULL)) {
    std::printf("ReadTags ignored failed cursor restore\n");
    return 1;
  }

  ExposedProfile sub_profile;
  IccTagEntry entry = {};
  entry.TagInfo.sig = icSigEmbeddedV5ProfileTag;
  entry.TagInfo.offset = 128;
  entry.TagInfo.size = 16;
  sub_profile.m_Tags.push_back(entry);

  FailedSeekReadIO failed_seek;
  CIccIO *connected = sub_profile.ConnectSubProfile(&failed_seek, false);
  if (connected || failed_seek.read_count != 0) {
    void *connected_ptr = connected;
    std::printf("ConnectSubProfile read after failed seek: connected=%p reads=%d\n",
                connected_ptr, failed_seek.read_count);
    delete connected;
    return 1;
  }

  std::printf("Parser restore call checks OK\n");
  return 0;
}
