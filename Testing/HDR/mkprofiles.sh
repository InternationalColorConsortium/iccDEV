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
# depending on which copy was edited. Add a fixture HERE and in
# Testing/qa-profile-manifest.tsv, and nowhere else.
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


# The ten BT.2100 fixtures. v5 multiProcessElement profiles, not clause 8.10 HDR
# Profiles: v5 is a different major version with its own tag model and is outside
# 8.10.1's version window. Four are narrow-range and carry the range expansion as
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
iccFromXml HagcDisplay.xml HagcDisplay.icc
iccFromXml HagcCommonParams.xml HagcCommonParams.icc
iccFromXml HagcHexData.xml HagcHexData.icc
iccFromXml HagcMixingTypes.xml HagcMixingTypes.icc
iccFromXml HagcInvalidXOrder.xml HagcInvalidXOrder.icc
iccFromXml HagcRefWhiteToneMap.xml HagcRefWhiteToneMap.icc

# Clause 8.10 HDR Profile coverage: metadata, the baked AToB0/BToA0 pair that makes
# 8.10.3's precedence observable, the ColourPrimaries-2 pair, and the Linear
# content-headroom order of 8.10.4 (rules a, b and c, plus the HAGC-white case).
# HdrInvalidTransfer, HdrMissingBToA0, HdrMissingLutPair and HdrCicp2NoColumns are
# negatives.
iccFromXml HdrCicpUnspecified.xml HdrCicpUnspecified.icc
iccFromXml HdrDisplayMetadata.xml HdrDisplayMetadata.icc
iccFromXml HdrBakedLut.xml HdrBakedLut.icc
iccFromXml HdrInvalidTransfer.xml HdrInvalidTransfer.icc
iccFromXml HdrMissingBToA0.xml HdrMissingBToA0.icc
iccFromXml HdrMissingLutPair.xml HdrMissingLutPair.icc
iccFromXml HdrCicp2NoColumns.xml HdrCicp2NoColumns.icc
iccFromXml HdrInputDisplayMeta.xml HdrInputDisplayMeta.icc
iccFromXml HdrLinearCll.xml HdrLinearCll.icc
iccFromXml HdrLinearMdcv.xml HdrLinearMdcv.icc
iccFromXml HdrLinearNoMetadata.xml HdrLinearNoMetadata.icc
iccFromXml HdrLinearHagcWhite.xml HdrLinearHagcWhite.icc

# Clause 8.10.1 membership. Each flips exactly ONE membership condition and
# satisfies every other, so a classifier that drops one condition misclassifies
# exactly one fixture. Before these the only membership negative was
# HdrInvalidTransfer, which fails two conditions at once (non-HDR transfer AND TRC
# tags present) and so cannot say which was tested. All six are CONFORMANT
# profiles that classify as icHdrProfileHdrContent -- failing 8.10.1 makes a
# profile not an HDR Profile, it does not make it non-conformant.
iccFromXml HdrNonRgbSpace.xml HdrNonRgbSpace.icc
iccFromXml HdrColorSpaceClass.xml HdrColorSpaceClass.icc
iccFromXml HdrVersion44.xml HdrVersion44.icc
iccFromXml HdrNoCicpTag.xml HdrNoCicpTag.icc
iccFromXml HdrTrcTagsPresent.xml HdrTrcTagsPresent.icc
iccFromXml HdrTransferSdr.xml HdrTransferSdr.icc

# The other edge of the version window: 4.6.0.0 is "4.5.0.0 or later within v4",
# so this one IS an HDR Profile. The only fixture separating a correct
# ">= 4.5 and < 5" test from a wrong "== 4.5".
iccFromXml HdrVersion46.xml HdrVersion46.icc

# Clause 8.10.5 display-headroom precedence, rules b) and c). HdrDisplayMetadata
# fires rule a); these remove entries to expose the rules below it, with values
# chosen so a reader firing the wrong rule returns a different number.
iccFromXml HdrHeadroomDcvDrwl.xml HdrHeadroomDcvDrwl.icc
iccFromXml HdrHeadroomDcvCrwl.xml HdrHeadroomDcvCrwl.icc

# Clause 8.10.6 pairing at x = 1. HdrMissingBToA0 covers x = 0, but x = 0 is the
# pair 8.10.6 makes mandatory outright, so an implementation that only ever looked
# for AToB0Tag/BToA0Tag passes it. A negative.
iccFromXml HdrMissingBToA1.xml HdrMissingBToA1.icc

# IMPL-02: the VideoFullRangeFlag pair, identical in every byte but that field.
# Nothing reads the flag, so these render IDENTICALLY -- the pair pins the gap
# rather than asserting a pass.
iccFromXml HdrFullRangeFlag.xml HdrFullRangeFlag.icc
iccFromXml HdrNarrowRangeFlag.xml HdrNarrowRangeFlag.icc

echo "====================== Exiting HDR/mkprofiles.sh =========================="
