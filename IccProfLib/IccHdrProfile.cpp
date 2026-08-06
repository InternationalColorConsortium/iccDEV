/** @file
    File:       IccHdrProfile.cpp

    Contains:   Implementation of HDR Profile (ICC.1 clause 8.10) recognition,
                CICP ColourPrimaries resolution, and the HDR Image /
                HDR Display metadataTag reader

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
// -Initial implementation of HDR Profile recognition and CICP primaries
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #pragma warning( disable: 4786) //disable warning in <list.h>
  #include <windows.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

#include "IccHdrProfile.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagDict.h"
#include "IccTagHagc.h"
#include "IccMatrixMath.h"
#include "IccUtil.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

/**
 ****************************************************************************
 * ITU-T H.273 Table 2 ColourPrimaries.
 *
 * These chromaticities do not exist anywhere else in IccProfLib. The cicp tag
 * carries the code point and CIccTagCicp::Describe() renders a handful of
 * combined codes as prose, but neither has ever held the coordinates, so
 * there is nothing here to refactor out of - this is the first place the
 * table is expressed as data. Every value below is quoted directly from
 * Table 2 of Recommendation ITU-T H.273 (07/2021).
 *
 * Values absent from this table are Reserved, unassigned, or - in the case of
 * 2 - deliberately without fixed coordinates: clause 9.2.17 directs value 2
 * to be resolved against the profile's own matrix column tags instead, which
 * is what icGetProfilePrimaries() does.
 ****************************************************************************
 */
static const struct {
  icUInt8Number nValue;
  icCicpPrimaries primaries;
} icCicpPrimariesTable[] = {
  /* 1: Rec. ITU-R BT.709-6 / IEC 61966-2-1 sRGB / sYCC, D65 */
  {  1, { 0.640f, 0.330f,  0.300f, 0.600f,  0.150f, 0.060f,  0.3127f, 0.3290f } },

  /* 4: Rec. ITU-R BT.470-6 System M (NTSC 1953), Illuminant C */
  {  4, { 0.670f, 0.330f,  0.210f, 0.710f,  0.140f, 0.080f,  0.3100f, 0.3160f } },

  /* 5: Rec. ITU-R BT.470-6 System B, G / BT.601-7 625, D65 */
  {  5, { 0.640f, 0.330f,  0.290f, 0.600f,  0.150f, 0.060f,  0.3127f, 0.3290f } },

  /* 6: Rec. ITU-R BT.601-7 525 / SMPTE ST 170, D65 */
  {  6, { 0.630f, 0.340f,  0.310f, 0.595f,  0.155f, 0.070f,  0.3127f, 0.3290f } },

  /* 7: SMPTE ST 240. Table 2 gives it the same chromaticities as 6; the two
   * code points differ in their transfer characteristics, not their gamut. */
  {  7, { 0.630f, 0.340f,  0.310f, 0.595f,  0.155f, 0.070f,  0.3127f, 0.3290f } },

  /* 8: Generic film (colour filters using Illuminant C) */
  {  8, { 0.681f, 0.319f,  0.243f, 0.692f,  0.145f, 0.049f,  0.3100f, 0.3160f } },

  /* 9: Rec. ITU-R BT.2020-2 / Rec. ITU-R BT.2100-2, D65 */
  {  9, { 0.708f, 0.292f,  0.170f, 0.797f,  0.131f, 0.046f,  0.3127f, 0.3290f } },

  /* 10: SMPTE ST 428-1 (CIE 1931 XYZ). The primaries are the XYZ axes
   * themselves and the white is equal-energy illuminant E, not a D series. */
  { 10, { 1.000f, 0.000f,  0.000f, 1.000f,  0.000f, 0.000f,
          (icFloatNumber)(1.0 / 3.0), (icFloatNumber)(1.0 / 3.0) } },

  /* 11: SMPTE RP 431-2 (DCI P3), DCI white - not D65 */
  { 11, { 0.680f, 0.320f,  0.265f, 0.690f,  0.150f, 0.060f,  0.3140f, 0.3510f } },

  /* 12: SMPTE EG 432-1 (Display P3). Same primaries as 11, D65 white. */
  { 12, { 0.680f, 0.320f,  0.265f, 0.690f,  0.150f, 0.060f,  0.3127f, 0.3290f } },

  /* 22: EBU Tech. 3213-E, D65 */
  { 22, { 0.630f, 0.340f,  0.295f, 0.605f,  0.155f, 0.077f,  0.3127f, 0.3290f } },
};

