#!/bin/sh
#################################################################################
# mkprofiles.sh | iccMAX Project
# Copyright (C) 2024-2025 The International Color Consortium. 
#                                        All rights reserved.
# 
#
#  Last Updated: Mon Mar 24 16:40:19 EDT 2025 by David Hoyt
#  date -d @1742848819
#  Mon Mar 24 16:40:19 EDT 2025
#
#
#
#
# Intent: iccMAX CICD
#
#
#
#
#################################################################################

echo "====================== Entering HDR/mkprofiles.sh =========================="

# Properly handle newline-separated paths as a list
find ../../Build/Tools -type f -perm -111 -exec dirname {} \; | sort -u | while read -r d; do
  abs_path=$(cd "$d" && pwd)
  PATH="$abs_path:$PATH"
done

export PATH


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
