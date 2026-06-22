/*
  File:     IccVizModel.cpp

  Contains: Implementation of the data-first profile visualization model (IccVizModel.hpp).

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
 * IccVizModel implementation  -  see IccVizModel.hpp.
 *
 * The producers below compute the profile's chromaticity, tone-curve, named-
 * colour and CLUT visualizations and return them as data structures (point
 * series, axis hints, raster samples) rather than as PDF/TIFF drawing commands.
 */

#include "IccVizModel.hpp"
#include "IccVizMath.hpp"     // shared XYZ->xy + planckian math

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccTagComposite.h"   // CIccTagArray / CIccTagStruct (v5 named-colour arrays)
#include "IccUtil.h"

#include "spectralLocus.hpp"   // const spectralLocus2degree (internal linkage)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace iccviz {
namespace {

const float chromaticityChartScale = 0.85f;
const float abChartScale = 2 * 130.0f;
const float kNaN = std::numeric_limits<float>::quiet_NaN();

// -- diagnostic output state (see SetSilent / SetDiagnosticContext) ------------
// Defaults reproduce iccProfileVisualize's top-level call: NOT silent, no prefix.
bool g_silent = false;
std::string g_diagContext;

bool effectiveSilent(Verbosity v) {
  switch (v) {
    case Verbosity::Stderr: return false;
    case Verbosity::Silent: return true;
    default:                return g_silent;   // Verbosity::Default
  }
}

// Echo a result's diagnostics to stderr unless silenced. The message text already
// carries the exact upstream wording (Skipping ... / WARNING - ... / ERROR - ...); the
// optional context reproduces iccProfileVisualize's leading "<filename>: ".
void emitDiagnostics(const std::vector<Diagnostic>& diags, Verbosity v) {
  if (effectiveSilent(v)) return;
  for (const Diagnostic& d : diags) {
    if (g_diagContext.empty())
      std::fprintf(stderr, "%s\n", d.message.c_str());
    else
      std::fprintf(stderr, "%s: %s\n", g_diagContext.c_str(), d.message.c_str());
  }
}

std::string sigStr(icTagSignature sig) {
  char buf[64];
  return std::string(icGetSigStr(buf, sizeof buf, static_cast<icUInt32Number>(sig)));
}

// -- colour helpers  -  single-sourced in IccVizMath.hpp so the XYZ->xy +
//    planckian math lives in exactly one place.
using iccvizmath::XY;
using iccvizmath::xyFromICCXYZ;
using iccvizmath::xyFromXYZFloat;
using iccvizmath::approxPlanck;

/*
label points for spectrum in nm
sorta, kinda evenly spaced, plus endpoints
 */
const std::vector<int> locusLabelWavelengths =
{
  360,
  460, 450,
  470, 475, 480, 485, 490, 495, 500, 505, 510, 515,
  520, 530, 540, 550, 560, 570, 580, 590, 600, 610, 620,
  640,
  700
};

std::string channelName(int index, bool useInput, icColorSpaceSignature inSpace,
                        icColorSpaceSignature outSpace, int inCh, int outCh) {
  char buf[128];
  icColorIndexName(buf, 128, useInput ? inSpace : outSpace, index,
                   useInput ? inCh : outCh, useInput ? "In" : "Out");
  return std::string(buf);
}

unsigned char clipU8(icFloatNumber v) {
  if (std::isnan(v)) return 0;
  if (std::isinf(v)) return 255;
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<unsigned char>(v);
}
unsigned short clipU16(icFloatNumber v) {
  if (std::isnan(v)) return 0;
  if (std::isinf(v)) return 65535;
  if (v < 0) return 0;
  if (v > 65535) return 65535;
  return static_cast<unsigned short>(v);
}

int photometricFromSpace(icColorSpaceSignature s) {
  switch (s) {
    case icSigRgbData: case icSigCmyData: case icSigXYZData: case icSigLuvData:
    case icSigYCbCrData: case icSigYxyData: case icSigHsvData: case icSigHlsData:
      return 2;  // RGB
    case icSigCmykData: return 5;            // CMYK
    case icSigLabData:  return 8;            // CIELAB
    case icSigGrayData: case icSigGamutData: return 1;  // BlackIsZero
    default: return 0;                        // WhiteIsZero (N-ink fallback)
  }
}

// -- curve description + validation gate --------------------------------------

// Validate a curve, returning the report text by reference. A status
// > icValidateWarning means the curve is malformed (e.g. gamma 0, bad LUT
// size). Callers no longer DROP failing curves: they enumerate them anyway and
// surface this report as a diagnostic, so a malformed curve shows its reason
// instead of the graph silently vanishing.
icValidateStatus curveValidate(CIccCurve* curve, const std::string& sigDesc,
                               std::string& report) {
  return curve->Validate(":" + sigDesc, report, nullptr);
}

std::string describeCurve(CIccCurve* curve) {
  if (auto* tc = dynamic_cast<CIccTagCurve*>(curve)) {
    auto size = tc->GetSize();
    if (size == 0) {
      return "Y = X";
    } else if (size == 1) {
      icFloatNumber value0 = (*tc)[0];
      icFloatNumber dGamma = (icFloatNumber)(value0 * 256.0f);
      return "Y = X ^ " + std::to_string(dGamma);
    } else {
      return "LookupTable[" + std::to_string(size) + "]";
    }
  }
  std::string desc;
  curve->Describe(desc, 100);
  return desc;
}

// -- producers ----------------------------------------------------------------

Graph buildCurveGraph(CIccCurve* curve, const std::string& title) {
  Graph g;
  g.title = title;
  g.description = describeCurve(curve);
  g.xAxis = Axis{"Input", 0.0f, 1.0f, false};
  g.yAxis = Axis{"Output", 0.0f, 1.0f, false};

  int steps = 1000;
  if (auto* tc = dynamic_cast<CIccTagCurve*>(curve))
    steps = std::max(1000, static_cast<int>(tc->GetSize()));
  if (curve->IsIdentity()) steps = 2;

  Series data;
  data.id = "curve"; data.name = title; data.role = Role::Primary;
  data.shape = Shape::Polyline; data.colorHint = "neutral";
  data.verts.reserve(steps + 1);
  for (int i = 0; i <= steps; ++i) {
    float in = i / static_cast<float>(steps);
    float out = curve->Apply(in);
    if (std::isnan(out)) out = 0.0f;
    if (std::isinf(out)) out = 1.0f;
    out = std::min(1.0f, std::max(0.0f, out));
    data.verts.push_back(Vertex{in, out, "", kNaN});
  }
  g.series.push_back(std::move(data));

  Series ident;
  ident.id = "identity"; ident.name = "Identity"; ident.role = Role::Hint;
  ident.shape = Shape::Polyline; ident.colorHint = "neutral";
  ident.verts = {Vertex{0, 0, "", kNaN}, Vertex{1, 1, "", kNaN}};
  g.series.push_back(std::move(ident));
  return g;
}

void addPrimaryPoint(Graph& g, Series& s, CIccTag* tag, const char* label,
                     const char* colorHint) {
  auto* xyzTag = dynamic_cast<CIccTagXYZ*>(tag);
  if (!xyzTag) return;
  const icXYZNumber* xyz = xyzTag->GetXYZ(0);
  if (!xyz) return;
  XY p = xyFromICCXYZ(xyz);
  s.verts.push_back(Vertex{p.x, p.y, label, kNaN});
  (void)g; (void)colorHint;   // reserved for a future per-point colour hint
}

Graph buildChromaticityGraph(CIccProfile* pIcc) {
  Graph g;
  g.title = "Chromaticity xy";
  g.xAxis = Axis{"x (CIE 1931)", 0.0f, chromaticityChartScale, true};
  g.yAxis = Axis{"y (CIE 1931)", 0.0f, chromaticityChartScale, true};

  // spectral locus (Hint, closed horseshoe).
  Series locus;
  locus.id = "locus"; locus.name = "Spectral locus"; locus.role = Role::Hint;
  locus.shape = Shape::ClosedPath; locus.colorHint = "locus";
  locus.verts.reserve(spectralLocus2degree.size());
  for (const auto& p : spectralLocus2degree)
    locus.verts.push_back(Vertex{static_cast<float>(p.x), static_cast<float>(p.y), "", kNaN});
  g.series.push_back(std::move(locus));

  // labels for spectral locus (Hint).
  Series labels;
  labels.id = "locusLabels"; labels.name = "Wavelengths"; labels.role = Role::Hint;
  labels.shape = Shape::Scatter; labels.auxKind = "nm"; labels.colorHint = "locus";
  const int wavelengthOffset = spectralLocus2degree[0].wavelength;
  for (auto& nm : locusLabelWavelengths) {
    size_t index = static_cast<size_t>(nm - wavelengthOffset);
    if (index >= spectralLocus2degree.size()) continue;
    const auto& p = spectralLocus2degree[index];
    labels.verts.push_back(Vertex{static_cast<float>(p.x), static_cast<float>(p.y),
                                  std::to_string(nm), static_cast<float>(nm)});
  }
  g.series.push_back(std::move(labels));

  // plankian white curve (Hint).
  Series planck;
  planck.id = "planckian"; planck.name = "Planckian locus"; planck.role = Role::Hint;
  planck.shape = Shape::Polyline; planck.auxKind = "kelvin"; planck.colorHint = "neutral";
  const float start_temp = 1500.0f;   // degrees Kelvin
  const float end_temp = 20000.0f;
  const float temp_step = 200.0f;
  for (float temp = start_temp; temp <= end_temp; temp += temp_step) {
    XY p = approxPlanck(temp);
    planck.verts.push_back(Vertex{p.x, p.y, "", temp});
  }
  g.series.push_back(std::move(planck));

  // Primaries + white (Primary).
  Series prim;
  prim.id = "primaries"; prim.name = "Primaries"; prim.role = Role::Primary;
  prim.shape = Shape::Scatter;
  CIccTag* w = pIcc->FindTag(icSigMediaWhitePointTag);
  if (w) addPrimaryPoint(g, prim, w, "White", "white");
  CIccTag* r = pIcc->FindTag(icSigRedColorantTag);
  CIccTag* gr = pIcc->FindTag(icSigGreenColorantTag);
  CIccTag* b = pIcc->FindTag(icSigBlueColorantTag);
  Series gamut;
  gamut.id = "gamut"; gamut.name = "Gamut"; gamut.role = Role::Primary;
  gamut.shape = Shape::ClosedPath; gamut.colorHint = "neutral";
  if (r && gr && b) {
    size_t base = prim.verts.size();
    addPrimaryPoint(g, prim, r, "R", "R");
    addPrimaryPoint(g, prim, gr, "G", "G");
    addPrimaryPoint(g, prim, b, "B", "B");
    for (size_t i = base; i < prim.verts.size(); ++i)
      gamut.verts.push_back(Vertex{prim.verts[i].x, prim.verts[i].y, "", kNaN});
  }
  g.series.push_back(std::move(prim));
  if (!gamut.verts.empty()) g.series.push_back(std::move(gamut));
  return g;
}

struct NamedLab { std::string name; float L, a, b; };

// Collect named/colorant colours as Lab + name. `diag` (when supplied) carries
// the granular skip reasons  -  including the IccProfLib validation `report`
// string, which the data-first path would otherwise discard. Enumerate() probes
// with diag==nullptr (silent skip); RenderGraph() pulls the reasons through.
bool collectNamedColors(CIccProfile* pIcc, CIccTag* tag, const std::string& sigDesc,
                        std::vector<NamedLab>& out, std::string& title,
                        std::vector<Diagnostic>* diag = nullptr) {
  auto record = [&](Severity sev, const std::string& msg) -> bool {
    if (diag) diag->push_back({sev, msg});
    return false;
  };

  icTagTypeSignature type = tag->GetType();
  icFloatNumber illum[3];
  pIcc->getNormIlluminantXYZ(illum);

  // PCS basis for the colour conversion is the profile header PCS  -  matching
  // iccProfileVisualize, which reads pIcc->m_Header.pcs (not the table's own
  // PCS) and uses it for every named-colour type. Anything that isn't XYZ/Lab
  // can't be plotted: icSigNoColorData (spectral / iccMAX) skips silently, while
  // any other space records a warning.
  icColorSpaceSignature pcs = pIcc->m_Header.pcs;
  if (pcs != icSigXYZData && pcs != icSigLabData) {
    if (pcs != icSigNoColorData)
      return record(Severity::Warning,
                    "WARNING - unknown pcs for colors: " + sigStr(static_cast<icTagSignature>(pcs)));
    return false;
  }

  if (type == icSigColorantTableType) {
    auto* table = dynamic_cast<CIccTagColorantTable*>(tag);
    if (!table)
      return record(Severity::Error, "Skipping " + sigDesc + ": unable to convert colorantTable");
    std::string path = ":colorantTable", report;
    if (table->Validate(path, report, nullptr) > icValidateWarning)
      return record(Severity::Warning, "WARNING - colorantTable failed validation:\n" + report);
    // CIccTagColorantTable carries no PCS of its own; assume the profile PCS.
    icUInt32Number n = table->GetSize();
    for (icUInt32Number i = 0; i < n; ++i) {
      icColorantTableEntry* e = table->GetEntry(i);
      icFloatNumber lab[3];
      if (pcs == icSigXYZData) {
        icFloatNumber xyz[3] = {icU16toF(e->data[0]), icU16toF(e->data[1]), icU16toF(e->data[2])};
        icXYZtoLab(lab, xyz, illum);
      } else {
        lab[0]=icU16toF(e->data[0]); lab[1]=icU16toF(e->data[1]); lab[2]=icU16toF(e->data[2]);
        icLabFromPcs(lab);
      }
      out.push_back(NamedLab{std::to_string(i+1) + " " + std::string(e->name), lab[0], lab[1], lab[2]});
    }
    title = "Colorant Table";
    return !out.empty();
  }

  if (type == icSigNamedColor2Type) {
    auto* table = dynamic_cast<CIccTagNamedColor2*>(tag);
    if (!table)
      return record(Severity::Error, "Skipping " + sigDesc + ": unable to convert namedColorTable");
    std::string path = ":namedColor2", report;
    if (table->Validate(path, report, nullptr) > icValidateWarning)
      return record(Severity::Warning, "WARNING - namedColorTable failed validation:\n" + report);
    if (pcs != table->GetPCS())
      return record(Severity::Warning,
                    "WARNING - bad pcs for namedColorTable: " + sigStr(static_cast<icTagSignature>(pcs)));
    icUInt32Number n = table->GetSize();
    std::string prefix = table->GetPrefix(), suffix = table->GetSufix();
    for (icUInt32Number i = 0; i < n; ++i) {
      SIccNamedColorEntry* e = table->GetEntry(i);
      icFloatNumber lab[3];
      if (pcs == icSigXYZData) {
        icXYZtoLab(lab, e->pcsCoords, illum);
      } else {
        icFloatNumber lab2[3] = {e->pcsCoords[0], e->pcsCoords[1], e->pcsCoords[2]};
        table->Lab2ToLab4(lab, lab2);
        icLabFromPcs(lab);
      }
      out.push_back(NamedLab{prefix + std::string(e->rootName) + suffix, lab[0], lab[1], lab[2]});
    }
    title = "Named Color Table";
    return !out.empty();
  }

  if (type == icSigTagArrayType) {   // v5 named-colour / colorant-info array
    auto* array = dynamic_cast<CIccTagArray*>(tag);
    if (!array)
      return record(Severity::Error, "Skipping " + sigDesc + ": unable to convert named color array");
    icArraySignature arrayType = array->GetTagArrayType();
    if (arrayType != icSigColorantInfoArray && arrayType != icSigNamedColorArray)
      return record(Severity::Warning,
                    "WARNING - unknown color array type: " +
                    sigStr(static_cast<icTagSignature>(arrayType)) + " for tag " + sigDesc);
    std::string path = ":" + sigDesc, report;
    if (array->Validate(path, report, nullptr) > icValidateWarning)
      return record(Severity::Warning, "WARNING - named color array failed validation:\n" + report);

    icUInt32Number items = array->GetSize();
    for (icUInt32Number i = 0; i < items; ++i) {
      CIccTag* thisItem = array->GetIndex(i);
      if (!thisItem) continue;

      icStructSignature structType = thisItem->GetTagStructType();
      if (structType != icSigColorantInfoStruct &&
          structType != icSigTintZeroStruct &&
          structType != icSigNamedColorStruct) {
        record(Severity::Warning, "Unknown named color struct " +
               sigStr(static_cast<icTagSignature>(structType)) + " for tag " + sigDesc);
        continue;
      }
      auto* structPtr = dynamic_cast<CIccTagStruct*>(thisItem);
      if (!structPtr) continue;

      // PCS data member  -  Float16/32/64 array of (L*a*b* or XYZ) triples.
      CIccTag* pcsElem = structPtr->FindElem(icSigCinfPcsDataMbr);
      if (!pcsElem) continue;
      icTagTypeSignature pcsDataType = pcsElem->GetType();
      if (pcsDataType != icSigFloat64ArrayType && pcsDataType != icSigFloat32ArrayType &&
          pcsDataType != icSigFloat16ArrayType) {
        record(Severity::Warning, "Unknown named color struct data type " +
               sigStr(static_cast<icTagSignature>(pcsDataType)) + " for tag " + sigDesc);
        continue;
      }
      auto* flt = dynamic_cast<CIccTagNumArray*>(pcsElem);
      if (!flt) continue;

      // TODO - can we easily convert spectra to PCS? Probably not without
      //        specifying viewing conditions. (carried over from iccProfileVisualize)
      std::vector<NamedLab> tempColors;
      icUInt32Number colorCount = flt->GetNumValues() / 3;   // ignore any partial triple
      for (icUInt32Number k = 0; k < colorCount; ++k) {
        icFloatNumber v[3], lab[3];
        flt->GetValues(v, k * 3, 3);
        if (pcs == icSigXYZData) {
          icXYZtoLab(lab, v, illum);
        } else {
          lab[0] = v[0]; lab[1] = v[1]; lab[2] = v[2];   // Lab directly coded as float
        }
        tempColors.push_back(NamedLab{"", lab[0], lab[1], lab[2]});
      }

      // Name member (optional): CinfName, falling back to CinfLocalizedName.
      CIccTag* nameElem = structPtr->FindElem(icSigCinfNameMbr);
      if (!nameElem)
        nameElem = structPtr->FindElem(icSigCinfLocalizedNameMbr);
      if (nameElem) {
        std::string nameString;
        switch (nameElem->GetType()) {
          case icSigUtf8TextType:
            if (auto* t = dynamic_cast<CIccTagUtf8Text*>(nameElem))
              nameString = std::string((char*)t->GetText());
            break;
          case icSigUtf16TextType:
            if (auto* t = dynamic_cast<CIccTagUtf16Text*>(nameElem)) {
              std::string buffer;
              nameString = std::string((char*)t->GetText(buffer));   // GetText converts to UTF8
            }
            break;
          case icSigTextType:
            if (auto* t = dynamic_cast<CIccTagText*>(nameElem))
              nameString = std::string((char*)t->GetText());
            break;
          case icSigDictType:                       // sometimes used where MLU expected
          case icSigMultiLocalizedUnicodeType:
            if (auto* t = dynamic_cast<CIccTagMultiLocalizedUnicode*>(nameElem)) {
              CIccLocalizedUnicode* u = t->Find(icLanguageCodeEnglish, icCountryCodeUSA);
              if (u) u->GetText(nameString);
            }
            break;
          default:
            record(Severity::Warning, "Unknown named color struct name type " +
                   sigStr(static_cast<icTagSignature>(nameElem->GetType())) + " for tag " + sigDesc);
            break;
        }
        if (!nameString.empty())
          for (auto& c : tempColors) c.name = nameString;
      }

      // Tint member (optional): per-colour tint % appended to the name.
      CIccTag* tintElem = structPtr->FindElem(icSigNmclTintMbr);
      if (tintElem) {
        icTagTypeSignature tintType = tintElem->GetType();
        if (tintType == icSigFloat64ArrayType || tintType == icSigFloat32ArrayType ||
            tintType == icSigFloat16ArrayType) {
          if (auto* tflt = dynamic_cast<CIccTagNumArray*>(tintElem)) {
            icUInt32Number dataCount = tflt->GetNumValues();
            if (dataCount <= tempColors.size()) {
              for (icUInt32Number k = 0; k < dataCount; ++k) {
                icFloatNumber tv;
                tflt->GetValues(&tv, k, 1);
                int percent = static_cast<int>(std::lround(tv * 100.0f));
                tempColors[k].name += "(" + std::to_string(percent) + "%)";
              }
            }
          }
        } else {
          record(Severity::Warning, "Unknown named color tint data type " +
                 sigStr(static_cast<icTagSignature>(tintType)) + " for tag " + sigDesc);
        }
      }

      out.insert(out.end(), tempColors.begin(), tempColors.end());
    }

    // Match iccProfileVisualize's "Color Array: <sig>" page label.
    title = "Color Array";
    return !out.empty();
  }

  return false;  // unknown / unsupported named-colour tag type
}

Graph buildNamedAB(const std::vector<NamedLab>& colors, const std::string& title) {
  Graph g;
  g.title = title + "  -  a*b*";
  g.xAxis = Axis{"a*", -abChartScale / 2, abChartScale / 2, true};
  g.yAxis = Axis{"b*", -abChartScale / 2, abChartScale / 2, true};

  // Constant-chroma circles (Hint).
  for (float radius = 30.0f; radius <= 150.0f; radius += 30.0f) {
    Series circ;
    circ.id = "chroma" + std::to_string(static_cast<int>(radius));
    circ.name = "C* = " + std::to_string(static_cast<int>(radius));
    circ.role = Role::Hint; circ.shape = Shape::ClosedPath; circ.colorHint = "neutral";
    for (int k = 0; k < 72; ++k) {
      float th = static_cast<float>(k) / 72.0f * 6.28318530718f;
      circ.verts.push_back(Vertex{radius * std::cos(th), radius * std::sin(th), "", kNaN});
    }
    g.series.push_back(std::move(circ));
  }
  // Quadrant labels (Hint)  -  match the PDF's +a Magenta / -a Green / etc.
  Series ax;
  ax.id = "axisLabels"; ax.name = "Axes"; ax.role = Role::Hint; ax.shape = Shape::Scatter;
  ax.colorHint = "neutral";
  ax.verts = {
    Vertex{ abChartScale / 2, 0, "+a Magenta", kNaN}, Vertex{-abChartScale / 2, 0, "-a Green", kNaN},
    Vertex{0,  abChartScale / 2, "+b Yellow", kNaN},  Vertex{0, -abChartScale / 2, "-b Blue", kNaN},
  };
  g.series.push_back(std::move(ax));

  Series data;
  data.id = "colors"; data.name = title; data.role = Role::Primary;
  data.shape = Shape::Scatter; data.auxKind = "Lstar";
  for (const auto& c : colors)
    data.verts.push_back(Vertex{c.a, c.b, c.name, c.L});
  g.series.push_back(std::move(data));
  return g;
}

Graph buildNamedXY(CIccProfile* pIcc, const std::vector<NamedLab>& colors,
                   const std::string& title) {
  Graph g;
  g.title = title + "  -  xy";
  g.xAxis = Axis{"x (CIE 1931)", 0.0f, chromaticityChartScale, true};
  g.yAxis = Axis{"y (CIE 1931)", 0.0f, chromaticityChartScale, true};

  // Reuse the locus + planckian reference geometry.
  Graph chrom = buildChromaticityGraph(pIcc);
  for (auto& s : chrom.series)
    if (s.role == Role::Hint) g.series.push_back(std::move(s));

  icFloatNumber illum[3];
  pIcc->getNormIlluminantXYZ(illum);
  Series data;
  data.id = "colors"; data.name = title; data.role = Role::Primary;
  data.shape = Shape::Scatter; data.auxKind = "Lstar";
  for (const auto& c : colors) {
    icFloatNumber lab[3] = {c.L, c.a, c.b}, xyz[3];
    icLabtoXYZ(xyz, lab, illum);
    XY p = xyFromXYZFloat(xyz);
    data.verts.push_back(Vertex{p.x, p.y, c.name, c.L});
  }
  g.series.push_back(std::move(data));
  return g;
}

// CLUT lattice -> raster. The nD CLUT is flattened to a 2-D image: the first two
// input dimensions form each tile, and the remaining dimensions are laid out as
// a grid of tiles arranged toward a square.
//
// On failure returns false and  -  when `diag` is supplied  -  records the granular
// skip/warn reason. Enumerate() probes with diag==nullptr, so a tag that simply
// carries no CLUT skips silently; only an actual RenderRaster() pulls the reasons
// through. Non-fatal conditions (tile-count overflow, an out-of-range sqrt)
// record a Warning and recover, leaving the raster geometry intact.
bool buildClutRaster(CIccTag* tag, const std::string& sigDesc, Raster& out,
                     std::vector<Diagnostic>* diag = nullptr) {
  // Granular diagnostics carrying the EXACT iccProfileVisualize wording, so a
  // stderr-echoing caller (and the structured `error`) read identically.
  auto skip = [&](const std::string& why) -> bool {
    if (diag) diag->push_back({Severity::Error, "Skipping " + sigDesc + ": " + why});
    return false;
  };
  auto record = [&](Severity sev, const std::string& msg) {
    if (diag) diag->push_back({sev, msg});
  };

  // these are all subclases of CIccMBB, and can share most of the code
  auto* lut = dynamic_cast<CIccMBB*>(tag);
  if (!lut)
    return skip("unable to convert LUT");

  int bytes = lut->GetPrecision();    // currently only 1 or 2
  int inputChannels = lut->InputChannels();
  int outputChannels = lut->OutputChannels();
  if (inputChannels <= 0 || outputChannels <= 0)
    return skip("invalid channel count");

  // write nD Data to TIFF
  icTagTypeSignature typeSig = tag->GetType();
  CIccCLUT* clut = lut->GetCLUT();
  if (!clut) {
    // clut is optional in mAB and mBA tags - only report if it isn't one of those
    if ( !(typeSig == icSigLutAtoBType || typeSig == icSigLutBtoAType) ) {
      char b[64];
      std::string typeDesc = icGetSigStr(b, sizeof b, static_cast<icUInt32Number>(typeSig));
      record(Severity::Error, "ERROR - clut data could not be read for tag '" +
                              sigDesc + "' of type '" + typeDesc + "'");
    }
    return false;
  }

  // validate is called back before the Describe call
  clut->Begin();  // initialize some grid information

  int gridPoints = clut->GridPoints(); // gridSize[0]
  if (gridPoints <= 0)
    return skip("invalid CLUT grid");

  int tiles = gridPoints;
  int tileWidth = 1;
  int tileHeight = 1;

  if (inputChannels >= 2) {
    tileWidth = clut->GridPoint(1);
    if (tileWidth <= 0)
      return skip("invalid CLUT width");
  }

  if (inputChannels >= 3) {
    tileHeight = clut->GridPoint(2);
    if (tileHeight <= 0)
      return skip("invalid CLUT height");
  }

  if (inputChannels > 3) {
    for (int i = 3; i < inputChannels; ++i) {
      int extraGridPoints = clut->GridPoint(i);
      if (extraGridPoints <= 0)
        return skip("invalid CLUT tile count");
      tiles *= extraGridPoints;
    }
  }

  // special case for single dimensional LUT
  if (inputChannels == 1) {
    tileWidth = tiles;
    tiles = 1;
    tileHeight = 1;
  }

  // special case for 2 dimensional LUT
  if (inputChannels == 2) {
    tileHeight = tiles;
    tiles = 1;
  }

  // find tile arrangement closest to a square
  if (tiles <= 0) {
    record(Severity::Warning, "WARNING - tile count overflow.");
    tiles = 1;
  }

  double tempResult = std::sqrt(static_cast<double>(tiles));
  if (tempResult > std::numeric_limits<int>::max()) {
    record(Severity::Warning, "ERROR - sqrt bad result!");
    tempResult = tiles / 2.0;
  }
  int tilesWide = static_cast<int>(tempResult);
  if (tilesWide <= 0) tilesWide = 1;   // guards the tilesHigh divide below

  // some odd counts need a tweak to align and look more sane
  if (inputChannels > 3 && (inputChannels & 1)) {
    int oldValue = tilesWide;
    // round down to a multiple of the grid size to better align rows
    tilesWide -= (tilesWide % (gridPoints * tileWidth));
    if (tilesWide == 0) {
      // this does happen -- should I round up in some cases?
      tilesWide = oldValue;
    }
  }

  int tilesHigh = (tiles + (tilesWide - 1)) / tilesWide;

  // multiply out by tile size
  int imageWidth = tilesWide * tileWidth;
  int imageHeight = tilesHigh * tileHeight;
  if (imageWidth <= 0 || imageHeight <= 0 || bytes <= 0)
    return skip("invalid image geometry");

  size_t bufferSize = static_cast<size_t>(imageWidth) * imageHeight * outputChannels * bytes;
  // NOTE that bufferSize will usually be greater than clutSize
  if (!bufferSize)
    return skip("empty image buffer");

  out.samples.assign(bufferSize, 0);
  unsigned char* buf = out.samples.data();
  unsigned short* buf16 = reinterpret_cast<unsigned short*>(buf);
  float* buf32 = reinterpret_cast<float*>(buf);
  icFloatNumber* clutData = clut->GetData(0);

  size_t n001 = static_cast<size_t>(tileWidth) * tileHeight * outputChannels;
  size_t n010 = static_cast<size_t>(tileWidth) * outputChannels;
  size_t n100 = static_cast<size_t>(outputChannels);
  if (inputChannels < 2) std::swap(n010, n100);
  size_t outTileStepV = static_cast<size_t>(imageWidth) * tileHeight * outputChannels;
  size_t outTileStepH = static_cast<size_t>(tileWidth) * outputChannels;
  size_t outColStep = static_cast<size_t>(outputChannels);
  size_t outRowStep = static_cast<size_t>(imageWidth) * outputChannels;

  for (int z = 0; z < tiles; ++z) {
    int z2 = z % tilesWide, z3 = z / tilesWide;
    for (int x = 0; x < tileWidth; ++x)
      for (int y = 0; y < tileHeight; ++y) {
        size_t in = z * n001 + x * n010 + (tileHeight - 1 - y) * n100;
        size_t o = z3 * outTileStepV + z2 * outTileStepH + y * outRowStep + x * outColStep;
        if (bytes == 4 || bytes == 8)
          for (int c = 0; c < outputChannels; ++c) buf32[o + c] = clutData[in + c];
        else if (bytes == 2)
          for (int c = 0; c < outputChannels; ++c) buf16[o + c] = clipU16(clutData[in + c] * 65535.0f);
        else
          for (int c = 0; c < outputChannels; ++c) buf[o + c] = clipU8(clutData[in + c] * 255.0f);
      }
  }

  out.width = imageWidth; out.height = imageHeight;
  out.channels = outputChannels; out.bitsPerChannel = 8 * bytes;
  out.photometric = photometricFromSpace(lut->GetCsOutput());
  out.normalizedICC = true;
  return true;
}

// Append LUT sub-curve descriptors in the A->B->M order output3DLUT uses.
void enumerateLutCurves(CIccProfile* pIcc, icTagSignature sig, CIccMBB* lut,
                        std::vector<Descriptor>& out) {
  std::string base = sigStr(sig);
  int inCh = lut->InputChannels(), outCh = lut->OutputChannels();
  icColorSpaceSignature inSp = lut->GetCsInput(), outSp = lut->GetCsOutput();
  bool inMtx = lut->IsInputMatrix();
  struct Grp { char g; CIccCurve** arr; int count; bool useInput; };
  Grp groups[] = {
    {'A', lut->GetCurvesA(), inMtx ? outCh : inCh, !inMtx},
    {'B', lut->GetCurvesB(), inMtx ? inCh : outCh, inMtx},
    {'M', lut->GetCurvesM(), inMtx ? inCh : outCh, inMtx},
  };
  for (const Grp& grp : groups) {
    if (!grp.arr) continue;
    for (int i = 0; i < grp.count; ++i) {
      CIccCurve* c = grp.arr[i];
      if (!c) continue;
      std::string ch = channelName(i, grp.useInput, inSp, outSp, inCh, outCh);
      std::string label = base + ": curve" + grp.g + "[ " + ch + " ]";
      // Enumerate every present curve, even a malformed one; RenderGraph
      // validates and surfaces the reason rather than dropping it silently.
      Descriptor d;
      d.kind = Kind::Curve1D; d.output = Output::Graph;
      d.id = "curve:" + base + ":" + grp.g + ":" + std::to_string(i);
      d.title = label; d.tag = sig; d.grp = grp.g; d.idx = i;
      out.push_back(std::move(d));
    }
  }
  (void)pIcc;
}

CIccCurve* lutCurveFor(CIccMBB* lut, char grp, int idx) {
  CIccCurve** arr = grp == 'A' ? lut->GetCurvesA()
                  : grp == 'B' ? lut->GetCurvesB()
                  : grp == 'M' ? lut->GetCurvesM() : nullptr;
  return arr ? arr[idx] : nullptr;
}

} // namespace

