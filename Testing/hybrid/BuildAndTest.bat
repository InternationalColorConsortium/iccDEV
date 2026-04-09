@REM setup directory to the tools used in this script
@if exist ..\iccFromXml.exe (SET TOOLDIR=..\) else (SET TOOLDIR=)

@if not exist ICC mkdir ICC
@if not exist Results mkdir Results
@if not exist config mkdir config

@ECHO First lets build some useful ICC profiles 

%TOOLDIR%iccFromXml.exe MultSpectralRGB.xml ICC\MultSpectralRGB.icc
%TOOLDIR%iccFromXml.exe LCDDisplay.xml ICC\LCDDisplay.icc
%TOOLDIR%iccFromXml.exe CMYK_Hybrid_Profile.xml ICC\CMYK_Hybrid_Profile.icc
%TOOLDIR%iccFromXml.exe CMYK-W_Overprint_Profile.xml ICC\CMYK-W_Overprint_Profile.icc
%TOOLDIR%iccFromXml.exe CMYK-S_Overprint_Profile.xml ICC\CMYK-S_Overprint_Profile.icc
%TOOLDIR%iccFromXml.exe CMYK-STop_Overprint_Profile.xml ICC\CMYK-STop_Overprint_Profile.icc
%TOOLDIR%iccFromXml.exe MW-Mid_Overprint.xml ICC\MW-Mid_Overprint.icc
%TOOLDIR%iccFromXml.exe MS-Mid_Overprint.xml ICC\MS-Mid_Overprint.icc
%TOOLDIR%iccFromXml.exe SC-Mid_Overprint.xml ICC\SC-Mid_Overprint.icc

%TOOLDIR%iccFromXml.exe Data\Lab_float-D50_2deg.xml ICC\Lab_float-D50_2deg.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-D93_2deg-MAT.xml ICC\Lab_float-D93_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-F11_2deg-MAT.xml ICC\Lab_float-F11_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Lab_float-IllumA_2deg-MAT.xml ICC\Lab_float-IllumA_2deg-MAT.icc
%TOOLDIR%iccFromXml.exe Data\Cat8Lab-D65_2deg.xml ICC\Cat8Lab-D65_2deg.icc

%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-D50_2deg.xml ICC\Spec400_10_700-D50_2deg.icc
%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-IllumA_2deg-Abs.xml ICC\Spec400_10_700-IllumA_2deg-Abs.icc
%TOOLDIR%iccFromXml.exe Data\Spec400_10_700-F11_2deg-Abs.xml ICC\Spec400_10_700-F11_2deg-Abs.icc
%TOOLDIR%iccFromXml.exe Data\Spec380_10_730-D50_2deg.xml ICC\Spec380_10_730-D50_2deg.icc

@ECHO ************************************************
@ECHO Make a multi-spectral image from a spectral one
@ECHO ************************************************

%TOOLDIR%iccTiffDump.exe Data\smCows380_5_780.tif

%TOOLDIR%iccApplyProfiles.exe -exportcfg config\makeMS_smCows.json Data\smCows380_5_780.tif Results\MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC\MultSpectralRGB.icc 10003

%TOOLDIR%iccTiffDump.exe Results\MS_smCows.tif

@ECHO *****************************************************************
@ECHO Apply PCC's to the spectral images to get colorimetric renderings
@ECHO *****************************************************************

%TOOLDIR%iccApplyProfiles.exe -exportcfg config\makeCowsA_fromRef.json Data\smCows380_5_780.tif Results\cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe -exportcfg config\makeCowsA_fromMS.json Results\MS_smCows.tif Results\cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe -exportcfg config\makeCowsF11_fromRef.json Data\smCows380_5_780.tif Results\cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccApplyProfiles.exe -exportcfg config\makeCowsF11_fromMS.json Results\MS_smCows.tif Results\cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ..\sRGB_v4_ICC_preference.icc 1

@ECHO *****************************************************************
@ECHO Apply custom observer
@ECHO *****************************************************************

%TOOLDIR%iccDumpProfile.exe ICC\LCDDisplay.icc

%TOOLDIR%iccV5DspObsToV4Dsp.exe ICC\LCDDisplay.icc ICC\Cat8Lab-D65_2deg.icc Results\LCDDisplayCat8Obs.icc

