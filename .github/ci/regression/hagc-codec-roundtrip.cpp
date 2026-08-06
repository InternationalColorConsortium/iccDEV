// Codec regression for the headroomAdaptiveGainCurveTag ('HAGC') and its
// headroomAdaptiveGainCurveType ('hagc'), which wrap SMPTE ST 2094-50:2026
// metadata in a 12 byte ICC header.
//
// The wire format is variable length in four independent ways -- a custom HDR
// reference white shifts every later field by two bytes, the chromaticity array
// is present only for mode 3, the coefficient array's length is the number of
// set flag bits rather than a stored count, and the Common Component Mixing /
// Common Curve Parameters flags delete whole fields from every alternate after
// the first. None of those lengths is written down anywhere in the block, so a
// decoder that mis-reads one field does not fail: it silently reframes every
// field after it and returns a plausible curve. Byte equality across
// Pack -> Unpack -> Pack is what catches that, because a reframed decode
// re-encodes to a different length.
//
// The boundary cases matter for the same reason in the other direction. Each
// decode formula clamps in the integer domain and then divides (proposal
// 1.2.2.3/1.2.2.4/1.2.2.7, 1.1.3.2.1, 0.1.3.5/0.1.3.6/0.1.3.9). Clamping after
// the divide instead rounds differently at the limits, and the encode side has
// to be the exact inverse over the whole representable range or the round trip
// above is not byte stable. Both directions are pinned here at their limits.
//
// The tag envelope is checked separately: Read() must retain the SMPTE bytes
// verbatim so a block this decoder cannot parse still survives a load/save
// cycle, and Write() must emit its own pad, because CIccProfile records the tag
// size *before* calling Align32() and annex 1 note 2 requires the recorded
// length to be a multiple of four.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccTagHagc.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccDefs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool cond, const char *szWhat)
{
  if (!cond) {
    printf("FAIL: %s\n", szWhat);
    g_failures++;
  }
}

void checkClose(double got, double want, double tol, const char *szWhat)
{
  if (!(fabs(got - want) <= tol)) {
    printf("FAIL: %s (got %.9g, want %.9g)\n", szWhat, got, want);
    g_failures++;
  }
}

std::string hex(const std::vector<icUInt8Number> &v)
{
  std::string s;
  char buf[8];
  for (size_t i = 0; i < v.size(); i++) {
    snprintf(buf, sizeof(buf), "%02x", v[i]);
    s += buf;
  }
  return s;
}

// Pack, decode the result, re-pack, and require the two encodings to agree.
// The second Pack() runs off a model that was reconstructed purely from bytes,
// so any field the decoder placed at the wrong offset changes the length or the
// content of the re-encoding.
void checkRoundTrip(const icHagcMetadata &src, const char *szWhat)
{
  std::vector<icUInt8Number> a, b;

  if (!src.Pack(a)) {
    printf("FAIL: %s - initial Pack failed\n", szWhat);
    g_failures++;
    return;
  }

  icHagcMetadata decoded;
  if (!decoded.Unpack(a.empty() ? NULL : &a[0], (icUInt32Number)a.size())) {
    printf("FAIL: %s - Unpack of %u bytes failed (%s)\n",
           szWhat, (unsigned)a.size(), hex(a).c_str());
    g_failures++;
    return;
  }

  if (decoded.m_nTrailingBytes) {
    printf("FAIL: %s - Unpack left %u trailing bytes\n", szWhat, decoded.m_nTrailingBytes);
    g_failures++;
  }

  if (!decoded.Pack(b)) {
    printf("FAIL: %s - re-Pack failed\n", szWhat);
    g_failures++;
    return;
  }

  if (a != b) {
    printf("FAIL: %s - Pack/Unpack/Pack differ\n  first:  %s\n  second: %s\n",
           szWhat, hex(a).c_str(), hex(b).c_str());
    g_failures++;
  }
}

