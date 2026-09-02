/** @file
    File:       IccHdrProfile.h

    Contains:   Header for HDR Profile (ICC.1 clause 8.10) recognition, the
                CICP ColourPrimaries resolution that clause 9.2.17/10.3
                defines, and the HDR Image / HDR Display metadataTag reader

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

#if !defined(_ICCHDRPROFILE_H)
#define _ICCHDRPROFILE_H

#include <string>
#include "IccDefs.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccProfile;
class CIccTagDict;

/** The three TransferCharacteristics values an HDR Profile may declare
 * (clause 8.10.1 and 8.10.6): Linear, PQ and HLG. Any other value "shall not
 * be used in an HDR Profile". */
#define icCicpTransferLinear   8
#define icCicpTransferPQ      16
#define icCicpTransferHLG     18

/** ITU-T H.273 ColourPrimaries value 2, "Unspecified". For a three-component
 * matrix-based RGB Input or Display profile this is not "unknown": clause
 * 9.2.17 directs the primaries to be recovered from the profile's own matrix
 * column tags. */
#define icCicpPrimariesUnspecified 2

/** Default HDR reference white luminance in cd/m^2 when no Content HDR
 * Reference White Luminance entry is present (clause 8.10.4). Matches the
 * HAGC tag's own default, which is not a coincidence - both cite Rec. ITU-R
 * BT.2408 graphics white. */
#define icHdrDefaultContentReferenceWhite 203.0

/** Default mastering peak luminance in cd/m^2 for a Linear HDR Profile that
 * carries neither a CLL nor an MDCV entry (clause 8.10.4, priority order c).
 * Unlike the 203 above this is not a reference white: it is the content peak
 * the priority order assumes in the absence of metadata, and it is stated
 * only for the Linear transfer, which - unlike PQ and HLG - establishes no
 * peak of its own. */
#define icHdrDefaultMasteringPeak 1000.0

/**
 ***********************************************************************
 * Chromaticity coordinates of a set of colour primaries and their white,
 * in the (x, y) form of Table 2 of ITU-T H.273.
 ***********************************************************************
 */
typedef struct {
  icFloatNumber xRed,   yRed;
  icFloatNumber xGreen, yGreen;
  icFloatNumber xBlue,  yBlue;
  icFloatNumber xWhite, yWhite;
} icCicpPrimaries;

/**
 * How a profile relates to the HDR Profile sub-class of clause 8.10.
 *
 * 8.10.1 states the relation in its own words: "An HDR Profile is a sub-class
 * of the three-component matrix-based Input or Display profile defined in 8.3.3
 * and 8.4.3". It then states four conditions for membership of that sub-class -
 * profile format version 4.5.0.0, the matrix-based RGB Input or Display
 * structure, a cicpTag, and a TransferCharacteristics of 8, 16 or 18 - and they
 * are DEFINITIONAL, not conformance requirements. A profile that misses one is
 * not an HDR Profile; it remains the Input or Display profile 8.10.1 names as
 * the parent, and nothing is wrong with it. A matrix/TRC profile whose cicpTag
 * says TransferCharacteristics 13, for instance, is an ordinary Display profile
 * that happens to carry a cicpTag - not a broken HDR Profile.
 *
 * Nothing in this API, in CIccProfile::CheckHdrProfile(), or in the PAWG report
 * reports non-membership as an error or a warning. The most that is ever said
 * is what a profile IS.
 */
typedef enum {
  /** No HDR Profile content and no HDR-related content. */
  icHdrProfileNone = 0,

  /** Satisfies clause 8.10.1 in full: version 4.5.0.0, RGB, Input or Display,
   * three-component matrix-based, and a cicpTag with TransferCharacteristics
   * in {8, 16, 18}. */
  icHdrProfileConforming = 1,

  /** Not an HDR Profile, but carrying HDR-related content: a HAGC tag, HDR
   * Image / HDR Display metadata, or a cicpTag declaring the PQ or HLG
   * transfer. Purely descriptive - it exists so a report can say what the
   * profile is, and carries no verdict of any kind. */
  icHdrProfileHdrContent = 2,
} icHdrProfileClass;

