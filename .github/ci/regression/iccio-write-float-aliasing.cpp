// Guards the strict-aliasing fix in CIccIO::Write16/Write32/Write64
// (IccProfLib/IccIO.cpp) reported as #1948.
//
// CIccIO::WriteFloat32Float() forwards straight to Write32() whenever
// icFloatNumber and icFloat32Number are the same width, which is the normal
// build.  Write32() used to load the caller's buffer through an
// icUInt32Number lvalue:
//
//     icUInt32Number *ptr = (icUInt32Number*)pBuf32;
//     tmp = *ptr;
//
// The object being read is a float, so that load is undefined behaviour: a
// float and an icUInt32Number are not similar types, and the compiler is
// entitled to assume a store through one cannot be observed by a load through
// the other.  With -flto (ENABLE_LTO defaults to ON) the store in
// CIccSegmentedCurve::Write() and this load land in one optimization scope,
// and clang at -O3 drops the store as dead.  Every breakpoint then reached the
// file as the stack slot's initial 0.0.
//
// The read path was always safe because icSwab32Array() walks the buffer as
// icUInt8Number, which may alias any object - which is why readers agreed
// across toolchains while writers did not.  The writers now load the same way.
//
// The damage is not cosmetic.  A segmented curve whose trailing breakpoint is
// flattened to 0.0 gives its sampled segment an end point equal to its start
// point, and CIccSampledCurveSegment::Begin() (IccMpeBasic.cpp) refuses a
// segment whose range is zero, so the curve set cannot be applied at all.
//
// This test drives the library path rather than calling Write32() directly:
// both the store and the load live inside IccProfLib, so the test reproduces
// wherever the library itself is miscompiled, independently of how this
// translation unit is built.

#include "IccMpeBasic.h"
#include "IccIO.h"
#include "icProfileHeader.h"

#include <cstdio>
#include <cstring>

// CIccSegmentedCurve::Write() emits, in order: the curve signature (4 bytes),
// a reserved word (4), the segment count (2), a second reserved word (2), then
// one big-endian float breakpoint for every segment after the first.  With
// three segments the two breakpoints therefore sit at offsets 12 and 16.
static const size_t kBreakpoint0Offset = 12;
static const size_t kBreakpoint1Offset = 16;

static icUInt32Number beU32(const icUInt8Number *p)
{
  return ((icUInt32Number)p[0] << 24) | ((icUInt32Number)p[1] << 16) |
         ((icUInt32Number)p[2] << 8) | (icUInt32Number)p[3];
}

int main()
{
  // The shape taken from Testing/Calc/RGBWProjector.xml, the fixture that
  // exposed this: a formula segment covering everything below 0.0, a sampled
  // segment across the unit interval, and a formula segment above 1.0.  The
  // breakpoint that was being lost is the third segment's start point, 1.0.
  CIccSegmentedCurve curve;

  icFloatNumber params[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

  CIccFormulaCurveSegment *pLow = new CIccFormulaCurveSegment(icMinFloat32Number, 0.0f);
  pLow->SetFunction(0, 4, params);
  if (!curve.Insert(pLow)) {
    std::printf("FAIL: could not insert the low formula segment\n");
    return 1;
  }

  CIccSampledCurveSegment *pMid = new CIccSampledCurveSegment(0.0f, 1.0f);
  if (!pMid->SetSize(4)) {
    std::printf("FAIL: could not size the sampled segment\n");
    return 1;
  }
  icFloatNumber *pSamples = pMid->GetSamples();
  for (icUInt32Number i = 0; i < pMid->GetSize(); i++)
    pSamples[i] = (icFloatNumber)i / (icFloatNumber)(pMid->GetSize() - 1);
  if (!curve.Insert(pMid)) {
    std::printf("FAIL: could not insert the sampled segment\n");
    return 1;
  }

  icFloatNumber highParams[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
  CIccFormulaCurveSegment *pHigh = new CIccFormulaCurveSegment(1.0f, icMaxFloat32Number);
  pHigh->SetFunction(0, 4, highParams);
  if (!curve.Insert(pHigh)) {
    std::printf("FAIL: could not insert the high formula segment\n");
    return 1;
  }

  CIccMemIO io;
  if (!io.Alloc(4096, true)) {
    std::printf("FAIL: could not allocate the write buffer\n");
    return 1;
  }

  if (!curve.Write(&io)) {
    std::printf("FAIL: CIccSegmentedCurve::Write reported failure\n");
    return 1;
  }

  const size_t written = io.GetLength();
  if (written < kBreakpoint1Offset + 4) {
    std::printf("FAIL: wrote only %zu bytes, too short to hold two breakpoints\n", written);
    return 1;
  }

  // 0x3F800000 is the big-endian IEEE-754 encoding of 1.0f, and 0x00000000 of
  // 0.0f.  Reading the emitted bytes rather than the in-memory segment is the
  // point: the defect was in what reached the buffer, not in what the object
  // held.
  const icUInt8Number *pData = io.GetData();
  const icUInt32Number bp0 = beU32(pData + kBreakpoint0Offset);
  const icUInt32Number bp1 = beU32(pData + kBreakpoint1Offset);

  if (bp0 != 0x00000000u) {
    std::printf("FAIL: first breakpoint wrote 0x%08X, expected 0x00000000 (0.0)\n", bp0);
    return 1;
  }

  if (bp1 != 0x3F800000u) {
    std::printf("FAIL: second breakpoint wrote 0x%08X, expected 0x3F800000 (1.0)\n"
                "      The segmented curve declared a start point of 1.0 for its "
                "trailing segment;\n"
                "      a value of 0x00000000 is the #1948 strict-aliasing defect in "
                "CIccIO::Write32.\n",
                bp1);
    return 1;
  }

  // Read the bytes back and confirm the curve is usable.  This is the part a
  // byte comparison alone would miss: with the breakpoint flattened, the
  // sampled segment comes back with a zero range and Begin() rejects it, so
  // the whole curve set fails to initialize.
  io.Seek(0, icSeekSet);

  CIccSegmentedCurve readBack;
  if (!readBack.Read((icUInt32Number)written, &io)) {
    std::printf("FAIL: CIccSegmentedCurve::Read rejected the bytes just written\n");
    return 1;
  }

  if (!readBack.Begin(icElemInterpLinear, NULL)) {
    std::printf("FAIL: the round-tripped curve would not Begin(); a sampled segment "
                "with a zero range is the #1948 symptom\n");
    return 1;
  }

  std::printf("CIccIO float writers OK: breakpoints 0x%08X 0x%08X, curve begins after "
              "round-trip\n",
              bp0, bp1);
  return 0;
}
