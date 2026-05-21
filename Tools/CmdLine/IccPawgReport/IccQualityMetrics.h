/** @file
    File:       IccQualityMetrics.h

    Contains:   ICC PAWG quality metric helpers for IccPawgReport

    Copyright:  (c) see below
 */

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

#ifndef ICC_QUALITY_METRICS_H
#define ICC_QUALITY_METRICS_H

#include "IccProfile.h"
#include "IccCmm.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace iccquality {

/// Maximum channel count for fixed-size stack arrays in quality metrics.
/// All std::array<icFloatNumber, 16> buffers depend on this bound.
constexpr int kMaxQualityChannels = 16;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDegToRad = kPi / 180.0;

struct RoundTripMetrics {
  bool measured = false;
  std::string model;
  int samples = 0;
  double avgFirstDe00 = 0.0;
  double maxFirstDe00 = 0.0;
  double avgSecondDe00 = 0.0;
  double maxSecondDe00 = 0.0;
};

struct SmoothnessMetrics {
  bool measured = false;
  std::string model;
  int samples = 0;
  double avgStepDe00 = 0.0;
  double maxStepDe00 = 0.0;
  double maxCurvatureDe00 = 0.0;
  int discontinuities = 0;
};

struct CurveInvertibilityMetrics {
  struct CurveResult {
    std::string name;
    double avgError = 0.0;
    double maxError = 0.0;
    bool monotonic = true;
    bool flat = false;
  };

  std::vector<CurveResult> curves;
};

struct CharacterizationMetrics {
  bool hasTag = false;
  bool parsedFormat = false;
  bool parsedData = false;
  bool hasDeviceColumns = false;
  bool hasPcsColumns = false;
  int fieldCount = 0;
  int rowCount = 0;
  int rowsUsed = 0;
  double avgDe00 = 0.0;
  double maxDe00 = 0.0;
};

constexpr int kQualityMaxDeviceChannels = 4;

inline double clamp01(double v) {
  if (v < 0.0) return 0.0;
  if (v > 1.0) return 1.0;
  return v;
}

inline double average_metric(double total, int n) {
  return n > 0 ? total / static_cast<double>(n) : 0.0;
}

inline bool finite3(const icFloatNumber *v) {
  return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

inline bool is_zero_entry_identity_curve(const CIccCurve *curve) {
  auto *tagCurve = dynamic_cast<const CIccTagCurve*>(curve);
  return tagCurve && tagCurve->GetSize() == 0;
}

inline void begin_curve_safe(CIccCurve *curve) {
  if (!curve || is_zero_entry_identity_curve(curve)) {
    return;
  }
  curve->Begin();
}

inline icFloatNumber apply_curve_safe(CIccCurve *curve, icFloatNumber v) {
  if (!curve) {
    return v;
  }
  if (is_zero_entry_identity_curve(curve)) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  }
  return curve->Apply(v);
}

inline icFloatNumber find_curve_safe(CIccCurve *curve, icFloatNumber v) {
  if (!curve) {
    return v;
  }
  if (is_zero_entry_identity_curve(curve)) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  }
  return curve->Find(v);
}

inline std::string trim_copy(const std::string &in) {
  size_t begin = 0;
  while (begin < in.size() && std::isspace(static_cast<unsigned char>(in[begin]))) {
    ++begin;
  }
  size_t end = in.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
    --end;
  }
  return in.substr(begin, end - begin);
}

inline std::string upper_ascii(std::string s) {
  for (char &ch : s) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return s;
}

inline std::vector<std::string> split_ws(const std::string &line) {
  std::vector<std::string> out;
  std::istringstream iss(line);
  std::string tok;
  while (iss >> tok) {
    out.push_back(tok);
  }
  return out;
}

inline void pcs_to_lab(icColorSpaceSignature pcs, const icFloatNumber *src, icFloatNumber *lab) {
  lab[0] = src[0];
  lab[1] = src[1];
  lab[2] = src[2];

  if (pcs == icSigLabData) {
    icLabFromPcs(lab);
  } else {
    icXYZtoLab(lab, const_cast<icFloatNumber*>(src), nullptr);
  }
}

inline double delta_e_2000(const icFloatNumber *lab1, const icFloatNumber *lab2) {
  const double L1 = lab1[0];
  const double a1 = lab1[1];
  const double b1 = lab1[2];
  const double L2 = lab2[0];
  const double a2 = lab2[1];
  const double b2 = lab2[2];

  const double c1 = std::sqrt(a1 * a1 + b1 * b1);
  const double c2 = std::sqrt(a2 * a2 + b2 * b2);
  const double cBar = (c1 + c2) * 0.5;
  const double cBar7 = std::pow(cBar, 7.0);
  const double g = 0.5 * (1.0 - std::sqrt(cBar7 / (cBar7 + std::pow(25.0, 7.0))));

  const double a1Prime = (1.0 + g) * a1;
  const double a2Prime = (1.0 + g) * a2;
  const double c1Prime = std::sqrt(a1Prime * a1Prime + b1 * b1);
  const double c2Prime = std::sqrt(a2Prime * a2Prime + b2 * b2);

  auto hue_angle = [](double a, double b) {
    double h = std::atan2(b, a) * 180.0 / kPi;
    return h < 0.0 ? h + 360.0 : h;
  };

  const double h1Prime = (c1Prime < 1e-12) ? 0.0 : hue_angle(a1Prime, b1);
  const double h2Prime = (c2Prime < 1e-12) ? 0.0 : hue_angle(a2Prime, b2);

  const double deltaLPrime = L2 - L1;
  const double deltaCPrime = c2Prime - c1Prime;

  double deltaHPrime = 0.0;
  if (c1Prime >= 1e-12 && c2Prime >= 1e-12) {
    double diff = h2Prime - h1Prime;
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;
    deltaHPrime = diff;
  }
  const double deltaBigHPrime = 2.0 * std::sqrt(c1Prime * c2Prime) *
                                std::sin((deltaHPrime * 0.5) * kDegToRad);

  const double LBarPrime = (L1 + L2) * 0.5;
  const double CBarPrime = (c1Prime + c2Prime) * 0.5;

  double hBarPrime = h1Prime + h2Prime;
  if (c1Prime < 1e-12 || c2Prime < 1e-12) {
    hBarPrime = h1Prime + h2Prime;
  } else if (std::fabs(h1Prime - h2Prime) > 180.0) {
    hBarPrime += (h1Prime + h2Prime < 360.0) ? 360.0 : -360.0;
    hBarPrime *= 0.5;
  } else {
    hBarPrime *= 0.5;
  }

  const double t = 1.0
                 - 0.17 * std::cos((hBarPrime - 30.0) * kDegToRad)
                 + 0.24 * std::cos((2.0 * hBarPrime) * kDegToRad)
                 + 0.32 * std::cos((3.0 * hBarPrime + 6.0) * kDegToRad)
                 - 0.20 * std::cos((4.0 * hBarPrime - 63.0) * kDegToRad);

  const double deltaTheta = 30.0 * std::exp(-std::pow((hBarPrime - 275.0) / 25.0, 2.0));
  const double rc = 2.0 * std::sqrt(std::pow(CBarPrime, 7.0) /
                                    (std::pow(CBarPrime, 7.0) + std::pow(25.0, 7.0)));
  const double sl = 1.0 + (0.015 * std::pow(LBarPrime - 50.0, 2.0)) /
                            std::sqrt(20.0 + std::pow(LBarPrime - 50.0, 2.0));
  const double sc = 1.0 + 0.045 * CBarPrime;
  const double sh = 1.0 + 0.015 * CBarPrime * t;
  const double rt = -std::sin(2.0 * deltaTheta * kDegToRad) * rc;

  const double termL = deltaLPrime / sl;
  const double termC = deltaCPrime / sc;
  const double termH = deltaBigHPrime / sh;

  const double value = termL * termL + termC * termC + termH * termH +
                       rt * termC * termH;
  return value > 0.0 ? std::sqrt(value) : 0.0;
}

template <typename Fn>
inline void visit_bounded_grid_points_impl(int axis,
                                           int channels,
                                           int grid,
                                           std::array<icFloatNumber, 16> &device,
                                           Fn &fn) {
  if (axis >= channels || axis >= kMaxQualityChannels) {
    fn();
    return;
  }

  for (int i = 0; i < grid; ++i) {
    device[static_cast<size_t>(axis)] =
        static_cast<icFloatNumber>(i / static_cast<double>(grid - 1));
    visit_bounded_grid_points_impl(axis + 1, channels, grid, device, fn);
  }
}

