/*
    File:       curve-setsize-contract.cpp

    Contains:   CTest helper for CIccTagCurve::SetSize()'s return-value contract
                and for CIccTagCurve::Read()'s handling of an over-cap on-disk
                curveType (#2006, part 4a).

    SetSize() used to answer a request above MAX_CURVE_ENTRIES the same way it
    answered SetSize(0): free the table, set m_nSize to 0 and return true.  Those
    two conditions mean opposite things.  Emptying a curve is a success -- a
    zero-entry curveType is ICC.1 10.6's encoding of the identity -- while an
    over-cap request is a refusal, and sharing one branch made the return value
    unable to tell them apart.  Eleven places in IccXML and IccJSON ended up
    defending against that independently -- eight compare m_nSize with the size
    they asked for, three null-check the data pointer instead -- and three of
    those eight only gained the guard in part 4b (#2008), having until then
    written the parsed samples through the NULL m_Curve the branch left behind.

    Two consequences are pinned here:

      * SetSize() reports an over-cap request as false, and leaves any existing
        table intact rather than destroying it on the way out.  Before 4a a
        refused resize silently discarded a perfectly good curve and reported
        success twice over.

      * CIccTagCurve::Read() rejects a curveType whose declared count exceeds the
        cap.  Read() has always checked SetSize()'s return, so the fix to the
        setter is what makes that check load-bearing; previously the tag loaded
        as a silent identity curve with its samples dropped and no error raised
        anywhere.  curveType is the only file-driven path that can reach the cap:
        lut8Type's tables are fixed at 256 entries, and lut16Type's counts are
        uInt16 and already rejected above 4096 by CIccTagLut16::Read.

    A declared count of 0 must keep working in both directions -- it is the
    identity encoding, not a malformed tag -- so the zero and boundary cases are
    checked alongside the refusal.

    Exit codes:
      0 - expected results observed
      1 - unexpected result
*/

#include "IccIO.h"
#include "IccTagLut.h"

#include <cstdio>

static int g_failures = 0;

// MAX_CURVE_ENTRIES is a plain integer literal; give it the type SetSize() and
// GetSize() actually use so none of the comparisons below is a signed/unsigned
// mismatch under the strict warning sets CI builds with.
static const icUInt32Number kCap = MAX_CURVE_ENTRIES;

static void check(bool condition, const char* label)
{
  if (condition) {
    std::fprintf(stdout, "curve-setsize-contract: PASS  %s\n", label);
    return;
  }

  std::fprintf(stderr, "curve-setsize-contract: FAIL  %s\n", label);
  g_failures++;
}

// Serialises a curveType tag: signature, reserved, count, then count uInt16
// samples.  Writing through CIccIO keeps the byte order right without the test
// having to hand-encode big-endian values.
static bool buildCurveTag(CIccMemIO& io, icUInt32Number nCount)
{
  const size_t nBytes = 3 * sizeof(icUInt32Number) +
                        (size_t)nCount * sizeof(icUInt16Number);

  if (!io.Alloc(nBytes, true))
    return false;

  icTagTypeSignature sig = icSigCurveType;
  icUInt32Number nReserved = 0;
  icUInt32Number nSize = nCount;

  if (!io.Write32(&sig) || !io.Write32(&nReserved) || !io.Write32(&nSize))
    return false;

  for (icUInt32Number i = 0; i < nCount; i++) {
    // Ramp the samples so a dropped table is distinguishable from a read one.
    icUInt16Number v = (icUInt16Number)(i & 0xffff);
    if (!io.Write16(&v))
      return false;
  }

  io.Seek(0, icSeekSet);
  return true;
}

// Drives CIccTagCurve::Read over a synthesised tag of nCount entries and reports
// whether the tag was accepted, along with the size the curve ended up with.
static void readCase(icUInt32Number nCount, bool bExpectAccept,
                     icUInt32Number nExpectSize, const char* label)
{
  CIccMemIO io;
  if (!buildCurveTag(io, nCount)) {
    std::fprintf(stderr, "curve-setsize-contract: SETUP FAIL  %s\n", label);
    g_failures++;
    return;
  }

  CIccTagCurve curve;
  const bool bRead = curve.Read((icUInt32Number)io.GetLength(), &io);

  check(bRead == bExpectAccept, label);
  if (bRead == bExpectAccept)
    check(curve.GetSize() == nExpectSize, label);
}

int main()
{
  // ---- SetSize(): an over-cap request is refused, and is not destructive ----
  {
    CIccTagCurve curve;
    const icUInt32Number nKept = 4096;

    check(curve.SetSize(nKept, icInitIdentity), "SetSize(4096) accepted");
    check(curve.GetSize() == nKept, "SetSize(4096) sized the table");

    icFloatNumber* pData = curve.GetData(0);
    check(pData != NULL, "SetSize(4096) allocated the table");
    const icFloatNumber vFirst = pData ? pData[0] : -1.0f;
    const icFloatNumber vLast = pData ? pData[nKept - 1] : -1.0f;

    // The refusal itself.  Before 4a this returned true.
    check(!curve.SetSize(kCap + 1),
          "SetSize(MAX_CURVE_ENTRIES+1) reports failure");

    // ...and it must not have taken the existing table down with it.  Before 4a
    // the curve was left empty with m_Curve NULL.
    check(curve.GetSize() == nKept,
          "refused SetSize left the existing size intact");
    check(curve.GetData(0) != NULL,
          "refused SetSize left the existing table allocated");
    if (curve.GetData(0) != NULL) {
      check(curve.GetData(0)[0] == vFirst && curve.GetData(0)[nKept - 1] == vLast,
            "refused SetSize left the existing samples intact");
    }
  }

  // ---- SetSize(): the boundary is accepted, and zero still empties ----
  {
    CIccTagCurve curve;
    check(curve.SetSize(kCap),
          "SetSize(MAX_CURVE_ENTRIES) accepted");
    check(curve.GetSize() == kCap,
          "SetSize(MAX_CURVE_ENTRIES) sized the table");

    check(curve.SetSize(0), "SetSize(0) reports success");
    check(curve.GetSize() == 0, "SetSize(0) emptied the table");
    check(curve.GetData(0) == NULL, "SetSize(0) released the table");
  }

  // ---- Read(): an over-cap declared count is rejected ----
  // Before 4a this returned true with an empty curve: SetSize() freed the table
  // and reported success, and Read()'s `if (m_nSize)` then skipped the samples.
  readCase(kCap + 1, false, 0,
           "Read rejects a curveType declaring MAX_CURVE_ENTRIES+1 entries");

  // ---- Read(): the boundary and the identity encoding still load ----
  readCase(kCap, true, kCap,
           "Read accepts a curveType declaring MAX_CURVE_ENTRIES entries");
  readCase(0, true, 0,
           "Read accepts a zero-entry curveType as the identity");
  readCase(256, true, 256,
           "Read accepts an ordinary 256-entry curveType");

  if (g_failures) {
    std::fprintf(stderr, "curve-setsize-contract: %d check(s) failed\n",
                 g_failures);
    return 1;
  }

  std::fprintf(stdout, "curve-setsize-contract: all checks passed\n");
  return 0;
}
