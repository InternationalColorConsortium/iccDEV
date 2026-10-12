/*
    File:       IccApplyTelemetry.h

    Contains:   Shared telemetry helpers for apply command line tools

    Version:    V1

    Copyright:  (c) see below
*/

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

#ifndef ICCAPPLYTELEMETRY_H
#define ICCAPPLYTELEMETRY_H

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "IccUtil.h"

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

inline FILE* icOpenNewRegularWriteTextFile(const char* path)
{
  if (!path || !path[0]) {
    errno = EINVAL;
    return nullptr;
  }

#if defined(_WIN32)
  const int fd = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_TEXT,
                       _S_IREAD | _S_IWRITE);
  if (fd < 0)
    return nullptr;

  FILE* file = _fdopen(fd, "wt");
  if (!file)
    _close(fd);
  return file;
#else
  const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL,
                      S_IRUSR | S_IWUSR);
  if (fd < 0)
    return nullptr;

  struct stat fileStatus;
  if (fstat(fd, &fileStatus) || !S_ISREG(fileStatus.st_mode)) {
    close(fd);
    errno = EINVAL;
    return nullptr;
  }

  FILE* file = fdopen(fd, "wt");
  if (!file)
    close(fd);
  return file;
#endif
}

enum icApplyTelemetryMode {
  icApplyTelemetryOff,
  icApplyTelemetryHuman,
  icApplyTelemetryJsonl
};

class CIccApplyToolTelemetry {
public:
  CIccApplyToolTelemetry()
    : m_mode(icApplyTelemetryOff), m_file(nullptr), m_sequence(0),
      m_startedEvent(false), m_terminalEvent(false) {}

  ~CIccApplyToolTelemetry()
  {
    if (m_file && m_startedEvent && !m_terminalEvent) {
      try {
        Emit("run_failed", "\"status\":\"tool_failure\",\"reason\":\"early_exit\"");
      }
      catch (...) {
        // An allocation failure may also prevent writing the final receipt.
      }
    }
    if (m_file)
      fclose(m_file);
  }

  bool Open(const std::string& path)
  {
    m_file = OpenNewFile(path, "Telemetry file");
    return m_file != nullptr;
  }

  bool Emit(const char* event, const std::string& fields)
  {
    if (m_mode != icApplyTelemetryJsonl)
      return true;

    const std::string text =
      "{\"schema\":\"iccdev-apply-telemetry/v1\",\"event\":" + JsonString(event) +
      ",\"sequence\":" + std::to_string(++m_sequence) +
      ",\"timestamp_utc\":" + JsonString(TimestampUtc()) +
      ",\"monotonic_elapsed_ms\":" + std::to_string(ElapsedMs()) +
      (fields.empty() ? "" : "," + fields) + "}\n";

    if (fwrite(text.data(), 1, text.size(), m_file) != text.size() || fflush(m_file)) {
      fprintf(stderr, "Unable to write telemetry event '%s'\n", event);
      return false;
    }
    if (!strcmp(event, "run_started"))
      m_startedEvent = true;
    else if (!strcmp(event, "run_completed") || !strcmp(event, "run_failed"))
      m_terminalEvent = true;
    return true;
  }

  bool WriteEvidence(const std::string& path, const std::string& fields) const
  {
    FILE* file = OpenNewFile(path, "Evidence file");
    if (!file)
      return false;

    const std::string text =
      "{\"schema\":\"iccdev-apply-evidence/v1\"," + fields + "}\n";
    const bool wrote = fwrite(text.data(), 1, text.size(), file) == text.size();
    const bool flushed = !fflush(file);
    const bool closed = !fclose(file);
    const bool ok = wrote && flushed && closed;
    if (!ok) {
      fprintf(stderr, "Unable to write evidence file '%s'\n",
              icSanitizeConsoleText(path.c_str()).c_str());
    }
    return ok;
  }

  long long ElapsedMs() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - m_started).count();
  }

  static std::string JsonString(const std::string& value)
  {
    return "\"" + icJsonEscape(value.c_str()) + "\"";
  }

  icApplyTelemetryMode m_mode;

  static std::string TimestampUtc()
  {
    const std::time_t now = std::time(nullptr);
    struct tm utc;
#if defined(_WIN32)
    if (gmtime_s(&utc, &now))
      return std::string();
#else
    if (!gmtime_r(&now, &utc))
      return std::string();
#endif
    char text[32];
    if (!strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc))
      return std::string();
    return text;
  }

private:
  static FILE* OpenNewFile(const std::string& path, const char* label)
  {
    if (path.empty())
      return nullptr;

    FILE* file = icOpenNewRegularWriteTextFile(path.c_str());
    if (!file) {
      if (errno == EEXIST) {
        fprintf(stderr, "%s already exists: '%s'\n", label,
                icSanitizeConsoleText(path.c_str()).c_str());
      }
      else {
        fprintf(stderr, "Unable to create %s '%s'\n", label,
                icSanitizeConsoleText(path.c_str()).c_str());
      }
    }
    return file;
  }

  FILE* m_file;
  unsigned long long m_sequence;
  bool m_startedEvent;
  bool m_terminalEvent;
  std::chrono::steady_clock::time_point m_started = std::chrono::steady_clock::now();
};

#endif
