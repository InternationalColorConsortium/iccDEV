#!/bin/sh
#################################################################################
# updateprev.sh | iccMAX Project
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

echo "====================== Entering mcs/updateprev.sh =========================="

echo "====================== Updating PATH =========================="

# Properly handle newline-separated paths as a list
find ../../Build/Tools -type f -perm -111 -exec dirname {} \; | sort -u | while read -r d; do
  abs_path=$(cd "$d" && pwd)
  PATH="$abs_path:$PATH"
done

export PATH

echo "====================== Running iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc =========================="

iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc

echo "====================== Running iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc ================"

iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc

echo "====================== Running iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif ====================="

iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif 1 0 1 0 1 6ChanSelect-MID.icc 0 18ChanWithSpots-MVIS.icc 0 ..\sRGB_IEC61966_21.icm 1

echo "====================== Exiting mcs/updateprev.sh =========================="
