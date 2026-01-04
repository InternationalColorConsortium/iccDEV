#!/bin/bash
##
## Copyright (c) 2025 International Color Consortium. All rights reserved.
##
## Written by David Hoyt 
## Date: 21-OCT-2025 1700Z by David Hoyt
#
# Branch: PR174
# Intent: BUILD in Testing/Display
# Production: YES
# Runner: YES
#
#
# Updates: Added platform conditional
#          Fixed globbing
#
# 
# 
#  
## 

iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbColorimetric.icc 1 ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ../PCC/Spec400_10_700-D65_20yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ../PCC/Spec400_10_700-D65_40yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ../PCC/Spec400_10_700-D65_60yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ../PCC/Spec400_10_700-D65_80yo2deg-MAT.icc ../PCC/XYZ_int-D50_2deg.icc 1