// -- public API ---------------------------------------------------------------

std::vector<Descriptor> Enumerate(CIccProfile* pIcc, Order order) {
  std::vector<Descriptor> out;
  if (!pIcc) return out;

  // NOTE: we deliberately do NOT iterate pIcc->m_Tags here. m_Tags is a
  // std::list owned by the IccProfLib translation unit; iterating it from this
  // TU walks off the end into the circular list (cross-TU std::list iterator
  // mismatch). CIccProfile::FindTag (which lives in IccProfLib) is safe, so we
  // probe a fixed, canonically-ordered signature list instead. This also gives
  // deterministic ordering, close to the profile's tag-table order.

  // Chromaticity first (only when RGB colorants are present).
  if (pIcc->FindTag(icSigRedColorantTag) && pIcc->FindTag(icSigGreenColorantTag) &&
      pIcc->FindTag(icSigBlueColorantTag)) {
    Descriptor d;
    d.kind = Kind::ChromaticityXY; d.output = Output::Graph;
    d.id = "chroma:xy"; d.title = "Chromaticity xy";
    out.push_back(std::move(d));
  }

  static const icTagSignature kTrcSigs[] = {
    icSigRedTRCTag, icSigGreenTRCTag, icSigBlueTRCTag, icSigGrayTRCTag };
  for (icTagSignature sig : kTrcSigs) {
    auto* c = dynamic_cast<CIccCurve*>(pIcc->FindTag(sig));
    if (!c) continue;   // present-but-malformed curves are kept; see RenderGraph
    Descriptor d;
    d.kind = Kind::Curve1D; d.output = Output::Graph;
    d.id = "curve:" + sigStr(sig); d.title = sigStr(sig); d.tag = sig;
    out.push_back(std::move(d));
  }

  static const icTagSignature kLutSigs[] = {
    icSigAToB0Tag, icSigAToB1Tag, icSigAToB2Tag, icSigAToB3Tag,
    icSigBToA0Tag, icSigBToA1Tag, icSigBToA2Tag, icSigBToA3Tag,
    icSigGamutTag, icSigPreview0Tag, icSigPreview1Tag, icSigPreview2Tag };
  for (icTagSignature sig : kLutSigs) {
    CIccTag* t = pIcc->FindTag(sig);
    auto* lut = dynamic_cast<CIccMBB*>(t);
    if (!lut) continue;
    enumerateLutCurves(pIcc, sig, lut, out);
    Raster probe;
    // Probe only  -  diag==nullptr, so tags that legitimately carry no CLUT
    // (matrix/curve-only mAB/mBA) skip silently; a real RenderRaster() surfaces
    // any failure reason.
    if (buildClutRaster(t, sigStr(sig), probe)) {
      Descriptor d;
      d.kind = Kind::ClutImage; d.output = Output::Raster;
      d.id = "clut:" + sigStr(sig); d.title = sigStr(sig) + " CLUT"; d.tag = sig;
      out.push_back(std::move(d));
    }
  }

  static const icTagSignature kNamedSigs[] = {
    icSigNamedColorTag, icSigNamedColor2Tag, icSigColorantTableTag, icSigColorantTableOutTag,
    icSigColorantInfoTag, icSigColorantInfoOutTag };   // last two are v5 tagArray
  for (icTagSignature sig : kNamedSigs) {
    CIccTag* t = pIcc->FindTag(sig);
    if (!t) continue;
    std::vector<NamedLab> colors; std::string title;
    if (!collectNamedColors(pIcc, t, sigStr(sig), colors, title)) continue;   // probe: diag==nullptr
    Descriptor ab;
    ab.kind = Kind::NamedColorsAB; ab.output = Output::Graph;
    ab.id = "named:ab:" + sigStr(sig); ab.title = title + "  -  a*b* (" + sigStr(sig) + ")";
    ab.tag = sig; out.push_back(std::move(ab));
    Descriptor xy;
    xy.kind = Kind::NamedColorsXY; xy.output = Output::Graph;
    xy.id = "named:xy:" + sigStr(sig); xy.title = title + "  -  xy (" + sigStr(sig) + ")";
    xy.tag = sig; out.push_back(std::move(xy));
  }

  if (order == Order::TagTable) {
    // Reproduce the profile's tag-table page order (what iccProfileVisualize
    // emitted by walking its tag directory). The portable, in-library-safe
    // accessor for a tag's position is protected (CIccProfile::GetTag), so this
    // path reads the PUBLIC m_Tags directory directly to recover the exact tag
    // order, then ranks each descriptor by its owning tag's directory index.
    //
    // SCOPE NOTE: iterating m_Tags is only safe when the caller is linked in the
    // same toolchain as IccProfLib (the CLI case  -  exactly what iccProfileVisualize
    // does). Order::Canonical, the default, never touches m_Tags and is the
    // cross-module/WASM-safe ordering; prefer it where the std::list ABI is not
    // guaranteed to match. Chromaticity (tag == 0, whole-profile) is pinned first,
    // as iccProfileVisualize emitted it before its tag loop. std::stable_sort
    // preserves the canonical sub-order within one tag (a LUT's A/B/M curves then
    // its CLUT; named a*b* then xy).
    std::vector<icTagSignature> seq;
    for (const IccTagEntry& e : pIcc->m_Tags)
      seq.push_back(e.TagInfo.sig);
    auto rank = [&seq](const Descriptor& d) -> size_t {
      if (d.tag == static_cast<icTagSignature>(0)) return 0;   // chromaticity first
      for (size_t i = 0; i < seq.size(); ++i)
        if (seq[i] == d.tag) return i + 1;
      return seq.size() + 1;
    };
    std::stable_sort(out.begin(), out.end(),
                     [&](const Descriptor& a, const Descriptor& b) {
                       return rank(a) < rank(b);
                     });
  }
  return out;
}

