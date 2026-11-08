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

// #3012/#3014: reusing one LUT reader with different channel counts must
// replace its curve arrays before the new header changes their cleanup bounds.
#include "IccIO.h"
#include "IccTagLut.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

template <typename Tag>
static bool readTag(Tag &tag, std::vector<icUInt8Number> &bytes)
{
  CIccMemIO io;
  return io.Attach(bytes.data(), bytes.size()) &&
         tag.Read((icUInt32Number)bytes.size(), &io);
}

static std::vector<icUInt8Number> makeLegacyTag(bool is16, unsigned outputs)
{
  const unsigned inputs = 1;
  const unsigned entries = is16 ? 2 : 256;
  const size_t header = is16 ? 52 : 48;
  const size_t sample = is16 ? 2 : 1;
  std::vector<icUInt8Number> bytes(
    header + inputs * entries * sample + 2 * outputs * sample +
    outputs * entries * sample, 0);
  bytes[0] = 'm';
  bytes[1] = 'f';
  bytes[2] = 't';
  bytes[3] = is16 ? '2' : '1';
  bytes[8] = inputs;
  bytes[9] = (icUInt8Number)outputs;
  bytes[10] = 2;
  if (is16) {
    bytes[49] = entries;
    bytes[51] = entries;
  }
  return bytes;
}

template <typename Tag>
static bool testLegacy(const char *name, bool is16)
{
  Tag tag;
  auto one = makeLegacyTag(is16, 1);
  auto two = makeLegacyTag(is16, 2);
  if (!readTag(tag, one))
    return false;

  // This transfers B curves into M curves; the next binary Read must clear
  // both the old allocations and the transfer flag before installing stages.
  tag.SetColorSpaces(icSigXYZData, icSigRgbData);
  if (!readTag(tag, two) || tag.SwapMBCurves())
    return false;
  // With the transfer flag left set, Write() looked for B curves in the M
  // array that the re-read had released, and refused a valid tag.
  CIccMemIO out;
  if (!out.Alloc(1 << 16, true) || !tag.Write(&out) || !readTag(tag, one))
    return false;

  auto truncated = makeLegacyTag(is16, 3);
  truncated.resize((is16 ? 52 : 48) + (is16 ? 4 : 256) + 1);
  if (readTag(tag, truncated) || !readTag(tag, two))
    return false;

  std::printf("%s same-object reads passed\n", name);
  return true;
}

template <typename Tag>
static bool testLegacyModeCarry(const char *name, bool is16, bool modeCarry = true)
{
  auto two = makeLegacyTag(is16, 2); // One input channel, two output channels.
  Tag control;
  if (!readTag(control, two) || !control.RawInputMatrix() || !control.IsInputB() ||
      !control.GetCurvesA() || !control.GetCurvesA()[1])
    return false;
  if (!modeCarry) {
    std::printf("%s fresh-object control passed\n", name);
    return true;
  }

  Tag tag;
  Tag donor;
  donor.SetRawInputMatrix(false);
  static_cast<CIccMBB &>(tag) = static_cast<const CIccMBB &>(donor);
  if (!tag.RawInputMatrix() || !tag.IsInputB())
    return false;

  // Base assignment normalizes the raw state to the legacy layout. Read must
  // also restore it when an existing object arrives with stale raw state.
  tag.SetRawInputMatrix(false);
  if (!readTag(tag, two) || !tag.RawInputMatrix() || !tag.IsInputB() ||
      !tag.GetCurvesA() || !tag.GetCurvesA()[1])
    return false;

  std::printf("%s mode-carry read passed\n", name);
  return true;
}

class LegacyLut8Probe : public CIccTagLut8 {
public:
  bool RawInputMatrix() const { return m_bInputMatrix; }
  void SetRawInputMatrix(bool value) { m_bInputMatrix = value; }
};

class LegacyLut16Probe : public CIccTagLut16 {
public:
  bool RawInputMatrix() const { return m_bInputMatrix; }
  void SetRawInputMatrix(bool value) { m_bInputMatrix = value; }
};

template <typename Tag>
static bool testCrossLayoutAssignment(const char *name)
{
  CIccTagLutAtoB donor;
  if (!donor.Init(1, 2))
    return false;
  LPIccCurve *donorCurves = donor.NewCurvesA();
  if (!donorCurves)
    return false;
  donorCurves[0] = new CIccTagCurve;

  Tag target;
  if (!target.Init(1, 2))
    return false;
  LPIccCurve *targetCurves = target.NewCurvesA();
  if (!targetCurves)
    return false;
  targetCurves[0] = new CIccTagCurve;
  targetCurves[1] = new CIccTagCurve;
  CIccCurve *first = targetCurves[0];
  CIccCurve *second = targetCurves[1];

  static_cast<CIccMBB &>(target) = static_cast<const CIccMBB &>(donor);
  if (target.GetCurvesA() != targetCurves || targetCurves[0] != first ||
      targetCurves[1] != second || !target.RawInputMatrix() ||
      !target.IsInputB())
    return false;

  std::printf("%s incompatible-layout assignment passed\n", name);
  return true;
}

