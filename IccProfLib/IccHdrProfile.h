/** @file
    File:       IccHdrProfile.h

    Contains:   Header for HDR ColorSpace Profile (ICC.1 clause 8.7.1) recognition, the
                CICP ColourPrimaries resolution that clause 9.2.17/10.3
                defines, and the HDR Image metadataTag reader

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

#if !defined(_ICCHDRPROFILE_H)
#define _ICCHDRPROFILE_H

#include <string>
#include "IccDefs.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccProfile;
class CIccTagDict;

/** The three TransferCharacteristics values an HDR ColorSpace Profile may declare
 * (clause 8.7.1.1 and 8.7.1.5): Linear, PQ and HLG. Any other value "shall not
 * be used in an HDR ColorSpace Profile". */
#define icCicpTransferLinear   8
#define icCicpTransferPQ      16
#define icCicpTransferHLG     18

/** ITU-T H.273 ColourPrimaries value 2, "Unspecified".
 *
 * In an HDR ColorSpace Profile this is not "unknown", but nor is it the
 * matrix-column recovery of the first CICP Unspecified-Primaries amendment:
 * clause 8.7.1.1 requires the cicpType to carry the CUSTOM CHROMATICITY
 * EXTENSION of 10.3, and 8.7.1.1 NOTE 2 takes both the chromaticities AND the
 * white point from that extension rather than from mediaWhitePointTag.  A
 * ColorSpace profile (8.7) defines no matrix column tag, so there is nothing
 * to recover from.
 *
 * BLOCKED: this build does not implement the extension.  ICC.1:2022 10.3
 * Table 32 is a fixed twelve bytes ending at VideoFullRangeFlag, and the
 * amendment that appends the chromaticity array to it - the CICP Unspecified
 * Primaries Amendment Proposal v2, named in the HDR amendment's normative
 * references - is not in docs/notes/hdr-proposals/.  Without it the array's
 * position, presence signalling, entry count and number encoding are all
 * unknown, so CIccTagCicp still reads twelve bytes and every HDR path treats
 * ColourPrimaries 2 as a profile whose primaries it cannot obtain.  See
 * CIccProfile::CheckHdrProfile(), which reports it.
 *
 * The matrix-column recovery of icGetProfilePrimaries() survives for the ICC
 * dictType Metadata Registry, whose MDCV and CLL entries define their own
 * primaries code 2 that way; that is a registry rule and not this one. */
#define icCicpPrimariesUnspecified 2

/** Default HDR reference white luminance in cd/m^2 when no Content HDR
 * Reference White Luminance entry is present (clause 8.7.1.4). Matches the
 * HAGC tag's own default, which is not a coincidence - both cite Rec. ITU-R
 * BT.2408 graphics white. */
#define icHdrDefaultContentReferenceWhite 203.0

/** Default mastering peak luminance in cd/m^2 for a Linear HDR ColorSpace Profile that
 * carries neither a CLL nor an MDCV entry (clause 8.7.1.4, priority order c).
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
 * How a profile relates to the HDR ColorSpace Profile sub-class of clause 8.7.1.
 *
 * 8.7.1.1 states the relation in its own words: "An HDR ColorSpace Profile is a
 * sub-class of the ColorSpace profile defined in 8.7 in which the source signal
 * is High Dynamic Range and a tone-mapping step is inserted into the HDR
 * processing chain".  The conditions for membership - the ColorSpace class, the
 * RGB data colour space, a cicpTag, and a TransferCharacteristics of 8, 16 or
 * 18 - are DEFINITIONAL, not conformance requirements.  A profile that misses
 * one is not an HDR ColorSpace Profile; it remains the profile of whatever
 * class it declares, and nothing is wrong with it.  A Display profile carrying
 * a PQ cicpTag, for instance, is an ordinary Display profile that happens to
 * carry a cicpTag - and the amendment says so in as many words: such a profile
 * "is not, however, an HDR ColorSpace Profile under clause 8.7.1".
 *
 * Nothing in this API, in CIccProfile::CheckHdrProfile(), or in the PAWG report
 * reports non-membership as an error or a warning. The most that is ever said
 * is what a profile IS.
 *
 * WHAT THE 23-09-2026 REVISION REMOVED FROM THIS TEST.  The sub-class moved
 * from the three-component matrix-based Input/Display profile (8.3.3, 8.4.3)
 * to the ColorSpace profile (8.7), and three membership conditions went with
 * the old parent:
 *
 *  - the profileVersionField 4.5.0.0 LOWER bound.  4.7 of the revision is
 *    explicit that this is a minor-version change "without requiring any
 *    change to the profileVersionField".  The UPPER bound survives, for a
 *    reason that was never the amendment's - see bVersion4 below.  The
 *    cicpTag's own >= 4.4 tag-type gate in CIccProfile::CheckTagTypes() is
 *    untouched and still applies.
 *  - the redTRCTag/greenTRCTag/blueTRCTag prohibition.  8.7 never defines
 *    those tags for a ColorSpace profile, so, in the revision's words, "there
 *    is nothing for this amendment to prohibit".
 *  - the matrix column tags, required when ColourPrimaries was 2.  8.7 defines
 *    none, and 8.7.1.1 routes that case to the cicpType extension of 10.3
 *    instead - see icCicpPrimariesUnspecified above for why this build cannot
 *    follow it yet.
 */
