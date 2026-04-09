@echo off
call checkInvalidProfiles
iccFromXml calcCheckInit.xml calcCheckInit.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcCheckInit.icc 1 >> report.txt
iccFromXml calcExercizeOps.xml calcExercizeOps.icc
iccApplyNamedCmm -debugcalc rgbExercise8bit.txt 0 1 calcExercizeOps.icc 1 >> report.txt
