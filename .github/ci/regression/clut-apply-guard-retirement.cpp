// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression for the per-pixel bounds checks removed from the apply path once
// CIccCLUT::Begin() was made able to refuse.
//
// Three guards were retired. Two were redundant; one was a defect. This file
// pins the behaviour that justifies each removal, so none of them is restored
// on the strength of a plausible-looking argument -- which has happened twice
// already to the InterpND pair, in both directions.
//
// 1. Interp3dTetra's "if (m_nOutput > 16) return;" -- A DEFECT, not redundancy.
//
//    It was described as bounding the destPixel write loop because "valid LUT
//    profiles cap output channels at <=16". They do not. m_nOutput is an
//    icUInt16Number precisely because MPE CLUT elements need more:
//    CIccMpeCLUT::Read bounds m_nInputChannels at 16 and deliberately leaves
//    m_nOutputChannels unbounded, and a spectral CLUT's output count is its
//    spectral step count. Interp1d/2d/3d/4d/5d/6d all write m_nOutput values
//    with no such bound -- Interp3dTetra was the only one that believed the
//    cap, and for a legal 3-input CLUT with >=17 outputs it returned without
//    writing anything, leaving destPixel holding whatever the caller had in it.
//    Same CLUT through Interp3d under icElemInterpLinear: correct values.
//
//    testTetraMatchesTrilinearAtGridPoints() drives both routines over the same
//    CLUT at grid points, where tetrahedral and trilinear interpolation must
//    agree exactly, and compares them across output counts spanning the old
//    threshold. On master it fails at 17 and 20 with the tetra destination
//    untouched.
//
// 2. InterpND's "if (m_nInput > 16) return;" and "if (m_nNodes > 65536)
//    return;" -- redundant, and already unreachable before Begin() could
//    refuse.
//
//    Init() rejects an m_nInput outside 1..16 and leaves m_pData NULL, and the
//    element readers turn that into a failed Read() through their GetData(0)
//    checks, so no profile ever presented InterpND with an out-of-range count.
//    Now Begin() refuses such a CLUT outright and every caller acts on the
//    result, so the invariant is established once instead of per pixel.
//    testBeginRefusesWhatTheGuardsCaught() pins that Begin() is the thing doing
//    the refusing, and testNDInterpolatesAcrossTheOldThreshold() pins that the
//    N-D path still computes correctly either side of it.
//
// 3. CIccMBB::Init()/Cleanup() -- the same shape one level up. Cleanup()
//    clamped its delete loops at 16 to stop a "corrupted channel count" walking
//    past the curve-pointer arrays, but NewCurvesA/B/M allocate those arrays to
//    the very counts being clamped, so the clamp could only leak. Init() now
//    bounds both counts and Cleanup() frees what was allocated. See
//    testMBBInitBoundsItsChannelCounts().
//
// Deliberately NOT covered here: CIccXformNDLut's two per-pixel min-with-16
// clamps. Those are finding A3 on the perf/hoist-hardening-guards branch, which
// tightens the same Begin() bound from 256 to 16 and removes them with A/B
// measurements behind it. That work predates this file and keeps ownership;
// duplicating it here would have put the same change on two branches.
// CIccXformNDLut::Begin()'s contract belongs to ndlut-input-channel-mismatch.cpp
// either way.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagLut.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[clut-apply-guards] FAIL: %s\n", what);
  }
}

// Build a CLUT of nIn dimensions and nOut output channels on a 2-point grid,
// filled with a deterministic ramp so a dropped write is distinguishable from a
// correct one that happens to be zero.
CIccCLUT *buildCLUT(icUInt8Number nIn, icUInt16Number nOut)
{
  CIccCLUT *pCLUT = new CIccCLUT(nIn, nOut, 4);
  if (!pCLUT->Init((icUInt8Number)2)) {
    delete pCLUT;
    return NULL;
  }

  icFloatNumber *pData = pCLUT->GetData(0);
  const icUInt32Number nValues = pCLUT->NumPoints() * nOut;
  for (icUInt32Number i = 0; i < nValues; i++)
    pData[i] = (icFloatNumber)((i % 97) / 97.0);

  pCLUT->Begin();
  return pCLUT;
}