template <typename Fn>
inline void visit_bounded_grid_points(int channels,
                                      std::array<icFloatNumber, 16> &device,
                                      Fn &&fn) {
  const int grid = channels >= 4 ? 5 : 9;
  visit_bounded_grid_points_impl(0, channels, grid, device, fn);
}

struct MatrixTrcTransform {
  int channels = 0;
  icColorSpaceSignature pcs = static_cast<icColorSpaceSignature>(0);
  CIccCurve *curves[4] = {nullptr, nullptr, nullptr, nullptr};
  double matrix[9] = {0.0};
  double invMatrix[9] = {0.0};
  double whiteXYZ[3] = {0.9642, 1.0, 0.8249};
};

inline bool invert_3x3(const double m[9], double out[9]) {
  const double det =
      m[0] * (m[4] * m[8] - m[5] * m[7]) -
      m[1] * (m[3] * m[8] - m[5] * m[6]) +
      m[2] * (m[3] * m[7] - m[4] * m[6]);

  if (!std::isfinite(det) || std::fabs(det) < 1e-12) {
    return false;
  }

  const double invDet = 1.0 / det;
  out[0] =  (m[4] * m[8] - m[5] * m[7]) * invDet;
  out[1] = -(m[1] * m[8] - m[2] * m[7]) * invDet;
  out[2] =  (m[1] * m[5] - m[2] * m[4]) * invDet;
  out[3] = -(m[3] * m[8] - m[5] * m[6]) * invDet;
  out[4] =  (m[0] * m[8] - m[2] * m[6]) * invDet;
  out[5] = -(m[0] * m[5] - m[2] * m[3]) * invDet;
  out[6] =  (m[3] * m[7] - m[4] * m[6]) * invDet;
  out[7] = -(m[0] * m[7] - m[1] * m[6]) * invDet;
  out[8] =  (m[0] * m[4] - m[1] * m[3]) * invDet;
  return true;
}

inline bool build_matrix_trc_transform(CIccProfile *pIcc,
                                       MatrixTrcTransform &xform,
                                       std::string &reason) {
  if (!pIcc) {
    reason = "Profile handle is null";
    return false;
  }

  const uint32_t pcs = static_cast<uint32_t>(pIcc->m_Header.pcs);
  xform.pcs = static_cast<icColorSpaceSignature>(pcs);
  if (pcs != static_cast<uint32_t>(icSigXYZData)) {
    reason = "Matrix/TRC quality metrics require XYZ PCS";
    return false;
  }

  const uint32_t colorSpace = static_cast<uint32_t>(pIcc->m_Header.colorSpace);
  if (colorSpace == static_cast<uint32_t>(icSigRgbData)) {
    CIccCurve *rTrc = dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigRedTRCTag));
    CIccCurve *gTrc = dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigGreenTRCTag));
    CIccCurve *bTrc = dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigBlueTRCTag));
    CIccTagXYZ *rXYZ = dynamic_cast<CIccTagXYZ*>(pIcc->FindTag(icSigRedMatrixColumnTag));
    CIccTagXYZ *gXYZ = dynamic_cast<CIccTagXYZ*>(pIcc->FindTag(icSigGreenMatrixColumnTag));
    CIccTagXYZ *bXYZ = dynamic_cast<CIccTagXYZ*>(pIcc->FindTag(icSigBlueMatrixColumnTag));

    if (!rTrc || !gTrc || !bTrc) {
      reason = "Matrix/TRC tags are incomplete";
      return false;
    }
    if (!rXYZ || !gXYZ || !bXYZ) {
      reason = "Matrix/TRC tags are incomplete";
      return false;
    }
    if (rXYZ->GetSize() < 1 || gXYZ->GetSize() < 1 || bXYZ->GetSize() < 1) {
      reason = "Matrix/TRC tags are incomplete";
      return false;
    }

    xform.channels = 3;
    xform.curves[0] = rTrc;
    xform.curves[1] = gTrc;
    xform.curves[2] = bTrc;
    for (int i = 0; i < 3; ++i) {
      begin_curve_safe(xform.curves[i]);
    }

    const icXYZNumber &r = (*rXYZ)[0];
    const icXYZNumber &g = (*gXYZ)[0];
    const icXYZNumber &b = (*bXYZ)[0];

    xform.matrix[0] = icFtoD(r.X);
    xform.matrix[1] = icFtoD(g.X);
    xform.matrix[2] = icFtoD(b.X);
    xform.matrix[3] = icFtoD(r.Y);
    xform.matrix[4] = icFtoD(g.Y);
    xform.matrix[5] = icFtoD(b.Y);
    xform.matrix[6] = icFtoD(r.Z);
    xform.matrix[7] = icFtoD(g.Z);
    xform.matrix[8] = icFtoD(b.Z);

    if (!invert_3x3(xform.matrix, xform.invMatrix)) {
      reason = "Matrix/TRC primary matrix is singular";
      return false;
    }
    return true;
  }

  if (colorSpace == icSigGrayData) {
    CIccCurve *kTrc = dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigGrayTRCTag));
    if (!kTrc) {
      reason = "Gray matrix/TRC profile is missing grayTRC";
      return false;
    }
    begin_curve_safe(kTrc);
    xform.channels = 1;
    xform.curves[0] = kTrc;

    CIccTagXYZ *wtpt = dynamic_cast<CIccTagXYZ*>(pIcc->FindTag(icSigMediaWhitePointTag));
    if (!wtpt) {
      return true;
    }
    if (wtpt->GetSize() < 1) {
      return true;
    }

    const icXYZNumber &xyz = (*wtpt)[0];
    xform.whiteXYZ[0] = icFtoD(xyz.X);
    xform.whiteXYZ[1] = icFtoD(xyz.Y);
    xform.whiteXYZ[2] = icFtoD(xyz.Z);
    return true;
  }

  reason = "Matrix/TRC quality metrics currently support RGB and Gray only";
  return false;
}

inline bool evaluate_matrix_trc_forward(const MatrixTrcTransform &xform,
                                        const icFloatNumber *device,
                                        icFloatNumber *pcsXYZ) {
  if (xform.channels == 3) {
    const double r = clamp01(apply_curve_safe(xform.curves[0], device[0]));
    const double g = clamp01(apply_curve_safe(xform.curves[1], device[1]));
    const double b = clamp01(apply_curve_safe(xform.curves[2], device[2]));

    pcsXYZ[0] = static_cast<icFloatNumber>(xform.matrix[0] * r + xform.matrix[1] * g + xform.matrix[2] * b);
    pcsXYZ[1] = static_cast<icFloatNumber>(xform.matrix[3] * r + xform.matrix[4] * g + xform.matrix[5] * b);
    pcsXYZ[2] = static_cast<icFloatNumber>(xform.matrix[6] * r + xform.matrix[7] * g + xform.matrix[8] * b);
    return finite3(pcsXYZ);
  }

  if (xform.channels == 1) {
    const double y = clamp01(apply_curve_safe(xform.curves[0], device[0]));
    pcsXYZ[0] = static_cast<icFloatNumber>(y * xform.whiteXYZ[0]);
    pcsXYZ[1] = static_cast<icFloatNumber>(y * xform.whiteXYZ[1]);
    pcsXYZ[2] = static_cast<icFloatNumber>(y * xform.whiteXYZ[2]);
    return finite3(pcsXYZ);
  }

  return false;
}

inline bool evaluate_matrix_trc_reverse(const MatrixTrcTransform &xform,
                                        const icFloatNumber *pcsXYZ,
                                        icFloatNumber *device) {
  if (xform.channels == 3) {
    const double x = pcsXYZ[0];
    const double y = pcsXYZ[1];
    const double z = pcsXYZ[2];

    for (int c = 0; c < 3; ++c) {
      const double linear =
          xform.invMatrix[c * 3 + 0] * x +
          xform.invMatrix[c * 3 + 1] * y +
          xform.invMatrix[c * 3 + 2] * z;
      const double clamped = clamp01(linear);
      device[c] = static_cast<icFloatNumber>(clamp01(find_curve_safe(xform.curves[c], static_cast<icFloatNumber>(clamped))));
    }
    return std::isfinite(device[0]) && std::isfinite(device[1]) && std::isfinite(device[2]);
  }

  if (xform.channels == 1) {
    double y = pcsXYZ[1];
    const double whiteY = xform.whiteXYZ[1];
    if (std::fabs(whiteY) > 1e-12) {
      y = pcsXYZ[1] / whiteY;
    }
    y = clamp01(y);
    device[0] = static_cast<icFloatNumber>(clamp01(find_curve_safe(xform.curves[0], static_cast<icFloatNumber>(y))));
    return std::isfinite(device[0]);
  }

  return false;
}

