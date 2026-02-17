#!/bin/sh
#################################################################################
# updateprev.sh | iccDEV Project
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

echo "====================== Entering mcs/updateprev.sh =========================="

echo "====================== Running iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc =========================="

iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc

echo "====================== Running iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc ================"

iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc

echo "====================== Running iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif ====================="

iccApplyNamedCmm CMYKSS-Numbered-Overprint.tif prev.tif 1 0 1 0 1 6ChanSelect-MID.icc 0 18ChanWithSpots-MVIS.icc 0 ../sRGB_v4_ICC_preference.icc 1

echo "====================== Exiting mcs/updateprev.sh =========================="