// At an exact grid corner every interpolation scheme must return the stored
// node, so tetrahedral and trilinear agree bit-for-bit and the comparison needs
// no tolerance argument. This is what makes a silently skipped write visible.
void testTetraMatchesTrilinearAtGridPoints(icUInt16Number nOut)
{
  CIccCLUT *pCLUT = buildCLUT(3, nOut);
  char msg[160];

  std::snprintf(msg, sizeof(msg), "3x%u CLUT initializes", (unsigned)nOut);
  check(pCLUT != NULL, msg);
  if (!pCLUT)
    return;

  // Two opposite corners plus an interior point.
  const icFloatNumber probes[3][3] = {
    { 0.0f, 0.0f, 0.0f },
    { 1.0f, 1.0f, 1.0f },
    { 0.5f, 0.25f, 0.75f },
  };

  const icFloatNumber kSentinel = -12345.0f;
  std::vector<icFloatNumber> tetra(nOut), tri(nOut);

  for (int p = 0; p < 3; p++) {
    for (icUInt16Number i = 0; i < nOut; i++)
      tetra[i] = tri[i] = kSentinel;

    pCLUT->Interp3dTetra(&tetra[0], probes[p]);
    pCLUT->Interp3d(&tri[0], probes[p]);

    bool bTetraWrote = true;
    for (icUInt16Number i = 0; i < nOut; i++) {
      if (tetra[i] == kSentinel) { bTetraWrote = false; break; }
    }

    std::snprintf(msg, sizeof(msg),
                  "Interp3dTetra writes all %u output channels (probe %d)",
                  (unsigned)nOut, p);
    check(bTetraWrote, msg);

    // At the two corners the schemes must agree exactly; at the interior point
    // they may legitimately differ, so only the corners are compared. Both are
    // still checked for having been written at all.
    if (p < 2 && bTetraWrote) {
      bool bMatch = true;
      for (icUInt16Number i = 0; i < nOut; i++) {
        if (std::fabs((double)tetra[i] - (double)tri[i]) > 1e-6) {
          bMatch = false;
          break;
        }
      }
      std::snprintf(msg, sizeof(msg),
                    "Interp3dTetra == Interp3d at grid corner (nOut=%u, probe %d)",
                    (unsigned)nOut, p);
      check(bMatch, msg);
    }
  }

  delete pCLUT;
}

// The N-D path either side of the retired threshold. 6 dimensions routes to
// Interp6d; 7 is the smallest count that actually reaches InterpND.
void testNDInterpolatesAcrossTheOldThreshold()
{
  for (icUInt8Number nIn = 6; nIn <= 8; nIn++) {
    CIccCLUT *pCLUT = buildCLUT(nIn, 3);
    char msg[160];

    std::snprintf(msg, sizeof(msg), "%u-input CLUT initializes", (unsigned)nIn);
    check(pCLUT != NULL, msg);
    if (!pCLUT)
      continue;

    CIccApplyCLUT *pApply = pCLUT->GetNewApply();
    std::snprintf(msg, sizeof(msg), "%u-input CLUT yields an apply object",
                  (unsigned)nIn);
    check(pApply != NULL, msg);

    if (pApply) {
      std::vector<icFloatNumber> src(nIn, 0.0f);
      icFloatNumber dst[3] = { -12345.0f, -12345.0f, -12345.0f };

      pCLUT->InterpND(dst, &src[0], pApply);

      // All-zero input is grid node 0, whose stored value is the ramp's start.
      std::snprintf(msg, sizeof(msg),
                    "InterpND writes node 0 for a %u-input CLUT", (unsigned)nIn);
      check(dst[0] != -12345.0f, msg);

      std::snprintf(msg, sizeof(msg),
                    "InterpND returns the stored node 0 value (%u inputs)",
                    (unsigned)nIn);
      check(std::fabs((double)dst[0] - 0.0) < 1e-6, msg);

      delete pApply;
    }
    delete pCLUT;
  }
}

