@echo off
:: Auto-call path.bat if present alongside this script (for standalone bundles)
if exist "%~dp0path.bat" call "%~dp0path.bat"
where iccFromXml
if not "%1"=="clean" goto do_begin
echo CLEANING!
:do_begin

cd Calc
if not "%1"=="clean" goto do_Calc
del /F/Q *.icc 2>NUL:
goto end_Calc
:do_Calc
@echo on
iccFromXml CameraModel.xml CameraModel.icc
iccFromXml ElevenChanKubelkaMunk.xml ElevenChanKubelkaMunk.icc
iccFromXml RGBWProjector.xml RGBWProjector.icc
iccFromXml argbCalc.xml argbCalc.icc
iccFromXml srgbCalcTest.xml srgbCalcTest.icc
iccFromXml srgbCalc++Test.xml srgbCalc++Test.icc
@echo off
REM calcImport.xml is not a standalone XML file
REM calcVars.xml is not a standalone XML file
:end_Calc

cd ..\CalcTest
if not "%1"=="clean" goto do_CalcTest
REM cannot delete *.icc as many are non-XML test files
REM del /F/Q *.icc 2>NUL:
goto end_CalcTest
:do_CalcTest
@echo on
iccFromXml calcCheckInit.xml   calcCheckInit.icc
iccFromXml calcExercizeOps.xml calcExercizeOps.icc
@echo off
:end_CalcTest

cd ..\CMYK-3DLUTs
if not "%1"=="clean" goto do_CMYK-3DLUTs
del /F/Q *.icc 2>NUL:
goto end_CMYK-3DLUTs
:do_CMYK-3DLUTs
@echo on
iccFromXml CMYK-3DLUTs.xml  CMYK-3DLUTs.icc
iccFromXml CMYK-3DLUTs2.xml CMYK-3DLUTs2.icc
@echo off
:end_CMYK-3DLUTs

cd ..\Display
if not "%1"=="clean" goto do_Display
del /F/Q *.icc 2>NUL:
goto end_Display
:do_Display
@echo on
iccFromXml GrayGSDF.xml GrayGSDF.icc
iccFromXml LCDDisplay.xml LCDDisplay.icc
iccFromXml LaserProjector.xml LaserProjector.icc
iccFromXml Rec2020rgbColorimetric.xml Rec2020rgbColorimetric.icc
iccFromXml Rec2020rgbSpectral.xml Rec2020rgbSpectral.icc
iccFromXml Rec2100HlgFull.xml Rec2100HlgFull.icc
iccFromXml Rec2100HlgNarrow.xml Rec2100HlgNarrow.icc
iccFromXml RgbGSDF.xml RgbGSDF.icc
iccFromXml sRGB_D65_MAT-300lx.xml sRGB_D65_MAT-300lx.icc
iccFromXml sRGB_D65_MAT-500lx.xml sRGB_D65_MAT-500lx.icc
iccFromXml sRGB_D65_MAT.xml       sRGB_D65_MAT.icc
iccFromXml sRGB_D65_colorimetric.xml sRGB_D65_colorimetric.icc
@echo off
:end_Display

cd ..\Encoding
if not "%1"=="clean" goto do_Encoding
del /F/Q *.icc 2>NUL:
goto end_Encoding
:do_Encoding
@echo on
iccFromXml ISO22028-Encoded-sRGB.xml ISO22028-Encoded-sRGB.icc
iccFromXml ISO22028-Encoded-bg-sRGB.xml ISO22028-Encoded-bg-sRGB.icc
iccFromXml sRgbEncoding.xml sRgbEncoding.icc
iccFromXml sRgbEncodingOverrides.xml sRgbEncodingOverrides.icc
@echo off
:end_Encoding

