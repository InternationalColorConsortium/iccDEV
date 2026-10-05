/** @file
    File:       IccHdrProfile.cpp

    Contains:   Implementation of HDR ColorSpace Profile (ICC.1 clause 8.7.1) recognition,
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
// -Initial implementation of HDR ColorSpace Profile recognition and CICP primaries
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
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "IccHdrProfile.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagDict.h"
#include "IccTagHagc.h"
#include "IccHdrToneMap.h"
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

  /* 22: D65, primaries with no source. H.273 V4 Table 2's informative remark
   * for this row is "No corresponding industry specification identified" - it
   * is the one entry in the table that names no standard, so nothing may be
   * cited for it. (An earlier comment here attributed it to EBU Tech. 3213-E,
   * which is wrong twice over: H.273 declines to name a source, and EBU Tech.
   * 3213's primaries are the value 5 row, not this one.) */
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
 *  clause 10.3 (normative body text in the approved amendment).
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
 *  "no cicpTag, not an HDR ColorSpace Profile". The failure is silent and direction
 *  dependent: read the same file eagerly and the answers are right.
 *
 *  So the lookup goes through FindTag(), which loads on demand, via a cast.
 *  The cast is sound in the way that matters: loading a tag populates a cache
 *  and changes nothing about the profile's value, which is exactly the reason
 *  FindTag() exists rather than requiring callers to pre-load. Keeping the
 *  const on the interface is worth this much - a caller has no business being
 *  handed a mutable profile to ask whether it is an HDR ColorSpace Profile.
 *
 *  The load is still a side effect on a const object, so callers that must
 *  not have one test icHdrHeaderAdmitsMembership() first: it needs no tag,
 *  and CIccProfile::CheckHdrProfile() uses it so that validating a profile
 *  whose header rules out membership loads nothing.
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
 *  clause 10.3 (normative body text in the approved amendment).
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

  /* PROPOSAL-ISSUE BALLOT-02 (structural; the fix is two sentences already
   * drafted) -- clause 4.1 of the CICP amendment scopes this to three-component
   * matrix-based profiles: without all three matrix column tags the value 2
   * keeps its original "unknown" meaning and there is nothing to recover.  But
   * clause 4.1 is the *proposal's* scope statement, not part of the amendment:
   * only 4.2 and 4.3 are appended to ICC.1, and the paragraph 4.2 adds to
   * 9.2.17 carries an unqualified "shall" with no scope at all.  We therefore
   * have to cite the proposal here rather than the clause it amends, which is
   * the whole of the finding. */
  icFloatNumber red[3], green[3], blue[3], white[3];
  if (!icHdrGetXyzTag(pProfile, icSigRedMatrixColumnTag, red) ||
      !icHdrGetXyzTag(pProfile, icSigGreenMatrixColumnTag, green) ||
      !icHdrGetXyzTag(pProfile, icSigBlueMatrixColumnTag, blue) ||
      !icHdrGetXyzTag(pProfile, icSigMediaWhitePointTag, white))
    return false;

  /* PROPOSAL-ISSUE BALLOT-01 (design-level; one sentence would close it) -- the
   * amendment's clause 4.3 delegates the chromaticAdaptationTag relationship to
   * ICC.1 9.2.15 / 9.2.36 / Annex F.3 and does not state the direction, but
   * those clauses describe the forward relationship while this derivation needs
   * the inverse.  The wrong direction returns a complete, plausible set of
   * chromaticities, so the error is not self-detecting.
   *
   * Step 1 asks for the tristimulus values "expressed relative to
   * the profile's actual adopted white". The matrix column tags are encoded
   * relative to the *PCS* adopted white (D50); the chromaticAdaptationTag is
   * the matrix that took them there, so recovering the profile's actual
   * adopted white means applying its inverse. NOTE 1 covers the other case
   * (NOTE 2 in the ballot text, renumbered on approval):
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
bool icGetResolvedPrimaries(const CIccProfile * /* pProfile */,
                            icUInt8Number nColourPrimaries,
                            icCicpPrimaries &primaries)
{
  /* ColourPrimaries 2 resolves nothing.  8.7.1.1 puts the chromaticities and
   * the white point in the cicpType custom chromaticity extension of 10.3,
   * which this build cannot read, and the matrix-column recovery that used to
   * answer here has no tags to read in a ColorSpace profile.  See
   * icCicpPrimariesUnspecified.
   *
   * pProfile is where the extension will be read from once its wire format is
   * known, which is why it stays in the signature. */
  if (nColourPrimaries == icCicpPrimariesUnspecified)
    return false;

  return icGetCicpPrimaries(nColourPrimaries, primaries);
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
 * HDR Image metadata (clause 8.7.1.4)
 *
 * The four HDR Image entries are taken from the ICC dictType Metadata Registry
 * (HDR Image category, registered by Apple Inc. 2025-06-04), which 8.7.1.4 names
 * as authoritative: CRWL, CLL, MDCV and CCV, with the field lists each entry's
 * Value column gives.  Those are read facts, not reconstructions.
 *
 * THE HDR DISPLAY ENTRIES ARE NOT READ.  DCV, DRWL and DERH were read here
 * under clause 8.10.5, which the 23-09-2026 revision deletes: "Physical
 * display characterization (peak luminance, extended-range headroom, and
 * similar measurement-derived properties of a specific display) is
 * intentionally out of scope for the HDR ColorSpace Profile sub-class defined
 * by this amendment, which characterizes a colour encoding rather than a
 * display."  Nothing in 8.7.1 consumes a display headroom, so nothing here
 * derives one.
 *
 * PROPOSAL-ISSUE HDR-11 is WITHDRAWN with that clause.  It recorded that the
 * three entries had a registration document (owner ICC, change date
 * 2026-06-24) but were still absent from the registry itself, so their shapes
 * had to be reconstructed from 8.10.5's prose against the registered sibling
 * MDCV.  With no clause to give them meaning there is nothing left to rule
 * on.  The registration document is unaffected by this amendment; if a future
 * one gives the entries a clause again, this is the section that reads them.
 * ===========================================================================
 */

/* Entry names, all as the registrations give them. */
static const char *kIccHdrKeyCrwl = "CRWL";   /* Content HDR Reference White Luminance */
static const char *kIccHdrKeyCll  = "CLL";    /* Content Light Level */
static const char *kIccHdrKeyMdcv = "MDCV";   /* Mastering Display Colour Volume */
static const char *kIccHdrKeyCcv  = "CCV";    /* Content Colour Volume */

/* The registrations also specify a Display name element for each of these seven
 * keys - the human-readable string an authoring path puts in a dictType
 * record's display-name mluc, and the label a report would use so a reader can
 * find the entry in the registry.  They were transcribed here in 446564a6 and
 * are NOT kept, because nothing in this build writes a dictType record or
 * labels one: seven string constants with no consumer are dead weight that
 * -Wunused-variable correctly flags, and ci-pr-gcc15 builds with -Werror.
 *
 * They are not lost.  The authoritative source is the live dictType Metadata
 * Registry, with the HDR Display half additionally in the registration document
 * held at docs/notes/hdr-proposals/HDR.Display.Metadata.DictType.Entries.ICC.docx.
 * Transcribe them again when there is a caller - and transcribe them, do not
 * recall them: they are literal registered strings, American spelling included
 * ("Color", not "Colour"), not descriptions to be localised into house style.
 * That spelling was verified against the live registry when it was first taken,
 * and getting it wrong would produce a record that does not match the registry
 * it claims to follow. */

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
static bool icHdrIsNumberSeparator(char c)
{
  return c == ' ' || c == '\t' || c == ',' || c == ';' || c == '\r' || c == '\n';
}

static bool icHdrParseNumbers(const std::string &s, double *pVals, int nWanted)
{
  size_t pos = 0;
  const size_t len = s.size();
  int n = 0;

  while (pos < len) {
    while (pos < len && icHdrIsNumberSeparator(s[pos]))
      pos++;

    if (pos >= len)
      break;

    size_t end = pos;
    while (end < len && !icHdrIsNumberSeparator(s[end]))
      end++;

    if (n >= nWanted)
      return false;

    /* Each separator-delimited field has to be ONE number, consumed whole.
     * strtod stopped at the first character it could not use and the next
     * field started there, so "1000.0.005 9" read as the three numbers 1000.0,
     * 0.005 and 9.  And strtod follows the C locale the host last set: a host
     * that called setlocale(LC_ALL, "") under a comma-decimal locale read
     * "203.0" as 203 followed by an unparseable ".0", silently turning every
     * stated luminance into "unparsed" and the resolved values into defaults.
     * A registry value is not locale text, so it is parsed in the classic
     * locale whatever the host has chosen. */
    std::istringstream field(s.substr(pos, end - pos));
    field.imbue(std::locale::classic());

    double v = 0.0;

    if (!(field >> v))
      return false;

    if (field.peek() != std::char_traits<char>::eof())
      return false;

    /* A non-finite luminance would poison every derivation downstream, so it
     * is refused here rather than propagated. */
    if (!std::isfinite(v) || v > 1.0e30 || v < -1.0e30)
      return false;

    pVals[n++] = v;
    pos = end;
  }

  return n == nWanted;
}

/**
 ****************************************************************************
 * Name: icHdrToPositiveFloat
 *
 * Purpose: Narrow a parsed luminance or ratio that has to be strictly positive
 *  into icFloatNumber, refusing what does not survive the narrowing.
 *
 *  "> 0.0" on the double is not enough.  1e-50 is positive as a double and 0.0
 *  as a float, so it used to be accepted as stated and then divided by; and a
 *  subnormal float such as 1e-40 is nonzero but turns every ratio it divides
 *  into an infinity.  The smallest normal float is the floor.
 *****************************************************************************
 */
static bool icHdrToPositiveFloat(double v, icFloatNumber &out)
{
  if (!(v >= (double)(std::numeric_limits<icFloatNumber>::min)()) ||
      !(v <= (double)(std::numeric_limits<icFloatNumber>::max)()))
    return false;

  out = (icFloatNumber)v;
  return true;
}

/**
 ****************************************************************************
 * Name: icHdrRatio
 *
 * Purpose: num / den as a finite, non-negative icFloatNumber, or false.
 *
 *  Every headroom this file resolves is a luminance over a reference white,
 *  and both come from the file.  A ratio that does not fit an icFloatNumber is
 *  not a large headroom - it is no headroom anyone can render to - so the rule
 *  that produced it yields no value rather than an infinity.
 *****************************************************************************
 */
static bool icHdrRatio(double num, double den, icFloatNumber &out)
{
  if (!(den > 0.0))
    return false;

  double r = num / den;

  if (!(r >= 0.0) || !(r <= (double)(std::numeric_limits<icFloatNumber>::max)()))
    return false;

  out = (icFloatNumber)r;
  return true;
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
 * Name: icHdrParseRegistryVolume
 *
 * Purpose: Parse an HDR Image value of the form "<luminance>{n} <primaries>":
 *  n floating point fields followed by an 8-bit ITU-T H.273 ColourPrimaries
 *  code, which is the shape the ICC dictType Metadata Registry gives these
 *  entries.
 *
 *    MDCV  max luminance, min luminance, primaries
 *    CLL   max light level, average light level, primaries
 *
 *  DCV was read on the same shape while clause 8.10.5 existed.  The
 *  23-09-2026 revision deletes that clause, so this helper now serves MDCV
 *  and CLL only - both registered, both read on the shape the registry gives
 *  rather than on a reconstruction.
 *
 *  Two registry semantics are carried through rather than rejected. A
 *  luminance of 0.0 means the author did not know the value, so it is stored
 *  as given and callers that divide by it test it first. A primaries code of 2
 *  means the primaries are not expressible in H.273 and are instead given by
 *  the tags of the profile containing the metadata, so no chromaticities are
 *  resolved for it and the caller is told so.
 *****************************************************************************
 */
static bool icHdrParseRegistryVolume(const CIccProfile *pProfile, const std::string &s,
                                     int nLums, double *pLums,
                                     icCicpPrimaries &primaries, bool &bPrimariesResolved,
                                     bool bCode2FromProfile)
{
  double v[4];
  int i;

  bPrimariesResolved = false;
  memset(&primaries, 0, sizeof(primaries));

  if (nLums < 1 || nLums > 3)
    return false;

  if (!icHdrParseNumbers(s, v, nLums + 1))
    return false;

  for (i = 0; i < nLums; i++) {
    /* Negated so a NaN, which every ordered comparison accepts, is refused. */
    if (!(v[i] >= 0.0))
      return false;
    pLums[i] = v[i];
  }

  /* The last field is a code point, not a chromaticity: it has to be a whole
   * number in 8-bit range or the value is not of this shape at all. */
  if (!(v[nLums] >= 0.0) || v[nLums] > 255.0 ||
      v[nLums] != (double)(icUInt8Number)v[nLums])
    return false;

  icUInt8Number nCode = (icUInt8Number)v[nLums];

  if (nCode == icCicpPrimariesUnspecified) {
    /* NOT FOR EVERY ENTRY - hence bCode2FromProfile.  The registry gives code 2
     * two different meanings.  For MDCV: "At this point the primaries value of 2
     * in the MDCV is reserved for future use."  A reserved value has no meaning
     * to resolve, so MDCV passes false and code 2 stays unresolved.  This helper
     * used to apply the CLL/CCV rule below to all three of its callers, because
     * sharing the parser generalised the rule; the comment that follows cited
     * only CLL and CCV and never mentioned MDCV.
     *
     * The registry is explicit about this code, and it does NOT mean "unknown"
     * here.  Its CLL and CCV entries say a primaries value of 2 "means that
     * the primaries are defined by tags required by [a] three component
     * matrix-based display profile containing this metadata", and the HDR
     * Display registration of 2026-06-24 says of DCV that "a primaries value
     * of 2 has the same meaning as in the cicpTag" - which, under the CICP
     * Unspecified-Primaries amendment, is the same recovery.  So it resolves
     * against the containing profile, exactly as icGetResolvedPrimaries()
     * does for the cicpTag itself.
     *
     * PROPOSAL-ISSUE HDR-18: in a profile authored to the 29-08-2026 revision
     * this usually cannot be satisfied.  8.7.1.1 removes the matrix column tags
     * from an HDR ColorSpace Profile unless the cicpTag's own ColourPrimaries is 2, so a
     * metadata entry carrying primaries 2 has nothing to resolve against in
     * every other conforming HDR ColorSpace Profile - and these entries are Optional, so
     * nothing prevents one being written.  icGetProfilePrimaries() returns
     * false there, which leaves bPrimariesResolved false: the same answer as
     * before this branch existed, now for a stated reason rather than by
     * omission. */
    bPrimariesResolved = bCode2FromProfile && icGetProfilePrimaries(pProfile, primaries);
  }
  else {
    bPrimariesResolved = icGetCicpPrimaries(nCode, primaries);
  }

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
  m_bCllPrimariesResolved = m_bMdcvPrimariesResolved = false;
  m_bUnparsed = false;

  m_crwl = (icFloatNumber)icHdrDefaultContentReferenceWhite;
  m_maxCll = m_maxFall = 0.0f;
  m_mdcvMaxLuminance = m_mdcvMinLuminance = 0.0f;

  memset(&m_cllPrimaries, 0, sizeof(m_cllPrimaries));
  memset(&m_mdcvPrimaries, 0, sizeof(m_mdcvPrimaries));
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::Read
 *
 * Purpose: Read the HDR Image entries out of a profile's metadataTag.
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

  /* --- HDR Image (8.7.1.4) --- */

  /* A single decimal luminance in cd/m^2 - DEFINED, not assumed.  This used to
   * say ASSUMED, because 8.7.1.4 NOTE 11 says the encoding is "defined in
   * accordance with ISO 22028-5", which this build does not have.  But the
   * amendment makes the dictType Metadata Registry authoritative for entry
   * encodings, the registry gives CRWL's Value as "Floating point number",
   * and the dictType proposal makes that Value column the entry's mandatory
   * format description.  The registry cites ISO 22028-5 for the QUANTITY -
   * what HDR reference white luminance means - not for its encoding; that
   * mismatch is PROPOSAL-ISSUE HDR-16. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCrwl, value, bParseable)) {
    /* Stricter than a positive float: CRWL divides the transfer's own peak
     * (10 000 cd/m^2 for PQ) and every metadata luminance, so a value those
     * divisions cannot survive is refused here and reported unparsed.  Taking
     * it as stated gave inf and NaN PCS from a profile Validate() called
     * valid; letting it underflow to 0.0 left HasContentReferenceWhite() true
     * while the transfer quietly substituted 203. */
    icFloatNumber crwl = 0.0f, peak = 0.0f;
    if (bParseable && icHdrParseNumbers(value, v, 1) && icHdrToPositiveFloat(v[0], crwl) &&
        icHdrRatio(icPqPeakLuminance, (double)crwl, peak)) {
      m_crwl = crwl;
      m_bHasCrwl = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  /* CLL as registered: maximum and average light level, computed per
   * CTA-861.3-A Annex A, then the primaries code. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCll, value, bParseable)) {
    double lums[2];
    if (bParseable && icHdrParseRegistryVolume(pProfile, value, 2, lums, m_cllPrimaries,
                                               m_bCllPrimariesResolved, true)) {
      m_maxCll = (icFloatNumber)lums[0];
      m_maxFall = (icFloatNumber)lums[1];
      m_bHasCll = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  /* MDCV as registered: maximum and minimum luminance, then the primaries. */
  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyMdcv, value, bParseable)) {
    double lums[2];
    if (bParseable && icHdrParseRegistryVolume(pProfile, value, 2, lums, m_mdcvPrimaries,
                                               m_bMdcvPrimariesResolved, false)) {
      m_mdcvMaxLuminance = (icFloatNumber)lums[0];
      m_mdcvMinLuminance = (icFloatNumber)lums[1];
      m_bHasMdcv = true;
    }
    else {
      m_bUnparsed = true;
    }
  }

  bParseable = false;
  if (icHdrGetDictValue(pDict, kIccHdrKeyCcv, value, bParseable)) {
    m_bHasCcv = true;
    if (bParseable)
      m_ccvRaw = value;
    else
      m_bUnparsed = true;
  }

  /* DCV, DRWL and DERH are deliberately NOT read.  Their clause, 8.10.5, is
   * deleted by the 23-09-2026 revision; see the section banner above.  A
   * profile that still carries them is not thereby defective and draws no
   * diagnostic - a metadataTag may carry any entry - but nothing in 8.7.1
   * gives them a meaning, so this reader does not claim one.  They also no
   * longer count towards HasAnyHdrEntry(): an HDR Display entry on its own is
   * no longer a statement of HDR intent under any clause. */

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
  /* "parsed or not", as the header says.  m_bUnparsed is only ever set for a
   * recognised key that was present, so it counts as carrying the entry: a
   * profile whose only HDR entry is a mistyped CRWL still states HDR intent,
   * and leaving it out classified that profile as carrying nothing at all. */
  return m_bHasCrwl || m_bHasCll || m_bHasMdcv || m_bHasCcv || m_bUnparsed;
}

/**
 ****************************************************************************
 * Name: CIccHdrMetadataReader::ResolveContentHeadroom
 *
 * Purpose: Apply the priority order of clause 8.7.1.4 a) to c), which the
 *  clause states for the Linear (8) transfer only.
 *
 *  Unlike 8.10.5's display order this one always produces a value: c) has no
 *  metadata precondition, so a Linear profile carrying no HDR Image entry at
 *  all still gets 1000 cd/m^2 / CRWL.  That is the point of the order - the
 *  Linear transfer establishes no peak luminance, so without an assumed one
 *  the tone-mapping operator has no scale to work in.
 *
 *  The clause calls the order "recommended" and then says the operator
 *  "shall select that value according to" it.  The two modal verbs pull in
 *  opposite directions and the clause does not say which governs; this
 *  implementation follows the order exactly, which satisfies both readings.
 *
 * Args:
 *  headroom = receives the resolved value when the return is not
 *             icHdrContentHeadroomNone
 *  crwl = the content HDR reference white to divide by, resolved by the
 *         caller (see the header for why it is not read from this class)
 *
 * Return:
 *  the rule that produced the value
 *****************************************************************************
 */
icHdrContentHeadroomSource CIccHdrMetadataReader::ResolveContentHeadroom(icFloatNumber &headroom,
                                                                        icFloatNumber crwl) const
{
  /* Every branch divides by CRWL, so a non-positive one is checked once here
   * rather than three times below.  It cannot arise from this class - the
   * parser rejects a non-positive CRWL and the default is 203 - but the value
   * is the caller's, and an HAGC tag's reference white reaches this function
   * without passing through that parser. */
  if (!(crwl > 0.0f))
    return icHdrContentHeadroomNone;

  /* The dictType Metadata Registry defines a CLL or MDCV luminance of 0.0 as
   * "unknown".  An unknown maximum is not a peak, so it does not select its
   * rule; resolution falls through exactly as if the entry were absent.
   * Taking it at face value gave Hcontent = 0, which is below every target
   * headroom and so switched the transform's target-volume clamp off.
   *
   * A rule that IS selected but whose quotient is not a finite float yields no
   * value rather than falling through: the order is a precedence, so a later
   * rule does not become authoritative because an earlier one overflowed.
   * CLL 1e30 over a CRWL of 1e-34 is not a headroom, and falling through to
   * MDCV would make the mastering display authoritative because the content
   * overflowed. */

  /* a) CLL maximum content light level */
  if (m_bHasCll && m_maxCll > 0.0f) {
    return icHdrRatio(m_maxCll, crwl, headroom) ? icHdrContentHeadroomCll
                                               : icHdrContentHeadroomNone;
  }

  /* b) MDCV maximum luminance, when CLL is absent.  The order is a real
   * precedence and not a fallback chain: CLL measures the content, MDCV
   * measures the display it was mastered on, so a profile carrying both is
   * answered from the content and the mastering peak is not consulted. */
  if (m_bHasMdcv && m_mdcvMaxLuminance > 0.0f) {
    return icHdrRatio(m_mdcvMaxLuminance, crwl, headroom) ? icHdrContentHeadroomMdcv
                                                         : icHdrContentHeadroomNone;
  }

  /* c) the assumed typical mastering peak */
  return icHdrRatio(icHdrDefaultMasteringPeak, crwl, headroom) ? icHdrContentHeadroomDefault
                                                              : icHdrContentHeadroomNone;
}

/**
 ****************************************************************************
 * Name: icHdrIsRgbColorSpace
 *
 * Purpose: The structural half of 8.7.1.1's membership test, as 8.7.1.5
 *  states it: "The data colour space in the profile header shall be RGB.  The
 *  profile class shall be ColorSpace ('spac')."
 *
 *  This is the whole of it.  Under the previous revision the parent class was
 *  the three-component matrix-based Input or Display profile, and the clause
 *  named that parent by its tag set and then removed six of those tags again,
 *  so the test had to be assembled from what survived.  The ColorSpace
 *  profile of 8.7 needs no such reconstruction: an HDR ColorSpace Profile
 *  "shall be a ColorSpace profile (see 8.7) and shall therefore contain all
 *  tags required of that profile class", with nothing taken away.  The
 *  required-tag rules of 8.7 are CIccProfile::CheckRequiredTags()'s business,
 *  as they are for any ColorSpace profile.
 ****************************************************************************
 */
static bool icHdrIsRgbColorSpace(const CIccProfile *pProfile)
{
  return pProfile->m_Header.colorSpace == icSigRgbData &&
         pProfile->m_Header.deviceClass == icSigColorSpaceClass;
}

/**
 ****************************************************************************
 * Name: icHdrIsVersion4
 *
 * Purpose: Keep ICC.2 profiles out of an ICC.1 clause.
 *
 *  THE LOWER BOUND IS GONE, THE UPPER BOUND IS NOT, and the two came from
 *  different places.  The previous revision required profileVersionField
 *  4.5.0.0, and 4.7 of the 23-09-2026 revision withdraws that: the amendment
 *  is a minor-version change "without requiring any change to the
 *  profileVersionField".  So a 4.4 or a 4.0 ColorSpace profile carrying a
 *  conforming cicpTag is an HDR ColorSpace Profile, and Testing/HDR pins that
 *  with HdrVersion44.
 *
 *  The upper bound was never the amendment's; it is what keeps an ICC.1
 *  clause from being applied to an ICC.2 profile.  This amendment amends
 *  ICC.1:2022 - its scope sentence, its normative references and every clause
 *  it touches are ICC.1's - and 8.7, the ColorSpace profile it builds the
 *  sub-class on, is ICC.1's 8.7.  ICC.2 has its own profile model, its own
 *  required-tag rules and its own notion of a ColorSpace-class profile, and
 *  nothing in this amendment reaches into it.  A v5 profile that happens to
 *  declare 'spac' and carry a PQ cicpTag is therefore not an HDR ColorSpace
 *  Profile under 8.7.1, and saying otherwise would apply a chain the v5 tag
 *  model does not define to a profile whose author never asked for it.
 *
 *  This is a RULING, not a reading: the revision states no version condition
 *  at either end (PROPOSAL-ISSUE HDR-24).  It is the conservative one - it narrows what this build
 *  claims rather than widening it - and it is the one that keeps the eight
 *  v5 BT2100 fixtures in Testing/HDR out of the sub-class, which is what they
 *  were added to check.  If the ICC extends the sub-class to ICC.2, this
 *  predicate is the single place that changes.
 ****************************************************************************
 */
static bool icHdrIsVersion4(const CIccProfile *pProfile)
{
  return pProfile->m_Header.version < icVersionNumberV5;
}

/**
 ****************************************************************************
 * Name: icHdrHeaderAdmitsMembership
 *
 * Purpose: The clause 8.7.1.1 membership conditions that need no tag.
 *
 *  Composed from the same two predicates icGetHdrProfileInfo() records, so
 *  the early answer and the full classification cannot drift apart.  It
 *  exists because classifying loads tags (see icHdrFindTag()), and a const
 *  caller such as CIccProfile::Validate() should not load anything for a
 *  profile its header has already ruled out.
 *
 * Args:
 *  pProfile = the profile to test; may be NULL
 *
 * Return:
 *  true when version, class and colour space all permit membership.
 ****************************************************************************
 */
bool icHdrHeaderAdmitsMembership(const CIccProfile *pProfile)
{
  if (!pProfile)
    return false;

  return icHdrIsRgbColorSpace(pProfile) && icHdrIsVersion4(pProfile) &&
         pProfile->m_Header.pcs == icSigXYZData;
}

/**
 ****************************************************************************
 * Name: icBuildRgbToXyzMatrix
 *
 * Purpose: The columns of an RGB to XYZ matrix are the primaries' own
 *  tristimulus values, scaled so that RGB = (1, 1, 1) is the white point.
 *
 *  Chromaticities give directions only: (x, y) fixes X : Y : Z but not the
 *  magnitude, so the three columns are known up to three unknown scale
 *  factors.  Requiring that they sum to the white point's XYZ is what
 *  determines them, which is one 3x3 solve.
 *
 * Args:
 *  primaries = the four chromaticities
 *  matrix = receives nine elements, row major
 *
 * Return:
 *  false when a chromaticity has y = 0, or the primaries are collinear
 ****************************************************************************
 */
bool icBuildRgbToXyzMatrix(const icCicpPrimaries &primaries, icFloatNumber *matrix)
{
  if (!matrix)
    return false;

  const icFloatNumber ys[4] = { primaries.yRed, primaries.yGreen, primaries.yBlue, primaries.yWhite };
  const icFloatNumber xs[4] = { primaries.xRed, primaries.xGreen, primaries.xBlue, primaries.xWhite };
  int i;

  /* A zero y is not a rounding problem to guard against: it is a chromaticity
   * with no luminance, which names no colour at all. */
  for (i = 0; i < 4; i++) {
    if (!(ys[i] > 0.0))
      return false;
  }

  /* Each primary at unit luminance.  Column i of this is the direction of
   * primary i in XYZ. */
  icFloatNumber prim[9];
  for (i = 0; i < 3; i++) {
    prim[i]     = (icFloatNumber)((double)xs[i] / (double)ys[i]);
    prim[3 + i] = 1.0;
    prim[6 + i] = (icFloatNumber)((1.0 - (double)xs[i] - (double)ys[i]) / (double)ys[i]);
  }

  /* The white at Y = 1, which is the vector the three columns must sum to. */
  icFloatNumber white[3];
  white[0] = (icFloatNumber)((double)xs[3] / (double)ys[3]);
  white[1] = 1.0;
  white[2] = (icFloatNumber)((1.0 - (double)xs[3] - (double)ys[3]) / (double)ys[3]);

  icFloatNumber inv[9];
  memcpy(inv, prim, sizeof(inv));

  if (!icMatrixInvert3x3(inv))
    return false;

  /* scale = prim^-1 . white */
  icFloatNumber scale[3];
  for (i = 0; i < 3; i++) {
    scale[i] = (icFloatNumber)((double)inv[i * 3 + 0] * (double)white[0] +
                               (double)inv[i * 3 + 1] * (double)white[1] +
                               (double)inv[i * 3 + 2] * (double)white[2]);
  }

  for (i = 0; i < 3; i++) {
    matrix[0 + i] = (icFloatNumber)((double)prim[0 + i] * (double)scale[i]);
    matrix[3 + i] = (icFloatNumber)((double)prim[3 + i] * (double)scale[i]);
    matrix[6 + i] = (icFloatNumber)((double)prim[6 + i] * (double)scale[i]);
  }

  return true;
}

/**
 ****************************************************************************
 * Name: icBuildPrimariesConversionMatrix
 *
 * Purpose: Linear RGB in one set of primaries to linear RGB in another,
 *  through XYZ, chromatically adapting between the two white points.
 *
 *  The adaptation is ICC.1:2022 Annex E.3's linearized Bradford transform,
 *  which E.2 composes exactly this way and which 9.2.15 recommends for ICC
 *  profiles.  Doing it in cone space rather than scaling XYZ directly is the
 *  whole point: a von Kries scaling of XYZ moves hues, and the difference
 *  between the two is visible at a D65-to-D50 adaptation.
 *
 * Args:
 *  src, dst = the two primary sets
 *  matrix = receives nine elements, row major
 *
 * Return:
 *  false when either set is degenerate or a matrix in the chain is singular
 ****************************************************************************
 */
bool icBuildPrimariesConversionMatrix(const icCicpPrimaries &src,
                                      const icCicpPrimaries &dst,
                                      icFloatNumber *matrix)
{
  if (!matrix)
    return false;

  /* ICC.1:2022 Annex E.3, Equation (E.1). */
  static const icFloatNumber kBradford[9] = {
     (icFloatNumber) 0.8951, (icFloatNumber) 0.2664, (icFloatNumber)-0.1614,
     (icFloatNumber)-0.7502, (icFloatNumber) 1.7135, (icFloatNumber) 0.0367,
     (icFloatNumber) 0.0389, (icFloatNumber)-0.0685, (icFloatNumber) 1.0296
  };

  icFloatNumber mSrc[9], mDst[9];

  if (!icBuildRgbToXyzMatrix(src, mSrc) || !icBuildRgbToXyzMatrix(dst, mDst))
    return false;

  /* Both whites at Y = 1, then into cone space. */
  if (!(src.yWhite > 0.0) || !(dst.yWhite > 0.0))
    return false;

  double wSrc[3], wDst[3];
  wSrc[0] = (double)src.xWhite / (double)src.yWhite;
  wSrc[1] = 1.0;
  wSrc[2] = (1.0 - (double)src.xWhite - (double)src.yWhite) / (double)src.yWhite;
  wDst[0] = (double)dst.xWhite / (double)dst.yWhite;
  wDst[1] = 1.0;
  wDst[2] = (1.0 - (double)dst.xWhite - (double)dst.yWhite) / (double)dst.yWhite;

  double coneSrc[3], coneDst[3];
  int i;

  for (i = 0; i < 3; i++) {
    coneSrc[i] = (double)kBradford[i * 3 + 0] * wSrc[0] +
                 (double)kBradford[i * 3 + 1] * wSrc[1] +
                 (double)kBradford[i * 3 + 2] * wSrc[2];
    coneDst[i] = (double)kBradford[i * 3 + 0] * wDst[0] +
                 (double)kBradford[i * 3 + 1] * wDst[1] +
                 (double)kBradford[i * 3 + 2] * wDst[2];

    /* A cone response of zero would need a white with no response in one of
     * the three bands, which no real chromaticity has; refuse rather than
     * divide. */
    if (!(coneSrc[i] > 0.0))
      return false;
  }

  /* M_adapt = M_BFD^-1 . diag(cone_dst / cone_src) . M_BFD  (Equation E.2). */
  icFloatNumber bfdInv[9];
  memcpy(bfdInv, kBradford, sizeof(bfdInv));

  if (!icMatrixInvert3x3(bfdInv))
    return false;

  icFloatNumber scaled[9];
  for (i = 0; i < 3; i++) {
    double r = coneDst[i] / coneSrc[i];
    scaled[i * 3 + 0] = (icFloatNumber)(r * (double)kBradford[i * 3 + 0]);
    scaled[i * 3 + 1] = (icFloatNumber)(r * (double)kBradford[i * 3 + 1]);
    scaled[i * 3 + 2] = (icFloatNumber)(r * (double)kBradford[i * 3 + 2]);
  }

  icFloatNumber adapt[9];
  icMatrixMultiply3x3(adapt, bfdInv, scaled);

  /* M = M_dst^-1 . M_adapt . M_src */
  icFloatNumber dstInv[9];
  memcpy(dstInv, mDst, sizeof(dstInv));

  if (!icMatrixInvert3x3(dstInv))
    return false;

  icFloatNumber tmp[9];
  icMatrixMultiply3x3(tmp, adapt, mSrc);
  icMatrixMultiply3x3(matrix, dstInv, tmp);

  return true;
}

/**
 ****************************************************************************
 * Name: icHagcGetGainApplicationPrimaries
 *
 * Purpose: Resolve a HAGC Gain Curve Chromaticities Mode to chromaticities.
 *
 *  See the header for PROPOSAL-ISSUE HAGC-09 and why mode 0 resolves to
 *  BT.709 rather than to the H.273 code point the proposal names beside it.
 *
 * Args:
 *  nMode = the mode, 0 to 3
 *  pCustom = eight values [xR yR xG yG xB yB xW yW], read only for mode 3
 *  primaries = receives the result
 *
 * Return:
 *  false for an unknown mode, or mode 3 with no values
 ****************************************************************************
 */
bool icHagcGetGainApplicationPrimaries(icUInt8Number nMode, const icFloatNumber *pCustom,
                                       icCicpPrimaries &primaries)
{
  switch (nMode) {
    case 0:
      return icGetCicpPrimaries(1, primaries);   /* BT.709-6 */

    case 1:
      return icGetCicpPrimaries(12, primaries);  /* SMPTE EG 432-1, Display P3 */

    case 2:
      return icGetCicpPrimaries(9, primaries);   /* BT.2020-2 */

    case 3:
      if (!pCustom)
        return false;

      primaries.xRed   = pCustom[0];
      primaries.yRed   = pCustom[1];
      primaries.xGreen = pCustom[2];
      primaries.yGreen = pCustom[3];
      primaries.xBlue  = pCustom[4];
      primaries.yBlue  = pCustom[5];
      primaries.xWhite = pCustom[6];
      primaries.yWhite = pCustom[7];
      return true;

    default:
      return false;
  }
}

/**
 ****************************************************************************
 * Name: icHdrGetChad
 *
 * Purpose: Read a profile's chromaticAdaptationTag into a row-major 3x3.
 *
 * Return:
 *  true when the tag is present and well formed; false when it is absent,
 *  which is not an error - the caller then treats the profile's actual
 *  adopted white as the PCS adopted white
 ****************************************************************************
 */
static bool icHdrGetChad(const CIccProfile *pProfile, icFloatNumber *chad)
{
  const CIccTag *pChad = icHdrFindTag(pProfile, icSigChromaticAdaptationTag);

  if (!pChad || pChad->GetType() != icSigS15Fixed16ArrayType)
    return false;

  const CIccTagS15Fixed16 *pChadTag = (const CIccTagS15Fixed16*)pChad;

  if (pChadTag->GetSize() < 9)
    return false;

  /* Read through GetValues() rather than operator[], for the reasons
   * icGetProfilePrimaries() gives at its own chad read: it is the const
   * accessor, it converts from s15Fixed16 itself, and it bounds-checks. */
  return pChadTag->GetValues(chad, 0, 9);
}

/**
 ****************************************************************************
 * Name: icHdrHasMalformedChad
 *
 * Purpose: A chromaticAdaptationTag that is present but that icHdrGetChad()
 *  cannot read.  See the header for why this is kept apart from absence.
 *****************************************************************************
 */
bool icHdrHasMalformedChad(const CIccProfile *pProfile)
{
  if (!pProfile || !icHdrFindTag(pProfile, icSigChromaticAdaptationTag))
    return false;

  icFloatNumber chad[9];
  return !icHdrGetChad(pProfile, chad);
}

/**
 ****************************************************************************
 * Name: icBuildHdrForwardMatrix
 *
 * Purpose: Clause 8.7.1.2 c)'s RGB-to-PCSXYZ matrix, built from the cicpTag's
 *  primaries, with the chromatic adaptation NOTE 2 leaves out.
 *
 *  READ THE HEADER.  The three-step order there is a ruling against defective
 *  normative text (PROPOSAL-ISSUE HDR-07), not a transcription of it.
 *
 * Args:
 *  pProfile = the profile
 *  nColourPrimaries = the cicpTag's ColourPrimaries field
 *  matrix = receives nine elements, row major
 *
 * Return:
 *  false when the value names no chromaticities, the mediaWhitePointTag is
 *  absent, or the chromaticAdaptationTag is singular
 ****************************************************************************
 */
bool icBuildHdrForwardMatrix(const CIccProfile *pProfile, icUInt8Number nColourPrimaries,
                             icFloatNumber *matrix)
{
  if (!pProfile || !matrix)
    return false;

  icCicpPrimaries primaries;

  /* Value 2 is deliberately not handled here: 9.2.17 sends it to the profile's
   * own matrix column tags, which are then the matrix and need no building. */
  if (!icGetCicpPrimaries(nColourPrimaries, primaries))
    return false;

  icFloatNumber white[3];

  if (!icHdrGetXyzTag(pProfile, icSigMediaWhitePointTag, white))
    return false;

  /* Absent and malformed used to be the same false from icHdrGetChad(), so a
   * chad of the wrong type or with fewer than nine values built the UNADAPTED
   * matrix - 11% high in X for a D65 profile - and every caller rendered with
   * it.  icGetProfilePrimaries() in this file already refuses that case. */
  if (icHdrHasMalformedChad(pProfile))
    return false;

  icFloatNumber chad[9], chadInv[9];
  bool bHasChad = icHdrGetChad(pProfile, chad);

  if (bHasChad) {
    memcpy(chadInv, chad, sizeof(chadInv));

    if (!icMatrixInvert3x3(chadInv))
      return false;

    /* Step 1: the profile's ACTUAL adopted white.  The mediaWhitePointTag is
     * encoded relative to the PCS adopted white and the chad is the matrix
     * that took it there, so the inverse recovers it - BALLOT-01's ruling,
     * and getting the direction wrong here moves the white by about 0,035 in
     * x while still producing a plausible matrix. */
    double w[3];
    int i;

    for (i = 0; i < 3; i++) {
      w[i] = (double)chadInv[i * 3 + 0] * (double)white[0] +
             (double)chadInv[i * 3 + 1] * (double)white[1] +
             (double)chadInv[i * 3 + 2] * (double)white[2];
    }

    white[0] = (icFloatNumber)w[0];
    white[1] = (icFloatNumber)w[1];
    white[2] = (icFloatNumber)w[2];
  }

  /* The white as a chromaticity, which is what the primary matrix wants.  A
   * white with no tristimulus sum is not a white. */
  double sum = (double)white[0] + (double)white[1] + (double)white[2];

  if (!(sum > 0.0))
    return false;

  primaries.xWhite = (icFloatNumber)((double)white[0] / sum);
  primaries.yWhite = (icFloatNumber)((double)white[1] / sum);

  /* Step 2: the primary matrix at the profile's actual adopted white. */
  icFloatNumber actual[9];

  if (!icBuildRgbToXyzMatrix(primaries, actual))
    return false;

  if (!bHasChad) {
    /* No chad means the actual adopted white IS the PCS adopted white, so
     * there is nothing to adapt and step 3 is the identity. */
    memcpy(matrix, actual, sizeof(icFloatNumber) * 9);
    return true;
  }

  /* Step 3: M_PCS = [chad] . M_actual.  This is the operation NOTE 2 omits,
   * and it is why the columns of the result do not sit at the H.273
   * chromaticities. */
  icMatrixMultiply3x3(matrix, chad, actual);
  return true;
}

/**
 ****************************************************************************
 * Name: icHdrSelectForwardMatrix
 *
 * Purpose: The one decision about where an HDR rendering's RGB-to-PCSXYZ
 *  matrix comes from - see the header for why there has to be only one.
 ****************************************************************************
 */
icHdrMatrixSource icHdrSelectForwardMatrix(const CIccProfile *pProfile,
                                           icUInt8Number nColourPrimaries,
                                           icFloatNumber *matrix)
{
  if (!pProfile || !matrix)
    return icHdrMatrixUnavailable;

  /* ColourPrimaries 2 needs the cicpType custom chromaticity extension of
   * 10.3, which this build cannot read - see icCicpPrimariesUnspecified.  It
   * used to route to the matrix column tags; a ColorSpace profile has none,
   * and 8.7.1.1 NOTE 2 would take the white point from the extension rather
   * than from mediaWhitePointTag even if it did. */
  if (nColourPrimaries == icCicpPrimariesUnspecified)
    return icHdrMatrixNeedsCicpExt;

  if (icHdrHasMalformedChad(pProfile))
    return icHdrMatrixMalformedChad;

  if (icBuildHdrForwardMatrix(pProfile, nColourPrimaries, matrix))
    return icHdrMatrixFromCicp;

  /* No second matrix to fall back to.  The conventional matrix/TRC fallback
   * that stood here served the Input/Display parent class of the previous
   * revision; a ColorSpace profile carries neither matrix column nor TRC
   * tags, so there is nothing to fall back to and nothing to be gained by
   * pretending otherwise. */
  return icHdrMatrixUnavailable;
}

/**
 ****************************************************************************
 * Name: icGetHdrProfileInfo
 *
 * Purpose: Classify a profile against clause 8.7.1 and resolve everything the
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
  info.nContentHeadroomSource = icHdrContentHeadroomNone;

  if (!pProfile)
    return false;

  info.bRgbColorSpace = icHdrIsRgbColorSpace(pProfile);
  info.bVersion4 = icHdrIsVersion4(pProfile);
  info.bPcsXyz = (pProfile->m_Header.pcs == icSigXYZData);

  const CIccTag *pCicp = icHdrFindTag(pProfile, icSigCicpTag);
  if (pCicp && pCicp->GetType() == icSigCicpType) {
    /* GetFields() is not const, so the read goes through a non-const view of a
     * tag this function only ever reads. Casting here rather than taking a
     * non-const profile keeps the constness where it belongs - on the caller,
     * which has no business being handed a mutable profile to ask a question. */
    CIccTagCicp *pCicpTag = (CIccTagCicp*)pCicp;
    icUInt8Number mtx = 0, full = 0;
    pCicpTag->GetFields(info.nColourPrimaries, info.nTransferCharacteristics, mtx, full);
    info.nMatrixCoefficients = mtx;
    /* The field is a flag in a byte: anything non-zero is full range.  Both of
     * these were read into locals and discarded before; see the struct. */
    info.bVideoFullRange = (full != 0);
    info.bHasCicp = true;
    info.bTransferIsHdr = (info.nTransferCharacteristics == icCicpTransferLinear ||
                           info.nTransferCharacteristics == icCicpTransferPQ ||
                           info.nTransferCharacteristics == icCicpTransferHLG);
  }

  info.bHasHagc = pProfile->IsTagPresent(icSigHeadroomAdaptiveGainCurveTag);
  info.bHasAToB0 = pProfile->IsTagPresent(icSigAToB0Tag);
  info.bHasBToA0 = pProfile->IsTagPresent(icSigBToA0Tag);

  /* Content reference white (8.7.1.4). The HAGC tag carries its own HDR
   * reference white, and when both it and a CRWL entry are present they are
   * describing the same quantity; the HAGC value is preferred because it is
   * the one the gain curve in that same tag was authored against, so using
   * the other would evaluate the curve at a white it was not built for.
   *
   * PROPOSAL-ISSUE HDR-10: 8.7.1.4 states no precedence between the two, and
   * its 203 cd/m^2 default is stated twice with different conditions - the
   * Default paragraph fires only when there is neither an HAGC tag nor a CRWL
   * entry, while the Linear priority order restates it as "defaulting to 203
   * cd/m^2 when absent" with no HAGC qualifier.  The order below is this
   * implementation's ruling: HAGC first, then the CRWL entry, then 203.  It
   * is also what the content-headroom block further down divides by; see the
   * HDR-10 marker there for the arithmetic that makes the disagreement
   * visible, and Testing/HDR/HdrLinearHagcWhite.xml for the fixture that
   * pins it. */
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

  /* Content headroom (8.7.1.4).  Gated on Linear because that is the only
   * transfer the priority order is stated for, and because rule c) would
   * otherwise assert a 1000 cd/m^2 peak for a PQ profile whose transfer
   * already fixes one at 10 000.
   *
   * The divisor is info.contentReferenceWhite, resolved just above, not the
   * reader's own CRWL.
   *
   * PROPOSAL-ISSUE HDR-10: 8.7.1.4 states the 203 cd/m^2 default twice with
   * different conditions.  The Default paragraph fires only when there is no
   * HAGC tag AND no CRWL entry; the priority order restates it as "defaulting
   * to 203 cd/m^2 when absent" with no HAGC qualifier.  A Linear profile with
   * an HAGC tag carrying a custom reference white and no CRWL entry therefore
   * has two answers for this divisor.  The ruling here is the one taken at
   * the reference-white block above: the HAGC tag's value wins, because the
   * gain curve in that same tag was authored against it.  The two readings
   * coincide at 203,0 whenever the HAGC tag sets no custom value, which is
   * why the disagreement will not show up in testing.
   *
   * The reader is consulted even when it holds no HDR Image entry, since rule
   * c) applies precisely to that case; the bHasMeta guard above is not
   * repeated here.  A profile with no metadataTag leaves the reader empty,
   * which is what c) tests for. */
  if (info.bHasCicp && info.nTransferCharacteristics == icCicpTransferLinear) {
    info.nContentHeadroomSource = meta.ResolveContentHeadroom(info.contentHeadroom,
                                                              info.contentReferenceWhite);
  }

  /* Source primaries (9.2.17 / 10.3). Only meaningful with a cicpTag, since
   * the ColourPrimaries field is what selects between the H.273 table and the
   * profile's own matrix columns. */
  if (info.bHasCicp) {
    info.bPrimariesResolved = icGetResolvedPrimaries(pProfile, info.nColourPrimaries,
                                                     info.primaries);
  }

  /* Classification.  The conforming test is the full set of 8.7.1.1
   * membership conditions: the ColorSpace class and RGB data colour space of
   * 8.7.1.5, PCSXYZ (see bPcsXyz in the header for why that one is here), and
   * the cicpTag requirement with its permitted transfer values.
   *
   * THE 23-09-2026 REVISION DELETED THREE TERMS FROM THIS EXPRESSION, and
   * each deletion is the removal of a test rather than a relaxation of one -
   * the clause that stated it is gone:
   *
   *  - bVersion4_5.  The previous revision required profileVersionField
   *    4.5.0.0; 4.7 of this one says the amendment is a minor-version change
   *    "without requiring any change to the profileVersionField".
   *  - bTrcTagsPresent.  The previous revision prohibited the three TRC tags,
   *    and their absence was a membership condition.  8.7 defines no TRC tag
   *    for a ColorSpace profile, so, in the revision's words, "there is
   *    nothing for this amendment to prohibit".  A ColorSpace profile that
   *    carries one anyway is already reported by
   *    CIccProfile::CheckTagExclusion(), which is where a tag with no defined
   *    interpretation for a class belongs.
   *  - the matrix column tags.  8.7 defines none, and the ColourPrimaries 2
   *    case they served now routes to the cicpType extension of 10.3.
   *    PROPOSAL-ISSUE HDR-15, which recorded that 8.10.1 stated the matrix
   *    column rule three times at three different strengths, is WITHDRAWN:
   *    the sentences that disagreed are all gone.
   *
   * Missing a condition is not a defect and is never reported as one - see the
   * icHdrProfileClass comment in the header.  The second state below exists
   * only so a report can describe what the profile is.  A HAGC tag, HDR
   * metadata, or a cicpTag declaring PQ or HLG is HDR-related content whatever
   * class the profile turns out to belong to.  Linear (8) is deliberately not
   * a signal on its own: it is as common in SDR workflows as in HDR ones, and
   * treating it as one would open an HDR section on ordinary profiles. */
  bool bMembership = info.bRgbColorSpace && info.bVersion4 && info.bPcsXyz &&
                     info.bHasCicp && info.bTransferIsHdr;

  /* ColourPrimaries 2 is deliberately NOT a membership condition, even though
   * this build can neither read the cicpType extension 8.7.1.1 requires there
   * nor build a matrix without it.  The amendment calls such a profile
   * "non-conforming", not a non-member - it is an HDR ColorSpace Profile that
   * is broken, exactly as a profile missing a required tag is - and
   * membership and conformance are kept apart here for the reason
   * icHdrProfileClass gives.  CIccProfile::CheckHdrProfile() reports it, and
   * icHdrSelectForwardMatrix() refuses to render it. */

  if (bMembership) {
    info.nClass = icHdrProfileConforming;
  }
  else if (info.bHasHagc || (bHasMeta && meta.HasAnyHdrEntry()) ||
           (info.bHasCicp && (info.nTransferCharacteristics == icCicpTransferPQ ||
                              info.nTransferCharacteristics == icCicpTransferHLG))) {
    info.nClass = icHdrProfileHdrContent;
  }

  return true;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