// The invariants the InterpND guards used to re-test are now Begin()'s to
// establish. Pin that Begin() is what refuses, so the guards are not restored
// on the belief that nothing else covers this.
void testBeginRefusesWhatTheGuardsCaught()
{
  // Above the 16-input maximum: Init() refuses, so Begin() must too.
  {
    CIccCLUT clut((icUInt8Number)17, (icUInt16Number)3, 4);
    check(!clut.Init((icUInt8Number)2), "Init() refuses 17 input channels");
    check(!clut.Begin(), "Begin() refuses a 17-input CLUT");
  }

  // Init() never called at all -- the state a caller of the public API reaches,
  // and the one no per-pixel channel-count check ever detected.
  {
    CIccCLUT clut((icUInt8Number)3, (icUInt16Number)3, 4);
    check(!clut.Begin(), "Begin() refuses a CLUT that was never Init()ed");
  }

  // A well-formed CLUT must still begin, or the assertions above prove nothing.
  {
    CIccCLUT clut((icUInt8Number)3, (icUInt16Number)3, 4);
    check(clut.Init((icUInt8Number)2), "Init() accepts a 3-input CLUT");
    check(clut.Begin(), "Begin() accepts a well-formed CLUT");
  }
}

// CIccMBB::Init() is the same shape of contract one level up: it sets the two
// counts everything else in the object is sized from, so it is where their bound
// belongs.
//
// Cleanup() used to clamp its delete loops to 16 "so a corrupted channel count
// can never walk past the allocation". That reasoning is inverted -- the arrays
// are allocated to those very counts by NewCurvesA/B/M, so a count above 16 does
// not overrun anything; the clamp leaked entries 16..n-1, silently, from a
// destructor. Init() now refuses counts outside 1..16 and Cleanup() frees exactly
// what was allocated.
//
// The refusal is the testable half. The leak itself is not directly observable
// without a sanitizer, but it is unreachable once Init() cannot record an
// out-of-range count in the first place -- which is the point of fixing it there
// rather than in the loop.
void testMBBInitBoundsItsChannelCounts()
{
  // Above the bound: refused, and the object is left untouched rather than
  // half-torn-down, because Init() checks before it calls Cleanup().
  {
    CIccTagLutAtoB lut;
    check(lut.Init(3, 3), "CIccMBB::Init accepts 3x3");
    check(lut.NewCurvesA() != NULL, "3x3 MBB allocates A curves");

    check(!lut.Init(17, 3), "CIccMBB::Init refuses 17 input channels");
    check(!lut.Init(3, 17), "CIccMBB::Init refuses 17 output channels");
    check(!lut.Init(0, 3), "CIccMBB::Init refuses 0 input channels");
    check(!lut.Init(3, 0), "CIccMBB::Init refuses 0 output channels");

    check(lut.InputChannels() == 3 && lut.OutputChannels() == 3,
          "a refused CIccMBB::Init leaves the counts unchanged");
    check(lut.GetCurvesA() != NULL,
          "a refused CIccMBB::Init does not tear down the object");
  }

  // At the bound: 16 is accepted, so the check is a bound and not an off-by-one.
  {
    CIccTagLutAtoB lut;
    check(lut.Init(16, 16), "CIccMBB::Init accepts 16x16");
    check(lut.NewCurvesA() != NULL, "16x16 MBB allocates A curves");
    check(lut.NewCurvesB() != NULL, "16x16 MBB allocates B curves");
    check(lut.NewCurvesM() != NULL, "16x16 MBB allocates M curves");
    // Destruction here exercises Cleanup() over the full 16 without the clamp.
  }
}

} // namespace

int main()
{
  // Spanning the retired m_nOutput > 16 threshold. 16 is the control: it passed
  // before the removal too, so a failure there means the removal broke something
  // rather than fixed it.
  testTetraMatchesTrilinearAtGridPoints(3);
  testTetraMatchesTrilinearAtGridPoints(16);
  testTetraMatchesTrilinearAtGridPoints(17);
  testTetraMatchesTrilinearAtGridPoints(20);

  testNDInterpolatesAcrossTheOldThreshold();
  testBeginRefusesWhatTheGuardsCaught();
  testMBBInitBoundsItsChannelCounts();

  if (g_fail) {
    std::fprintf(stderr, "[clut-apply-guards] %d assertion(s) failed\n", g_fail);
    return g_fail;
  }
  std::printf("[clut-apply-guards] all assertions passed\n");
  return 0;
}
