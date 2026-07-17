/*
  File:     IccVizModel.hpp

  Contains: Data-first profile visualization model - public API (pick a Kind, supply a CIccProfile, call Enumerate/RenderGraph/RenderRaster).

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
 * IccVizModel - data-first profile visualization model.
 *
 * Computes a profile's visualizations - tone curves, the CIE 1931 chromaticity
 * chart, named/colorant a*b* and xy scatters, and the nD CLUT lattice - and
 * *returns the underlying data* rather than finished drawings:
 *
 *   - Graph visualizations (tone curves, chromaticity, named-colour scatters)
 *     come back as Graph: 2-D point series plus axis range hints, so the caller
 *     draws/rasterizes them in its own look-and-feel. NO raster is produced for
 *     graph types.
 *   - Genuine images (the nD CLUT lattice) come back as Raster: the flattened,
 *     ICC-normalized CLUT samples plus geometry - the caller decides how to
 *     colour-manage/display them.
 *
 * Series carry a role: Primary (the profile's own data - primaries, white,
 * curve, named colours) vs Hint (reference geometry the caller may draw or
 * ignore - spectral locus, planckian curve, wavelength labels, chroma circles,
 * identity line). No ticks/grid are shipped: those are a caller-side decision;
 * only an axis range *hint* is provided.
 *
 * Design intent: this file depends ONLY on IccProfLib + spectralLocus.hpp + the
 * C++ standard library - no embind, no nlohmann, no PDF/TIFF writers - so it is
 * self-contained and portable between callers (CLI, browser/WASM, tests).
 */

#ifndef ICC_VIZ_MODEL_HPP
#define ICC_VIZ_MODEL_HPP

#include "IccDefs.h"           // icTagSignature, icColorSpaceSignature, and ICC header packing
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
  // SmoothnessLattice = 6,  // reserved - deferred to a later phase
  // InkReversalL     = 7,   // retired - per-channel L* tone-reversal scan (removed); value reserved
  NeutralAxisInking = 8,// device colorant along the neutral axis from a PCS->device LUT (graph)
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
  std::string id;          // "curve","identity","locus","planckian","chroma30",...
  std::string name;        // human/legend label
  Role  role  = Role::Primary;
  Shape shape = Shape::Polyline;
  std::string auxKind;     // "", "Lstar", "nm", "kelvin" - meaning of Vertex.aux
  std::string colorHint;   // optional: "R","G","B","white","neutral","locus"
  std::vector<Vertex> verts;
};