cd ..\ICS
if not "%1"=="clean" goto do_ICS
del /F/Q *.icc 2>NUL:
goto end_ICS
:do_ICS
@echo on
iccFromXml Lab_float-D65_2deg-Part1.xml    Lab_float-D65_2deg-Part1.icc
iccFromXml Lab_float-IllumA_2deg-Part2.xml Lab_float-IllumA_2deg-Part2.icc
iccFromXml Lab_int-D65_2deg-Part1.xml      Lab_int-D65_2deg-Part1.icc
iccFromXml Lab_int-IllumA_2deg-Part2.xml   Lab_int-IllumA_2deg-Part2.icc
iccFromXml Rec2100HlgFull-Part1.xml Rec2100HlgFull-Part1.icc
iccFromXml Rec2100HlgFull-Part2.xml Rec2100HlgFull-Part2.icc
iccFromXml Rec2100HlgFull-Part3.xml Rec2100HlgFull-Part3.icc
iccFromXml Spec400_10_700-D50_2deg-Part1.xml Spec400_10_700-D50_2deg-Part1.icc
iccFromXml Spec400_10_700-D93_2deg-Part2.xml Spec400_10_700-D93_2deg-Part2.icc
iccFromXml XYZ_float-D65_2deg-Part1.xml    XYZ_float-D65_2deg-Part1.icc
iccFromXml XYZ_float-IllumA_2deg-Part2.xml XYZ_float-IllumA_2deg-Part2.icc
iccFromXml XYZ_int-D65_2deg-Part1.xml      XYZ_int-D65_2deg-Part1.icc
iccFromXml XYZ_int-IllumA_2deg-Part2.xml   XYZ_int-IllumA_2deg-Part2.icc
@echo off
:end_ICS

cd ..\Named
if not "%1"=="clean" goto do_Named
del /F/Q *.icc 2>NUL:
goto end_Named
:do_Named
@echo on
iccFromXml FluorescentNamedColor.xml FluorescentNamedColor.icc
iccFromXml NamedColor.xml NamedColor.icc
iccFromXml SparseMatrixNamedColor.xml SparseMatrixNamedColor.icc
@echo off
:end_Named

cd ..\mcs
if not "%1"=="clean" goto do_mcs
del /F/Q *.icc 2>NUL:
goto end_mcs
:do_mcs
@echo on
iccFromXml 17ChanWithSpots-MVIS.xml 17ChanWithSpots-MVIS.icc
iccFromXml 18ChanWithSpots-MVIS.xml 18ChanWithSpots-MVIS.icc
iccFromXml 6ChanSelect-MID.xml 6ChanSelect-MID.icc
@echo off
:end_mcs

cd ..\Overprint
if not "%1"=="clean" goto do_Overprint
del /F/Q *.icc 2>NUL:
goto end_Overprint
:do_Overprint
@echo on
iccFromXml 17ChanPart1.xml 17ChanPart1.icc
@echo off
:end_Overprint

cd ..\mcs\Flexo-CMYKOGP
if not "%1"=="clean" goto do_Flexo-CMYKOGP
del /F/Q *.icc 2>NUL:
goto end_Flexo-CMYKOGP
:do_Flexo-CMYKOGP
@echo on
iccFromXml 4ChanSelect-MID.xml 4ChanSelect-MID.icc
iccFromXml 7ChanSelect-MID.xml 7ChanSelect-MID.icc
iccFromXml CGYK-SelectMID.xml  CGYK-SelectMID.icc
iccFromXml CMPK-SelectMID.xml  CMPK-SelectMID.icc
iccFromXml CMYK-SelectMID.xml  CMYK-SelectMID.icc
iccFromXml CMYKOGP-MVIS-Smooth.xml CMYKOGP-MVIS-Smooth.icc
iccFromXml OMYK-SelectMID.xml  OMYK-SelectMID.icc
@echo off
:end_Flexo-CMYKOGP

