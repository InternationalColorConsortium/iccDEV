// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
// Regression: the Mini{PDF,SVG,TIFF} serialization layer had no instrument
// pointed at it (#2116).
//
// iccProfileVisualizePlot is two layers. The data layer -- iccviz::Enumerate /
// RenderGraph / RenderRaster in IccVizModel -- already had coverage: two CTests
// (#1712) and the in-process CFL harness on ci-qa-issue-1975, whose raster
// observer reads result.raster.samples.size() and stops there. Everything below
// that boundary, which is where bytes are actually produced, ran only when the
// CLI ran: WriteTIFF's IFD layout and offset/strip arithmetic, PDFWriter's
// object graph and xref offset table, and SVGOut, which the tool never even
// constructs. The only way in was a function that derives every output path
// from the input path and writes a PDF plus N TIFFs per call, so no harness
// could reach it without a filesystem.
//
// #2116 added the seam: PDFWriter::Serialize(ostream&), SVGOut::Serialize
// (ostream&), and a WriteTIFF overload taking an already-open FILE*. This test
// is what proves the seam is worth having and that it did not change what the
// tool writes.
//
// Why the writers deserve the attention. The dimensions WriteTIFF computes file
// offsets from are profile-controlled: they come out of a CLUT via RenderRaster
// and reach the writer unmodified (iccProfileVisualize.cpp:1388). Inside, they
// are narrowed to uint32_t and turned into offsets.
//
// Being precise about what is and is not guarded there, because the loose
// version of this claim is wrong. MiniTIFF.cpp does have returning checks -- on
// ftell failure, on strip offsets exceeding UINT32_MAX, on a short fwrite, and
// on a non-finite or out-of-range dpi. What none of them cover is the incoming
// geometry: width, height, channels and depth are used to size rows and strips
// with no returning check at all, and the only guards on them are three
// assert()s (nrowBytes > 0, and the two UINT_MAX checks on the IFD offsets),
// which CMake's default NDEBUG in CMAKE_CXX_FLAGS_RELEASE compiles out --
// verified in this tree, not assumed. So the guarded quantities are the ones
// derived from the stream, and the unguarded ones are the ones derived from the
// profile.
//
// That is not a live bug, and this test does not claim one. The guards are in
// buildClutRaster (IccVizModel.cpp:819-834), which refuses non-positive
// geometry and caps the buffer at 256 MB, so every raster the engine can emit
// is already in range. The point is that the contract keeping WriteTIFF's
// unguarded arithmetic safe is enforced entirely by a *different file*, was
// nowhere written down as a test, and would break silently. Level 4 below is
// that contract, asserted against whatever the engine actually produces.
//
// Levels:
//   1. PDF   -- Serialize(ostream) and CloseFile() produce identical bytes.
//   2. SVG   -- same, and SVGOut's first coverage of any kind.
//   3. TIFF  -- the FILE* overload and the name-taking overload produce
//               identical bytes, driven by the real engine's raster.
//   4. The raster geometry contract WriteTIFF's arithmetic depends on.
//   5. The TIFF bytes are structurally what the geometry says they should be,
//      so level 3 cannot pass by two paths being identically wrong.
//
// Behaviour preservation was also measured outside this test, against the
// pre-split binary built from b5f8def1: iccProfileVisualizePlot on
// sRGB_v4_ICC_preference.icc produces five files (four .tif, one .pdf) and all
// five are byte-identical before and after the split, with identical stderr.
//
// Exit code 0 = pass, 1 = a case regressed, 2 = usage/setup error.

#include "IccProfile.h"
#include "IccVizModel.hpp"
#include "MiniPDF.hpp"
#include "MiniSVG.hpp"
#include "MiniTIFF.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

static int g_failures = 0;

static void fail(const char *what)
{
  std::printf("FAIL: %s\n", what);
  ++g_failures;
}

static void pass(const char *what)
{
  std::printf("ok: %s\n", what);
}

// Read a whole file back, always in binary. Returns false if it could not be
// opened or read; the caller reports, because "could not read what we just
// wrote" is a failure of the test's own plumbing and not of the writer under
// test.
static bool slurp(const std::string &path, std::vector<unsigned char> &out)
{
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f)
    return false;

  out.clear();
  unsigned char buf[4096];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0)
    out.insert(out.end(), buf, buf + got);

  bool ok = (std::ferror(f) == 0);
  std::fclose(f);
  return ok;
}

