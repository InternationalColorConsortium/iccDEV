/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// Regression (#2795): the BT.2100 PQ fixtures in Testing/HDR did not encode
// BT.2100 PQ.
//
// The five Testing/HDR/BT2100PQ*.xml fixtures build their transforms from ICC.2
// formula segments.  Six defects, each checked here against Rec. ITU-R BT.2100
// Table 4 (ST 2084) computed independently in this file:
//
//   1. The PQ EOTF, function type 6 (Y = d * (max(e*X^g - a, 0)/(b - c*X^g))^w,
//      parameters w, g, a, b, c, d, e), carried w = m1.  The EOTF needs
//      w = 1/m1, so a code value of 0.5 decoded to 8879 cd/m^2, not 92.2.
//   2. The inverse EOTF, function type 7, takes luminance / 10000.  The Display
//      profiles fed it cd/m^2, so 203 cd/m^2 encoded to 1.47, not 0.5807.
//   3. The type 7 segment started at 1/12, HLG's breakpoint, though the segment
//      before it ends at 0.  The binary keeps only breakpoints, so that segment,
//      which outputs 0, covered [0, 1/12): once the input is divided by 10000,
//      everything below 833 cd/m^2 encodes to 0.
//   4. The Scene profiles' inverse OOTF took the BT.709 breakpoint at
//      E = 0.003024 rather than 0.0003024: E' = 0.409 for 0.081, with a linear
//      slope ten times 1/267.84.
//   5. The OOTF in the Scene reverse transforms and in the link applied the
//      BT.709 part, E' = G709(59.5208 * E), and stopped; the BT.1886 part,
//      100 * E'^2.4, was missing.
//   6. The Narrow Scene reverse transform divided by 4.8048 (HLG's 75% scale)
//      where its forward transform divides by 203.
//
// The fixtures are tracked as XML (Testing/**/*.icc is gitignored), so each is
// parsed through the CIccProfileXml::LoadXml path iccFromXml uses, written to
// memory as iccFromXml writes it to disk, and read back.  The write matters:
// the parsed curve keeps each segment's own Start and End, but the binary keeps
// only the breakpoints, so defect 3 exists only in the written profile.  Each
// multiProcessElementType tag is then applied directly, with no CMM or PCC, so
// the result is the tag's own arithmetic.  For each colour profile the test checks
// AToB1Tag against the reference, BToA1Tag on the reference's output, and the
// round trip; for the link, AToB0Tag against OOTF(E) / 10000.
//
// Usage:
//   bt2100-pq-fixtures <Testing/HDR directory>
//
// Returns 0 on success, 1 if any assertion failed (each is printed).

#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccTagMPE.h"
#include "IccIO.h"
#include "IccUtil.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const std::string &what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[bt2100-pq-fixtures] FAIL: %s\n", what.c_str());
  }
}

// Rec. ITU-R BT.2100 Table 4 constants.
const double kM1 = 2610.0 / 16384.0;
const double kM2 = 2523.0 / 4096.0 * 128.0;
const double kC1 = 3424.0 / 4096.0;
const double kC2 = 2413.0 / 4096.0 * 32.0;
const double kC3 = 2392.0 / 4096.0 * 32.0;

// The narrow-range black and nominal peak the fixtures map: 12-bit 256 and 3760.
const double kNarrowLo = 256.0 / 4095.0;
const double kNarrowHi = 3760.0 / 4095.0;

// PQ code value to display luminance in cd/m^2.
double eotf(double e)
{
  const double p = std::pow(e, 1.0 / kM2);
  return 10000.0 * std::pow(std::fmax(p - kC1, 0.0) / (kC2 - kC3 * p), 1.0 / kM1);
}

// Display luminance in cd/m^2 to PQ code value.
double inverseEotf(double l)
{
  const double y = std::pow(l / 10000.0, kM1);
  return std::pow((kC1 + kC2 * y) / (1.0 + kC3 * y), kM2);
}

// The PQ OOTF: scene light E in [0, 1] to display luminance in cd/m^2.
double g709(double e)
{
  return e <= 0.0003024 ? 267.84 * e : 1.099 * std::pow(59.5208 * e, 0.45) - 0.099;
}

double ootf(double e)
{
  return 100.0 * std::pow(g709(e), 2.4);
}

double inverseOotf(double l)
{
  const double ep = std::pow(l / 100.0, 1.0 / 2.4);
  return ep <= 0.081 ? ep / 267.84 : std::pow((ep + 0.099) / 1.099, 1.0 / 0.45) / 59.5208;
}

// BT.2020 RGB to XYZ, as the fixtures carry it.
const double kRgbToXyz[3][3] = {
  {0.63695805, 0.14461690, 0.16888098},
  {0.26270021, 0.67799807, 0.05930172},
  {0.00000000, 0.02807269, 1.06098506},
};

