/*
    File:       pcs-edge-metadata.cpp

    Contains:   Regression test for issue #1748 -- CIccPcsXform::ConnectFirst()
                and ConnectLast() must describe the edge conversion they perform,
                the same way Connect() describes an interior connection.

    A CIccPcsXform built by CheckPCSConnections() is pushed onto the CMM's xform
    list and is then read like any other xform: CIccCmm::Begin() compares the
    last xform's GetNumDstSamples() against GetDestSamples() before allocating
    the apply chain.  Connect() sets m_srcSpace/m_nSrcSamples/m_dstSpace/
    m_nDstSamples; ConnectFirst() and ConnectLast() used to leave all four at the
    CIccXform defaults ('????' and 0 samples).  Any chain whose PCS edge space
    differs from the profile's own PCS routes through one of those two, so a
    Lab-PCS profile driven with an XYZ edge was rejected with
    icCmmStatBadSpaceLink even though the profile and the chain were both fine.

    The fixture is Testing/sRGB_v4_ICC_preference.icc: a tracked (not generated)
    v4 RGB profile whose PCS is Lab, which is exactly the mismatch needed to make
    CheckPCSConnections() take the ConnectFirst()/ConnectLast() branches.
*/

#include "IccCmm.h"
#include "IccProfile.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdio>

static const char *kProfile = "Testing/sRGB_v4_ICC_preference.icc";

static int g_failures = 0;

static void check(bool bCondition, const char *szMessage)
{
  if (bCondition) {
    std::printf("ok:   %s\n", szMessage);
  }
  else {
    std::printf("FAIL: %s\n", szMessage);
    g_failures++;
  }
}

// ---------------------------------------------------------------------------
// The four fields, read straight off a CIccPcsXform built by each of the three
// entry points.  Connect() is the control: it was always correct, so a failure
// there means something other than #1748 has moved.
// ---------------------------------------------------------------------------
static void testEdgeMetadata()
{
  CIccProfile *pProfile1 = OpenIccProfile(kProfile);
  CIccProfile *pProfile2 = OpenIccProfile(kProfile);
  if (!pProfile1 || !pProfile2) {
    std::printf("FAIL: cannot open %s\n", kProfile);
    g_failures++;
    delete pProfile1;
    delete pProfile2;
    return;
  }

  // pToXform runs PCS->device (Lab in, RGB out); pFromXform runs device->PCS.
  // CIccXform::Create takes ownership of the profile it is given.
  CIccXform *pToXform = CIccXform::Create(pProfile1, false, icPerceptual, icInterpTetrahedral);
  CIccXform *pFromXform = CIccXform::Create(pProfile2, true, icPerceptual, icInterpTetrahedral);
  if (!pToXform || !pFromXform) {
    std::printf("FAIL: cannot create xforms for %s\n", kProfile);
    g_failures++;
    delete pToXform;
    delete pFromXform;
    return;
  }
  pToXform->Begin();
  pFromXform->Begin();

  check(pToXform->GetSrcSpace() == icSigLabData, "fixture drives a Lab PCS (PCS->device side)");
  check(pFromXform->GetDstSpace() == icSigLabData, "fixture drives a Lab PCS (device->PCS side)");

  // Leading edge: XYZ from the caller into the Lab the next xform consumes.
  {
    CIccPcsXform pcs;
    icStatusCMM rv = pcs.ConnectFirst(pToXform, icSigXYZData);
    check(rv == icCmmStatOk, "ConnectFirst builds a real XYZ->Lab edge (not identity)");
    check(pcs.GetSrcSpace() == icSigXYZData, "ConnectFirst records the source space");
    check(pcs.GetNumSrcSamples() == 3, "ConnectFirst records the source sample count");
    check(pcs.GetDstSpace() == icSigLabData, "ConnectFirst records the destination space");
    check(pcs.GetNumDstSamples() == 3, "ConnectFirst records the destination sample count");
  }

  // Trailing edge: the Lab the previous xform produced out to the caller's XYZ.
  {
    CIccPcsXform pcs;
    icStatusCMM rv = pcs.ConnectLast(pFromXform, icSigXYZData);
    check(rv == icCmmStatOk, "ConnectLast builds a real Lab->XYZ edge (not identity)");
    check(pcs.GetSrcSpace() == icSigLabData, "ConnectLast records the source space");
    check(pcs.GetNumSrcSamples() == 3, "ConnectLast records the source sample count");
    check(pcs.GetDstSpace() == icSigXYZData, "ConnectLast records the destination space");
    check(pcs.GetNumDstSamples() == 3, "ConnectLast records the destination sample count");
  }

  // Control: an interior connection has always described itself correctly.
  {
    CIccPcsXform pcs;
    pcs.Connect(pFromXform, pToXform);
    check(pcs.GetNumSrcSamples() == 3 && pcs.GetNumDstSamples() == 3,
          "Connect() control still records both sample counts");
  }

  delete pToXform;
  delete pFromXform;
}

