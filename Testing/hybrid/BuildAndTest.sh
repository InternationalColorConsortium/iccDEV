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
iccApplyProfiles -exportcfg config/makeMS_smCows.json Data/smCows380_5_780.tif Results/MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC/MultSpectralRGB.icc 10003
iccApplyProfiles -exportcfg config/makeCowsA_fromRef.json Data/smCows380_5_780.tif Results/cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC/Spec400_10_700-IllumA_2deg-Abs.ICC ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles -exportcfg config/makeCowsA_fromMS.json Results/MS_smCows.tif Results/cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC/Spec400_10_700-IllumA_2deg-Abs.ICC ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles -exportcfg config/makeCowsF11_fromRef.json Data/smCows380_5_780.tif Results/cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles -exportcfg config/makeCowsF11_fromMS.json Results/MS_smCows.tif Results/cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC/Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyNamedCmm -exportcfganddata config/cmykGraysRef.json Data/cmykGrays.txt 3 1 ICC/CMYK_Hybrid_Profile.icc 10003 ICC/Spec380_10_730-D50_2deg.icc 3 > Results/cmykGraysRef.txt
iccApplySearch -exportcfganddata config/cmykGraysEst.json Results/cmykGraysRef.txt 0 1 ICC/Spec380_10_730-d50_2deg.icc 3 ICC/Lab_float-D50_2deg.icc 3 ICC/CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC/Lab_float-D50_2deg.icc 1 ICC/Lab_float-D93_2deg-MAT.icc 1 ICC/Lab_float-F11_2deg-MAT.icc 1 ICC/Lab_float-illumA_2deg-MAT.icc 1  > Results/cmykGraysEst.txt
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-W.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-W.tif 1 1 0 0 0 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-R.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-G.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-B.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-K.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUW-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 -embedded 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-W.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUS-W.tif 1 1 0 0 0 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-R.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUS-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-G.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUS-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-B.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUS-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-K.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevUS-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 ICC/CMYK-S_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevOS-W.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevOS-W.tif 1 1 0 0 0 ICC/CMYK-STop_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevOS-R.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevOS-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 ICC/CMYK-STop_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevOS-G.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevOS-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC/CMYK-STop_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevOS-B.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevOS-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 ICC/CMYK-STop_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevOS-K.json Data/TShirtDesignCMYKW.tif Results/TShirtDesignPrevOS-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 ICC/CMYK-STop_Overprint_Profile.icc 10001 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUW-G-M.json Data/TShirtDesignKW.tif Results/TShirtDesignPrevUW-G-M.tif 1 1 0 0 0 ICC/MW-Mid_Overprint.icc 80 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -ENV:0ni? 1 ICC/CMYK-W_Overprint_Profile.icc 10080 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-G-M.json Data/TShirtDesignKW.tif Results/TShirtDesignPrevUS-G-M.tif 1 1 0 0 0 ICC/MS-Mid_Overprint.icc 80 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -ENV:0ni? 1 ICC/CMYK-S_Overprint_Profile.icc 10080 ../sRGB_v4_ICC_preference.icc 1
iccapplyprofiles -exportcfg config/TShirtDesignPrevUS-W-CS.json Data/TShirtDesignKW.tif Results/TShirtDesignPrevUS-W-CS.tif 1 1 0 0 0 ICC/SC-Mid_Overprint.icc 80 -ENV:0ni? 1 ICC/CMYK-STop_Overprint_Profile.icc 10080 ../sRGB_v4_ICC_preference.icc 1
