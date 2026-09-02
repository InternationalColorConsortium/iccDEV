/** @file
    File:       IccTagHagc.h

    Contains:   Header for implementation of the CIccTagHagc class
                and the standalone SMPTE ST 2094-50:2026 data model it wraps

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

#if !defined(_ICCTAGHAGC_H)
#define _ICCTAGHAGC_H

#include <string>
#include <vector>
#include "IccDefs.h"
#include "IccTagBasic.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccIO;
class CIccProfile;

/**
 ***********************************************************************
 * headroomAdaptiveGainCurveType structural limits.
 *
 * These are not defensive guesses; each is what the encoding itself can
 * express, so a value outside them cannot have come from a well formed
 * tag and is a decode error rather than a policy choice:
 *
 *  - the count of alternate images occupies 3 bits, so the field can hold
 *    0..7, but the proposal caps alternate images at 4.  This is the one
 *    limit the encoding is *wider* than the specification, so it is the
 *    only one Validate() has to police.
 *
 *    PROPOSAL-ISSUE (encoding, no register key): policing it at 4 is a
 *    ruling, not a transcription.  The HAGC proposal states the cap in prose
 *    and the encoding contradicts it by construction, and the proposal does
 *    not say which governs - a tag declaring 5 alternate images is well
 *    formed against the bit layout and out of range against the prose.  This
 *    implementation reads such a tag (all 5 are decoded, so no data is lost)
 *    and reports it from Validate(), which is the reading that keeps a
 *    parser from rejecting bytes the encoding permits while still surfacing
 *    the disagreement.  A future revision that widens the prose to 7 would
 *    need only this constant changed.
 *  - the gain curve point array last index occupies 5 bits, so it can hold
 *    0..31 and the count of control points is 1..32.  The proposal's
 *    "shall be >= 0 and <= 31" is therefore satisfied by construction.
 *  - the component mixing coefficients are a fixed set of six, in the order
 *    their presence flags and their array entries appear in the encoding.
 ***********************************************************************
 */
#define icHagcMaxAlternates      4
#define icHagcMaxControlPoints  32
#define icHagcNumCoefficients    6

/** The largest metadata block CIccTagHagc will hold, in bytes.
 *
 * A single tag is not a plausible place for a megabyte of SMPTE metadata; the
 * largest layout the format can describe is well under a kilobyte.  This
 * bound exists so a corrupt declared size cannot drive a huge allocation
 * before the parse rejects it, mirroring what CIccTagUnknown::Read does with
 * MAX_UNKNOWN_TAG_SIZE.  It is in the header because the XML and JSON
 * authoring paths have to apply it to the hex text *before* they allocate a
 * buffer to decode it into - reaching SetRawMetadata()'s copy of the check
 * means the allocation has already happened.
 *
 * PROPOSAL-ISSUE (no register key -- the corpus states no bound): unlike the
 * structural limits above, 1 MiB is not something the encoding expresses.  It
 * is a resource-exhaustion bound with no basis in the HAGC proposal, chosen
 * to sit three orders of magnitude above the largest layout the format can
 * describe so that no conformant tag can reach it.  Recorded as a decision
 * because a future revision could in principle define a metadata payload this
 * refuses, and because a bound with no spec basis is exactly the kind of
 * limit that becomes invisible once it stops being questioned. */
#define icHagcMaxMetadataSize 0x00100000u

/** The largest Application Version and Min Application Version the encoding
 * can carry: both are three bit fields (proposal Table 1, byte 12).  Named so
 * that the authoring paths can refuse a larger value instead of letting
 * Pack() mask it down to something the author never asked for. */
#define icHagcMaxApplicationVersion 7

/** Default HDR reference white in cd/m^2, used when the tag carries no
 * Custom HDR Reference White value (proposal 1.2.2.3). */
#define icHagcDefaultReferenceWhite 203.0

/**
 * Component mixing coefficients, in the order the Non-Zero Coefficient Flags
 * appear in the alternate image byte and in which the coefficient array is
 * serialized (proposal 1.1.3.2.1).  The enumerator values are the array
 * indices, so this order is load bearing for both Unpack() and Pack().
 */