struct Axis {
  std::string label;       // "Input", "a*", "x (CIE 1931)"
  float minHint = 0.0f;    // suggested range only - caller may recompute/override
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

// A diagnostic raised while building a visualization - a granular skip/warn
// reason, carried here as DATA so a library caller (browser UI, CLI, test)
// decides how to surface them (log to stderr, show inline, ignore). Additive:
// `error` still holds the single fatal reason for the simple ok/error path,
// while `diagnostics` preserves per-failure detail and adds non-fatal warnings
// (e.g. tile-count overflow) that the simple path can't represent.
enum class Severity : unsigned char { Warning, Error };
struct Diagnostic { Severity severity = Severity::Error; std::string message; };

struct GraphResult  { bool ok = false; std::string error; std::vector<Diagnostic> diagnostics; Graph  graph;  };
struct RasterResult { bool ok = false; std::string error; std::vector<Diagnostic> diagnostics; Raster raster; };

// -- diagnostic output policy -------------------------------------------------
// Each render result ALWAYS carries its diagnostics as data (see Diagnostic). In
// ADDITION, the model echoes them to stderr - reproducing the top-level
// behaviour a command-line caller expects. A process-global switch controls
// this; it DEFAULTS to not-silent, so a caller that never touches it (and never
// passes a Verbosity) behaves exactly like the CLI.
//
// A library embedding the model where stderr is wrong (browser/WASM) calls
// SetSilent(true) once, naturally in its profile-supply / setup step. Any single
// render call can override the global by passing an explicit Verbosity.
enum class Verbosity {
  Default,   // consult the global SetSilent() switch (the polymorphic default)
  Stderr,    // force-echo this call's diagnostics to stderr
  Silent     // suppress this call's stderr, regardless of the global switch
};

void SetSilent(bool silent = true);                  // global; default state is not-silent
bool GetSilent();                                    // read that global silent switch
// Optional "<name>: " prefix prepended to each stderr line - the CLI sets the
// profile filename here so its output matches iccProfileVisualize byte-for-byte.
void SetDiagnosticContext(const std::string& name);

// List every visualization available for the profile, in a stable canonical
// order: chromaticity first, then in a fixed canonical signature order - TRC curves, then
// each LUT's A/B/M curves followed by its CLUT image, then a neutral-axis inking
// graph for each PCS->device BToA table, then named/colorant tables as a*b* then xy.
std::vector<Descriptor> Enumerate(CIccProfile* pIcc);

// RenderGraph / RenderRaster - render one graph or raster by descriptor id.
// Re-enumerates to find the descriptor (cheap; callers should cache the parsed
// profile). Diagnostics are echoed to stderr per the Verbosity (default -> the
// global SetSilent() switch).
GraphResult  RenderGraph (CIccProfile* pIcc, const std::string& id, Verbosity v = Verbosity::Default);  // graph kinds
RasterResult RenderRaster(CIccProfile* pIcc, const std::string& id, Verbosity v = Verbosity::Default);  // ClutImage raster

// -- Gamut volume -------------------------------------------------------------
// Volume (dE*ab^3) enclosed by a profile's gamut, measured by boundary
// voxelisation + flood-fill: sample the device-cube boundary (all facets) through
// the AToB transform -> L*a*b*, voxelise, dilate to seal sampling gaps, flood-fill
// the exterior, erode the dilation back, count enclosed voxels. A scalar
// metric - not a Graph/Raster - so it sits outside the Enumerate/Render path.
//
// Adapted from chardata's gamut-wasm `gamutVolumeIcc`; here the device->PCS step uses
// IccProfLib (CIccXform on `aToBTag`, device values 0..1, PCS->Lab decoded via
// icLabFromPcs / icXyzFromPcs+icXYZtoLab). Pick (tag, intent) to select the
// gamut: perceptual = AToB0/icPerceptual, relative = AToB1/icRelativeColorimetric,
// saturation = AToB2/icSaturation, absolute = AToB1/icAbsoluteColorimetric.
//
// `volume` is a discrete-voxel estimate (resolution = voxelSize). Check
// `degenerate`: when true the boundary sampling largely failed or collapsed, so
// the number is unreliable and a caller cannot tell "genuinely tiny gamut" from
// "sampling collapsed" - surface it as N/A rather than a real measurement.
struct GamutVolumeResult {
  bool        ok             = false;
  std::string error;
  double      volume         = 0.0;  // dE*ab^3 = enclosed voxels x voxelSize^3
  long long   voxels         = 0;
  int         samplesPerAxis = 0;    // device-cube boundary steps per free axis
  double      voxelSize      = 0.0;  // Lab grid cell edge (dE*ab)
  int         nColorants     = 0;    // device channels
  bool        degenerate     = false;// boundary collapsed / mostly non-finite: volume unreliable
};

// Compute the gamut volume for one device->PCS (AToB) tag at `intent`. Pass 0 for
// samplesPerAxis / voxelSize / dilate to auto-pick them from the colorant count.
GamutVolumeResult GamutVolume(CIccProfile* pIcc, icTagSignature aToBTag,
                              icRenderingIntent intent,
                              int samplesPerAxis = 0, double voxelSize = 0.0,
                              int dilate = 0);

// -- B2A round-trip accuracy ---------------------------------------------------
// Round-trip of Lab through the profile: seed in-gamut L*a*b* by sampling the
// device cube on an interior grid and pushing it through A2B, then run each
// Lab1 -> device (B2A) -> Lab2 (A2B) and report dE*ab(Lab1, Lab2). Measures how
// accurately the B2A (PCS->device) table inverts the A2B for that intent - a
// method suggested by Harold Boll. A scalar metric (outside the Enumerate/Render
// path).
//
// Why seed from the device cube rather than sampling L*a*b* directly: every seed
// is then the A2B image of a real device value, so the test points are wholly
// IN-GAMUT and the round trip measures genuine B2A/A2B agreement. A directly
// sampled L*a*b* grid would place many points outside the gamut, where B2A only
// clamps them and reports a large, meaningless dE. Walking the device interior on
// a regular grid also spreads the seeds reasonably evenly, in terms of spacing,
// through the interior of the in-gamut region of L*a*b* space.
//
// Needs matching AToB/BToA tags for `intent` (perceptual = A2B0/B2A0, relative &
// absolute = A2B1/B2A1, saturation = A2B2/B2A2); `intent` also drives the PCS
// white handling (relative vs absolute).
struct RoundTripResult {
  bool        ok         = false;
  std::string error;
  int         n          = 0;    // finite test points
  double      meanDE     = 0.0;  // dE*ab
  double      p90DE      = 0.0;
  double      maxDE      = 0.0;
  double      stdDE      = 0.0;
  int         nColorants = 0;    // device channels
};

// samplesPerAxis 0 -> auto-pick the device seed grid from the colorant count.
RoundTripResult RoundTripDE(CIccProfile* pIcc, icRenderingIntent intent,
                            int samplesPerAxis = 0);

} // namespace iccviz

#endif // ICC_VIZ_MODEL_HPP