cd ..\..\PCC
if not "%1"=="clean" goto do_PCC
del /F/Q *.icc 2>NUL:
goto end_PCC
:do_PCC
@echo on
iccFromXml Lab_float-D50_2deg.xml Lab_float-D50_2deg.icc
iccFromXml Lab_float-D93_2deg-MAT.xml Lab_float-D93_2deg-MAT.icc
iccFromXml Lab_int-D50_2deg.xml Lab_int-D50_2deg.icc
iccFromXml Lab_int-D65_2deg-MAT.xml Lab_int-D65_2deg-MAT.icc
iccFromXml Lab_int-IllumA_2deg-MAT.xml Lab_int-IllumA_2deg-MAT.icc
iccFromXml Spec380_10_730-D50_2deg.xml Spec380_10_730-D50_2deg.icc
iccFromXml Spec380_10_730-D65_2deg-MAT.xml Spec380_10_730-D65_2deg-MAT.icc
iccFromXml Spec380_1_780-D50_2deg.xml Spec380_1_780-D50_2deg.icc
iccFromXml Spec380_5_780-D50_2deg.xml Spec380_5_780-D50_2deg.icc
iccFromXml Spec400_10_700-B_2deg-Abs.xml Spec400_10_700-B_2deg-Abs.icc
iccFromXml Spec400_10_700-B_2deg-CAM.xml Spec400_10_700-B_2deg-CAM.icc
iccFromXml Spec400_10_700-B_2deg-CAT02.xml Spec400_10_700-B_2deg-CAT02.icc
iccFromXml Spec400_10_700-B_2deg-MAT.xml Spec400_10_700-B_2deg-MAT.icc
iccFromXml Spec400_10_700-D50_10deg-Abs.xml Spec400_10_700-D50_10deg-Abs.icc
iccFromXml Spec400_10_700-D50_10deg-MAT.xml Spec400_10_700-D50_10deg-MAT.icc
iccFromXml Spec400_10_700-D50_20yo2deg-MAT.xml Spec400_10_700-D50_20yo2deg-MAT.icc
iccFromXml Spec400_10_700-D50_2deg-Abs.xml Spec400_10_700-D50_2deg-Abs.icc
iccFromXml Spec400_10_700-D50_2deg.xml Spec400_10_700-D50_2deg.icc
iccFromXml Spec400_10_700-D50_40yo2deg-MAT.xml Spec400_10_700-D50_40yo2deg-MAT.icc
iccFromXml Spec400_10_700-D50_60yo2deg-MAT.xml Spec400_10_700-D50_60yo2deg-MAT.icc
iccFromXml Spec400_10_700-D50_80yo2deg-MAT.xml Spec400_10_700-D50_80yo2deg-MAT.icc
iccFromXml Spec400_10_700-D65_10deg-Abs.xml Spec400_10_700-D65_10deg-Abs.icc
iccFromXml Spec400_10_700-D65_10deg-MAT.xml Spec400_10_700-D65_10deg-MAT.icc
iccFromXml Spec400_10_700-D65_20yo2deg-MAT.xml Spec400_10_700-D65_20yo2deg-MAT.icc
iccFromXml Spec400_10_700-D65_2deg-Abs.xml Spec400_10_700-D65_2deg-Abs.icc
iccFromXml Spec400_10_700-D65_2deg-MAT.xml Spec400_10_700-D65_2deg-MAT.icc
iccFromXml Spec400_10_700-D65_40yo2deg-MAT.xml Spec400_10_700-D65_40yo2deg-MAT.icc
iccFromXml Spec400_10_700-D65_60yo2deg-MAT.xml Spec400_10_700-D65_60yo2deg-MAT.icc
iccFromXml Spec400_10_700-D65_80yo2deg-MAT.xml Spec400_10_700-D65_80yo2deg-MAT.icc
iccFromXml Spec400_10_700-D93_10deg-Abs.xml Spec400_10_700-D93_10deg-Abs.icc
iccFromXml Spec400_10_700-D93_10deg-MAT.xml Spec400_10_700-D93_10deg-MAT.icc
iccFromXml Spec400_10_700-D93_2deg-Abs.xml Spec400_10_700-D93_2deg-Abs.icc
iccFromXml Spec400_10_700-D93_2deg-MAT.xml Spec400_10_700-D93_2deg-MAT.icc
iccFromXml Spec400_10_700-DB_2deg-Abs.xml Spec400_10_700-DB_2deg-Abs.icc
iccFromXml Spec400_10_700-DB_2deg-CAT02.xml Spec400_10_700-DB_2deg-CAT02.icc
iccFromXml Spec400_10_700-DB_2deg-MAT.xml Spec400_10_700-DB_2deg-MAT.icc
iccFromXml Spec400_10_700-DG_2deg-Abs.xml Spec400_10_700-DG_2deg-Abs.icc
iccFromXml Spec400_10_700-DG_2deg-CAT02.xml Spec400_10_700-DG_2deg-CAT02.icc
iccFromXml Spec400_10_700-DG_2deg-MAT.xml Spec400_10_700-DG_2deg-MAT.icc
iccFromXml Spec400_10_700-DR_2deg-Abs.xml Spec400_10_700-DR_2deg-Abs.icc
iccFromXml Spec400_10_700-DR_2deg-CAT02.xml Spec400_10_700-DR_2deg-CAT02.icc
iccFromXml Spec400_10_700-DR_2deg-MAT.xml Spec400_10_700-DR_2deg-MAT.icc
iccFromXml Spec400_10_700-F11_2deg-CAT.xml Spec400_10_700-F11_2deg-CAT.icc
iccFromXml Spec400_10_700-F11_2deg-MAT.xml Spec400_10_700-F11_2deg-MAT.icc
iccFromXml Spec400_10_700-G_2deg-Abs.xml Spec400_10_700-G_2deg-Abs.icc
iccFromXml Spec400_10_700-G_2deg-CAT02.xml Spec400_10_700-G_2deg-CAT02.icc
iccFromXml Spec400_10_700-G_2deg-MAT.xml Spec400_10_700-G_2deg-MAT.icc
iccFromXml Spec400_10_700-IllumA_10deg-Abs.xml Spec400_10_700-IllumA_10deg-Abs.icc
iccFromXml Spec400_10_700-IllumA_10deg-MAT.xml Spec400_10_700-IllumA_10deg-MAT.icc
iccFromXml Spec400_10_700-IllumA_2deg-Abs.xml Spec400_10_700-IllumA_2deg-Abs.icc
iccFromXml Spec400_10_700-IllumA_2deg-CAT.xml Spec400_10_700-IllumA_2deg-CAT.icc
iccFromXml Spec400_10_700-IllumA_2deg-MAT.xml Spec400_10_700-IllumA_2deg-MAT.icc
iccFromXml Spec400_10_700-N_2deg-Abs.xml Spec400_10_700-N_2deg-Abs.icc
iccFromXml Spec400_10_700-N_2deg-CAT02.xml Spec400_10_700-N_2deg-CAT02.icc
iccFromXml Spec400_10_700-N_2deg-MAT.xml Spec400_10_700-N_2deg-MAT.icc
iccFromXml Spec400_10_700-R1_2deg-Abs.xml Spec400_10_700-R1_2deg-Abs.icc
iccFromXml Spec400_10_700-R1_2deg-CAT02.xml Spec400_10_700-R1_2deg-CAT02.icc
iccFromXml Spec400_10_700-R1_2deg-MAT.xml Spec400_10_700-R1_2deg-MAT.icc
iccFromXml Spec400_10_700-R2_2deg-Abs.xml Spec400_10_700-R2_2deg-Abs.icc
iccFromXml Spec400_10_700-R2_2deg-CAT02.xml Spec400_10_700-R2_2deg-CAT02.icc
iccFromXml Spec400_10_700-R2_2deg-MAT.xml Spec400_10_700-R2_2deg-MAT.icc
iccFromXml Spec400_10_700-Y_2deg-Abs.xml Spec400_10_700-Y_2deg-Abs.icc
iccFromXml Spec400_10_700-Y_2deg-CAT02.xml Spec400_10_700-Y_2deg-CAT02.icc
iccFromXml Spec400_10_700-Y_2deg-MAT.xml Spec400_10_700-Y_2deg-MAT.icc
iccFromXml XYZ_float-D50_2deg.xml XYZ_float-D50_2deg.icc
iccFromXml XYZ_float-D65_2deg-MAT.xml XYZ_float-D65_2deg-MAT.icc
iccFromXml XYZ_int-D50_2deg.xml XYZ_int-D50_2deg.icc
iccFromXml XYZ_int-D65_2deg-MAT-Lvl2.xml XYZ_int-D65_2deg-MAT-Lvl2.icc
iccFromXml XYZ_int-D65_2deg-MAT.xml XYZ_int-D65_2deg-MAT.icc
@echo off
:end_PCC

cd ..\SpecRef
if not "%1"=="clean" goto do_SpecRef
del /F/Q *.icc 2>NUL:
goto end_SpecRef
:do_SpecRef
@echo on
iccFromXml argbRef.xml argbRef.icc
iccFromXml SixChanCameraRef.xml SixChanCameraRef.icc
iccFromXml SixChanInputRef.xml  SixChanInputRef.icc
iccFromXml srgbRef.xml srgbRef.icc
iccFromXml RefDecC.xml RefDecC.icc
iccFromXml RefDecH.xml RefDecH.icc
iccFromXml RefIncW.xml RefIncW.icc
@echo off
REM RefEstimationImport.xml is not a standalone XML file
:end_SpecRef

cd ..
