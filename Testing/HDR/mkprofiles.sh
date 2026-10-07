#!/bin/sh
#################################################################################
# mkprofiles.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-09-09
#
# Intent: iccDEV CICD
#################################################################################
#
# This file is the SINGLE SOURCE of the Testing/HDR fixture list. Per #2328,
# Testing/CreateAllProfiles.sh calls this script rather than keeping a second
# copy of the list: a duplicate drifts silently and leaves
# iccdev.qa-profile-manifest either short a profile or short a manifest row,
# depending on which copy was edited. The LIST lives here and nowhere else.
#
# A new fixture also needs its expectations recorded, each CTest-enforced: a
# row in Testing/qa-profile-manifest.tsv (iccdev.qa-profile-manifest) and in
# Testing/HDR/hdr-corpus-manifest.tsv (iccdev.hdr-corpus-manifest), and - for a
# negative fixture iccFromXml saves but reports invalid - a row in
# Testing/expected-invalid-fromxml.tsv.
#
# The caller owns the .icc delete and the "clean" argument; this script only
# builds.

# Auto-source path.sh if present (sets PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/../path.sh" ]; then
	. "$SCRIPT_DIR/../path.sh"
fi

echo "====================== Entering HDR/mkprofiles.sh =========================="

echo "====================== Running iccFromXml Checks =========================="


# The ten BT.2100 fixtures. v5 multiProcessElement profiles, not clause 8.7.1 HDR
# Profiles: v5 is a different major version with its own tag model and is outside
# 8.7.1.1's version window. Four are narrow-range and carry the range expansion as
# an explicit curve, which is why they need no VideoFullRangeFlag handling.
iccFromXml BT2100HlgFullScene.xml BT2100HlgFullScene.icc
iccFromXml BT2100HlgNarrowScene.xml BT2100HlgNarrowScene.icc
iccFromXml BT2100HlgFullDisplay.xml BT2100HlgFullDisplay.icc
iccFromXml BT2100HlgNarrowDisplay.xml BT2100HlgNarrowDisplay.icc
iccFromXml BT2100PQFullScene.xml BT2100PQFullScene.icc
iccFromXml BT2100PQNarrowScene.xml BT2100PQNarrowScene.icc
iccFromXml BT2100PQFullDisplay.xml BT2100PQFullDisplay.icc
iccFromXml BT2100PQNarrowDisplay.xml BT2100PQNarrowDisplay.icc
iccFromXml BT2100HlgSceneToDisplayLink.xml BT2100HlgSceneToDisplayLink.icc
iccFromXml BT2100PQSceneToDisplayLink.xml BT2100PQSceneToDisplayLink.icc

# headroomAdaptiveGainCurveTag ('HAGC'). The positives cover the shapes the
# encoding takes: the full layout, the two sharing flags that delete fields from
# every alternate after the first, the raw-byte authoring path, the component
# mixing types no other fixture reaches, and the C.3.8 reference-white tone map.
# HagcInvalidXOrder is a negative -- see Testing/expected-invalid-fromxml.tsv.
iccFromXml HagcColorSpace.xml HagcColorSpace.icc
iccFromXml HagcCommonParams.xml HagcCommonParams.icc
iccFromXml HagcHexData.xml HagcHexData.icc
iccFromXml HagcMixingTypes.xml HagcMixingTypes.icc
iccFromXml HagcInvalidXOrder.xml HagcInvalidXOrder.icc
iccFromXml HagcRefWhiteToneMap.xml HagcRefWhiteToneMap.icc

# Clause 8.7.1 HDR ColorSpace Profile coverage: metadata, the baked AToB0/BToA0
# pair that makes 8.7.1.3's precedence observable, the ColourPrimaries-2 case,
# and the Linear content-headroom order of 8.7.1.4 (rules a, b and c, plus the
# HAGC-white case).  HdrInvalidTransfer, HdrMissingBToA0 and HdrMissingLutPair
# are negatives.
#
# HdrCicp2NoColumns is GONE.  Its subject was the matrix column tags that
# 8.10.1 required when ColourPrimaries was 2; the 23-09-2026 revision routes
# that case to the cicpType custom chromaticity extension of 10.3 instead, and
# a ColorSpace profile has no matrix column tags for the old rule to be about.
# HdrCicpUnspecified covers ColourPrimaries 2 on its own now.
iccFromXml HdrCicpUnspecified.xml HdrCicpUnspecified.icc
iccFromXml HdrClassDisplayNegative.xml HdrClassDisplayNegative.icc
iccFromXml HdrBakedLut.xml HdrBakedLut.icc
iccFromXml HdrInvalidTransfer.xml HdrInvalidTransfer.icc
iccFromXml HdrMissingBToA0.xml HdrMissingBToA0.icc
iccFromXml HdrMissingLutPair.xml HdrMissingLutPair.icc
iccFromXml HdrClassInputNegative.xml HdrClassInputNegative.icc
iccFromXml HdrLinearCll.xml HdrLinearCll.icc
iccFromXml HdrLinearMdcv.xml HdrLinearMdcv.icc
iccFromXml HdrLinearNoMetadata.xml HdrLinearNoMetadata.icc
iccFromXml HdrLinearHagcWhite.xml HdrLinearHagcWhite.icc
iccFromXml HdrLinearHagcCrwlDisagree.xml HdrLinearHagcCrwlDisagree.icc

