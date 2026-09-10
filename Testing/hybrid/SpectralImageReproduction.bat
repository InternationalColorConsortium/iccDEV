@REM ###############################################################################
@REM Testing/hybrid/SpectralImageReproduction.bat | iccDEV Project
@REM Copyright (C) 2024-2026 The International Color Consortium.
@REM                                        All rights reserved.
@REM
@REM Reproduce the FULL 600x420 multispectral cows image into the hybrid CMYK
@REM printer profile by spectral inverse search, under the same four observing
@REM conditions (D93 / Illuminant A / D50 / F11) the cmykGrays search uses.
@REM
@REM Deliberately NOT part of BuildAndTest.bat: every pixel runs a Nelder-Mead
@REM search across four weighted PCCs, so the full image costs minutes rather
@REM than the seconds the 92x64 icon step costs.  Run BuildAndTest.bat first --
@REM it builds ICC/ and produces Results/MS_smCows.tif, the input consumed here.
@REM ###############################################################################

@REM setup directory to the tools used in this script
@if exist ..\iccFromXml.exe (SET TOOLDIR=..\) else (SET TOOLDIR=)

@if not exist Results\MS_smCows.tif (
  @ECHO missing Results\MS_smCows.tif -- run BuildAndTest.bat first
  @EXIT /B 1
)

@ECHO *****************************************************************
@ECHO Spectral image reproduction: full MS cows to hybrid CMYK by inverse search
@ECHO This runs an inverse search per pixel; expect minutes, not seconds.
@ECHO *****************************************************************

%TOOLDIR%iccApplyProfiles -cfg config/msCowsToCmyk.json
%TOOLDIR%iccTiffDump Results\MS_smCowsCmyk.tif

@ECHO Wrote Results\MS_smCowsCmyk.tif
