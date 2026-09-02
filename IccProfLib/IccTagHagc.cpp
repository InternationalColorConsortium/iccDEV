/** @file
    File:       IccTagHagc.cpp

    Contains:   Implementation of the CIccTagHagc class and of the
                SMPTE ST 2094-50:2026 metadata model it wraps

    Version:    V1

    Copyright:  (c) see Software License
*/

/*
 * Copyright (c) International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation of headroomAdaptiveGainCurveTag/Type
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
  #include <windows.h>
#endif
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>

#include "IccTagHagc.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccProfile.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/* M_PI is not in the C++ standard headers and MSVC hides it behind
 * _USE_MATH_DEFINES, so IccProfLib does not use it anywhere.  The slope angle
 * decode needs pi in double precision, so name it locally rather than making
 * this the one translation unit with a platform dependent math header. */
static const double icHagcPi = 3.14159265358979323846;

/* Size of the fixed headroomAdaptiveGainCurveType header that precedes the
 * SMPTE ST 2094-50:2026 metadata: type signature, reserved, metadata size
 * (proposal Table 1).  Note that Table 1 counts the General Information byte
 * at offset 12 as part of the header, but the same table also says "the
 * metadata defined as per SMPTE ST 2094-50:2026 begins at byte offset 12", so
 * that byte belongs to the metadata block and is the first thing Unpack()
 * reads.  Getting this boundary wrong shifts every subsequent field by one. */
#define icHagcHeaderSize 12

/**
 ****************************************************************************
 * Class: CIccHagcBitReader
 *
 * Purpose: Bounds checked, most significant bit first cursor over the
 *  metadata block.
 *
 *  Every field in the layout is either a whole uInt8/uInt16 or a bit field
 *  inside one byte, and the bit fields in each byte always sum to eight, so
 *  the cursor is byte aligned at every field boundary.  Reading through a bit
 *  cursor rather than masking bytes by hand keeps the field order in the code
 *  in the same order as the tables in the proposal, and makes running off the
 *  end of the block a single checked condition instead of one per field.
 ****************************************************************************
 */
class CIccHagcBitReader
{
public:
  CIccHagcBitReader(const icUInt8Number *pData, icUInt32Number nSize)
    : m_pData(pData), m_nBits((icUInt64Number)nSize * 8), m_nPos(0) {}

  /** Read nBits (1..32) MSB first.  Returns false, and consumes nothing, if
   * the block does not have that many bits left. */
  bool Read(unsigned nBits, icUInt32Number &val)
  {
    if (nBits > 32 || m_nPos + nBits > m_nBits)
      return false;

    icUInt32Number v = 0;
    for (unsigned i = 0; i < nBits; i++) {
      icUInt64Number bit = m_nPos + i;
      icUInt8Number byte = m_pData[bit >> 3];
      v = (v << 1) | ((byte >> (7 - (bit & 7))) & 1);
    }
    m_nPos += nBits;
    val = v;
    return true;
  }

  bool ReadFlag(bool &val)
  {
    icUInt32Number v;
    if (!Read(1, v))
      return false;
    val = (v != 0);
    return true;
  }

  bool ReadU8(icUInt8Number &val)
  {
    icUInt32Number v;
    if (!Read(8, v))
      return false;
    val = (icUInt8Number)v;
    return true;
  }

  bool ReadU16(icUInt16Number &val)
  {
    icUInt32Number v;
    if (!Read(16, v))
      return false;
    val = (icUInt16Number)v;
    return true;
  }

  /** Bytes consumed so far, rounded up.  The cursor is byte aligned at every
   * field boundary, so this is exact wherever a caller asks for it. */
  icUInt32Number BytesConsumed() const { return (icUInt32Number)((m_nPos + 7) / 8); }

protected:
  const icUInt8Number *m_pData;
  icUInt64Number m_nBits;
  icUInt64Number m_nPos;
};

/**
 ****************************************************************************
 * Class: CIccHagcBitWriter
 *
 * Purpose: The Pack() counterpart of CIccHagcBitReader.  Appends MSB first to
 *  a caller owned buffer, growing it a byte at a time as bits are written, so
 *  the caller never has to size the block in advance - which matters because
 *  the layout's length depends on the values being written.
 ****************************************************************************
 */
class CIccHagcBitWriter
{
public:
  CIccHagcBitWriter(std::vector<icUInt8Number> &buf) : m_buf(buf), m_nBitPos(0) {}

  bool Write(unsigned nBits, icUInt32Number val)
  {
    if (nBits > 32)
      return false;

    for (unsigned i = 0; i < nBits; i++) {
      if (m_nBitPos == 0)
        m_buf.push_back(0);

      icUInt32Number bit = (val >> (nBits - 1 - i)) & 1;
      if (bit)
        m_buf.back() |= (icUInt8Number)(1 << (7 - m_nBitPos));

      m_nBitPos = (m_nBitPos + 1) & 7;
    }
    return true;
  }

  bool WriteFlag(bool val) { return Write(1, val ? 1 : 0); }
  bool WriteU8(icUInt8Number val) { return Write(8, val); }
  bool WriteU16(icUInt16Number val) { return Write(16, val); }

protected:
  std::vector<icUInt8Number> &m_buf;
  unsigned m_nBitPos;
};

/*
 * Decode helpers, one per formula in the proposal.  Each is written to look
 * like the formula it implements - clamp in the integer domain first, then
 * divide - because clamping after the divide would round differently at the
 * limits.  Arithmetic is done in double and narrowed once at the end, so a
 * value near a clamp boundary is not pushed across it by float rounding.
 */