// A two-alternate model with every optional field present: custom reference
// white, custom chromaticities, custom component mixing with a partly populated
// coefficient array, and explicit slope angles. This is the longest layout the
// format can produce, so it exercises every conditional branch of both codecs.
icHagcMetadata makeFullModel()
{
  icHagcMetadata m;
  int i;

  m.m_nApplicationVersion = 3;
  m.m_nMinApplicationVersion = 1;

  m.m_bCustomReferenceWhite = true;
  m.m_referenceWhite = 300.0f;
  m.m_bHeadroomAdaptiveToneMap = true;
  m.m_baselineHeadroom = 3.0f;

  m.m_nChromaticitiesMode = icHagcChromaticitiesCustom;
  const icFloatNumber chrom[8] = { 0.708f, 0.292f, 0.170f, 0.797f,
                                   0.131f, 0.046f, 0.3127f, 0.3290f };
  for (i = 0; i < 8; i++)
    m.m_chromaticities[i] = chrom[i];

  m.SetNumAlternates(2);

  // Alternate 0 tone maps *down* from the 3.0 stop baseline, so every Y is
  // negative (proposal 0.1.3.6 fixes the sign from the headroom ordering).
  icHagcAlternateImage *a0 = m.GetAlternate(0);
  a0->m_headroom = 0.0f;
  a0->m_nMixingType = icHagcMixingCustom;
  a0->m_coef[icHagcCoefRed] = 0.25f;
  a0->m_coef[icHagcCoefGreen] = 0.5f;
  a0->m_coef[icHagcCoefBlue] = 0.0f;      // zero: omitted from the array
  a0->m_coef[icHagcCoefMax] = 0.25f;
  a0->m_coef[icHagcCoefMin] = 0.0f;       // zero: omitted from the array
  a0->m_coef[icHagcCoefComponent] = 0.0f; // zero: omitted from the array
  a0->m_nControlPoints = 4;
  a0->m_bPchipSlope = false;
  for (i = 0; i < 4; i++) {
    a0->m_x[i] = (icFloatNumber)(0.25 * (i + 1));
    a0->m_y[i] = (icFloatNumber)(-0.1 * (i + 1));
    a0->m_slope[i] = (icFloatNumber)(-0.4 + 0.1 * i);
  }

  // Alternate 1 sits above the baseline, so its Y values are positive, and it
  // uses a fixed mixing type and derived (PCHIP) slopes.
  icHagcAlternateImage *a1 = m.GetAlternate(1);
  a1->m_headroom = 5.0f;
  a1->m_nMixingType = icHagcMixingWeighted;
  a1->m_coef[icHagcCoefRed] = a1->m_coef[icHagcCoefGreen] = a1->m_coef[icHagcCoefBlue] =
    (icFloatNumber)(1.0 / 6.0);
  a1->m_coef[icHagcCoefMax] = 0.5f;
  a1->m_nControlPoints = 3;
  a1->m_bPchipSlope = true;
  for (i = 0; i < 3; i++) {
    a1->m_x[i] = (icFloatNumber)(0.5 * (i + 1));
    a1->m_y[i] = (icFloatNumber)(0.2 * (i + 1));
  }

  return m;
}

