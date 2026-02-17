#!/bin/sh
#################################################################################
# mkprofiles.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium. 
#                                        All rights reserved.
# 
#
#  Last Updated: 2026-02-11 16:41:15 UTC by David Hoyt
#                Remove PATH
#
#
#
#
#
# Intent: iccDEV CICD
#
#
#
#
#################################################################################

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

echo "====================== Exiting HDR/mkprofiles.sh =========================="