typedef enum {
  /** No HDR ColorSpace Profile content and no HDR-related content. */
  icHdrProfileNone = 0,

  /** Satisfies clause 8.7.1.1 in full: RGB, ColorSpace class, a version 4
   * profile, PCSXYZ, and a cicpTag with TransferCharacteristics in
   * {8, 16, 18}. */
  icHdrProfileConforming = 1,

  /** Not an HDR ColorSpace Profile, but carrying HDR-related content: a HAGC
   * tag, HDR Image metadata, or a cicpTag declaring the PQ or HLG transfer.
   * Purely descriptive - it exists so a report can say what the profile is,
   * and carries no verdict of any kind. */
  icHdrProfileHdrContent = 2,
} icHdrProfileClass;

/* AMENDMENT NOTE NUMBERING, AS OF THE 23-09-2026 REVISION.
 *
 * NOTES 1 to 11 keep the numbers the 2026-09-06 revision gave them, and the
 * citations in this tree were already renumbered to those.  What moved is the
 * tail: 8.10.5 HDR display metadata is deleted outright, taking its NOTES 12,
 * 13 and 14 with it, and 8.7.1.4 gains a NEW NOTE 12 whose subject is
 * different - physical display characterization is out of scope for the
 * sub-class.  A citation of "NOTE 12" written against the previous revision
 * therefore points at text that no longer exists; there are none left in this
 * tree, and a new one must be read against 8.7.1.4 of the current revision.
 *
 * 8.7.1.5 has its own NOTE 1 to NOTE 4, independent of 8.7.1.1's.  Always cite
 * the sub-clause with the note number.
 *
 * Do NOT renumber a citation of SMPTE ST 2094-50 Annex A.2 NOTE 7: that is a
 * different document whose numbering did not move, and IccHdrToneMap.{h,cpp}
 * carry three of them.  Check which specification a NOTE belongs to before
 * touching it.
 */

/**
 * Which rule of clause 8.7.1.4's priority order produced the scalar content
 * headroom Hcontent, in the order that clause defines.
 *
 * The order applies to the Linear (8) transfer only. PQ and HLG carry a peak
 * luminance in the transfer function itself, so a content headroom derived
 * from metadata would be answering a question those two transfers have
 * already answered; 8.7.1.4 states the order for Linear and NOTE 10 sends a CMM
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
  icHdrContentHeadroomCll     = 1,  /* 8.7.1.4 a): CLL.max / CRWL */
  icHdrContentHeadroomMdcv    = 2,  /* 8.7.1.4 b): MDCV.maxLuminance / CRWL */
  icHdrContentHeadroomDefault = 3,  /* 8.7.1.4 c): 1000 cd/m^2 / CRWL */
} icHdrContentHeadroomSource;

