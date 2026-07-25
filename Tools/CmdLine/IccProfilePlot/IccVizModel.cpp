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
 * IccVizModel implementation - see IccVizModel.hpp.
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
#include "IccCmm.h"            // CIccXform / CIccApplyXform - PCS/device sampling (neutral axis, gamut, round-trip)
#include "IccUtil.h"

#include "spectralLocus.hpp"   // const spectralLocus2degree (internal linkage)

#include <algorithm>
#include <cmath>
#include <cstddef>     // std::size_t
#include <cstdint>     // std::uint32_t / std::uint64_t (used below; previously only transitive)
#include <cstdio>
#include <cstdlib>     // std::abs (int)
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

// effectiveSilent - decide whether to suppress diagnostics for this call. An
// explicit per-call Verbosity (Stderr/Silent) wins; Verbosity::Default defers to
// the process-global g_silent switch. Centralised so every emit path shares one
// precedence rule instead of re-deriving it.
bool effectiveSilent(Verbosity v) {
  switch (v) {
    case Verbosity::Stderr: return false;
    case Verbosity::Silent: return true;
    default:                return g_silent;   // Verbosity::Default
  }
}

// emitDiagnostics - echo a result's diagnostics to stderr unless silenced. The
// message text already carries the exact upstream wording (Skipping ... /
// WARNING - ... / ERROR - ...); the optional context reproduces iccProfileVisualize's
// leading "<filename>: " so library output can match the CLI byte-for-byte.
void emitDiagnostics(const std::vector<Diagnostic>& diags, Verbosity v) {
  if (effectiveSilent(v)) return;
  for (const Diagnostic& d : diags) {
    if (g_diagContext.empty())
      std::fprintf(stderr, "%s\n", d.message.c_str());
    else
      std::fprintf(stderr, "%s: %s\n", g_diagContext.c_str(), d.message.c_str());
  }
}

// sigStr - format a 4-char ICC tag/space signature as its printable string (e.g.
// 'A2B0'). Thin wrapper over IccProfLib's icGetSigStr with a local buffer, so the
// many callers that build titles and descriptor ids don't each manage a buffer.
std::string sigStr(icTagSignature sig) {
  char buf[64];
  return std::string(icGetSigStr(buf, sizeof buf, static_cast<icUInt32Number>(sig)));
}

// -- colour helpers - single-sourced in IccVizMath.hpp so the XYZ->xy +
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

// channelName - human label for one channel of a device or PCS space (e.g.
// "Cyan", "L*"), choosing the input or output space per `useInput`. Delegates to
// IccProfLib's icColorIndexName so channel names match the rest of the toolchain
// rather than being invented here.
std::string channelName(int index, bool useInput, icColorSpaceSignature inSpace,
                        icColorSpaceSignature outSpace, int inCh, int outCh) {
  char buf[128];
  icColorIndexName(buf, 128, useInput ? inSpace : outSpace, index,
                   useInput ? inCh : outCh, useInput ? "In" : "Out");
  return std::string(buf);
}

// clipU8 - clamp a float sample into the 8-bit unsigned range when packing a CLUT
// raster, mapping NaN->0 and +Inf->255. Written as explicit finite/range checks (not
// a bare cast) so a malformed or non-finite CLUT value can never wrap or overflow
// the output byte.
unsigned char clipU8(icFloatNumber v) {
  if (std::isnan(v)) return 0;
  if (std::isinf(v)) return 255;
  if (v < 0) return 0;
  if (v > 255) return 255;
  return static_cast<unsigned char>(v);
}
// clipU16 - the 16-bit twin of clipU8: the same NaN->0 / +Inf->65535 / range clamping
// for packing a 16-bit CLUT raster.
unsigned short clipU16(icFloatNumber v) {
  if (std::isnan(v)) return 0;
  if (std::isinf(v)) return 65535;
  if (v < 0) return 0;
  if (v > 65535) return 65535;
  return static_cast<unsigned short>(v);
}

// photometricFromSpace - map an ICC colour space to the TIFF PhotometricInterpretation
// value the CLUT raster is tagged with (RGB=2, CMYK=5, CIELAB=8, Gray/Gamut=1,
// N-ink -> WhiteIsZero=0). One switch so the raster writer and any future exporter
// agree on how each space is labelled.
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

// curveValidate - validate a curve, returning IccProfLib's report text by
// reference. A status > icValidateWarning means the curve is malformed (e.g.
// gamma 0, bad LUT size). Callers no longer DROP failing curves: they enumerate
// them anyway and surface this report as a diagnostic, so a malformed curve shows
// its reason instead of the graph silently vanishing.
icValidateStatus curveValidate(CIccCurve* curve, const std::string& sigDesc,
                               std::string& report) {
  return curve->Validate(":" + sigDesc, report, nullptr);
}

// describeCurve - one-line human summary of a curve for the graph subtitle:
// identity ("Y = X"), a single-entry gamma ("Y = X ^ g"), a sampled table
// ("LookupTable[n]"), or IccProfLib's own Describe() text for richer types. The
// non-CIccTagCurve path runs Validate() before Describe() so the formatter never
// touches unvalidated, possibly-malformed data first (CWE-476); a bad curve is
// still described (status is advisory), matching this module's don't-drop policy.
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
  // Other curve types are formatted by Describe(), which walks the curve's data.
  // Run the curve's own Validate() first - the same validate-before-describe gate
  // the rest of this module uses (see curveValidate) - so the formatter is never
  // the first thing to touch unvalidated, possibly-malformed data (CWE-476). Per
  // this module's design we do NOT drop a bad curve: the status is advisory and
  // we still return its description.
  std::string report;
  curve->Validate(":curve", report, nullptr);
  std::string desc;
  curve->Describe(desc, 100);
  return desc;
}

// -- producers ----------------------------------------------------------------

