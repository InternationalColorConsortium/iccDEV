#!/bin/sh
#################################################################################
# Testing/RunTests.sh | iccDEV Project
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

# Auto-source path.sh if present (sets PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/path.sh" ]; then
	. "$SCRIPT_DIR/path.sh"
fi

echo "====================== Entering Testing/RunTests.sh =========================="

if ! command -v iccApplyNamedCmm   # print which executable is being used
then
	exit 1
fi

echo "==========================================================================="
echo "Test CalcElement Operations return of zero's indicates that something bad happened"
iccApplyNamedCmm Calc/srgbCalcTest.txt 2 0 Calc/srgbCalcTest.icc 3 sRGB_v4_ICC_preference.icc 3

echo "==========================================================================="
echo "Test NamedColor"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 sRGB_v4_ICC_preference.icc 1

echo "==========================================================================="
echo "Test NamedColor with D93 2degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D93_2deg-Abs.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with D93 10degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D93_10deg-MAT.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with D65 2degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D93_2deg-Abs.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with D65 10degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D65_10deg-MAT.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with D50 2degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D93_2deg-Abs.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with D50 10degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-D50_10deg-MAT.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with Illuminant A 2degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-IllumA_2deg-Abs.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test NamedColor with Illuminant A 10degree"
iccApplyNamedCmm Named/NamedColorTest.txt 2 0 Named/NamedColor.icc 3 -pcc PCC/Spec400_10_700-IllumA_10deg-MAT.icc PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test Grayscale GSDF Display link profile with ambient luminance of 20cd/m^2"
iccApplyNamedCmm Display/GrayTest.txt 3 0 -ENV:ambL 20 Display/GrayGSDF.icc 0

echo "==========================================================================="
echo "Test RGB GSDF Display link profile with ambient luminant of 30cd/m^2"
iccApplyNamedCmm Display/RgbTest.txt 3 0 -ENV:ambL 30 Display/RgbGSDF.icc 0

echo "==========================================================================="
echo "Test Fluorescent Color under D93"
iccApplyNamedCmm Named/FluorescentNamedColorTest.txt 2 0 Named/FluorescentNamedColor.icc 3 -pcc PCC/Spec400_10_700-D93_2deg-Abs.icc SpecRef/SixChanCameraRef.icc 1

echo "========================================="
echo "Test Fluorescent Color under D65"
iccApplyNamedCmm Named/FluorescentNamedColorTest.txt 2 0 Named/FluorescentNamedColor.icc 3 -pcc PCC/Spec400_10_700-D65_2deg-Abs.icc SpecRef/SixChanCameraRef.icc 1

echo "==========================================================================="
echo "Test Fluorescent under D50"
iccApplyNamedCmm Named/FluorescentNamedColorTest.txt 2 0 Named/FluorescentNamedColor.icc 3 -pcc PCC/Spec400_10_700-D50_10deg.icc SpecRef/SixChanCameraRef.icc 1

echo "==========================================================================="
echo "Test Fluorescent under Illuminant A"
iccApplyNamedCmm Named/FluorescentNamedColorTest.txt 2 0 Named/FluorescentNamedColor.icc 3 -pcc PCC/Spec400_10_700-IllumA_2deg-Abs.icc SpecRef/SixChanCameraRef.icc 1

echo "==========================================================================="
echo "Test Six Channel Reflectance Camera"
iccApplyNamedCmm SpecRef/sixChanTest.txt 2 0 SpecRef/SixChanCameraRef.icc 3 PCC/Spec400_10_700-D50_2deg-Abs.icc 3

echo "==========================================================================="
echo "Test Six Channel Reflectance Camera to Lab"
iccApplyNamedCmm SpecRef/sixChanTest.txt 3 0 SpecRef/SixChanCameraRef.icc 3 PCC/Lab_float-D50_2deg.icc 3

echo "==========================================================================="
echo "Test Six Channel Reflectance Camera reflectance under D93 to Lab"
iccApplyNamedCmm SpecRef/sixChanTest.txt 3 0 SpecRef/SixChanCameraRef.icc 3 -pcc PCC/Spec400_10_700-D93_2deg-Abs.icc PCC/Lab_float-D50_2deg.icc 3

echo "====================== Exiting Testing/RunTests.sh =========================="
