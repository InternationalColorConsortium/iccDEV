# IccProfilePlot / IccVizModel — data-first profile visualization

`IccVizModel` turns an ICC profile's plottable content into **data** instead of a
finished picture. Where [`IccProfileVisualize`](../IccProfileVisualize) renders a
profile into a single PDF (tone curves, chromaticity, named-colour scatters) plus
per-tag TIFFs, `IccVizModel` lets a caller **enumerate** the available
visualizations and **render each one individually**:

- **Graphs** (tone curves, chromaticity, named-colour scatters) come back as 2-D
  point series + axis range hints. No raster is produced — the caller draws and
  styles them however it likes.
- **Genuine images** (the nD CLUT lattice) come back as ICC-normalized raster
  samples + geometry. The caller decides how to colour-manage / display them.

It depends only on **IccProfLib** + `spectralLocus.hpp` — no PDF/TIFF/SVG
writers, no third-party libraries — so `IccVizModel.{hpp,cpp}` can be compiled
directly into any consumer (it is, for example, compiled into a WebAssembly
module by the [profiletool](https://github.com/colourbill-ctrl/profiletool) web
front-end). `iccProfileVisualize` is left untouched; the maths is reimplemented
here independently.

---

## Two ways to use it

### 1. The C++ library API (`IccVizModel.hpp`)

Everything lives in namespace `iccviz`. Parse a profile once, then enumerate and
render against it.

```cpp
#include "IccVizModel.hpp"
#include "IccProfile.h"

CIccProfile* p = OpenIccProfile("photo.icc");          // your own parse

for (const iccviz::Descriptor& d : iccviz::Enumerate(p)) {
  if (d.output == iccviz::Output::Graph) {
    iccviz::GraphResult g = iccviz::RenderGraph(p, d.id);
    // g.ok, g.graph.series → draw / restyle however you like
  } else {                                             // Output::Raster
    iccviz::RasterResult r = iccviz::RenderRaster(p, d.id);
    // r.ok, r.raster.samples + geometry → your own display path
  }
}
delete p;
```

#### The three calls

```cpp
std::vector<Descriptor> Enumerate    (CIccProfile* pIcc);
GraphResult             RenderGraph  (CIccProfile* pIcc, const std::string& id);
RasterResult            RenderRaster (CIccProfile* pIcc, const std::string& id);
```

`RenderGraph` / `RenderRaster` re-enumerate internally to resolve the `id`, so
callers that render many graphs should keep the parsed `CIccProfile` alive (the
parse is the expensive part, not enumeration).

#### The data model

```cpp
enum class Kind   { Curve1D=1, ChromaticityXY=2, NamedColorsAB=3,
                    NamedColorsXY=4, ClutImage=5 };   // stable, append-only
enum class Output { Graph, Raster };
enum class Role   { Primary, Hint };                  // profile data vs reference geometry
enum class Shape  { Polyline, ClosedPath, Scatter };

struct Vertex { float x, y; std::string label; float aux; };   // aux = NaN when none
struct Series {
  std::string id, name;        // "curve","identity","locus","planckian","chroma30",…
  Role  role;  Shape shape;
  std::string auxKind;         // "", "Lstar", "nm", "kelvin"  (meaning of Vertex.aux)
  std::string colorHint;       // "R","G","B","white","neutral","locus"  (optional)
  std::vector<Vertex> verts;
};
struct Axis  { std::string label; float minHint, maxHint; bool equalAspect; };
struct Graph { std::string title, description; Axis xAxis, yAxis;
               std::vector<Series> series; };

struct Raster {
  int  width, height, channels, bitsPerChannel;
  int  photometric;            // TIFF-style: 0/1 gray · 2 RGB · 5 CMYK · 8 CIELAB
  bool normalizedICC;          // samples are ICC-normalized (see note below)
  std::vector<unsigned char> samples;   // row-major, channels interleaved
};

struct Descriptor { Kind kind; Output output; std::string id, title;
                    icTagSignature tag; char grp; int idx; };
```

#### Contract

- **`id` is the stable handle.** Enumerate returns ids like `chroma:xy`,
  `curve:rTRC`, `curve:A2B0:B:1`, `clut:A2B0`, `named:ab:ncl2`. Titles may
  change; ids do not. Pass an `id` back to `RenderGraph`/`RenderRaster`.
- **Graphs are pure data.** Each plot is point series split into **Primary** (the
  profile's own values — primaries, white, gamut, the curve, named colours) and
  **Hint** (reference geometry you may draw or ignore — spectral locus, Planckian
  curve, wavelength labels, constant-chroma circles, the identity line). Only an
  **axis range hint** is provided — no ticks, no grid (those are caller choices).
- **Rasters stay rasters.** The nD CLUT lattice is returned as samples +
  geometry, never coerced into a graph.
- **No drawing dependency.** Want a PDF? Use `iccProfileVisualize`. Want your own
  charts? Use this.

#### What each `Kind` returns

| Kind | Output | Primary series | Hint series |
|---|---|---|---|
| `Curve1D` (TRC, and A/B/M curves of a LUT tag) | Graph | `curve` (input,output) sampled at `max(1000, tableSize)` | `identity` (0,0)→(1,1) |
| `ChromaticityXY` (RGB primaries + white) | Graph | `primaries` R/G/B + `white`; `gamut` triangle | `locus` (closed, with `locusLabels` aux=nm) + `planckian` (aux=kelvin) |
| `NamedColorsAB` (colorantTable / namedColor2) | Graph | `colors` (a\*,b\*), aux=L\* | constant-chroma circles + quadrant labels |
| `NamedColorsXY` (same tags) | Graph | `colors` (x,y), aux=L\* | `locus` + `planckian` |
| `ClutImage` (AToB\*/BToA\*/gamut/preview CLUTs) | Raster | — | — |

### 2. The CLI tool (`iccProfilePlot`)

A thin command-line wrapper, for scripting and inspection:

```
iccProfilePlot <profile.icc> list
iccProfilePlot <profile.icc> graph  <id>
iccProfilePlot <profile.icc> raster <id> [out.raw]
```

- **`list`** → JSON array of descriptors:
  ```json
  [ {"kind":2,"output":"graph","id":"chroma:xy","title":"Chromaticity xy"},
    {"kind":1,"output":"graph","id":"curve:rTRC","title":"rTRC"},
    {"kind":5,"output":"raster","id":"clut:A2B0","title":"A2B0 CLUT"} ]
  ```
- **`graph <id>`** → one graph as JSON:
  ```json
  {"title":"…","description":"…",
   "xAxis":{"label":"Input","min":0,"max":1,"equalAspect":false},
   "yAxis":{…},
   "series":[{"id":"curve","name":"…","role":"primary","shape":"polyline",
              "colorHint":"neutral","auxKind":"",
              "points":[x,y,x,y,…],
              "labels":[{"i":12,"t":"520","a":520}]}]}
  ```
  Geometry is a flat `points:[x,y,…]` array; labels are listed **sparsely**
  (`{i: vertexIndex, t: text, a: aux}`) so a 1000-point curve isn't bloated.
- **`raster <id> [out.raw]`** → JSON geometry; with `out.raw`, writes the
  row-major, channel-interleaved ICC-normalized samples.

---

## Important: raster sample encoding

`Raster.samples` are **ICC-normalized** (`normalizedICC == true`): each channel
is the profile's internal 0…1 value scaled to the integer range (8- or 16-bit,
per `bitsPerChannel`). For a CIELAB CLUT this means:

```
L* = (sample / maxVal) * 100
a* = (sample / maxVal) * 255 - 128
b* = (sample / maxVal) * 255 - 128
```

These are **not** the TIFF-standard *signed* CIELAB encoding. `iccProfileVisualize`
writes its TIFFs through `MiniTIFF::WriteTIFF`, whose `shiftTIFFLAB()` step
rewrites a\*/b\* into the signed convention (subtract half-range, i.e. flip the
top bit). `IccVizModel` does **not** do that — it hands back the raw
ICC-normalized samples. A consumer that expects signed-CIELAB input must apply
that shift itself; a consumer using the formulas above must not.

---

## Building

Built as part of the iccDEV CMake tree (registered in `Build/Cmake/CMakeLists.txt`
as `Tools/IccProfilePlot`). To reuse just the library in another project,
compile `IccVizModel.cpp` and add both this directory and
`Tools/CmdLine/IccProfileVisualize` (for `spectralLocus.hpp`) to the include
path; link against IccProfLib.