/**
 ****************************************************************************
 * Name: icGetCicpPrimaries
 *
 * Purpose: Look up an ITU-T H.273 Table 2 ColourPrimaries value.
 *
 * Args:
 *  nColourPrimaries = the code point
 *  primaries = receives the chromaticities
 *
 * Return:
 *  true if the value names a set of primaries, false otherwise
 *****************************************************************************
 */
bool icGetCicpPrimaries(icUInt8Number nColourPrimaries, icCicpPrimaries &primaries)
{
  size_t i;
  for (i = 0; i < sizeof(icCicpPrimariesTable) / sizeof(icCicpPrimariesTable[0]); i++) {
    if (icCicpPrimariesTable[i].nValue == nColourPrimaries) {
      primaries = icCicpPrimariesTable[i].primaries;
      return true;
    }
  }
  return false;
}

/**
 ****************************************************************************
 * Name: icHdrXyzToChromaticity
 *
 * Purpose: Convert one CIEXYZ tristimulus triplet to (x, y), per step 2 of
 *  clause 10.3 NOTE 1.
 *
 *  A triplet summing to zero has no chromaticity at all - the conversion is
 *  0/0, not a large number - so it is refused rather than clamped. That case
 *  is reachable from a real profile: a blueMatrixColumnTag of all zeros is a
 *  legal encoding and appears in degenerate fixtures.
 *****************************************************************************
 */
static bool icHdrXyzToChromaticity(icFloatNumber X, icFloatNumber Y, icFloatNumber Z,
                                   icFloatNumber &x, icFloatNumber &y)
{
  icFloatNumber sum = X + Y + Z;

  /* Written as a negated comparison so a NaN sum - which every ordered
   * comparison would accept - is refused too. */
  if (!(sum > 0.0f) && !(sum < 0.0f))
    return false;

  x = X / sum;
  y = Y / sum;
  return true;
}

/**
 ****************************************************************************
 * Name: icHdrFindTag
 *
 * Purpose: Fetch a tag by signature, loading it if the profile has not.
 *
 *  CIccProfile::FindTagConst() returns a tag only when it is already in
 *  memory; it deliberately does not load one, because loading needs the
 *  profile's attached IO object and is not a const operation. That is wrong
 *  for everything in this file. A profile opened rather than read - which is
 *  what CIccCmm does for every profile it is given a path to - carries a tag
 *  directory and no tag objects, so FindTagConst() returns NULL for tags that
 *  are demonstrably present and every question this module answers comes back
 *  "no cicpTag, not an HDR Profile". The failure is silent and direction
 *  dependent: read the same file eagerly and the answers are right.
 *
 *  So the lookup goes through FindTag(), which loads on demand, via a cast.
 *  The cast is sound in the way that matters: loading a tag populates a cache
 *  and changes nothing about the profile's value, which is exactly the reason
 *  FindTag() exists rather than requiring callers to pre-load. Keeping the
 *  const on the interface is worth this much - a caller has no business being
 *  handed a mutable profile to ask whether it is an HDR Profile.
 *****************************************************************************
 */
static const CIccTag *icHdrFindTag(const CIccProfile *pProfile, icSignature sig)
{
  return ((CIccProfile*)pProfile)->FindTag(sig);
}

/**
 ****************************************************************************
 * Name: icHdrGetXyzTag
 *
 * Purpose: Fetch one XYZType tag's first triplet as floats.
 *****************************************************************************
 */
static bool icHdrGetXyzTag(const CIccProfile *pProfile, icTagSignature sig,
                           icFloatNumber *pXYZ)
{
  const CIccTag *pTag = icHdrFindTag(pProfile, sig);
  if (!pTag || pTag->GetType() != icSigXYZArrayType)
    return false;

  const CIccTagXYZ *pXyzTag = (const CIccTagXYZ*)pTag;
  if (!pXyzTag->GetSize())
    return false;

  icXYZNumber &xyz = (*pXyzTag)[0];
  pXYZ[0] = icFtoD(xyz.X);
  pXYZ[1] = icFtoD(xyz.Y);
  pXYZ[2] = icFtoD(xyz.Z);
  return true;
}

