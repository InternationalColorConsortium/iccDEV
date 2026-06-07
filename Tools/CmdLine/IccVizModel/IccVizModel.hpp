/**
 * IccVizModel — data-first profile visualization model.
 *
 * A standalone, dependency-light re-implementation of the visualizations that
 * iccDEV's iccProfileVisualize tool draws into a single PDF/TIFF report. Where
 * iccProfileVisualize *renders* (it emits finished PDF pages and TIFF rasters),
 * this model *returns the underlying data*:
 *
 *   - Graph visualizations (tone curves, chromaticity, named-colour scatters)
 *     come back as IccVizGraph: 2-D point series plus axis range hints, so the
 *     caller draws/rasterizes them in its own look-and-feel. NO raster is
 *     produced for graph types.
 *   - Genuine images (the nD CLUT lattice) come back as IccVizRaster: the same
 *     ICC-normalized samples iccProfileVisualize would have written to TIFF,
 *     plus geometry — the caller decides how to colour-manage/display them.
 *
 * Series carry a role: Primary (the profile's own data — primaries, white,
 * curve, named colours) vs Hint (reference geometry the caller may draw or
 * ignore — spectral locus, planckian curve, wavelength labels, chroma circles,
 * identity line). No ticks/grid are shipped: those are a caller-side decision;
 * only an axis range *hint* is provided.
 *
 * Design intent: this file depends ONLY on IccProfLib + spectralLocus.hpp + the
 * C++ standard library — no embind, no nlohmann, no PDF/TIFF writers — so it can
 * be lifted into iccDEV verbatim once the profiletool proof-of-concept is
 * approved. iccProfileVisualize.cpp is deliberately left untouched; the math is
 * duplicated here on purpose so the two can be diffed independently.
 */

#ifndef ICC_VIZ_MODEL_HPP
#define ICC_VIZ_MODEL_HPP

#include "IccProfLibConf.h"    // required before icProfileHeader.h (uint types/config)
#include "icProfileHeader.h"   // icTagSignature, icColorSpaceSignature
#include <string>
#include <vector>
#include <limits>

class CIccProfile;

namespace iccviz {

// Stable identity for a visualization category. Append-only: existing values
// never change, so they are safe to use as a persistent wire identifier.
enum class Kind : unsigned int {
  Curve1D        = 1,   // 1-D tone curve (TRC, or an A/B/M curve of a LUT tag)
  ChromaticityXY = 2,   // RGB primaries + white on the CIE 1931 xy chart
  NamedColorsAB  = 3,   // named/colorant colours on a CIELAB a*b* chart
  NamedColorsXY  = 4,   // named/colorant colours on the xy chart
  ClutImage      = 5,   // nD CLUT lattice flattened to an image (raster)
  // SmoothnessLattice = 6,  // reserved — deferred to a later phase
};

enum class Output : unsigned char { Graph, Raster };

// What a series represents, so the caller can style data vs reference geometry.
enum class Role : unsigned char { Primary, Hint };

enum class Shape : unsigned char { Polyline, ClosedPath, Scatter };

struct Vertex {
  float x = 0.0f;
  float y = 0.0f;
  std::string label;                                  // "" when unlabelled
  float aux = std::numeric_limits<float>::quiet_NaN(); // NaN when no aux scalar
};

struct Series {
  std::string id;          // "curve","identity","locus","planckian","chroma30",…
  std::string name;        // human/legend label
  Role  role  = Role::Primary;
  Shape shape = Shape::Polyline;
  std::string auxKind;     // "", "Lstar", "nm", "kelvin" — meaning of Vertex.aux
  std::string colorHint;   // optional: "R","G","B","white","neutral","locus"
  std::vector<Vertex> verts;
};

struct Axis {
  std::string label;       // "Input", "a*", "x (CIE 1931)"
  float minHint = 0.0f;    // suggested range only — caller may recompute/override
  float maxHint = 1.0f;
  bool  equalAspect = false;  // chart wants square aspect (xy / ab plots)
};

struct Graph {
  std::string title;
  std::string description;   // e.g. curve Describe() text
  Axis xAxis, yAxis;
  std::vector<Series> series;  // mixed Primary + Hint
};

struct Raster {
  int  width = 0, height = 0, channels = 0, bitsPerChannel = 0;
  int  photometric = 1;       // TIFF-style hint: 0/1 gray, 2 RGB, 5 CMYK, 8 CIELAB
  bool normalizedICC = true;  // samples are ICC-normalized (not TIFF-standard Lab)
  std::vector<unsigned char> samples;  // row-major, channels interleaved
};

// One available visualization. `id` is stable and is what callers pass back to
// render(). Internal addressing fields (tag/group/index) let render() rebuild
// the exact source object without re-parsing the id string.
struct Descriptor {
  Kind   kind;
  Output output;
  std::string id;
  std::string title;
  icTagSignature tag = static_cast<icTagSignature>(0);  // owning tag (0 = whole profile)
  char grp = 0;          // 'A' | 'B' | 'M' for LUT sub-curves, else 0
  int  idx = -1;         // channel index within grp, else -1
};

struct GraphResult  { bool ok = false; std::string error; Graph  graph;  };
struct RasterResult { bool ok = false; std::string error; Raster raster; };

// List every visualization available for the profile, in the SAME order
// iccProfileVisualize emits them (chromaticity first, then per-tag in tag-table
// order: TRC curves, then each LUT's A/B/M curves followed by its CLUT image,
// then named/colorant tables as a*b* then xy).
std::vector<Descriptor> Enumerate(CIccProfile* pIcc);

// Render one graph / raster by descriptor id. render*ById re-enumerates to find
// the descriptor (cheap; callers should cache the parsed profile).
GraphResult  RenderGraph (CIccProfile* pIcc, const std::string& id);
RasterResult RenderRaster(CIccProfile* pIcc, const std::string& id);

} // namespace iccviz

#endif // ICC_VIZ_MODEL_HPP
