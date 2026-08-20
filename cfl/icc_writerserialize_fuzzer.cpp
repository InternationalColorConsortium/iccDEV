/*
 * Copyright (c) 2026 International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING
 * MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file
    LibFuzzer harness for the iccDEV profile visualization *serialization* layer.

    Companion to icc_profilevisualize_fuzzer.cpp, which drives the same
    Enumerate/RenderGraph/RenderRaster data API but stops at the data boundary:
    its observers read result.raster.samples.size() and never serialize, so
    nothing downstream of the model executes.  This harness carries the render
    results the last step into Mini{TIFF,PDF,SVG} -- the IFD layout and
    offset/strip arithmetic, the PDF object graph and xref offset table, and
    SVGOut -- which is the byte-generating layer #2116 named as uninstrumented.

    Two targets rather than one on purpose: it keeps a crash attributable to one
    layer (with the caveat at the end of this comment), and the data-API harness
    keeps its existing signal and corpus without paying for serialization on
    every iteration.

    == Why this harness does not validate the raster geometry ==

    The regression CTest for the same seam (.github/ci/regression/
    iccviz-writer-serialization.cpp) checks that width/height/channels/
    bitsPerChannel are in range and that samples.size() matches, and fails the
    test when they are not.  That is right for a test: it pins the contract that
    the *producer* is supposed to hold.

    A fuzzer must do the opposite.  WriteTIFF narrows the caller's geometry into
    uint32_t file offsets with no returning check, and the values come from a
    profile-supplied CLUT by way of iccviz::buildClutRaster.  The entire
    question this harness exists to answer is whether some profile can drive
    buildClutRaster to produce geometry that breaks that arithmetic.  Screening
    the geometry before the call would remove exactly the inputs worth finding.
    So render results go into the writers unmodified, and ASan/UBSan is the
    oracle.

    Because only profile-derived values reach the writers, any report is
    reachable from a real input by construction -- no need to argue afterwards
    whether a synthesized width could ever occur.  Nothing here fabricates
    geometry, and it should stay that way: a harness that made up dimensions
    would be testing a contract these writers do not hold (see the note on the
    WriteTIFF overload in MiniTIFF.hpp) and would report unreachable defects.

    Two limits on that, both worth knowing before reading a report from here.

    First, the asserts are LIVE in this build.  cfl/build.sh compiles at -O1 and
    never defines NDEBUG, so MiniTIFF's assert(nrowBytes > 0) and its two
    <= UINT_MAX asserts still fire.  An out-of-contract geometry therefore
    aborts at the assert and ends the session BEFORE reaching the offset
    arithmetic this target is pointed at.  That is a real signal, but it is not
    the same failure this file argues is worth hunting -- the unguarded
    narrowing only becomes reachable in a build that compiles the asserts out.

    Second, attribution is not absolute.  raster.samples.data() is nullptr for
    an empty-but-ok raster, and every raster this corpus produces is
    photometric 8 (CIELAB), so WriteTIFF calls shiftTIFFLAB(nullptr, ...) before
    any bounds-limited fwrite.  That report lands in MiniTIFF.cpp while the
    actual contract violation is upstream in iccviz::buildClutRaster.  A crash
    here is *usually* the writers; check the raster's geometry before concluding
    it.

    == The corpus and max_len decide whether this target does anything ==

    Only a profile carrying a CLUT reaches RenderRaster, and therefore the TIFF
    writer.  libFuzzer truncates every input to max_len, so a max_len below the
    size of a CLUT-bearing profile makes the whole raster half of this harness
    dead while it still reports runs, coverage and a clean exit.

    That is not hypothetical, and the corpus is narrower than it looks.  The CFL
    seed set is the eleven files in .github/ci/test-data, and exactly one of them
    reaches the writers: fuzz-clut-hbo-69197729.icc, 61,015 bytes -- measured by
    running each seed through this harness with a trap inside the WriteTIFF
    FILE* overload.  That single seed is also the only one large enough to have
    been hit by BOTH gates before #2120: build.sh's max_seed_bytes deleted it
    outright at the old 49152 default, and max_len=8192 truncated it.  They fail
    at different thresholds -- truncating the seed to 49,152 bytes still renders
    a raster, truncating to 8,192 does not -- and either one alone leaves this
    target reporting runs, coverage and a clean exit with the writers never
    executed.

    Both are now raised, and dropping a committed seed is a hard error rather
    than a silent rm, so this cannot regress quietly; see #2120.

    So: keep max_len above the size of the CLUT seeds, keep the seed cap above
    them too, and check that the corpus still contains one.  The same caution
    applies to any future target that reaches the raster path.

    == Hermetic by construction ==

    No filesystem access on any path.
      * TIFF  -- the FILE* overload added by #2116, over an fmemopen() stream.
      * PDF   -- PDFWriter::Serialize(ostream).  OpenFile() is still called, but
                 it opens nothing: it names the document and builds the six
                 preliminary objects (root, outline, page parent, procset, font,
                 group) that AddPage/AddObject need.  Passing an empty name
                 keeps ~PDFWriter's CloseFile() from writing anything while
                 still letting it free the object list.
      * SVG   -- SVGOut::Serialize(ostream).  OpenFile() is skipped entirely;
                 it only resets state the constructor already set.
 */