/**
 * Which rule of clause 8.10.5 produced the display headroom, in the
 * precedence order that clause defines. Reported alongside the value because
 * NOTE 13 makes the provenance normative: when all three entries are present
 * and disagree, DERH wins and the DCV/DRWL derivation is explicitly *not* to
 * be recomputed, so a consumer needs to know which rule fired.
 */
typedef enum {
  icHdrHeadroomNone     = 0,  /* 8.10.5 d): not derivable from the profile */
  icHdrHeadroomDerh     = 1,  /* 8.10.5 a): DERH taken directly */
  icHdrHeadroomDcvDrwl  = 2,  /* 8.10.5 b): DCV.maxLuminance / DRWL */
  icHdrHeadroomDcvCrwl  = 3,  /* 8.10.5 c): DCV.maxLuminance / CRWL */
} icHdrHeadroomSource;

/**
 * Which rule of clause 8.10.4's priority order produced the scalar content
 * headroom Hcontent, in the order that clause defines.
 *
 * The order applies to the Linear (8) transfer only. PQ and HLG carry a peak
 * luminance in the transfer function itself, so a content headroom derived
 * from metadata would be answering a question those two transfers have
 * already answered; 8.10.4 states the order for Linear and NOTE 9 sends a CMM
 * that wants the others to "the conventions of the
 * cicpTag.TransferCharacteristics" instead.
 *
 * Reported alongside the value for the same reason the display source is:
 * rule c) is a stated assumption about content the profile says nothing
 * about, and a consumer that cannot tell it from a) has no way to know the
 * 1000 cd/m^2 came from the amendment rather than from the metadata.
 */
typedef enum {
  icHdrContentHeadroomNone    = 0,  /* not derivable: the transfer is not Linear,
                                     * or the reference white is not positive */
  icHdrContentHeadroomCll     = 1,  /* 8.10.4 a): CLL.max / CRWL */
  icHdrContentHeadroomMdcv    = 2,  /* 8.10.4 b): MDCV.maxLuminance / CRWL */
  icHdrContentHeadroomDefault = 3,  /* 8.10.4 c): 1000 cd/m^2 / CRWL */
} icHdrContentHeadroomSource;

