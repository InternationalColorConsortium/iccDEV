/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ICC_TIFF_TELEMETRY_H
#define ICC_TIFF_TELEMETRY_H

#include <cstdio>
#include <cstring>

#ifndef ICCDEV_TELEMETRY
inline bool icRejectTiffTelemetryOption(const char* option)
{
  if (!strcmp(option, "--telemetry") || !strncmp(option, "--telemetry=", 12) ||
      !strcmp(option, "--telemetry-file")) {
    fprintf(stderr, "Telemetry is not built; configure ICCDEV_ENABLE_TELEMETRY=ON\n");
    return true;
  }
  return false;
}
#else
#include <cmath>
#include <vector>
#include "IccApplyTelemetry.h"
#include "IccSameFile.h"

class CIccTiffTelemetry {
public:
  CIccTiffTelemetry() : m_receipt("iccdev-tiff-telemetry/v1") {}

  ~CIccTiffTelemetry()
  {
    if (m_started && !m_terminal)
      Emit("run_failed", "\"status\":\"tool_failure\",\"stage\":" +
           JsonString(m_stage));
  }

  static void Help(FILE* stream)
  {
    fprintf(stream, "  --telemetry=off|human|jsonl --telemetry-file FILE\n");
    fprintf(stream, "  Human telemetry uses stderr; JSONL requires a new receipt file.\n");
  }

  int ParseOption(int& arg, int argc, char** argv)
  {
    const char* option = argv[arg];
    if (!strcmp(option, "--telemetry") || !strncmp(option, "--telemetry=", 12)) {
      if (m_modeSpecified) {
        fprintf(stderr, "Duplicate --telemetry option\n");
        return -1;
      }
      m_modeSpecified = true;
      const char* mode = option + 12;
      if (!strcmp(option, "--telemetry")) {
        if (arg + 1 >= argc) {
          fprintf(stderr, "Missing mode for --telemetry\n");
          return -1;
        }
        mode = argv[++arg];
      }
      if (!strcmp(mode, "off"))
        m_receipt.m_mode = icApplyTelemetryOff;
      else if (!strcmp(mode, "human"))
        m_receipt.m_mode = icApplyTelemetryHuman;
      else if (!strcmp(mode, "jsonl"))
        m_receipt.m_mode = icApplyTelemetryJsonl;
      else {
        fprintf(stderr, "Invalid --telemetry mode: expected off, human, or jsonl\n");
        return -1;
      }
      return 1;
    }
    if (!strcmp(option, "--telemetry-file")) {
      if (m_fileSpecified || arg + 1 >= argc || !argv[arg + 1][0] ||
          !strncmp(argv[arg + 1], "--", 2)) {
        fprintf(stderr, "Duplicate or missing --telemetry-file path\n");
        return -1;
      }
      m_fileSpecified = true;
      m_path = argv[++arg];
      return 1;
    }
    return 0;
  }

  bool Active() const { return m_receipt.m_mode != icApplyTelemetryOff; }

  bool Start(const char* tool, const std::vector<std::string>& protectedPaths,
             const std::string& fields = "")
  {
    if ((m_receipt.m_mode == icApplyTelemetryJsonl) != m_fileSpecified) {
      fprintf(stderr, "--telemetry=jsonl requires --telemetry-file, and vice versa\n");
      return false;
    }
    if (!Active())
      return true;
    m_tool = tool;
    if (m_fileSpecified) {
      for (const std::string& path : protectedPaths) {
        if (icOutputPathsAlias(m_path.c_str(), path.c_str())) {
          fprintf(stderr, "Telemetry file must not alias an input or output\n");
          return false;
        }
      }
      if (!m_receipt.Open(m_path))
        return false;
      for (const std::string& path : protectedPaths) {
        if (icOutputPathsAlias(m_path.c_str(), path.c_str())) {
          fprintf(stderr, "Telemetry file aliases an input or output\n");
          return false;
        }
      }
    }
    m_startTime = Now();
    m_started = true;
    return Emit("run_started", "\"tool\":" + JsonString(tool) +
                (fields.empty() ? "" : "," + fields));
  }

  void SetStage(const char* stage) { m_stage = stage; }

  bool Emit(const char* event, const std::string& fields)
  {
    if (!Active())
      return true;
    bool ok = true;
    if (m_receipt.m_mode == icApplyTelemetryJsonl)
      ok = m_receipt.Emit(event, fields);
    else {
      ok = fprintf(stderr, "[%s] %s %s elapsed_ms=%s {%s}\n",
                   CIccApplyToolTelemetry::TimestampUtc().c_str(), m_tool, event,
                   Number(Since(m_startTime)).c_str(), fields.c_str()) >= 0;
      if (fflush(stderr))
        ok = false;
    }
    if (!ok) {
      fprintf(stderr, "Unable to write TIFF telemetry event\n");
      return false;
    }
    if (!strcmp(event, "run_completed") || !strcmp(event, "run_failed"))
      m_terminal = true;
    return true;
  }

  bool Finish(const std::string& fields)
  {
    if (!Active())
      return true;
    return Emit("run_completed", "\"status\":\"success\",\"elapsed_ms\":" +
                Number(Since(m_startTime)) + (fields.empty() ? "" : "," + fields));
  }

  static double Now()
  {
    return std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  static double Since(double start) { return Now() - start; }

  static std::string Number(double value)
  {
    if (!std::isfinite(value))
      return "null";
    char text[64];
    snprintf(text, sizeof(text), "%.9g", value);
    return text;
  }

  static std::string JsonString(const std::string& text)
  {
    return CIccApplyToolTelemetry::JsonString(text);
  }

private:
  CIccApplyToolTelemetry m_receipt;
  std::string m_path;
  const char* m_tool = "TIFF";
  const char* m_stage = "initialization";
  double m_startTime = 0.0;
  bool m_modeSpecified = false;
  bool m_fileSpecified = false;
  bool m_started = false;
  bool m_terminal = false;
};
#endif
#endif
