/*
  File:     iccProfilePlot.cpp

  Contains: Console app that exposes IccVizModel — the data-first profile
            visualization API. Where iccProfileVisualize renders a single
            PDF + per-tag TIFFs, this lists the visualizations a profile
            supports and emits each one as DATA (graphs as 2-D point series +
            axis hints; the nD CLUT lattice as ICC-normalized raster samples),
            so a caller can draw/colour-manage them however it likes.

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

#include "IccVizModel.hpp"
#include "IccProfile.h"
#include "IccProfLibVer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// ── tiny hand-rolled JSON emitter (no third-party deps, like IccPawgReport) ──

static void jsonEscape(const std::string& s, std::string& out) {
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
        else out += ch;
    }
  }
}

static std::string jstr(const std::string& s) {
  std::string out = "\"";
  jsonEscape(s, out);
  out += "\"";
  return out;
}

static std::string jnum(double v) {
  if (std::isnan(v) || std::isinf(v)) return "null";
  char buf[32];
  std::snprintf(buf, sizeof buf, "%g", v);
  return std::string(buf);
}

static const char* outputStr(iccviz::Output o) {
  return o == iccviz::Output::Raster ? "raster" : "graph";
}
static const char* roleStr(iccviz::Role r) {
  return r == iccviz::Role::Hint ? "hint" : "primary";
}
static const char* shapeStr(iccviz::Shape s) {
  switch (s) {
    case iccviz::Shape::ClosedPath: return "closedPath";
    case iccviz::Shape::Scatter:    return "scatter";
    default:                        return "polyline";
  }
}

// ── emitters ─────────────────────────────────────────────────────────────────

static void printList(const std::vector<iccviz::Descriptor>& descs) {
  std::printf("[");
  for (size_t i = 0; i < descs.size(); ++i) {
    const auto& d = descs[i];
    std::printf("%s{\"kind\":%u,\"output\":\"%s\",\"id\":%s,\"title\":%s}",
                i ? "," : "",
                static_cast<unsigned>(d.kind), outputStr(d.output),
                jstr(d.id).c_str(), jstr(d.title).c_str());
  }
  std::printf("]\n");
}

static void printGraph(const iccviz::Graph& g) {
  std::string s = "{";
  s += "\"title\":" + jstr(g.title);
  s += ",\"description\":" + jstr(g.description);
  auto axis = [](const iccviz::Axis& a) {
    return std::string("{\"label\":") + jstr(a.label) +
           ",\"min\":" + jnum(a.minHint) + ",\"max\":" + jnum(a.maxHint) +
           ",\"equalAspect\":" + (a.equalAspect ? "true" : "false") + "}";
  };
  s += ",\"xAxis\":" + axis(g.xAxis);
  s += ",\"yAxis\":" + axis(g.yAxis);
  s += ",\"series\":[";
  for (size_t si = 0; si < g.series.size(); ++si) {
    const auto& ser = g.series[si];
    if (si) s += ",";
    s += "{\"id\":" + jstr(ser.id) + ",\"name\":" + jstr(ser.name);
    s += ",\"role\":\"" + std::string(roleStr(ser.role)) + "\"";
    s += ",\"shape\":\"" + std::string(shapeStr(ser.shape)) + "\"";
    s += ",\"colorHint\":" + jstr(ser.colorHint);
    s += ",\"auxKind\":" + jstr(ser.auxKind);
    s += ",\"points\":[";
    for (size_t i = 0; i < ser.verts.size(); ++i) {
      if (i) s += ",";
      s += jnum(ser.verts[i].x) + "," + jnum(ser.verts[i].y);
    }
    s += "],\"labels\":[";
    bool firstLabel = true;
    for (size_t i = 0; i < ser.verts.size(); ++i) {
      const auto& v = ser.verts[i];
      if (v.label.empty() && std::isnan(v.aux)) continue;
      if (!firstLabel) s += ",";
      firstLabel = false;
      s += "{\"i\":" + std::to_string(i);
      if (!v.label.empty()) s += ",\"t\":" + jstr(v.label);
      if (!std::isnan(v.aux)) s += ",\"a\":" + jnum(v.aux);
      s += "}";
    }
    s += "]}";
  }
  s += "]}";
  std::printf("%s\n", s.c_str());
}

static void printRaster(const iccviz::Raster& r, const char* outFile) {
  std::printf("{\"width\":%d,\"height\":%d,\"channels\":%d,\"bitsPerChannel\":%d,"
              "\"photometric\":%d,\"normalizedICC\":%s",
              r.width, r.height, r.channels, r.bitsPerChannel, r.photometric,
              r.normalizedICC ? "true" : "false");
  if (outFile) {
    FILE* f = std::fopen(outFile, "wb");
    if (f) {
      std::fwrite(r.samples.data(), 1, r.samples.size(), f);
      std::fclose(f);
      std::printf(",\"samplesFile\":%s,\"sampleBytes\":%zu",
                  jstr(outFile).c_str(), r.samples.size());
    } else {
      std::printf(",\"error\":\"could not open output file\"");
    }
  } else {
    std::printf(",\"sampleBytes\":%zu", r.samples.size());
  }
  std::printf("}\n");
}

static void usage() {
  std::printf(
    "Usage:\n"
    "  iccProfilePlot <profile.icc> list\n"
    "  iccProfilePlot <profile.icc> graph  <id>\n"
    "  iccProfilePlot <profile.icc> raster <id> [out.raw]\n"
    "\n"
    "  list   - JSON array of available visualizations {kind,output,id,title}\n"
    "  graph  - JSON for one graph (2-D point series + axis hints)\n"
    "  raster - JSON geometry for one CLUT image; with out.raw, writes the\n"
    "           row-major, channel-interleaved ICC-normalized samples.\n"
    "iccProfilePlot built with IccProfLib version " ICCPROFLIBVER "\n");
}

int main(int argc, char* argv[]) {
  if (argc < 3) { usage(); return argc < 2 ? 0 : 1; }

  const char* path = argv[1];
  const std::string cmd = argv[2];

  CIccProfile* pIcc = OpenIccProfile(path);
  if (!pIcc) {
    std::fprintf(stderr, "Unable to parse '%s' as an ICC profile.\n", path);
    return 2;
  }

  int rc = 0;
  if (cmd == "list") {
    printList(iccviz::Enumerate(pIcc));
  } else if (cmd == "graph" && argc >= 4) {
    auto res = iccviz::RenderGraph(pIcc, argv[3]);
    if (!res.ok) { std::fprintf(stderr, "%s\n", res.error.c_str()); rc = 3; }
    else printGraph(res.graph);
  } else if (cmd == "raster" && argc >= 4) {
    auto res = iccviz::RenderRaster(pIcc, argv[3]);
    if (!res.ok) { std::fprintf(stderr, "%s\n", res.error.c_str()); rc = 3; }
    else printRaster(res.raster, argc >= 5 ? argv[4] : nullptr);
  } else {
    usage();
    rc = 1;
  }

  delete pIcc;
  return rc;
}