// Read back everything written to an open stream, from the beginning.
static bool slurpStream(FILE *f, std::vector<unsigned char> &out)
{
  if (std::fflush(f) != 0 || std::fseek(f, 0, SEEK_SET) != 0)
    return false;

  out.clear();
  unsigned char buf[4096];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof buf, f)) > 0)
    out.insert(out.end(), buf, buf + got);

  return std::ferror(f) == 0;
}

// Drop carriage returns so a CRLF file compares equal to the LF string the
// in-memory path produced.
//
// This is the one place platform differences reach this test, and it is not
// optional. PDFWriter::CloseFile writes through icOpenRegularWriteTextFile,
// i.e. fopen(name, "wt") (IccCmdLineUtil.h:240), and SVGOut::CloseFile uses a
// default-mode std::ofstream: both are TEXT streams, so a Windows CRT expands
// every '\n' to "\r\n" on the way out, while Serialize() writes to an
// ostringstream that expands nothing. Comparing raw bytes would report a size
// difference that is the platform's line-ending policy rather than a
// serialization defect -- measured on this test's own output, 1450 bytes on
// disk against 1371 in memory for the PDF -- and would red the Windows ctest
// legs while staying green everywhere else.
//
// Normalizing rather than reading the file back in text mode is deliberate.
// The text-mode round-trip is only conditionally guaranteed (C17 7.21.2p2
// excludes lines ending in a space, and this PDF's xref entries end in "n \n"),
// so relying on it would make the test depend on CRT behaviour the standard
// does not promise. Stripping '\r' depends on nothing. It is safe here because
// no writer emits a bare CR and this test's content is fixed ASCII; TIFF is
// binary on both sides and is compared strictly, without this.
static std::vector<unsigned char> withoutCR(const std::vector<unsigned char> &in)
{
  std::vector<unsigned char> out;
  out.reserve(in.size());
  for (unsigned char c : in) {
    if (c != '\r')
      out.push_back(c);
  }
  return out;
}

static void compareBytes(const char *what,
                         const std::vector<unsigned char> &streamPath,
                         const std::vector<unsigned char> &filePath)
{
  if (streamPath.size() != filePath.size()) {
    std::printf("  %s: %zu bytes via stream, %zu via file\n",
                what, streamPath.size(), filePath.size());
    fail(what);
    return;
  }
  if (streamPath.empty()) {
    std::printf("  %s: both paths produced nothing\n", what);
    fail(what);
    return;
  }
  for (size_t i = 0; i < streamPath.size(); ++i) {
    if (streamPath[i] != filePath[i]) {
      std::printf("  %s: first difference at byte %zu (0x%02x vs 0x%02x)\n",
                  what, i, streamPath[i], filePath[i]);
      fail(what);
      return;
    }
  }
  pass(what);
}

// -- level 1: PDF -------------------------------------------------------------
//
// Serialize() must not disturb the writer's state, because CloseFile() runs the
// same four Write* calls again over the same object list and the recorded
// object offsets have to come out the same. That is the whole reason object
// cleanup stayed in CloseFile rather than moving into Serialize.
static void checkPdf(const std::string &path)
{
  std::vector<unsigned char> viaStream, viaFile;

  {
    PDFWriter pdf;
    pdf.OpenFile(path, 8 * inch2point, 8 * inch2point);

    // Two pages sharing one named XObject, so the xref table, the page tree and
    // the XObject resource dictionary all carry more than one entry -- a
    // single-page document would let an offset-table bug stay invisible.
    std::string axes = "0 0 m 100 100 l S\n";
    pdf.AddXObject(Rect2D(0.0f, 200.0f, 0.0f, 200.0f), axes, "Axes");

    for (int page = 0; page < 2; ++page) {
      std::ostringstream commands;
      commands << "BT /F1 12 Tf 20 " << (700 - 20 * page) << " Td (page) Tj ET\n";
      pdf.AddObject(new PDFGraphic(commands.str()));
      pdf.AddPage(pdf.ObjectCount(), "Axes");
    }

    if (pdf.PageCount() != 2) {
      fail("pdf: two pages were added");
      return;
    }

    std::ostringstream out;
    out.exceptions(std::ios::badbit | std::ios::failbit);
    pdf.Serialize(out);
    const std::string &s = out.str();
    viaStream.assign(s.begin(), s.end());

    pdf.CloseFile();
  }

  if (!slurp(path, viaFile)) {
    fail("pdf: CloseFile output could not be read back");
    return;
  }

  // Line-ending-normalized: CloseFile wrote this through a text stream. See withoutCR().
  compareBytes("pdf: Serialize(ostream) matches CloseFile bytes",
               withoutCR(viaStream), withoutCR(viaFile));

  // A document, not just a matching pair of empty strings.
  if (viaFile.size() < 200 ||
      std::memcmp(viaFile.data(), "%PDF-1.7", 8) != 0)
    fail("pdf: output is a PDF document");
  else
    pass("pdf: output is a PDF document");
}