inline bool is_lab_or_xyz_pcs(icColorSpaceSignature pcs) {
  return pcs == icSigLabData || pcs == icSigXYZData;
}

inline bool status_ok(icStatusCMM status, const std::string &phase, std::string &reason) {
  if (status == icCmmStatOk) {
    return true;
  }
  reason = "CIccCmm " + phase + " failed: " + CIccCmm::GetStatusText(status);
  return false;
}

inline bool begin_profile_cmm(CIccProfile *pIcc,
                              CIccCmm &cmm,
                              const char *direction,
                              std::string &reason) {
  if (!pIcc) {
    reason = "Profile handle is null";
    return false;
  }

  const icColorSpaceSignature pcs = static_cast<icColorSpaceSignature>(pIcc->m_Header.pcs);
  if (!is_lab_or_xyz_pcs(pcs)) {
    reason = "CIccCmm quality metrics require Lab or XYZ PCS";
    return false;
  }

  icStatusCMM status = cmm.AddXform(*pIcc);
  if (!status_ok(status, std::string(direction) + " AddXform", reason)) {
    return false;
  }

  status = cmm.Begin();
  return status_ok(status, std::string(direction) + " Begin", reason);
}

inline bool has_standard_cmm_forward_candidate(CIccProfile *pIcc) {
  if (!pIcc) {
    return false;
  }

  const icTagSignature forwardTags[] = {
      icSigAToB0Tag, icSigAToB1Tag, icSigAToB2Tag,
      icSigDToB0Tag, icSigDToB1Tag, icSigDToB2Tag,
  };
  for (icTagSignature sig : forwardTags) {
    if (pIcc->FindTag(sig)) {
      return true;
    }
  }
  return false;
}

inline bool has_standard_cmm_round_trip_candidate(CIccProfile *pIcc) {
  if (!pIcc) {
    return false;
  }

  struct Pair {
    icTagSignature forward;
    icTagSignature reverse;
  };
  const Pair pairs[] = {
      {icSigAToB0Tag, icSigBToA0Tag},
      {icSigAToB1Tag, icSigBToA1Tag},
      {icSigAToB2Tag, icSigBToA2Tag},
      {icSigDToB0Tag, icSigBToD0Tag},
      {icSigDToB1Tag, icSigBToD1Tag},
      {icSigDToB2Tag, icSigBToD2Tag},
  };
  for (const Pair &pair : pairs) {
    if (pIcc->FindTag(pair.forward) && pIcc->FindTag(pair.reverse)) {
      return true;
    }
  }
  return false;
}

inline bool measure_cmm_round_trip(CIccProfile *pIcc,
                                   RoundTripMetrics &metrics,
                                   std::string &reason) {
  if (!pIcc) {
    reason = "Profile handle is null";
    return false;
  }

  const icColorSpaceSignature colorSpace =
      static_cast<icColorSpaceSignature>(pIcc->m_Header.colorSpace);
  const icColorSpaceSignature pcs =
      static_cast<icColorSpaceSignature>(pIcc->m_Header.pcs);
  const int deviceChannels = static_cast<int>(icGetSpaceSamples(colorSpace));
  const int pcsChannels = static_cast<int>(icGetSpaceSamples(pcs));
  if (deviceChannels < 1 || deviceChannels > kMaxQualityChannels ||
      pcsChannels < 3 || pcsChannels > kMaxQualityChannels ||
      !is_lab_or_xyz_pcs(pcs)) {
    reason = "CIccCmm round-trip requires bounded device channels and Lab/XYZ PCS";
    return false;
  }
  if (!has_standard_cmm_round_trip_candidate(pIcc)) {
    reason = "No supported round-trip transform pair present";
    return false;
  }

  CIccCmm forward(colorSpace, pcs, true);
  CIccCmm reverse(pcs, colorSpace, false);
  std::string localReason;
  if (!begin_profile_cmm(pIcc, forward, "forward", localReason) ||
      !begin_profile_cmm(pIcc, reverse, "reverse", localReason)) {
    reason = localReason;
    return false;
  }

  std::array<icFloatNumber, 16> device0{};
  std::array<icFloatNumber, 16> pcs1{};
  std::array<icFloatNumber, 16> device1{};
  std::array<icFloatNumber, 16> pcs2{};
  std::array<icFloatNumber, 16> device2{};
  std::array<icFloatNumber, 16> pcs3{};
  std::array<icFloatNumber, 3> lab1{};
  std::array<icFloatNumber, 3> lab2{};
  std::array<icFloatNumber, 3> lab3{};

  double sumFirst = 0.0;
  double sumSecond = 0.0;
  int samples = 0;

  visit_bounded_grid_points(deviceChannels, device0, [&]() {
    if (forward.Apply(pcs1.data(), device0.data()) != icCmmStatOk ||
        reverse.Apply(device1.data(), pcs1.data()) != icCmmStatOk ||
        forward.Apply(pcs2.data(), device1.data()) != icCmmStatOk ||
        reverse.Apply(device2.data(), pcs2.data()) != icCmmStatOk ||
        forward.Apply(pcs3.data(), device2.data()) != icCmmStatOk ||
        !finite3(pcs1.data()) || !finite3(pcs2.data()) || !finite3(pcs3.data())) {
      return;
    }

    pcs_to_lab(pcs, pcs1.data(), lab1.data());
    pcs_to_lab(pcs, pcs2.data(), lab2.data());
    pcs_to_lab(pcs, pcs3.data(), lab3.data());

    const double de1 = delta_e_2000(lab1.data(), lab2.data());
    const double de2 = delta_e_2000(lab2.data(), lab3.data());
    if (!std::isfinite(de1) || !std::isfinite(de2)) {
      return;
    }

    sumFirst += de1;
    sumSecond += de2;
    metrics.maxFirstDe00 = std::max(metrics.maxFirstDe00, de1);
    metrics.maxSecondDe00 = std::max(metrics.maxSecondDe00, de2);
    ++samples;
  });

  if (samples == 0) {
    reason = "CIccCmm round-trip measurement produced no valid samples";
    return false;
  }

  metrics.measured = true;
  metrics.model = "CIccCmm profile transform";
  metrics.samples = samples;
  metrics.avgFirstDe00 = sumFirst / samples;
  metrics.avgSecondDe00 = sumSecond / samples;
  return true;
}

inline bool measure_cmm_forward_smoothness(CIccProfile *pIcc,
                                           SmoothnessMetrics &metrics,
                                           std::string &reason) {
  if (!pIcc) {
    reason = "Profile handle is null";
    return false;
  }

  const icColorSpaceSignature colorSpace =
      static_cast<icColorSpaceSignature>(pIcc->m_Header.colorSpace);
  const icColorSpaceSignature pcs =
      static_cast<icColorSpaceSignature>(pIcc->m_Header.pcs);
  const int deviceChannels = static_cast<int>(icGetSpaceSamples(colorSpace));
  if (deviceChannels < 1 || deviceChannels > kMaxQualityChannels ||
      !is_lab_or_xyz_pcs(pcs)) {
    reason = "CIccCmm smoothness requires bounded device channels and Lab/XYZ PCS";
    return false;
  }
  if (!has_standard_cmm_forward_candidate(pIcc)) {
    reason = "No supported forward transform tag present";
    return false;
  }

  CIccCmm forward(colorSpace, pcs, true);
  if (!begin_profile_cmm(pIcc, forward, "forward", reason)) {
    return false;
  }

  constexpr int kSamples = 65;
  std::array<icFloatNumber, 16> device{};
  std::array<icFloatNumber, 16> pcsValue{};
  std::array<icFloatNumber, 3> prevLab{};
  std::array<icFloatNumber, 3> currLab{};

  double sumStep = 0.0;
  int stepCount = 0;

  auto accumulate_path = [&](bool diagonal, int axis) {
    bool havePrev = false;
    double prevStep = -1.0;

    for (int i = 0; i < kSamples; ++i) {
      const double v = i / static_cast<double>(kSamples - 1);
      device.fill(0.0f);

      if (deviceChannels == 1) {
        device[0] = static_cast<icFloatNumber>(v);
      } else if (diagonal) {
        for (int c = 0; c < deviceChannels; ++c) {
          device[static_cast<size_t>(c)] = static_cast<icFloatNumber>(v);
        }
      } else {
        for (int c = 0; c < deviceChannels; ++c) {
          device[static_cast<size_t>(c)] = 0.5f;
        }
        device[static_cast<size_t>(axis)] = static_cast<icFloatNumber>(v);
      }

      if (forward.Apply(pcsValue.data(), device.data()) != icCmmStatOk ||
          !finite3(pcsValue.data())) {
        continue;
      }

      pcs_to_lab(pcs, pcsValue.data(), currLab.data());
      if (havePrev) {
        const double step = delta_e_2000(prevLab.data(), currLab.data());
        if (!std::isfinite(step)) {
          continue;
        }
        metrics.maxStepDe00 = std::max(metrics.maxStepDe00, step);
        sumStep += step;
        if (prevStep >= 0.0) {
          metrics.maxCurvatureDe00 =
              std::max(metrics.maxCurvatureDe00, std::fabs(step - prevStep));
        }
        if (step > 6.0) {
          ++metrics.discontinuities;
        }
        prevStep = step;
        ++stepCount;
      } else {
        havePrev = true;
      }

      prevLab = currLab;
    }
  };

  if (deviceChannels == 1) {
    accumulate_path(false, 0);
  } else {
    accumulate_path(true, 0);
    for (int axis = 0; axis < deviceChannels; ++axis) {
      accumulate_path(false, axis);
    }
  }

  if (stepCount == 0) {
    reason = "CIccCmm smoothness measurement produced no valid samples";
    return false;
  }

  metrics.avgStepDe00 = average_metric(sumStep, stepCount);
  metrics.measured = true;
  metrics.model = "CIccCmm profile transform";
  metrics.samples = stepCount;
  return true;
}

