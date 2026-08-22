#include "IccSignatureUtils.h"
#include "IccUtil.h"

#include <cstring>
#include <iostream>

static int fail(const char *msg)
{
  std::cerr << msg << "\n";
  return 1;
}

int main()
{
  if (!IsValidColorSpaceSignature((icUInt32Number)icSigRgbData))
    return fail("RGB signature rejected");

  icColorSpaceSignature nchannel =
      (icColorSpaceSignature)(icSigNChannelData + 7);
  if (!IsValidColorSpaceSignature((icUInt32Number)nchannel))
    return fail("N-channel signature rejected");
  if (std::strcmp(ColorSpaceSignatureToStr((icUInt32Number)nchannel), "NChannel"))
    return fail("N-channel signature not named");

  icColorSpaceSignature mcs =
      (icColorSpaceSignature)(icSigSrcMCSChannelData + 3);
  if (!IsValidColorSpaceSignature((icUInt32Number)mcs))
    return fail("MCS signature rejected");
  if (std::strcmp(ColorSpaceSignatureToStr((icUInt32Number)mcs), "MCS"))
    return fail("MCS signature not named");

  CIccInfo info;
  const char *mcsName = info.GetColorSpaceSigName(mcs);
  if (!std::strstr(mcsName, "MCS"))
    return fail("CIccInfo lost MCS signature context");

  char sigBuf[16];
  if (std::strcmp(icGetColorSigStr(sigBuf, sizeof(sigBuf),
                                   (icUInt32Number)mcs), "mc0003"))
    return fail("MCS compact signature formatting failed");

  return 0;
}
