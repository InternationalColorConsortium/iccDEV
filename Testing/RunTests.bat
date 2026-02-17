@echo off
:: Auto-call path.bat if present alongside this script (for standalone bundles)
if exist "%~dp0path.bat" call "%~dp0path.bat"
echo ===========================================================================
echo Test CalcElement Operations return of zero's indicates that something bad happened
iccApplyNamedCMM Calc\srgbCalcTest.txt 2 0 Calc\srgbCalcTest.icc 3 sRGB_v4_ICC_preference.icc 3

echo ===========================================================================
echo Test Extended CalcElement Operations return of zero's indicates that something bad happened
iccApplyNamedCMM Calc\srgbCalcTest.txt 2 0 Calc\srgbCalc++Test.icc 3 sRGB_v4_ICC_preference.icc 3


echo ===========================================================================
echo Test NamedColor
iccApplyNamedCMM Named\NamedColorTest.txt 2 0 Named\NamedColor.icc 3 sRGB_v4_ICC_preference.icc 1

echo ===========================================================================
echo Test NamedColor with D93 2degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D93_2deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with D93 10degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D93_10deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with D65 2degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D65_2deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with D65 10degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D65_10deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with D50 2degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D50_2deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with D50 10degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-D50_10deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with Illuminant A 2degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-IllumA_2deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test NamedColor with Illuminant A 10degree
iccApplyNamedCMM Named\NamedColorTest.txt 3 0 Named\NamedColor.icc 3 -pcc PCC\Spec400_10_700-IllumA_10deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test Grayscale GSDF Display link profile with ambient luminance of 20cd/m^2
iccApplyNamedCMM Display\GrayTest.txt 3 0 -ENV:ambL 20 Display\GrayGSDF.icc 0

echo ===========================================================================
echo Test RGB GSDF Display link profile with ambient luminant of 30cd/m^2
iccApplyNamedCMM Display\RgbTest.txt 3 0 -ENV:ambL 30 Display\RgbGSDF.icc 0

echo ===========================================================================
echo Test Fluorescent Color under D93
iccApplyNamedCMM Named\FluorescentNamedColorTest.txt 2 0 Named\FluorescentNamedColor.icc 3 -pcc PCC\Spec400_10_700-D93_2deg-Abs.icc SpecRef\SixChanCameraRef.icc 1

echo =========================================
echo Test Fluorescent Color under D65
iccApplyNamedCMM Named\FluorescentNamedColorTest.txt 2 0 Named\FluorescentNamedColor.icc 3 -pcc PCC\Spec400_10_700-D65_2deg-Abs.icc SpecRef\SixChanCameraRef.icc 1

echo ===========================================================================
echo Test Fluorescent under D50
iccApplyNamedCMM Named\FluorescentNamedColorTest.txt 2 0 Named\FluorescentNamedColor.icc 3 -pcc PCC\Spec400_10_700-D50_10deg-Abs.icc SpecRef\SixChanCameraRef.icc 1

echo ===========================================================================
echo Test Fluorescent under Illuminant A
iccApplyNamedCMM Named\FluorescentNamedColorTest.txt 2 0 Named\FluorescentNamedColor.icc 3 -pcc PCC\Spec400_10_700-IllumA_2deg-Abs.icc SpecRef\SixChanCameraRef.icc 1

echo ===========================================================================
echo Test Six Channel Reflectance Camera
iccApplyNamedCMM SpecRef\sixChanTest.txt 2 0 SpecRef\SixChanCameraRef.icc 3 PCC\Spec400_10_700-D50_2deg-Abs.icc 3

echo ===========================================================================
echo Test Six Channel Reflectance Camera to Lab
iccApplyNamedCMM SpecRef\sixChanTest.txt 3 0 SpecRef\SixChanCameraRef.icc 3 PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test Six Channel Reflectance Camera reflectance under D93 to Lab
iccApplyNamedCMM SpecRef\sixChanTest.txt 3 0 SpecRef\SixChanCameraRef.icc 3 -pcc PCC\Spec400_10_700-D93_2deg-Abs.icc PCC\Lab_float-D50_2deg.icc 3

echo ===========================================================================
echo Test 380_5_780 Reflectance under D50 to XYZ
iccApplyNamedCmm.exe ApplyDataFiles\cc_ref-380_5_780.txt 1 0 pcc\Spec380_5_780-D50_2deg.icc 3 pcc\XYZ_float-D50_2deg.icc 3

echo ===========================================================================
echo Test 380_10_730 Reflectance under D50 to XYZ
iccApplyNamedCmm.exe ApplyDataFiles\cc_ref-380_10_730.txt 1 0 pcc\Spec380_10_730-D50_2deg.icc 3 pcc\XYZ_float-D50_2deg.icc 3

echo ===========================================================================
echo CalcElement based aRGB profile test
iccApplyNamedCMM -debugcalc Calc\srgbCalcTest.txt 3 0 Calc\argbCalc.icc 1