GraphResult RenderGraph(CIccProfile* pIcc, const std::string& id, Verbosity v) {
  GraphResult res;
  if (!pIcc) { res.error = "null profile"; return res; }
  for (const Descriptor& d : Enumerate(pIcc)) {
    if (d.id != id) continue;
    if (d.output != Output::Graph) { res.error = "descriptor is not a graph"; return res; }
    if (d.kind == Kind::ChromaticityXY) {
      res.graph = buildChromaticityGraph(pIcc); res.ok = true; return res;
    }
    if (d.kind == Kind::Curve1D) {
      CIccTag* t = pIcc->FindTag(d.tag);
      CIccCurve* c = nullptr;
      if (d.grp) { auto* lut = dynamic_cast<CIccMBB*>(t); if (lut) c = lutCurveFor(lut, d.grp, d.idx); }
      else c = dynamic_cast<CIccCurve*>(t);
      if (!c) { res.error = "curve not found"; return res; }
      // A malformed curve (e.g. gamma 0) is no longer dropped at enumerate; we
      // render its (degenerate) shape but carry the validation reason as a
      // diagnostic so the UI can show it instead of a blank/absent graph.
      std::string report;
      if (curveValidate(c, d.title, report) > icValidateWarning)
        res.diagnostics.push_back({Severity::Warning, "WARNING - curve failed validation:\n" + report});
      res.graph = buildCurveGraph(c, d.title); res.ok = true;
      emitDiagnostics(res.diagnostics, v);
      return res;
    }
    if (d.kind == Kind::NamedColorsAB || d.kind == Kind::NamedColorsXY) {
      CIccTag* t = pIcc->FindTag(d.tag);
      std::vector<NamedLab> colors; std::string title;
      if (t && collectNamedColors(pIcc, t, sigStr(d.tag), colors, title, &res.diagnostics)) {
        res.graph = (d.kind == Kind::NamedColorsAB) ? buildNamedAB(colors, title)
                                                    : buildNamedXY(pIcc, colors, title);
        res.ok = true;
      } else {
        // Surface the specific reason (validation report, bad pcs, ...) rather
        // than a generic string  -  restoring outputNamedColors' diagnostics.
        res.error = res.diagnostics.empty() ? "no colours" : res.diagnostics.back().message;
      }
      emitDiagnostics(res.diagnostics, v);
      return res;
    }
    res.error = "unsupported graph kind"; return res;
  }
  res.error = "unknown visualization id: " + id;
  return res;
}

RasterResult RenderRaster(CIccProfile* pIcc, const std::string& id, Verbosity v) {
  RasterResult res;
  if (!pIcc) { res.error = "null profile"; return res; }
  for (const Descriptor& d : Enumerate(pIcc)) {
    if (d.id != id) continue;
    if (d.output != Output::Raster) { res.error = "descriptor is not a raster"; return res; }
    CIccTag* t = pIcc->FindTag(d.tag);
    if (t && buildClutRaster(t, sigStr(d.tag), res.raster, &res.diagnostics)) {
      res.ok = true;
    } else {
      // Specific reason (invalid CLUT width/height/grid, empty buffer, ...).
      res.error = res.diagnostics.empty() ? "could not build raster"
                                          : res.diagnostics.back().message;
    }
    emitDiagnostics(res.diagnostics, v);
    return res;
  }
  res.error = "unknown visualization id: " + id;
  return res;
}

// -- diagnostic output policy (see IccVizModel.hpp) ----------------------------
void SetSilent(bool silent) { g_silent = silent; }
bool GetSilent() { return g_silent; }
void SetDiagnosticContext(const std::string& name) { g_diagContext = name; }

} // namespace iccviz