/**
 ****************************************************************************
 * Name: icGetProfilePrimaries
 *
 * Purpose: Recover the source primaries from the profile's own tags, per
 *  clause 10.3 NOTE 1.
 *
 * Args:
 *  pProfile = the profile
 *  primaries = receives the recovered chromaticities
 *
 * Return:
 *  true if the primaries could be recovered
 *****************************************************************************
 */
bool icGetProfilePrimaries(const CIccProfile *pProfile, icCicpPrimaries &primaries)
{
  if (!pProfile)
    return false;

  /* Clause 4.1 of the CICP amendment scopes this to three-component
   * matrix-based profiles: without all three matrix column tags the value 2
   * keeps its original "unknown" meaning and there is nothing to recover. */
  icFloatNumber red[3], green[3], blue[3], white[3];
  if (!icHdrGetXyzTag(pProfile, icSigRedMatrixColumnTag, red) ||
      !icHdrGetXyzTag(pProfile, icSigGreenMatrixColumnTag, green) ||
      !icHdrGetXyzTag(pProfile, icSigBlueMatrixColumnTag, blue) ||
      !icHdrGetXyzTag(pProfile, icSigMediaWhitePointTag, white))
    return false;

  /* PROPOSAL-ISSUE CICP-01 (design-level; one sentence would close it) -- the
   * amendment's NOTE 1 delegates the chromaticAdaptationTag relationship to
   * ICC.1 9.2.15 / 9.2.36 / Annex F.3 and does not state the direction, but
   * those clauses describe the forward relationship while this derivation needs
   * the inverse.  The wrong direction returns a complete, plausible set of
   * chromaticities, so the error is not self-detecting.
   *
   * Step 1 of NOTE 1 asks for the tristimulus values "expressed relative to
   * the profile's actual adopted white". The matrix column tags are encoded
   * relative to the *PCS* adopted white (D50); the chromaticAdaptationTag is
   * the matrix that took them there, so recovering the profile's actual
   * adopted white means applying its inverse. NOTE 2 covers the other case:
   * with no chromaticAdaptationTag the profile's actual adopted white is D50
   * already and the encoded values are used as they stand.
   *
   * Getting this backwards is not a small error. For a BT.2020 display
   * profile the chad matrix carries a D65-to-D50 adaptation, so skipping the
   * inverse reports D50-adapted chromaticities as though they were the
   * display's own - which moves the white point by roughly 0.035 in x, far
   * more than the tolerance any downstream gamut computation would expect. */
  const CIccTag *pChad = icHdrFindTag(pProfile, icSigChromaticAdaptationTag);
  if (pChad && pChad->GetType() == icSigS15Fixed16ArrayType) {
    const CIccTagS15Fixed16 *pChadTag = (const CIccTagS15Fixed16*)pChad;

    if (pChadTag->GetSize() < 9)
      return false;

    /* Read through GetValues() rather than operator[]: it is the const
     * accessor, it applies the s15Fixed16 to float conversion itself, and it
     * carries the overflow-safe bounds check that operator[] has none of. */
    icFloatNumber vals[9];
    if (!pChadTag->GetValues(vals, 0, 9))
      return false;

    CIccMatrixMath chad(3, 3);
    int i;
    for (i = 0; i < 9; i++)
      *chad.entry((icUInt16Number)(i / 3), (icUInt16Number)(i % 3)) = vals[i];

    if (!chad.Invert())
      return false;

    icFloatNumber tmp[3];
    chad.VectorMult(tmp, red);   red[0] = tmp[0];   red[1] = tmp[1];   red[2] = tmp[2];
    chad.VectorMult(tmp, green); green[0] = tmp[0]; green[1] = tmp[1]; green[2] = tmp[2];
    chad.VectorMult(tmp, blue);  blue[0] = tmp[0];  blue[1] = tmp[1];  blue[2] = tmp[2];
    chad.VectorMult(tmp, white); white[0] = tmp[0]; white[1] = tmp[1]; white[2] = tmp[2];
  }
  else if (pChad) {
    /* Present but not an s15Fixed16Array: the tag is malformed and the
     * adaptation cannot be undone. Reporting failure is better than silently
     * treating the profile as unadapted, which would return the wrong white. */
    return false;
  }

  if (!icHdrXyzToChromaticity(red[0], red[1], red[2], primaries.xRed, primaries.yRed) ||
      !icHdrXyzToChromaticity(green[0], green[1], green[2], primaries.xGreen, primaries.yGreen) ||
      !icHdrXyzToChromaticity(blue[0], blue[1], blue[2], primaries.xBlue, primaries.yBlue) ||
      !icHdrXyzToChromaticity(white[0], white[1], white[2], primaries.xWhite, primaries.yWhite))
    return false;

  return true;
}

