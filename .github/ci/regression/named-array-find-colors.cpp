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
// CTest contract for CIccArrayNamedColor's colour searches (#2879).
//
// A v5 named colour array keeps each colour's device, PCS and spectral values
// in separate struct members. The reverse lookups read the wrong ones:
//   - FindPcsColor() compared against the DEVICE member, with the device
//     stride, and on a gray array (one device value) its three-value read
//     failed and left the compared Lab uninitialised;
//   - FindSpectralColor() also read the device member, so a colour with only
//     spectral data was never found;
//   - FindDeviceColor() tested the tint-ramp length where it meant "every
//     sample matched", so it returned colours whose values differ.
// CIccXformNamedColor::Apply() also converted an XYZ PCS value to Lab before
// handing it to FindPcsColor(), which converts XYZ itself.
//
// Each search is run against a three-colour array arranged so that reading the
// wrong member, or not comparing values, picks a different colour: an unfixed
// library fails these checks in any build, without a sanitizer.
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccArrayBasic.h"
#include "IccStructBasic.h"
#include "IccTagBasic.h"
#include "IccTagComposite.h"
#include "IccUtil.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_failures = 0;

void check(bool ok, const char *label)
{
  std::printf("%s %s\n", ok ? "ok:  " : "FAIL:", label);
  if (!ok)
    ++g_failures;
}

typedef std::vector<icFloatNumber> Vals;

void attachValues(CIccTagStruct *pStruct, icUInt32Number sig, const Vals &vals)
{
  if (vals.empty())
    return;
  CIccTagFloat32 *pTag = new CIccTagFloat32((int)vals.size());
  for (size_t i = 0; i < vals.size(); i++)
    (*pTag)[(icUInt32Number)i] = vals[i];
  pStruct->AttachElem(sig, pTag);
}

CIccTag *colour(icStructSignature sigStruct, const char *szName,
                const Vals &dev, const Vals &pcs, const Vals &spec)
{
  CIccTagStruct *pStruct = new CIccTagStruct();
  pStruct->SetTagStructType(sigStruct);
  if (szName) {
    CIccTagUtf8Text *pName = new CIccTagUtf8Text();
    pName->SetText(szName);
    pStruct->AttachElem(icSigNmclNameMbr, pName);
  }
  attachValues(pStruct, icSigNmclDeviceDataMbr, dev);
  attachValues(pStruct, icSigNmclPcsDataMbr, pcs);
  attachValues(pStruct, icSigNmclSpectralDataMbr, spec);
  return pStruct;
}

struct Entry {
  const char *name;
  Vals dev, pcs, spec;
};

// Owns a named colour array: index 0 is the tint-zero struct, then 'entries'.
class Fixture {
public:
  Fixture(icColorSpaceSignature csPcs, icColorSpaceSignature csDevice,
          const std::vector<Entry> &entries)
    : m_pTag(new CIccTagArray(icSigNamedColorArray)), m_pArray(NULL)
  {
    m_pTag->SetSize((icUInt32Number)entries.size() + 1);
    m_pTag->AttachTag(0, colour(icSigTintZeroStruct, NULL, Vals(), Vals(), Vals()));
    for (size_t i = 0; i < entries.size(); i++)
      m_pTag->AttachTag((icUInt32Number)i + 1,
                        colour(icSigNamedColorStruct, entries[i].name, entries[i].dev,
                               entries[i].pcs, entries[i].spec));

    m_pArray = (CIccArrayNamedColor *)m_pTag->GetArrayHandler();
    if (m_pArray) {
      icSpectralRange range;
      std::memset(&range, 0, sizeof(range));
      range.start = icRange400nm;
      range.end = icRange700nm;
      range.steps = 3;
      m_pArray->SetColorSpaces(csPcs, csDevice,
                               icNColorSpaceSig(icSigReflectanceSpectralData, 3), &range);
      if (!m_pArray->Begin())
        m_pArray = NULL;
    }
  }
  ~Fixture() { delete m_pTag; }

  CIccArrayNamedColor *array() const { return m_pArray; }

private:
  CIccTagArray *m_pTag;
  CIccArrayNamedColor *m_pArray;
};

std::string nameOf(CIccStructNamedColor *pColor)
{
  return pColor ? pColor->getName() : std::string("(none)");
}

