#!/bin/bash
#################################################################################
# Testing/CreateAllProfiles.sh | iccMAX Project
# Copyright (C) 2024-2025 The International Color Consortium. 
#                                        All rights reserved.
# 
#
#  Last Updated: 20-OCT-2025 by David Hoyt
#
# file Location: Testing/hybrid
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

iccFromXml  MultSpectralRGB.xml ICC/MultSpectralRGB.icc
iccFromXml  LCDDisplay.xml ICC/LCDDisplay.icc
iccFromXml  UCD_candidate_Hybrid.xml ICC/UCD_candidate_Hybrid.icc
iccFromXml   Data/Lab_float-D50_2deg.xml ICC/Lab_float-D50_2deg.icc
iccFromXml   Data/Lab_float-D93_2deg-MAT.xml ICC/Lab_float-D93_2deg-MAT.icc
iccFromXml   Data/Lab_float-F11_2deg-MAT.xml ICC/Lab_float-F11_2deg-MAT.icc
iccFromXml   Data/Lab_float-IllumA_2deg-MAT.xml ICC/Lab_float-illumA_2deg-MAT.icc
iccFromXml   Data/Spec400_10_700-D50_2deg.xml ICC/Spec400_10_700-D50_2deg.icc
iccFromXml   Data/Spec400_10_700-IllumA_2deg-Abs.xml ICC/Spec400_10_700-IllumA_2deg-Abs.ICC
iccFromXml   Data/Spec400_10_700-F11_2deg-Abs.xml ICC/Spec400_10_700-F11_2deg-Abs.icc
iccFromXml   Data/Spec380_10_730-D50_2deg.xml ICC/Spec380_10_730-D50_2deg.icc
iccTiffDump   Data/smCows380_5_780.tif
iccApplyProfiles Data/smCows380_5_780.tif Results/MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC/MultSpectralRGB.icc 10003
iccApplyProfiles Data/smCows380_5_780.tif Results/cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC/Spec400_10_700-IllumA_2deg-Abs.ICC ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles Results/MS_smCows.tif Results/cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC/Spec400_10_700-IllumA_2deg-Abs.ICC ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles   Data/smCows380_5_780.tif Results/cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles  Results/MS_smCows.tif Results/cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyNamedCmm Data/cmykGrays.txt 3 1 ICC/UCD_candidate_Hybrid.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3 > Results/cmykGraysRef.txt
iccApplySearch  Results/cmykGraysRef.txt 0 1 ICC/Spec380_10_730-d50_2deg.icc 3 ICC/Lab_float-D50_2deg.icc 3 ICC/UCD_candidate_Hybrid.icc 10003 -INIT 3 ICC/Lab_float-D50_2deg.icc 1 ICC/Lab_float-D93_2deg-MAT.icc 1 ICC/Lab_float-F11_2deg-MAT.icc 1 ICC/Lab_float-illumA_2deg-MAT.icc 1  > Results/cmykGraysEst.txt