#include "IccProfile.h"
#include "IccVizModel.hpp"

#include "MiniPDF.hpp"
#include "MiniSVG.hpp"
#include "MiniTIFF.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Input bounds, matched to icc_profilevisualize_fuzzer.cpp so the two targets
// can share a corpus: 132 is the ICC header plus tag count.
static constexpr size_t kMinIccInputSize = 132;
static constexpr size_t kMaxIccInputSize = 5 * 1024 * 1024;
static constexpr size_t kMaxProfileTags  = 200;
static constexpr size_t kMaxDescriptors  = 256;

// Resource guards, NOT correctness guards -- see the note below on what they
// can hide.
//
// kTiffSinkBytes bounds the TIFF sink.  This is the one number that lets the
// geometry stay unscreened: fmemopen gives a seekable stream over a fixed
// buffer, so the writer's arithmetic runs in full and only the bytes are
// bounded.  A write past the end fails the fwrite, which WriteTIFF already
// checks and reports by returning false, so an oversized raster ends the
// iteration through the writer's own error path rather than filling a disk.
// Sized against the rasters this corpus actually produces (~26 KB), not against
// the worst case: the buffer is zero-filled on every raster descriptor of every
// input, so an oversized sink is a per-exec memset tax. Measured at 8 MiB it
// cost ~30% throughput (160 vs 208 exec/s) for no extra coverage. A raster that
// overruns this ends through WriteTIFF's own short-fwrite check, which is a
// legitimate outcome rather than a lost finding.
static constexpr size_t kTiffSinkBytes = 1024u * 1024u;

// kMaxVerts bounds how much of a graph is turned into PDF/SVG drawing
// commands.  Both writers accumulate into memory (PDF into its object list,
// SVG into m_buf), so an unbounded series would be an OOM rather than a
// finding.
static constexpr size_t kMaxVerts = 4096;

// ---------------------------------------------------------------------------
// Raster -> MiniTIFF
//
// The render result is handed over exactly as produced.  Note the samples
// buffer is deliberately passed by non-const pointer into the writer: WriteTIFF
// re-biases a* and b* in place for CIELAB rasters (shiftTIFFLAB), and since the
// RasterResult is discarded at the end of the iteration there is nothing to
// protect from that mutation.  The CTest copies the buffer per call only
// because it calls the writer twice and compares the bytes.
static size_t serializeRaster(iccviz::RasterResult &result)
{
  size_t observations = result.error.size() + result.diagnostics.size();
  if (!result.ok)
    return observations;

  iccviz::Raster &raster = result.raster;

  std::vector<char> sink(kTiffSinkBytes);
  FILE *stream = fmemopen(sink.data(), sink.size(), "w+b");
  if (!stream)
    return observations;

  // No screening of raster.width / height / channels / bitsPerChannel, and no
  // check that samples.size() agrees with them.  A short buffer here is an
  // out-of-bounds read inside WriteTIFF, which is the defect class this target
  // is pointed at; ASan reports it.
  const bool wrote = WriteTIFF(stream, "fuzz", 100.0f, raster.photometric,
                               raster.samples.data(),
                               static_cast<size_t>(raster.width),
                               static_cast<size_t>(raster.height),
                               raster.channels, raster.bitsPerChannel);

  // NOT a measure of how much was written. WriteTIFF's last act is an fseek
  // back to the STRIPBYTECOUNTS entry followed by a 12-byte putIFDLong, so for
  // the single-strip case this lands at a fixed offset -- measured at 118 for
  // both a 26,520-byte raster and a 48-byte one. It is fed back only as a
  // stable success token; do not read it as a size.
  if (wrote)
    observations += 1;

  fclose(stream);
  return observations;
}

