@echo off
call checkInvalidProfiles
@if NOT "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
iccFromXml calcCheckInit.xml calcCheckInit.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcCheckInit.icc 1 >> report.txt
iccFromXml calcExercizeOps.xml calcExercizeOps.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcExercizeOps.icc 1 >> report.txt