# Clause 8.7.1.1 membership. Each flips exactly ONE membership condition and
# satisfies every other, so a classifier that drops one condition misclassifies
# exactly one fixture. All of them are CONFORMANT profiles that classify as
# icHdrProfileHdrContent -- failing 8.7.1.1 makes a profile not an HDR
# ColorSpace Profile, it does not make it non-conformant.
#
# THREE OF THESE INVERTED IN THE 23-09-2026 REVISION, and the inversions are
# the discriminators the revision most needs:
#   HdrColorSpaceClass  was the class negative; 'spac' is now the class the
#                       sub-class is built on, so it is the class POSITIVE and
#                       the base fixture.
#   HdrTrcTagsPresent   carried the prohibited TRC tags; 8.7 defines none for
#                       a ColorSpace profile, so there is nothing to prohibit
#                       and it is a positive (it still WARNS - a TRC tag has
#                       no interpretation on 'spac' - which is a different
#                       question from membership).
# The class condition keeps negatives on the other side: HdrClassDisplayNegative
# ('mntr') and HdrClassInputNegative ('scnr'), single-attribute deltas of the
# base.  They replace HdrDisplayMetadata and HdrInputDisplayMeta, which modelled
# the Display-class format this revision retires - the owner directed that the
# corpus model ColorSpace-class usage only.  A class NEGATIVE is not
# compatibility: it is the test that Display and Input are rejected.
iccFromXml HdrNonRgbSpace.xml HdrNonRgbSpace.icc
iccFromXml HdrColorSpaceClass.xml HdrColorSpaceClass.icc
iccFromXml HdrNoCicpTag.xml HdrNoCicpTag.icc
iccFromXml HdrTrcTagsPresent.xml HdrTrcTagsPresent.icc
iccFromXml HdrTransferSdr.xml HdrTransferSdr.icc

# 4.6.0.0, a v4 minor version above the published 4.4. It is still an HDR
# ColorSpace Profile - 8.7.1.1 sets no lower or exact v4 version - and it
# draws the validator's "Version 4 minor number is unexpected", a header
# diagnostic, not a membership one. Every other fixture is at 4.4, the
# published ICC.1 version HDR ColorSpace Profiles carry until the ICC issues
# the next edition (owner ruling 2026-10-07), so HdrColorSpaceClass itself is
# what fails a classifier still carrying a ">= 4.5" test.
# The surviving version rule is the UPPER bound - see icHdrIsVersion4() - and
# the eight v5 BT2100 fixtures are what pin that.
iccFromXml HdrVersion46.xml HdrVersion46.icc

# The v5 ceiling and the PCS condition, each isolated.  Both are single-
# attribute deltas of HdrColorSpaceClass.  Before these, the eight v5 BT2100
# fixtures failed on class AND version, and nothing failed on the PCS alone,
# so a classifier that dropped either test still read the whole manifest clean.
iccFromXml HdrVersion5.xml HdrVersion5.icc
iccFromXml HdrPcsLab.xml HdrPcsLab.icc

# The clause 8.10.5 display-headroom fixtures HdrHeadroomDcvDrwl and
# HdrHeadroomDcvCrwl are DELETED. The revision deletes the clause - physical
# display characterization is out of scope for the sub-class - so rules b) and
# c) no longer exist to be isolated.

# Clause 8.7.1.5 pairing at x = 1. HdrMissingBToA0 covers x = 0, but x = 0 is the
# pair 8.7.1.5 makes mandatory outright, so an implementation that only ever looked
# for AToB0Tag/BToA0Tag passes it. A negative.
iccFromXml HdrMissingBToA1.xml HdrMissingBToA1.icc

# IMPL-02: the VideoFullRangeFlag pair, identical in every byte but that field.
# Nothing reads the flag, so these render IDENTICALLY -- the pair pins the gap
# rather than asserting a pass.
iccFromXml HdrFullRangeFlag.xml HdrFullRangeFlag.icc
iccFromXml HdrNarrowRangeFlag.xml HdrNarrowRangeFlag.icc

# IMPL-04: the HLG OOTF luma-coefficient pair. Identical but for
# ColourPrimaries. Only a NON-NEUTRAL colour separates them - the coefficients
# sum to 1 in both sets, so a grey renders the same either way. The BT.2020
# half is the control: if it moves, that is a regression, not the fix.
iccFromXml HdrHlgBt709Primaries.xml HdrHlgBt709Primaries.icc
iccFromXml HdrHlgBt2020Primaries.xml HdrHlgBt2020Primaries.icc

echo "====================== Exiting HDR/mkprofiles.sh =========================="
