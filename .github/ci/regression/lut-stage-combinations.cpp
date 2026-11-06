/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "IccProfile.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include <cstdio>
#include <memory>
#include <string>

enum Stage : unsigned {
  kB = 1,
  kM = 2,
  kMatrix = 4,
  kA = 8,
  kCLUT = 16,
};

struct Case {
  const char *name;
  unsigned stages;
  const char *finding;
};

static bool addCurves(LPIccCurve *curves)
{
  if (!curves)
    return false;
  for (int i = 0; i < 3; i++)
    curves[i] = new CIccTagCurve; // Zero entries encode the identity curve.
  return true;
}

static bool addIdentityMatrix(CIccMBB &tag)
{
  CIccMatrix *matrix = tag.NewMatrix();
  if (!matrix)
    return false;
  matrix->m_e[9] = matrix->m_e[10] = matrix->m_e[11] = 0.0f;
  return true;
}

static bool checkCase(const Case &test, bool atoB, icUInt32Number version)
{
  CIccProfile profile;
  profile.InitHeader();
  profile.m_Header.version = version;
  profile.m_Header.deviceClass = icSigDisplayClass;
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigXYZData;

  std::unique_ptr<CIccMBB> tag(atoB ? static_cast<CIccMBB *>(new CIccTagLutAtoB)
                                    : static_cast<CIccMBB *>(new CIccTagLutBtoA));
  if (!tag->Init(3, 3))
    return false;
  tag->SetColorSpaces(atoB ? icSigRgbData : icSigXYZData,
                      atoB ? icSigXYZData : icSigRgbData);

  if ((test.stages & kB) && !addCurves(tag->NewCurvesB()))
    return false;
  if ((test.stages & kM) && !addCurves(tag->NewCurvesM()))
    return false;
  if ((test.stages & kMatrix) && !addIdentityMatrix(*tag))
    return false;
  if ((test.stages & kA) && !addCurves(tag->NewCurvesA()))
    return false;
  if ((test.stages & kCLUT) && !tag->NewCLUT(2))
    return false;

  const icTagSignature sig = atoB ? icSigAToB0Tag : icSigBToA0Tag;
  std::string report;
  const icValidateStatus status = tag->Validate(icGetSigPath(sig), report, &profile);
  const bool pass = test.finding
    ? status == icValidateNonCompliant && report.find(test.finding) != std::string::npos
    : status == icValidateOK;
  if (!pass) {
    std::fprintf(stderr, "%s ICC %s %s: status=%d, expected %s\n%s",
                 atoB ? "mAB" : "mBA", version >= icVersionNumberV5 ? "5" : "4",
                 test.name, static_cast<int>(status),
                 test.finding ? test.finding : "valid", report.c_str());
  }
  return pass;
}

static bool checkTypeScope()
{
  const char *finding = "Matrix and M-curves must both be present or absent";
  CIccTagLutAtoB atoB;
  CIccTagLutBtoA btoA;
  CIccTagLut8 legacy8;
  CIccTagLut16 legacy16;
  CIccMBB *tags[] = {&atoB, &btoA, &legacy8, &legacy16};
  const icTagSignature sigs[] = {icSigAToB3Tag, icSigBToA3Tag,
                                 icSigAToB0Tag, icSigAToB0Tag};
  for (int i = 0; i < 4; i++) {
    if (!tags[i]->Init(3, 3) || !addIdentityMatrix(*tags[i]))
      return false;
    std::string report;
    const icValidateStatus status = tags[i]->Validate(icGetSigPath(sigs[i]), report);
    const bool hasFinding = report.find(finding) != std::string::npos;
    if (i < 2 ? status < icValidateNonCompliant || !hasFinding : hasFinding) {
      std::fprintf(stderr, "tag type scope %d: status=%d, report=%s\n",
                   i, static_cast<int>(status), report.c_str());
      return false;
    }
  }
  return true;
}

int main()
{
  // ICC.1:2022 10.12-10.13 and ICC.2:2023 10.2.13-10.2.14 permit
  // exactly B, B+M+matrix, B+A+CLUT, and all five stages.
  const Case cases[] = {
    {"B only", kB, nullptr},
    {"B+M+matrix", kB | kM | kMatrix, nullptr},
    {"B+A+CLUT", kB | kA | kCLUT, nullptr},
    {"all stages", kB | kM | kMatrix | kA | kCLUT, nullptr},
    {"matrix only", kMatrix, "Matrix and M-curves must both be present or absent"},
    {"no B", kM | kMatrix, "B-curves must be present"},
    {"M without matrix", kB | kM, "Matrix and M-curves must both be present or absent"},
    {"matrix without M", kB | kMatrix, "Matrix and M-curves must both be present or absent"},
    {"A without CLUT", kB | kA, "CLUT and A-curves must both be present or absent"},
    {"CLUT without A", kB | kCLUT, "CLUT and A-curves must both be present or absent"},
  };

  bool pass = true;
  for (const icUInt32Number version : {icVersionNumberV4_4, icVersionNumberV5_1}) {
    for (bool atoB : {true, false}) {
      for (const Case &test : cases)
        pass = checkCase(test, atoB, version) && pass;
    }
  }
  pass = checkTypeScope() && pass;
  return pass ? 0 : 1;
}