// buildCurveGraph - trace a tone/shaper curve as a polyline (input 0..1 -> output
// 0..1) plus a dashed identity reference line. The sample count adapts to the
// curve - >=1000 for a smooth analytic curve, at least the LUT's own point count
// so a fine sampled table is never under-drawn, and just 2 for a pure identity -
// and every output is finiteness-guarded and clamped to [0,1] so a malformed curve
// still yields a drawable, bounded series.
Graph buildCurveGraph(CIccCurve* curve, const std::string& title) {
  Graph g;
  g.title = title;
  g.description = describeCurve(curve);
  g.xAxis = Axis{"Input", 0.0f, 1.0f, false};
  g.yAxis = Axis{"Output", 0.0f, 1.0f, false};

  // Number of samples used to trace the curve. Default to a smooth 1000; for a
  // sampled LUT curve use at least its own point count so we never under-sample
  // it, and drop to the 2 endpoints when the curve is a pure identity.
  int steps = 1000;
  if (auto* tc = dynamic_cast<CIccTagCurve*>(curve))
    steps = std::max(1000, static_cast<int>(tc->GetSize()));
  if (curve->IsIdentity()) steps = 2;
  // Defensive floor: steps drives the divisor below, so guarantee it is at least
  // 1 even if the logic above is ever changed to allow a smaller value (avoids a
  // divide-by-zero on i / (float)steps). Written as <= 0 so the non-zero
  // invariant on the denominator is explicit (steps is int).
  if (steps <= 0) steps = 1;

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

// addPrimaryPoint - append one colorant/white-point XYZ tag to a chromaticity
// series as a single xy point, skipping silently if the tag is absent or not an
// XYZ tag. Factored out so the red/green/blue/white primaries are all projected to
// xy and plotted identically on the CIE chart.
void addPrimaryPoint(Graph& g, Series& s, CIccTag* tag, const char* label,
                     const char* colorHint) {
  auto* xyzTag = dynamic_cast<CIccTagXYZ*>(tag);
  if (!xyzTag) return;
  const icXYZNumber* xyz = xyzTag->GetXYZ(0);
  if (!xyz) return;
  XY p = xyFromICCXYZ(xyz);
  s.verts.push_back(Vertex{p.x, p.y, label, kNaN});
  (void)g; (void)colorHint;   // both reserved for a future per-point colour hint
}

// buildChromaticityGraph - build the CIE 1931 xy chart for a profile: the spectral
// locus, wavelength labels and planckian curve as reference Hints (single-sourced
// from spectralLocus.hpp and IccVizMath's approxPlanck so the geometry lives in one
// place), plus the profile's own media white point and, when the RGB colorants are
// present, the primaries and gamut triangle as Primary data. Emits geometry only -
// no display colours - so the caller renders it in its own look.
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

// collectNamedColors - collect a named/colorant table's colours as Lab + name.
// `diag` (when supplied) carries
// the granular skip reasons - including the IccProfLib validation `report`
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

  // PCS basis for the colour conversion is the profile header PCS - matching
  // iccProfileVisualize, which reads pIcc->m_Header.pcs (not the table's own PCS)
  // and uses it for every named-colour type. Anything that isn't XYZ/Lab can't be
  // plotted: icSigNoColorData (spectral / iccMAX) skips silently, while any other
  // space records a warning.
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
    // CIccTagColorantTable carries no PCS of its own; assume the profile PCS
    // (already validated as XYZ/Lab above).
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
      // strnlen-bound: e->name is a fixed-size profile field; a non-NUL-terminated
      // one would over-read adjacent heap if passed to std::string(const char*).
      out.push_back(NamedLab{std::to_string(i+1) + " " +
          std::string(e->name, strnlen(e->name, sizeof e->name)), lab[0], lab[1], lab[2]});
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
      // strnlen-bound (see Colorant Table above): rootName is a fixed-size field.
      out.push_back(NamedLab{prefix +
          std::string(e->rootName, strnlen(e->rootName, sizeof e->rootName)) + suffix,
          lab[0], lab[1], lab[2]});
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

      // PCS data member - Float16/32/64 array of (L*a*b* or XYZ) triples.
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

// buildNamedAB - plot a set of named/colorant colours on a CIELAB a*b* chart:
// constant-chroma reference circles and quadrant labels as Hints, the colours
// themselves as a Primary scatter carrying L* in Vertex.aux (so the caller can
// shade by lightness). a*b* is chosen (over xy) because named colours are stored
// in Lab, so this view needs no chromaticity projection and preserves hue/chroma.
Graph buildNamedAB(const std::vector<NamedLab>& colors, const std::string& title) {
  Graph g;
  g.title = title + " - a*b*";
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
  // Quadrant labels (Hint) - match the PDF's +a Magenta / -a Green / etc.
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

// buildNamedXY - the same named/colorant colours on the CIE 1931 xy chart. Reuses
// buildChromaticityGraph's locus/planckian Hints (so the two named views share one
// reference frame), then converts each Lab colour to XYZ under the profile's
// illuminant and projects to xy. Offered alongside buildNamedAB so the caller can
// show spot colours against the spectral horseshoe as well as in Lab a*b*.
Graph buildNamedXY(CIccProfile* pIcc, const std::vector<NamedLab>& colors,
                   const std::string& title) {
  Graph g;
  g.title = title + " - xy";
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

// buildClutRaster - flatten a CLUT lattice to a 2-D raster. The nD CLUT becomes an
// image: the first two
// input dimensions form each tile, and the remaining dimensions are laid out as
// a grid of tiles arranged toward a square.
//
// On failure returns false and - when `diag` is supplied - records the granular
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
    // Accumulate in 64-bit and bail on overflow: tiles is profile-derived and an
    // int multiply here could wrap to a small positive value that escapes the
    // <=0 guards below and drives a too-small image buffer (heap overflow).
    std::uint64_t tiles64 = static_cast<std::uint64_t>(tiles);
    for (int i = 3; i < inputChannels; ++i) {
      int extraGridPoints = clut->GridPoint(i);
      if (extraGridPoints <= 0)
        return skip("invalid CLUT tile count");
      tiles64 *= static_cast<std::uint64_t>(extraGridPoints);
      if (tiles64 > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        return skip("CLUT tile count overflow");
    }
    tiles = static_cast<int>(tiles64);
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
    // round down to a multiple of the grid size to better align rows.
    // Guard the divisor: gridPoints*tileWidth could overflow int to 0 and trap.
    int alignTo = gridPoints * tileWidth;
    if (alignTo > 0) {
      tilesWide -= (tilesWide % alignTo);
      if (tilesWide == 0) {
        // this does happen -- should I round up in some cases?
        tilesWide = oldValue;
      }
    }
  }

  int tilesHigh = (tiles + (tilesWide - 1)) / tilesWide;

  // multiply out by tile size
  int imageWidth = tilesWide * tileWidth;
  int imageHeight = tilesHigh * tileHeight;
  if (imageWidth <= 0 || imageHeight <= 0 || bytes <= 0)
    return skip("invalid image geometry");

  // Compute in 64-bit and enforce a hard ceiling before allocating: every
  // factor here is profile-derived and a size_t multiply on wasm32 (32-bit
  // size_t) could wrap to a small value, under-sizing the buffer the sample
  // loop then writes past. 256 MB is far above any legitimate CLUT raster.
  static const std::uint64_t kMaxRasterBytes = 256ull * 1024 * 1024;
  std::uint64_t bufferSize64 = static_cast<std::uint64_t>(imageWidth) *
                               static_cast<std::uint64_t>(imageHeight) *
                               static_cast<std::uint64_t>(outputChannels) *
                               static_cast<std::uint64_t>(bytes);
  if (!bufferSize64)
    return skip("empty image buffer");
  if (bufferSize64 > kMaxRasterBytes)
    return skip("CLUT raster too large");

  size_t bufferSize = static_cast<size_t>(bufferSize64);
  // NOTE that bufferSize will usually be greater than clutSize
  out.samples.assign(bufferSize, 0);
  unsigned char* buf = out.samples.data();
  unsigned short* buf16 = reinterpret_cast<unsigned short*>(buf);
  float* buf32 = reinterpret_cast<float*>(buf);
  icFloatNumber* clutData = clut->GetData(0);
  if (!clutData)
    return skip("CLUT data unavailable");

  // Defense-in-depth bound for the input read below. The stride fix below makes
  // the index correct for well-formed CLUTs (square and non-square), but the
  // packing geometry is still derived from grid metadata rather than the actual
  // sample array, so a malformed profile with inconsistent grid/channel counts
  // could still compute an in-range-looking index past the data. Bound every read
  // against the true element count - NumPoints() grid nodes x outputChannels
  // samples per node - and treat any out-of-range node as 0 (the buffer is
  // pre-zeroed) instead of reading out of bounds (CWE-125; issue #1548).
  const size_t clutSampleCount =
      static_cast<size_t>(clut->NumPoints()) * static_cast<size_t>(outputChannels);

  // CLUT input strides. n010 is the per-row (x dimension) stride and MUST be
  // tileHeight*outputChannels: x indexes the tileWidth dimension, and advancing
  // one x-step skips a full column of tileHeight samples. Using tileWidth here
  // (as an earlier revision did) only coincides for square CLUTs (tileWidth ==
  // tileHeight); for a non-square CLUT it over-strides and walks the input index
  // off the end of clutData - the root cause of the #1548 heap-overflow read.
  // (Matches the reference iccProfileVisualize layout.)
  size_t n001 = static_cast<size_t>(tileWidth) * tileHeight * outputChannels;
  size_t n010 = static_cast<size_t>(tileHeight) * outputChannels;
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
        // Skip nodes the packing geometry addresses beyond the real CLUT array;
        // leaves the pre-zeroed output sample intact rather than over-reading (#1548).
        if (in + static_cast<size_t>(outputChannels) > clutSampleCount)
          continue;
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

// enumerateLutCurves - append descriptors for a LUT's A/B/M sub-curves in the
// A->B->M order output3DLUT uses, so the enumerated list matches the reference layout.
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
  // Upper bound on curve channels we will ever enumerate. ICC colour spaces top
  // out at 15 device channels (nCLR) plus PCS, so 256 is comfortably generous
  // while still capping a malformed LUT that reports a bogus channel count -
  // preventing an unbounded loop / DoS (CWE-400/CWE-834).
  const int kMaxVizChannels = 256;
  for (const Grp& grp : groups) {
    if (!grp.arr) continue;
    // grp.count comes straight from the profile's LUT channel count; clamp it
    // before driving the loop so untrusted data cannot dictate the iteration
    // count. The curve arrays are sized by the same channel count, so clamping
    // down can never read past the array.
    const int count = std::min(grp.count, kMaxVizChannels);
    for (int i = 0; i < count; ++i) {
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

// lutCurveFor - fetch the idx-th A/B/M shaper curve of an mAB/mBA LUT, selected by
// the group letter a Descriptor carries ('A','B','M'). Returns nullptr for an
// unknown group or an absent curve array, so RenderGraph can resolve a curve
// descriptor back to its curve without duplicating the group dispatch.
CIccCurve* lutCurveFor(CIccMBB* lut, char grp, int idx) {
  CIccCurve** arr = grp == 'A' ? lut->GetCurvesA()
                  : grp == 'B' ? lut->GetCurvesB()
                  : grp == 'M' ? lut->GetCurvesM() : nullptr;
  return arr ? arr[idx] : nullptr;
}

// -----------------------------------------------------------------------------
// Shared device<->PCS sampling helpers (used by the CLUT raster, neutral-axis,
// gamut and round-trip paths below).
// -----------------------------------------------------------------------------

// Bound for the untrusted profile geometry: ICC device spaces top out at 15 (nCLR).
const int kMaxInkChannels = 15;

// isPcsSpace - true if a colour space is a PCS (Lab or XYZ). Used throughout the
// sampling paths to tell a device side from a PCS side of a LUT (e.g. A2B is
// device->PCS, B2A is PCS->device), so each path can reject a tag wired the wrong
// way round before it samples.
bool isPcsSpace(icColorSpaceSignature s) {
  return s == icSigLabData || s == icSigXYZData;
}

// -----------------------------------------------------------------------------
// Neutral Axis Inking  (Kind::NeutralAxisInking)
//
// Sweeps the neutral axis (a*=b*=0) from white (L*=100) to black (L*=0) through a
// B2A table (PCS->device) and records how much of each device colorant the profile
// lays down - the classic GCR / neutral-build curve. One graph, one polyline per
// device channel: x = L* (100->0), y = colorant % (0-100). The receiver styles and
// plots it (and restricts the analysis to output profiles).
// -----------------------------------------------------------------------------
const int kNeutralSamples = 101;

// neutralIntentForSig - the rendering intent a B2A tag embodies (...0 perceptual,
// ...2 saturation, else
// relative colorimetric) - used only as the Create() hint; the specific tag wins.
icRenderingIntent neutralIntentForSig(icTagSignature sig) {
  switch (sig) {
    case icSigBToA0Tag: return icPerceptual;
    case icSigBToA2Tag: return icSaturation;
    default:            return icRelativeColorimetric;
  }
}

// neutralSrc - map a neutral CIELAB (L*,0,0) to the B2A source-space input, written
// into src[] in the
// internal PCS encoding the xform expects (Lab directly, or D50 XYZ for XYZ PCS).
void neutralSrc(float L, icColorSpaceSignature pcs, icFloatNumber* src) {
  if (pcs == icSigXYZData) {
    // Reuse IccProfLib's CIELAB->XYZ (nullptr white => D50) instead of hand-rolling
    // the inverse companding, so the constants live in exactly one place.
    icFloatNumber lab[3] = { L, 0.0f, 0.0f };
    icLabtoXYZ(src, lab, nullptr);                                   // -> D50 human XYZ (Y=1)
    icXyzToPcs(src);
  } else {
    src[0] = L; src[1] = 0.0f; src[2] = 0.0f;                        // human L*a*b*
    icLabToPcs(src);
  }
}

// buildNeutralAxisGraph - build the neutral-axis inking graph for one B2A tag: walk
// the neutral axis L*=100->0 (a*=b*=0) through the PCS->device table and record each
// colorant's amount, one polyline per channel. This is the GCR/ink-build curve, so
// it must come from the B2A (PCS->device) direction; the function guards that the
// tag really is PCS-in / device-out and that channel counts match before sampling,
// since the tag and its geometry are untrusted profile data. Returns false with a
// diagnostic on any guard failure.
bool buildNeutralAxisGraph(CIccProfile* pIcc, icTagSignature sig, Graph& out,
                           std::vector<Diagnostic>* diag) {
  const std::string sigDesc = sigStr(sig);
  auto skip = [&](const std::string& why) -> bool {
    if (diag) diag->push_back({Severity::Error, "Skipping " + sigDesc + " neutral inking: " + why});
    return false;
  };

  // -- guard: a CLUT-bearing PCS->device LUT --
  CIccTag* tag = pIcc ? pIcc->FindTag(sig) : nullptr;
  auto* lut = dynamic_cast<CIccMBB*>(tag);
  if (!lut)  return skip("tag is not a LUT");
  if (!lut->GetCLUT()) return skip("LUT carries no CLUT lattice");

  const icColorSpaceSignature inSp  = lut->GetCsInput();
  const icColorSpaceSignature outSp = lut->GetCsOutput();
  if (!isPcsSpace(inSp)) return skip("LUT input is not a PCS");        // B2A: PCS in
  if (isPcsSpace(outSp)) return skip("LUT output is not a device space");
  const int inCh  = lut->InputChannels();    // 3 (PCS)
  const int outCh = lut->OutputChannels();   // N device colorants
  if (inCh < 3 || outCh <= 0 || outCh > kMaxInkChannels) return skip("invalid channel count");

  CIccXform* xform = CIccXform::Create(pIcc, tag, /*bInput=*/false,
                                       neutralIntentForSig(sig), icInterpLinear, /*pPcc=*/NULL, /*bUseSpectralPCS=*/false, /*pHintManager=*/NULL, /*bOwnsProfile=*/false);
  if (!xform) return skip("could not build PCS->device transform");
  xform->ShareProfile();
  if (xform->Begin() != icCmmStatOk) { delete xform; return skip("transform Begin failed"); }
  icStatusCMM st = icCmmStatOk;
  CIccApplyXform* apply = xform->GetNewApply(st);
  if (!apply || st != icCmmStatOk) { delete apply; delete xform; return skip("transform apply init failed"); }

  // Apply reads GetNumSrcSamples()/writes GetNumDstSamples(); require they match the
  // raw LUT channel counts so the src/dst buffers below (sized inCh/outCh) cannot be
  // over-read/over-written by a multi-element transform (cf. iccDEV #1633).
  if (xform->GetNumSrcSamples() != inCh || xform->GetNumDstSamples() != outCh) {
    delete apply; delete xform; return skip("transform channel-count mismatch");
  }

  // One series per device colorant; sample L* from 100 (white) down to 0 (black).
  std::vector<Series> series(outCh);
  for (int c = 0; c < outCh; ++c) {
    series[c].id = "ch" + std::to_string(c);
    series[c].name = channelName(c, /*useInput=*/false, inSp, outSp, inCh, outCh);
    series[c].role = Role::Primary;
    series[c].shape = Shape::Polyline;
    series[c].verts.reserve(kNeutralSamples);
  }

  // Value-initialise these scratch buffers - `(n)`, not `(n, 0.0f)`. Both spellings
  // zero every element, but libstdc++ 15 implements the fill constructor as
  // `for (; __n--; ++__first)` (bits/stl_uninitialized.h:469): the loop-exit test
  // reads 0 and then decrements it to SIZE_MAX. That wraparound is well-defined
  // unsigned arithmetic and harmless, but clang's -fsanitize=integer reports it on
  // EVERY such construction regardless of the count, which is how it surfaced as a
  // "uio in GamutVolume" finding (#1777) - the report names our call site because
  // the header code is inlined into it, not because anything here is wrong. The
  // value-initialising overload goes through __uninitialized_default_n, which has
  // no such loop, so preferring it keeps the integer sanitizer usable on this
  // engine without an ignorelist entry. Applied to every fill-construction below.
  std::vector<icFloatNumber> src(inCh), dst(outCh);
  for (int i = 0; i < kNeutralSamples; ++i) {
    float L = 100.0f * (1.0f - static_cast<float>(i) / static_cast<float>(kNeutralSamples - 1));
    neutralSrc(L, inSp, src.data());
    xform->Apply(apply, dst.data(), src.data());
    for (int c = 0; c < outCh; ++c) {
      float v = static_cast<float>(dst[c]) * 100.0f;                  // device 0..1 -> %
      if (!std::isfinite(v)) v = 0.0f;
      Vertex vert; vert.x = L; vert.y = v;
      series[c].verts.push_back(vert);
    }
  }

  delete apply;
  delete xform;

  // Per-colorant display hint: the Lab of 100% of each ink alone, obtained from
  // the forward A2B1 (relative-colorimetric) table. Carried in series.colorHint as
  // a "L,a,b" string - DATA only; the receiver does the Lab->sRGB display mapping
  // (this model never produces display colours). Absent/odd A2B1 -> no hint, and the
  // receiver falls back to its channel palette.
  if (CIccTag* fwdTag = pIcc->FindTag(icSigAToB1Tag)) {
    if (CIccXform* fwd = CIccXform::Create(pIcc, fwdTag, /*bInput=*/true,
                                           icRelativeColorimetric, icInterpLinear, /*pPcc=*/NULL, /*bUseSpectralPCS=*/false, /*pHintManager=*/NULL, /*bOwnsProfile=*/false)) {
      fwd->ShareProfile();
      icStatusCMM fst = icCmmStatOk;
      CIccApplyXform* fapply = (fwd->Begin() == icCmmStatOk) ? fwd->GetNewApply(fst) : nullptr;
      if (fapply && fst == icCmmStatOk &&
          fwd->GetNumSrcSamples() == outCh && fwd->GetNumDstSamples() >= 3) {
        const icColorSpaceSignature fOut = fwd->GetDstSpace();
        // Value-initialised, not fill-constructed - see the note on src/dst above (#1777).
        std::vector<icFloatNumber> usrc(outCh), udst(fwd->GetNumDstSamples());
        for (int c = 0; c < outCh; ++c) {
          for (int k = 0; k < outCh; ++k) usrc[k] = (k == c) ? 1.0f : 0.0f;   // 100% of ink c
          fwd->Apply(fapply, udst.data(), usrc.data());
          icFloatNumber lab[3] = { udst[0], udst[1], udst[2] };
          if (fOut == icSigLabData) {
            icLabFromPcs(lab);
          } else if (fOut == icSigXYZData) {
            icXyzFromPcs(lab);
            icFloatNumber l2[3] = { 0, 0, 0 };
            icXYZtoLab(l2, lab, nullptr);
            lab[0] = l2[0]; lab[1] = l2[1]; lab[2] = l2[2];
          } else {
            continue;
          }
          // All three components feed the snprintf below, so all three must be finite -
          // a NaN/Inf in a* or b* (e.g. from a degenerate XYZ->Lab conversion) would
          // otherwise be formatted straight into the colour hint.
          if (!std::isfinite(lab[0]) || !std::isfinite(lab[1]) || !std::isfinite(lab[2])) continue;
          char buf[48];
          std::snprintf(buf, sizeof buf, "%.1f,%.1f,%.1f",
                        static_cast<double>(lab[0]), static_cast<double>(lab[1]), static_cast<double>(lab[2]));
          series[c].colorHint = buf;
        }
      }
      delete fapply;
      delete fwd;
    }
  }

  out = Graph{};
  out.title = sigDesc + " - neutral axis inking";
  out.xAxis.label = "L*";    out.xAxis.minHint = 100.0f; out.xAxis.maxHint = 0.0f;  // 100 left -> 0 right
  out.yAxis.label = "% ink"; out.yAxis.minHint = 0.0f;   out.yAxis.maxHint = 100.0f;
  for (auto& s : series) out.series.push_back(std::move(s));
  return true;
}

// -- Gamut volume: boundary voxelisation + flood-fill (see IccVizModel.hpp) ----
// Pure Lab-space geometry, adapted from chardata's gamut-wasm `gamutVolumeIcc`
// (the ICC-specific device->PCS boundary eval lives in the public GamutVolume()
// below). Why voxel occupancy and not signed-tetra / convex hull / star-|tetra|
// on the boundary: the sampled device boundary self-overlaps and isn't
// consistently wound, so those all mis-measure; voxel occupancy of the enclosed
// solid is robust. Dilate seals sampling gaps against flood-fill leaks; the
// erosion removes the dilation's outward bias - the CUBE (Chebyshev) dilation is
// matched by a 26-neighbour (Chebyshev) erosion, so dilate-then-erode is a proper
// morphological closing: the added shell cancels on the outer surface (corners
// included) rather than over-reaching there, while the gap-sealing survives.
// The boundary is sampled over ALL cube facets (see boundaryDeviceSamples), so it
// is captured for N>=4 too. `volume` remains a discrete-voxel ESTIMATE (resolution
// set by voxelSize), but without the earlier corner-high / CMYK-low systematic bias.

// Bounds for the untrusted / caller-supplied gamut-volume geometry (see the
// GamutVolume() argument clamping and the cell ceiling below).
const int           kMaxGamutDilate = 4;            // structuring-element radius clamp (DoS guard)
const double        kMinVoxelSize   = 0.5;          // dE*ab floor: caps the Lab grid resolution
const std::uint64_t kMaxVoxelCells  = 256000000ull; // total voxel ceiling (~256 MB; CWE-190/400)

// voxelEnclosedVolume - voxelise flat Lab boundary points (x3), dilate by `dilate`,
// flood-fill the
// exterior, erode the dilation back, return the enclosed volume (dE*ab^3). Fixed
// generous Lab box so real gamuts sit strictly interior to it; points outside the
// box are dropped (not clamped onto its face). Returns -1 with enclosedCells = -2 if
// vs is not a usable voxel pitch, and -1 with enclosedCells = -1 if the grid would
// exceed kMaxVoxelCells; the two are distinguished so GamutVolume() can report which.
double voxelEnclosedVolume(const std::vector<float>& lab, double vs,
                           int dilate, long long& enclosedCells) {
  const double Lmin = -20, Lmax = 120, ABmin = -150, ABmax = 150;
  // Establish the voxel-pitch precondition locally, before vs is used as a divisor.
  // Two distinct problems are rejected here, and only the first is a memory-safety
  // one:
  //
  //   vs == 0 or NaN - the quotient (Lmax - Lmin) / vs is +inf or NaN, and casting
  //     that to int below is undefined behaviour (CWE-681/CWE-704).
  //   vs < 0 or +inf - the quotient stays finite (e.g. 140 / -2 == -70, 140 / inf
  //     == +0), so the cast itself is well-defined; these are rejected because they
  //     are not usable resolutions. A negative pitch inverts the voxel mapping and a
  //     infinite one collapses the whole Lab box into a single cell, either of which
  //     yields a meaningless "volume" rather than a measurement.
  //
  // std::isfinite is what rejects NaN: NaN compares false against every relational
  // operator, so `vs < kMinVoxelSize` alone would let it through.
  //
  // GamutVolume() does sanitise its voxelSize argument before calling here, but that
  // clamp lives ~430 lines away at the call site. A helper that divides by a
  // parameter should not depend on a distant caller to stay memory-safe, and a future
  // caller (or a refactor that drops the clamp) would silently reintroduce the UB.
  //
  // Rejection is signalled through enclosedCells = -2, kept distinct from the
  // oversized-grid signal (-1) below so GamutVolume() reports an accurate reason
  // instead of aliasing an invalid pitch onto "grid too large". +inf is the one value
  // that still reaches this guard from the existing caller: it passes both that
  // caller's `!(vs > 0.0)` and `vs < kMinVoxelSize` tests.
  if (!std::isfinite(vs) || vs < kMinVoxelSize) { enclosedCells = -2; return -1.0; }
  const int nL = std::max(1, (int)std::ceil((Lmax - Lmin) / vs));
  const int nA = std::max(1, (int)std::ceil((ABmax - ABmin) / vs));
  const int nB = nA;
  // Reject an oversized/overflowing grid before allocating (a tiny caller voxelSize
  // could otherwise wrap the size_t product on 32-bit targets - CWE-190). Signalled
  // back through enclosedCells = -1 so GamutVolume() can fail cleanly.
  const std::uint64_t cells64 = static_cast<std::uint64_t>(nL) *
                                static_cast<std::uint64_t>(nA) *
                                static_cast<std::uint64_t>(nB);
  if (cells64 == 0 || cells64 > kMaxVoxelCells) { enclosedCells = -1; return -1.0; }
  auto IDX = [&](int l, int a, int b) -> std::size_t {
    return ((std::size_t)l * nA + a) * nB + b;
  };
  auto cl = [](int v, int hi) { return v < 0 ? 0 : (v >= hi ? hi - 1 : v); };
  std::vector<unsigned char> g((std::size_t)nL * nA * nB, 0);  // 0 empty, 1 solid, 2 exterior

  const int nPts = (int)(lab.size() / 3);
  for (int i = 0; i < nPts; ++i) {
    // Bounded floor: drop points outside the Lab box rather than clamping their
    // voxel onto the exterior face - a clamped solid voxel both clips the gamut to
    // the box and can block a flood-fill seed on that face. Range-checking here also
    // makes the float->int cast safe: GamutVolume filters only for finiteness, and a
    // huge-but-finite coordinate is UB to cast (CWE-704).
    const double lf = std::floor((lab[i*3]     - Lmin)  / vs);
    const double af = std::floor((lab[i*3 + 1] - ABmin) / vs);
    const double bf = std::floor((lab[i*3 + 2] - ABmin) / vs);
    if (!(lf >= 0.0 && lf < nL) || !(af >= 0.0 && af < nA) || !(bf >= 0.0 && bf < nB))
      continue;
    const int li = (int)lf, ai = (int)af, bi = (int)bf;
    // Splat a cube (Chebyshev ball of radius `dilate`). This seals diagonal gaps
    // between sparse boundary samples so the flood-fill can't leak through them.
    // The erosion below peels the SAME Chebyshev shell back (26-neighbour), so
    // dilate-then-erode is a proper morphological closing: the outward shell
    // cancels on the outer surface (corners included) while the gap-sealing
    // survives - no systematic corner-high bias, no leak-driven low bias.
    for (int dl = -dilate; dl <= dilate; ++dl)
      for (int da = -dilate; da <= dilate; ++da)
        for (int db = -dilate; db <= dilate; ++db)
          g[IDX(cl(li + dl, nL), cl(ai + da, nA), cl(bi + db, nB))] = 1;
  }

  std::vector<std::size_t> st;   // linear voxel ids; size_t so a >INT_MAX grid can't truncate
  auto push = [&](int l, int a, int b) {
    const std::size_t id = IDX(l, a, b);
    if (g[id] == 0) { g[id] = 2; st.push_back(id); }
  };
  for (int a = 0; a < nA; ++a) for (int b = 0; b < nB; ++b) { push(0, a, b); push(nL - 1, a, b); }
  for (int l = 0; l < nL; ++l) for (int b = 0; b < nB; ++b) { push(l, 0, b); push(l, nA - 1, b); }
  for (int l = 0; l < nL; ++l) for (int a = 0; a < nA; ++a) { push(l, a, 0); push(l, a, nB - 1); }
  while (!st.empty()) {
    const std::size_t id = st.back(); st.pop_back();
    const int b = (int)(id % nB), a = (int)((id / nB) % nA), l = (int)(id / ((std::size_t)nB * nA));
    if (l > 0)      push(l - 1, a, b);
    if (l < nL - 1) push(l + 1, a, b);
    if (a > 0)      push(l, a - 1, b);
    if (a < nA - 1) push(l, a + 1, b);
    if (b > 0)      push(l, a, b - 1);
    if (b < nB - 1) push(l, a, b + 1);
  }

  const std::size_t total = (std::size_t)nL * nA * nB;
  for (int pass = 0; pass < dilate; ++pass) {
    std::vector<std::size_t> add;
    for (std::size_t id = 0; id < total; ++id) {
      if (g[id] == 2) continue;
      const int b = (int)(id % nB), a = (int)((id / nB) % nA), l = (int)(id / ((std::size_t)nB * nA));
      // 26-neighbour (Chebyshev) test so the erosion peels the same cube shell the
      // dilation added (matched morphological closing). Any exterior neighbour in
      // the 3x3x3 stencil reclaims this cell.
      bool touchesExterior = false;
      for (int dl = -1; dl <= 1 && !touchesExterior; ++dl)
        for (int da = -1; da <= 1 && !touchesExterior; ++da)
          for (int db = -1; db <= 1 && !touchesExterior; ++db) {
            if (!dl && !da && !db) continue;
            const int ll = l + dl, aa = a + da, bb = b + db;
            if (ll < 0 || ll >= nL || aa < 0 || aa >= nA || bb < 0 || bb >= nB) continue;
            if (g[IDX(ll, aa, bb)] == 2) touchesExterior = true;
          }
      if (touchesExterior) add.push_back(id);
    }
    for (std::size_t id : add) g[id] = 2;
  }
  std::size_t ext = 0;
  for (std::size_t i = 0; i < total; ++i) if (g[i] == 2) ++ext;
  enclosedCells = (long long)(total - ext);
  return (double)enclosedCells * vs * vs * vs;
}

// boundarySampleCount - number of samples boundaryDeviceSamples(N, S) emits: the
// device N-cube has 2N
// facets, each an (N-1)-cube grid-sampled at (S+1) points per free axis. (Shared
// edges/faces between facets are double-counted, which voxelisation collapses.)
double boundarySampleCount(int N, int S) {
  if (N < 1) N = 1;
  const int e = (N >= 2) ? (N - 1) : 0;      // free axes per facet
  return 2.0 * N * std::pow((double)S + 1.0, (double)e);
}

// boundaryDeviceSamples - sample the BOUNDARY of the device N-cube in 0..1
// (IccProfLib device convention):
// the union of its 2N facets - each obtained by fixing one coordinate to 0 or 1 and
// grid-sampling the remaining N-1 coordinates at S+1 steps. Flat buffer, N floats
// per point.
//
// For N==3 this is the six cube faces (identical to the old 2-skeleton). For N>=4
// it also covers the interiors of the 3-faces (three free coordinates) the
// 2-skeleton missed, so the enclosed volume is no longer biased low for CMYK+.
std::vector<float> boundaryDeviceSamples(int N, int S) {
  std::vector<float> out;
  if (N < 1) return out;
  float cv[kMaxInkChannels];
  int   freeAxes[kMaxInkChannels];
  int   idx[kMaxInkChannels];
  for (int fixedAxis = 0; fixedAxis < N; ++fixedAxis) {
    int nFree = 0;
    for (int d = 0; d < N; ++d) if (d != fixedAxis) freeAxes[nFree++] = d;
    for (int fv = 0; fv <= 1; ++fv) {                 // fixed coordinate = 0 or 1
      for (int i = 0; i < nFree; ++i) idx[i] = 0;
      for (;;) {                                      // odometer over the nFree free axes, each 0..S
        for (int d = 0; d < N; ++d) cv[d] = 0.0f;
        cv[fixedAxis] = (float)fv;
        for (int i = 0; i < nFree; ++i) cv[freeAxes[i]] = (float)idx[i] / (float)S;
        for (int d = 0; d < N; ++d) out.push_back(cv[d]);
        int k = 0;
        for (; k < nFree; ++k) { if (++idx[k] <= S) break; idx[k] = 0; }
        if (k >= nFree) break;                          // all free axes wrapped (also ends the N==1 facet)
      }
    }
  }
  return out;
}

// gamutVolumeParams - auto-pick boundary-sampling params by colorant count; mirrors
// chardata gamut.js
// volumeParams. Returns steps = -1 when even the coarsest sampling would exceed
// the boundary-point ceiling (a very-high-channel profile -> volume unsupported).
void gamutVolumeParams(int N, int& steps, double& vs, int& dilate) {
  if (N < 1) N = 1;
  const double TARGET = 180000.0, MAX_POINTS = 1500000.0;
  // Boundary point count is 2N*(S+1)^(N-1); invert it to hit TARGET, then shrink
  // until it fits MAX_POINTS. (For N==3 the exponent is 2, reproducing the old
  // face-sampling budget; for N>=4 facet sampling grows faster, so S auto-drops.)
  const int e = (N >= 2) ? (N - 1) : 1;
  int s = (int)std::floor(std::pow(TARGET / (2.0 * N), 1.0 / (double)e)) - 1;
  if (s > 48) s = 48;
  if (s < 6)  s = 6;
  while (s > 2 && boundarySampleCount(N, s) > MAX_POINTS) --s;
  steps  = (boundarySampleCount(N, s) > MAX_POINTS) ? -1 : s;
  vs     = (N <= 4) ? 2.0 : (N <= 6 ? 2.5 : 3.0);
  dilate = (s >= 40) ? 1 : (s >= 20 ? 2 : 3);
}

// -- B2A round-trip helpers ----------------------------------------------------
// pcsToLabFull - decode a transform's internal-PCS-encoded output (Lab or XYZ) to
// human L*a*b*, returning false for an unsupported PCS. The round trip compares in
// L*a*b*, so both legs are brought to the same human Lab here rather than at each
// call site.
bool pcsToLabFull(const icFloatNumber* pcs, icColorSpaceSignature sp, icFloatNumber out[3]) {
  icFloatNumber v[3] = { pcs[0], pcs[1], pcs[2] };
  if (sp == icSigLabData) { icLabFromPcs(v); out[0]=v[0]; out[1]=v[1]; out[2]=v[2]; return true; }
  if (sp == icSigXYZData) { icXyzFromPcs(v); icXYZtoLab(out, v, nullptr); return true; }  // D50
  return false;
}
// deltaEab - plain Euclidean dE*ab (CIE76) between two L*a*b* triples; the per-point
// error the round-trip metric aggregates. CIE76 (not dE2000) is deliberate: this is
// a relative agreement check, so the simplest distance keeps the number interpretable.
double deltaEab(const icFloatNumber a[3], const icFloatNumber b[3]) {
  const double dL=a[0]-b[0], da=a[1]-b[1], db=a[2]-b[2];
  return std::sqrt(dL*dL + da*da + db*db);
}
// roundTripSteps - device interior-grid steps per axis, chosen so the full N-D seed
// grid stays near ~30k points (bounded and clamped to [2,32]) regardless of channel
// count, keeping the round trip fast without under-sampling low-channel devices.
int roundTripSteps(int N) {
  if (N < 1) N = 1;
  int s = (int)std::floor(std::pow(30000.0, 1.0 / N)) - 1;
  return s < 2 ? 2 : (s > 32 ? 32 : s);
}

} // namespace

// -- public API ---------------------------------------------------------------

// Enumerate - see IccVizModel.hpp for the contract. Implementation note: it probes
// a FIXED, canonically-ordered signature list instead of iterating the profile's
// tag list, which keeps the order deterministic and the walk cross-TU / WASM-safe
// (it never crosses a module boundary into the std::list tag directory). Each
// producer runs in "probe" mode (diag==nullptr) so a tag with nothing to show is
// skipped silently rather than advertised.
std::vector<Descriptor> Enumerate(CIccProfile* pIcc) {
  std::vector<Descriptor> out;
  if (!pIcc) return out;

  // NOTE: we deliberately do NOT iterate pIcc->m_Tags here. m_Tags is a
  // std::list owned by the IccProfLib translation unit; iterating it from this
  // TU walks off the end into the circular list (cross-TU std::list iterator
  // mismatch). CIccProfile::FindTag (which lives in IccProfLib) is safe, so we
  // probe a fixed, canonically-ordered signature list instead. This also gives
  // deterministic ordering using a fixed canonical signature list (NOT the profile's tag-table order).

  // Chromaticity first. Enumerated whenever the profile carries a media white
  // point OR the full RGB colorant set: buildChromaticityGraph plots the white
  // point on its own and only adds the primaries/gamut polygon when all three
  // colorants exist, so output/CMYK profiles (white point but no colorants)
  // still get a meaningful white-point-on-the-locus chart for wtpt.
  if (pIcc->FindTag(icSigMediaWhitePointTag) ||
      (pIcc->FindTag(icSigRedColorantTag) && pIcc->FindTag(icSigGreenColorantTag) &&
       pIcc->FindTag(icSigBlueColorantTag))) {
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
    // Probe only - diag==nullptr, so tags that legitimately carry no CLUT
    // (matrix/curve-only mAB/mBA) skip silently; a real RenderRaster() surfaces
    // any failure reason.
    if (buildClutRaster(t, sigStr(sig), probe)) {
      Descriptor d;
      d.kind = Kind::ClutImage; d.output = Output::Raster;
      d.id = "clut:" + sigStr(sig); d.title = sigStr(sig) + " CLUT"; d.tag = sig;
      out.push_back(std::move(d));
    }
  }

  // Neutral-axis inking (Kind::NeutralAxisInking) - one graph per B2A table that
  // maps PCS->device. (The receiver restricts these to output profiles and adds the
  // rendering-intent labels; structurally we just need a PCS-in / device-out CLUT.)
  static const icTagSignature kNeutralSigs[] = {
    icSigBToA0Tag, icSigBToA1Tag, icSigBToA2Tag };
  for (icTagSignature sig : kNeutralSigs) {
    auto* lut = dynamic_cast<CIccMBB*>(pIcc->FindTag(sig));
    if (!lut || !lut->GetCLUT()) continue;
    const icColorSpaceSignature inSp  = lut->GetCsInput();
    const icColorSpaceSignature outSp = lut->GetCsOutput();
    if (!isPcsSpace(inSp)) continue;                                 // PCS input
    if (isPcsSpace(outSp)) continue;                                 // device output
    const int outCh = lut->OutputChannels();
    if (outCh <= 0 || outCh > kMaxInkChannels) continue;
    Descriptor d;
    d.kind = Kind::NeutralAxisInking; d.output = Output::Graph;
    d.id = "neutral:" + sigStr(sig); d.title = sigStr(sig) + " neutral axis inking"; d.tag = sig;
    out.push_back(std::move(d));
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
    ab.id = "named:ab:" + sigStr(sig); ab.title = title + " - a*b* (" + sigStr(sig) + ")";
    ab.tag = sig; out.push_back(std::move(ab));
    Descriptor xy;
    xy.kind = Kind::NamedColorsXY; xy.output = Output::Graph;
    xy.id = "named:xy:" + sigStr(sig); xy.title = title + " - xy (" + sigStr(sig) + ")";
    xy.tag = sig; out.push_back(std::move(xy));
  }
  return out;
}

// RenderGraph - see IccVizModel.hpp. Re-enumerates to resolve the descriptor id,
// then dispatches to the matching producer (chromaticity / curve / named a*b* / xy
// / neutral-axis). Re-enumerating rather than caching descriptors keeps the API
// stateless and cheap (callers cache the parsed CIccProfile); diagnostics are
// returned as data and also echoed to stderr per the Verbosity.
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
        // than a generic string - restoring outputNamedColors' diagnostics.
        res.error = res.diagnostics.empty() ? "no colours" : res.diagnostics.back().message;
      }
      emitDiagnostics(res.diagnostics, v);
      return res;
    }
    if (d.kind == Kind::NeutralAxisInking) {
      if (buildNeutralAxisGraph(pIcc, d.tag, res.graph, &res.diagnostics))
        res.ok = true;
      else
        res.error = res.diagnostics.empty() ? "no neutral data"
                                            : res.diagnostics.back().message;
      emitDiagnostics(res.diagnostics, v);
      return res;
    }
    res.error = "unsupported graph kind"; return res;
  }
  res.error = "unknown visualization id: " + id;
  return res;
}

