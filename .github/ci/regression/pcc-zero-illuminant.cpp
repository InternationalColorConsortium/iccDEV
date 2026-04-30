#include "IccPcc.h"
#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccUtil.h"

#include <cmath>
#include <cstring>

static bool is_near(icFloatNumber a, icFloatNumber b)
{
  return std::fabs(a - b) < 0.00001f;
}

static bool is_zero_xyz(const icFloatNumber *xyz)
{
  return is_near(xyz[0], 0.0f) &&
         is_near(xyz[1], 0.0f) &&
         is_near(xyz[2], 0.0f);
}

static bool is_d50_xyz(const icFloatNumber *xyz)
{
  return is_near(xyz[0], icD50XYZ[0]) &&
         is_near(xyz[1], icD50XYZ[1]) &&
         is_near(xyz[2], icD50XYZ[2]);
}

int main()
{
  CIccProfile profile;
  profile.InitHeader();
  profile.m_Header.version = icVersionNumberV5;

  icSpectralRange emptyRange;
  std::memset(&emptyRange, 0, sizeof(emptyRange));

  CIccTagSpectralViewingConditions *view = new CIccTagSpectralViewingConditions();
  if (!view)
    return 1;

  if (!view->setIlluminant(icIlluminantCustom, emptyRange, nullptr)) {
    delete view;
    return 2;
  }

  if (!view->setObserver(icStdObsCustom, emptyRange, nullptr)) {
    delete view;
    return 3;
  }

  view->m_illuminantXYZ.X = 12.5f;
  view->m_illuminantXYZ.Y = 0.0f;
  view->m_illuminantXYZ.Z = 3.75f;

  if (!profile.AttachTag(icSigSpectralViewingConditionsTag, view)) {
    delete view;
    return 4;
  }

  icFloatNumber xyz[3] = {-1.0f, -1.0f, -1.0f};
  profile.getNormIlluminantXYZ(xyz);
  if (!is_zero_xyz(xyz))
    return 5;
  if (is_d50_xyz(xyz))
    return 6;

  CIccCombinedConnectionConditions combined(&profile, &profile, true);
  xyz[0] = xyz[1] = xyz[2] = -1.0f;
  combined.getNormIlluminantXYZ(xyz);
  if (!is_zero_xyz(xyz))
    return 7;

  icFloatNumber mediaXYZ[3] = {-1.0f, -1.0f, -1.0f};
  if (combined.getMediaWhiteXYZ(mediaXYZ))
    return 8;

  return 0;
}
