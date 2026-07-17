/*
  File:     IccVizMath.hpp

  Contains: Shared colour-math helpers (XYZ->xy projection, planckian locus) used by the visualization model.

  Version:  V1

  Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *  International Color Consortium" must not be used to imply that the
 *  ICC organization endorses or promotes products derived from this
 *  software.
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

/**
 * IccVizMath - small shared colour-math helpers for the visualization model.
 *
 * The pure scalar conversions the visualizations need: XYZ->xy chromaticity
 * projection and the Kang et al. planckian-locus approximation.
 *
 * They live in one header so the math - especially approxPlanck()'s polynomial
 * constants - has a single source. The functions are header-only (inline) and
 * depend only on the ICC scalar types, so the header carries no link dependency.
 */

#ifndef ICC_VIZ_MATH_HPP
#define ICC_VIZ_MATH_HPP

#include "IccDefs.h"           // icXYZNumber, icFloatNumber, and ICC header packing
#include <cmath>               // std::sqrt / std::acos / std::cos (principalStdDevs)
#include <cstddef>             // std::size_t

namespace iccvizmath {

// Plain 2-D point (chromaticity x,y). Aggregate so both tools can assign into it.
struct XY {
  float x = 0.0f;
  float y = 0.0f;
};

// xyFromICCXYZ - project a 16-bit ICC XYZ (s15Fixed16 stored as integer/65535) to
// CIE xy chromaticity. Separate from the float version because the integer source
// can't be NaN/Inf, so it skips those checks; a near-zero sum returns (0,0).
inline XY xyFromICCXYZ(const icXYZNumber* xyz) {
  // integers, so don't have to test for NaN or Inf
  float X = xyz->X / 65535.0f;
  float Y = xyz->Y / 65535.0f;
  float Z = xyz->Z / 65535.0f;
  float sum = X + Y + Z;
  if (sum <= 1e-8f) return XY{0.0f, 0.0f};
  return XY{X / sum, Y / sum};
}

// xyFromXYZFloat - project a floating-point XYZ triple to CIE xy chromaticity; the
// float twin of xyFromICCXYZ for values that come from a transform rather than a
// stored tag. A near-zero (or non-positive) sum returns (0,0) to avoid dividing by ~0.
inline XY xyFromXYZFloat(const icFloatNumber* xyz) {
  float X = xyz[0];
  float Y = xyz[1];
  float Z = xyz[2];
  float sum = X + Y + Z;
  if (sum <= 1e-8f) return XY{0.0f, 0.0f};
  return XY{X / sum, Y / sum};
}

// approxPlanck - approximate the planckian (blackbody) locus in CIE xy for a colour
// temperature `t` (Kelvin), used to draw the reference curve on the chromaticity
// chart. Uses the Kang et al. piecewise polynomial fit rather than integrating the
// Planck radiator, which is fast, table-free and accurate enough for a plotted guide.
//   https://en.wikipedia.org/wiki/Planckian_locus
//   Bongsoon Kang; Ohak Moon; Changhee Hong; Honam Lee; Bonghwan Cho;
//   Youngsun Kim (December 2002). "Design of Advanced Color Temperature Control
//   System for HDTV Applications" J. Korean Physical Society 41(6): 865-871.
inline XY approxPlanck(double t) {
  const double c3a = -0.2661239;
  const double c2a = -0.2343589;
  const double c1a =  0.8776956;
  const double c0a =  0.179910;

  const double c3b = -3.0258469;
  const double c2b =  2.1070379;
  const double c1b =  0.2226347;
  const double c0b =  0.240390;

  const double k3a = -1.1063814;
  const double k2a = -1.34811020;
  const double k1a =  2.18555832;
  const double k0a = -0.20219683;

  const double k3b = -0.9549476;
  const double k2b = -1.37418593;
  const double k1b =  2.09137015;
  const double k0b = -0.16748867;

  const double k3c =  3.0817580;
  const double k2c = -5.87338670;
  const double k1c =  3.75112997;
  const double k0c = -0.37001483;

  double t2 = t * t;
  double t3 = t * t * t;

  double x = 0.0;
  if (t < 4000.0) {
    x = c3a * (1e9 / t3) + c2a * (1e6 / t2) + c1a * (1e3 / t) + c0a;
  } else {
    x = c3b * (1e9 / t3) + c2b * (1e6 / t2) + c1b * (1e3 / t) + c0b;
  }

  double x2 = x * x;
  double x3 = x * x * x;

  double y = 0.0;
  if (t < 2222.0) {
    y = k3a * x3 + k2a * x2 + k1a * x + k0a;
  } else if (t < 4000.0) {
    y = k3b * x3 + k2b * x2 + k1b * x + k0b;
  } else {
    y = k3c * x3 + k2c * x2 + k1c * x + k0c;
  }

  return XY{static_cast<float>(x), static_cast<float>(y)};
}

// principalStdDevs - standard deviations of a 3-D point cloud along its principal
// axes (s1 >= s2 >= s3, so s3 is the thinnest extent), from the closed-form
// symmetric-3x3 eigenvalues of the covariance matrix. Used to detect a boundary
// cloud collapsed toward a plane (s3 ~ 0) or a line (s2 ~ s3 ~ 0), where a
// closing-based enclosed volume becomes a sheet/tube artifact. pts is x3-
// interleaved (x,y,z, x,y,z, ...); n is the point count. Outputs 0 for n < 2.
inline void principalStdDevs(const float* pts, std::size_t n,
                             double& s1, double& s2, double& s3) {
  s1 = s2 = s3 = 0.0;
  if (!pts || n < 2) return;

  double mx = 0.0, my = 0.0, mz = 0.0;
  for (std::size_t i = 0; i < n; ++i) { mx += pts[i*3]; my += pts[i*3+1]; mz += pts[i*3+2]; }
  const double inv = 1.0 / static_cast<double>(n);
  mx *= inv; my *= inv; mz *= inv;

  // covariance (symmetric): [cxx cxy cxz; cxy cyy cyz; cxz cyz czz]
  double cxx = 0.0, cyy = 0.0, czz = 0.0, cxy = 0.0, cxz = 0.0, cyz = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double dx = pts[i*3]     - mx;
    const double dy = pts[i*3 + 1] - my;
    const double dz = pts[i*3 + 2] - mz;
    cxx += dx*dx; cyy += dy*dy; czz += dz*dz;
    cxy += dx*dy; cxz += dx*dz; cyz += dy*dz;
  }
  cxx *= inv; cyy *= inv; czz *= inv; cxy *= inv; cxz *= inv; cyz *= inv;

