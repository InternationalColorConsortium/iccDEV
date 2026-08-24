/*
 * Copyright (c) 2026 International Color Consortium.
 * All rights reserved.
 *
 * This software is provided under the BSD 3-Clause License.
 */

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"
#include "IccQualityMetrics.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace iccquality;

static int g_failures = 0;

static void check(bool condition, const char *message)
{
  if (condition) {
    std::printf("ok:   %s\n", message);
  }
  else {
    std::printf("FAIL: %s\n", message);
    ++g_failures;
  }
}

static void attach_xyz(CIccProfile &profile, icSignature signature,
                       double x, double y, double z)
{
  CIccTagXYZ *tag = new CIccTagXYZ;
  (*tag)[0].X = icDtoF(x);
  (*tag)[0].Y = icDtoF(y);
  (*tag)[0].Z = icDtoF(z);
  profile.AttachTag(signature, tag);
}

static void build_rgb_matrix_profile(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.deviceClass = icSigDisplayClass;
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigXYZData;
  profile.m_Header.version = icVersionNumberV4_3;

  attach_xyz(profile, icSigRedMatrixColumnTag, 0.4361, 0.2225, 0.0139);
  attach_xyz(profile, icSigGreenMatrixColumnTag, 0.3851, 0.7169, 0.0971);
  attach_xyz(profile, icSigBlueMatrixColumnTag, 0.1431, 0.0606, 0.7141);
  attach_xyz(profile, icSigMediaWhitePointTag, 0.9642, 1.0, 0.8249);

  profile.AttachTag(icSigRedTRCTag, new CIccTagCurve(0));
  profile.AttachTag(icSigGreenTRCTag, new CIccTagCurve(0));
  profile.AttachTag(icSigBlueTRCTag, new CIccTagCurve(0));
}

static void build_gray_matrix_profile(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.deviceClass = icSigDisplayClass;
  profile.m_Header.colorSpace = icSigGrayData;
  profile.m_Header.pcs = icSigXYZData;
  profile.m_Header.version = icVersionNumberV4_3;
  attach_xyz(profile, icSigMediaWhitePointTag, 0.9642, 1.0, 0.8249);
  profile.AttachTag(icSigGrayTRCTag, new CIccTagCurve(0));
}

static CIccTagLut16 *make_constant_lut(int inputChannels, int outputChannels,
                                      icColorSpaceSignature inputSpace,
                                      icColorSpaceSignature outputSpace,
                                      icFloatNumber value)
{
  CIccTagLut16 *lut = new CIccTagLut16;
  lut->Init(inputChannels, outputChannels);
  lut->SetColorSpaces(inputSpace, outputSpace);

  LPIccCurve *inputCurves = lut->NewCurvesB();
  LPIccCurve *outputCurves = lut->NewCurvesA();
  if (!inputCurves || !outputCurves) {
    delete lut;
    return nullptr;
  }
  for (int i = 0; i < inputChannels; ++i) {
    inputCurves[i] = new CIccTagCurve(0);
  }
  for (int i = 0; i < outputChannels; ++i) {
    outputCurves[i] = new CIccTagCurve(0);
  }

  icUInt8Number grid[16] = {};
  for (int i = 0; i < inputChannels; ++i) {
    grid[i] = 2;
  }
  if (!lut->NewCLUT(grid, 2)) {
    delete lut;
    return nullptr;
  }

  CIccCLUT *clut = lut->GetCLUT();
  icFloatNumber *data = clut->GetData(0);
  const icUInt32Number count = clut->NumPoints() * outputChannels;
  for (icUInt32Number i = 0; i < count; ++i) {
    data[i] = value;
  }
  return lut;
}

static void build_cmyk_classic_profile(CIccProfile &profile)
{
  profile.InitHeader();
  profile.m_Header.deviceClass = icSigOutputClass;
  profile.m_Header.colorSpace = icSigCmykData;
  profile.m_Header.pcs = icSigLabData;
  profile.m_Header.version = icVersionNumberV4_3;
  profile.AttachTag(icSigAToB1Tag,
                    make_constant_lut(4, 3, icSigCmykData, icSigLabData, 0.5f));
  profile.AttachTag(icSigBToA1Tag,
                    make_constant_lut(3, 4, icSigLabData, icSigCmykData, 0.5f));
}

