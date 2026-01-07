#!/bin/sh
#################################################################################
# updateprevWithBkgd.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium. 
#                                        All rights reserved.
# 
#
#  Last Updated: Wed Jan 7 03:27:11 AM UTC 2026 by David Hoyt
#
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

echo "====================== Entering mcs/updateprevWithBkgd.sh =========================="

echo "====================== Updating PATH =========================="

# Properly handle newline-separated paths as a list
find ../../Build/Tools -type f -perm -111 -exec dirname {} \; | sort -u | while read -r d; do
  abs_path=$(cd "$d" && pwd)
  PATH="$abs_path:$PATH"
done

export PATH

echo "====================== Running iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc =========================="

iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc

echo "====================== Running iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc =========================="

iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc

echo "====================== Running iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif =========================="

iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif 1 0 1 0 1 6ChanSelect-MID.icc 0 -ENV:bkgX 0.4014 -ENV:bkgY 0.2391 -ENV:bkgZ 0.0272 18ChanWithSpots-MVIS.icc 0 ../sRGB_v4_ICC_preference.icc 1

echo "====================== Exiting mcs/updateprevWithBkgd.sh =========================="
