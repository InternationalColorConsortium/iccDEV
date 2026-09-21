// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

/*
    File:       tonemap-write-many-outputs.cpp

    Contains:   CTest helper for writing a toneMapElement with more than 16
                output channels.

    A toneMapElement stores one position entry per output channel, and the
    channel count is a uInt16Number.  Read(), and the XML and JSON readers,
    accept any count, but Write() kept the positions it had written in a
    16-entry stack array, so saving an element with 17 or more outputs wrote
    past the array.  #2608.

    Each case builds an element with an identity luminance curve and tone
    function type 0 with parameters { 1, 0, k }, so channel i maps a pixel
    value of 0.5 to 0.5 + k, where k names the function channel i uses.  The
    element is written, its position table is checked directly (a channel that
    shares a function points at the same offset, and every other channel at its
    own), it is read back and applied, and the copy that was read is written
    again and compared with the first write byte for byte.

    The cases:
      16 outputs, each with its own function   the old limit, a control
      17 outputs, each with its own function
      17 outputs, all sharing channel 0's      the shape of the #2608 report
      1000 outputs, every third one sharing    far past the old limit
*/

#include "IccDefs.h"
#include "IccIO.h"
#include "IccMpeBasic.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static int g_failures = 0;

static void check(bool cond, const char* msg, int nOutputs)
{
  if (!cond) {
    std::printf("FAIL: %d outputs: %s\n", nOutputs, msg);
    ++g_failures;
  }
}

static icUInt32Number be32(const icUInt8Number* p)
{
  return ((icUInt32Number)p[0] << 24) | ((icUInt32Number)p[1] << 16) | ((icUInt32Number)p[2] << 8) | p[3];
}

/* owner[i] is the channel whose function channel i uses; owner[i] == i means
   channel i has its own. */
static void runCase(const char* name, const std::vector<int>& owner)
{
  const int n = (int)owner.size();

  CIccMpeToneMap out((icUInt16Number)n);

  CIccSegmentedCurve* pLum = new CIccSegmentedCurve();
  CIccFormulaCurveSegment* pSeg = new CIccFormulaCurveSegment(icMinFloat32Number, icMaxFloat32Number);
  icFloatNumber lumParams[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
  pSeg->SetFunction(0, 4, lumParams);
  pLum->Insert(pSeg);
  out.SetLumCurve(pLum);

  std::vector<CIccToneMapFunc*> funcs(n, NULL);
  for (int i = 0; i < n; i++) {
    if (owner[i] == i) {
      funcs[i] = new CIccToneMapFunc();
      icFloatNumber params[3] = { 1.0f, 0.0f, (icFloatNumber)i };
      funcs[i]->SetFunction(0, 3, params);
    }
    else {
      funcs[i] = funcs[owner[i]];
    }
    check(out.Insert(funcs[i]), "Insert() refused a function", n);
  }

  CIccMemIO io;
  const icUInt32Number bufSize = 1024 * 1024;
  bool bWrote = io.Alloc(bufSize, true) && out.Write(&io);
  check(bWrote, "Write() failed", n);
  if (!bWrote)
    return;
  icUInt32Number nWritten = (icUInt32Number)io.Tell();

  /* 'tmap', reserved, and the input and output channel counts (12 bytes), the
     luminance curve's position, then one position per output channel. */
  const icUInt8Number* pData = io.GetData();
  const icUInt32Number tableStart = 12 + 8;
  check(tableStart + 8 * (icUInt32Number)n <= nWritten, "position table runs past the element", n);
  if (tableStart + 8 * (icUInt32Number)n > nWritten)
    return;

  icUInt32Number lastOwnEnd = 0;
  for (int i = 0; i < n; i++) {
    icUInt32Number offset = be32(pData + tableStart + 8 * i);
    icUInt32Number size = be32(pData + tableStart + 8 * i + 4);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "channel %d position %u+%u is outside the element", i, offset, size);
    check(offset >= tableStart + 8 * (icUInt32Number)n && size && offset + size <= nWritten, msg, n);

    /* Functions are written in channel order, so each channel with its own
       function lies after the one before it. */
    if (owner[i] != i) {
      std::snprintf(msg, sizeof(msg), "channel %d does not share channel %d's function", i, owner[i]);
      check(offset == be32(pData + tableStart + 8 * owner[i]), msg, n);
    }
    else {
      std::snprintf(msg, sizeof(msg), "channel %d does not have a function of its own", i);
      check(offset >= lastOwnEnd, msg, n);
      lastOwnEnd = offset + size;
    }
  }

  io.Seek(0, icSeekSet);
  CIccMpeToneMap in;
  bool bRead = in.Read(nWritten, &io);
  check(bRead, "Read() refused what Write() wrote", n);
  if (!bRead)
    return;

  bool bBegin = in.Begin(icElemInterpLinear, NULL);
  check(bBegin, "Begin() refused the element that was read", n);
  if (bBegin) {
    std::vector<icFloatNumber> src(n + 1, 0.5f);
    std::vector<icFloatNumber> dst(n, 0.0f);
    src[n] = 1.0f;
    in.Apply(NULL, dst.data(), src.data());
    for (int i = 0; i < n; i++) {
      icFloatNumber expected = 0.5f + (icFloatNumber)owner[i];
      if (std::fabs(dst[i] - expected) > 1e-4f) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "channel %d gives %g, expected %g", i, (double)dst[i], (double)expected);
        check(false, msg, n);
      }
    }
  }

  CIccMemIO io2;
  bool bRewrote = io2.Alloc(bufSize, true) && in.Write(&io2);
  check(bRewrote, "Write() failed on the element that was read", n);
  if (bRewrote) {
    check(io2.Tell() == nWritten &&
            std::memcmp(io2.GetData(), pData, nWritten) == 0,
          "writing the element that was read does not give the same bytes", n);
  }

  std::printf("%s: %d outputs, %u bytes\n", name, n, nWritten);
}

int main()
{
  std::vector<int> owner;

  owner.clear();
  for (int i = 0; i < 16; i++)
    owner.push_back(i);
  runCase("control, the old limit", owner);

  owner.clear();
  for (int i = 0; i < 17; i++)
    owner.push_back(i);
  runCase("each output its own function", owner);

  owner.assign(17, 0);
  runCase("all outputs share channel 0", owner);

  owner.clear();
  for (int i = 0; i < 1000; i++)
    owner.push_back(i % 3 == 1 ? i - 1 : i);
  runCase("every third output shares", owner);

  if (g_failures) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