// RenderRaster - see IccVizModel.hpp. Resolves the raster descriptor id and builds
// the CLUT lattice image via buildClutRaster, surfacing the specific skip/failure
// reason as both an error string and a diagnostic. Only ClutImage descriptors carry
// a raster; every other kind is a graph and goes through RenderGraph instead.
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

// -- Gamut volume -------------------------------------------------------------
// GamutVolume - public entry: the ICC-specific half of the gamut-volume metric
// (contract in IccVizModel.hpp). Builds the device->PCS transform for one AToB tag,
// samples the device-cube boundary through it into L*a*b*, and hands the finite Lab
// points to voxelEnclosedVolume. Every profile-derived and caller-supplied value
// (channel count, sampling params) is bounded here first, since it drives large
// allocations/loops on untrusted input; a collapsed/unreliable result is flagged
// via GamutVolumeResult.degenerate rather than reported as a real measurement.
GamutVolumeResult GamutVolume(CIccProfile* pIcc, icTagSignature aToBTag,
                              icRenderingIntent intent,
                              int samplesPerAxis, double voxelSize, int dilate) {
  GamutVolumeResult r;
  auto fail = [&](const std::string& why) -> GamutVolumeResult { r.ok = false; r.error = why; return r; };

  if (!pIcc) return fail("null profile");

  // Only the device->PCS AToB tags describe a gamut boundary. Reject BToA (and any
  // other) signatures up front: because we build the transform with bInput=true,
  // CIccXform::GetSrcSpace() reports the profile's *device* colorSpace regardless of
  // the tag's actual direction (see IccCmm.cpp), so the isPcsSpace(srcSp) guard below
  // would NOT catch a BToA tag - its inverse LUT would run backwards and yield a
  // plausible-but-meaningless volume. Gate on the signature itself instead.
  if (aToBTag != icSigAToB0Tag && aToBTag != icSigAToB1Tag && aToBTag != icSigAToB2Tag)
    return fail("not an AToB tag");

  CIccTag* pTag = pIcc->FindTag(aToBTag);
  if (!pTag) return fail("AToB tag not present");

  // Device->PCS transform for this tag (bInput=true = A2B / "input" side).
  CIccXform* x = CIccXform::Create(pIcc, pTag, /*bInput=*/true, intent, icInterpLinear, /*pPcc=*/NULL, /*bUseSpectralPCS=*/false, /*pHintManager=*/NULL, /*bOwnsProfile=*/false);
  if (!x) return fail("could not build device->PCS transform");
  x->ShareProfile();                                   // we do NOT own pIcc
  if (x->Begin() != icCmmStatOk) { delete x; return fail("transform Begin failed"); }

  const icColorSpaceSignature srcSp = x->GetSrcSpace();
  const icColorSpaceSignature dstSp = x->GetDstSpace();
  const int N     = x->GetNumSrcSamples();
  const int dstCh = x->GetNumDstSamples();
  if (isPcsSpace(srcSp))                                { delete x; return fail("tag input is not a device space"); }
  if (dstSp != icSigLabData && dstSp != icSigXYZData)   { delete x; return fail("tag output is not a PCS"); }
  if (N < 1 || N > kMaxInkChannels)                     { delete x; return fail("unsupported device channel count"); }
  if (dstCh < 3)                                        { delete x; return fail("PCS output has < 3 channels"); }

  // Sampling params: auto-pick any left at their sentinel (<=0), then bound the
  // caller-supplied values so a crafted/typo argument can't drive a huge Lab grid
  // or an unbounded dilate/erode (CWE-400/CWE-190). `!(vs > 0)` also rejects NaN.
  int S = samplesPerAxis, dl = dilate;
  double vs = voxelSize;
  {
    int aS, aDl; double aVs;
    gamutVolumeParams(N, aS, aVs, aDl);
    if ((S <= 0 || dl <= 0) && aS < 0) { delete x; return fail("too many device channels for volume"); }
    if (S  <= 0)     S  = aS;
    if (!(vs > 0.0)) vs = aVs;
    if (dl <= 0)     dl = aDl;
  }
  if (S < 2) S = 2;
  if (vs < kMinVoxelSize) vs = kMinVoxelSize;         // floor the Lab grid resolution
  if (dl < 0) dl = 0;
  if (dl > kMaxGamutDilate) dl = kMaxGamutDilate;     // clamp the structuring element
  // Hard ceiling on total boundary points (CWE-400 guard against a crafted profile).
  if (boundarySampleCount(N, S) > 3000000.0) { delete x; return fail("device boundary too large for volume"); }

  icStatusCMM st = icCmmStatOk;
  CIccApplyXform* ap = x->GetNewApply(st);
  if (!ap || st != icCmmStatOk) { delete ap; delete x; return fail("transform apply init failed"); }

  // Sample the device-cube boundary (all facets) -> human L*a*b*.
  const std::vector<float> dev = boundaryDeviceSamples(N, S);
  const int nPts = (int)(dev.size() / N);
  std::vector<float> lab;
  lab.reserve((std::size_t)nPts * 3);
  // Value-initialised, not fill-constructed - see the note in buildNeutralAxisGraph
  // (#1777); this is the exact construction the reported diagnostic pointed at.
  std::vector<icFloatNumber> src(N), dst(dstCh);
  for (int i = 0; i < nPts; ++i) {
    for (int c = 0; c < N; ++c) src[c] = (icFloatNumber)dev[(std::size_t)i * N + c];
    x->Apply(ap, dst.data(), src.data());
    icFloatNumber v[3] = { dst[0], dst[1], dst[2] };
    if (dstSp == icSigLabData) {
      icLabFromPcs(v);                                 // internal PCS Lab -> human L*a*b*
    } else {
      icXyzFromPcs(v);                                 // internal PCS XYZ -> human XYZ (D50)
      icFloatNumber labv[3] = { 0, 0, 0 };
      icXYZtoLab(labv, v, nullptr);                    // nullptr white -> D50
      v[0] = labv[0]; v[1] = labv[1]; v[2] = labv[2];
    }
    if (std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2])) {
      lab.push_back((float)v[0]); lab.push_back((float)v[1]); lab.push_back((float)v[2]);
    }
  }
  delete ap;
  delete x;

  // Degeneracy floor: need >=3 finite boundary points to enclose any volume. NOTE
  // a boundary that survives this but has collapsed toward a point/plane still
  // yields a small, technically-ok volume with no distinct "degenerate" signal -
  // a caller cannot tell "genuinely tiny gamut" from "sampling collapsed".
  if (lab.size() < 9) return fail("no finite boundary samples");

  long long cells = 0;
  r.volume         = voxelEnclosedVolume(lab, vs, dl, cells);
  // Distinguish the two rejections voxelEnclosedVolume() can signal so the caller is
  // told which precondition it broke: -2 is an unusable voxel pitch (non-finite or
  // below kMinVoxelSize, reachable here only as a caller-supplied +inf voxelSize),
  // -1 is a grid that would exceed kMaxVoxelCells. Reporting an invalid pitch as
  // "grid too large" would be actively misleading - a +inf pitch collapses the Lab
  // box to a single cell, which is the smallest possible grid, not the largest.
  if (cells == -2) return fail("voxelSize is not a usable voxel pitch");  // x/ap already freed above
  if (cells < 0) return fail("voxel grid too large for volume");   // x/ap already freed above
  // A caller-supplied enormous/infinite voxelSize makes cells*vs^3 overflow to
  // +/-Inf (or NaN); never report a non-finite measurement as a successful volume.
  if (!std::isfinite(r.volume)) return fail("computed volume is not finite");
  r.voxels         = cells;
  r.samplesPerAxis = S;
  r.voxelSize      = vs;
  r.nColorants     = N;
  // Degeneracy signal (result still ok, but unreliable): flag when most boundary
  // samples were non-finite (transform failing over much of the device cube) or the
  // enclosed region is at/below the voxel-resolution floor (collapsed toward a
  // point/plane). Lets a caller show N/A instead of a misleading tiny number.
  const int finitePts = (int)(lab.size() / 3);
  // Thinnest principal extent of the boundary cloud. voxelEnclosedVolume seals
  // gaps with a morphological closing that floors a collapsed plane/line at ~1
  // voxel thick, so its volume stays a sheet/tube artifact the cell-count floor
  // above cannot catch. Flag degenerate when the cloud is effectively coplanar/
  // collinear (s3 a negligible fraction of s1) or thinner than the voxel grid can
  // resolve (s3 below vs): the volume is then not a meaningful 3-D measurement.
  double s1 = 0.0, s2 = 0.0, s3 = 0.0;
  iccvizmath::principalStdDevs(lab.data(), (std::size_t)finitePts, s1, s2, s3);
  (void)s2;   // middle extent unused: s3 alone captures plane and line collapse
  const bool flat = (s1 > 0.0) && (s3 < 0.02 * s1 || s3 < vs);
  r.degenerate     = (finitePts * 2 < nPts) || (cells <= 27) || flat;
  r.ok             = true;
  return r;
}