/**
 ***********************************************************************
 * Class: CIccHdrMetadataReader
 *
 * Purpose:
 *  Reads the HDR Image and HDR Display entries of clauses 8.10.4 and 8.10.5
 *  out of a profile's metadataTag.
 *
 *  Both clauses make the ICC dictType Metadata Registry "the authoritative
 *  source for the names, encodings and semantics of each entry". The four
 *  HDR Image entries this class reads - CRWL, CLL, MDCV, CCV - are registered
 *  (HDR Image category, Apple Inc., 2025-06-04) and are read on the field
 *  lists their Value columns give. The dictType definition posted alongside
 *  the registry fixes the storage: name and value strings are UTF-16BE
 *  Unicode, not NULL terminated.
 *
 *  The three HDR Display entries - DERH, DCV, DRWL - are not registered.
 *  Clause 8.10.5 is not yet accepted, so the registry correctly carries no
 *  HDR Display category; their names and shapes are read from 8.10.5 itself,
 *  DCV on the shape of its registered sibling MDCV.
 *
 *  PROPOSAL-ISSUE HDR-11: that reconstruction is a ruling.  8.10.5 makes the
 *  registry "the authoritative source for the names, encodings and semantics
 *  of each entry" and then describes entries the registry does not carry, so
 *  there is no authoritative shape to read; DCV's field order and units here
 *  are MDCV's, on the strength of the two being siblings and nothing more.
 *  Re-verified against the live registry 2026-09-01: it carries HDR image
 *  (MDCV, CCV, CLL, CRWL) and Printing, and no HDR Display category.  If the
 *  category is registered with a different shape, this parse changes - which
 *  is why a wrong field count is reported as unparsed rather than guessed at.
 *
 *  One thing the registry does not state is what separates the fields of a
 *  multi-value entry (PROPOSAL-ISSUE HDR-11: the registry's Value column
 *  gives the field list and no delimiter, and the amendment does not
 *  reproduce the definitions), so any of space, tab, comma or semicolon is
 *  accepted,
 *  and a value whose field count is wrong is reported as unparsed rather than
 *  read as its first few fields. Callers can tell "absent" from "present but
 *  not understood" (see HasUnparsedEntries()), and no validation diagnostic
 *  anywhere is raised on the strength of a parse this class performed - only
 *  informational output.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccHdrMetadataReader
{
public:
  CIccHdrMetadataReader();

  /** Read the profile's metadataTag. Returns false when the profile has no
   * metadataTag, which is not an error - the entries are all optional. */
  bool Read(const CIccProfile *pProfile);

  /* --- HDR Image entries (clause 8.10.4) --- */

  /** CRWL, Content HDR Reference White Luminance, in cd/m^2. */
  bool HasContentReferenceWhite() const { return m_bHasCrwl; }
  icFloatNumber GetContentReferenceWhite() const { return m_crwl; }

  /** CRWL with clause 8.10.4's 203 cd/m^2 default applied. This is the only
   * entry the amendment gives a normative default to; NOTE 9 is explicit that
   * the others have none. */
  icFloatNumber GetResolvedContentReferenceWhite() const;

  /** CLL, Content Light Level: maximum content light level and maximum
   * frame-average light level, both in cd/m^2. */
  bool HasContentLightLevel() const { return m_bHasCll; }
  icFloatNumber GetMaxContentLightLevel() const { return m_maxCll; }
  icFloatNumber GetMaxFrameAverageLightLevel() const { return m_maxFall; }
  bool ContentLightLevelPrimariesResolved() const { return m_bCllPrimariesResolved; }
  const icCicpPrimaries &GetContentLightLevelPrimaries() const { return m_cllPrimaries; }

  /** MDCV, Mastering Display Colour Volume: primaries plus a luminance range. */
  bool HasMasteringDisplayColourVolume() const { return m_bHasMdcv; }
  const icCicpPrimaries &GetMasteringPrimaries() const { return m_mdcvPrimaries; }
  icFloatNumber GetMasteringMaxLuminance() const { return m_mdcvMaxLuminance; }
  icFloatNumber GetMasteringMinLuminance() const { return m_mdcvMinLuminance; }
  bool MasteringPrimariesResolved() const { return m_bMdcvPrimariesResolved; }

  /** CCV, Content Colour Volume. Retained as its raw string only: unlike the
   * others its shape has no unambiguous counterpart to reconstruct from, so
   * inventing a parse would be guessing rather than reconstructing. */
  bool HasContentColourVolume() const { return m_bHasCcv; }
  const std::string &GetContentColourVolumeRaw() const { return m_ccvRaw; }

  /* --- HDR Display entries (clause 8.10.5) --- */

  /** DERH, Display Extended Range Headroom: peak luminance over reference
   * white luminance, as a ratio. */
  bool HasDisplayHeadroom() const { return m_bHasDerh; }
  icFloatNumber GetDisplayHeadroom() const { return m_derh; }

  /** DRWL, Display HDR Reference White Luminance, in cd/m^2. */
  bool HasDisplayReferenceWhite() const { return m_bHasDrwl; }
  icFloatNumber GetDisplayReferenceWhite() const { return m_drwl; }

  /** DCV, Display Colour Volume: primaries plus a luminance range. */
  bool HasDisplayColourVolume() const { return m_bHasDcv; }
  const icCicpPrimaries &GetDisplayPrimaries() const { return m_dcvPrimaries; }
  icFloatNumber GetDisplayMaxLuminance() const { return m_dcvMaxLuminance; }
  icFloatNumber GetDisplayMinLuminance() const { return m_dcvMinLuminance; }
  bool DisplayPrimariesResolved() const { return m_bDcvPrimariesResolved; }

  /**
   * Resolve the scalar display headroom by the precedence of clause 8.10.5.
   *
   * Returns the rule that fired. When it is icHdrHeadroomNone the profile
   * does not determine a headroom and the caller must take H_target from the
   * destination device instead (8.10.5 d) and NOTE 12).
   */
  icHdrHeadroomSource ResolveDisplayHeadroom(icFloatNumber &headroom) const;

  /**
   * Resolve the scalar content headroom Hcontent by the priority order of
   * clause 8.10.4, for a profile whose cicpTag.TransferCharacteristics is
   * Linear (8). The caller owns that test: this class reads the metadataTag
   * and never the cicpTag.
   *
   * crwl is the content HDR reference white to divide by. It is a parameter
   * rather than a lookup because the profile-level answer is not always this
   * class's own: an HAGC tag carries its own reference white, and
   * icGetHdrProfileInfo() prefers it. Passing the value the caller resolved
   * keeps Hcontent consistent with the reference white reported beside it.
   *
   * Returns the rule that fired; icHdrContentHeadroomNone means no value was
   * produced, which for a Linear profile can only happen when crwl is not
   * positive, since rule c) has no metadata precondition.
   */
  icHdrContentHeadroomSource ResolveContentHeadroom(icFloatNumber &headroom,
                                                   icFloatNumber crwl) const;

  /** ResolveContentHeadroom() against this class's own CRWL - the metadataTag
   * entry, or clause 8.10.4's 203 cd/m^2 default when it is absent. Correct
   * for a profile with no HAGC tag; see the overload above for why the
   * profile-level path passes its own value instead. */
  icHdrContentHeadroomSource ResolveContentHeadroom(icFloatNumber &headroom) const
  { return ResolveContentHeadroom(headroom, GetResolvedContentReferenceWhite()); }

  /** True when a recognised key was present but its value did not parse.
   * Kept separate from absence so a caller never reads a reconstruction
   * failure as "the profile does not carry this". */
  bool HasUnparsedEntries() const { return m_bUnparsed; }

  /** True when the profile carries any HDR Image or HDR Display entry at all,
   * parsed or not. This is one of the two signals that a profile intends HDR
   * semantics (the other is a tone-mapping descriptor). */
  bool HasAnyHdrEntry() const;