// ---------------------------------------------------------------------------
// Graph -> MiniPDF and MiniSVG
//
// The profile-derived values that reach the byte layer are the vertex
// coordinates and the title/description/axis/series strings, so both documents
// are built from those rather than from constants.  The drawing itself is
// deliberately crude -- this is not trying to reproduce the CLI's charts, only
// to push profile-controlled floats and text through the writers' escaping,
// object numbering and offset accounting.
static size_t serializeGraph(const iccviz::GraphResult &result)
{
  size_t observations = result.error.size() + result.diagnostics.size();
  if (!result.ok)
    return observations;

  const iccviz::Graph &graph = result.graph;

  // Build one content stream from the series vertices, shared by both writers.
  std::ostringstream path;
  size_t emitted = 0;
  for (const auto &series : graph.series) {
    for (const auto &vertex : series.verts) {
      if (emitted >= kMaxVerts)
        break;
      path << vertex.x << " " << vertex.y << (emitted == 0 ? " m\n" : " l\n");
      ++emitted;
    }
    if (emitted >= kMaxVerts)
      break;
  }
  path << "S\n";

  {
    PDFWriter pdf;
    // Empty name: builds the object graph without ever naming a file, so the
    // destructor's CloseFile() frees the objects and writes nothing.
    pdf.OpenFile("", 8 * inch2point, 8 * inch2point);

    std::string content = path.str();
    pdf.AddXObject(Rect2D(0.0f, 200.0f, 0.0f, 200.0f), content, "Series");

    // Two pages so the xref table and page tree carry more than one entry --
    // a single-page document leaves offset-table arithmetic under-exercised.
    for (int page = 0; page < 2; ++page) {
      std::ostringstream commands;
      commands << "BT /F1 12 Tf 20 " << (700 - 20 * page) << " Td ("
               << graph.title << " " << graph.xAxis.label << " "
               << graph.yAxis.label << ") Tj ET\n";
      pdf.AddObject(new PDFGraphic(commands.str()));
      pdf.AddPage(pdf.ObjectCount(), "Series");
    }

    std::ostringstream out;
    pdf.Serialize(out);
    observations += out.str().size();
  }

  {
    SVGOut svg;
    svg.SetPageSize(200.0f, 200.0f);
    svg.NextPage();
    svg.StartGroup(graph.title);

    size_t drawn = 0;
    for (const auto &series : graph.series) {
      for (size_t i = 1; i < series.verts.size() && drawn < kMaxVerts; ++i, ++drawn)
        svg.AddLine(series.verts[i - 1].x, series.verts[i - 1].y,
                    series.verts[i].x, series.verts[i].y);
      if (drawn >= kMaxVerts)
        break;
    }

    // Profile-derived text straight into the document body. Note there is no
    // escaping to exercise: AddText emits `text` verbatim between <text> tags,
    // StartGroup emits `name` verbatim into an id="..." attribute, and the PDF
    // branch above drops the title into a ( ... ) Tj literal without escaping
    // '(', ')' or '\'. A profile whose desc carries '<', '"' or ')' therefore
    // produces a malformed document rather than a crash -- which an ASan/UBSan
    // oracle cannot see. Driving it here is still worth it for the buffer
    // arithmetic, but well-formedness is NOT covered by this target.
    svg.AddText(10.0f, 20.0f, graph.description, 14.0f,
                "Helvetica", "Regular", "left");
    svg.EndGroup();

    std::ostringstream out;
    svg.Serialize(out);
    observations += out.str().size();
  }

  return observations;
}

// ---------------------------------------------------------------------------

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || size < kMinIccInputSize || size > kMaxIccInputSize)
    return 0;

  std::unique_ptr<CIccProfile> profile(
      OpenIccProfile(const_cast<icUInt8Number *>(data), size));
  if (!profile || profile->m_Tags.size() > kMaxProfileTags)
    return 0;

  // The library's own quiet switch, not a stderr redirect: ASan and UBSan
  // reports share that stream, and silencing it would hide the oracle.
  iccviz::SetSilent(true);

  const std::vector<iccviz::Descriptor> descriptors =
      iccviz::Enumerate(profile.get());
  if (descriptors.size() > kMaxDescriptors)
    return 0;

  size_t observations = descriptors.size();
  for (const auto &descriptor : descriptors) {
    if (descriptor.output == iccviz::Output::Graph) {
      observations += serializeGraph(
          iccviz::RenderGraph(profile.get(), descriptor.id,
                              iccviz::Verbosity::Silent));
    } else {
      iccviz::RasterResult raster =
          iccviz::RenderRaster(profile.get(), descriptor.id,
                               iccviz::Verbosity::Silent);
      observations += serializeRaster(raster);
    }
  }

  volatile size_t resultSink = observations;
  (void)resultSink;
  return 0;
}