/**
 ****************************************************************************
 * Name: icGetResolvedPrimaries
 *
 * Purpose: Resolve the source primaries the way a cicpTag consumer should.
 *****************************************************************************
 */
bool icGetResolvedPrimaries(const CIccProfile *pProfile, icUInt8Number nColourPrimaries,
                            icCicpPrimaries &primaries, bool *bFromProfile /*=NULL*/)
{
  if (bFromProfile)
    *bFromProfile = false;

  if (nColourPrimaries != icCicpPrimariesUnspecified)
    return icGetCicpPrimaries(nColourPrimaries, primaries);

  if (!icGetProfilePrimaries(pProfile, primaries))
    return false;

  if (bFromProfile)
    *bFromProfile = true;

  return true;
}

/**
 ****************************************************************************
 * Name: icGetHdrTransferName
 *****************************************************************************
 */
const icChar *icGetHdrTransferName(icUInt8Number nTransferCharacteristics)
{
  switch (nTransferCharacteristics) {
    case icCicpTransferLinear: return "Linear";
    case icCicpTransferPQ:     return "SMPTE ST 2084 (PQ)";
    case icCicpTransferHLG:    return "Rec. ITU-R BT.2100 (HLG)";
    default:                   return NULL;
  }
}

/*
 * ===========================================================================
 * HDR Image and HDR Display metadata (clauses 8.10.4 and 8.10.5)
 *
 * PROPOSAL-ISSUE HDR-05 (external dependency, blocking) -- 8.10.4 and 8.10.5
 * make the ICC dictType Metadata Registry authoritative for these key names and
 * their encodings, and the registry is not supplied with the amendment.  Every
 * ASSUMED marker below is therefore a reconstruction, and none of them feeds a
 * diagnostic: an assumed key name must not produce a validation message about
 * someone else's profile.
 *
 * Every ASSUMED marker below flags a reconstruction that the ICC dictType
 * Metadata Registry - which the amendment names as authoritative and does not
 * reproduce - must be checked against.
 * ===========================================================================
 */

/* ASSUMED: the registered key names. The amendment's prose names the entries
 * only by their expansions ("Content HDR Reference White Luminance") and by
 * these abbreviations in the precedence list of 8.10.5. */
static const char *kIccHdrKeyCrwl = "CRWL";   /* Content HDR Reference White Luminance */
static const char *kIccHdrKeyCll  = "CLL";    /* Content Light Level */
static const char *kIccHdrKeyMdcv = "MDCV";   /* Mastering Display Colour Volume */
static const char *kIccHdrKeyCcv  = "CCV";    /* Content Colour Volume */
static const char *kIccHdrKeyDerh = "DERH";   /* Display Extended Range Headroom */
static const char *kIccHdrKeyDrwl = "DRWL";   /* Display HDR Reference White Luminance */
static const char *kIccHdrKeyDcv  = "DCV";    /* Display Colour Volume */

/**
 ****************************************************************************
 * Name: icHdrNarrow
 *
 * Purpose: Narrow a dictType value to 8-bit for numeric parsing.
 *
 *  dictType values are UTF-16 and CIccTagDict hands them back as a wstring.
 *  Every value this reader parses is a number written in ASCII, so any code
 *  unit above 0x7F cannot be part of one; such a value is rejected rather
 *  than narrowed, because narrowing it would turn a non-numeric string into a
 *  plausible-looking one.
 *****************************************************************************
 */
static bool icHdrNarrow(const std::wstring &src, std::string &dst)
{
  dst.clear();
  dst.reserve(src.size());

  size_t i;
  for (i = 0; i < src.size(); i++) {
    if (src[i] > 0x7F || src[i] < 0)
      return false;
    dst += (char)src[i];
  }
  return true;
}