struct ClassicLutTransform {
  CIccMBB *mbb = nullptr;
  CIccCurve **inputCurves = nullptr;
  CIccCurve **outputCurves = nullptr;
  CIccCLUT *clut = nullptr;
  CIccMatrix *matrix = nullptr;
  icColorSpaceSignature pcs = static_cast<icColorSpaceSignature>(0);
  int inputChannels = 0;
  int outputChannels = 0;
  icTagTypeSignature type = static_cast<icTagTypeSignature>(0);
};

struct ClassicLutTagSelection {
  icTagSignature forwardSig = static_cast<icTagSignature>(0);
  icTagSignature reverseSig = static_cast<icTagSignature>(0);
  const char *label = "";
};

inline const std::array<ClassicLutTagSelection, 3>& classic_lut_round_trip_pairs() {
  static const std::array<ClassicLutTagSelection, 3> kPairs{{
      {icSigAToB0Tag, icSigBToA0Tag, "A2B0/B2A0"},
      {icSigAToB1Tag, icSigBToA1Tag, "A2B1/B2A1"},
      {icSigAToB2Tag, icSigBToA2Tag, "A2B2/B2A2"},
  }};
  return kPairs;
}

inline const std::array<icTagSignature, 3>& classic_lut_forward_tags() {
  static const std::array<icTagSignature, 3> kTags{{
      icSigAToB0Tag,
      icSigAToB1Tag,
      icSigAToB2Tag,
  }};
  return kTags;
}

inline const char *tag_sig_short_name(icTagSignature sig) {
  switch (sig) {
    case icSigAToB0Tag: return "A2B0";
    case icSigAToB1Tag: return "A2B1";
    case icSigAToB2Tag: return "A2B2";
    case icSigBToA0Tag: return "B2A0";
    case icSigBToA1Tag: return "B2A1";
    case icSigBToA2Tag: return "B2A2";
    default: break;
  }
  return "unknown";
}

inline bool select_classic_lut_round_trip_pair(CIccProfile *pIcc,
                                               CIccTag *&forwardTag,
                                               CIccTag *&reverseTag,
                                               const char *&label) {
  if (!pIcc) {
    return false;
  }

  for (const auto &pair : classic_lut_round_trip_pairs()) {
    CIccTag *candidateForward = pIcc->FindTag(pair.forwardSig);
    CIccTag *candidateReverse = pIcc->FindTag(pair.reverseSig);
    if (candidateForward && candidateReverse) {
      forwardTag = candidateForward;
      reverseTag = candidateReverse;
      label = pair.label;
      return true;
    }
  }

  return false;
}

inline bool select_classic_lut_forward_tag(CIccProfile *pIcc,
                                           CIccTag *&forwardTag,
                                           const char *&label) {
  if (!pIcc) {
    return false;
  }

  for (icTagSignature sig : classic_lut_forward_tags()) {
    CIccTag *candidate = pIcc->FindTag(sig);
    if (candidate) {
      forwardTag = candidate;
      label = tag_sig_short_name(sig);
      return true;
    }
  }

  return false;
}

inline bool build_classic_lut_transform(CIccTag *tag,
                                        icColorSpaceSignature pcs,
                                        ClassicLutTransform &xform,
                                        std::string &reason) {
  if (!tag) {
    reason = "Transform tag is missing";
    return false;
  }

  const icTagTypeSignature type = tag->GetType();
  if (type != icSigLut8Type && type != icSigLut16Type) {
    reason = "Only classic lut8/lut16 quality metrics are currently supported";
    return false;
  }

  CIccMBB *mbb = dynamic_cast<CIccMBB*>(tag);
  if (!mbb) {
    reason = "Classic LUT tag is not an MBB transform";
    return false;
  }

  CIccCLUT *clut = mbb->GetCLUT();
  if (!clut) {
    reason = "Classic LUT tag has no CLUT";
    return false;
  }
  if (clut->GetInputDim() < 1 || clut->GetInputDim() > 4) {
    reason = "Classic LUT CLUT dimension outside bounded evaluator support";
    return false;
  }

  CIccCurve **inputCurves = mbb->GetCurvesB();
  if (!inputCurves) {
    inputCurves = mbb->GetCurvesM();
  }
  CIccCurve **outputCurves = mbb->GetCurvesA();
  if (!inputCurves || !outputCurves) {
    reason = "Classic LUT tag is missing input/output curves";
    return false;
  }

  xform.mbb = mbb;
  xform.inputCurves = inputCurves;
  xform.outputCurves = outputCurves;
  xform.clut = clut;
  xform.matrix = mbb->GetMatrix();
  xform.pcs = pcs;
  xform.inputChannels = mbb->InputChannels();
  xform.outputChannels = mbb->OutputChannels();
  xform.type = type;

  if (xform.inputChannels < 1 || xform.inputChannels > kMaxQualityChannels ||
      xform.outputChannels < 1 || xform.outputChannels > kMaxQualityChannels) {
    reason = "Classic LUT channel count exceeds safe array bounds";
    return false;
  }

  for (int i = 0; i < xform.inputChannels; ++i) {
    if (!xform.inputCurves[i]) {
      reason = "Classic LUT has null input curve";
      return false;
    }
    begin_curve_safe(xform.inputCurves[i]);
  }
  for (int i = 0; i < xform.outputChannels; ++i) {
    if (!xform.outputCurves[i]) {
      reason = "Classic LUT has null output curve";
      return false;
    }
    begin_curve_safe(xform.outputCurves[i]);
  }
  xform.clut->Begin();
  return true;
}

inline bool interpolate_clut(const ClassicLutTransform &xform,
                             icFloatNumber *dst,
                             const icFloatNumber *src) {
  switch (xform.clut->GetInputDim()) {
    case 1: xform.clut->Interp1d(dst, src); return true;
    case 2: xform.clut->Interp2d(dst, src); return true;
    case 3: xform.clut->Interp3d(dst, src); return true;
    case 4: xform.clut->Interp4d(dst, src); return true;
    default: break;
  }
  return false;
}

inline bool evaluate_classic_lut_forward(const ClassicLutTransform &xform,
                                         const icFloatNumber *device,
                                         icFloatNumber *pcsOut) {
  std::array<icFloatNumber, 16> stage1{};
  std::array<icFloatNumber, 16> stage2{};

  for (int i = 0; i < xform.inputChannels; ++i) {
    stage1[i] = static_cast<icFloatNumber>(clamp01(apply_curve_safe(xform.inputCurves[i], device[i])));
  }

  if (xform.matrix && xform.inputChannels == 3) {
    xform.matrix->Apply(stage1.data());
  }

  if (!interpolate_clut(xform, stage2.data(), stage1.data())) {
    return false;
  }

  for (int i = 0; i < xform.outputChannels; ++i) {
    pcsOut[i] = static_cast<icFloatNumber>(clamp01(apply_curve_safe(xform.outputCurves[i], stage2[i])));
  }

  return xform.outputChannels < 3 || finite3(pcsOut);
}