// ---------------------------------------------------------------------------
// The symptom.  A whole CMM whose destination space is XYZ while the profile's
// PCS is Lab appends a ConnectLast() edge, and CIccCmm::Begin() then compares
// that edge's output count against GetDestSamples().  With the count left at 0
// the comparison failed and Begin() returned icCmmStatBadSpaceLink.
// ---------------------------------------------------------------------------
static icStatusCMM beginChain(icColorSpaceSignature nSrcSpace,
                              icColorSpaceSignature nDstSpace,
                              bool bInputDir,
                              icFloatNumber *pDst, const icFloatNumber *pSrc)
{
  CIccCmm cmm(nSrcSpace, nDstSpace, bInputDir);
  icStatusCMM rv = cmm.AddXform(kProfile, icPerceptual, icInterpTetrahedral);
  if (rv != icCmmStatOk)
    return rv;
  rv = cmm.Begin();
  if (rv != icCmmStatOk)
    return rv;
  if (pDst && pSrc)
    return cmm.Apply(pDst, pSrc);
  return icCmmStatOk;
}

static void testChainBegin()
{
  const icFloatNumber rgb[3] = { 0.2f, 0.5f, 0.8f };
  icFloatNumber lab[3] = { 0 }, xyz[3] = { 0 };

  // The Lab edges match the profile's own PCS, so no CIccPcsXform is inserted;
  // these are the cases that already worked and must keep working.
  check(beginChain(icSigRgbData, icSigLabData, true, lab, rgb) == icCmmStatOk,
        "RGB->Lab chain (PCS matches, no edge xform) still begins and applies");

  // The XYZ edges are the ones #1748 broke.
  check(beginChain(icSigRgbData, icSigXYZData, true, xyz, rgb) == icCmmStatOk,
        "RGB->XYZ chain (ConnectLast edge) begins and applies");

  icFloatNumber rgbFromLab[3] = { 0 }, rgbFromXyz[3] = { 0 };
  check(beginChain(icSigLabData, icSigRgbData, false, rgbFromLab, lab) == icCmmStatOk,
        "Lab->RGB chain still begins and applies");
  check(beginChain(icSigXYZData, icSigRgbData, false, rgbFromXyz, xyz) == icCmmStatOk,
        "XYZ->RGB chain (ConnectFirst edge) begins and applies");

  // The two PCS routes describe the same colour, so a round trip through either
  // must land on the same device values.  This is what distinguishes a real fix
  // from one that merely satisfies the sample-count guard: the edge conversion
  // itself has to be right.  The tolerance is float round-off over a Lab<->XYZ
  // pair, not a modelling allowance.
  double dWorst = 0.0;
  for (int i = 0; i < 3; i++) {
    double d = std::fabs((double)rgbFromLab[i] - (double)rgbFromXyz[i]);
    if (d > dWorst)
      dWorst = d;
  }
  std::printf("      round trip via Lab = [%.6f %.6f %.6f]\n",
              (double)rgbFromLab[0], (double)rgbFromLab[1], (double)rgbFromLab[2]);
  std::printf("      round trip via XYZ = [%.6f %.6f %.6f]  max delta %.9f\n",
              (double)rgbFromXyz[0], (double)rgbFromXyz[1], (double)rgbFromXyz[2], dWorst);
  check(dWorst < 1.0e-4, "the XYZ edge agrees with the Lab edge to float round-off");
}

int main()
{
  testEdgeMetadata();
  testChainBegin();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }

  std::printf("\nall checks passed\n");
  return 0;
}