/**
 ***********************************************************************
 * Class: CIccHdrMetadataReader
 *
 * Purpose:
 *  Reads the HDR Image entries of clause 8.7.1.4 out of a profile's
 *  metadataTag.
 *
 *  The clause makes the ICC dictType Metadata Registry "the authoritative
 *  source for the names, encodings and semantics of each entry". The four
 *  HDR Image entries this class reads - CRWL, CLL, MDCV, CCV - are registered
 *  (HDR Image category, Apple Inc., 2025-06-04) and are read on the field
 *  lists their Value columns give. The dictType definition posted alongside
 *  the registry fixes the storage: name and value strings are UTF-16BE
 *  Unicode, not NULL terminated.
 *
 *  HDR DISPLAY ENTRIES ARE GONE.  The 23-09-2026 revision deletes clause
 *  8.10.5 and with it the DERH, DCV and DRWL entries and their display-
 *  headroom precedence: "Physical display characterization (peak luminance,
 *  extended-range headroom, and similar measurement-derived properties of a
 *  specific display) is intentionally out of scope for the HDR ColorSpace
 *  Profile sub-class defined by this amendment, which characterizes a colour
 *  encoding rather than a display."  Nothing in 8.7.1 consumes a display
 *  headroom, so this class no longer reads, resolves or reports one, and
 *  PROPOSAL-ISSUE HDR-11 - which recorded that those three entries had to be
 *  reconstructed from prose because the registry never carried them - is
 *  withdrawn with the clause that raised it.  The registration document of
 *  2026-06-24 still exists; if a future amendment gives those entries a
 *  clause again, this is where they come back.
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

  /* --- HDR Image entries (clause 8.7.1.4) --- */

  /** CRWL, Content HDR Reference White Luminance, in cd/m^2. */
  bool HasContentReferenceWhite() const { return m_bHasCrwl; }
  icFloatNumber GetContentReferenceWhite() const { return m_crwl; }

  /** CRWL with clause 8.7.1.4's 203 cd/m^2 default applied. This is the only
   * entry the amendment gives a normative default to; NOTE 10 is explicit that
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

  /**
   * Resolve the scalar content headroom Hcontent by the priority order of
   * clause 8.7.1.4, for a profile whose cicpTag.TransferCharacteristics is
   * Linear (8). The caller owns that test: this class reads the metadataTag
   * and never the cicpTag.
   *
   * crwl is the content HDR reference white to divide by. It is a parameter
   * rather than a lookup because the profile-level answer is not always this
   * class's own: an HAGC tag carries its own reference white, and
   * icGetHdrProfileInfo() prefers it. Passing the value the caller resolved
   * keeps Hcontent consistent with the reference white reported beside it.
   *
   * A CLL or MDCV maximum of 0.0 supplies no peak.  The dictType Metadata
   * Registry defines 0.0 in those entries as "unknown", so rules a) and b) do
   * not fire on it and resolution falls through - to b), then to the default
   * of c).  Treating it as a real peak gave Hcontent = 0, below every target,
   * which switched CIccXformMatrixTrcHdr's target-volume clamp off.
   *
   * Returns the rule that fired; icHdrContentHeadroomNone means no value was
   * produced, which for a Linear profile can only happen when crwl is not
   * positive, since rule c) has no metadata precondition.
   */
  icHdrContentHeadroomSource ResolveContentHeadroom(icFloatNumber &headroom,
                                                   icFloatNumber crwl) const;

  /** ResolveContentHeadroom() against this class's own CRWL - the metadataTag
   * entry, or clause 8.7.1.4's 203 cd/m^2 default when it is absent. Correct
   * for a profile with no HAGC tag; see the overload above for why the
   * profile-level path passes its own value instead. */
  icHdrContentHeadroomSource ResolveContentHeadroom(icFloatNumber &headroom) const
  { return ResolveContentHeadroom(headroom, GetResolvedContentReferenceWhite()); }

  /** True when a recognised key was present but its value did not parse.
   * Kept separate from absence so a caller never reads a reconstruction
   * failure as "the profile does not carry this". */
  bool HasUnparsedEntries() const { return m_bUnparsed; }

  /** True when the profile carries any HDR Image entry at all, parsed or not.
   * This is one of the two signals that a profile intends HDR semantics (the
   * other is a tone-mapping descriptor). */
  bool HasAnyHdrEntry() const;