inline bool measure_matrix_trc_round_trip(CIccProfile *pIcc,
                                          RoundTripMetrics &metrics,
                                          std::string &reason) {
  MatrixTrcTransform xform;
  if (!build_matrix_trc_transform(pIcc, xform, reason)) {
    return false;
  }

  constexpr int kGrid = 9;
  std::array<icFloatNumber, 16> device0{};
  std::array<icFloatNumber, 16> pcs1{};
  std::array<icFloatNumber, 16> device1{};
  std::array<icFloatNumber, 16> pcs2{};
  std::array<icFloatNumber, 16> device2{};
  std::array<icFloatNumber, 16> pcs3{};
  std::array<icFloatNumber, 3> lab1{};
  std::array<icFloatNumber, 3> lab2{};
  std::array<icFloatNumber, 3> lab3{};

  double sumFirst = 0.0;
  double sumSecond = 0.0;
  int samples = 0;

  if (xform.channels == 1) {
    for (int g = 0; g < kGrid; ++g) {
      device0[0] = static_cast<icFloatNumber>(g / static_cast<double>(kGrid - 1));
      if (!evaluate_matrix_trc_forward(xform, device0.data(), pcs1.data()) ||
          !evaluate_matrix_trc_reverse(xform, pcs1.data(), device1.data()) ||
          !evaluate_matrix_trc_forward(xform, device1.data(), pcs2.data()) ||
          !evaluate_matrix_trc_reverse(xform, pcs2.data(), device2.data()) ||
          !evaluate_matrix_trc_forward(xform, device2.data(), pcs3.data())) {
        continue;
      }
      pcs_to_lab(icSigXYZData, pcs1.data(), lab1.data());
      pcs_to_lab(icSigXYZData, pcs2.data(), lab2.data());
      pcs_to_lab(icSigXYZData, pcs3.data(), lab3.data());

      const double de1 = delta_e_2000(lab1.data(), lab2.data());
      const double de2 = delta_e_2000(lab2.data(), lab3.data());
      sumFirst += de1;
      sumSecond += de2;
      metrics.maxFirstDe00 = std::max(metrics.maxFirstDe00, de1);
      metrics.maxSecondDe00 = std::max(metrics.maxSecondDe00, de2);
      ++samples;
    }
  } else {
    for (int r = 0; r < kGrid; ++r) {
      for (int g = 0; g < kGrid; ++g) {
        for (int b = 0; b < kGrid; ++b) {
          device0[0] = static_cast<icFloatNumber>(r / static_cast<double>(kGrid - 1));
          device0[1] = static_cast<icFloatNumber>(g / static_cast<double>(kGrid - 1));
          device0[2] = static_cast<icFloatNumber>(b / static_cast<double>(kGrid - 1));

          if (!evaluate_matrix_trc_forward(xform, device0.data(), pcs1.data()) ||
              !evaluate_matrix_trc_reverse(xform, pcs1.data(), device1.data()) ||
              !evaluate_matrix_trc_forward(xform, device1.data(), pcs2.data()) ||
              !evaluate_matrix_trc_reverse(xform, pcs2.data(), device2.data()) ||
              !evaluate_matrix_trc_forward(xform, device2.data(), pcs3.data())) {
            continue;
          }

          pcs_to_lab(icSigXYZData, pcs1.data(), lab1.data());
          pcs_to_lab(icSigXYZData, pcs2.data(), lab2.data());
          pcs_to_lab(icSigXYZData, pcs3.data(), lab3.data());

          const double de1 = delta_e_2000(lab1.data(), lab2.data());
          const double de2 = delta_e_2000(lab2.data(), lab3.data());
          sumFirst += de1;
          sumSecond += de2;
          metrics.maxFirstDe00 = std::max(metrics.maxFirstDe00, de1);
          metrics.maxSecondDe00 = std::max(metrics.maxSecondDe00, de2);
          ++samples;
        }
      }
    }
  }

  if (samples == 0) {
    reason = "Matrix/TRC round-trip measurement produced no valid samples";
    return false;
  }

  metrics.measured = true;
  metrics.model = "matrix/TRC";
  metrics.samples = samples;
  metrics.avgFirstDe00 = sumFirst / samples;
  metrics.avgSecondDe00 = sumSecond / samples;
  return true;
}

inline bool measure_classic_lut_round_trip(CIccProfile *pIcc,
                                           RoundTripMetrics &metrics,
                                           std::string &reason) {
  ClassicLutTransform forward;
  ClassicLutTransform reverse;

  CIccTag *forwardTag = nullptr;
  CIccTag *reverseTag = nullptr;
  const char *pairLabel = "";
  if (!select_classic_lut_round_trip_pair(pIcc, forwardTag, reverseTag, pairLabel)) {
    reason = "No supported AToB/BToA classic LUT pair present";
    return false;
  }

  std::string forwardReason;
  if (!build_classic_lut_transform(forwardTag, pIcc->m_Header.pcs, forward, forwardReason)) {
    reason = std::string(pairLabel) + " present but " + forwardReason;
    return false;
  }

  std::string reverseReason;
  if (!build_classic_lut_transform(reverseTag, pIcc->m_Header.colorSpace, reverse, reverseReason)) {
    reason = std::string(pairLabel) + " present but " + reverseReason;
    return false;
  }

  if (forward.outputChannels < 3 ||
      forward.inputChannels != reverse.outputChannels ||
      forward.outputChannels != reverse.inputChannels) {
    reason = std::string(pairLabel) + " channel mismatch prevents bounded round-trip";
    return false;
  }

  std::array<icFloatNumber, 16> device0{};
  std::array<icFloatNumber, 16> pcs1{};
  std::array<icFloatNumber, 16> device1{};
  std::array<icFloatNumber, 16> pcs2{};
  std::array<icFloatNumber, 16> device2{};
  std::array<icFloatNumber, 16> pcs3{};
  std::array<icFloatNumber, 3> lab1{};
  std::array<icFloatNumber, 3> lab2{};
  std::array<icFloatNumber, 3> lab3{};

  double sumFirst = 0.0;
  double sumSecond = 0.0;
  int samples = 0;

  visit_bounded_grid_points(forward.inputChannels, device0, [&]() {
    if (!evaluate_classic_lut_forward(forward, device0.data(), pcs1.data()) ||
        !evaluate_classic_lut_forward(reverse, pcs1.data(), device1.data()) ||
        !evaluate_classic_lut_forward(forward, device1.data(), pcs2.data()) ||
        !evaluate_classic_lut_forward(reverse, pcs2.data(), device2.data()) ||
        !evaluate_classic_lut_forward(forward, device2.data(), pcs3.data())) {
      return;
    }

    pcs_to_lab(pIcc->m_Header.pcs, pcs1.data(), lab1.data());
    pcs_to_lab(pIcc->m_Header.pcs, pcs2.data(), lab2.data());
    pcs_to_lab(pIcc->m_Header.pcs, pcs3.data(), lab3.data());

    const double de1 = delta_e_2000(lab1.data(), lab2.data());
    const double de2 = delta_e_2000(lab2.data(), lab3.data());
    sumFirst += de1;
    sumSecond += de2;
    metrics.maxFirstDe00 = std::max(metrics.maxFirstDe00, de1);
    metrics.maxSecondDe00 = std::max(metrics.maxSecondDe00, de2);
    ++samples;
  });

  if (samples == 0) {
    reason = "Classic LUT round-trip measurement produced no valid samples";
    return false;
  }

  metrics.measured = true;
  metrics.model = std::string("classic lut8/lut16 ") + pairLabel;
  metrics.samples = samples;
  metrics.avgFirstDe00 = sumFirst / samples;
  metrics.avgSecondDe00 = sumSecond / samples;
  return true;
}

inline bool measure_round_trip(CIccProfile *pIcc,
                               RoundTripMetrics &metrics,
                               std::string &reason) {
  if (measure_classic_lut_round_trip(pIcc, metrics, reason)) {
    return true;
  }

  std::string matrixReason;
  if (measure_matrix_trc_round_trip(pIcc, metrics, matrixReason)) {
    return true;
  }

  std::string cmmReason;
  if (measure_cmm_round_trip(pIcc, metrics, cmmReason)) {
    return true;
  }

  reason = !cmmReason.empty() ? cmmReason : (matrixReason.empty() ? reason : matrixReason);
  if (reason.empty()) {
    reason = "No supported round-trip transform pair present";
  }
  return false;
}