template <typename Tag>
static bool testLegacyCopyWithRawMode(const char *name)
{
  Tag donor;
  if (!donor.Init(1, 2))
    return false;
  LPIccCurve *curves = donor.NewCurvesA();
  if (!curves)
    return false;
  curves[0] = new CIccTagCurve;
  curves[1] = new CIccTagCurve;
  donor.SetRawInputMatrix(false);

  Tag copy(donor);
  Tag assigned;
  static_cast<CIccMBB &>(assigned) = static_cast<const CIccMBB &>(donor);
  donor.SetRawInputMatrix(true);
  if (!copy.RawInputMatrix() || !copy.IsInputB() || !copy.GetCurvesA() ||
      !copy.GetCurvesA()[0] || !copy.GetCurvesA()[1] ||
      !assigned.RawInputMatrix() || !assigned.IsInputB() ||
      !assigned.GetCurvesA() || !assigned.GetCurvesA()[0] ||
      !assigned.GetCurvesA()[1])
    return false;

  std::printf("%s legacy copy normalization passed\n", name);
  return true;
}

static bool testBaseCrossModeCopy()
{
  CIccTagLutAtoB donor;
  if (!donor.Init(1, 2))
    return false;
  LPIccCurve *donorCurves = donor.NewCurvesA();
  if (!donorCurves)
    return false;
  donorCurves[0] = new CIccTagCurve;

  CIccMBB target;
  if (!target.Init(1, 2))
    return false;
  LPIccCurve *targetCurves = target.NewCurvesA();
  if (!targetCurves)
    return false;
  targetCurves[0] = new CIccTagCurve;
  targetCurves[1] = new CIccTagCurve;

  target = donor;
  CIccMBB copy(donor);
  if (target.IsInputMatrix() || !target.GetCurvesA() ||
      !target.GetCurvesA()[0] || copy.IsInputMatrix() ||
      !copy.GetCurvesA() || !copy.GetCurvesA()[0])
    return false;

  std::puts("MBB cross-mode copy passed");
  return true;
}

static std::vector<icUInt8Number> makeMbbTag(bool atoB, unsigned channels)
{
  std::unique_ptr<CIccTagLutAtoB> tag(
    atoB ? new CIccTagLutAtoB : new CIccTagLutBtoA);
  if (!tag->Init(atoB ? 1 : (icUInt8Number)channels,
                 atoB ? (icUInt8Number)channels : 1))
    return {};

  LPIccCurve *curves = tag->NewCurvesB();
  if (!curves)
    return {};
  for (unsigned i=0; i<channels; i++)
    curves[i] = new CIccTagCurve;

  CIccMemIO io;
  if (!io.Alloc(1024, true) || !tag->Write(&io) || io.Tell() < 0)
    return {};
  return std::vector<icUInt8Number>(io.GetData(), io.GetData() + io.Tell());
}

static bool testMbb(bool atoB)
{
  auto one = makeMbbTag(atoB, 1);
  auto two = makeMbbTag(atoB, 2);
  if (one.empty() || two.empty())
    return false;

  std::unique_ptr<CIccTagLutAtoB> tag(
    atoB ? new CIccTagLutAtoB : new CIccTagLutBtoA);
  if (!readTag(*tag, one) || !readTag(*tag, two) || !readTag(*tag, one))
    return false;

  auto truncated = two;
  truncated.resize(36);
  if (readTag(*tag, truncated) || !readTag(*tag, two))
    return false;
  return true;
}

static std::vector<icUInt8Number> makeLut(bool is16, unsigned inputs,
                                           unsigned outputs)
{
  const unsigned entries = is16 ? 2 : 256;
  const size_t sample = is16 ? 2 : 1;
  const size_t header = is16 ? 52 : 48;
  const size_t clutEntries = (size_t(1) << inputs) * outputs;
  std::vector<icUInt8Number> data(header + inputs * entries * sample +
                                 clutEntries * sample + outputs * entries * sample, 0);
  data[0] = 'm'; data[1] = 'f'; data[2] = 't'; data[3] = is16 ? '2' : '1';
  data[8] = (icUInt8Number)inputs;
  data[9] = (icUInt8Number)outputs;
  data[10] = 2;
  if (is16) {
    data[48] = 0; data[49] = entries;
    data[50] = 0; data[51] = entries;
  }
  return data;
}