// ---------------------------------------------------------------------------
// 1. Pack -> Unpack -> Pack byte equality across the layout's shape space
// ---------------------------------------------------------------------------
void testRoundTrips()
{
  icHagcMetadata full = makeFullModel();
  checkRoundTrip(full, "full model (custom white, custom chromaticities, mixing type 3)");

  // No custom reference white: the "k" offset in proposal Table 2 collapses to
  // zero and everything after byte 0 moves two bytes earlier.
  icHagcMetadata noWhite = full;
  noWhite.m_bCustomReferenceWhite = false;
  checkRoundTrip(noWhite, "no custom reference white");

  // Named chromaticities: the sixteen byte array disappears entirely.
  icHagcMetadata named = full;
  named.m_nChromaticitiesMode = icHagcChromaticitiesBT2020;
  checkRoundTrip(named, "named chromaticities (no custom array)");

  // Common Component Mixing: alternate 1 loses its mixing byte and inherits
  // alternate 0's type 3 coefficients, which is a different array length than
  // the fixed set it would otherwise have carried.
  icHagcMetadata commonMix = full;
  commonMix.m_bCommonComponentMixing = true;
  checkRoundTrip(commonMix, "common component mixing");

  // Common Curve Parameters: alternate 1 loses the count/PCHIP byte and the X
  // array, so its Y array length is governed by alternate 0's count. Its own
  // count and PCHIP flag are set to disagree on purpose -- Pack() must use
  // alternate 0's, or the block it writes will not decode back to itself.
  icHagcMetadata commonCurve = full;
  commonCurve.m_bCommonCurveParameters = true;
  checkRoundTrip(commonCurve, "common curve parameters (alternates disagree)");

  icHagcMetadata commonBoth = full;
  commonBoth.m_bCommonComponentMixing = true;
  commonBoth.m_bCommonCurveParameters = true;
  checkRoundTrip(commonBoth, "both common flags");

  // Zero alternates: proposal 1.2.2.6 says no tone mapping is performed. The
  // block is header plus global parameters and nothing else.
  icHagcMetadata none = full;
  none.SetNumAlternates(0);
  checkRoundTrip(none, "zero alternate images");

  // Four alternates, the maximum the proposal allows.
  icHagcMetadata four = full;
  four.SetNumAlternates(4);
  for (int i = 2; i < 4; i++) {
    icHagcAlternateImage *a = four.GetAlternate((icUInt8Number)i);
    a->m_headroom = (icFloatNumber)(6.0 + i);
    a->m_nMixingType = icHagcMixingMax;
    a->m_coef[icHagcCoefMax] = 1.0f;
    a->m_nControlPoints = 2;
    a->m_bPchipSlope = false;
    a->m_x[0] = 0.5f;  a->m_y[0] = 0.1f;  a->m_slope[0] = 0.0f;
    a->m_x[1] = 1.5f;  a->m_y[1] = 0.3f;  a->m_slope[1] = 1.0f;
  }
  checkRoundTrip(four, "four alternate images");

  // Single control point: the smallest curve the 5 bit last index can express.
  icHagcMetadata onePoint = full;
  onePoint.SetNumAlternates(1);
  onePoint.GetAlternate(0)->m_nControlPoints = 1;
  checkRoundTrip(onePoint, "one control point");

  // Thirty-two control points: the largest.
  icHagcMetadata maxPoints = full;
  maxPoints.SetNumAlternates(1);
  {
    icHagcAlternateImage *a = maxPoints.GetAlternate(0);
    a->m_nControlPoints = icHagcMaxControlPoints;
    a->m_bPchipSlope = false;
    for (int i = 0; i < icHagcMaxControlPoints; i++) {
      a->m_x[i] = (icFloatNumber)(0.1 * (i + 1));
      a->m_y[i] = (icFloatNumber)(-0.05 * (i + 1));
      a->m_slope[i] = (icFloatNumber)(-1.0 + 0.0625 * i);
    }
  }
  checkRoundTrip(maxPoints, "thirty-two control points");

  // Reference White Tone Mapping: proposal 1.2.2.5 says the rest of the byte is
  // zero and no records follow, so the block ends immediately after it.
  icHagcMetadata refWhite = full;
  refWhite.m_bReferenceWhiteToneMapping = true;
  checkRoundTrip(refWhite, "reference white tone mapping (no records follow)");
}

// ---------------------------------------------------------------------------
// 2. Decode formulas at their clamp boundaries
// ---------------------------------------------------------------------------
//
// These are driven through Unpack() rather than by calling the static decoders,
// because the decoders are file-local: what is under test is the composition of
// "read this field at this offset" with "apply this formula", and testing the
// formula in isolation would not catch a field read at the wrong offset.

// Build a minimal block by hand: general info byte, global flag byte, optional
// reference white, baseline headroom, and the byte k+3 with zero alternates.
std::vector<icUInt8Number> makeMinimalBlock(bool bCustomWhite, icUInt16Number nWhite,
                                            icUInt16Number nBaseline)
{
  std::vector<icUInt8Number> v;
  v.push_back(0x00);                                   // app versions + reserved
  v.push_back((icUInt8Number)(bCustomWhite ? 0x80 : 0x00)); // custom white flag
  if (bCustomWhite) {
    v.push_back((icUInt8Number)(nWhite >> 8));
    v.push_back((icUInt8Number)(nWhite & 0xff));
  }
  v.push_back((icUInt8Number)(nBaseline >> 8));
  v.push_back((icUInt8Number)(nBaseline & 0xff));
  v.push_back(0x00);                                   // no ref-white TM, 0 alternates
  return v;
}

