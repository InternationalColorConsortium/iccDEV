@ECHO First lets build some ICC ICC

iccFromXml.exe MultSpectralRGB.xml ICC\MultSpectralRGB.icc
iccFromXml.exe LCDDisplay.xml ICC\LCDDisplay.icc
iccFromXml.exe UCD_candidate_hybrid.xml ICC\UCD_candidate_hybrid.icc

iccFromXml.exe Data\Lab_float-D50_2deg.xml ICC\Lab_float-D50_2deg.icc
iccFromXml.exe Data\Lab_float-D93_2deg-MAT.xml ICC\Lab_float-D93_2deg-MAT.icc
iccFromXml.exe Data\Lab_float-F11_2deg-MAT.xml ICC\Lab_float-F11_2deg-MAT.icc
iccFromXml.exe Data\Lab_float-illumA_2deg-MAT.xml ICC\Lab_float-illumA_2deg-MAT.icc

iccFromXml.exe Data\Spec400_10_700-D50_2deg.xml ICC\Spec400_10_700-D50_2deg.icc
iccFromXml.exe Data\Spec400_10_700-illumA_2deg-Abs.xml ICC\Spec400_10_700-illumA_2deg-Abs.icc
iccFromXml.exe Data\Spec400_10_700-F11_2deg-Abs.xml ICC\Spec400_10_700-F11_2deg-Abs.icc
iccFromXml.exe Data\Spec380_10_730-D50_2deg.xml ICC\Spec380_10_730-D50_2deg.icc

@ECHO ************************************************
@ECHO Make a multi-spectral image from a spectral one
@ECHO ************************************************

iccTiffDump.exe Data\smCows380_5_780.tif

iccApplyProfiles.exe Data\smCows380_5_780.tif Results\MS_smCows.tif 2 1 0 1 1 -embedded 3 ICC\MultSpectralRGB.icc 10003

iccTiffDump.exe Results\MS_smCows.tif

@ECHO *****************************************************************
@ECHO Apply PCC's to the spectral images to get colorimetric renderings
@ECHO *****************************************************************

iccApplyProfiles.exe Data\smCows380_5_780.tif Results\cowsA_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles.exe Results\MS_smCows.tif Results\cowsA_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-illumA_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles.exe Data\smCows380_5_780.tif Results\cowsF11_fromRef.tif 1 1 0 1 1 -embedded 3 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1
iccApplyProfiles.exe Results\MS_smCows.tif Results\cowsF11_fromMS.tif 1 1 0 1 1 -embedded 10003 -pcc ICC\Spec400_10_700-F11_2deg-Abs.icc ../sRGB_v4_ICC_preference.icc 1

@ECHO *****************************************************************
@ECHO Do some spectral color management from an extended print ICC
@ECHO *****************************************************************

iccApplyNamedCmm.exe Data\cmykGrays.txt 3 1 ICC\UCD_candidate_hybrid.icc 10003 ICC\Spec380_10_730-D50_2deg.icc 3 > Results\cmykGraysRef.txt

@type Results\cmykGraysRef.txt

iccApplySearch.exe Results\cmykGraysRef.txt 0 1 ICC\Spec380_10_730-d50_2deg.icc 3 ICC\Lab_float-D50_2deg.icc 3 ICC\UCD_candidate_hybrid.icc 10003 -INIT 3 ICC\Lab_float-D50_2deg.icc 1 ICC\Lab_float-D93_2deg-MAT.icc 1 ICC\Lab_float-F11_2deg-MAT.icc 1 ICC\Lab_float-illumA_2deg-MAT.icc 1 > Results\cmykGraysEst.txt

@type Results\cmykGraysEst.txt