// -- level 2: SVG -------------------------------------------------------------
//
// SVGOut is dead in the shipping tool: iccProfileVisualize.cpp declares
// DrawAxisSVG and an SVG graph function taking SVGOut&, but never constructs
// one. So this is the class's first execution in any test, and the reason the
// seam covers it even though nothing calls it today.
//
// Serialize() runs Finalize() before writing and CloseFile() runs it again, so
// finalization has to be idempotent or the second serialization would carry an
// extra closing </g>. It is idempotent because closing the page group clears
// m_pageInProgress -- not because of any separate latch, which is worth stating
// because the obvious assertion here does not test it: comparing Serialize
// against CloseFile passes even with the guard removed, since by the time
// CloseFile runs the flag is already false either way.
//
// What does discriminate is serializing twice from the same writer, below: drop
// the m_pageInProgress guard and the second document gains a </g> the first
// does not have.
static void checkSvg(const std::string &path)
{
  std::vector<unsigned char> viaStream, viaFile, viaStreamAgain;

  {
    SVGOut svg;
    svg.OpenFile(path);
    svg.SetPageSize(200.0f, 200.0f);
    svg.NextPage();
    svg.StartGroup("curve");
    svg.AddLine(0.0f, 0.0f, 100.0f, 100.0f);
    svg.AddText(10.0f, 20.0f, "Input", 14.0f, "Helvetica", "Regular", "left");
    svg.EndGroup();

    // Deliberately left open: NextPage's group is closed by Finalize, not by
    // the caller, which is the state Serialize has to reproduce.
    std::ostringstream out;
    out.exceptions(std::ios::badbit | std::ios::failbit);
    svg.Serialize(out);
    const std::string &s = out.str();
    viaStream.assign(s.begin(), s.end());

    // Second serialization of the same writer: this is the case that fails if
    // finalization stops being idempotent.
    std::ostringstream again;
    again.exceptions(std::ios::badbit | std::ios::failbit);
    svg.Serialize(again);
    const std::string &s2 = again.str();
    viaStreamAgain.assign(s2.begin(), s2.end());

    svg.CloseFile();
  }

  if (!slurp(path, viaFile)) {
    fail("svg: CloseFile output could not be read back");
    return;
  }

  // Line-ending-normalized: CloseFile wrote this through a text stream. See withoutCR().
  compareBytes("svg: Serialize(ostream) matches CloseFile bytes",
               withoutCR(viaStream), withoutCR(viaFile));
  // Both sides are in-memory here, so this one stays a strict byte comparison.
  compareBytes("svg: repeated Serialize is idempotent", viaStreamAgain, viaStream);

  std::string text(viaFile.begin(), viaFile.end());
  // One closing tag for the StartGroup/EndGroup pair, one for the page group
  // Finalize closes. Three would mean finalization ran its body twice; this is
  // the absolute count behind the relative comparison above.
  size_t closes = 0;
  for (size_t at = text.find("</g>"); at != std::string::npos;
       at = text.find("</g>", at + 1))
    ++closes;

  if (closes != 2) {
    std::printf("  svg: %zu closing group tags, expected 2\n", closes);
    fail("svg: exactly one closing tag per opened group");
  }
  else {
    pass("svg: exactly one closing tag per opened group");
  }
}

// -- levels 3-5: TIFF, driven by the real engine ------------------------------

// Decode a host-order 32-bit field written by putLong.
static uint32_t readLong(const std::vector<unsigned char> &bytes, size_t at)
{
  uint32_t v = 0;
  std::memcpy(&v, bytes.data() + at, sizeof v);
  return v;
}