static void test_sample_budget()
{
  size_t samples = 0;
  check(bounded_grid_sample_count(1, samples) && samples == 9,
        "Gray grid uses 9 samples");
  check(bounded_grid_sample_count(3, samples) && samples == 729,
        "RGB grid uses 729 samples");
  check(bounded_grid_sample_count(4, samples) && samples == 625,
        "CMYK grid uses 625 samples");
  check(bounded_grid_sample_count(9, samples) && samples == 1953125,
        "largest accepted bounded grid stays below the sample budget");
  check(!bounded_grid_sample_count(10, samples),
        "10-channel grid is rejected before exponential evaluation");
  check(!bounded_grid_sample_count(16, samples),
        "16-channel grid is rejected before exponential evaluation");

  CIccProfile highChannel;
  highChannel.InitHeader();
  highChannel.m_Header.deviceClass = icSigOutputClass;
  highChannel.m_Header.colorSpace = icSig10colorData;
  highChannel.m_Header.pcs = icSigLabData;
  highChannel.m_Header.version = icVersionNumberV4_3;
  highChannel.AttachTag(icSigAToB1Tag, new CIccTagLut16);
  highChannel.AttachTag(icSigBToA1Tag, new CIccTagLut16);

  RoundTripMetrics metrics;
  std::string reason;
  check(!measure_cmm_round_trip(&highChannel, metrics, reason) &&
          reason.find("sample grid exceeds quality metric budget") !=
            std::string::npos,
        "general CMM evaluator rejects an over-budget profile before iteration");
}

static void test_ciede2000_vectors()
{
  const icFloatNumber lab1[][3] = {
    {50.0000f, 2.6772f, -79.7751f},
    {50.0000f, 3.1571f, -77.2803f},
    {50.0000f, 2.8361f, -74.0200f},
    {50.0000f, -1.3802f, -84.2814f},
    {50.0000f, -1.1848f, -84.8006f},
    {50.0000f, -0.9009f, -85.5211f},
  };
  const icFloatNumber lab2[3] = {50.0000f, 0.0000f, -82.7485f};
  const double expected[] = {2.0425, 2.8615, 3.4412, 1.0000, 1.0000, 1.0000};

  for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
    const double actual = delta_e_2000(lab1[i], lab2);
    check(std::fabs(actual - expected[i]) < 5e-5,
          "CIEDE2000 reference vector matches");
  }
}

static void check_round_trip_model(CIccProfile &profile,
                                   const char *expectedModel,
                                   int expectedSamples,
                                   const char *message)
{
  RoundTripMetrics metrics;
  std::string reason;
  const bool measured = measure_round_trip(&profile, metrics, reason);
  if (!measured) {
    std::printf("      reason: %s\n", reason.c_str());
  }
  check(measured && metrics.model == expectedModel &&
          metrics.samples == expectedSamples,
        message);
}

static void test_round_trip_models()
{
  CIccProfile rgb;
  build_rgb_matrix_profile(rgb);
  check_round_trip_model(rgb, "matrix/TRC", 729,
                         "RGB/XYZ matrix-TRC model uses the 9x9x9 grid");

  CIccProfile gray;
  build_gray_matrix_profile(gray);
  check_round_trip_model(gray, "matrix/TRC", 9,
                         "Gray/XYZ matrix-TRC model uses the 9-point grid");

  CIccProfile cmyk;
  build_cmyk_classic_profile(cmyk);
  check_round_trip_model(cmyk, "classic lut8/lut16 A2B1/B2A1", 625,
                         "CMYK/Lab classic LUT model uses the 5x5x5x5 grid");

  CIccProfile *cmm = ReadIccProfile("Testing/sRGB_v4_ICC_preference.icc");
  check(cmm != nullptr, "tracked RGB/Lab CMM profile loads");
  if (cmm) {
    check_round_trip_model(*cmm, "CIccCmm profile transform", 729,
                           "RGB/Lab general CMM model uses the 9x9x9 grid");
    delete cmm;
  }
}

int main()
{
  test_sample_budget();
  test_ciede2000_vectors();
  test_round_trip_models();

  if (g_failures) {
    std::printf("\n%d check(s) FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nall checks passed\n");
  return 0;
}