typedef enum {
  icHagcCoefRed       = 0,
  icHagcCoefGreen     = 1,
  icHagcCoefBlue      = 2,
  icHagcCoefMax       = 3,
  icHagcCoefMin       = 4,
  icHagcCoefComponent = 5,
} icHagcCoefficient;

/**
 * Gain Curve Chromaticities Mode (proposal 1.2.2.7).  Modes 0..2 name entries
 * in Table 2 of ITU-T H.273 and mode 3 means the tag carries eight explicit
 * chromaticity values.
 *
 * The proposal cites the code points 2, 12 and 9 with the names BT.709-6,
 * Display P3 and BT.2020-2.  Checked against H.273 (V4) on 2026-09-02, the
 * second and third are right and the FIRST IS NOT: BT.709-6 is value 1, and
 * value 2 is Unspecified.  These enumerators follow the names - see
 * PROPOSAL-ISSUE HAGC-09 at m_nChromaticitiesMode.
 */
typedef enum {
  icHagcChromaticitiesBT709     = 0,
  icHagcChromaticitiesDisplayP3 = 1,
  icHagcChromaticitiesBT2020    = 2,
  icHagcChromaticitiesCustom    = 3,
} icHagcChromaticitiesMode;

/**
 * Component Mixing Type (proposal 1.1.3.2 and informative annex 1).  Types
 * 0..2 fix the six coefficients implicitly; only type 3 serializes them.
 */
typedef enum {
  icHagcMixingMax       = 0,   /* kMax = 1, all others 0 */
  icHagcMixingComponent = 1,   /* kComponent = 1, all others 0 */
  icHagcMixingWeighted  = 2,   /* kRed = kGreen = kBlue = 1/6, kMax = 1/2 */
  icHagcMixingCustom    = 3,   /* coefficients carried in the tag */
} icHagcMixingType;

/**
 ***********************************************************************
 * Class: icHagcAlternateImage
 *
 * Purpose:
 *  One alternate image's gain curve, decoded (proposal Table 3).
 *
 *  Every member is the *decoded* floating point value, not the uInt16 it
 *  was carried in.  The encoded form is deliberately not retained here:
 *  byte exact round tripping is the raw blob's job (see CIccTagHagc), and
 *  keeping a second, encoded copy of the same numbers would create a pair
 *  of fields that can silently disagree after any setter call.  The decode
 *  and encode formulas are exact inverses over the representable range, so
 *  nothing is lost by storing only the decoded side.
 ***********************************************************************
 */
class ICCPROFLIB_API icHagcAlternateImage
{
public:
  icHagcAlternateImage();

  /** Set m_nMixingType and reset m_coef[] to what that type implies: the
   * fixed values for types 0..2, all zero for type 3 (whose coefficients the
   * caller then fills in).  This is the only correct way to change the mixing
   * type - assigning m_nMixingType directly leaves the previous type's
   * coefficients in place, and Pack() derives the presence flags from the
   * values, so a leftover is written out as if it had been authored. */
  void SetMixingType(icHagcMixingType nType);

  /** Alternate HDR Headroom in log2 space (proposal 1.1.3.1). */
  icFloatNumber m_headroom;

  /** Component Mixing Type, 0..3 (proposal 1.1.3.2).  Set it through
   * SetMixingType() so m_coef[] stays consistent with it. */
  icHagcMixingType m_nMixingType;

  /** Component mixing coefficients indexed by icHagcCoefficient.  Populated
   * for every mixing type: types 0..2 fill in the fixed values the annex
   * defines, so consumers never have to special case the type.  Only type 3
   * serializes them. */
  icFloatNumber m_coef[icHagcNumCoefficients];

  /** Number of gain curve control points, 1..icHagcMaxControlPoints.  The
   * encoding carries this as a last index (count - 1). */
  icUInt8Number m_nControlPoints;

  /** When true the control point slopes were not carried in the tag and are
   * to be derived per clause C.3.9 of SMPTE ST 2094-50:2026 (proposal
   * 1.1.3.5).  m_slope[] is left zeroed in that case and stays zeroed: this
   * struct is the decoded wire format and nothing derived belongs in it.  The
   * derivation is icHagcDerivePchipSlopes(), which the evaluator calls at
   * Begin() time - read that function's header for where C.3.9 came from and
   * the two places this implementation deliberately departs from it. */
  bool m_bPchipSlope;

