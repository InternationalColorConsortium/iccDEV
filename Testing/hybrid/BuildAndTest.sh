#!/bin/bash
#################################################################################
# Testing/CreateAllProfiles.sh | iccDEV Project
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

iccFromXml  MultSpectralRGB.xml ICC/MultSpectralRGB.icc
iccFromXml  LCDDisplay.xml ICC/LCDDisplay.icc
iccFromXml  CMYK_Hybrid_Profile.xml ICC/CMYK_Hybrid_Profile.icc
iccFromXml  CMYK-W_Overprint_Profile.xml ICC\CMYK-W_Overprint_Profile.icc
iccFromXml  CMYK-S_Overprint_Profile.xml ICC\CMYK-S_Overprint_Profile.icc
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
iccApplyNamedCmm Data/cmykGrays.txt 3 1 ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3 > Results/cmykGraysRef.txt
iccApplySearch  Results/cmykGraysRef.txt 0 1 ICC/Spec380_10_730-d50_2deg.icc 3 ICC/Lab_float-D50_2deg.icc 3 ICC/CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC/Lab_float-D50_2deg.icc 1 ICC/Lab_float-D93_2deg-MAT.icc 1 ICC/Lab_float-F11_2deg-MAT.icc 1 ICC/Lab_float-illumA_2deg-MAT.icc 1  > Results/cmykGraysEst.txt
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevWW.tif 1 1 0 0 0 -embedded 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevRW.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 -embedded 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevGW.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -embedded 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevBW.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 -embedded 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevKW.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 -embedded 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevWS.tif 1 1 0 0 0 ICC\CMYK-S_Overprint_Profile.icc 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevRS.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 ICC\CMYK-S_Overprint_Profile.icc 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevGS.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC\CMYK-S_Overprint_Profile.icc 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevBS.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 ICC\CMYK-S_Overprint_Profile.icc 10001 e:..\sRGB_v4_ICC_preference.icc 1
iccapplyprofiles Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevKS.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 ICC\CMYK-S_Overprint_Profile.icc 10001 e:..\sRGB_v4_ICC_preference.icc 1
