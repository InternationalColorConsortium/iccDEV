#!/bin/sh
#################################################################################
# mkprofiles.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-04-09
#
# Intent: iccDEV CICD
#################################################################################

# Auto-source path.sh if present (sets PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/../path.sh" ]; then
	. "$SCRIPT_DIR/../path.sh"
fi

echo "====================== Entering HDR/mkprofiles.sh =========================="

echo "====================== Running iccFromXml Checks =========================="

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

# headroomAdaptiveGainCurveTag ('HAGC') coverage. The four positives cover the
# four shapes the encoding takes: the full layout, the two sharing flags that
# delete fields from every alternate after the first, the raw-byte authoring
# path, and the component mixing types no other fixture reaches -- type 3 with a
# partly authored coefficient array and type 1, which serializes none at all.
# HagcInvalidXOrder is a negative -- see Testing/expected-invalid-fromxml.tsv.
iccFromXml HagcDisplay.xml HagcDisplay.icc
iccFromXml HagcCommonParams.xml HagcCommonParams.icc
iccFromXml HagcHexData.xml HagcHexData.icc
iccFromXml HagcMixingTypes.xml HagcMixingTypes.icc
iccFromXml HagcInvalidXOrder.xml HagcInvalidXOrder.icc

# Clause 8.10 HDR Profile coverage: a conforming profile carrying HDR Image and
# HDR Display metadata but no tone-mapping descriptor, one whose only descriptor
# is a baked AToB0/BToA0 pair (which is what makes the 8.10.3 precedence
# observable), and two negatives for the TransferCharacteristics restriction and
# the AToBx/BToAx pairing rule.
iccFromXml HdrCicpUnspecified.xml HdrCicpUnspecified.icc
iccFromXml HdrDisplayMetadata.xml HdrDisplayMetadata.icc
iccFromXml HdrBakedLut.xml HdrBakedLut.icc
iccFromXml HdrInvalidTransfer.xml HdrInvalidTransfer.icc
iccFromXml HdrMissingBToA0.xml HdrMissingBToA0.icc

echo "====================== Exiting HDR/mkprofiles.sh =========================="