/**
 ****************************************************************************
 * Name: icHdrParseNumbers
 *
 * Purpose: Parse a whitespace-, comma- or semicolon-separated list of decimal
 *  numbers, requiring exactly nWanted of them.
 *
 *  Requiring the exact count rather than "at least" is what makes a
 *  reconstruction safe to act on: a value with the wrong number of fields is
 *  a value in a shape this build does not know, and is reported as unparsed
 *  rather than read as its first few fields.
 *****************************************************************************
 */
static bool icHdrParseNumbers(const std::string &s, double *pVals, int nWanted)
{
  const char *p = s.c_str();
  int n = 0;

  while (*p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' ||
                  *p == '\r' || *p == '\n'))
      p++;

    if (!*p)
      break;

    if (n >= nWanted)
      return false;

    char *pEnd = NULL;
    double v = strtod(p, &pEnd);

    if (pEnd == p)
      return false;

    /* strtod reports overflow and underflow through HUGE_VAL/0 plus errno; a
     * non-finite luminance would poison every derivation downstream, so it is
     * refused here rather than propagated. */
    if (!(v == v) || v > 1.0e30 || v < -1.0e30)
      return false;

    pVals[n++] = v;
    p = pEnd;
  }

  return n == nWanted;
}

/**
 ****************************************************************************
 * Name: icHdrGetDictValue
 *
 * Purpose: Fetch one dictType entry as a narrowed 8-bit string.
 *
 * Return:
 *  true when the key is present (bParseable then says whether the value
 *  narrowed cleanly), false when the key is absent
 *****************************************************************************
 */
static bool icHdrGetDictValue(const CIccTagDict *pDict, const char *szKey,
                              std::string &value, bool &bParseable)
{
  bool bIsSet = false;
  std::wstring wide = pDict->GetValue(szKey, &bIsSet);

  if (!bIsSet)
    return false;

  bParseable = icHdrNarrow(wide, value);
  return true;
}

/**
 ****************************************************************************
 * Name: icHdrParseColourVolume
 *
 * Purpose: Parse a colour-volume entry (MDCV or DCV) into primaries plus a
 *  luminance range.
 *
 *  ASSUMED: ten values in SMPTE ST 2086 order -
 *    xR yR xG yG xB yB xW yW maxLuminance minLuminance
 *  with chromaticities as plain decimals in [0, 1] and luminances in cd/m^2.
 *
 *  This is the single most speculative parse in the file, which is why it is
 *  one isolated function. ST 2086 is the shape "Mastering Display Colour
 *  Volume" denotes everywhere it appears in the video standards the amendment
 *  draws on, and clause 8.10.5 describes the Display Colour Volume entry as
 *  carrying exactly "its colour volume (maximum and minimum luminance and
 *  primaries)" - the same eight-plus-two fields. But ST 2086 encodes those
 *  fields as scaled integers in a binary payload, and dictType values are
 *  text, so the *textual* representation here is a reconstruction on top of a
 *  reconstruction. Nothing in this build raises a validation diagnostic from
 *  it; it feeds Describe()-style output and the 8.10.5 b)/c) headroom
 *  derivations, and a value that does not match this shape is reported as
 *  unparsed rather than guessed at.
 *****************************************************************************
 */
