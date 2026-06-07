/**
 * IccVizModel implementation — see IccVizModel.hpp.
 *
 * The producers below mirror, in math, the plots iccProfileVisualize.cpp draws
 * into PDF/TIFF, but emit data structures instead of drawing commands. Kept
 * intentionally independent of that file so the two can be diffed.
 */

#include "IccVizModel.hpp"

#include "IccProfile.h"
#include "IccTag.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include "spectralLocus.hpp"   // const spectralLocus2degree (internal linkage)

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace iccviz {
namespace {

constexpr float kChromaScale = 0.85f;   // matches chromaticityChartScale
constexpr float kAbHalfRange = 130.0f;  // matches abChartScale/2
const float kNaN = std::numeric_limits<float>::quiet_NaN();

std::string sigStr(icTagSignature sig) {
  char buf[64];
  return std::string(icGetSigStr(buf, sizeof buf, static_cast<icUInt32Number>(sig)));
}

// ── colour helpers (ported verbatim from iccProfileVisualize.cpp) ────────────

struct XY { float x, y; };

XY xyFromICCXYZ(const icXYZNumber* xyz) {
  float X = xyz->X / 65535.0f, Y = xyz->Y / 65535.0f, Z = xyz->Z / 65535.0f;
  float sum = X + Y + Z;
  if (sum <= 1e-8f) return {0.0f, 0.0f};
  return {X / sum, Y / sum};
}

XY xyFromXYZFloat(const icFloatNumber* xyz) {
  float sum = xyz[0] + xyz[1] + xyz[2];
  if (sum <= 1e-8f) return {0.0f, 0.0f};
  return {xyz[0] / sum, xyz[1] / sum};
}

// Planckian locus approximation (Kang et al. 2002), ported verbatim.
XY approxPlanck(double t) {
  const double c3a=-0.2661239,c2a=-0.2343589,c1a=0.8776956,c0a=0.179910;
  const double c3b=-3.0258469,c2b=2.1070379,c1b=0.2226347,c0b=0.240390;
  const double k3a=-1.1063814,k2a=-1.34811020,k1a=2.18555832,k0a=-0.20219683;
  const double k3b=-0.9549476,k2b=-1.37418593,k1b=2.09137015,k0b=-0.16748867;
  const double k3c=3.0817580,k2c=-5.87338670,k1c=3.75112997,k0c=-0.37001483;
  double t2=t*t, t3=t*t*t, x;
  if (t < 4000.0) x = c3a*(1e9/t3)+c2a*(1e6/t2)+c1a*(1e3/t)+c0a;
  else            x = c3b*(1e9/t3)+c2b*(1e6/t2)+c1b*(1e3/t)+c0b;
  double x2=x*x, x3=x*x*x, y;
  if (t < 2222.0)      y = k3a*x3+k2a*x2+k1a*x+k0a;
  else if (t < 4000.0) y = k3b*x3+k2b*x2+k1b*x+k0b;
  else                 y = k3c*x3+k2c*x2+k1c*x+k0c;
  return {static_cast<float>(x), static_cast<float>(y)};
}

// Curated wavelength labels along the spectral locus (from iccProfileVisualize).
const int kLocusLabels[] = {
  360, 460, 450, 470, 475, 480, 485, 490, 495, 500, 505, 510, 515,
  520, 530, 540, 550, 560, 570, 580, 590, 600, 610, 620, 640, 700
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

// ── curve description + validation gate (mirrors describe1DLUT) ──────────────

bool curvePasses(CIccCurve* curve, const std::string& sigDesc) {
  std::string path = ":" + sigDesc, report;
  return curve->Validate(path, report, nullptr) <= icValidateWarning;
}

std::string describeCurve(CIccCurve* curve) {
  if (auto* tc = dynamic_cast<CIccTagCurve*>(curve)) {
    icUInt32Number size = tc->GetSize();
    if (size == 0) return "Y = X";
    if (size == 1) {
      icFloatNumber g = (*tc)[0] * 256.0f;
      return "Y = X ^ " + std::to_string(g);
    }
    return "LookupTable[" + std::to_string(size) + "]";
  }
  std::string desc;
  curve->Describe(desc, 100);
  return desc;
}

// ── producers ────────────────────────────────────────────────────────────────

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
  (void)g;
}

Graph buildChromaticityGraph(CIccProfile* pIcc) {
  Graph g;
  g.title = "Chromaticity xy";
  g.xAxis = Axis{"x (CIE 1931)", 0.0f, kChromaScale, true};
  g.yAxis = Axis{"y (CIE 1931)", 0.0f, kChromaScale, true};

  // Spectral locus (Hint, closed horseshoe).
  Series locus;
  locus.id = "locus"; locus.name = "Spectral locus"; locus.role = Role::Hint;
  locus.shape = Shape::ClosedPath; locus.colorHint = "locus";
  locus.verts.reserve(spectralLocus2degree.size());
  for (const auto& p : spectralLocus2degree)
    locus.verts.push_back(Vertex{static_cast<float>(p.x), static_cast<float>(p.y), "", kNaN});
  g.series.push_back(std::move(locus));

  // Wavelength number labels along the locus (Hint).
  Series labels;
  labels.id = "locusLabels"; labels.name = "Wavelengths"; labels.role = Role::Hint;
  labels.shape = Shape::Scatter; labels.auxKind = "nm"; labels.colorHint = "locus";
  int wlOffset = spectralLocus2degree.front().wavelength;
  for (int nm : kLocusLabels) {
    size_t idx = static_cast<size_t>(nm - wlOffset);
    if (idx >= spectralLocus2degree.size()) continue;
    const auto& p = spectralLocus2degree[idx];
    labels.verts.push_back(Vertex{static_cast<float>(p.x), static_cast<float>(p.y),
                                  std::to_string(nm), static_cast<float>(nm)});
  }
  g.series.push_back(std::move(labels));

  // Planckian (blackbody) locus (Hint).
  Series planck;
  planck.id = "planckian"; planck.name = "Planckian locus"; planck.role = Role::Hint;
  planck.shape = Shape::Polyline; planck.auxKind = "kelvin"; planck.colorHint = "neutral";
  for (float t = 1500.0f; t <= 20000.0f; t += 200.0f) {
    XY p = approxPlanck(t);
    planck.verts.push_back(Vertex{p.x, p.y, "", t});
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

// Collect named/colorant colours as Lab + name (mirrors outputNamedColors).
bool collectNamedColors(CIccProfile* pIcc, CIccTag* tag,
                        std::vector<NamedLab>& out, std::string& title) {
  icTagTypeSignature type = tag->GetType();
  icFloatNumber illum[3];
  pIcc->getNormIlluminantXYZ(illum);

  if (type == icSigColorantTableType) {
    auto* table = dynamic_cast<CIccTagColorantTable*>(tag);
    if (!table) return false;
    std::string path = ":colorantTable", report;
    if (table->Validate(path, report, nullptr) > icValidateWarning) return false;
    icColorSpaceSignature pcs = pIcc->m_Header.pcs;
    if (pcs != icSigXYZData && pcs != icSigLabData) return false;
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
    if (!table) return false;
    std::string path = ":namedColor2", report;
    if (table->Validate(path, report, nullptr) > icValidateWarning) return false;
    icColorSpaceSignature pcs = table->GetPCS();
    if (pcs != icSigXYZData && pcs != icSigLabData) return false;
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

  return false;  // tagArray (v5) not yet dissected, like upstream
}

Graph buildNamedAB(const std::vector<NamedLab>& colors, const std::string& title) {
  Graph g;
  g.title = title + " — a*b*";
  g.xAxis = Axis{"a*", -kAbHalfRange, kAbHalfRange, true};
  g.yAxis = Axis{"b*", -kAbHalfRange, kAbHalfRange, true};

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
  // Quadrant labels (Hint) — match the PDF's +a Magenta / -a Green / etc.
  Series ax;
  ax.id = "axisLabels"; ax.name = "Axes"; ax.role = Role::Hint; ax.shape = Shape::Scatter;
  ax.colorHint = "neutral";
  ax.verts = {
    Vertex{ kAbHalfRange, 0, "+a Magenta", kNaN}, Vertex{-kAbHalfRange, 0, "-a Green", kNaN},
    Vertex{0,  kAbHalfRange, "+b Yellow", kNaN},  Vertex{0, -kAbHalfRange, "-b Blue", kNaN},
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
  g.title = title + " — xy";
  g.xAxis = Axis{"x (CIE 1931)", 0.0f, kChromaScale, true};
  g.yAxis = Axis{"y (CIE 1931)", 0.0f, kChromaScale, true};

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

// CLUT lattice → raster (ported from output3DLUT's flatten).
bool buildClutRaster(CIccTag* tag, Raster& out) {
  auto* lut = dynamic_cast<CIccMBB*>(tag);
  if (!lut) return false;
  CIccCLUT* clut = lut->GetCLUT();
  if (!clut) return false;

  int bytes = lut->GetPrecision();
  int inputChannels = lut->InputChannels();
  int outputChannels = lut->OutputChannels();
  if (inputChannels <= 0 || outputChannels <= 0 || bytes <= 0) return false;

  clut->Begin();
  int gridPoints = clut->GridPoints();
  if (gridPoints <= 0) return false;

  int tiles = gridPoints, tileWidth = 1, tileHeight = 1;
  if (inputChannels >= 2) { tileWidth = clut->GridPoint(1); if (tileWidth <= 0) return false; }
  if (inputChannels >= 3) { tileHeight = clut->GridPoint(2); if (tileHeight <= 0) return false; }
  if (inputChannels > 3)
    for (int i = 3; i < inputChannels; ++i) {
      int g = clut->GridPoint(i); if (g <= 0) return false; tiles *= g;
    }
  if (inputChannels == 1) { tileWidth = tiles; tiles = 1; tileHeight = 1; }
  if (inputChannels == 2) { tileHeight = tiles; tiles = 1; }
  if (tiles <= 0) tiles = 1;

  double sq = std::sqrt(static_cast<double>(tiles));
  int tilesWide = static_cast<int>(sq);
  if (tilesWide <= 0) tilesWide = 1;
  if (inputChannels > 3 && (inputChannels & 1)) {
    int old = tilesWide;
    tilesWide -= (tilesWide % (gridPoints * tileWidth));
    if (tilesWide == 0) tilesWide = old;
  }
  int tilesHigh = (tiles + (tilesWide - 1)) / tilesWide;
  int imageWidth = tilesWide * tileWidth;
  int imageHeight = tilesHigh * tileHeight;
  if (imageWidth <= 0 || imageHeight <= 0) return false;

  size_t bufferSize = static_cast<size_t>(imageWidth) * imageHeight * outputChannels * bytes;
  if (!bufferSize) return false;

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

// Append LUT sub-curve descriptors in the A→B→M order output3DLUT uses.
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
      if (!curvePasses(c, label)) continue;
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

// ── public API ───────────────────────────────────────────────────────────────

std::vector<Descriptor> Enumerate(CIccProfile* pIcc) {
  std::vector<Descriptor> out;
  if (!pIcc) return out;

  // NOTE: we deliberately do NOT iterate pIcc->m_Tags here. m_Tags is a
  // std::list owned by the IccProfLib translation unit; iterating it from this
  // TU walks off the end into the circular list (cross-TU std::list iterator
  // mismatch). CIccProfile::FindTag (which lives in IccProfLib) is safe, so we
  // probe a fixed, canonically-ordered signature list instead. This also gives
  // deterministic ordering — close to, though not byte-identical with, the
  // file-order iccProfileVisualize uses.

  // Chromaticity first (only when RGB colorants are present, like upstream).
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
    if (!c || !curvePasses(c, sigStr(sig))) continue;
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
    if (buildClutRaster(t, probe)) {
      Descriptor d;
      d.kind = Kind::ClutImage; d.output = Output::Raster;
      d.id = "clut:" + sigStr(sig); d.title = sigStr(sig) + " CLUT"; d.tag = sig;
      out.push_back(std::move(d));
    }
  }

  static const icTagSignature kNamedSigs[] = {
    icSigNamedColorTag, icSigNamedColor2Tag, icSigColorantTableTag, icSigColorantTableOutTag };
  for (icTagSignature sig : kNamedSigs) {
    CIccTag* t = pIcc->FindTag(sig);
    if (!t) continue;
    std::vector<NamedLab> colors; std::string title;
    if (!collectNamedColors(pIcc, t, colors, title)) continue;
    Descriptor ab;
    ab.kind = Kind::NamedColorsAB; ab.output = Output::Graph;
    ab.id = "named:ab:" + sigStr(sig); ab.title = title + " — a*b* (" + sigStr(sig) + ")";
    ab.tag = sig; out.push_back(std::move(ab));
    Descriptor xy;
    xy.kind = Kind::NamedColorsXY; xy.output = Output::Graph;
    xy.id = "named:xy:" + sigStr(sig); xy.title = title + " — xy (" + sigStr(sig) + ")";
    xy.tag = sig; out.push_back(std::move(xy));
  }
  return out;
}

GraphResult RenderGraph(CIccProfile* pIcc, const std::string& id) {
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
      res.graph = buildCurveGraph(c, d.title); res.ok = true; return res;
    }
    if (d.kind == Kind::NamedColorsAB || d.kind == Kind::NamedColorsXY) {
      CIccTag* t = pIcc->FindTag(d.tag);
      std::vector<NamedLab> colors; std::string title;
      if (!t || !collectNamedColors(pIcc, t, colors, title)) { res.error = "no colours"; return res; }
      res.graph = (d.kind == Kind::NamedColorsAB) ? buildNamedAB(colors, title)
                                                  : buildNamedXY(pIcc, colors, title);
      res.ok = true; return res;
    }
    res.error = "unsupported graph kind"; return res;
  }
  res.error = "unknown visualization id: " + id;
  return res;
}

RasterResult RenderRaster(CIccProfile* pIcc, const std::string& id) {
  RasterResult res;
  if (!pIcc) { res.error = "null profile"; return res; }
  for (const Descriptor& d : Enumerate(pIcc)) {
    if (d.id != id) continue;
    if (d.output != Output::Raster) { res.error = "descriptor is not a raster"; return res; }
    CIccTag* t = pIcc->FindTag(d.tag);
    if (!t || !buildClutRaster(t, res.raster)) { res.error = "could not build raster"; return res; }
    res.ok = true; return res;
  }
  res.error = "unknown visualization id: " + id;
  return res;
}

} // namespace iccviz