%TOOLDIR%iccDumpProfile.exe Results\LCDDisplayCat8Obs.icc

@ECHO *****************************************************************
@ECHO Do some spectral color management from an hybrid print profile
@ECHO *****************************************************************

@type Data\cmykGrays.txt

%TOOLDIR%iccApplyNamedCmm.exe -exportcfganddata config/cmykGraysRef.json Data\cmykGrays.txt 3 1 ICC\CMYK_Hybrid_Profile.icc 10003 ICC\Spec380_10_730-D50_2deg.icc 3 > Results\cmykGraysRef.txt

@type Results\cmykGraysRef.txt

%TOOLDIR%iccApplySearch.exe -exportcfganddata config/cmykGraysEst.json Results\cmykGraysRef.txt 0 1 ICC\Spec380_10_730-d50_2deg.icc 3 ICC\Lab_float-D50_2deg.icc 3 ICC\CMYK_Hybrid_Profile.icc 10003 -INIT 3 ICC\Lab_float-D50_2deg.icc 1 ICC\Lab_float-D93_2deg-MAT.icc 1 ICC\Lab_float-F11_2deg-MAT.icc 1 ICC\Lab_float-illumA_2deg-MAT.icc 1 > Results\cmykGraysEst.txt

@type Results\cmykGraysEst.txt

@ECHO *****************************************************************
@ECHO Apply Overprint simulation to CMYKW image to get background previews
@ECHO *****************************************************************

%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-W.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUW-W.tif 1 1 0 0 0 -embedded 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-R.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUW-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 -embedded 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-G.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUW-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -embedded 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-B.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUW-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 -embedded 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-K.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUW-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 -embedded 10001 ..\sRGB_v4_ICC_preference.icc 1

%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-W.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUS-W.tif 1 1 0 0 0 ICC\CMYK-S_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-R.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUS-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 ICC\CMYK-S_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-G.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUS-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC\CMYK-S_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-B.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUS-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 ICC\CMYK-S_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-K.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevUS-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 ICC\CMYK-S_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1

%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevOS-W.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevOS-W.tif 1 1 0 0 0 ICC\CMYK-STop_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevOS-R.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevOS-R.tif 1 1 0 0 0 -ENV:bkgX 0.264 -ENV:bkgY 0.168 -ENV:bkgZ 0.033 ICC\CMYK-STop_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevOS-G.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevOS-G.tif 1 1 0 0 0 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 ICC\CMYK-STop_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevOS-B.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevOS-B.tif 1 1 0 0 0 -ENV:bkgX 0.2099 -ENV:bkgY 0.182 -ENV:bkgZ 0.498 ICC\CMYK-STop_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevOS-K.json Data\TShirtDesignCMYKW.tif Results\TShirtDesignPrevOS-K.tif 1 1 0 0 0 -ENV:bkgX 0 -ENV:bkgY 0 -ENV:bkgZ 0 ICC\CMYK-STop_Overprint_Profile.icc 10001 ..\sRGB_v4_ICC_preference.icc 1

%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUW-G-M.json Data\TShirtDesignKW.tif Results\TShirtDesignPrevUW-G-M.tif 1 1 0 0 0 ICC\MW-Mid_Overprint.icc 80 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -ENV:0ni? 1 ICC\CMYK-W_Overprint_Profile.icc 10080 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-G-M.json Data\TShirtDesignKW.tif Results\TShirtDesignPrevUS-G-M.tif 1 1 0 0 0 ICC\MS-Mid_Overprint.icc 80 -ENV:bkgX 0.0985 -ENV:bkgY 0.159 -ENV:bkgZ 0.122 -ENV:0ni? 1 ICC\CMYK-S_Overprint_Profile.icc 10080 ..\sRGB_v4_ICC_preference.icc 1
%TOOLDIR%iccapplyprofiles -exportcfg config\TShirtDesignPrevUS-W-CS.json Data\TShirtDesignKW.tif Results\TShirtDesignPrevUS-W-CS.tif 1 1 0 0 0 ICC\SC-Mid_Overprint.icc 80 -ENV:0ni? 1 ICC\CMYK-STop_Overprint_Profile.icc 10080 ..\sRGB_v4_ICC_preference.icc 1