void testGlobalBoundaries()
{
  icHagcMetadata m;
  std::vector<icUInt8Number> v;

  // Proposal 1.2.2.3: MIN(MAX(1, v), 50000)/5.0. The MAX(1, ...) means an
  // encoded zero decodes to 0.2, not to 0.
  v = makeMinimalBlock(true, 0, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: reference white 0");
  checkClose(m.GetReferenceWhite(), 0.2, 1e-6, "reference white 0 -> 0.2");

  v = makeMinimalBlock(true, 1015, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: reference white 1015");
  checkClose(m.GetReferenceWhite(), 203.0, 1e-4, "reference white 1015 -> 203.0");

  v = makeMinimalBlock(true, 50000, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: reference white 50000");
  checkClose(m.GetReferenceWhite(), 10000.0, 1e-3, "reference white 50000 -> 10000.0");

  v = makeMinimalBlock(true, 65535, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: reference white 65535");
  checkClose(m.GetReferenceWhite(), 10000.0, 1e-3, "reference white 65535 clamps to 10000.0");

  // Absent custom value: proposal 1.2.2.3 defaults it to 203.0.
  v = makeMinimalBlock(false, 0, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: default reference white");
  check(!m.m_bCustomReferenceWhite, "default reference white: flag clear");
  checkClose(m.GetReferenceWhite(), 203.0, 1e-6, "absent reference white -> 203.0");

  // Proposal 1.2.2.4: MIN(v, 60000)/10000.0, no MAX, so zero stays zero.
  v = makeMinimalBlock(false, 0, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: baseline headroom 0");
  checkClose(m.m_baselineHeadroom, 0.0, 1e-9, "baseline headroom 0 -> 0.0");

  v = makeMinimalBlock(false, 0, 60000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: baseline headroom 60000");
  checkClose(m.m_baselineHeadroom, 6.0, 1e-5, "baseline headroom 60000 -> 6.0");

  v = makeMinimalBlock(false, 0, 65535);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: baseline headroom 65535");
  checkClose(m.m_baselineHeadroom, 6.0, 1e-5, "baseline headroom 65535 clamps to 6.0");
}

// A one-alternate block with an explicit control point, used to probe the X, Y
// and slope formulas at their limits. Baseline headroom is left at 0 and the
// alternate headroom at 1, so the Y sign is +1 (proposal 0.1.3.6).
std::vector<icUInt8Number> makeOnePointBlock(icUInt16Number nX, icUInt16Number nY,
                                             icUInt16Number nSlope)
{
  std::vector<icUInt8Number> v;
  v.push_back(0x00);                        // app versions + reserved
  v.push_back(0x00);                        // no custom white, no adaptive flag
  v.push_back(0x00); v.push_back(0x00);     // baseline headroom = 0
  v.push_back(0x10);                        // 0 001 00 0 0: no ref-white TM, 1 alternate,
                                            // BT.709 chromaticities, neither common flag
  v.push_back(0x27); v.push_back(0x10);     // alternate headroom = 10000 -> 1.0
  v.push_back(0x40);                        // mixing type 1, no coefficient flags
  v.push_back(0x00);                        // last index 0, PCHIP off, reserved 0
  v.push_back((icUInt8Number)(nX >> 8));     v.push_back((icUInt8Number)(nX & 0xff));
  v.push_back((icUInt8Number)(nY >> 8));     v.push_back((icUInt8Number)(nY & 0xff));
  v.push_back((icUInt8Number)(nSlope >> 8)); v.push_back((icUInt8Number)(nSlope & 0xff));
  return v;
}

void testControlPointBoundaries()
{
  icHagcMetadata m;
  std::vector<icUInt8Number> v;
  const icHagcAlternateImage *a;

  // Proposal 0.1.3.5: X = MIN(X, 64000)/1000.0
  v = makeOnePointBlock(0, 0, 18000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: X boundary block");
  a = m.GetAlternate(0);
  check(a != NULL, "X boundary: alternate present");
  if (a) {
    checkClose(a->m_x[0], 0.0, 1e-9, "X 0 -> 0.0");
    checkClose(a->m_slope[0], 0.0, 1e-9, "slope 18000 -> tan(0) = 0.0");
  }

  v = makeOnePointBlock(64000, 0, 18000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: X 64000");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_x[0], 64.0, 1e-4, "X 64000 -> 64.0");

  v = makeOnePointBlock(65535, 0, 18000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: X 65535");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_x[0], 64.0, 1e-4, "X 65535 clamps to 64.0");

  // Proposal 0.1.3.6: Y = s * MIN(Y, 60000)/10000.0, s = +1 here.
  v = makeOnePointBlock(1000, 60000, 18000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: Y 60000");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_y[0], 6.0, 1e-5, "Y 60000 -> +6.0 (alternate above baseline)");

  v = makeOnePointBlock(1000, 65535, 18000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: Y 65535");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_y[0], 6.0, 1e-5, "Y 65535 clamps to +6.0");

  // Proposal 0.1.3.9: M = tan((MIN(MAX(1, t), 35999) - 18000) * pi/36000).
  // theta 27000 is +45 degrees; 9000 is -45 degrees.
  v = makeOnePointBlock(1000, 0, 27000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: slope 27000");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_slope[0], 1.0, 1e-5, "slope 27000 -> tan(45 deg) = 1.0");

  v = makeOnePointBlock(1000, 0, 9000);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: slope 9000");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_slope[0], -1.0, 1e-5, "slope 9000 -> tan(-45 deg) = -1.0");

  // theta 0 is clamped up to 1 by the MAX, which is what stops the angle from
  // reaching exactly -90 degrees where the tangent is undefined.
  v = makeOnePointBlock(1000, 0, 0);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: slope 0");
  a = m.GetAlternate(0);
  if (a) {
    check(a->m_slope[0] < -11000.0f, "slope 0 clamps to theta 1, a large negative finite slope");
    check(a->m_slope[0] == a->m_slope[0], "slope 0 is not NaN");
  }

  // theta 65535 clamps down to 35999, symmetrically just short of +90 degrees.
  v = makeOnePointBlock(1000, 0, 65535);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: slope 65535");
  a = m.GetAlternate(0);
  if (a) {
    check(a->m_slope[0] > 11000.0f, "slope 65535 clamps to theta 35999, a large positive finite slope");
    check(a->m_slope[0] == a->m_slope[0], "slope 65535 is not NaN");
  }

  // The Y sign follows the headroom ordering, not the encoded value. Flipping
  // the baseline above the alternate must flip every decoded Y.
  v = makeOnePointBlock(1000, 10000, 18000);
  v[2] = 0x4e; v[3] = 0x20;   // baseline headroom = 20000 -> 2.0, above the alternate's 1.0
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "unpack: baseline above alternate");
  a = m.GetAlternate(0);
  if (a)
    checkClose(a->m_y[0], -1.0, 1e-5, "Y sign is negative when baseline >= alternate headroom");
}

// ---------------------------------------------------------------------------
// 3. Malformed input
// ---------------------------------------------------------------------------
void testMalformed()
{
  icHagcMetadata m;
  std::vector<icUInt8Number> v;

  check(!m.Unpack(NULL, 0), "null block is refused");
  check(!m.m_bUnpacked, "null block leaves m_bUnpacked false");

  v = makeOnePointBlock(1000, 10000, 18000);

  // Truncate one byte at a time from the end. Every prefix is short by at least
  // half of a uInt16 field, so every one of them must be refused rather than
  // returning a curve built from whatever the last partial field decoded to.
  for (size_t n = 1; n < v.size(); n++) {
    icHagcMetadata t;
    if (t.Unpack(&v[0], (icUInt32Number)n)) {
      printf("FAIL: truncated block of %u bytes was accepted\n", (unsigned)n);
      g_failures++;
      break;
    }
    if (t.m_bUnpacked || t.GetNumAlternates()) {
      printf("FAIL: truncated block of %u bytes left state behind\n", (unsigned)n);
      g_failures++;
      break;
    }
  }

  // An alternate count of 5 to 7 fits the three bit field but exceeds the
  // proposal's maximum of 4. Decoding only the first four would leave the
  // cursor mid record, so the whole block has to be refused.
  for (int n = 5; n <= 7; n++) {
    std::vector<icUInt8Number> bad = makeMinimalBlock(false, 0, 0);
    bad[bad.size() - 1] = (icUInt8Number)(n << 4);
    icHagcMetadata t;
    if (t.Unpack(&bad[0], (icUInt32Number)bad.size())) {
      printf("FAIL: alternate count %d was accepted\n", n);
      g_failures++;
    }
  }

  // Trailing bytes are recorded rather than rejected: the block decoded, it is
  // simply longer than the layout accounts for, and Validate() reports it.
  v = makeOnePointBlock(1000, 10000, 18000);
  v.push_back(0x00);
  v.push_back(0x00);
  check(m.Unpack(&v[0], (icUInt32Number)v.size()), "block with trailing bytes still decodes");
  check(m.m_nTrailingBytes == 2, "two trailing bytes are counted");
}

// ---------------------------------------------------------------------------
// 4. Tag envelope: raw retention, pad, and Write/Read byte equality
// ---------------------------------------------------------------------------
void testTagEnvelope()
{
  // A metadata block whose length is not a multiple of four, so the tag has to
  // pad. makeOnePointBlock() produces 15 bytes.
  std::vector<icUInt8Number> meta = makeOnePointBlock(1000, 10000, 18000);
  check((meta.size() & 3) != 0, "fixture metadata length is deliberately unaligned");

  CIccTagHagc tag;
  check(tag.SetRawMetadata(&meta[0], (icUInt32Number)meta.size()), "SetRawMetadata succeeds");
  check(tag.GetRawMetadataSize() == meta.size(), "raw size matches");
  check(memcmp(tag.GetRawMetadata(), &meta[0], meta.size()) == 0, "raw bytes match");
  check(tag.GetMetadata().m_bUnpacked, "raw block decoded");

  CIccMemIO io;
  const size_t nBuf = 256;
  check(io.Alloc(nBuf, true), "memory IO allocated");
  check(tag.Write(&io), "tag writes");

  size_t nWritten = io.GetLength();

  // Annex 1 note 2: the tag's overall length must be a multiple of four. It is
  // written here rather than left to CIccProfile::Write(), which records the
  // tag size before calling Align32() -- so a tag that did not pad itself would
  // have an unaligned length recorded in the directory.
  check((nWritten & 3) == 0, "tag length is a multiple of four");
  check(nWritten == 12 + meta.size() + 1, "tag length is header + metadata + one pad byte");

  check(io.Seek(0, icSeekSet) == 0, "rewound for read-back");

  CIccTagHagc readBack;
  check(readBack.Read((icUInt32Number)nWritten, &io), "tag reads back");
  check(readBack.GetRawMetadataSize() == meta.size(), "read-back raw size matches");
  check(readBack.GetRawMetadata() != NULL &&
        memcmp(readBack.GetRawMetadata(), &meta[0], meta.size()) == 0,
        "read-back raw bytes are byte identical");
  check(readBack.GetPadSize() == 1, "one pad byte seen on read");
  check(!readBack.HasNonZeroPad(), "pad bytes are null");
  check(readBack.GetMetadata().m_bUnpacked, "read-back decoded");

  // A block this decoder cannot parse must still survive a load/save cycle
  // verbatim -- that is the whole point of retaining the raw bytes, and it is
  // what lets an ICC file carry a SMPTE revision this build predates.
  // The blob has to be chosen, not just filled with 0xff: a block of seven 0xff
  // bytes decodes cleanly, because byte 6 sets the Reference White Tone Mapping
  // flag and proposal 1.2.2.5 ends the block right there. An alternate image
  // count of 5 is undecodable for a reason no future revision can retract -- the
  // proposal caps the count at 4 while the field is three bits wide.
  icUInt8Number opaque[5] = { 0x00, 0x00, 0x00, 0x00, 0x50 };
  CIccTagHagc opaqueTag;
  check(opaqueTag.SetRawMetadata(opaque, sizeof(opaque)), "opaque block stored");
  check(!opaqueTag.GetMetadata().m_bUnpacked, "opaque block does not decode");
  check(opaqueTag.GetRawMetadataSize() == sizeof(opaque), "opaque block retained whole");

  CIccMemIO io2;
  check(io2.Alloc(nBuf, true), "second memory IO allocated");
  check(opaqueTag.Write(&io2), "opaque tag writes");
  size_t nOpaque = io2.GetLength();
  check((nOpaque & 3) == 0, "opaque tag length is a multiple of four");
  check(io2.Seek(0, icSeekSet) == 0, "rewound for opaque read-back");

  CIccTagHagc opaqueBack;
  check(opaqueBack.Read((icUInt32Number)nOpaque, &io2), "opaque tag reads back");
  check(opaqueBack.GetRawMetadataSize() == sizeof(opaque) &&
        opaqueBack.GetRawMetadata() != NULL &&
        memcmp(opaqueBack.GetRawMetadata(), opaque, sizeof(opaque)) == 0,
        "undecodable block round-trips byte identically");

  // NewCopy() must deep copy the raw block; a shared buffer would tie the
  // copy's lifetime to the original's.
  CIccTag *pCopy = readBack.NewCopy();
  check(pCopy != NULL, "NewCopy returns a tag");
  CIccTagHagc *pHagcCopy = dynamic_cast<CIccTagHagc *>(pCopy);
  check(pHagcCopy != NULL, "NewCopy returns a CIccTagHagc");
  if (pHagcCopy) {
    check(pHagcCopy->GetRawMetadata() != readBack.GetRawMetadata(), "copy owns its own buffer");
    check(pHagcCopy->GetRawMetadataSize() == meta.size() &&
          memcmp(pHagcCopy->GetRawMetadata(), &meta[0], meta.size()) == 0,
          "copy carries the same bytes");
    check(pHagcCopy->GetMetadata().GetNumAlternates() ==
          readBack.GetMetadata().GetNumAlternates(), "copy carries the same model");
  }
  delete pCopy;

  // SetMetadata() is the authoring path: it packs the model and the packed
  // bytes become the tag's raw block, so a tag built in memory writes the same
  // way as one that was read from a file.
  icHagcMetadata model = makeFullModel();
  CIccTagHagc authored;
  check(authored.SetMetadata(model), "SetMetadata packs and installs");
  check(authored.GetRawMetadataSize() > 0, "authored tag has raw bytes");
  check(authored.GetMetadata().m_bUnpacked, "authored tag reports decoded");

  std::vector<icUInt8Number> packed;
  check(model.Pack(packed), "model packs independently");
  check(packed.size() == authored.GetRawMetadataSize() &&
        memcmp(&packed[0], authored.GetRawMetadata(), packed.size()) == 0,
        "SetMetadata stores exactly what Pack produces");
}

// ---------------------------------------------------------------------------
// 5. Validate() reports the defects it is meant to
// ---------------------------------------------------------------------------
void testValidate()
{
  std::string report;

  // A clean tag, with no profile context, must not report anything.
  icHagcMetadata clean = makeFullModel();
  CIccTagHagc cleanTag;
  check(cleanTag.SetMetadata(clean), "clean model installs");
  report.clear();
  check(cleanTag.Validate("HAGC", report, NULL) == icValidateOK,
        "clean tag validates OK");
  if (!report.empty())
    printf("  (clean tag report: %s)\n", report.c_str());

  // Non-increasing X makes the piecewise cubic's t = (x - x_i)/(x_i+1 - x_i)
  // divide by zero, so it is a defect and not merely untidy ordering.
  icHagcMetadata badX = makeFullModel();
  badX.GetAlternate(0)->m_x[2] = badX.GetAlternate(0)->m_x[1];
  CIccTagHagc badXTag;
  badXTag.SetMetadata(badX);
  report.clear();
  check(badXTag.Validate("HAGC", report, NULL) == icValidateNonCompliant,
        "non-increasing control point X is non-compliant");
  check(report.find("strictly increasing") != std::string::npos,
        "non-increasing X names the defect");

  // Mixing type 3 with every coefficient zero makes p_sum zero, which the
  // informative annex says cannot happen because the mixing divides by it.
  icHagcMetadata zeroSum = makeFullModel();
  zeroSum.SetNumAlternates(1);
  {
    icHagcAlternateImage *a = zeroSum.GetAlternate(0);
    a->m_nMixingType = icHagcMixingCustom;
    for (int i = 0; i < icHagcNumCoefficients; i++)
      a->m_coef[i] = 0.0f;
  }
  CIccTagHagc zeroSumTag;
  zeroSumTag.SetMetadata(zeroSum);
  report.clear();
  check(zeroSumTag.Validate("HAGC", report, NULL) == icValidateNonCompliant,
        "mixing type 3 with a zero coefficient sum is non-compliant");

  // A tag whose metadata will not decode reports that, rather than silently
  // validating an empty model as if it were a well formed zero-alternate tag.
  icUInt8Number opaque[5] = { 0x00, 0x00, 0x00, 0x00, 0x50 };
  CIccTagHagc opaqueTag;
  opaqueTag.SetRawMetadata(opaque, sizeof(opaque));
  report.clear();
  check(opaqueTag.Validate("HAGC", report, NULL) == icValidateNonCompliant,
        "undecodable metadata is non-compliant");
  check(report.find("could not be decoded") != std::string::npos,
        "undecodable metadata names the defect");
}

} // namespace

int main()
{
  testRoundTrips();
  testGlobalBoundaries();
  testControlPointBoundaries();
  testMalformed();
  testTagEnvelope();
  testValidate();

  if (g_failures)
    printf("hagc-codec-roundtrip: %d assertion(s) failed\n", g_failures);
  else
    printf("hagc-codec-roundtrip: all assertions passed\n");

  return g_failures;
}
