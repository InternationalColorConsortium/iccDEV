#!/bin/sh
#################################################################################
# updateprevWithBkgd.sh | iccDEV Project
# Copyright (C) 2024-2026 The International Color Consortium.
#                                        All rights reserved.
#
#  Last Updated: 2026-04-09
#
# Intent: iccDEV CICD
#################################################################################

echo "====================== Entering mcs/updateprevWithBkgd.sh =========================="

echo "====================== Updating PATH =========================="

echo "====================== Running iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc =========================="

iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc

echo "====================== Running iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc =========================="

iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc

echo "====================== Running iccApplyProfiles CMYKSS-Numbered-Overprint.tif prev.tif =========================="

iccApplyProfiles CMYKSS-Numbered-Overprint.tif prev.tif 1 0 1 0 1 6ChanSelect-MID.icc 0 -ENV:bkgX 0.4014 -ENV:bkgY 0.2391 -ENV:bkgZ 0.0272 18ChanWithSpots-MVIS.icc 0 ../sRGB_v4_ICC_preference.icc 1

echo "====================== Exiting mcs/updateprevWithBkgd.sh =========================="