void mul(const double m[3][3], const double in[3], double out[3])
{
  for (int i = 0; i < 3; i++)
    out[i] = m[i][0] * in[0] + m[i][1] * in[1] + m[i][2] * in[2];
}

void invert(const double m[3][3], double out[3][3])
{
  const double a = m[0][0], b = m[0][1], c = m[0][2];
  const double d = m[1][0], e = m[1][1], f = m[1][2];
  const double g = m[2][0], h = m[2][1], i = m[2][2];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  out[0][0] = (e * i - f * h) / det; out[0][1] = (c * h - b * i) / det; out[0][2] = (b * f - c * e) / det;
  out[1][0] = (f * g - d * i) / det; out[1][1] = (a * i - c * g) / det; out[1][2] = (c * d - a * f) / det;
  out[2][0] = (d * h - e * g) / det; out[2][1] = (b * g - a * h) / det; out[2][2] = (a * e - b * d) / det;
}

// One multiProcessElementType tag, begun and ready to apply.
class TagEval
{
public:
  TagEval(CIccProfile *pProfile, icTagSignature sig, const std::string &name)
  {
    CIccTag *pTag = pProfile ? pProfile->FindTag(sig) : NULL;
    m_pMpe = pTag ? dynamic_cast<CIccTagMultiProcessElement*>(pTag) : NULL;
    check(m_pMpe != NULL, name + ": no multiProcessElementType tag");
    if (m_pMpe && m_pMpe->Begin())
      m_pApply.reset(m_pMpe->GetNewApply());
    check(!m_pMpe || m_pApply, name + ": Begin() or GetNewApply() failed");
  }

  bool ok() const { return m_pApply != NULL; }

  void apply(const double in[3], double out[3]) const
  {
    icFloatNumber src[3] = {(icFloatNumber)in[0], (icFloatNumber)in[1], (icFloatNumber)in[2]};
    icFloatNumber dst[3] = {0, 0, 0};
    m_pMpe->Apply(m_pApply.get(), dst, src);
    for (int i = 0; i < 3; i++)
      out[i] = dst[i];
  }

private:
  CIccTagMultiProcessElement *m_pMpe = NULL;
  std::unique_ptr<CIccApplyTagMpe> m_pApply;
};

// The profile iccFromXml would write for this document, read back from memory.
std::unique_ptr<CIccProfile> loadAsWritten(const std::string &path)
{
  CIccProfileXml xml;
  std::string parseStr;
  if (!xml.LoadXml(path.c_str(), NULL, &parseStr)) {
    check(false, "could not parse " + path + "\n" + parseStr);
    return NULL;
  }

  CIccMemIO io;
  if (!io.Alloc(1 << 20, true) || !xml.Write(&io)) {
    check(false, "could not write " + path);
    return NULL;
  }

  std::unique_ptr<CIccProfile> pProfile(ReadIccProfile(io.GetData(), (icUInt32Number)io.GetLength()));
  check(pProfile != NULL, "could not read back " + path);
  return pProfile;
}

std::string fmt(const char *szFormat, ...)
{
  char buf[256];
  va_list args;
  va_start(args, szFormat);
  std::vsnprintf(buf, sizeof(buf), szFormat, args);
  va_end(args);
  return buf;
}

// Code values: neutrals across the range (0.58 sits on 203 cd/m^2) and four
// chromatic triples, so a per-channel defect cannot hide behind a neutral.
const double kCodes[][3] = {
  {0.05, 0.05, 0.05}, {0.1, 0.1, 0.1}, {0.2, 0.2, 0.2}, {0.3, 0.3, 0.3},
  {0.4, 0.4, 0.4}, {0.5, 0.5, 0.5}, {0.58, 0.58, 0.58}, {0.7, 0.7, 0.7},
  {0.8, 0.8, 0.8}, {0.9, 0.9, 0.9}, {1.0, 1.0, 1.0},
  {0.7, 0.3, 0.1}, {0.2, 0.6, 0.4}, {0.5, 0.5, 0.9}, {0.9, 0.1, 0.6},
};
const int kNumCodes = (int)(sizeof(kCodes) / sizeof(kCodes[0]));