  /** The 2 reserved bits of the byte carrying the last index and the PCHIP
   * flag (proposal Table 3, byte M).  Shall be zero; retained so Validate()
   * can say when it was not. */
  icUInt8Number m_nCurveReserved;

  icFloatNumber m_x[icHagcMaxControlPoints];      /* control point X */
  icFloatNumber m_y[icHagcMaxControlPoints];      /* control point Y, signed */
  icFloatNumber m_slope[icHagcMaxControlPoints];  /* control point M slope */
};

/**
 ***********************************************************************
 * Class: icHagcMetadata
 *
 * Purpose:
 *  The SMPTE ST 2094-50:2026 metadata block carried by the
 *  headroomAdaptiveGainCurveType, decoded, and independent of any profile.
 *
 *  This is deliberately not a CIccTag.  The tone mapping evaluator and the
 *  A2B0 baking path both need to work from this model without a profile in
 *  hand, and callers outside IccProfLib need to be able to build one from
 *  metadata that never came from an ICC file at all.  CIccTagHagc is a thin
 *  wrapper that adds the ICC tag envelope around it.
 *
 *  Unpack() and Pack() are the only two places the wire format is known.
 ***********************************************************************
 */
class ICCPROFLIB_API icHagcMetadata
{
public:
  icHagcMetadata();

  /** Reset to the state a default constructed object has. */
  void Reset();

  /**
   * Decode a SMPTE ST 2094-50:2026 metadata block.
   *
   * pData/nSize describe the metadata alone, i.e. what follows the 12 byte
   * headroomAdaptiveGainCurveType header, of exactly the declared length.
   *
   * Returns false and leaves the object Reset() if the block is malformed or
   * runs out of bytes.  A false return is *not* a tag read failure: see
   * CIccTagHagc::Read().
   */
  bool Unpack(const icUInt8Number *pData, icUInt32Number nSize);

  /**
   * Encode this model back to a SMPTE ST 2094-50:2026 metadata block,
   * appending to buf.  Used only on the authoring path (XML/JSON parse,
   * SetMetadata(), A2B0 baking); tags that were read from a profile are
   * re-emitted from their retained raw bytes instead.
   *
   * Returns false if the model cannot be represented: a control point count
   * outside 1..icHagcMaxControlPoints, an alternate count above
   * icHagcMaxAlternates, or an application version above
   * icHagcMaxApplicationVersion.  Everything else it clamps to the encodable
   * range, which is why CIccTagHagc::SetMetadata() keeps a decode of these
   * bytes rather than the model it was handed.
   */
  bool Pack(std::vector<icUInt8Number> &buf) const;

  /** Number of alternate images actually present, 0..icHagcMaxAlternates.
   * Zero means no tone mapping is to be performed and the baseline image
   * should be clamped to the target colour volume (proposal 1.2.2.6). */
  icUInt8Number GetNumAlternates() const { return m_nAlternates; }

  const icHagcAlternateImage *GetAlternate(icUInt8Number n) const
  { return n < m_nAlternates ? &m_alternates[n] : NULL; }

  icHagcAlternateImage *GetAlternate(icUInt8Number n)
  { return n < m_nAlternates ? &m_alternates[n] : NULL; }

  /** Set the alternate image count, zeroing any newly exposed entries.
   * Returns false if n exceeds icHagcMaxAlternates. */
  bool SetNumAlternates(icUInt8Number n);

  /** Resolved HDR reference white in cd/m^2: the custom value when the tag
   * carries one, otherwise icHagcDefaultReferenceWhite (proposal 1.2.2.3). */
  icFloatNumber GetReferenceWhite() const;

  /* --- General Information (proposal Table 1, byte 12) --- */
  icUInt8Number m_nApplicationVersion;      /* 3 bits */
  icUInt8Number m_nMinApplicationVersion;   /* 3 bits */
  icUInt8Number m_nGeneralInfoReserved;     /* 2 bits, shall be zero */

  /* --- Global Tone Mapping Parameters (proposal Table 2) --- */

  /** True when a Custom HDR Reference White is carried.  Kept separate from
   * the value because "absent" and "present and equal to the 203.0 default"
   * are different byte layouts and both are legal. */
  bool m_bCustomReferenceWhite;
  icFloatNumber m_referenceWhite;           /* cd/m^2, meaningful iff the flag is set */