static uint16_t readShort(const std::vector<unsigned char> &bytes, size_t at)
{
  uint16_t v = 0;
  std::memcpy(&v, bytes.data() + at, sizeof v);
  return v;
}

// Find an IFD entry by tag and return its value field. The directory starts at
// offset 8 with a 2-byte entry count, then 12-byte entries.
static bool findIfdValue(const std::vector<unsigned char> &bytes,
                         uint16_t wantTag, uint32_t &value)
{
  if (bytes.size() < 10)
    return false;

  uint16_t entries = readShort(bytes, 8);
  for (uint16_t i = 0; i < entries; ++i) {
    size_t at = 10 + size_t(i) * 12;
    if (at + 12 > bytes.size())
      return false;
    if (readShort(bytes, at) == wantTag) {
      value = readLong(bytes, at + 8);
      return true;
    }
  }
  return false;
}

static void checkRaster(const iccviz::Raster &raster, const std::string &id,
                        const std::string &pathPrefix, int index)
{
  // Level 4: the geometry contract. WriteTIFF narrows these to uint32_t and
  // computes file offsets from them with no returning check, so every one of
  // these has to be guaranteed by the producer. buildClutRaster is what
  // guarantees them today; this is where that guarantee is written down.
  bool geometryOk = true;
  if (raster.width <= 0 || raster.height <= 0 || raster.channels <= 0) {
    std::printf("  raster %s: non-positive geometry %dx%d x%d\n",
                id.c_str(), raster.width, raster.height, raster.channels);
    geometryOk = false;
  }
  if (raster.bitsPerChannel != 8 && raster.bitsPerChannel != 16) {
    std::printf("  raster %s: bitsPerChannel %d is neither 8 nor 16\n",
                id.c_str(), raster.bitsPerChannel);
    geometryOk = false;
  }

  if (!geometryOk) {
    fail("tiff: raster geometry is in range for the writer's arithmetic");
    return;
  }

  // The sample buffer must be exactly what the geometry describes. WriteTIFF
  // reads width*channels*(depth/8) bytes per row for height rows with no
  // reference to the vector's actual length, so anything smaller is an
  // out-of-bounds read and anything larger means the geometry is lying.
  const size_t expectBytes = size_t(raster.width) * size_t(raster.height) *
                             size_t(raster.channels) *
                             size_t(raster.bitsPerChannel / 8);
  if (raster.samples.size() != expectBytes) {
    std::printf("  raster %s: %zu sample bytes, geometry describes %zu\n",
                id.c_str(), raster.samples.size(), expectBytes);
    fail("tiff: sample buffer matches the declared geometry");
    return;
  }

  // Level 3: same bytes through both overloads.
  //
  // Each call gets its own copy of the samples. WriteTIFF rewrites the caller's
  // buffer in place for CIELAB rasters (shiftTIFFLAB re-biases a* and b*), so
  // handing the same vector to both calls would compare a shifted buffer
  // against a twice-shifted one and report a difference that is the test's own
  // doing.
  std::vector<unsigned char> forStream(raster.samples);
  std::vector<unsigned char> forFile(raster.samples);

  const std::string tifPath = pathPrefix + "_" + std::to_string(index) + ".tif";

  // tmpfile() is deliberately not used: on Windows it creates the file in the
  // drive root and fails for a non-elevated process, which would turn this into
  // three red Windows legs. A named file opened "w+b" behaves the same
  // everywhere, and a freshly opened one satisfies the overload's precondition
  // that the stream be positioned at 0 (the layout constants are absolute from
  // the start of the file, so it refuses anything else).
  FILE *stream = std::fopen((tifPath + ".stream").c_str(), "w+b");
  if (!stream) {
    fail("tiff: could not open a scratch stream");
    return;
  }

  bool streamOk = WriteTIFF(stream, "in-memory", 100.0f, raster.photometric,
                            forStream.data(), size_t(raster.width),
                            size_t(raster.height), raster.channels,
                            raster.bitsPerChannel);

  std::vector<unsigned char> viaStream;
  bool readBack = streamOk && slurpStream(stream, viaStream);
  std::fclose(stream);

  if (!streamOk) {
    fail("tiff: FILE* overload wrote the raster");
    return;
  }
  if (!readBack) {
    fail("tiff: FILE* overload output could not be read back");
    return;
  }

  if (!WriteTIFF(tifPath, 100.0f, raster.photometric, forFile.data(),
                 size_t(raster.width), size_t(raster.height),
                 raster.channels, raster.bitsPerChannel)) {
    fail("tiff: name-taking overload wrote the raster");
    return;
  }

  std::vector<unsigned char> viaFile;
  if (!slurp(tifPath, viaFile)) {
    fail("tiff: name-taking overload output could not be read back");
    return;
  }

  compareBytes("tiff: FILE* overload matches the name-taking overload",
               viaStream, viaFile);

  // Level 5: the bytes are structurally what the geometry says. Without this,
  // two identically-wrong outputs would pass level 3 -- the failure mode a pure
  // A-equals-B pin cannot see.
  uint32_t tagWidth = 0, tagHeight = 0, tagSamples = 0, tagByteCount = 0;
  if (viaFile.size() < 10 ||
      !findIfdValue(viaFile, 256 /* TIFF_WIDTH */, tagWidth) ||
      !findIfdValue(viaFile, 257 /* TIFF_HEIGHT */, tagHeight) ||
      !findIfdValue(viaFile, 277 /* TIFF_SAMPLESPERPIXEL */, tagSamples) ||
      !findIfdValue(viaFile, 279 /* TIFF_STRIPBYTECOUNTS */, tagByteCount)) {
    fail("tiff: the required IFD entries are present");
    return;
  }

  if (tagWidth != uint32_t(raster.width) ||
      tagHeight != uint32_t(raster.height) ||
      tagSamples != uint32_t(raster.channels)) {
    std::printf("  raster %s: IFD says %ux%u x%u, raster says %dx%d x%d\n",
                id.c_str(), tagWidth, tagHeight, tagSamples,
                raster.width, raster.height, raster.channels);
    fail("tiff: IFD geometry matches the raster it came from");
    return;
  }

  // One strip covering the whole image, which is what this writer emits.
  if (tagByteCount != uint32_t(expectBytes)) {
    std::printf("  raster %s: strip byte count %u, expected %zu\n",
                id.c_str(), tagByteCount, expectBytes);
    fail("tiff: strip byte count matches the pixel data written");
    return;
  }

  pass("tiff: IFD geometry and strip byte count match the raster");
}

