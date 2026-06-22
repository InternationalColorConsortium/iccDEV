/*
  File:     IccVizModel.hpp

  Contains: Data-first profile visualization model — public API (see the header doc-comment for usage: pick a Kind, supply a CIccProfile, call Enumerate/RenderGraph/RenderRaster).

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
 * IccVizModel — data-first profile visualization model.
 *
 * Computes a profile's visualizations — tone curves, the CIE 1931 chromaticity
 * chart, named/colorant a*b* and xy scatters, and the nD CLUT lattice — and
 * *returns the underlying data* rather than finished drawings:
 *
 *   - Graph visualizations (tone curves, chromaticity, named-colour scatters)
 *     come back as Graph: 2-D point series plus axis range hints, so the caller
 *     draws/rasterizes them in its own look-and-feel. NO raster is produced for
 *     graph types.
 *   - Genuine images (the nD CLUT lattice) come back as Raster: the flattened,
 *     ICC-normalized CLUT samples plus geometry — the caller decides how to
 *     colour-manage/display them.
 *
 * Series carry a role: Primary (the profile's own data — primaries, white,
 * curve, named colours) vs Hint (reference geometry the caller may draw or
 * ignore — spectral locus, planckian curve, wavelength labels, chroma circles,
 * identity line). No ticks/grid are shipped: those are a caller-side decision;
 * only an axis range *hint* is provided.
 *
 * ── HOW TO USE THIS API ──────────────────────────────────────────────────────
 *
 *   1. Open a profile into a CIccProfile* (e.g. OpenIccProfile("foo.icc")).
 *      Everything here takes that pointer; nothing is mutated, so one parsed
 *      profile can drive many calls.
 *
 *   2. Call Enumerate(pIcc) to discover what the profile can show. It returns a
 *      vector<Descriptor>, one per available visualization, in a stable order.
 *      Each Descriptor tells you:
 *        - kind   : which visualization (see enum Kind — Curve1D, ChromaticityXY,
 *                   NamedColorsAB, NamedColorsXY, ClutImage). Use this to pick a
 *                   chart/lens, or to drive a menu.
 *        - output : Output::Graph (vector data) or Output::Raster (an image).
 *        - id     : the opaque handle you pass back to render this one item.
 *        - title  : a human label; tag/grp/idx address the source object.
 *
 *   3. Render the item you want, branching on Descriptor::output:
 *        - Output::Graph  -> RenderGraph (pIcc, descriptor.id) -> GraphResult.
 *          GraphResult.graph is a Graph: a list of Series (each a polyline /
 *          closed path / scatter of Vertex{x,y,label,aux}) plus x/y Axis hints.
 *          Draw the Primary series as the data; draw or skip the Hint series
 *          (locus, planckian, chroma circles) as your UI prefers.
 *        - Output::Raster -> RenderRaster(pIcc, descriptor.id) -> RasterResult.
 *          RasterResult.raster is a Raster: width/height/channels/bitsPerChannel,
 *          a photometric hint, and the row-major ICC-normalized samples. Colour-
 *          manage / convert to your display space yourself.
 *
 *   4. Check ok. On failure, error holds the reason and diagnostics carries the
 *      granular skip/warning detail (the same text iccProfileVisualize prints).
 *      By default those are also echoed to stderr; see SetSilent() / Verbosity
 *      below to suppress or redirect that for library use.
 *
 *   A caller that wants a finished report (e.g. a PDF) walks the Enumerate list,
 *   renders each descriptor, and draws the returned data with its own toolkit —
 *   see iccProfilePlot.cpp for a worked, commented example.
 *
 * Design intent: this file depends ONLY on IccProfLib + spectralLocus.hpp + the
 * C++ standard library — no embind, no nlohmann, no PDF/TIFF writers — so it is
 * self-contained and portable between callers (CLI, browser/WASM, tests).
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

// A diagnostic raised while building a visualization — a granular skip/warn
// reason, carried here as DATA so a library caller (browser UI, CLI, test)
// decides how to surface them (log to stderr, show inline, ignore). Additive:
// `error` still holds the single fatal reason for the simple ok/error path,
// while `diagnostics` preserves per-failure detail and adds non-fatal warnings
// (e.g. tile-count overflow) that the simple path can't represent.
enum class Severity : unsigned char { Warning, Error };
struct Diagnostic { Severity severity = Severity::Error; std::string message; };

struct GraphResult  { bool ok = false; std::string error; std::vector<Diagnostic> diagnostics; Graph  graph;  };
struct RasterResult { bool ok = false; std::string error; std::vector<Diagnostic> diagnostics; Raster raster; };

// ── diagnostic output policy ─────────────────────────────────────────────────
// Each render result ALWAYS carries its diagnostics as data (see Diagnostic). In
// ADDITION, the model echoes them to stderr — reproducing the top-level
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
bool GetSilent();
// Optional "<name>: " prefix prepended to each stderr line — the CLI sets the
// profile filename here so its output matches iccProfileVisualize byte-for-byte.
void SetDiagnosticContext(const std::string& name);

// Ordering of Enumerate's result.
enum class Order : unsigned char {
  Canonical,   // fixed, deterministic signature order (default; cross-TU/WASM-safe)
  TagTable     // approximate the profile's tag-table (directory) order, by tag offset
};

// List every visualization available for the profile.
//
// Order::Canonical (default) returns a fixed, deterministic order: chromaticity
// first, then TRC curves, then each LUT's A/B/M curves followed by its CLUT
// image, then named/colorant tables as a*b* then xy. It probes a fixed signature
// list (never iterates the profile's tag list), so it is safe to call from a
// module compiled separately from IccProfLib (e.g. WASM).
//
// Order::TagTable reorders that same set to follow the profile's tag-table order
// — what iccProfileVisualize produced by walking its tag directory — so a report
// generator can match that page sequence. It does so WITHOUT the cross-TU tag-list
// iteration: it sorts by each owning tag's byte offset (via the in-library-safe
// GetTag()), with chromaticity pinned first. Offset order matches directory order
// for essentially all real profiles; it is an approximation only in the rare case
// of a tag whose directory position disagrees with its offset.
std::vector<Descriptor> Enumerate(CIccProfile* pIcc, Order order = Order::Canonical);

// Render one graph / raster by descriptor id. Re-enumerates to find the
// descriptor (cheap; callers should cache the parsed profile). Diagnostics are
// echoed to stderr per the Verbosity (default → the global SetSilent() switch).
GraphResult  RenderGraph (CIccProfile* pIcc, const std::string& id, Verbosity v = Verbosity::Default);
RasterResult RenderRaster(CIccProfile* pIcc, const std::string& id, Verbosity v = Verbosity::Default);

} // namespace iccviz

#endif // ICC_VIZ_MODEL_HPP