  // eigenvalues of a symmetric 3x3 in closed form (Smith 1961 trig method)
  double e1, e2, e3;   // descending
  const double p1 = cxy*cxy + cxz*cxz + cyz*cyz;
  if (p1 <= 0.0) {                       // already diagonal
    e1 = cxx; e2 = cyy; e3 = czz;
    double t;
    if (e1 < e2) { t = e1; e1 = e2; e2 = t; }
    if (e2 < e3) { t = e2; e2 = e3; e3 = t; }
    if (e1 < e2) { t = e1; e1 = e2; e2 = t; }
  } else {
    const double q  = (cxx + cyy + czz) / 3.0;
    const double p2 = (cxx-q)*(cxx-q) + (cyy-q)*(cyy-q) + (czz-q)*(czz-q) + 2.0*p1;
    const double p  = std::sqrt(p2 / 6.0);   // p2 >= 2*p1 > 0, so p > 0
    const double b00 = (cxx-q)/p, b11 = (cyy-q)/p, b22 = (czz-q)/p;
    const double b01 = cxy/p, b02 = cxz/p, b12 = cyz/p;
    double det = b00*(b11*b22 - b12*b12) - b01*(b01*b22 - b12*b02) + b02*(b01*b12 - b11*b02);
    double rr = det / 2.0;
    if (rr < -1.0) rr = -1.0; else if (rr > 1.0) rr = 1.0;
    const double phi = std::acos(rr) / 3.0;
    const double twoPiOver3 = 2.0943951023931953;
    e1 = q + 2.0*p*std::cos(phi);
    e3 = q + 2.0*p*std::cos(phi + twoPiOver3);
    e2 = 3.0*q - e1 - e3;                     // trace invariant
  }

  s1 = std::sqrt(e1 > 0.0 ? e1 : 0.0);
  s2 = std::sqrt(e2 > 0.0 ? e2 : 0.0);
  s3 = std::sqrt(e3 > 0.0 ? e3 : 0.0);
}

} // namespace iccvizmath

#endif // ICC_VIZ_MATH_HPP
