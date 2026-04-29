#include "IccIO.h"
#include "IccTagLut.h"

int main()
{
  CIccTagLut16 lut;
  lut.Init(3, 3);
  lut.SetColorSpaces(icSigRgbData, icSigXYZData);

  LPIccCurve *bCurves = lut.NewCurvesB();
  LPIccCurve *aCurves = lut.NewCurvesA();
  if (!bCurves || !aCurves)
    return 2;

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(0))
      return 3;
    bCurves[i] = curve;
  }

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(0))
      return 4;
    aCurves[i] = curve;
  }

  icUInt8Number grid[16] = {2, 2, 2};
  if (!lut.NewCLUT(grid, 2))
    return 5;

  CIccMemIO io;
  if (!io.Alloc(4096, true))
    return 6;

  if (!lut.Write(&io))
    return 7;

  return io.Tell() > 0 ? 0 : 8;
}