protected:
  bool m_bHasCrwl, m_bHasCll, m_bHasMdcv, m_bHasCcv;

  /* A registry primaries code of 2 means "use the containing profile's own
   * matrix column tags", which is resolved here rather than reported as
   * unknown - see icHdrParseRegistryVolume().  This is the REGISTRY's rule for
   * its MDCV and CLL entries, and it is untouched by the 23-09-2026 revision;
   * do not confuse it with cicpTag.ColourPrimaries 2, which 8.7.1.1 now routes
   * to the cicpType extension of 10.3 instead.  PROPOSAL-ISSUE HDR-18 recorded
   * that an HDR ColorSpace Profile carries no matrix column tags for the
   * registry rule to read, and it still does not: code 2 in an MDCV or CLL
   * entry of such a profile resolves nothing, and is reported unresolved
   * rather than guessed at.  Unassigned codes name nothing at all.  Either way
   * a caller must not read the primaries struct without checking these
   * first. */
  bool m_bCllPrimariesResolved, m_bMdcvPrimariesResolved;
  bool m_bUnparsed;

  icFloatNumber m_crwl;
  icFloatNumber m_maxCll, m_maxFall;
  icCicpPrimaries m_cllPrimaries;
  icCicpPrimaries m_mdcvPrimaries;
  icFloatNumber m_mdcvMaxLuminance, m_mdcvMinLuminance;
  std::string m_ccvRaw;
};

/**
 ***********************************************************************
 * Everything clause 8.7.1 lets a consumer determine about a profile, resolved
 * in one pass so that a caller never has to re-derive a value the clause
 * already defines a precedence for.
 ***********************************************************************
 */
typedef struct {
  icHdrProfileClass nClass;

  /* Structural facts the classification was made from. */

  /** RGB data colour space and the ColorSpace ('spac') device class - the
   * structural half of 8.7.1.1's membership test, and all that survives of
   * the parent class after the 23-09-2026 revision moved the sub-class from
   * the three-component matrix-based Input/Display profile to the ColorSpace
   * profile of 8.7.  8.7.1.5 states both halves: "The data colour space in
   * the profile header shall be RGB.  The profile class shall be ColorSpace
   * ('spac')." */
  bool bRgbColorSpace;

  /** profileVersionField is below 5.0.0.0.  NOT the 4.5.0.0 condition the
   * previous revision stated - that one is withdrawn by 4.7 - but the bound
   * that keeps an ICC.1 clause off an ICC.2 profile.  See icHdrIsVersion4()
   * in IccHdrProfile.cpp for the ruling and why it is one. */
  bool bVersion4;

  /** Header PCS is PCSXYZ.  A condition of membership: the chain of 8.7.1.2
   * ends in the RGB-to-PCSXYZ matrix of step c), which produces XYZ, and
   * nothing in the chain converts it to Lab.  A Lab-PCS profile classified as
   * a member used to be given an xform that wrote XYZ numbers into a port
   * reporting Lab.
   *
   * This condition did NOT come free with the old parent and does not go away
   * with it: 8.7 permits a ColorSpace profile to connect through either PCS,
   * so a PCSLAB ColorSpace profile is a perfectly ordinary profile that
   * simply has no 8.7.1.2 chain to run.  The amendment's own position is the
   * same one - a PCSLAB-connected destination is tone-mapped to H_target =
   * 1.0 first (8.7.1.2 NOTE 7) - which is a statement about the DESTINATION
   * of the chain, not licence to end the chain in Lab. */
  bool bPcsXyz;
  bool bHasCicp;
  icUInt8Number nColourPrimaries;
  icUInt8Number nTransferCharacteristics;

  /* The other two cicpType fields.  Clause 8.7.1 never mentions either, and
   * nothing in the HDR path consults them today - they are reported because
   * they were being read and thrown away, and because MatrixCoefficients is
   * the field that would answer which luma coefficients the HLG OOTF should
   * use: ITU-T H.273 Table 4 ties BT.2100's 0,2627 / 0,0593 to the value 9 and
   * its equations 39 to 44 derive them from chromaticities for 12 and 13.  See
   * the coefficient block in IccHdrToneMap.h.  VideoFullRangeFlag is reported
   * for symmetry; a narrow-range HDR ColorSpace Profile is legal and nothing here acts on
   * it. */
  icUInt8Number nMatrixCoefficients;
  bool bVideoFullRange;

  bool bTransferIsHdr;       /* TransferCharacteristics in {8, 16, 18} */

  /* Tone-mapping descriptors of clause 8.7.1.3, in its recommended ranking. */
  bool bHasHagc;             /* 8.7.1.3 a) */
  bool bHasAToB0;            /* 8.7.1.3 c) */
  bool bHasBToA0;

  /* Resolved content reference white (8.7.1.4), always populated: the 203
   * cd/m^2 default applies when neither the HAGC tag nor a CRWL entry
   * supplies one. */
  icFloatNumber contentReferenceWhite;
  bool bContentReferenceWhiteFromProfile;

  /* Resolved content headroom (8.7.1.4's Linear priority order). Populated
   * only when the cicpTag declares TransferCharacteristics 8; for PQ and HLG
   * the source is icHdrContentHeadroomNone and contentHeadroom is 0, which
   * says the clause defines no metadata-derived value there, not that the
   * content has no headroom. */
  icHdrContentHeadroomSource nContentHeadroomSource;
  icFloatNumber contentHeadroom;

  /* Resolved source primaries (9.2.17 / 10.3), from the H.273 Table 2 entry
   * that ColourPrimaries names.  ColourPrimaries 2 resolves NOTHING here:
   * 8.7.1.1 routes it to the cicpType custom chromaticity extension of 10.3,
   * which this build cannot read - see icCicpPrimariesUnspecified.  There is
   * no matrix-column fallback any more, because a ColorSpace profile has no
   * matrix column tag to fall back to. */
  bool bPrimariesResolved;
  icCicpPrimaries primaries;
} icHdrProfileInfo;

