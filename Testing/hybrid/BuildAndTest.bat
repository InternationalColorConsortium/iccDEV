@REM setup directory to the tools used in this script
@if exist ..\iccFromXml.exe (SET TOOLDIR=..\) else (SET TOOLDIR=)

@ECHO First lets build some useful ICC profiles 

%TOOLDIR%iccFromXml.exe MultSpectralRGB.xml ICC\MultSpectralRGB.icc
%TOOLDIR%iccFromXml.exe LCDDisplay.xml ICC\LCDDisplay.icc
%TOOLDIR%iccFromXml.exe CMYK_Hybrid_Profile.xml ICC\CMYK_Hybrid_Profile.icc

%TOOLDIR%iccFromXml.exe Data\Lab_float-D50_2deg.xml ICC\Lab_float-D50_2deg.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-D93_2deg-MAT.xml ICC\Lab_float-D93_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-F11_2deg-MAT.xml ICC\Lab_float-F11_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-illumA_2deg-MAT.xml ICC\Lab_float-illumA_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Cat8Lab-D65_2deg.xml ICC\Cat8Lab-D65_2deg.icc

%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-D50_2deg.xml ICC\Spec400_10_700-D50_2deg.icc
%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-illumA_2deg-Abs.xml ICC\Spec400_10_700-illumA_2deg-Abs.icc
%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-F11_2deg-Abs.xml ICC\Spec400_10_700-F11_2deg-Abs.icc
%TOOLDIR%iccFromXml.exe Data\Spec380_10_730-D50_2deg.xml ICC\Spec380_10_730-D50_2deg.icc

@ECHO ************************************************
@ECHO Make a multi-spectral image from a spectral one
@ECHO ************************************************

%TOOLDIR%iccTiffDump.exe Data\smCows380_5_780.tif

%TOOLDIR%iccApplyProfiles.exe Data\smCows380_5_780.tif Results\MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC\MultSpectralRGB.icc 10003

%TOOLDIR%iccTiffDump.exe Results\MS_smCows.tif

@ECHO *****************************************************************
@ECHO Apply PCC's to the spectral images to get colorimetric renderings
@ECHO *****************************************************************

%TOOLDIR%iccApplyProfiles.exe Data\smCows380_5_780.tif Results\cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe Results\MS_smCows.tif Results\cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe Data\smCows380_5_780.tif Results\cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe Results\MS_smCows.tif Results\cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1

@ECHO *****************************************************************
@ECHO Apply custom observer
@ECHO *****************************************************************

%TOOLDIR%iccDumpProfile ICC\LCDDisplay.icc

%TOOLDIR%iccV5DspObsToV4Dsp.exe ICC\LCDDisplay.icc ICC\Cat8Lab-D65_2deg.icc Results\LCDDisplayCat8Obs.icc

%TOOLDIR%iccDumpProfile Results\LCDDisplayCat8Obs.icc

@ECHO *****************************************************************
@ECHO Do some spectral color management from an hybrid print profile
@ECHO *****************************************************************

@type Data\cmykGrays.txt

%TOOLDIR%iccApplyNamedCmm.exe Data\cmykGrays.txt 3 1 ICC\CMYK_Hybrid_Profile.icc 10003 ICC\Spec380_10_730-D50_2deg.icc 3 > Results\cmykGraysRef.txt

@type Results\cmykGraysRef.txt

%TOOLDIR%iccApplySearch.exe Results\cmykGraysRef.txt 0 1 ICC\Spec380_10_730-d50_2deg.icc 3 ICC\Lab_float-D50_2deg.icc 3 ICC\CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC\Lab_float-D50_2deg.icc 1 ICC\Lab_float-D93_2deg-MAT.icc 1 ICC\Lab_float-F11_2deg-MAT.icc 1 ICC\Lab_float-illumA_2deg-MAT.icc 1 > Results\cmykGraysEst.txt

@type Results\cmykGraysEst.txt