inline bool measure_forward_smoothness_matrix_trc(CIccProfile *pIcc,
                                                  SmoothnessMetrics &metrics,
                                                  std::string &reason) {
  MatrixTrcTransform xform;
  if (!build_matrix_trc_transform(pIcc, xform, reason)) {
    return false;
  }

  constexpr int kSamples = 65;
  std::array<icFloatNumber, 16> device{};
  std::array<icFloatNumber, 16> pcs{};
  std::array<icFloatNumber, 3> prevLab{};
  std::array<icFloatNumber, 3> currLab{};

  double sumStep = 0.0;
  int stepCount = 0;

  auto accumulate_path = [&](bool diagonal, int axis) {
    bool havePrev = false;
    double prevStep = -1.0;

    for (int i = 0; i < kSamples; ++i) {
      const double v = i / static_cast<double>(kSamples - 1);
      device.fill(0.0f);

      if (xform.channels == 1) {
        device[0] = static_cast<icFloatNumber>(v);
      } else if (diagonal) {
        for (int c = 0; c < xform.channels; ++c) {
          device[static_cast<size_t>(c)] = static_cast<icFloatNumber>(v);
        }
      } else {
        for (int c = 0; c < xform.channels; ++c) {
          device[static_cast<size_t>(c)] = 0.5f;
        }
        device[static_cast<size_t>(axis)] = static_cast<icFloatNumber>(v);
      }

      if (!evaluate_matrix_trc_forward(xform, device.data(), pcs.data())) {
        continue;
      }

      pcs_to_lab(icSigXYZData, pcs.data(), currLab.data());
      if (havePrev) {
        const double step = delta_e_2000(prevLab.data(), currLab.data());
        metrics.maxStepDe00 = std::max(metrics.maxStepDe00, step);
        sumStep += step;
        if (prevStep >= 0.0) {
          metrics.maxCurvatureDe00 =
              std::max(metrics.maxCurvatureDe00, std::fabs(step - prevStep));
        }
        if (step > 6.0) {
          ++metrics.discontinuities;
        }
        prevStep = step;
        ++stepCount;
      } else {
        havePrev = true;
      }

      prevLab = currLab;
    }
  };

  if (xform.channels == 1) {
    accumulate_path(false, 0);
  } else {
    accumulate_path(true, 0);
    for (int axis = 0; axis < xform.channels; ++axis) {
      accumulate_path(false, axis);
    }
  }

  if (stepCount == 0) {
    reason = "Matrix/TRC smoothness measurement produced no valid samples";
    return false;
  }

  metrics.avgStepDe00 = average_metric(sumStep, stepCount);
  metrics.measured = true;
  metrics.model = xform.channels == 1 ? "gray matrix/TRC" : "matrix/TRC multi-axis";
  metrics.samples = stepCount;
  return true;
}

inline bool measure_forward_smoothness_classic_lut(CIccProfile *pIcc,
                                                   SmoothnessMetrics &metrics,
                                                   std::string &reason) {
  ClassicLutTransform forward;
  CIccTag *forwardTag = nullptr;
  const char *tagLabel = "";
  if (!select_classic_lut_forward_tag(pIcc, forwardTag, tagLabel)) {
    reason = "No supported forward transform tag present";
    return false;
  }
  if (!build_classic_lut_transform(forwardTag, pIcc->m_Header.pcs, forward, reason)) {
    reason = std::string(tagLabel) + " present but " + reason;
    return false;
  }

  constexpr int kSamples = 65;
  std::array<icFloatNumber, 16> device{};
  std::array<icFloatNumber, 16> pcs{};
  std::array<icFloatNumber, 3> prevLab{};
  std::array<icFloatNumber, 3> currLab{};

  double sumStep = 0.0;
  int stepCount = 0;

  auto accumulate_path = [&](bool diagonal, int axis) {
    bool havePrev = false;
    double prevStep = -1.0;

    for (int i = 0; i < kSamples; ++i) {
      const double v = i / static_cast<double>(kSamples - 1);
      device.fill(0.0f);

      if (forward.inputChannels == 1) {
        device[0] = static_cast<icFloatNumber>(v);
      } else if (diagonal) {
        for (int c = 0; c < forward.inputChannels; ++c) {
          device[static_cast<size_t>(c)] = static_cast<icFloatNumber>(v);
        }
      } else {
        for (int c = 0; c < forward.inputChannels; ++c) {
          device[static_cast<size_t>(c)] = 0.5f;
        }
        device[static_cast<size_t>(axis)] = static_cast<icFloatNumber>(v);
      }

      if (!evaluate_classic_lut_forward(forward, device.data(), pcs.data())) {
        continue;
      }

      pcs_to_lab(pIcc->m_Header.pcs, pcs.data(), currLab.data());
      if (havePrev) {
        const double step = delta_e_2000(prevLab.data(), currLab.data());
        metrics.maxStepDe00 = std::max(metrics.maxStepDe00, step);
        sumStep += step;
        if (prevStep >= 0.0) {
          metrics.maxCurvatureDe00 =
              std::max(metrics.maxCurvatureDe00, std::fabs(step - prevStep));
        }
        if (step > 6.0) {
          ++metrics.discontinuities;
        }
        prevStep = step;
        ++stepCount;
      } else {
        havePrev = true;
      }

      prevLab = currLab;
    }
  };

  if (forward.inputChannels == 1) {
    accumulate_path(false, 0);
  } else {
    accumulate_path(true, 0);
    for (int axis = 0; axis < forward.inputChannels; ++axis) {
      accumulate_path(false, axis);
    }
  }

  if (stepCount == 0) {
    reason = "Classic LUT smoothness measurement produced no valid samples";
    return false;
  }

  metrics.avgStepDe00 = average_metric(sumStep, stepCount);
  metrics.measured = true;
  metrics.model = std::string("classic lut8/lut16 ") + tagLabel;
  metrics.samples = stepCount;
  return true;
}

inline bool measure_transform_smoothness(CIccProfile *pIcc,
                                         SmoothnessMetrics &metrics,
                                         std::string &reason) {
  if (measure_forward_smoothness_classic_lut(pIcc, metrics, reason)) {
    return true;
  }

  std::string matrixReason;
  if (measure_forward_smoothness_matrix_trc(pIcc, metrics, matrixReason)) {
    return true;
  }

  std::string cmmReason;
  if (measure_cmm_forward_smoothness(pIcc, metrics, cmmReason)) {
    return true;
  }

  if (reason.empty()) {
    reason = !cmmReason.empty() ? cmmReason : matrixReason;
  }
  if (reason.empty()) {
    reason = "No supported forward transform for smoothness analysis";
  }
  return false;
}

inline void append_curve_target(std::vector<std::pair<std::string, CIccCurve*>> &targets,
                                const std::string &name,
                                CIccCurve *curve) {
  if (!curve) {
    return;
  }
  targets.push_back(std::make_pair(name, curve));
}

inline void append_mbb_curve_targets(std::vector<std::pair<std::string, CIccCurve*>> &targets,
                                     CIccProfile *pIcc,
                                     icTagSignature sig,
                                     const char *label) {
  CIccMBB *mbb = dynamic_cast<CIccMBB*>(pIcc->FindTag(sig));
  if (!mbb) {
    return;
  }

  auto add_curve_set = [&targets, label](CIccCurve **curves,
                                         int channels,
                                         const char *setName) {
    if (!curves) {
      return;
    }
    for (int i = 0; i < channels; ++i) {
      if (!curves[i]) {
        continue;
      }
      std::ostringstream oss;
      oss << label << " " << setName << "[" << i << "]";
      append_curve_target(targets, oss.str(), curves[i]);
    }
  };

  const int aCurves = mbb->IsInputB() ? mbb->OutputChannels() : mbb->InputChannels();
  const int mCurves = mbb->IsInputMatrix() ? mbb->InputChannels() : mbb->OutputChannels();
  const int bCurves = mbb->IsInputB() ? mbb->InputChannels() : mbb->OutputChannels();

  // they can't be negative when copied from unsigned values
  if (aCurves > kMaxQualityChannels ||
      mCurves > kMaxQualityChannels ||
      bCurves > kMaxQualityChannels) {
    return;
  }

  add_curve_set(mbb->GetCurvesA(), aCurves, "A");
  add_curve_set(mbb->GetCurvesM(), mCurves, "M");
  add_curve_set(mbb->GetCurvesB(), bCurves, "B");
}