/**
 * Look up the chromaticity coordinates of an ITU-T H.273 Table 2
 * ColourPrimaries value. Returns false for Reserved, Unspecified and every
 * unassigned value - including 2, which has no fixed chromaticities by
 * definition.  In an HDR ColorSpace Profile, 2 is 8.7.1.1's cicpType
 * extension case and this build resolves nothing for it; see
 * icCicpPrimariesUnspecified.
 */
ICCPROFLIB_API bool icGetCicpPrimaries(icUInt8Number nColourPrimaries, icCicpPrimaries &primaries);

/**
 * Recover the source primaries from a profile's own matrix column tags: take
 * the CIEXYZ of the three matrix column tags and the media white point, undo
 * the chromatic adaptation when a chromaticAdaptationTag is present so the
 * values are relative to the profile's actual adopted white rather than to
 * the PCS adopted white, then convert each to (x, y).
 *
 * THIS IS NO LONGER THE cicpTag's ColourPrimaries 2 PROCEDURE for an HDR
 * ColorSpace Profile.  It was, under the first CICP Unspecified-Primaries
 * amendment and the 8.10 revision of the HDR proposal; the 23-09-2026
 * revision builds the sub-class on the ColorSpace profile of 8.7, which has
 * no matrix column tags, and routes ColourPrimaries 2 to the cicpType custom
 * chromaticity extension of 10.3 instead (8.7.1.1 NOTE 2).
 *
 * What still uses it is the ICC dictType Metadata Registry: the MDCV and CLL
 * entries define their own primaries code 2 as "the containing profile's
 * matrix column tags", and that rule is the registry's, untouched by this
 * amendment.  See CIccHdrMetadataReader.
 *
 * Returns false when the profile is not three-component matrix-based, when
 * the chromaticAdaptationTag is present but singular, or when any tristimulus
 * triplet sums to zero and has no chromaticity.
 */
ICCPROFLIB_API bool icGetProfilePrimaries(const CIccProfile *pProfile, icCicpPrimaries &primaries);