  /** Headroom Adaptive Tone Map flag (proposal 1.2.2.2).  When false the tag
   * carries no tone mapping metadata at all and tone mapping is left to the
   * implementer. */
  bool m_bHeadroomAdaptiveToneMap;

  icUInt8Number m_nGlobalFlagsReserved;     /* 6 bits of byte 0, shall be zero */

  /** Baseline HDR Headroom in log2 space (proposal 1.2.2.4). */
  icFloatNumber m_baselineHeadroom;

  /** Reference White Tone Mapping flag (proposal 1.2.2.5).  When true the
   * remaining bit fields of the same byte are zero on the wire, no records
   * follow, and the alternate images are absent from the file: their effective
   * values are derived per clause C.3.8 of SMPTE ST 2094-50, which
   * icHagcDeriveReferenceWhiteToneMap() implements.  This struct stays as the
   * file has it - empty - and the derivation happens in the evaluator. */
  bool m_bReferenceWhiteToneMapping;

  /** Gain Curve Chromaticities Mode (proposal 0.1.2.7).
   *
   * PROPOSAL-ISSUE HAGC-09: mode 0 is described as "the colour primaries with
   * a value of 2 in Table 2 in ITU-T H.273, i.e. primaries from Recommendation
   * ITU-R BT.709-6".  Those are two different things.  H.273 Table 2 value 2
   * is Unspecified; BT.709-6 is value 1.  Modes 1 and 2 cite 12 and 9, both
   * correct.  The enumerator below follows the NAME, because the number is the
   * part that is wrong and because value 2 is the one code point that means
   * "resolve against the profile's own matrix column tags" under the CICP
   * Unspecified-Primaries amendment - a reader following it would get a
   * profile-dependent answer where BT.709 was meant.
   *
   * PROPOSAL-ISSUE HAGC-10: nothing in the amendment says what a consumer does
   * with this field.  It is defined here and used nowhere - annex 1's gain
   * curve function never mentions chromaticities.  See the mode enum. */
  icHagcChromaticitiesMode m_nChromaticitiesMode;

  /** Common Component Mixing flag (proposal 1.1.2.8): the mixing mode, and
   * the coefficient array when the mode is 3, are carried only for alternate
   * 0 and shared by the rest. */
  bool m_bCommonComponentMixing;

  /** Common Curve Parameters flag (proposal 1.1.2.9): the control point
   * count, the PCHIP slope flag and the X coordinate array are carried only
   * for alternate 0 and shared by the rest.  Y coordinates and slope angles
   * stay per alternate. */
  bool m_bCommonCurveParameters;

  /** Custom chromaticities [xR yR xG yG xB yB xW yW] from Table 2 bytes
   * k+4 to k+19. Those sixteen bytes are always present - the block has a
   * fixed layout - but they carry a value only when m_nChromaticitiesMode is
   * icHagcChromaticitiesCustom. In every other mode the primaries come from
   * the ITU-T H.273 entry that 0.1.2.7 names, and this array reads zero. */
  icFloatNumber m_chromaticities[8];

  /* --- Diagnostics recorded during Unpack(), reported by Validate() --- */

  /** True when the block decoded cleanly.  A tag whose metadata failed to
   * decode still exists and still writes back byte exactly; this is what
   * lets Validate() say so. */
  bool m_bUnpacked;

  /** Bytes left over after the last field the layout accounts for.  Non-zero
   * means the declared metadata size is larger than the content, which is
   * legal to store but worth reporting. */
  icUInt32Number m_nTrailingBytes;

  /** True when the Reference White Tone Mapping flag was set but the bit
   * fields sharing its byte were not all zero.  Proposal 1.2.2.5 requires
   * them to be zero in that case and says their effective values must be
   * derived rather than read, so a non-zero encoding here is a producer
   * defect - and one that would otherwise be invisible, since the decoder
   * is required to ignore exactly those bits. */
  bool m_bRefWhiteToneMapFieldsNonZero;

protected:
  /** The field-by-field body of Unpack().  Split out so that Unpack() can
   * guarantee its contract - a failed decode leaves the object Reset() - from
   * a single place.  The layout has more than twenty exit points, and the one
   * that matters most is the alternate image loop, which runs *after*
   * m_nAlternates has been set from the count field: without a single reset
   * point, a block that is truncated mid record leaves a model claiming
   * alternates whose contents were never read. */
  bool UnpackFields(const icUInt8Number *pData, icUInt32Number nSize);