int main(int argc, char **argv)
{
  if (argc != 3) {
    std::printf("usage: iccviz-writer-serialization <profile.icc> <output-prefix>\n");
    return 2;
  }

  const std::string profilePath = argv[1];
  const std::string prefix = argv[2];

  checkPdf(prefix + ".pdf");
  checkSvg(prefix + ".svg");

  std::unique_ptr<CIccProfile> profile(ReadIccProfile(profilePath.c_str()));
  if (!profile) {
    std::printf("could not read profile: %s\n", profilePath.c_str());
    return 2;
  }

  // Silent: the engine echoes diagnostics to stderr by default to match the
  // CLI, and this test is not asserting on them.
  iccviz::SetSilent(true);

  // Counts rasters the engine handed over, NOT rasters that passed: a count of
  // verified rasters would drop to zero the moment a writer case failed, and
  // report "the engine produced nothing" for what is really a writer defect.
  int rastersRendered = 0;
  const std::vector<iccviz::Descriptor> descriptors = iccviz::Enumerate(profile.get());
  for (const auto &d : descriptors) {
    if (d.output != iccviz::Output::Raster)
      continue;
    iccviz::RasterResult res = iccviz::RenderRaster(profile.get(), d.id,
                                                    iccviz::Verbosity::Silent);
    if (!res.ok)
      continue;
    checkRaster(res.raster, d.id, prefix, rastersRendered);
    ++rastersRendered;
  }

  // The profile this runs on has four CLUT tags (A2B0/A2B1/B2A0/B2A1), so a
  // zero here means the engine stopped producing rasters and every TIFF case
  // above silently checked nothing -- the discarded-coverage failure mode,
  // where a green run means the test never ran rather than that it passed.
  if (rastersRendered == 0) {
    fail("tiff: the engine produced at least one raster to write");
  }
  else {
    std::printf("ok: drove the writer with %d engine raster(s)\n", rastersRendered);
  }

  if (g_failures) {
    std::printf("%d case(s) regressed\n", g_failures);
    return 1;
  }

  std::printf("all cases passed\n");
  return 0;
}