/**
 * Resolve the source primaries the way a consumer of the cicpTag should: from
 * the H.273 Table 2 entry ColourPrimaries names.
 *
 * ColourPrimaries 2 returns false.  8.7.1.1 requires the cicpType custom
 * chromaticity extension of 10.3 in that case and takes the chromaticities
 * AND the white point from it; this build cannot read that extension, and
 * there is no matrix-column fallback to take instead because a ColorSpace
 * profile has none.  See icCicpPrimariesUnspecified for what is blocked and
 * on what.
 *
 * pProfile is retained in the signature for the extension, which is read from
 * the profile's own cicpTag, and is unused until it lands.
 */
ICCPROFLIB_API bool icGetResolvedPrimaries(const CIccProfile *pProfile, icUInt8Number nColourPrimaries,
                                           icCicpPrimaries &primaries);

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
 * Build the RGB-to-PCSXYZ matrix that clause 8.7.1.2 c) needs, for an HDR
 * Profile whose cicpTag names a set of primaries.
 *
 * 8.7.1.1 makes this mandatory and not optional: when ColourPrimaries is not 2,
 * the matrix "shall be computed from the chromaticity coordinates associated
 * with ColourPrimaries (see Rec. ITU-T H.273, Table 2) and the profile's
 * adopted white point", and the matrix column tags are no longer required to
 * be present at all.
 *
 * PROPOSAL-ISSUE HDR-07: NOTE 2's stated procedure cannot produce that matrix.
 * It asks for the H.273 chromaticities and the profile's adopted white point
 * and then stops, which yields a matrix whose columns sit at the H.273
 * chromaticities - but 8.7.1.2 c) feeds PCSXYZ, whose values are relative to
 * the PCS adopted white.  No choice of white point closes the gap, because
 * what is missing is not a white point but an operation: the chromatic
 * adaptation.  ICC.1 6.2.1 NOTE 1 explains why nobody noticed - a CMM never
 * applies the chad, because in a conventional profile the author baked it into
 * the matrix column tags, and NOTE 2 moved matrix construction to run time
 * without moving the adaptation with it.
 *
 * The ruling implemented here is the register's suggested resolution, in
 * order:
 *
 *   1. recover the profile's ACTUAL adopted white by applying the inverse of
 *      the chromaticAdaptationTag to the mediaWhitePointTag - the same
 *      inverse, and for the same reason, as icGetProfilePrimaries() above
 *      (PROPOSAL-ISSUE BALLOT-01);
 *   2. build the primary matrix from the H.273 chromaticities and that white;
 *   3. apply the chromaticAdaptationTag to the result, so that
 *      M_PCS = [chad] . M_actual.
 *
 * The columns of the returned matrix therefore do NOT sit at the H.273
 * chromaticities, which is the point: they sit where a conventional profile's
 * matrix column tags would have sat, because that is what PCSXYZ means.
 *
 * Returns false when ColourPrimaries names no chromaticities (0, 2, 3 and the
 * unassigned values), when the mediaWhitePointTag is absent, or when the
 * chromaticAdaptationTag is present but singular or malformed (see
 * icHdrHasMalformedChad()) - an absent one builds the unadapted matrix, which
 * is then correct.
 *
 * ColourPrimaries 2 is one of the values that returns false, and under the
 * 23-09-2026 revision there is nothing to do about it here: 8.7.1.1 NOTE 2
 * takes both the chromaticities and the white point from the cicpType
 * extension of 10.3, so neither this function's H.273 lookup nor the
 * profile's mediaWhitePointTag is the right source, and there are no matrix
 * column tags in a ColorSpace profile to fall back on.  See
 * icCicpPrimariesUnspecified.
 */
ICCPROFLIB_API bool icBuildHdrForwardMatrix(const CIccProfile *pProfile,
                                            icUInt8Number nColourPrimaries,
                                            icFloatNumber *matrix);

