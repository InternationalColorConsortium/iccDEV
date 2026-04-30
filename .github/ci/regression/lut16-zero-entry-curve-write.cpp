#include "IccIO.h"
#include "IccTagLut.h"

static bool set_lut_curves(CIccTagLut16 &lut, icUInt32Number nEntries)
{
  LPIccCurve *bCurves = lut.NewCurvesB();
  LPIccCurve *aCurves = lut.NewCurvesA();
  if (!bCurves || !aCurves)
    return false;

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(nEntries))
      return false;
    bCurves[i] = curve;
  }

  for (int i = 0; i < 3; i++) {
    CIccTagCurve *curve = new CIccTagCurve();
    if (!curve || !curve->SetSize(nEntries))
      return false;
    aCurves[i] = curve;
  }

  return true;
}

static int run_lut16_write_case(icUInt32Number nEntries, bool bExpectedWrite)
{
  CIccTagLut16 lut;
  lut.Init(3, 3);
  lut.SetColorSpaces(icSigRgbData, icSigXYZData);

  if (!set_lut_curves(lut, nEntries))
    return 2;

  icUInt8Number grid[16] = {2, 2, 2};
  if (!lut.NewCLUT(grid, 2))
    return 3;

  CIccMemIO io;
  if (!io.Alloc(131072, true))
    return 4;

  bool bWrote = lut.Write(&io);
  if (bWrote != bExpectedWrite)
    return bExpectedWrite ? 5 : 6;

  if (bExpectedWrite && io.Tell() == 0)
    return 7;

  return 0;
}

int main()
{
  if (run_lut16_write_case(0, false))
    return 10;
  if (run_lut16_write_case(1, false))
    return 11;
  if (run_lut16_write_case(2, true))
    return 12;
  if (run_lut16_write_case(4096, true))
    return 13;
  if (run_lut16_write_case(4097, false))
    return 14;

  return 0;
}