protected:
  bool m_bHasCrwl, m_bHasCll, m_bHasMdcv, m_bHasCcv;

  /* A registry primaries code of 2 means "not expressible in ITU-T H.273; use
   * the containing profile's own tags", and unassigned codes name nothing at
   * all. Either way no chromaticities are resolved, and a caller must not read
   * the primaries struct without checking. */
  bool m_bCllPrimariesResolved, m_bMdcvPrimariesResolved, m_bDcvPrimariesResolved;
  bool m_bHasDerh, m_bHasDrwl, m_bHasDcv;
  bool m_bUnparsed;

  icFloatNumber m_crwl;
  icFloatNumber m_maxCll, m_maxFall;
  icCicpPrimaries m_cllPrimaries;
  icCicpPrimaries m_mdcvPrimaries;
  icFloatNumber m_mdcvMaxLuminance, m_mdcvMinLuminance;
  std::string m_ccvRaw;

  icFloatNumber m_derh, m_drwl;
  icCicpPrimaries m_dcvPrimaries;
  icFloatNumber m_dcvMaxLuminance, m_dcvMinLuminance;
};

/**
 ***********************************************************************
 * Everything clause 8.10 lets a consumer determine about a profile, resolved
 * in one pass so that a caller never has to re-derive a value the clause
 * already defines a precedence for.
 ***********************************************************************
 */