/**
 * True when the profile carries a chromaticAdaptationTag that cannot be used:
 * present, but not an s15Fixed16ArrayType or with fewer than nine values.
 *
 * Absent and malformed are different answers and have to stay apart.  An
 * absent tag means the profile's adopted white IS the PCS adopted white, so
 * the forward matrix is built unadapted and that is correct.  A malformed one
 * means the adaptation exists and cannot be read; building unadapted then is a
 * wrong matrix - 11% in X for a D65 profile - delivered with no diagnostic.
 * icBuildHdrForwardMatrix() refuses that case, and a caller holding another
 * matrix to fall back to (the colorant tags of a conventional profile) uses
 * this to refuse it too rather than render around it, as
 * icGetProfilePrimaries() already does.
 */
ICCPROFLIB_API bool icHdrHasMalformedChad(const CIccProfile *pProfile);

/** Where the RGB-to-PCSXYZ matrix of clause 8.7.1.2 c) comes from. */
typedef enum {
  icHdrMatrixFromCicp = 0,     /* built from the cicpTag primaries; see below */
  icHdrMatrixNeedsCicpExt,     /* ColourPrimaries 2: needs the 10.3 extension */
  icHdrMatrixMalformedChad,    /* see icHdrHasMalformedChad() */
  icHdrMatrixUnavailable       /* no source the HDR chain can use */
} icHdrMatrixSource;

/**
 * Decide which matrix an HDR rendering of this profile uses, and build it when
 * it comes from the cicpTag.
 *
 * CIccXformMatrixTrcHdr::Begin() and CIccHdrBaker::Init() both take this one
 * decision, so the bake can never store a rendering the live chain refuses,
 * nor refuse one it renders.  They used to decide separately, and drifted.
 *
 * The 23-09-2026 revision leaves exactly one source.  A ColorSpace profile
 * has no matrix column tags, so the two column-fed answers this enum used to
 * carry - the ColourPrimaries 2 case and the conventional-profile fallback -
 * have nothing to read and are gone with the old parent class:
 *
 *  - ColourPrimaries 2: icHdrMatrixNeedsCicpExt.  8.7.1.1 NOTE 2 puts the
 *    chromaticities and the white point in the cicpType extension of 10.3,
 *    which this build cannot read; see icCicpPrimariesUnspecified.  The
 *    amendment calls a profile that omits the extension non-conforming, and
 *    this build cannot tell that profile from one that carries it, so both
 *    land here and neither renders.
 *  - otherwise a malformed chromaticAdaptationTag is refused.
 *  - otherwise icBuildHdrForwardMatrix() - icHdrMatrixFromCicp, with matrix
 *    filled in.
 *  - failing that, icHdrMatrixUnavailable.  There is no second matrix to fall
 *    back to: rendering with some other matrix than the one the clause's
 *    "shall" names would be a different rendering reported as success.
 */
ICCPROFLIB_API icHdrMatrixSource icHdrSelectForwardMatrix(const CIccProfile *pProfile,
                                                          icUInt8Number nColourPrimaries,
                                                          icFloatNumber *matrix);

/**
 * Classify a profile against clause 8.7.1 and resolve everything the clause
 * defines. Returns false only when pProfile is NULL; a profile that is not an
 * HDR ColorSpace Profile is reported as icHdrProfileNone with the structural fields
 * still filled in.
 */
ICCPROFLIB_API bool icGetHdrProfileInfo(const CIccProfile *pProfile, icHdrProfileInfo &info);

/**
 * True when the header alone leaves a profile eligible for clause 8.7.1.1:
 * RGB data colour space, the ColorSpace ('spac') class, and PCSXYZ (see
 * bPcsXyz).  These are the membership conditions that need no tag, so a
 * caller can rule a profile out before icGetHdrProfileInfo() loads any.
 * Returns false when pProfile is NULL.
 */
ICCPROFLIB_API bool icHdrHeaderAdmitsMembership(const CIccProfile *pProfile);

/** Human-readable name of a TransferCharacteristics value, restricted to the
 * three an HDR ColorSpace Profile may use. Returns NULL for any other value. */
ICCPROFLIB_API const icChar *icGetHdrTransferName(icUInt8Number nTransferCharacteristics);

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCHDRPROFILE_H)
