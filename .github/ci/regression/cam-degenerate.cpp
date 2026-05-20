#include "IccCAM.h"

#include <cmath>

static bool is_zero_triplet(const icFloatNumber *v)
{
  return std::fabs(v[0]) < 0.000001f &&
         std::fabs(v[1]) < 0.000001f &&
         std::fabs(v[2]) < 0.000001f;
}

static bool is_finite_triplet(const icFloatNumber *v)
{
  return std::isfinite((double)v[0]) &&
         std::isfinite((double)v[1]) &&
         std::isfinite((double)v[2]);
}

int main()
{
  CIccCamConverter cam;
  cam.SetParameter_Yb(0.0f);

  icFloatNumber xyz[3] = {0.25f, 0.50f, 0.75f};
  icFloatNumber jab[3] = {-1.0f, -1.0f, -1.0f};
  cam.XYZToJab(xyz, jab, 1);

  if (!is_finite_triplet(jab))
    return 1;
  if (!is_zero_triplet(jab))
    return 2;

  icFloatNumber inputJab[3] = {50.0f, 1.0f, -1.0f};
  icFloatNumber outputXyz[3] = {-1.0f, -1.0f, -1.0f};
  cam.JabToXYZ(inputJab, outputXyz, 1);

  if (!is_finite_triplet(outputXyz))
    return 3;
  if (!is_zero_triplet(outputXyz))
    return 4;

  return 0;
}