void pcsSearchUsesThePcsMember()
{
  // A's PCS is L* 80 and its device value is black; B's PCS is L* 20 and its
  // device value reads as L* 80 if taken for Lab.
  Fixture f(icSigLabData, icSigRgbData, {
    { "A", { 0, 0, 0 },  { 80, 0, 0 }, {} },
    { "B", { 80, 0, 0 }, { 20, 0, 0 }, {} },
  });
  check(f.array() != NULL, "Lab/RGB array begins");
  if (!f.array())
    return;
  icFloatNumber lab[3] = { 80, 0, 0 };
  check(nameOf(f.array()->FindPcsColor(lab)) == "A",
        "FindPcsColor(L* 80) finds the colour whose PCS is L* 80");
}

void pcsSearchOnAGrayArray()
{
  // One device value per colour: the three-value device read used to fail.
  Fixture f(icSigLabData, icSigGrayData, {
    { "Dark",  { 0.1f }, { 20, 0, 0 }, {} },
    { "Light", { 0.9f }, { 90, 0, 0 }, {} },
  });
  check(f.array() != NULL, "Lab/gray array begins");
  if (!f.array())
    return;
  icFloatNumber lab[3] = { 88, 0, 0 };
  check(nameOf(f.array()->FindPcsColor(lab)) == "Light",
        "gray array: FindPcsColor(L* 88) finds Light");
}

void pcsSearchOnAnXyzArray()
{
  // XYZ PCS: the stored values are XYZ and the query is XYZ.
  Fixture f(icSigXYZData, icSigRgbData, {
    { "White", { 1, 1, 1 }, { 0.9642f, 1.0f, 0.8249f }, {} },
    { "Grey",  { 0.5f, 0.5f, 0.5f }, { 0.1777f, 0.1842f, 0.1520f }, {} },
  });
  check(f.array() != NULL, "XYZ/RGB array begins");
  if (!f.array())
    return;
  icFloatNumber xyz[3] = { 0.18f, 0.19f, 0.15f };
  check(nameOf(f.array()->FindPcsColor(xyz)) == "Grey",
        "XYZ array: FindPcsColor(mid grey XYZ) finds Grey");
}

void spectralSearchUsesTheSpectralMember()
{
  Fixture f(icSigLabData, icSigRgbData, {
    { "Flat",  { 0.2f, 0.2f, 0.2f }, { 50, 0, 0 }, { 0.5f, 0.5f, 0.5f } },
    { "Ramp",  { 0.5f, 0.5f, 0.5f }, { 60, 0, 0 }, { 0.1f, 0.5f, 0.9f } },
  });
  check(f.array() != NULL, "spectral array begins");
  if (!f.array())
    return;
  icFloatNumber spec[3] = { 0.1f, 0.5f, 0.9f };
  check(nameOf(f.array()->FindSpectralColor(spec)) == "Ramp",
        "FindSpectralColor finds the colour whose spectral member matches");
}

void deviceSearchComparesValues()
{
  // Three tint steps of RGB per colour, so the ramp length equals the device
  // channel count and the old test returned the first colour regardless.
  Fixture f(icSigLabData, icSigRgbData, {
    { "First",  { 0, 0, 0,  0.25f, 0.25f, 0.25f,  0.5f, 0.5f, 0.5f }, { 50, 0, 0 }, {} },
    { "Second", { 0, 0, 0,  0.45f, 0.45f, 0.45f,  0.9f, 0.9f, 0.9f }, { 90, 0, 0 }, {} },
  });
  check(f.array() != NULL, "RGB array begins");
  if (!f.array())
    return;
  icFloatNumber match[3] = { 0.9f, 0.9f, 0.9f };
  check(nameOf(f.array()->FindDeviceColor(match)) == "Second",
        "FindDeviceColor finds the colour whose full-tint device value matches");
  icFloatNumber none[3] = { 0.7f, 0.7f, 0.7f };
  check(f.array()->FindDeviceColor(none) == NULL,
        "FindDeviceColor finds nothing for a value no colour has");
}

} // namespace

int main()
{
  pcsSearchUsesThePcsMember();
  pcsSearchOnAGrayArray();
  pcsSearchOnAnXyzArray();
  spectralSearchUsesTheSpectralMember();
  deviceSearchComparesValues();

  if (g_failures)
    std::printf("%d assertion(s) failed\n", g_failures);
  else
    std::printf("all assertions passed\n");
  return g_failures;
}