typedef struct {
  icHdrProfileClass nClass;

  /* Structural facts the classification was made from. */
  bool bRgbMatrixBased;      /* RGB + Input/Display + the three matrix column and TRC tags */
  bool bVersion4_5;          /* profileVersionField declares 4.5.0.0 or later within v4 */
  bool bHasCicp;
  icUInt8Number nColourPrimaries;
  icUInt8Number nTransferCharacteristics;

  /* The other two cicpType fields.  Clause 8.10 never mentions either, and
   * nothing in the HDR path consults them today - they are reported because
   * they were being read and thrown away, and because MatrixCoefficients is
   * the field that would answer which luma coefficients the HLG OOTF should
   * use: ITU-T H.273 Table 4 ties BT.2100's 0,2627 / 0,0593 to the value 9 and
   * its equations 39 to 44 derive them from chromaticities for 12 and 13.  See
   * the coefficient block in IccHdrToneMap.h.  VideoFullRangeFlag is reported
   * for symmetry; a narrow-range HDR Profile is legal and nothing here acts on
   * it. */
  icUInt8Number nMatrixCoefficients;
  bool bVideoFullRange;

  bool bTransferIsHdr;       /* TransferCharacteristics in {8, 16, 18} */

  /* Tone-mapping descriptors of clause 8.10.3, in its recommended ranking. */
  bool bHasHagc;             /* 8.10.3 a) */
  bool bHasAToB0;            /* 8.10.3 c) */
  bool bHasBToA0;

  /* Resolved content reference white (8.10.4), always populated: the 203
   * cd/m^2 default applies when neither the HAGC tag nor a CRWL entry
   * supplies one. */
  icFloatNumber contentReferenceWhite;
  bool bContentReferenceWhiteFromProfile;

  /* Resolved content headroom (8.10.4's Linear priority order). Populated
   * only when the cicpTag declares TransferCharacteristics 8; for PQ and HLG
   * the source is icHdrContentHeadroomNone and contentHeadroom is 0, which
   * says the clause defines no metadata-derived value there, not that the
   * content has no headroom. */
  icHdrContentHeadroomSource nContentHeadroomSource;
  icFloatNumber contentHeadroom;

  /* Resolved display headroom (8.10.5). */
  icHdrHeadroomSource nHeadroomSource;
  icFloatNumber displayHeadroom;

  /* Resolved source primaries (9.2.17 / 10.3), from the H.273 table when
   * ColourPrimaries names one and from the profile's own matrix column tags
   * when it is 2. */
  bool bPrimariesResolved;
  bool bPrimariesFromProfile;
  icCicpPrimaries primaries;
} icHdrProfileInfo;

/**
 * Look up the chromaticity coordinates of an ITU-T H.273 Table 2
 * ColourPrimaries value. Returns false for Reserved, Unspecified and every
 * unassigned value - including 2, which has no fixed chromaticities by
 * definition and must go through icGetProfilePrimaries() instead.
 */
ICCPROFLIB_API bool icGetCicpPrimaries(icUInt8Number nColourPrimaries, icCicpPrimaries &primaries);

/**
 * Recover the source primaries from a profile's own tags, as clause 10.3
 * (the CICP Unspecified-Primaries amendment, APPROVED 2026-08-20 - which
 * promoted this procedure out of a NOTE and into normative text, so cite the
 * clause and not a note number; the note numbering moved with it)
 * 4.3 specifies for ColourPrimaries equal to 2: take the CIEXYZ of the
 * three matrix column tags and the media white point, undo the chromatic
 * adaptation when a chromaticAdaptationTag is present so the values are
 * relative to the profile's actual adopted white rather than to the PCS
 * adopted white, then convert each to (x, y).
 *
 * Returns false when the profile is not three-component matrix-based, when
 * the chromaticAdaptationTag is present but singular, or when any tristimulus
 * triplet sums to zero and has no chromaticity.
 */
ICCPROFLIB_API bool icGetProfilePrimaries(const CIccProfile *pProfile, icCicpPrimaries &primaries);