inline CurveInvertibilityMetrics measure_curve_invertibility(CIccProfile *pIcc) {
  CurveInvertibilityMetrics metrics;
  if (!pIcc) {
    return metrics;
  }

  std::vector<std::pair<std::string, CIccCurve*>> curves;
  append_curve_target(curves, "rTRC", dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigRedTRCTag)));
  append_curve_target(curves, "gTRC", dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigGreenTRCTag)));
  append_curve_target(curves, "bTRC", dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigBlueTRCTag)));
  append_curve_target(curves, "kTRC", dynamic_cast<CIccCurve*>(pIcc->FindTag(icSigGrayTRCTag)));

  append_mbb_curve_targets(curves, pIcc, icSigAToB0Tag, "A2B0");
  append_mbb_curve_targets(curves, pIcc, icSigBToA0Tag, "B2A0");
  append_mbb_curve_targets(curves, pIcc, icSigAToB1Tag, "A2B1");
  append_mbb_curve_targets(curves, pIcc, icSigBToA1Tag, "B2A1");
  append_mbb_curve_targets(curves, pIcc, icSigAToB2Tag, "A2B2");
  append_mbb_curve_targets(curves, pIcc, icSigBToA2Tag, "B2A2");
  append_mbb_curve_targets(curves, pIcc, icSigDToB0Tag, "D2B0");
  append_mbb_curve_targets(curves, pIcc, icSigBToD0Tag, "B2D0");
  append_mbb_curve_targets(curves, pIcc, icSigDToB1Tag, "D2B1");
  append_mbb_curve_targets(curves, pIcc, icSigBToD1Tag, "B2D1");
  append_mbb_curve_targets(curves, pIcc, icSigDToB2Tag, "D2B2");
  append_mbb_curve_targets(curves, pIcc, icSigBToD2Tag, "B2D2");

  for (auto &entry : curves) {
    CIccCurve *curve = entry.second;
    if (!curve) {
      continue;
    }

    begin_curve_safe(curve);
    CurveInvertibilityMetrics::CurveResult result;
    result.name = entry.first;
    result.monotonic = true;
    result.flat = true;

    constexpr int kSamples = 257;
    std::array<double, kSamples> forward{};

    double minValue = 0.0;
    double maxValue = 0.0;
    for (int i = 0; i < kSamples; ++i) {
      const double x = i / static_cast<double>(kSamples - 1);
      const double y = apply_curve_safe(curve, static_cast<icFloatNumber>(x));
      forward[static_cast<size_t>(i)] = y;
      if (i == 0) {
        minValue = maxValue = y;
      } else {
        if (y < forward[static_cast<size_t>(i - 1)] - 1e-6) {
          result.monotonic = false;
        }
        minValue = std::min(minValue, y);
        maxValue = std::max(maxValue, y);
      }
    }

    result.flat = std::fabs(maxValue - minValue) < 1e-6;
    if (result.flat) {
      result.maxError = 1.0;
      result.avgError = 1.0;
      metrics.curves.push_back(result);
      continue;
    }

    double sumError = 0.0;
    for (int i = 0; i < kSamples; ++i) {
      const double x = i / static_cast<double>(kSamples - 1);
      const double y = forward[static_cast<size_t>(i)];
      const double inv = clamp01(find_curve_safe(curve, static_cast<icFloatNumber>(y)));
      const double err = std::fabs(inv - x);
      sumError += err;
      result.maxError = std::max(result.maxError, err);
    }
    result.avgError = sumError / kSamples;
    metrics.curves.push_back(result);
  }

  return metrics;
}

struct ParsedTargetData {
  bool hasTag = false;
  bool parsedFormat = false;
  bool parsedData = false;
  std::vector<std::string> fields;
  std::vector<std::vector<double>> rows;
};

inline ParsedTargetData parse_char_target(CIccProfile *pIcc) {
  ParsedTargetData parsed;
  if (!pIcc) {
    return parsed;
  }

  CIccTagText *textTag = dynamic_cast<CIccTagText*>(pIcc->FindTag(icSigCharTargetTag));
  if (!textTag) {
    return parsed;
  }
  parsed.hasTag = true;

  const char *text = textTag->GetText();
  if (!text || !text[0]) {
    return parsed;
  }

  std::istringstream input(text);
  std::string line;
  bool inFormat = false;
  bool inData = false;

  while (std::getline(input, line)) {
    line = trim_copy(line);
    if (line.empty()) {
      continue;
    }

    if (line == "BEGIN_DATA_FORMAT") {
      inFormat = true;
      parsed.parsedFormat = true;
      continue;
    }
    if (line == "END_DATA_FORMAT") {
      inFormat = false;
      continue;
    }
    if (line == "BEGIN_DATA") {
      inData = true;
      parsed.parsedData = true;
      continue;
    }
    if (line == "END_DATA") {
      inData = false;
      continue;
    }

    if (inFormat) {
      std::vector<std::string> toks = split_ws(line);
      parsed.fields.insert(parsed.fields.end(), toks.begin(), toks.end());
      continue;
    }

    if (inData) {
      std::vector<std::string> toks = split_ws(line);
      if (toks.empty()) {
        continue;
      }
      std::vector<double> row;
      row.reserve(toks.size());
      bool ok = true;
      for (const std::string &tok : toks) {
        char *end = nullptr;
        const double value = std::strtod(tok.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(value)) {
          ok = false;
          break;
        }
        row.push_back(value);
      }
      if (ok) {
        parsed.rows.push_back(std::move(row));
      }
    }
  }

  return parsed;
}