static bool icHdrParseColourVolume(const std::string &s, icCicpPrimaries &primaries,
                                   icFloatNumber &maxLuminance, icFloatNumber &minLuminance)
{
  double v[10];
  if (!icHdrParseNumbers(s, v, 10))
    return false;

  int i;
  for (i = 0; i < 8; i++) {
    if (v[i] < 0.0 || v[i] > 1.0)
      return false;
  }

  /* A colour volume whose maximum is at or below its minimum describes no
   * volume at all, and 8.10.5 b) would divide by it. */
  if (!(v[8] > 0.0) || v[9] < 0.0 || !(v[8] > v[9]))
    return false;

  primaries.xRed   = (icFloatNumber)v[0];  primaries.yRed   = (icFloatNumber)v[1];
  primaries.xGreen = (icFloatNumber)v[2];  primaries.yGreen = (icFloatNumber)v[3];
  primaries.xBlue  = (icFloatNumber)v[4];  primaries.yBlue  = (icFloatNumber)v[5];
  primaries.xWhite = (icFloatNumber)v[6];  primaries.yWhite = (icFloatNumber)v[7];
  maxLuminance = (icFloatNumber)v[8];
  minLuminance = (icFloatNumber)v[9];
  return true;
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::CIccHdrMetadataReader
 *****************************************************************************
 */
CIccHdrMetadataReader::CIccHdrMetadataReader()
{
  m_bHasCrwl = m_bHasCll = m_bHasMdcv = m_bHasCcv = false;
  m_bHasDerh = m_bHasDrwl = m_bHasDcv = false;
  m_bUnparsed = false;

  m_crwl = (icFloatNumber)icHdrDefaultContentReferenceWhite;
  m_maxCll = m_maxFall = 0.0f;
  m_mdcvMaxLuminance = m_mdcvMinLuminance = 0.0f;
  m_derh = m_drwl = 0.0f;
  m_dcvMaxLuminance = m_dcvMinLuminance = 0.0f;

  memset(&m_mdcvPrimaries, 0, sizeof(m_mdcvPrimaries));
  memset(&m_dcvPrimaries, 0, sizeof(m_dcvPrimaries));
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::Read
 *
 * Purpose: Read the HDR Image and HDR Display entries out of a profile's
 *  metadataTag.
 *
 * Return:
 *  true when a metadataTag was found and read, false when there is none
 *****************************************************************************
 */
bool CIccHdrMetadataReader::Read(const CIccProfile *pProfile)
{
  if (!pProfile)
    return false;

  const CIccTag *pTag = icHdrFindTag(pProfile, icSigMetaDataTag);
  if (!pTag || pTag->GetType() != icSigDictType)
    return false;

  const CIccTagDict *pDict = (const CIccTagDict*)pTag;
  std::string value;
  bool bParseable;
  double v[2];

  /* --- HDR Image (8.10.4) --- */

  /* ASSUMED: a single decimal luminance in cd/m^2. NOTE 10 says the encoding
   * is "defined in accordance with ISO 22028-5", which this build does not
   * have; a plain decimal is the only representation the amendment's own
   * "203 cd/m^2" prose commits to. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCrwl, value, bParseable)) {
    if (bParseable && icHdrParseNumbers(value, v, 1) && v[0] > 0.0) {
      m_crwl = (icFloatNumber)v[0];
      m_bHasCrwl = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  /* ASSUMED: two decimals, MaxCLL then MaxFALL, both in cd/m^2 - the pair
   * "Content Light Level" denotes in the video standards. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCll, value, bParseable)) {
    if (bParseable && icHdrParseNumbers(value, v, 2) && v[0] >= 0.0 && v[1] >= 0.0) {
      m_maxCll = (icFloatNumber)v[0];
      m_maxFall = (icFloatNumber)v[1];
      m_bHasCll = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyMdcv, value, bParseable)) {
    if (bParseable && icHdrParseColourVolume(value, m_mdcvPrimaries,
                                             m_mdcvMaxLuminance, m_mdcvMinLuminance)) {
      m_bHasMdcv = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  /* Retained raw. Content Colour Volume has no single reconstructable shape:
   * the ST 2094 family defines several, and picking one would be a guess
   * rather than the informed reconstruction the other entries get. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCcv, value, bParseable)) {
    m_bHasCcv = true;
    if (bParseable)
      m_ccvRaw = value;
    else
      m_bUnparsed = true;
  }

  /* --- HDR Display (8.10.5) --- */

  /* ASSUMED: a single decimal ratio. 8.10.5 defines the quantity itself
   * unambiguously - peak luminance over HDR reference white luminance - so
   * only the textual form is reconstructed here. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyDerh, value, bParseable)) {
    if (bParseable && icHdrParseNumbers(value, v, 1) && v[0] > 0.0) {
      m_derh = (icFloatNumber)v[0];
      m_bHasDerh = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  /* ASSUMED: a single decimal luminance in cd/m^2, as for CRWL. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyDrwl, value, bParseable)) {
    if (bParseable && icHdrParseNumbers(value, v, 1) && v[0] > 0.0) {
      m_drwl = (icFloatNumber)v[0];
      m_bHasDrwl = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyDcv, value, bParseable)) {
    if (bParseable && icHdrParseColourVolume(value, m_dcvPrimaries,
                                             m_dcvMaxLuminance, m_dcvMinLuminance)) {
      m_bHasDcv = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  return true;
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::GetResolvedContentReferenceWhite
 *****************************************************************************
 */
icFloatNumber CIccHdrMetadataReader::GetResolvedContentReferenceWhite() const
{
  return m_bHasCrwl ? m_crwl : (icFloatNumber)icHdrDefaultContentReferenceWhite;
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::HasAnyHdrEntry
 *****************************************************************************
 */
bool CIccHdrMetadataReader::HasAnyHdrEntry() const
{
  return m_bHasCrwl || m_bHasCll || m_bHasMdcv || m_bHasCcv ||
         m_bHasDerh || m_bHasDrwl || m_bHasDcv;
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::ResolveDisplayHeadroom
 *
 * Purpose: Apply the precedence of clause 8.10.5 a) to d).
 *
 *  The order is not a fallback chain that stops at the first available value
 *  - it is normative about which value wins when several are available. NOTE
 *  13 is explicit: when all three entries are present and disagree, DERH is
 *  authoritative and the DCV/DRWL derivation "shall not be recomputed". So
 *  the DERH branch returns immediately rather than cross-checking, and the
 *  disagreement is the profile author's to resolve.
 *
 * Args:
 *  headroom = receives the resolved value when the return is not
 *             icHdrHeadroomNone
 *
 * Return:
 *  the rule that produced the value
 *****************************************************************************
 */
icHdrHeadroomSource CIccHdrMetadataReader::ResolveDisplayHeadroom(icFloatNumber &headroom) const
{
  /* a) DERH taken directly */
  if (m_bHasDerh) {
    headroom = m_derh;
    return icHdrHeadroomDerh;
  }

  /* b) DCV maximum luminance divided by DRWL */
  if (m_bHasDcv && m_bHasDrwl && m_drwl > 0.0f) {
    headroom = m_dcvMaxLuminance / m_drwl;
    return icHdrHeadroomDcvDrwl;
  }

  /* c) DCV maximum luminance divided by the content reference white, which
   * carries 8.10.4's 203 cd/m^2 default when no CRWL entry is present. */
  if (m_bHasDcv) {
    icFloatNumber crwl = GetResolvedContentReferenceWhite();
    if (crwl > 0.0f) {
      headroom = m_dcvMaxLuminance / crwl;
      return icHdrHeadroomDcvCrwl;
    }
  }

  /* d) not derivable from the profile */
  return icHdrHeadroomNone;
}

/**
 ****************************************************************************
 * Name: icHdrIsRgbMatrixBased
 *
 * Purpose: Test the structural shape clause 8.10.6 requires: RGB data colour
 *  space, Input or Display class, and the three matrix column and TRC tags of
 *  a three-component matrix-based profile (8.3.3 / 8.4.3).
 *****************************************************************************
 */
static bool icHdrIsRgbMatrixBased(const CIccProfile *pProfile)
{
  if (pProfile->m_Header.colorSpace != icSigRgbData)
    return false;

  if (pProfile->m_Header.deviceClass != icSigInputClass &&
      pProfile->m_Header.deviceClass != icSigDisplayClass)
    return false;

  return pProfile->IsTagPresent(icSigRedMatrixColumnTag) &&
         pProfile->IsTagPresent(icSigGreenMatrixColumnTag) &&
         pProfile->IsTagPresent(icSigBlueMatrixColumnTag) &&
         pProfile->IsTagPresent(icSigRedTRCTag) &&
         pProfile->IsTagPresent(icSigGreenTRCTag) &&
         pProfile->IsTagPresent(icSigBlueTRCTag);
}

/**
 ****************************************************************************
 * Name: icGetHdrProfileInfo
 *
 * Purpose: Classify a profile against clause 8.10 and resolve everything the
 *  clause defines a precedence for.
 *
 * Args:
 *  pProfile = the profile
 *  info = receives the classification and the resolved values
 *
 * Return:
 *  true unless pProfile is NULL
 *****************************************************************************
 */
bool icGetHdrProfileInfo(const CIccProfile *pProfile, icHdrProfileInfo &info)
{
  memset(&info, 0, sizeof(info));
  info.nClass = icHdrProfileNone;
  info.contentReferenceWhite = (icFloatNumber)icHdrDefaultContentReferenceWhite;
  info.nHeadroomSource = icHdrHeadroomNone;

  if (!pProfile)
    return false;

  info.bRgbMatrixBased = icHdrIsRgbMatrixBased(pProfile);

  /* "4.5.0.0 or later within v4". Comparing the whole version word rather
   * than the minor nibble alone keeps a v5 profile - which is a different
   * major version with its own tag model - out of the HDR Profile class. */
  info.bVersion4_5 = (pProfile->m_Header.version >= icVersionNumberV4_5 &&
                      pProfile->m_Header.version < icVersionNumberV5);

  const CIccTag *pCicp = icHdrFindTag(pProfile, icSigCicpTag);
  if (pCicp && pCicp->GetType() == icSigCicpType) {
    /* GetFields() is not const, so the read goes through a non-const view of a
     * tag this function only ever reads. Casting here rather than taking a
     * non-const profile keeps the constness where it belongs - on the caller,
     * which has no business being handed a mutable profile to ask a question. */
    CIccTagCicp *pCicpTag = (CIccTagCicp*)pCicp;
    icUInt8Number mtx, full;
    pCicpTag->GetFields(info.nColourPrimaries, info.nTransferCharacteristics, mtx, full);
    info.bHasCicp = true;
    info.bTransferIsHdr = (info.nTransferCharacteristics == icCicpTransferLinear ||
                           info.nTransferCharacteristics == icCicpTransferPQ ||
                           info.nTransferCharacteristics == icCicpTransferHLG);
  }

  info.bHasHagc = pProfile->IsTagPresent(icSigHeadroomAdaptiveGainCurveTag);
  info.bHasAToB0 = pProfile->IsTagPresent(icSigAToB0Tag);
  info.bHasBToA0 = pProfile->IsTagPresent(icSigBToA0Tag);

  /* Content reference white (8.10.4). The HAGC tag carries its own HDR
   * reference white, and when both it and a CRWL entry are present they are
   * describing the same quantity; the HAGC value is preferred because it is
   * the one the gain curve in that same tag was authored against, so using
   * the other would evaluate the curve at a white it was not built for. */
  CIccHdrMetadataReader meta;
  bool bHasMeta = meta.Read(pProfile);

  if (info.bHasHagc) {
    const CIccTag *pHagc = icHdrFindTag(pProfile, icSigHeadroomAdaptiveGainCurveTag);
    if (pHagc && pHagc->GetType() == icSigHeadroomAdaptiveGainCurveType) {
      const CIccTagHagc *pHagcTag = (const CIccTagHagc*)pHagc;
      if (pHagcTag->GetMetadata().m_bUnpacked) {
        info.contentReferenceWhite = pHagcTag->GetMetadata().GetReferenceWhite();
        info.bContentReferenceWhiteFromProfile = true;
      }
    }
  }

  if (!info.bContentReferenceWhiteFromProfile && bHasMeta && meta.HasContentReferenceWhite()) {
    info.contentReferenceWhite = meta.GetContentReferenceWhite();
    info.bContentReferenceWhiteFromProfile = true;
  }

  if (bHasMeta)
    info.nHeadroomSource = meta.ResolveDisplayHeadroom(info.displayHeadroom);

  /* Source primaries (9.2.17 / 10.3). Only meaningful with a cicpTag, since
   * the ColourPrimaries field is what selects between the H.273 table and the
   * profile's own matrix columns. */
  if (info.bHasCicp) {
    info.bPrimariesResolved = icGetResolvedPrimaries(pProfile, info.nColourPrimaries,
                                                     info.primaries, &info.bPrimariesFromProfile);
  }

  /* PROPOSAL-ISSUE HDR-02 (design-level) -- 8.10.1 says an HDR Profile's
   * TransferCharacteristics "shall be" 16, 18 or 8 and that other values
   * "shall not be used in an HDR Profile", while NOTE 3 makes the conforming
   * cicpTag the sole thing that distinguishes the class.  The prohibition is
   * therefore unviolatable: a profile with any other value is not an HDR
   * Profile rather than a non-conforming one, so no validator can report it.
   * icHdrProfileIntended below is the non-circular hook that lets a profile
   * plainly authored as HDR, but mistyped, still be reported.
   *
   * Classification. The conforming test is the full set of 8.10.1
   * requirements; the intended test is the non-circular hook described in the
   * header - HDR-specific content without the qualification to carry it. */
  if (info.bRgbMatrixBased && info.bVersion4_5 && info.bHasCicp && info.bTransferIsHdr) {
    info.nClass = icHdrProfileConforming;
  }
  else if (info.bHasHagc || (bHasMeta && meta.HasAnyHdrEntry())) {
    info.nClass = icHdrProfileIntended;
  }

  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