static icUInt32Number icHagcClampU32(icUInt32Number v, icUInt32Number lo, icUInt32Number hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/** Proposal 1.2.2.3: HDR Reference White = MIN(MAX(1, v), 50000)/5.0 */
static icFloatNumber icHagcDecodeReferenceWhite(icUInt16Number v)
{
  return (icFloatNumber)(icHagcClampU32(v, 1, 50000) / 5.0);
}

/** Proposal 1.2.2.4 and 1.1.3.1: Headroom = MIN(v, 60000)/10000.0 */
static icFloatNumber icHagcDecodeHeadroom(icUInt16Number v)
{
  return (icFloatNumber)(icHagcClampU32(v, 0, 60000) / 10000.0);
}

/** Proposal 1.2.2.7 and 1.1.3.2.1 share the same /50000.0 scale. */
static icFloatNumber icHagcDecodeScaled50k(icUInt16Number v)
{
  return (icFloatNumber)(icHagcClampU32(v, 0, 50000) / 50000.0);
}

/** Proposal 0.1.3.5: X = MIN(X, 64000)/1000.0 */
static icFloatNumber icHagcDecodeX(icUInt16Number v)
{
  return (icFloatNumber)(icHagcClampU32(v, 0, 64000) / 1000.0);
}

/** Proposal 0.1.3.6: Y = s * MIN(Y, 60000)/10000.0.  The sign is not carried
 * in the tag; it is implied by whether the alternate is above or below the
 * baseline headroom, so the caller supplies it. */
static icFloatNumber icHagcDecodeY(icUInt16Number v, double s)
{
  return (icFloatNumber)(s * (icHagcClampU32(v, 0, 60000) / 10000.0));
}

/** Proposal 0.1.3.9: M = tan((MIN(MAX(1, theta), 35999) - 18000) * pi/36000).
 *
 * Checked against SMPTE ST 2094-50 clause C.3.7 on 2026-09-01 (PCD2 draft, see
 * icHagcDerivePchipSlopes()'s header for the provenance caveat): this decoder,
 * icHagcDecodeX's 64000/1000 cap and icHagcDecodeY's signed 60000/10000 are
 * each identical to the SMPTE text, as is the reference white's 203 default
 * and its clamp(x, 1, 50000)/5 custom form in C.3.3.  The ICC proposal
 * transcribed them faithfully; nothing here rests on a reconstruction. */
static icFloatNumber icHagcDecodeSlope(icUInt16Number v)
{
  double theta = (double)icHagcClampU32(v, 1, 35999);
  return (icFloatNumber)tan((theta - 18000.0) * icHagcPi / 36000.0);
}

/*
 * Encode helpers.  These are the exact inverses of the decoders above over the
 * representable range: the decoders map an integer onto a grid whose spacing
 * is far coarser than float precision at every point, so round() recovers the
 * original integer.  That is what makes Pack -> Unpack -> Pack byte stable.
 */

static icUInt16Number icHagcEncodeScaled(icFloatNumber v, double scale,
                                         icUInt32Number lo, icUInt32Number hi)
{
  double d = (double)v * scale;

  /* Round half away from zero, then clamp.  Clamping after the round rather
   * than before keeps a value that is merely out of range from also being
   * mis-rounded, and the NaN test has to come first because every comparison
   * against a NaN is false, which would otherwise leave d unclamped and the
   * cast undefined. */
  if (!(d == d))
    return (icUInt16Number)lo;

  d = (d < 0.0) ? -floor(-d + 0.5) : floor(d + 0.5);

  if (d < (double)lo)
    return (icUInt16Number)lo;
  if (d > (double)hi)
    return (icUInt16Number)hi;

  return (icUInt16Number)d;
}

static icUInt16Number icHagcEncodeReferenceWhite(icFloatNumber v)
{
  return icHagcEncodeScaled(v, 5.0, 1, 50000);
}

static icUInt16Number icHagcEncodeHeadroom(icFloatNumber v)
{
  return icHagcEncodeScaled(v, 10000.0, 0, 60000);
}

static icUInt16Number icHagcEncodeScaled50k(icFloatNumber v)
{
  return icHagcEncodeScaled(v, 50000.0, 0, 50000);
}

static icUInt16Number icHagcEncodeX(icFloatNumber v)
{
  return icHagcEncodeScaled(v, 1000.0, 0, 64000);
}

/** The sign of Y is carried by the headroom ordering, not by the tag, so only
 * the magnitude is encoded.  A model whose Y sign disagrees with that ordering
 * loses the sign here; Validate() reports the disagreement rather than letting
 * it pass silently. */
static icUInt16Number icHagcEncodeY(icFloatNumber v)
{
  return icHagcEncodeScaled((icFloatNumber)fabs((double)v), 10000.0, 0, 60000);
}

static icUInt16Number icHagcEncodeSlope(icFloatNumber v)
{
  double theta = atan((double)v) * 36000.0 / icHagcPi + 18000.0;
  return icHagcEncodeScaled((icFloatNumber)theta, 1.0, 1, 35999);
}

/** The sign applied to every control point Y of an alternate image
 * (proposal 0.1.3.6): +1 when the alternate is brighter than the baseline,
 * -1 otherwise, so tone mapping down yields negative gains in log2 space. */
static double icHagcYSign(icFloatNumber baselineHeadroom, icFloatNumber altHeadroom)
{
  return (baselineHeadroom < altHeadroom) ? 1.0 : -1.0;
}

/**
 ****************************************************************************
 * Name: icHagcAlternateImage::SetMixingType
 *
 * Purpose: Set the component mixing type and, with it, the coefficients that
 *  mixing types 0 to 2 define implicitly (informative annex 1).  Doing this
 *  in one place means every consumer of the model can read m_coef[]
 *  regardless of mixing type instead of re-deriving the fixed cases, which is
 *  exactly the kind of duplicated table that drifts.
 *
 *  The array is zeroed first, so this is also the only safe way to change the
 *  mixing type of an alternate that already carries coefficients: an
 *  open-coded switch that only writes the arms its type needs leaves whatever
 *  the previous type had set behind, and because Pack() derives the presence
 *  flags from the values, such a leftover is written out as a real
 *  coefficient.
 *
 * Args:
 *  nType = the component mixing type
 *****************************************************************************
 */
void icHagcAlternateImage::SetMixingType(icHagcMixingType nType)
{
  int i;

  m_nMixingType = nType;

  for (i = 0; i < icHagcNumCoefficients; i++)
    m_coef[i] = 0.0f;

  switch (nType) {
    case icHagcMixingMax:
      m_coef[icHagcCoefMax] = 1.0f;
      break;

    case icHagcMixingComponent:
      m_coef[icHagcCoefComponent] = 1.0f;
      break;

    case icHagcMixingWeighted:
      m_coef[icHagcCoefRed] = m_coef[icHagcCoefGreen] = m_coef[icHagcCoefBlue] = (icFloatNumber)(1.0 / 6.0);
      m_coef[icHagcCoefMax] = 0.5f;
      break;

    case icHagcMixingCustom:
      /* Carried in the tag; the caller fills these in from the coefficient
       * array.  Leaving them zeroed here is deliberate, so a type 3 alternate
       * whose coefficients failed to decode reads as all zero rather than as
       * whichever fixed set was written last. */
      break;
  }
}

/**
 ****************************************************************************
 * Name: icHagcAlternateImage::icHagcAlternateImage
 *
 * Purpose: Constructor
 *****************************************************************************
 */
icHagcAlternateImage::icHagcAlternateImage()
{
  int i;

  m_headroom = 0.0f;
  SetMixingType(icHagcMixingMax);
  m_nControlPoints = 0;
  m_bPchipSlope = false;
  m_nCurveReserved = 0;

  for (i = 0; i < icHagcMaxControlPoints; i++) {
    m_x[i] = 0.0f;
    m_y[i] = 0.0f;
    m_slope[i] = 0.0f;
  }
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::icHagcMetadata
 *
 * Purpose: Constructor
 *****************************************************************************
 */
icHagcMetadata::icHagcMetadata()
{
  Reset();
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::Reset
 *
 * Purpose: Return the model to its default constructed state.  Unpack() calls
 *  this on entry and on failure, so a block that fails to decode never leaves
 *  a half populated model behind for Describe() or Validate() to read.
 *****************************************************************************
 */
void icHagcMetadata::Reset()
{
  int i;

  m_nApplicationVersion = 0;
  m_nMinApplicationVersion = 0;
  m_nGeneralInfoReserved = 0;

  m_bCustomReferenceWhite = false;
  m_referenceWhite = (icFloatNumber)icHagcDefaultReferenceWhite;
  m_bHeadroomAdaptiveToneMap = false;
  m_nGlobalFlagsReserved = 0;
  m_baselineHeadroom = 0.0f;
  m_bReferenceWhiteToneMapping = false;
  m_nChromaticitiesMode = icHagcChromaticitiesBT709;
  m_bCommonComponentMixing = false;
  m_bCommonCurveParameters = false;

  for (i = 0; i < 8; i++)
    m_chromaticities[i] = 0.0f;

  m_bUnpacked = false;
  m_nTrailingBytes = 0;
  m_bRefWhiteToneMapFieldsNonZero = false;

  m_nAlternates = 0;
  for (i = 0; i < icHagcMaxAlternates; i++)
    m_alternates[i] = icHagcAlternateImage();
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::GetReferenceWhite
 *
 * Purpose: Resolve the HDR reference white, applying the 203.0 default when
 *  the tag carries no custom value (proposal 1.2.2.3).
 *****************************************************************************
 */
icFloatNumber icHagcMetadata::GetReferenceWhite() const
{
  return m_bCustomReferenceWhite ? m_referenceWhite : (icFloatNumber)icHagcDefaultReferenceWhite;
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::SetNumAlternates
 *
 * Purpose: Set the alternate image count, clearing any entry the change newly
 *  exposes so a grown array never shows stale values from an earlier model.
 *****************************************************************************
 */
bool icHagcMetadata::SetNumAlternates(icUInt8Number n)
{
  if (n > icHagcMaxAlternates)
    return false;

  icUInt8Number i;
  for (i = m_nAlternates; i < n; i++)
    m_alternates[i] = icHagcAlternateImage();

  m_nAlternates = n;
  return true;
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::Unpack
 *
 * Purpose: Decode a SMPTE ST 2094-50:2026 metadata block.
 *
 * Args:
 *  pData = the metadata block, i.e. what follows the 12 byte tag header
 *  nSize = its declared length in bytes
 *
 * Return:
 *  true = decoded, false = malformed or truncated
 *****************************************************************************
 */
bool icHagcMetadata::Unpack(const icUInt8Number *pData, icUInt32Number nSize)
{
  Reset();

  if (!UnpackFields(pData, nSize)) {
    /* Reset a second time so a partial decode never survives.  UnpackFields()
     * sets m_nAlternates from the count field before it reads a single
     * alternate image record, so a block truncated inside that loop would
     * otherwise leave a model advertising alternates that were never read. */
    Reset();
    return false;
  }

  m_bUnpacked = true;
  return true;
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::UnpackFields
 *
 * Purpose: Decode the fields of a metadata block.  See Unpack(), which owns
 *  the reset-on-failure contract this function relies on.
 *****************************************************************************
 */
bool icHagcMetadata::UnpackFields(const icUInt8Number *pData, icUInt32Number nSize)
{
  if (!pData || !nSize)
    return false;

  CIccHagcBitReader r(pData, nSize);
  icUInt32Number v;
  icUInt16Number u;
  int i, j;

  /* Table 1, byte 12: General Information */
  if (!r.Read(3, v))
    return false;
  m_nApplicationVersion = (icUInt8Number)v;

  if (!r.Read(3, v))
    return false;
  m_nMinApplicationVersion = (icUInt8Number)v;

  if (!r.Read(2, v))
    return false;
  m_nGeneralInfoReserved = (icUInt8Number)v;

  /* Table 2, byte 0: the two global flags plus six reserved bits */
  if (!r.ReadFlag(m_bCustomReferenceWhite) ||
      !r.ReadFlag(m_bHeadroomAdaptiveToneMap) ||
      !r.Read(6, v))
    return false;
  m_nGlobalFlagsReserved = (icUInt8Number)v;

  /* Table 2, bytes 1 to 2, present only when the flag says so.  This is the
   * "k" in the table's offsets: every field after it shifts by two bytes when
   * a custom reference white is carried. */
  if (m_bCustomReferenceWhite) {
    if (!r.ReadU16(u))
      return false;
    m_referenceWhite = icHagcDecodeReferenceWhite(u);
  }

  /* Table 2, bytes k+1 to k+2 */
  if (!r.ReadU16(u))
    return false;
  m_baselineHeadroom = icHagcDecodeHeadroom(u);

  /* Table 2, byte k+3 */
  icUInt32Number nCount, nMode;
  bool bCommonMixing, bCommonCurve;
  if (!r.ReadFlag(m_bReferenceWhiteToneMapping) ||
      !r.Read(3, nCount) ||
      !r.Read(2, nMode) ||
      !r.ReadFlag(bCommonMixing) ||
      !r.ReadFlag(bCommonCurve))
    return false;

  if (m_bReferenceWhiteToneMapping) {
    /* Proposal 1.2.2.5: the fields above are zero on the wire, their effective
     * values are derived per clause C.3.8 of SMPTE ST 2094-50:2026, and no
     * further records follow.  We do not have that clause, so the model stays
     * empty and Validate() reports the configuration as unsupported instead of
     * inventing an alternate image the tag never described. */
    m_bRefWhiteToneMapFieldsNonZero = (nCount != 0 || nMode != 0 || bCommonMixing || bCommonCurve);
    m_nTrailingBytes = nSize - r.BytesConsumed();
    return true;
  }

  /* The count occupies three bits, so the encoding can express 5 to 7 while
   * the proposal caps alternate images at 4.  Refusing here rather than
   * clamping is what keeps the rest of the parse honest: a count of 6 means
   * the byte stream contains six alternate image sections, so decoding only
   * the first four would leave the cursor mid record and mis-attribute every
   * field after it. */
  if (nCount > icHagcMaxAlternates)
    return false;

  m_nAlternates = (icUInt8Number)nCount;
  m_nChromaticitiesMode = (icHagcChromaticitiesMode)nMode;
  m_bCommonComponentMixing = bCommonMixing;
  m_bCommonCurveParameters = bCommonCurve;

  /* Table 2, bytes k+4 to k+19: always present, whatever the mode.  Table 2
   * marks its one conditional field explicitly - the Custom HDR Reference White
   * is "1 to 2 (if k != 0)", and k carries the shift into every later offset -
   * and it puts no such condition on this row.  So the global parameter block
   * has a fixed layout, and 0.1.2.7 governs only what the sixteen bytes MEAN:
   * in mode 3 they are the custom chromaticities, and in modes 0 to 2 the
   * primaries come from the named ITU-T H.273 entry instead and the bytes carry
   * nothing.  Reading them unconditionally is what keeps this decoder in step
   * with the record that follows. */
  for (i = 0; i < 8; i++) {
    if (!r.ReadU16(u))
      return false;
    m_chromaticities[i] = icHagcDecodeScaled50k(u);
  }

  /* Outside mode 3 the sixteen bytes have no defined content, so nothing is
   * carried forward from them: the chromaticities of modes 0 to 2 come from
   * H.273 and a decoded value here would only invite a caller to use it. */
  if (m_nChromaticitiesMode != icHagcChromaticitiesCustom)
    memset(m_chromaticities, 0, sizeof(m_chromaticities));

  /* Table 3, once per alternate image */
  for (i = 0; i < (int)m_nAlternates; i++) {
    icHagcAlternateImage &alt = m_alternates[i];

    if (!r.ReadU16(u))
      return false;
    alt.m_headroom = icHagcDecodeHeadroom(u);

    /* Proposal 1.1.2.8: with Common Component Mixing set, only alternate 0
     * carries the mixing byte and its coefficients; the rest inherit them. */
    if (i == 0 || !m_bCommonComponentMixing) {
      icUInt32Number nMixType;
      bool bFlag[icHagcNumCoefficients];

      if (!r.Read(2, nMixType))
        return false;
      alt.SetMixingType((icHagcMixingType)nMixType);

      for (j = 0; j < icHagcNumCoefficients; j++) {
        if (!r.ReadFlag(bFlag[j]))
          return false;
      }

      /* Proposal 1.1.3.2.1: only type 3 serializes coefficients, and only the
       * ones whose flag is set, in icHagcCoefficient order.  The array length
       * is therefore the number of set flags - it is not stored, so a wrong
       * flag byte silently reframes everything that follows. */
      if (alt.m_nMixingType == icHagcMixingCustom) {
        for (j = 0; j < icHagcNumCoefficients; j++) {
          if (!bFlag[j])
            continue;
          if (!r.ReadU16(u))
            return false;
          alt.m_coef[j] = icHagcDecodeScaled50k(u);
        }
      }
    }
    else {
      alt.m_nMixingType = m_alternates[0].m_nMixingType;
      for (j = 0; j < icHagcNumCoefficients; j++)
        alt.m_coef[j] = m_alternates[0].m_coef[j];
    }

    /* Proposal 1.1.2.9: with Common Curve Parameters set, only alternate 0
     * carries the control point count, the PCHIP flag and the X array. */
    if (i == 0 || !m_bCommonCurveParameters) {
      icUInt32Number nLastIndex;

      if (!r.Read(5, nLastIndex) ||
          !r.ReadFlag(alt.m_bPchipSlope) ||
          !r.Read(2, v))
        return false;
      alt.m_nCurveReserved = (icUInt8Number)v;

      /* Five bits hold 0..31, so the count is 1..32 by construction and the
       * proposal's "last index shall be <= 31" cannot be violated here. */
      alt.m_nControlPoints = (icUInt8Number)(nLastIndex + 1);

      for (j = 0; j < (int)alt.m_nControlPoints; j++) {
        if (!r.ReadU16(u))
          return false;
        alt.m_x[j] = icHagcDecodeX(u);
      }
    }
    else {
      alt.m_nControlPoints = m_alternates[0].m_nControlPoints;
      alt.m_bPchipSlope = m_alternates[0].m_bPchipSlope;
      alt.m_nCurveReserved = m_alternates[0].m_nCurveReserved;
      for (j = 0; j < (int)alt.m_nControlPoints; j++)
        alt.m_x[j] = m_alternates[0].m_x[j];
    }

    /* Y coordinates and slope angles stay per alternate even when the curve
     * parameters are shared - the shared set is only the count, the PCHIP flag
     * and the X positions, which is what makes a shared X grid useful. */
    double s = icHagcYSign(m_baselineHeadroom, alt.m_headroom);
    for (j = 0; j < (int)alt.m_nControlPoints; j++) {
      if (!r.ReadU16(u))
        return false;
      alt.m_y[j] = icHagcDecodeY(u, s);
    }

    if (!alt.m_bPchipSlope) {
      for (j = 0; j < (int)alt.m_nControlPoints; j++) {
        if (!r.ReadU16(u))
          return false;
        alt.m_slope[j] = icHagcDecodeSlope(u);
      }
    }
  }

  m_nTrailingBytes = nSize - r.BytesConsumed();
  return true;
}

/**
 ****************************************************************************
 * Name: icHagcMetadata::Pack
 *
 * Purpose: Encode this model as a SMPTE ST 2094-50:2026 metadata block.
 *
 *  This mirrors Unpack() field for field.  Where the encoding shares a field
 *  between alternates, Pack() writes alternate 0's value and uses that same
 *  value for the dependent lengths of every later alternate, so an authored
 *  model whose alternates disagree about a shared field still produces a block
 *  that decodes back to exactly what was written.  Validate() reports the
 *  disagreement; Pack() does not get to lose bytes over it.
 *
 * Args:
 *  buf = buffer the encoded block is appended to
 *
 * Return:
 *  true = encoded, false = the model cannot be represented
 *****************************************************************************
 */
bool icHagcMetadata::Pack(std::vector<icUInt8Number> &buf) const
{
  CIccHagcBitWriter w(buf);
  int i, j;

  /* Refused rather than masked.  Masking is how a model that says version 8
   * becomes a file that says version 0 with nothing reporting the change, and
   * SetMetadata() now keeps an Unpack() of these bytes, so the model would
   * quietly follow the file rather than the caller.  Both authoring parsers
   * reject the value before it gets here; this is the backstop for the
   * direct-API path. */
  if (m_nApplicationVersion > icHagcMaxApplicationVersion ||
      m_nMinApplicationVersion > icHagcMaxApplicationVersion)
    return false;

  w.Write(3, m_nApplicationVersion);
  w.Write(3, m_nMinApplicationVersion);
  w.Write(2, 0);   /* reserved, shall be zero */

  w.WriteFlag(m_bCustomReferenceWhite);
  w.WriteFlag(m_bHeadroomAdaptiveToneMap);
  w.Write(6, 0);   /* reserved, shall be zero */

  if (m_bCustomReferenceWhite)
    w.WriteU16(icHagcEncodeReferenceWhite(m_referenceWhite));

  w.WriteU16(icHagcEncodeHeadroom(m_baselineHeadroom));

  if (m_bReferenceWhiteToneMapping) {
    /* Proposal 1.2.2.5: the remaining bit fields are zero and nothing follows. */
    w.WriteFlag(true);
    w.Write(3, 0);
    w.Write(2, 0);
    w.WriteFlag(false);
    w.WriteFlag(false);
    return true;
  }

  if (m_nAlternates > icHagcMaxAlternates)
    return false;

  w.WriteFlag(false);
  w.Write(3, m_nAlternates);
  w.Write(2, (icUInt32Number)m_nChromaticitiesMode & 0x03);
  w.WriteFlag(m_bCommonComponentMixing);
  w.WriteFlag(m_bCommonCurveParameters);

  /* Always sixteen bytes - see the note on the matching read.  Outside mode 3
   * the field has no defined content, and zero is the only value that cannot be
   * mistaken for a chromaticity by a reader that ignores the mode. */
  for (i = 0; i < 8; i++) {
    w.WriteU16(m_nChromaticitiesMode == icHagcChromaticitiesCustom
                 ? icHagcEncodeScaled50k(m_chromaticities[i])
                 : (icUInt16Number)0);
  }

  for (i = 0; i < (int)m_nAlternates; i++) {
    const icHagcAlternateImage &alt = m_alternates[i];

    /* The alternate whose shared fields govern this one's record lengths. */
    const icHagcAlternateImage &mix = (i && m_bCommonComponentMixing) ? m_alternates[0] : alt;
    const icHagcAlternateImage &crv = (i && m_bCommonCurveParameters) ? m_alternates[0] : alt;

    if (crv.m_nControlPoints < 1 || crv.m_nControlPoints > icHagcMaxControlPoints)
      return false;

    w.WriteU16(icHagcEncodeHeadroom(alt.m_headroom));

    if (i == 0 || !m_bCommonComponentMixing) {
      w.Write(2, (icUInt32Number)mix.m_nMixingType & 0x03);

      /* Proposal 1.1.3.2.1 ties the flag to the value: a coefficient is
       * present in the array exactly when its flag is set, and a zero
       * coefficient is meant to be omitted.  Deriving the flags from the
       * values here is what keeps the two from ever disagreeing on output. */
      for (j = 0; j < icHagcNumCoefficients; j++)
        w.WriteFlag(mix.m_nMixingType == icHagcMixingCustom && mix.m_coef[j] != 0.0f);

      if (mix.m_nMixingType == icHagcMixingCustom) {
        for (j = 0; j < icHagcNumCoefficients; j++) {
          if (mix.m_coef[j] != 0.0f)
            w.WriteU16(icHagcEncodeScaled50k(mix.m_coef[j]));
        }
      }
    }

    if (i == 0 || !m_bCommonCurveParameters) {
      w.Write(5, (icUInt32Number)(crv.m_nControlPoints - 1));
      w.WriteFlag(crv.m_bPchipSlope);
      w.Write(2, 0);   /* reserved, shall be zero */

      for (j = 0; j < (int)crv.m_nControlPoints; j++)
        w.WriteU16(icHagcEncodeX(crv.m_x[j]));
    }

    for (j = 0; j < (int)crv.m_nControlPoints; j++)
      w.WriteU16(icHagcEncodeY(alt.m_y[j]));

    if (!crv.m_bPchipSlope) {
      for (j = 0; j < (int)crv.m_nControlPoints; j++)
        w.WriteU16(icHagcEncodeSlope(alt.m_slope[j]));
    }
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::CIccTagHagc
 *
 * Purpose: Constructor
 *****************************************************************************
 */
CIccTagHagc::CIccTagHagc()
{
  m_pRawData = NULL;
  m_nRawSize = 0;
  m_nDeclaredSize = 0;
  m_nPadSize = 0;
  m_bNonZeroPad = false;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::CIccTagHagc
 *
 * Purpose: Copy Constructor
 *
 * Args:
 *  ITHagc = the CIccTagHagc object to be copied
 *****************************************************************************
 */
CIccTagHagc::CIccTagHagc(const CIccTagHagc &ITHagc)
{
  m_pRawData = NULL;
  m_nRawSize = 0;
  m_nDeclaredSize = 0;
  m_nPadSize = 0;
  m_bNonZeroPad = false;

  *this = ITHagc;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::operator=
 *
 * Purpose: Copy Operator
 *
 * Args:
 *  HagcTag = the CIccTagHagc object to be copied
 *****************************************************************************
 */
CIccTagHagc &CIccTagHagc::operator=(const CIccTagHagc &HagcTag)
{
  if (&HagcTag == this)
    return *this;

  Cleanup();

  m_metadata = HagcTag.m_metadata;
  m_nDeclaredSize = HagcTag.m_nDeclaredSize;
  m_nPadSize = HagcTag.m_nPadSize;
  m_bNonZeroPad = HagcTag.m_bNonZeroPad;
  m_nReserved = HagcTag.m_nReserved;

  /* The raw block is copied rather than shared: NewCopy() exists so a profile
   * can hand out a tag the caller owns outright, and a shared buffer would
   * make the copy's lifetime depend on the original's. */
  if (HagcTag.m_pRawData && HagcTag.m_nRawSize) {
    m_pRawData = new (std::nothrow) icUInt8Number[HagcTag.m_nRawSize];
    if (m_pRawData) {
      memcpy(m_pRawData, HagcTag.m_pRawData, HagcTag.m_nRawSize);
      m_nRawSize = HagcTag.m_nRawSize;
    }
    else {
      /* Nothing here can report the failure - operator= has no return - so
       * the choice is which wrong answer to give.  An empty tag is the one
       * that fails visibly: keeping the decoded model beside no bytes would
       * leave a copy that Describe()s as a full gain curve and Write()s as a
       * zero length tag, i.e. a profile that silently lost its tone mapping.
       * Cleaned up, the copy is an empty tag, which Validate() calls out. */
      Cleanup();
    }
  }

  return *this;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::~CIccTagHagc
 *
 * Purpose: Destructor
 *****************************************************************************
 */
CIccTagHagc::~CIccTagHagc()
{
  Cleanup();
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::Cleanup
 *
 * Purpose: Release the raw block and return the tag to its empty state.
 *****************************************************************************
 */
void CIccTagHagc::Cleanup()
{
  delete [] m_pRawData;
  m_pRawData = NULL;
  m_nRawSize = 0;
  m_nDeclaredSize = 0;
  m_nPadSize = 0;
  m_bNonZeroPad = false;
  m_metadata.Reset();
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::SetRawMetadata
 *
 * Purpose: Install a metadata block verbatim and decode it.
 *
 * Args:
 *  pData = the metadata block
 *  nSize = its length in bytes
 *
 * Return:
 *  true = stored (whether or not it decoded), false = allocation failed
 *****************************************************************************
 */
bool CIccTagHagc::SetRawMetadata(const icUInt8Number *pData, icUInt32Number nSize)
{
  Cleanup();

  if (!pData || !nSize)
    return true;

  if (nSize > icHagcMaxMetadataSize)
    return false;

  m_pRawData = new (std::nothrow) icUInt8Number[nSize];
  if (!m_pRawData)
    return false;

  memcpy(m_pRawData, pData, nSize);
  m_nRawSize = nSize;
  m_nDeclaredSize = nSize;

  /* A block that will not decode is still a block: the tag keeps it and
   * Validate() reports it, which is what lets an ICC file survive a SMPTE
   * revision this decoder predates. */
  m_metadata.Unpack(m_pRawData, m_nRawSize);

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::SetMetadata
 *
 * Purpose: Replace the model and re-derive the raw block from it.
 *
 *  The model kept is deliberately *not* the caller's copy but an Unpack() of
 *  the bytes Pack() just produced.  Pack() only refuses a control point count
 *  outside 1..icHagcMaxControlPoints or too many alternates; everything else
 *  it either clamps (icHagcEncodeScaled clamps each field to its encodable
 *  range) or re-derives (the coefficient presence flags come from the
 *  values).  Storing the caller's copy beside those different bytes leaves a
 *  tag whose model and wire form disagree - and the disagreement is not
 *  cosmetic, because it is the decoder's clamps that establish the domain
 *  invariants the evaluator relies on.  A negative control point X, say, is
 *  unencodable, so no file can carry one, and CIccHagcEvaluator's
 *  log(x[last]/v) is written on that basis: hand it one straight from an
 *  authored model and it returns NaN for every pixel with nothing reporting
 *  a problem.
 *
 *  Round tripping here makes the authored path structurally identical to the
 *  read path, so those clamps are the single place the invariants are
 *  established.  The cost is one extra decode per authored tag.
 *
 * Args:
 *  metadata = the model to install
 *
 * Return:
 *  true = installed, false = the model could not be packed or its own bytes
 *  would not decode (tag unchanged in either case)
 *****************************************************************************
 */
bool CIccTagHagc::SetMetadata(const icHagcMetadata &metadata)
{
  std::vector<icUInt8Number> buf;

  if (!metadata.Pack(buf) || buf.empty())
    return false;

  if (buf.size() > icHagcMaxMetadataSize)
    return false;

  icUInt32Number nSize = (icUInt32Number)buf.size();

  /* Decoded into a temporary first so that a model Pack() accepted but the
   * decoder will not take leaves the tag exactly as it was, rather than
   * emptied.  Failure here means Pack() and Unpack() disagree about the wire
   * format, which is a defect in this file and not something the caller can
   * have caused. */
  icHagcMetadata decoded;
  if (!decoded.Unpack(&buf[0], nSize))
    return false;

  icUInt8Number *pNew = new (std::nothrow) icUInt8Number[nSize];
  if (!pNew)
    return false;

  memcpy(pNew, &buf[0], nSize);

  Cleanup();
  m_pRawData = pNew;
  m_nRawSize = nSize;
  m_nDeclaredSize = nSize;
  m_metadata = decoded;

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::Read
 *
 * Purpose: Read in the tag contents into a data block
 *
 * Args:
 *  size - # of bytes in tag,
 *  pIO - IO object to read tag from
 *
 * Return:
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagHagc::Read(icUInt32Number size, CIccIO *pIO)
{
  Cleanup();

  icTagTypeSignature sig;

  if (size < icHagcHeaderSize || !pIO)
    return false;

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nDeclaredSize))
    return false;

  icUInt32Number nAvail = size - icHagcHeaderSize;

  if (m_nDeclaredSize > icHagcMaxMetadataSize)
    return false;

  /* A declared size larger than the tag body is a defect, not a reason to
   * discard the tag: the bytes that are present still decode, and reporting
   * the shortfall from Validate() is more useful than a bare read failure
   * that leaves the caller unable to say which tag was wrong. */
  icUInt32Number nRead = m_nDeclaredSize < nAvail ? m_nDeclaredSize : nAvail;

  if (nRead) {
    m_pRawData = new (std::nothrow) icUInt8Number[nRead];
    if (!m_pRawData)
      return false;

    if (pIO->Read8(m_pRawData, nRead) != (size_t)nRead) {
      Cleanup();
      return false;
    }
    m_nRawSize = nRead;
  }

  /* Whatever follows the metadata is pad.  Annex 1 note 2 requires the tag's
   * overall length to be a multiple of four while the SMPTE block itself need
   * not be, so 0 to 3 null bytes belong here and anything else is a defect. */
  m_nPadSize = nAvail - nRead;
  m_bNonZeroPad = false;

  if (m_nPadSize) {
    icUInt32Number i;
    for (i = 0; i < m_nPadSize; i++) {
      icUInt8Number pad;
      if (!pIO->Read8(&pad))
        break;
      if (pad)
        m_bNonZeroPad = true;
    }
  }

  /* Deliberately not propagated: an undecodable block is still a tag.  See
   * SetRawMetadata(). */
  m_metadata.Unpack(m_pRawData, m_nRawSize);

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::Write
 *
 * Purpose: Write the tag to a file
 *
 * Args:
 *  pIO - The IO object to write tag to.
 *
 * Return:
 *  true = successful, false = failure
 *****************************************************************************
 */
bool CIccTagHagc::Write(CIccIO *pIO)
{
  icTagTypeSignature sig = GetType();

  if (!pIO)
    return false;

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  icUInt32Number nSize = m_nRawSize;
  if (!pIO->Write32(&nSize))
    return false;

  if (m_nRawSize && pIO->Write8(m_pRawData, m_nRawSize) != (size_t)m_nRawSize)
    return false;

  /* The pad is emitted here rather than left to CIccProfile, which pads with
   * Align32() *after* recording the tag size.  That records the unpadded
   * length in the tag directory, and annex 1 note 2 asks for the tag's overall
   * length - the number the directory carries - to be a multiple of four.
   * Writing the pad as part of the tag is what makes the recorded size satisfy
   * that; the profile's own Align32() then finds nothing left to do. */
  icUInt32Number nPad = (4 - (m_nRawSize & 3)) & 3;
  if (nPad) {
    icUInt8Number zero[4] = { 0, 0, 0, 0 };
    if (pIO->Write8(zero, nPad) != (size_t)nPad)
      return false;
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::Describe
 *
 * Purpose: Dump data associated with the tag to a string
 *
 * Args:
 *  sDescription - string to concatenate tag dump to
 *  nVerboseness - controls how much of the control point data is listed
 *****************************************************************************
 */
void CIccTagHagc::Describe(std::string &sDescription, int nVerboseness)
{
  const size_t bufSize = 256;
  icChar buf[bufSize];
  int i, j;

  snprintf(buf, bufSize, "SMPTE ST 2094-50:2026 metadata size: %u bytes\r\n", m_nRawSize);
  sDescription += buf;

  if (!m_metadata.m_bUnpacked) {
    sDescription += "Metadata could not be decoded; the tag is retained verbatim.\r\n";
    return;
  }

  snprintf(buf, bufSize, "Application version: %u (minimum %u)\r\n",
           m_metadata.m_nApplicationVersion, m_metadata.m_nMinApplicationVersion);
  sDescription += buf;

  snprintf(buf, bufSize, "HDR reference white: %.4f cd/m^2%s\r\n",
           m_metadata.GetReferenceWhite(),
           m_metadata.m_bCustomReferenceWhite ? "" : " (default)");
  sDescription += buf;

  snprintf(buf, bufSize, "Baseline HDR headroom: %.4f stops\r\n", m_metadata.m_baselineHeadroom);
  sDescription += buf;

  snprintf(buf, bufSize, "Headroom adaptive tone map: %s\r\n",
           m_metadata.m_bHeadroomAdaptiveToneMap ? "yes" : "no");
  sDescription += buf;

  if (m_metadata.m_bReferenceWhiteToneMapping) {
    sDescription += "Reference white tone mapping: yes - parameters are derived per\r\n"
                    "  clause C.3.8 of SMPTE ST 2094-50:2026 and are not carried in the tag.\r\n";
    return;
  }

  static const char *szChromaticities[] = {
    "ITU-R BT.709-6 (H.273 value 1)",
    "SMPTE EG 432-1-2010 / Display P3 (H.273 value 12)",
    "ITU-R BT.2020-2 (H.273 value 9)",
    "custom"
  };
  snprintf(buf, bufSize, "Gain curve chromaticities: %s\r\n",
           szChromaticities[m_metadata.m_nChromaticitiesMode & 0x03]);
  sDescription += buf;

  if (m_metadata.m_nChromaticitiesMode == icHagcChromaticitiesCustom) {
    snprintf(buf, bufSize, "  R=%.5f,%.5f G=%.5f,%.5f B=%.5f,%.5f W=%.5f,%.5f\r\n",
             m_metadata.m_chromaticities[0], m_metadata.m_chromaticities[1],
             m_metadata.m_chromaticities[2], m_metadata.m_chromaticities[3],
             m_metadata.m_chromaticities[4], m_metadata.m_chromaticities[5],
             m_metadata.m_chromaticities[6], m_metadata.m_chromaticities[7]);
    sDescription += buf;
  }

  snprintf(buf, bufSize, "Common component mixing: %s, common curve parameters: %s\r\n",
           m_metadata.m_bCommonComponentMixing ? "yes" : "no",
           m_metadata.m_bCommonCurveParameters ? "yes" : "no");
  sDescription += buf;

  snprintf(buf, bufSize, "Alternate images: %u\r\n", m_metadata.GetNumAlternates());
  sDescription += buf;

  if (!m_metadata.GetNumAlternates()) {
    sDescription += "  No tone mapping is to be performed; the baseline image should be\r\n"
                    "  clamped to the target colour volume.\r\n";
  }

  static const char *szMixing[] = { "max", "component", "weighted", "custom" };
  static const char *szCoef[] = { "kRed", "kGreen", "kBlue", "kMax", "kMin", "kComponent" };

  for (i = 0; i < (int)m_metadata.GetNumAlternates(); i++) {
    const icHagcAlternateImage *pAlt = m_metadata.GetAlternate((icUInt8Number)i);
    if (!pAlt)
      continue;

    snprintf(buf, bufSize, "\r\nAlternate image %d:\r\n", i);
    sDescription += buf;

    snprintf(buf, bufSize, "  Headroom: %.4f stops\r\n", pAlt->m_headroom);
    sDescription += buf;

    snprintf(buf, bufSize, "  Component mixing type: %u (%s)\r\n",
             (unsigned)pAlt->m_nMixingType, szMixing[pAlt->m_nMixingType & 0x03]);
    sDescription += buf;

    sDescription += "  Coefficients:";
    for (j = 0; j < icHagcNumCoefficients; j++) {
      snprintf(buf, bufSize, " %s=%.5f", szCoef[j], pAlt->m_coef[j]);
      sDescription += buf;
    }
    sDescription += "\r\n";

    snprintf(buf, bufSize, "  Control points: %u, slopes %s\r\n",
             pAlt->m_nControlPoints,
             pAlt->m_bPchipSlope ? "derived (PCHIP)" : "carried in the tag");
    sDescription += buf;

    /* The control point table is the bulk of the dump and is only useful when
     * someone is checking the curve itself, so it follows the same verboseness
     * threshold the LUT tags use for their sample data. */
    if (nVerboseness > 75) {
      for (j = 0; j < (int)pAlt->m_nControlPoints; j++) {
        if (pAlt->m_bPchipSlope)
          snprintf(buf, bufSize, "    [%2d] x=%9.4f  y=%9.5f\r\n", j, pAlt->m_x[j], pAlt->m_y[j]);
        else
          snprintf(buf, bufSize, "    [%2d] x=%9.4f  y=%9.5f  m=%12.5f\r\n",
                   j, pAlt->m_x[j], pAlt->m_y[j], pAlt->m_slope[j]);
        sDescription += buf;
      }
    }
  }
}

/**
 ****************************************************************************
 * Name: CIccTagHagc::Validate
 *
 * Purpose: Check tag data validity.
 *
 * Args:
 *  sigPath = signature path of tag being validated,
 *  sReport = String to add report information to
 *  pProfile = profile containing tag
 *
 * Return:
 *  icValidateStatusOK if valid, or other error status.
 *****************************************************************************
 */
icValidateStatus CIccTagHagc::Validate(std::string sigPath, std::string &sReport,
                                       const CIccProfile *pProfile /*=NULL*/) const
{
  icValidateStatus rv = CIccTag::Validate(sigPath, sReport, pProfile);

  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);
  const size_t bufSize = 256;
  icChar buf[bufSize];
  int i, j;

  /* --- Tag envelope (proposal Table 1 and annex 1 note 2) --- */

  if (m_nDeclaredSize != m_nRawSize) {
    snprintf(buf, bufSize,
             " - HAGC declared metadata size %u exceeds the %u bytes present in the tag.\r\n",
             m_nDeclaredSize, m_nRawSize);
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += buf;
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  /* The pad exists only to round the tag up to a multiple of four, so it can
   * never legitimately be four bytes or more; that would mean the tag reserved
   * a whole extra word it does not account for. */
  if (m_nPadSize >= 4) {
    snprintf(buf, bufSize, " - HAGC has %u unaccounted bytes after the metadata.\r\n", m_nPadSize);
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += buf;
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_bNonZeroPad) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC pad bytes are not null.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (!m_metadata.m_bUnpacked) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC SMPTE ST 2094-50:2026 metadata could not be decoded.\r\n";
    return icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_metadata.m_nTrailingBytes) {
    snprintf(buf, bufSize,
             " - HAGC metadata has %u trailing bytes the layout does not account for.\r\n",
             m_metadata.m_nTrailingBytes);
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += buf;
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  /* --- Reserved fields (proposal Tables 1, 2 and 3) --- */

  if (m_metadata.m_nGeneralInfoReserved || m_metadata.m_nGlobalFlagsReserved) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC reserved bits are not zero.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_metadata.m_bRefWhiteToneMapFieldsNonZero) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC Reference White Tone Mapping is set but the bit fields sharing its byte are not zero.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_metadata.m_bReferenceWhiteToneMapping) {
    /* Not a defect in the tag, and no longer a limitation of this
     * implementation either.
     *
     * PROPOSAL-ISSUE HAGC-05 (external dependency) -- with this flag set, four
     * header fields are deliberately zeroed in the file and the alternate
     * images are absent altogether; their effective values come from clause
     * C.3.8 of SMPTE ST 2094-50, so the tag is not merely unrenderable but
     * unparseable to a reader holding only ICC documents.  That is what the
     * item records, and it still holds against the amendment.
     *
     * icHagcDeriveReferenceWhiteToneMap() now performs the derivation, so the
     * curve CAN be evaluated - but from the second public committee draft of
     * ST 2094-50, which is the latest public text of it.  Said as information
     * rather
     * than as a warning: nothing is wrong with the profile, and a reader who
     * needs to know which text the numbers came from is told where to look. */
    sReport += icMsgValidateInformation;
    sReport += sSigPathName;
    sReport += " - HAGC Reference White Tone Mapping parameters are not carried in the tag; they\r\n"
               "    are derived per clause C.3.8 of SMPTE ST 2094-50, read from its 2026-02-23\r\n"
               "    public committee draft.\r\n";
  }

  /* --- Global tone mapping parameters (proposal Table 2) --- */

  if (m_metadata.GetNumAlternates() > icHagcMaxAlternates) {
    snprintf(buf, bufSize, " - HAGC declares %u alternate images; the maximum is %d.\r\n",
             m_metadata.GetNumAlternates(), icHagcMaxAlternates);
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += buf;
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (m_metadata.m_nChromaticitiesMode == icHagcChromaticitiesCustom) {
    for (i = 0; i < 8; i++) {
      if (m_metadata.m_chromaticities[i] < 0.0f || m_metadata.m_chromaticities[i] > 1.0f) {
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += " - HAGC custom chromaticity is outside 0.0 to 1.0.\r\n";
        rv = icMaxStatus(rv, icValidateNonCompliant);
        break;
      }
    }
  }

  /* --- Alternate images (proposal Table 3) --- */

  for (i = 0; i < (int)m_metadata.GetNumAlternates(); i++) {
    const icHagcAlternateImage *pAlt = m_metadata.GetAlternate((icUInt8Number)i);
    if (!pAlt)
      continue;

    if (pAlt->m_nCurveReserved) {
      snprintf(buf, bufSize, " - HAGC alternate image %d has non-zero reserved bits.\r\n", i);
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }

    if (!pAlt->m_nControlPoints || pAlt->m_nControlPoints > icHagcMaxControlPoints) {
      snprintf(buf, bufSize, " - HAGC alternate image %d has %u control points; 1 to %d are representable.\r\n",
               i, pAlt->m_nControlPoints, icHagcMaxControlPoints);
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
      continue;
    }

    /* The gain evaluator divides by (x[i+1] - x[i]) for every segment, so a
     * non-increasing X array is not merely unordered - it makes the piecewise
     * cubic undefined or infinite.  Equality is rejected for the same reason. */
    for (j = 1; j < (int)pAlt->m_nControlPoints; j++) {
      if (!(pAlt->m_x[j] > pAlt->m_x[j - 1])) {
        snprintf(buf, bufSize,
                 " - HAGC alternate image %d control point X values are not strictly increasing at index %d.\r\n",
                 i, j);
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
        break;
      }
    }

    /* Proposal 0.1.3.6 fixes the sign of every Y from the headroom ordering,
     * so a Y whose sign disagrees with it cannot have been encoded and cannot
     * be encoded back out - Pack() writes the magnitude and the sign is
     * re-derived on the next read. */
    double s = icHagcYSign(m_metadata.m_baselineHeadroom, pAlt->m_headroom);
    for (j = 0; j < (int)pAlt->m_nControlPoints; j++) {
      if (pAlt->m_y[j] != 0.0f && ((pAlt->m_y[j] < 0.0f) != (s < 0.0))) {
        snprintf(buf, bufSize,
                 " - HAGC alternate image %d control point Y sign disagrees with the headroom ordering at index %d.\r\n",
                 i, j);
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
        break;
      }
    }

    /* Note 1 of the informative annex requires every coefficient to lie in
     * [0.0, 1.0].  The decode formula already guarantees that, so this only
     * fires for a model built in memory, which is exactly where it is needed. */
    double pSum = 0.0;
    for (j = 0; j < icHagcNumCoefficients; j++) {
      if (pAlt->m_coef[j] < 0.0f || pAlt->m_coef[j] > 1.0f) {
        snprintf(buf, bufSize, " - HAGC alternate image %d coefficient %d is outside 0.0 to 1.0.\r\n", i, j);
        sReport += icMsgValidateNonCompliant;
        sReport += sSigPathName;
        sReport += buf;
        rv = icMaxStatus(rv, icValidateNonCompliant);
        break;
      }
      pSum += pAlt->m_coef[j];
    }

    /* Note 3 of the informative annex: pSum cannot be zero, because mixing
     * type 3 divides by it.  Only type 3 can reach zero - the fixed sets for
     * types 0 to 2 all sum to 1. */
    if (pAlt->m_nMixingType == icHagcMixingCustom && pSum == 0.0) {
      snprintf(buf, bufSize,
               " - HAGC alternate image %d uses component mixing type 3 with all coefficients zero.\r\n", i);
      sReport += icMsgValidateNonCompliant;
      sReport += sSigPathName;
      sReport += buf;
      rv = icMaxStatus(rv, icValidateNonCompliant);
    }
  }

  /* --- Profile context (proposal 1.1) --- */

  if (!pProfile)
    return rv;

  if (pProfile->m_Header.colorSpace != icSigRgbData) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC is only permitted when the data colour space is RGB.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  if (pProfile->m_Header.deviceClass != icSigInputClass &&
      pProfile->m_Header.deviceClass != icSigDisplayClass) {
    sReport += icMsgValidateNonCompliant;
    sReport += sSigPathName;
    sReport += " - HAGC is only permitted in Input or Display class profiles.\r\n";
    rv = icMaxStatus(rv, icValidateNonCompliant);
  }

  /* Nothing here checks whether the curve is image-specific, and nothing should.
   * The indication is not in Tables 1 to 3 - it is the profile header flags of
   * ICC.1:2022 Table 21, bit 0 "Embedded profile" and bit 1 "Profile cannot be
   * used independently of the embedded colour data", which proposal 1.1 requires
   * to be set when the curve is image-specific.  The HDR Profiles amendment
   * reads it the same way, calling it "the embedded/cannot-be-used-independently
   * flag handling defined for HAGC".
   *
   * Those flags are the author's declaration rather than a fact derivable from
   * the tag, so there is no second source to cross-check them against: a profile
   * with bit 1 clear has declared the curve reusable across images, and that is
   * always self-consistent.  A diagnostic here would have nothing to compare. */

  return rv;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
