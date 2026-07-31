/*
    File:       tointernal-fixedint-roundtrip.cpp

    Contains:   CTest regression for CIccCmm::ToInternalEncoding fixed-integer
                decode.

    ToInternalEncoding must be the inverse of FromInternalEncoding: given an
    already-encoded fixed-integer sample it must decode back to the internal
    0-1 representation.  It used to re-encode the input through icFtoU8/icFtoU16
    (which clamp to [0,1]), so any 0..255 / 0..65535 sample saturated to full
    and the round trip was destroyed for every space except RGB/CMYK (which the
    integer overloads special-case with an explicit /N).

    This helper asserts From(To(encoded)) == encoded (within half an LSB) for
    the default/CLR path (RGB, CMYK, 7CLR), the Lab path and the XYZ path across
    the 8-bit / 16-bit / 16-bit-v2 encodings, and that two distinct 7CLR samples
    decode to distinct internal values.

    Exit codes:
      0 - all round trips and the distinctness check passed
      1 - a round trip diverged
*/

#include "IccCmm.h"

#include <cmath>
#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static int g_failures = 0;

static const char* encName(icFloatColorEncoding e)
{
  switch (e) {
    case icEncode8Bit:    return "8Bit";
    case icEncode16Bit:   return "16Bit";
    case icEncode16BitV2: return "16BitV2";
    default:              return "?";
  }
}

// From(To(encoded)) must reproduce every encoded sample to within half an LSB.
static void checkRoundTrip(const char* label, icColorSpaceSignature space,
                           icFloatColorEncoding enc,
                           const icFloatNumber* encoded, int n)
{
  icFloatNumber internal[16];
  icFloatNumber back[16];

  if (CIccCmm::ToInternalEncoding(space, enc, internal, encoded, false) != icCmmStatOk) {
    std::printf("FAIL %s/%s: ToInternalEncoding returned error\n", label, encName(enc));
    g_failures++;
    return;
  }
  if (CIccCmm::FromInternalEncoding(space, enc, back, internal, false) != icCmmStatOk) {
    std::printf("FAIL %s/%s: FromInternalEncoding returned error\n", label, encName(enc));
    g_failures++;
    return;
  }
  for (int i = 0; i < n; i++) {
    if (std::fabs(back[i] - encoded[i]) > 0.5f) {
      std::printf("FAIL %s/%s ch%d: encoded %.3f -> internal %.6f -> re-encoded %.3f\n",
                  label, encName(enc), i, encoded[i], internal[i], back[i]);
      g_failures++;
    }
  }
}

// Two distinct encoded 7CLR samples must decode to distinct internal values.
// Pre-fix both saturate to (1,1,1,1,1,1,1) and this check fails.
static void check7clrDistinct()
{
  const icFloatNumber a8[7] = { 64, 64, 64, 64, 64, 64, 64 };
  const icFloatNumber b8[7] = { 200, 200, 200, 200, 200, 200, 200 };
  icFloatNumber ia[7], ib[7];

  if (CIccCmm::ToInternalEncoding(icSig7colorData, icEncode8Bit, ia, a8, false) != icCmmStatOk ||
      CIccCmm::ToInternalEncoding(icSig7colorData, icEncode8Bit, ib, b8, false) != icCmmStatOk) {
    std::printf("FAIL 7CLR distinct: ToInternalEncoding returned error\n");
    g_failures++;
    return;
  }
  bool differ = false;
  for (int i = 0; i < 7; i++)
    if (std::fabs(ia[i] - ib[i]) > 0.01f)
      differ = true;
  if (!differ) {
    std::printf("FAIL 7CLR distinct: 64s and 200s decoded to the same internal value\n");
    g_failures++;
  }
}

int main()
{
  // default/CLR device path (these signatures have no dedicated case in the
  // float overload, so they exercise the default: branch).
  const icFloatNumber rgb8[3]   = { 0.0f, 128.0f, 255.0f };
  const icFloatNumber rgb16[3]  = { 0.0f, 32768.0f, 65535.0f };
  checkRoundTrip("RGB",  icSigRgbData,     icEncode8Bit,    rgb8, 3);
  checkRoundTrip("RGB",  icSigRgbData,     icEncode16Bit,   rgb16, 3);
  checkRoundTrip("RGB",  icSigRgbData,     icEncode16BitV2, rgb16, 3);

  const icFloatNumber cmyk8[4]  = { 0.0f, 64.0f, 128.0f, 255.0f };
  const icFloatNumber cmyk16[4] = { 0.0f, 16384.0f, 32768.0f, 65535.0f };
  checkRoundTrip("CMYK", icSigCmykData,    icEncode8Bit,    cmyk8, 4);
  checkRoundTrip("CMYK", icSigCmykData,    icEncode16Bit,   cmyk16, 4);
  checkRoundTrip("CMYK", icSigCmykData,    icEncode16BitV2, cmyk16, 4);

  const icFloatNumber clr7_8[7]  = { 0.0f, 64.0f, 128.0f, 200.0f, 255.0f, 32.0f, 96.0f };
  const icFloatNumber clr7_16[7] = { 0.0f, 16384.0f, 32768.0f, 50000.0f, 65535.0f, 8192.0f, 24576.0f };
  checkRoundTrip("7CLR", icSig7colorData,  icEncode8Bit,    clr7_8, 7);
  checkRoundTrip("7CLR", icSig7colorData,  icEncode16Bit,   clr7_16, 7);
  checkRoundTrip("7CLR", icSig7colorData,  icEncode16BitV2, clr7_16, 7);

  // Lab: 8-bit encodes L as 0..255 and a*/b* via icABtoU8 (0..255); 16-bit and
  // 16-bit-v2 encode the normalized PCS channels as 0..65535.
  const icFloatNumber lab8[3]  = { 128.0f, 100.0f, 160.0f };
  const icFloatNumber lab16[3] = { 32768.0f, 25600.0f, 40960.0f };
  checkRoundTrip("Lab",  icSigLabData, icEncode8Bit,    lab8, 3);
  checkRoundTrip("Lab",  icSigLabData, icEncode16Bit,   lab16, 3);
  checkRoundTrip("Lab",  icSigLabData, icEncode16BitV2, lab16, 3);

  // XYZ: 16-bit / 16-bit-v2 encode each channel as a u1Fixed15 value (0..65535,
  // 0x8000 == 1.0).
  const icFloatNumber xyz16[3] = { 32768.0f, 16384.0f, 49152.0f };
  checkRoundTrip("XYZ",  icSigXYZData, icEncode16Bit,   xyz16, 3);
  checkRoundTrip("XYZ",  icSigXYZData, icEncode16BitV2, xyz16, 3);

  check7clrDistinct();

  if (g_failures) {
    std::printf("tointernal-fixedint-roundtrip: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("tointernal-fixedint-roundtrip: OK\n");
  return 0;
}