inline int find_field_index(const std::vector<std::string> &fields,
                            const std::vector<std::string> &candidates) {
  for (size_t i = 0; i < fields.size(); ++i) {
    const std::string field = upper_ascii(fields[i]);
    for (const std::string &candidate : candidates) {
      if (field == candidate) {
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

inline void normalize_device_rows(std::vector<std::array<double, kQualityMaxDeviceChannels>> &values,
                                  int channels) {
  double maxSeen = 0.0;
  for (const auto &v : values) {
    for (int i = 0; i < channels; ++i) {
      maxSeen = std::max(maxSeen, std::fabs(v[static_cast<size_t>(i)]));
    }
  }

  double scale = 1.0;
  if (maxSeen > 1.0 + 1e-6 && maxSeen <= 100.0 + 1e-6) {
    scale = 100.0;
  } else if (maxSeen > 100.0 + 1e-6 && maxSeen <= 255.0 + 1e-6) {
    scale = 255.0;
  } else if (maxSeen > 255.0 + 1e-6 && maxSeen <= 65535.0 + 1e-6) {
    scale = 65535.0;
  } else if (maxSeen > 65535.0) {
    scale = maxSeen;
  }

  if (scale != 1.0) {
    for (auto &v : values) {
      for (int i = 0; i < channels; ++i) {
        v[static_cast<size_t>(i)] /= scale;
      }
    }
  }
}

inline bool evaluate_characterization(CIccProfile *pIcc,
                                      CharacterizationMetrics &metrics,
                                      std::string &reason) {
  ParsedTargetData parsed = parse_char_target(pIcc);
  metrics.hasTag = parsed.hasTag;
  metrics.parsedFormat = parsed.parsedFormat;
  metrics.parsedData = parsed.parsedData;
  metrics.fieldCount = static_cast<int>(parsed.fields.size());
  metrics.rowCount = static_cast<int>(parsed.rows.size());

  if (!parsed.hasTag) {
    reason = "No characterization data (targ) tag present";
    return false;
  }
  if (!parsed.parsedFormat || parsed.fields.empty()) {
    reason = "Characterization data present but data format is not parseable";
    return false;
  }
  if (!parsed.parsedData || parsed.rows.empty()) {
    reason = "Characterization data present but no measurement rows were parsed";
    return false;
  }

  std::vector<std::array<double, kQualityMaxDeviceChannels>> deviceRows;
  std::vector<std::array<double, 3>> pcsRows;
  const icColorSpaceSignature colorSpace = pIcc->m_Header.colorSpace;
  const icColorSpaceSignature pcs = pIcc->m_Header.pcs;
  int deviceChannels = 0;

  if (colorSpace == icSigRgbData) {
    const int r = find_field_index(parsed.fields, {"RGB_R", "R"});
    const int g = find_field_index(parsed.fields, {"RGB_G", "G"});
    const int b = find_field_index(parsed.fields, {"RGB_B", "B"});
    if (r >= 0 && g >= 0 && b >= 0) {
      metrics.hasDeviceColumns = true;
      deviceChannels = 3;
      for (const auto &row : parsed.rows) {
        if (static_cast<int>(row.size()) <= std::max({r, g, b})) {
          continue;
        }
        deviceRows.push_back({row[static_cast<size_t>(r)],
                              row[static_cast<size_t>(g)],
                              row[static_cast<size_t>(b)],
                              0.0});
      }
      normalize_device_rows(deviceRows, deviceChannels);
    }
  } else if (colorSpace == icSigGrayData) {
    const int k = find_field_index(parsed.fields, {"GRAY", "K", "GRAY_K"});
    if (k >= 0) {
      metrics.hasDeviceColumns = true;
      deviceChannels = 1;
      for (const auto &row : parsed.rows) {
        if (static_cast<int>(row.size()) <= k) {
          continue;
        }
        deviceRows.push_back({row[static_cast<size_t>(k)], 0.0, 0.0, 0.0});
      }
      normalize_device_rows(deviceRows, deviceChannels);
    }
  } else if (colorSpace == icSigCmykData) {
    const int c = find_field_index(parsed.fields, {"CMYK_C", "C"});
    const int m = find_field_index(parsed.fields, {"CMYK_M", "M"});
    const int y = find_field_index(parsed.fields, {"CMYK_Y", "Y"});
    const int k = find_field_index(parsed.fields, {"CMYK_K", "K"});
    if (c >= 0 && m >= 0 && y >= 0 && k >= 0) {
      metrics.hasDeviceColumns = true;
      deviceChannels = 4;
      for (const auto &row : parsed.rows) {
        if (static_cast<int>(row.size()) <= std::max({c, m, y, k})) {
          continue;
        }
        deviceRows.push_back({row[static_cast<size_t>(c)],
                              row[static_cast<size_t>(m)],
                              row[static_cast<size_t>(y)],
                              row[static_cast<size_t>(k)]});
      }
      normalize_device_rows(deviceRows, deviceChannels);
    }
  }

  if (pcs == icSigLabData) {
    const int l = find_field_index(parsed.fields, {"LAB_L", "L"});
    const int a = find_field_index(parsed.fields, {"LAB_A", "A"});
    const int b = find_field_index(parsed.fields, {"LAB_B", "B"});
    if (l >= 0 && a >= 0 && b >= 0) {
      metrics.hasPcsColumns = true;
      for (const auto &row : parsed.rows) {
        if (static_cast<int>(row.size()) <= std::max({l, a, b})) {
          continue;
        }
        pcsRows.push_back({row[static_cast<size_t>(l)],
                           row[static_cast<size_t>(a)],
                           row[static_cast<size_t>(b)]});
      }
    }
  } else if (pcs == icSigXYZData) {
    const int x = find_field_index(parsed.fields, {"XYZ_X", "X"});
    const int y = find_field_index(parsed.fields, {"XYZ_Y", "Y"});
    const int z = find_field_index(parsed.fields, {"XYZ_Z", "Z"});
    if (x >= 0 && y >= 0 && z >= 0) {
      metrics.hasPcsColumns = true;
      for (const auto &row : parsed.rows) {
        if (static_cast<int>(row.size()) <= std::max({x, y, z})) {
          continue;
        }
        pcsRows.push_back({row[static_cast<size_t>(x)],
                           row[static_cast<size_t>(y)],
                           row[static_cast<size_t>(z)]});
      }
      double maxSeen = 0.0;
      for (const auto &v : pcsRows) {
        maxSeen = std::max(maxSeen, std::fabs(v[0]));
        maxSeen = std::max(maxSeen, std::fabs(v[1]));
        maxSeen = std::max(maxSeen, std::fabs(v[2]));
      }
      if (maxSeen > 2.0 && maxSeen <= 100.0) {
        for (auto &v : pcsRows) {
          v[0] /= 100.0;
          v[1] /= 100.0;
          v[2] /= 100.0;
        }
      }
    }
  }

  if (!metrics.hasDeviceColumns) {
    reason = "Characterization data present but device columns are not mapped for this profile class";
    return false;
  }
  if (!metrics.hasPcsColumns) {
    reason = "Characterization data present but PCS columns are not mapped for this profile";
    return false;
  }

  if (deviceRows.empty() || pcsRows.empty()) {
    reason = "Characterization data present but no usable measurement rows were parsed";
    return false;
  }

  const size_t usableRows = std::min(deviceRows.size(), pcsRows.size());
  if (usableRows == 0) {
    reason = "Characterization data present but no aligned device/PCS rows were parsed";
    return false;
  }

  ClassicLutTransform classicForward;
  MatrixTrcTransform matrixForward;
  bool useClassic = false;
  bool useMatrix = false;
  bool useCmm = false;
  CIccCmm cmmForward(static_cast<icColorSpaceSignature>(pIcc->m_Header.colorSpace),
                     static_cast<icColorSpaceSignature>(pIcc->m_Header.pcs),
                     true);

  std::string transformReason;
  const char *forwardLabel = "";
  if (CIccTag *forwardTag = nullptr; select_classic_lut_forward_tag(pIcc, forwardTag, forwardLabel)) {
    useClassic = build_classic_lut_transform(forwardTag, pIcc->m_Header.pcs, classicForward, transformReason);
    if (!useClassic && !transformReason.empty()) {
      transformReason = std::string(forwardLabel) + " present but " + transformReason;
    }
  }

  if (!useClassic) {
    useMatrix = build_matrix_trc_transform(pIcc, matrixForward, transformReason);
  }
  if (!useClassic && !useMatrix) {
    useCmm = begin_profile_cmm(pIcc, cmmForward, "forward", transformReason);
  }
  if (!useClassic && !useMatrix && !useCmm) {
    reason = "Characterization data present but no supported forward transform is available";
    if (!transformReason.empty()) {
      reason += ": " + transformReason;
    }
    return false;
  }

  if (useClassic) {
    if (classicForward.inputChannels != deviceChannels) {
      reason = "Characterization device columns do not match classic LUT input channels";
      return false;
    }
  } else if (useMatrix && matrixForward.channels != deviceChannels) {
    reason = "Characterization device columns do not match matrix/TRC input channels";
    return false;
  }

  const size_t limit = std::min<size_t>(usableRows, 128);
  std::array<icFloatNumber, 16> pcsPred{};
  std::array<icFloatNumber, 3> labPred{};
  std::array<icFloatNumber, 3> labMeasured{};

  double sumDe = 0.0;
  int used = 0;

  for (size_t i = 0; i < limit; ++i) {
    std::array<icFloatNumber, 16> device{};
    for (int c = 0; c < deviceChannels; ++c) {
      device[static_cast<size_t>(c)] =
          static_cast<icFloatNumber>(clamp01(deviceRows[i][static_cast<size_t>(c)]));
    }

    bool ok = false;
    if (useClassic) {
      ok = evaluate_classic_lut_forward(classicForward, device.data(), pcsPred.data());
    } else if (useMatrix) {
      ok = evaluate_matrix_trc_forward(matrixForward, device.data(), pcsPred.data());
    } else {
      ok = cmmForward.Apply(pcsPred.data(), device.data()) == icCmmStatOk &&
           finite3(pcsPred.data());
    }
    if (!ok) {
      continue;
    }

    pcs_to_lab(pIcc->m_Header.pcs, pcsPred.data(), labPred.data());
    if (pIcc->m_Header.pcs == icSigLabData) {
      labMeasured[0] = static_cast<icFloatNumber>(pcsRows[i][0]);
      labMeasured[1] = static_cast<icFloatNumber>(pcsRows[i][1]);
      labMeasured[2] = static_cast<icFloatNumber>(pcsRows[i][2]);
    } else {
      const icFloatNumber measuredXYZ[3] = {
          static_cast<icFloatNumber>(pcsRows[i][0]),
          static_cast<icFloatNumber>(pcsRows[i][1]),
          static_cast<icFloatNumber>(pcsRows[i][2]),
      };
      pcs_to_lab(icSigXYZData, measuredXYZ, labMeasured.data());
    }

    const double de = delta_e_2000(labPred.data(), labMeasured.data());
    sumDe += de;
    metrics.maxDe00 = std::max(metrics.maxDe00, de);
    ++used;
  }

  if (used == 0) {
    reason = "Characterization data present but no bounded forward samples could be evaluated";
    return false;
  }

  metrics.rowsUsed = used;
  metrics.avgDe00 = sumDe / used;
  return true;
}

} // namespace iccquality

#endif // ICC_QUALITY_METRICS_H