/**
 * Resolve the source primaries the way a consumer of the cicpTag should:
 * through the H.273 table when ColourPrimaries names a set, and through the
 * profile's own tags when it is 2 and the profile is three-component
 * matrix-based (clause 9.2.17). Sets bFromProfile so the caller can tell the
 * two paths apart.
 */
ICCPROFLIB_API bool icGetResolvedPrimaries(const CIccProfile *pProfile, icUInt8Number nColourPrimaries,
                                           icCicpPrimaries &primaries, bool *bFromProfile = NULL);

/**
 * Build the 3x3 matrix taking linear RGB in a set of primaries to CIEXYZ,
 * scaled so that RGB = (1, 1, 1) maps to the primaries' own white with Y = 1.
 *
 * This is the standard construction and is stated in no one document: the
 * three chromaticities give the directions of the columns and the white point
 * fixes their lengths.  Returns false when a chromaticity has y = 0, or when
 * the three primaries are collinear and the system has no solution.
 *
 * matrix is nine elements, row major, as every other 3x3 in this library.
 */
ICCPROFLIB_API bool icBuildRgbToXyzMatrix(const icCicpPrimaries &primaries, icFloatNumber *matrix);

/**
 * Build the 3x3 matrix taking linear RGB in one set of primaries to linear RGB
 * in another, chromatically adapting between their white points when they
 * differ.
 *
 * The adaptation is the linearized Bradford transform of ICC.1:2022 Annex E.3,
 * which is what E.2 composes into a chromatic adaptation matrix and what
 * clause 9.2.15 recommends for ICC profiles:
 *
 *   M = M_dst^-1 . M_BFD^-1 . diag(rho_dst / rho_src) . M_BFD . M_src
 *
 * Returns false when either primary set is degenerate or a matrix in the chain
 * is singular.  When the two white points are equal the adaptation reduces to
 * the identity, and it is computed rather than special cased so that a caller
 * cannot get a different answer by rounding.
 */
ICCPROFLIB_API bool icBuildPrimariesConversionMatrix(const icCicpPrimaries &src,
                                                     const icCicpPrimaries &dst,
                                                     icFloatNumber *matrix);

/**
 * Resolve a headroomAdaptiveGainCurveTag's Gain Curve Chromaticities Mode to a
 * set of chromaticities.
 *
 * Modes 0, 1 and 2 name entries in ITU-T H.273 Table 2 and mode 3 takes the
 * eight values the tag carries, in the order [xR yR xG yG xB yB xW yW].
 *
 * PROPOSAL-ISSUE HAGC-09: mode 0 is described in proposal 0.1.2.7 as "the
 * colour primaries with a value of 2 in Table 2 in ITU-T H.273, i.e. primaries
 * from Recommendation ITU-R BT.709-6".  Those are different things - BT.709-6
 * is H.273 value 1, and value 2 is Unspecified, which under the CICP
 * Unspecified-Primaries amendment means "resolve against the profile's own
 * matrix column tags".  This follows the NAME, so mode 0 resolves to BT.709.
 *
 * pCustom is read only for mode 3 and may be NULL otherwise.  Returns false
 * for an unknown mode, or for mode 3 with no values.
 */
ICCPROFLIB_API bool icHagcGetGainApplicationPrimaries(icUInt8Number nMode,
                                                      const icFloatNumber *pCustom,
                                                      icCicpPrimaries &primaries);

/**
 * Classify a profile against clause 8.10 and resolve everything the clause
 * defines. Returns false only when pProfile is NULL; a profile that is not an
 * HDR Profile is reported as icHdrProfileNone with the structural fields
 * still filled in.
 */
ICCPROFLIB_API bool icGetHdrProfileInfo(const CIccProfile *pProfile, icHdrProfileInfo &info);

/** Human-readable name of a TransferCharacteristics value, restricted to the
 * three an HDR Profile may use. Returns NULL for any other value. */
ICCPROFLIB_API const icChar *icGetHdrTransferName(icUInt8Number nTransferCharacteristics);

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCHDRPROFILE_H)