template <typename T>
static bool readLut(T &tag, bool is16, unsigned inputs, unsigned outputs)
{
  std::vector<icUInt8Number> data = makeLut(is16, inputs, outputs);
  CIccMemIO io;
  if (!io.Attach(data.data(), data.size()) ||
      !tag.Read((icUInt32Number)data.size(), &io))
    return false;

  if (tag.InputChannels() != inputs || tag.OutputChannels() != outputs)
    return false;
  LPIccCurve *a = tag.GetCurvesA();
  LPIccCurve *b = tag.GetCurvesB();
  if (!a || !b)
    return false;
  for (unsigned i = 0; i < outputs; i++) {
    if (!a[i])
      return false;
  }
  for (unsigned i = 0; i < inputs; i++) {
    if (!b[i])
      return false;
  }
  return true;
}

template <typename T>
static bool checkLut(bool is16)
{
  {
    T tag;
    if (!readLut(tag, is16, 1, 1) || !readLut(tag, is16, 1, 2) ||
        !readLut(tag, is16, 2, 1))
      return false;
  }

  // A rejected second header must also leave the old allocation counts intact
  // for the destructor. The old reader changed m_nOutput before rejecting 16.
  T tag;
  if (!readLut(tag, is16, 1, 1))
    return false;
  std::vector<icUInt8Number> bad = makeLut(is16, 1, 1);
  bad[9] = 16;
  CIccMemIO io;
  if (!io.Attach(bad.data(), bad.size()) ||
      tag.Read((icUInt32Number)bad.size(), &io))
    return false;
  return tag.InputChannels() == 1 && tag.OutputChannels() == 1;
}

template <typename T>
static bool checkMbb(bool inputMatrix)
{
  T tag;
  if (!tag.Init(1, 1))
    return false;
  LPIccCurve *old = inputMatrix ? tag.NewCurvesA() : tag.NewCurvesB();
  if (!old)
    return false;

  std::array<icUInt8Number, 32> data{};
  data[0] = 'm'; data[1] = inputMatrix ? 'B' : 'A';
  data[2] = inputMatrix ? 'A' : 'B'; data[3] = ' ';
  data[8] = 1; data[9] = 2;
  CIccMemIO io;
  if (!io.Attach(data.data(), data.size()) ||
      !tag.Read((icUInt32Number)data.size(), &io))
    return false;
  return tag.OutputChannels() == 2 && !tag.GetCurvesA() && !tag.GetCurvesB();
}

int main(int argc, char **argv)
{
  if (argc == 2 && !std::strcmp(argv[1], "--mode-control-8"))
    return testLegacyModeCarry<LegacyLut8Probe>("LUT8", false, false) ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--mode-control-16"))
    return testLegacyModeCarry<LegacyLut16Probe>("LUT16", true, false) ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--mode-carry-8"))
    return testLegacyModeCarry<LegacyLut8Probe>("LUT8", false) ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--mode-carry-16"))
    return testLegacyModeCarry<LegacyLut16Probe>("LUT16", true) ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--cross-layout-8"))
    return testCrossLayoutAssignment<LegacyLut8Probe>("LUT8") ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--cross-layout-16"))
    return testCrossLayoutAssignment<LegacyLut16Probe>("LUT16") ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--copy-raw-8"))
    return testLegacyCopyWithRawMode<LegacyLut8Probe>("LUT8") ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--copy-raw-16"))
    return testLegacyCopyWithRawMode<LegacyLut16Probe>("LUT16") ? 0 : 1;
  if (argc == 2 && !std::strcmp(argv[1], "--base-cross-mode"))
    return testBaseCrossModeCopy() ? 0 : 1;
  if (argc != 1)
    return 2;

  if (!testLegacy<CIccTagLut8>("LUT8", false) ||
      !testLegacy<CIccTagLut16>("LUT16", true) ||
      !testLegacyModeCarry<LegacyLut8Probe>("LUT8", false) ||
      !testLegacyModeCarry<LegacyLut16Probe>("LUT16", true) ||
      !testCrossLayoutAssignment<LegacyLut8Probe>("LUT8") ||
      !testCrossLayoutAssignment<LegacyLut16Probe>("LUT16") ||
      !testLegacyCopyWithRawMode<LegacyLut8Probe>("LUT8") ||
      !testLegacyCopyWithRawMode<LegacyLut16Probe>("LUT16") ||
      !testBaseCrossModeCopy() ||
      !testMbb(true) || !testMbb(false) ||
      !checkLut<CIccTagLut8>(false) || !checkLut<CIccTagLut16>(true) ||
      !checkMbb<CIccTagLutAtoB>(false) || !checkMbb<CIccTagLutBtoA>(true)) {
    std::fprintf(stderr, "lut-read-reuse: repeated Read failed\n");
    return 1;
  }
  return 0;
}