  icUInt8Number m_nAlternates;
  icHagcAlternateImage m_alternates[icHagcMaxAlternates];
};

/**
 ***********************************************************************
 * Class: CIccTagHagc
 *
 * Purpose:
 *  The headroomAdaptiveGainCurveType ICC tag ('hagc'), which wraps a
 *  SMPTE ST 2094-50:2026 metadata block in a 12 byte ICC header.
 *
 *  Read() retains the metadata bytes verbatim *and* decodes them; Write()
 *  re-emits the retained bytes.  That combination is what makes the tag
 *  transparent to a SMPTE revision that adds fields this decoder does not
 *  know: an unparsed field survives a load/save cycle instead of being
 *  silently dropped by a re-encode from the decoded model.  The decoded
 *  model is what Describe(), Validate() and the tone mapping path consume.
 *
 *  Authoring (XML/JSON parse, SetMetadata()) goes the other way: it fills
 *  the model and calls Pack() to build the raw bytes, so the retained blob
 *  is always the authoritative serialization whichever direction the tag
 *  was created from.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccTagHagc : public CIccTag
{
public:
  CIccTagHagc();
  CIccTagHagc(const CIccTagHagc &ITHagc);
  CIccTagHagc &operator=(const CIccTagHagc &HagcTag);
  virtual CIccTag *NewCopy() const { return new CIccTagHagc(*this); }
  virtual ~CIccTagHagc();

  virtual icTagTypeSignature GetType() const { return icSigHeadroomAdaptiveGainCurveType; }
  virtual const icChar *GetClassName() const { return "CIccTagHagc"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile *pProfile = NULL) const;

  /** The decoded SMPTE ST 2094-50:2026 model. */
  const icHagcMetadata &GetMetadata() const { return m_metadata; }
  icHagcMetadata &GetMetadata() { return m_metadata; }

  /**
   * Replace the model and re-derive the raw bytes from it with Pack().
   * Returns false, leaving the tag unchanged, if the model cannot be packed.
   */
  bool SetMetadata(const icHagcMetadata &metadata);

  /**
   * Install a metadata block verbatim, then decode it.  Returns false only
   * if the allocation fails; a block that does not decode is still stored,
   * matching what Read() does with a malformed tag.
   */
  bool SetRawMetadata(const icUInt8Number *pData, icUInt32Number nSize);

  const icUInt8Number *GetRawMetadata() const { return m_pRawData; }
  icUInt32Number GetRawMetadataSize() const { return m_nRawSize; }

  /** Pad bytes seen between the metadata and the end of the tag on Read().
   * The proposal requires the tag's overall length to be a multiple of four
   * (annex 1 note 2) while the SMPTE block itself need not be, so 0..3 null
   * bytes are expected here and anything else is a defect Validate() reports. */
  icUInt32Number GetPadSize() const { return m_nPadSize; }
  bool HasNonZeroPad() const { return m_bNonZeroPad; }

protected:
  void Cleanup();

  icHagcMetadata m_metadata;

  /** The SMPTE ST 2094-50:2026 metadata exactly as it appeared in the file,
   * or exactly as Pack() produced it when the tag was authored. */
  icUInt8Number *m_pRawData;
  icUInt32Number m_nRawSize;

  /** The metadata size the tag header declared when the tag was read.  Kept
   * apart from m_nRawSize because a tag can declare more metadata than the
   * tag body has room for; the bytes actually retained are then fewer, and
   * Validate() reports the difference rather than the read failing outright.
   *
   * It is not what Write() emits.  Write() declares m_nRawSize - the bytes it
   * actually has - so a truncated tag is rewritten as the honest shorter one
   * rather than re-declaring a length no reader would find.  Byte exactness
   * is therefore lost on exactly the tags that were malformed to begin with,
   * which is the right trade. */
  icUInt32Number m_nDeclaredSize;

  icUInt32Number m_nPadSize;
  bool m_bNonZeroPad;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCTAGHAGC_H)
