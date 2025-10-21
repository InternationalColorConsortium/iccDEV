@echo off

echo ===========================================================================
echo Colorimetric Rec2020 profile test
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbColorimetric.icc 1 ..\PCC\XYZ_int-D50_2deg.icc 1

echo ===========================================================================
echo Spectral Rec2020 profile test with standard 2-degree observer
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 ..\PCC\XYZ_int-D50_2deg.icc 1

echo ===========================================================================
echo Spectral Rec2020 profile test with 20 year old 2-degree observer
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ..\PCC\Spec400_10_700-D65_20yo2deg-MAT.icc ..\PCC\XYZ_int-D50_2deg.icc 1

echo ===========================================================================
echo Spectral Rec2020 profile test with 40 year old 2-degree observer
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ..\PCC\Spec400_10_700-D65_40yo2deg-MAT.icc ..\PCC\XYZ_int-D50_2deg.icc 1

echo ===========================================================================
echo Spectral Rec2020 profile test with 60 year old 2-degree observer
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ..\PCC\Spec400_10_700-D65_60yo2deg-MAT.icc ..\PCC\XYZ_int-D50_2deg.icc 1

echo ===========================================================================
echo Spectral Rec2020 profile test with 80 year old 2-degree observer
iccApplyNamedCmm RgbTest.txt 3 0 Rec2020rgbSpectral.icc 1 -PCC ..\PCC\Spec400_10_700-D65_80yo2deg-MAT.icc ..\PCC\XYZ_int-D50_2deg.icc 1