// -- B2A round-trip accuracy ---------------------------------------------------
// RoundTripDE - public entry (contract in IccVizModel.hpp). Seeds in-gamut L*a*b*
// from a device interior grid via A2B, round-trips each Lab through B2A then A2B,
// and aggregates dE*ab into mean / p90 / max / stddev. Seeding from the device cube
// (not a raw Lab grid) guarantees the test points are wholly in-gamut, so the metric
// reflects genuine B2A/A2B agreement rather than out-of-gamut clamping. Matching
// AToB/BToA tags and channel counts are verified before sampling (untrusted input).
RoundTripResult RoundTripDE(CIccProfile* pIcc, icRenderingIntent intent,
                            int samplesPerAxis) {
  RoundTripResult r;
  auto fail = [&](const std::string& why) -> RoundTripResult { r.ok = false; r.error = why; return r; };
  if (!pIcc) return fail("null profile");

  // Matching AToB / BToA tags for the intent (intent also drives PCS white handling).
  icTagSignature a2bSig, b2aSig;
  switch (intent) {
    case icPerceptual: a2bSig = icSigAToB0Tag; b2aSig = icSigBToA0Tag; break;
    case icSaturation: a2bSig = icSigAToB2Tag; b2aSig = icSigBToA2Tag; break;
    default:           a2bSig = icSigAToB1Tag; b2aSig = icSigBToA1Tag; break;  // relative + absolute
  }
  CIccTag* a2bTag = pIcc->FindTag(a2bSig);
  CIccTag* b2aTag = pIcc->FindTag(b2aSig);
  if (!a2bTag) return fail("AToB tag not present");
  if (!b2aTag) return fail("BToA tag not present");

  CIccXform* xA = CIccXform::Create(pIcc, a2bTag, /*bInput=*/true,  intent, icInterpLinear, /*pPcc=*/NULL, /*bUseSpectralPCS=*/false, /*pHintManager=*/NULL, /*bOwnsProfile=*/false);
  CIccXform* xB = CIccXform::Create(pIcc, b2aTag, /*bInput=*/false, intent, icInterpLinear, /*pPcc=*/NULL, /*bUseSpectralPCS=*/false, /*pHintManager=*/NULL, /*bOwnsProfile=*/false);
  if (!xA || !xB) { delete xA; delete xB; return fail("could not build transforms"); }
  xA->ShareProfile(); xB->ShareProfile();
  if (xA->Begin() != icCmmStatOk || xB->Begin() != icCmmStatOk) { delete xA; delete xB; return fail("transform Begin failed"); }

  const icColorSpaceSignature devSp = xA->GetSrcSpace();
  const icColorSpaceSignature pcsSp = xA->GetDstSpace();
  const int N    = xA->GetNumSrcSamples();     // device channels
  const int nPcs = xA->GetNumDstSamples();     // 3
  if (isPcsSpace(devSp))                                { delete xA; delete xB; return fail("AToB input is not a device space"); }
  if (pcsSp != icSigLabData && pcsSp != icSigXYZData)   { delete xA; delete xB; return fail("PCS is not Lab/XYZ"); }
  if (N < 1 || N > kMaxInkChannels)                     { delete xA; delete xB; return fail("unsupported device channel count"); }
  if (nPcs < 3)                                         { delete xA; delete xB; return fail("PCS has < 3 channels"); }
  // xB reads GetNumSrcSamples() floats from pcs1 (sized nPcs) and writes
  // GetNumDstSamples() into dev2 (sized N); require an exact match on BOTH so Apply
  // can never over-read pcs1 (a crafted B2A declaring >nPcs input channels would
  // otherwise read past the PCS buffer) or over-write dev2.
  if (xB->GetNumSrcSamples() != nPcs || xB->GetNumDstSamples() != N) { delete xA; delete xB; return fail("BToA transform shape mismatch"); }

  int S = samplesPerAxis > 0 ? samplesPerAxis : roundTripSteps(N);
  if (S < 2) S = 2;
  // total is the seed grid's point count, (S+1)^N, and is later cast to size_t for
  // the des.reserve() below - so pin why that cast is well-defined here rather than
  // at the cast. The proof is specific to these operands, not a general property of
  // pow(): pow() certainly can return NaN from finite arguments (a negative base with
  // a non-integer exponent, for instance). Here the base is S + 1 with S >= 2 from the
  // clamp on the line above, so it is positive and at least 3, and the exponent N is a
  // positive integer in [1, kMaxInkChannels] from the channel-count check earlier in
  // this function. A positive base raised to a positive integral exponent is an exact
  // finite product or, on overflow, +inf - never NaN, never negative. Even though NaN
  // is therefore unreachable, the guard below rejects any non-finite total explicitly
  // with std::isfinite rather than leaning on the ceiling test alone to catch +inf:
  // that makes the cast's precondition local and executable (and clears the
  // float-to-int-cast scanner at #2335, which recognises an isfinite/isnan call as the
  // guard but cannot follow the pow() argument above). total thus lands in [3, 3000000]
  // at the cast (the minimum is N == 1 giving 3^1, not 9).
  double total = std::pow((double)S + 1.0, N);
  if (!std::isfinite(total) || total > 3000000.0) { delete xA; delete xB; return fail("seed grid too large"); }
  // Reserve the delta-E accumulator now, while the (size_t)total cast still sits within
  // a few lines of the isfinite guard above (the float-to-int-cast scanner, #2335, ties
  // a guard to a cast only when it is <=5 lines above). total is finite, in [3,3000000].
  std::vector<double> des;
  des.reserve((size_t)total);

  icStatusCMM st = icCmmStatOk;
  CIccApplyXform* apA = xA->GetNewApply(st);
  CIccApplyXform* apB = (st == icCmmStatOk) ? xB->GetNewApply(st) : nullptr;
  if (!apA || !apB || st != icCmmStatOk) { delete apA; delete apB; delete xA; delete xB; return fail("transform apply init failed"); }

  // Seed in-gamut Lab from a device interior grid via A2B, then round-trip each.
  // Value-initialised, not fill-constructed - see the note in buildNeutralAxisGraph
  // (#1777). RoundTripDE carries a second instance of the same diagnostic.
  std::vector<icFloatNumber> dev(N), pcs1(nPcs), dev2(N), pcs2(nPcs);
  std::vector<int> idx(N);
  for (;;) {
    for (int c = 0; c < N; ++c) dev[c] = (icFloatNumber)idx[c] / S;   // device 0..1
    xA->Apply(apA, pcs1.data(), dev.data());     // device -> PCS (Lab1)
    xB->Apply(apB, dev2.data(), pcs1.data());    // Lab1 -> device
    xA->Apply(apA, pcs2.data(), dev2.data());    // device -> PCS (Lab2)
    icFloatNumber lab1[3], lab2[3];
    if (pcsToLabFull(pcs1.data(), pcsSp, lab1) && pcsToLabFull(pcs2.data(), pcsSp, lab2)) {
      const double de = deltaEab(lab1, lab2);
      if (std::isfinite(de)) des.push_back(de);
    }
    int d = 0;
    for (; d < N; ++d) { if (++idx[d] <= S) break; idx[d] = 0; }
    if (d == N) break;
  }
  delete apA; delete apB; delete xA; delete xB;

  if (des.empty()) return fail("no finite round-trip samples");
  std::sort(des.begin(), des.end());
  double sum = 0.0; for (double d : des) sum += d;
  const double mean = sum / des.size();
  double var = 0.0; for (double d : des) var += (d - mean) * (d - mean);
  var /= des.size();
  // 90th-percentile index = floor(0.90 * (n-1)) for integer n = des.size(). Compute it
  // as exact integer arithmetic (9*(n-1))/10 rather than std::floor on a double: it is
  // the same index for every n (des.size() <= 3,000,000 here, so 9*(n-1) cannot
  // overflow), and it avoids a float-to-int cast the scanner would flag as NaN-unsafe.
  const std::size_t p90i = (9 * (des.size() - 1)) / 10;

  r.ok = true;
  r.n = (int)des.size();
  r.meanDE = mean;
  r.p90DE = des[p90i];
  r.maxDE = des.back();
  r.stdDE = std::sqrt(var);
  r.nColorants = N;
  return r;
}

// -- diagnostic output policy (see IccVizModel.hpp) ----------------------------
// SetSilent - set the process-global switch that suppresses the stderr echo of
// diagnostics (the data is always still returned in each result). Global (not a
// parameter) so a caller configures it once; defaults to not-silent so code that
// never touches it behaves exactly like the CLI.
void SetSilent(bool silent) { g_silent = silent; }
// GetSilent - read that global switch.
bool GetSilent() { return g_silent; }
// SetDiagnosticContext - set the optional "<name>: " prefix prepended to each
// stderr diagnostic line; the CLI sets the profile filename here so its output
// matches iccProfileVisualize byte-for-byte.
void SetDiagnosticContext(const std::string& name) { g_diagContext = name; }

} // namespace iccviz
