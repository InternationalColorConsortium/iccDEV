#!/bin/bash
##
## Copyright (c) 2025 International Color Consortium. All rights reserved.
##
## Written by David Hoyt
## Date: 21-OCT-2025 1700Z by David Hoyt
##
## Updated: 2026-04-09 -- add path.sh, fix -PCC to -pcc

# Auto-source path.sh if present (sets PATH and LD_LIBRARY_PATH/DYLD_LIBRARY_PATH)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$SCRIPT_DIR/../path.sh" ]; then
	. "$SCRIPT_DIR/../path.sh"
fi

iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbColorimetric.icc 1 ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -pcc ../PCC/Spec400_10_700-D65_20yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -pcc ../PCC/Spec400_10_700-D65_40yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -pcc ../PCC/Spec400_10_700-D65_60yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -pcc ../PCC/Spec400_10_700-D65_80yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