void checkColourProfile(const std::string &dir, const char *szName, bool bScene, bool bNarrow)
{
  const std::string name(szName);
  std::unique_ptr<CIccProfile> pProfile = loadAsWritten(dir + "/" + name + ".xml");
  if (!pProfile)
    return;

  TagEval fwd(pProfile.get(), icSigAToB1Tag, name + " AToB1Tag");
  TagEval rev(pProfile.get(), icSigBToA1Tag, name + " BToA1Tag");
  if (!fwd.ok() || !rev.ok())
    return;

  double xyzToRgb[3][3];
  invert(kRgbToXyz, xyzToRgb);

  for (int n = 0; n < kNumCodes; n++) {
    double dev[3], lin[3], ref[3], got[3], back[3], trip[3];
    for (int i = 0; i < 3; i++) {
      dev[i] = bNarrow ? kNarrowLo + kCodes[n][i] * (kNarrowHi - kNarrowLo) : kCodes[n][i];
      const double l = eotf(kCodes[n][i]);
      // 203 cd/m^2 of display light, or the scene light that displays at it, is
      // the PCS white the fixtures scale to.
      lin[i] = (bScene ? inverseOotf(l) : l) / 203.0;
    }
    mul(kRgbToXyz, lin, ref);

    fwd.apply(dev, got);
    for (int i = 0; i < 3; i++) {
      const double err = std::fabs(got[i] - ref[i]) / std::fmax(std::fabs(ref[i]), 1.0e-6);
      check(err < 1.0e-3,
            name + " AToB1Tag " + fmt("code %.3f: PCS %.6g, BT.2100 gives %.6g", kCodes[n][i], got[i], ref[i]));
    }

    // The reverse transform on the reference XYZ must return the code value.
    rev.apply(ref, back);
    for (int i = 0; i < 3; i++)
      check(std::fabs(back[i] - dev[i]) < 1.0e-4,
            name + " BToA1Tag " + fmt("code %.4f: got %.6f back from %.6g", dev[i], back[i], ref[i]));

    rev.apply(got, trip);
    for (int i = 0; i < 3; i++)
      check(std::fabs(trip[i] - dev[i]) < 1.0e-4,
            name + " round trip " + fmt("code %.4f: got %.6f (PCS %.6g)", dev[i], trip[i], got[i]));
  }

  // The spot values ST 2084 is usually quoted by, on the full-range neutral:
  // code 0.5 is 92.25 cd/m^2, and 203 cd/m^2 is code 0.5807.
  if (!bScene && !bNarrow) {
    double in[3] = {0.5, 0.5, 0.5}, out[3];
    fwd.apply(in, out);
    check(std::fabs(out[1] * 203.0 - eotf(0.5)) < 0.01,
          name + fmt(": code 0.5 decodes to %.4f cd/m^2, not %.4f", out[1] * 203.0, eotf(0.5)));

    double rgb[3] = {1.0, 1.0, 1.0}, xyz[3];
    mul(kRgbToXyz, rgb, xyz);
    rev.apply(xyz, out);
    check(std::fabs(out[1] - inverseEotf(203.0)) < 1.0e-4,
          name + fmt(": 203 cd/m^2 encodes to %.6f, not %.6f", out[1], inverseEotf(203.0)));
  }
}

void checkLink(const std::string &dir, const char *szName)
{
  const std::string name(szName);
  std::unique_ptr<CIccProfile> pProfile = loadAsWritten(dir + "/" + name + ".xml");
  if (!pProfile)
    return;

  TagEval link(pProfile.get(), icSigAToB0Tag, name + " AToB0Tag");
  if (!link.ok())
    return;

  // Normalised scene light in, normalised display light (cd/m^2 / 10000) out.
  const double kScene[] = {0.0001, 0.001, 0.01, 0.0307, 0.1, 0.3, 0.6, 1.0};
  for (double e : kScene) {
    const double in[3] = {e, e, e};
    double out[3];
    link.apply(in, out);
    const double ref = ootf(e) / 10000.0;
    check(std::fabs(out[0] - ref) / ref < 1.0e-3,
          name + fmt(" AToB0Tag scene %.4f: %.6g, OOTF / 10000 gives %.6g", e, out[0], ref));
  }
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <Testing/HDR directory>\n",
                 argv[0] ? argv[0] : "bt2100-pq-fixtures");
    return 2;
  }
  const std::string dir(argv[1]);

  // iccFromXml pushes these before parsing; without them the XML tag types resolve to
  // their non-XML base classes and ParseXml is never reached.
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  checkColourProfile(dir, "BT2100PQFullDisplay", false, false);
  checkColourProfile(dir, "BT2100PQNarrowDisplay", false, true);
  checkColourProfile(dir, "BT2100PQFullScene", true, false);
  checkColourProfile(dir, "BT2100PQNarrowScene", true, true);
  checkLink(dir, "BT2100PQSceneToDisplayLink");

  if (g_fail)
    std::fprintf(stderr, "[bt2100-pq-fixtures] %d assertion(s) failed\n", g_fail);
  else
    std::printf("[bt2100-pq-fixtures] PASS\n");
  return g_fail ? 1 : 0;
}
