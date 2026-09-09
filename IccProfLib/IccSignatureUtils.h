/** @file
File:       IccSignatureUtils.h

Contains:   Implementation of definitions and macros for ICC signature logging.

Version:    V2

Copyright:  (c) see Software License
*/

/*
* Copyright (c) International Color Consortium.
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

////////////////////////////////////////////////////////////////////////
// Contributor: David H Hoyt LLC
// FILE: IccSignatureUtils.h
//
// DESCRIPTION:
//   Utility definitions and macros for ICC signature logging and 
//   debug tracing. This header provides standardized macros for 
//   warning, info, and debug output, including file and line context.
//
//   Logging is routed through wxWidgets if USE_WX_LOGGING is defined.
//   Otherwise, logs are printed to stderr using standard C I/O.
//
//   On Linux platforms, optional backtrace support via execinfo.h
//   is provided through the TRACE_CALLER macro for low-cost diagnostics.
//
// HISTORY:
//   - Initial implementation by David Hoyt on 01-MAR-2025 at 1800 EST.
//   - Fix UB found with libFuzzer 2026-03-06 03:17:14 UTC DHOYT
//   - V2: Add v5/iccMAX N-channel + MCS support, gate debug output
//     behind ICC_SIGNATURE_VERBOSE, add MCH A-F (10-15 channels).
//     19-MAR-2026 DHOYT
//
////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef _ICC_SIGNATURE_UTILS_H
#define _ICC_SIGNATURE_UTILS_H

#include "IccDefs.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#ifdef ICC_AVX2_CLUT_DEBUG
  #include <chrono>
#endif
#ifdef ICC_PERF_MONITORING
  #include <atomic>
  #include <chrono>
  #include <cstdlib>
  #include <cstring>
#endif


#ifndef icSigSpectralPcsData
#define icSigSpectralPcsData ((icColorSpaceSignature)0x73706320)  // 'spc '
#endif

inline bool IsSpaceSpectralPCS(icColorSpaceSignature sig)
{
  return sig == icSigSpectralPcsData;
}

// -----------------------------------------------------------------------------
// LOGGING MACROS
//
// -----------------------------------------------------------------------------
// LOGGING MACROS & DIAGNOSTIC WRAPPERS
//
// If USE_WX_LOGGING is defined, use wxWidgets logging facilities.
// Otherwise, fallback to standard stderr output. All logs include
// [file:line] for traceability. Supports both variadic and explicit labels.
// -----------------------------------------------------------------------------

#ifdef USE_WX_LOGGING
  #include <wx/log.h>
  #define ICC_LOG_WARNING(...) \
    do { wxLogWarning("[%s:%d] ", __FILE__, __LINE__); wxLogWarning(__VA_ARGS__); } while(0)
  #define ICC_LOG_INFO(...) \
    do { wxLogMessage("[%s:%d] ", __FILE__, __LINE__); wxLogMessage(__VA_ARGS__); } while(0)
  #define ICC_LOG_DEBUG(...) \
    do { wxLogVerbose("[%s:%d] ", __FILE__, __LINE__); wxLogVerbose(__VA_ARGS__); } while(0)
  #define ICC_LOG_ERROR(...) \
    do { wxLogError("[%s:%d] ", __FILE__, __LINE__); wxLogError(__VA_ARGS__); } while(0)
#else
  #include <cstdio>
  #define ICC_LOG_WARNING(...) \
    do { fprintf(stderr, "ICC_WARN: [%s:%d] ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
  #define ICC_LOG_INFO(...) \
    do { fprintf(stderr, "ICC_INFO: [%s:%d] ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
  #define ICC_LOG_DEBUG(...) \
    do { fprintf(stderr, "ICC_DEBUG: [%s:%d] ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
  #define ICC_LOG_ERROR(...) \
    do { fprintf(stderr, "ICC_ERROR: [%s:%d] ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while(0)
#endif

// -----------------------------------------------------------------------------
// FLOAT LOGGER (Safe, Typed)
//
// ICC_SAFE_FLOAT_LOG(label, val_expr)
//   Logs the float value (expression), assumes it is not a pointer.
// -----------------------------------------------------------------------------

#ifndef ICC_SAFE_FLOAT_LOG
#define ICC_SAFE_FLOAT_LOG(label, val_expr) \
  do { ICC_LOG_DEBUG(label, val_expr); } while(0)
#endif

// -----------------------------------------------------------------------------
// CALLER BACKTRACE TRACING (LINUX ONLY)
//
// TRACE_CALLER()       - emits anonymous stack trace (max 10 frames)
// TRACE_LOG(msg, ...)  - logs message and trace together
// -----------------------------------------------------------------------------

// backtrace()/backtrace_symbols_fd() and the <execinfo.h> that declares them are a
// glibc extension, not a Linux one. musl does not ship the header at all, so on
// Alpine and other musl targets __linux__ is defined and the include below fails
// outright -- a hard compile error rather than a missing feature. Probe for the
// header instead of inferring it from the kernel, and fall through to the same
// no-op definitions the non-Linux branch already provides when it is absent.
//
// This is preventive rather than a fix for an observed break: no translation unit in
// the tree includes IccSignatureUtils.h today, and TRACE_CALLER/TRACE_LOG have no
// callers, so nothing currently compiles the include. It becomes a real musl build
// failure the moment anything does -- which is exactly what happens on ci-qa-flags,
// where IccUtil.cpp picks the header up.
//
// __has_include is standard from C++17, which this project requires; the
// defined(__has_include) test costs nothing and keeps the header usable if it is ever
// pulled into a C or pre-C++17 consumer.
#if defined(__linux__) && defined(__has_include)
  #if __has_include(<execinfo.h>)
    #define ICC_HAS_EXECINFO 1
  #endif
#endif

#ifdef ICC_HAS_EXECINFO
  #include <execinfo.h>
  #define TRACE_CALLER() \
    do { \
      void* bt_buffer[10]; \
      int bt_size = backtrace(bt_buffer, 10); \
      backtrace_symbols_fd(bt_buffer, bt_size, fileno(stderr)); \
    } while(0)

  #define TRACE_LOG(msg, ...) \
    do { \
      ICC_LOG_WARNING(msg, ##__VA_ARGS__); \
      TRACE_CALLER(); \
    } while(0)
#else
  #define TRACE_CALLER() ((void)0)
  #define TRACE_LOG(msg, ...) ICC_LOG_WARNING(msg, ##__VA_ARGS__)
#endif

// -----------------------------------------------------------------------------
// INTERNAL RUNTIME CHECK - Soft Contract Enforcement
//
// ICC_ASSERT(expr, desc)
//   If 'expr' is false, logs description, file, line, backtrace (Linux), then traps.
// -----------------------------------------------------------------------------

#ifdef ICC_ENABLE_ASSERTS
  #define ICC_ASSERT(expr, desc) \
    do { \
      if (!(expr)) { \
        ICC_LOG_WARNING("ASSERT FAILED: %s", desc); \
        ICC_LOG_WARNING("  File: %s", __FILE__); \
        ICC_LOG_WARNING("  Line: %d", __LINE__); \
        TRACE_CALLER(); \
        __builtin_trap(); \
      } \
    } while(0)
#else
  #define ICC_ASSERT(expr, desc) ((void)0)
#endif

// -----------------------------------------------------------------------------
// SAFE VALUE LOGGER - Guarded Memory Access Diagnostic
//
// ICC_LOG_SAFE_VAL(name, idx, basePtr, limit)
//   Logs value only if idx is valid, else logs warning.
// -----------------------------------------------------------------------------

#ifdef ICC_LOG_SAFE
  #define ICC_LOG_SAFE_VAL(name, idx, basePtr, limit) \
    do { \
      if ((idx) >= 0 && (idx) < (limit)) { \
        ICC_LOG_DEBUG("  val[%s][%d] = %.6f", name, idx, (basePtr)[(idx)]); \
      } else { \
        ICC_LOG_WARNING("  val[%s] index %d out of bounds (limit=%d)", name, idx, limit); \
        TRACE_CALLER(); \
      } \
    } while(0)
#else
  #define ICC_LOG_SAFE_VAL(name, idx, basePtr, limit) ((void)0)
#endif

#ifdef ICC_PERF_MONITORING
enum IccPerfClutPath {
  icPerfClutScalar,
  icPerfClutSse2,
  icPerfClutAvx2,
  icPerfClutAvx512,
  icPerfClutPathCount
};

struct IccPerfStats {
  std::atomic<unsigned long long> clutCalls[icPerfClutPathCount] = {};
  std::atomic<unsigned long long> clutOutputChannels[17] = {};
  std::atomic<unsigned long long> clutElapsedNanoseconds = {};
  std::atomic<unsigned long long> threadedCalls = {};
  std::atomic<unsigned long long> threadedPixels = {};
  std::atomic<unsigned long long> threadedWorkerStrips = {};
  std::atomic<unsigned long long> threadedActiveWorkers = {};
};

inline IccPerfStats g_iccPerfStats;

inline bool IccPerfMonitoringEnabled()
{
  static const bool enabled = []() {
    const char *value = std::getenv("ICC_PERF_STATS_FILE");
    return value && value[0] && std::strcmp(value, "0");
  }();
  return enabled;
}

inline void IccPerfWriteReport()
{
  const char *path = std::getenv("ICC_PERF_STATS_FILE");
  if (!path || !path[0])
    return;

  FILE *stream = std::fopen(path, "a");
  if (!stream) {
    ICC_LOG_WARNING("Unable to write ICC performance report: %s", path);
    return;
  }

  std::fprintf(stream, "format=iccdev-perf-v1\n");
  std::fprintf(stream, "clut_elapsed_ns=%llu\n",
               g_iccPerfStats.clutElapsedNanoseconds.load(std::memory_order_relaxed));
  for (int pathIndex = 0; pathIndex < icPerfClutPathCount; pathIndex++) {
    static const char *const pathNames[] = {"scalar", "sse2", "avx2", "avx512"};
    std::fprintf(stream, "clut_calls_%s=%llu\n", pathNames[pathIndex],
                 g_iccPerfStats.clutCalls[pathIndex].load(std::memory_order_relaxed));
  }
  for (int outputChannels = 0; outputChannels <= 16; outputChannels++) {
    const unsigned long long calls =
      g_iccPerfStats.clutOutputChannels[outputChannels].load(std::memory_order_relaxed);
    if (calls)
      std::fprintf(stream, "clut_calls_outputs_%d=%llu\n", outputChannels, calls);
  }
  std::fprintf(stream, "threaded_calls=%llu\n",
               g_iccPerfStats.threadedCalls.load(std::memory_order_relaxed));
  std::fprintf(stream, "threaded_pixels=%llu\n",
               g_iccPerfStats.threadedPixels.load(std::memory_order_relaxed));
  std::fprintf(stream, "threaded_worker_strips=%llu\n",
               g_iccPerfStats.threadedWorkerStrips.load(std::memory_order_relaxed));
  std::fprintf(stream, "threaded_active_workers=%llu\n",
               g_iccPerfStats.threadedActiveWorkers.load(std::memory_order_relaxed));
  std::fclose(stream);
}

class IccPerfReportWriter {
public:
  ~IccPerfReportWriter()
  {
    IccPerfWriteReport();
  }
};

inline void IccPerfEnsureReportWriter()
{
  static IccPerfReportWriter writer;
  (void)writer;
}

class IccPerfClutScope {
public:
  explicit IccPerfClutScope(int outputChannels)
    : m_outputChannels(outputChannels), m_path(icPerfClutScalar),
      m_enabled(IccPerfMonitoringEnabled())
  {
    if (m_enabled) {
      IccPerfEnsureReportWriter();
      m_start = std::chrono::steady_clock::now();
    }
  }

  ~IccPerfClutScope()
  {
    if (!m_enabled)
      return;

    const unsigned long long elapsedNanoseconds =
      static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_start).count());
    g_iccPerfStats.clutCalls[m_path].fetch_add(1, std::memory_order_relaxed);
    if (m_outputChannels >= 0 && m_outputChannels <= 16)
      g_iccPerfStats.clutOutputChannels[m_outputChannels].fetch_add(
        1, std::memory_order_relaxed);
    g_iccPerfStats.clutElapsedNanoseconds.fetch_add(
      elapsedNanoseconds, std::memory_order_relaxed);
  }

  void SetPath(IccPerfClutPath path)
  {
    m_path = path;
  }

private:
  int m_outputChannels;
  IccPerfClutPath m_path;
  bool m_enabled;
  std::chrono::steady_clock::time_point m_start;
};

inline void IccPerfRecordThreadedCmm(icUInt32Number pixels, int activeWorkers)
{
  if (!IccPerfMonitoringEnabled())
    return;

  IccPerfEnsureReportWriter();
  g_iccPerfStats.threadedCalls.fetch_add(1, std::memory_order_relaxed);
  g_iccPerfStats.threadedPixels.fetch_add(pixels, std::memory_order_relaxed);
  g_iccPerfStats.threadedWorkerStrips.fetch_add(
    static_cast<unsigned long long>(activeWorkers - 1), std::memory_order_relaxed);
  g_iccPerfStats.threadedActiveWorkers.fetch_add(
    static_cast<unsigned long long>(activeWorkers), std::memory_order_relaxed);
}

#define ICC_PERF_CLUT_SCOPE(outputChannels) \
  IccPerfClutScope iccPerfClutScope(outputChannels)
#define ICC_PERF_CLUT_PATH(path) iccPerfClutScope.SetPath(path)
#define ICC_PERF_THREADED_CMM(pixels, activeWorkers) \
  IccPerfRecordThreadedCmm(pixels, activeWorkers)
#else
#define ICC_PERF_CLUT_SCOPE(outputChannels) ((void)0)
#define ICC_PERF_CLUT_PATH(path) ((void)0)
#define ICC_PERF_THREADED_CMM(pixels, activeWorkers) ((void)0)
#endif

  // -----------------------------------------------------------------------------
  // AVX2 CLUT DEBUG TRACEPOINTS
  //
  // Define ICC_AVX2_CLUT_DEBUG to log dispatch decisions and per-kernel timing.
  // IccTraceAvx2ClutDispatch() and IccTraceAvx2ClutKernel() are intentional
  // source-level breakpoints for inspecting the CLUT's offsets and weights.
  // The helpers and all call sites compile out in normal builds.
  // -----------------------------------------------------------------------------

  #ifdef ICC_AVX2_CLUT_DEBUG
  struct IccAvx2ClutTraceData {
    bool cpuSupportsAvx2;
    bool selected;
    int outputChannels;
    const void *data;
    const icUInt32Number *offsets;
    const icFloatNumber *weights;
  };

  inline void IccTraceAvx2ClutDispatch(const IccAvx2ClutTraceData &trace)
  {
    ICC_LOG_INFO(
      "AVX2 CLUT dispatch: selected=%d cpu=%d outputs=%d data=%p offsets=[%u,%u,%u,%u,%u,%u,%u,%u]",
      trace.selected, trace.cpuSupportsAvx2, trace.outputChannels, trace.data,
      trace.offsets ? trace.offsets[0] : 0, trace.offsets ? trace.offsets[1] : 0,
      trace.offsets ? trace.offsets[2] : 0, trace.offsets ? trace.offsets[3] : 0,
      trace.offsets ? trace.offsets[4] : 0, trace.offsets ? trace.offsets[5] : 0,
      trace.offsets ? trace.offsets[6] : 0, trace.offsets ? trace.offsets[7] : 0);
  }

  inline void IccTraceAvx2ClutKernel(const IccAvx2ClutTraceData &trace,
                                     long long elapsedNanoseconds)
  {
    const int vectorOutputs = (trace.outputChannels / 8) * 8;
    const int maskedOutputs = trace.outputChannels - vectorOutputs;
    ICC_LOG_INFO(
      "AVX2 CLUT kernel: outputs=%d vector_outputs=%d masked_outputs=%d elapsed_ns=%lld weights=[%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g]",
      trace.outputChannels, vectorOutputs, maskedOutputs,
      elapsedNanoseconds, trace.weights[0], trace.weights[1], trace.weights[2],
      trace.weights[3], trace.weights[4], trace.weights[5], trace.weights[6],
      trace.weights[7]);
  }

  class IccAvx2ClutTimer {
  public:
    explicit IccAvx2ClutTimer(const IccAvx2ClutTraceData &trace)
      : m_trace(trace), m_start(std::chrono::steady_clock::now())
    {
    }

    ~IccAvx2ClutTimer()
    {
      const long long elapsedNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - m_start).count();
      IccTraceAvx2ClutKernel(m_trace, elapsedNanoseconds);
    }

  private:
    const IccAvx2ClutTraceData &m_trace;
    std::chrono::steady_clock::time_point m_start;
  };

  #define ICC_AVX2_CLUT_TRACE_DISPATCH(cpuSupportsAvx2, selected, outputChannels, data, offsets, weights) \
    do { \
      const IccAvx2ClutTraceData trace = {cpuSupportsAvx2, selected, outputChannels, data, offsets, weights}; \
      IccTraceAvx2ClutDispatch(trace); \
    } while(0)

  #define ICC_AVX2_CLUT_TRACE_KERNEL(outputChannels, data, offsets, weights) \
    const IccAvx2ClutTraceData iccAvx2ClutTrace = {true, true, outputChannels, data, offsets, weights}; \
    IccAvx2ClutTimer iccAvx2ClutTimer(iccAvx2ClutTrace)
  #else
  #define ICC_AVX2_CLUT_TRACE_DISPATCH(cpuSupportsAvx2, selected, outputChannels, data, offsets, weights) ((void)0)
  #define ICC_AVX2_CLUT_TRACE_KERNEL(outputChannels, data, offsets, weights) ((void)0)
  #endif


  ///////////////////////////////////////////////////////////////////////////////
  // FUNCTION: ColorSpaceSignatureToStr
//
// PURPOSE:
//   Converts a 32-bit ICC color space signature (icUInt32Number) into a
//   human-readable string literal. Useful for debugging and logging.
//
// PARAMETERS:
//   sig - A 32-bit ICC color space signature.
//
// RETURNS:
//   A string literal representing the known ICC color space. If the signature
//   is unrecognized, returns "Unknown".
//
// NOTES:
//   Supports v2/v4 classic signatures (XYZ through MCH9) plus MCH A-F
//   (10-15 channels) and v5/iccMAX N-channel (0x6e63xxxx) and MCS
//   (0x6d63xxxx) dynamic signatures.
//
// EXAMPLE:
//   const char* name = ColorSpaceSignatureToStr(0x52474220); // returns "RGB"
///////////////////////////////////////////////////////////////////////////////
inline const char* ColorSpaceSignatureToStr(icUInt32Number sig)
{
  switch (sig) {
    case (icUInt32Number)icSigXYZData:    return "XYZ";
    case (icUInt32Number)icSigLabData:    return "Lab";
    case (icUInt32Number)icSigRgbData:    return "RGB";
    case (icUInt32Number)icSigCmykData:   return "CMYK";
    case (icUInt32Number)icSigGrayData:   return "Gray";
    case (icUInt32Number)icSigNamedData:  return "Named";
    case (icUInt32Number)icSigMCH1Data:   return "MCH1";
    case (icUInt32Number)icSigMCH2Data:   return "MCH2";
    case (icUInt32Number)icSigMCH3Data:   return "MCH3";
    case (icUInt32Number)icSigMCH4Data:   return "MCH4";
    case (icUInt32Number)icSigMCH5Data:   return "MCH5";
    case (icUInt32Number)icSigMCH6Data:   return "MCH6";
    case (icUInt32Number)icSigMCH7Data:   return "MCH7";
    case (icUInt32Number)icSigMCH8Data:   return "MCH8";
    case (icUInt32Number)icSigMCH9Data:   return "MCH9";
    case (icUInt32Number)icSigMCHAData:   return "MCHA";
    case (icUInt32Number)icSigMCHBData:   return "MCHB";
    case (icUInt32Number)icSigMCHCData:   return "MCHC";
    case (icUInt32Number)icSigMCHDData:   return "MCHD";
    case (icUInt32Number)icSigMCHEData:   return "MCHE";
    case (icUInt32Number)icSigMCHFData:   return "MCHF";
    default: break;
  }

  // v5/iccMAX: N-channel (0x6e63xxxx) — channels in low 16 bits
  icUInt32Number csType = sig & 0xffff0000;
  icUInt32Number nChan  = sig & 0x0000ffff;
  if (csType == 0x6e630000 && nChan > 0)
    return "NChannel";

  // v5/iccMAX: MCS (0x6d63xxxx) — multiplex channel set
  if (csType == 0x6d630000 && nChan > 0)
    return "MCS";

  return "Unknown";
}


///////////////////////////////////////////////////////////////////////////////
// FUNCTION: IsValidColorSpaceSignature
//
// PURPOSE:
//   Validates whether the provided ICC color space signature is recognized
//   per the ICC specification.
//
// PARAMETERS:
//   sig - A 32-bit ICC color space signature (icUInt32Number).
//
// RETURNS:
//   true  - If the signature matches a known ICC color space.
//   false - Otherwise.
//
// DIAGNOSTIC:
//   Debug and warning output is gated behind ICC_SIGNATURE_VERBOSE.
//   Callers that need silent validation (analyzers, fuzzers) should
//   NOT define ICC_SIGNATURE_VERBOSE.
//
// EXAMPLE:
//   if (!IsValidColorSpaceSignature(tag->m_ColorSpace)) { ... }
//
// HISTORY:
//   Refactored and instrumented by David Hoyt on 01-MAR-2025.
//   V2: Gate logging behind ICC_SIGNATURE_VERBOSE, add v5 N-channel +
//   MCS support, add MCH A-F (10-15 channels). 19-MAR-2026 DHOYT.
///////////////////////////////////////////////////////////////////////////////
inline bool IsValidColorSpaceSignature(icUInt32Number sig)
{
#ifdef ICC_SIGNATURE_VERBOSE
  ICC_LOG_DEBUG("IsValidColorSpaceSignature(): input = 0x%08x (%s)", sig, ColorSpaceSignatureToStr(sig));
#endif

  switch (sig) {
    case (icUInt32Number)icSigXYZData:
    case (icUInt32Number)icSigLabData:
    case (icUInt32Number)icSigRgbData:
    case (icUInt32Number)icSigCmykData:
    case (icUInt32Number)icSigGrayData:
    case (icUInt32Number)icSigNamedData:
    case (icUInt32Number)icSigMCH1Data:
    case (icUInt32Number)icSigMCH2Data:
    case (icUInt32Number)icSigMCH3Data:
    case (icUInt32Number)icSigMCH4Data:
    case (icUInt32Number)icSigMCH5Data:
    case (icUInt32Number)icSigMCH6Data:
    case (icUInt32Number)icSigMCH7Data:
    case (icUInt32Number)icSigMCH8Data:
    case (icUInt32Number)icSigMCH9Data:
    case (icUInt32Number)icSigMCHAData:
    case (icUInt32Number)icSigMCHBData:
    case (icUInt32Number)icSigMCHCData:
    case (icUInt32Number)icSigMCHDData:
    case (icUInt32Number)icSigMCHEData:
    case (icUInt32Number)icSigMCHFData:
      return true;

    default:
      break;
  }

  // v5/iccMAX: N-channel dynamic signatures (0x6e63xxxx)
  icUInt32Number csType = sig & 0xffff0000;
  icUInt32Number nChan  = sig & 0x0000ffff;
  if (csType == 0x6e630000 && nChan > 0 && nChan <= 0xFFFF)
    return true;

  // v5/iccMAX: MCS dynamic signatures (0x6d63xxxx)
  if (csType == 0x6d630000 && nChan > 0 && nChan <= 0xFFFF)
    return true;

#ifdef ICC_SIGNATURE_VERBOSE
  ICC_LOG_WARNING("ColorSpace signature: 0x%08x (%s)",
                  sig, ColorSpaceSignatureToStr(sig));
#endif
  return false;
}


///////////////////////////////////////////////////////////////////////////////
// FUNCTION: IsValidTechnologySignature
//
// PURPOSE:
//   Validates if the provided ICC technology signature corresponds to a known
//   output device type (such as printers, monitors, or cameras).
//
// PARAMETERS:
//   sig - A 32-bit ICC technology signature (icUInt32Number).
//
// RETURNS:
//   true  - If the signature is part of the standard ICC technology enum.
//   false - If the signature is unrecognized.
//
// DIAGNOSTIC:
//   Debug and warning output is gated behind ICC_SIGNATURE_VERBOSE.
//
// NOTE:
//   A technology signature appears in exactly two places: the top-level
//   technologyTag ('tech', a signatureType), and the `technology` field of each
//   icProfileDescStruct entry in profileSequenceDescTag ('pseq'). Use this
//   predicate for those. It is not carried by profileDescriptionTag ('desc'),
//   which is a multiLocalizedUnicode, and there is no "outputDevice" block --
//   this NOTE named both until #2368, contradicting the CAVEAT below.
//
//   CAVEAT: it rejects icSigUndefined (0), which CIccTagProfileSeqDesc::Validate
//   accepts as "technology not defined" and which is a common real-world value --
//   iccFromCube writes it (Tools/CmdLine/IccFromCube/iccFromCube.cpp:624). Gating a
//   profileSequenceDesc entry on this predicate alone therefore rejects legitimate
//   profiles; test for zero separately. It matches CIccTagSignature::Validate, where a
//   technologyTag holding zero is genuinely non-compliant (#2101).
//
// HISTORY:
//   Instrumented and standardized by David Hoyt on 01-MAR-2025.
//   V2: Gate logging behind ICC_SIGNATURE_VERBOSE. 19-MAR-2026 DHOYT.
///////////////////////////////////////////////////////////////////////////////
inline bool IsValidTechnologySignature(icUInt32Number sig)
{
#ifdef ICC_SIGNATURE_VERBOSE
  ICC_LOG_DEBUG("IsValidTechnologySignature(): input = 0x%08x", sig);
#endif

  switch (sig) {
    case (icUInt32Number)icSigDigitalCamera:
    case (icUInt32Number)icSigFilmScanner:
    case (icUInt32Number)icSigReflectiveScanner:
    case (icUInt32Number)icSigInkJetPrinter:
    case (icUInt32Number)icSigThermalWaxPrinter:
    case (icUInt32Number)icSigElectrophotographicPrinter:
    case (icUInt32Number)icSigElectrostaticPrinter:
    case (icUInt32Number)icSigDyeSublimationPrinter:
    case (icUInt32Number)icSigPhotographicPaperPrinter:
    case (icUInt32Number)icSigFilmWriter:
    case (icUInt32Number)icSigVideoMonitor:
    case (icUInt32Number)icSigVideoCamera:
    case (icUInt32Number)icSigProjectionTelevision:
    case (icUInt32Number)icSigCRTDisplay:
    case (icUInt32Number)icSigPMDisplay:
    case (icUInt32Number)icSigAMDisplay:
    // ICC.1:2022 Table 29's two display rows, absent from the enum until now (#2101).
    case (icUInt32Number)icSigLCDDisplay:
    case (icUInt32Number)icSigOLEDDisplay:
    case (icUInt32Number)icSigPhotoCD:
    case (icUInt32Number)icSigPhotoImageSetter:
    case (icUInt32Number)icSigGravure:
    case (icUInt32Number)icSigOffsetLithography:
    case (icUInt32Number)icSigSilkscreen:
    case (icUInt32Number)icSigFlexography:

    // Same four ICC.1 v4.3 rows missing from CIccInfo::GetTechnologySigName().  This
    // header ships as a public API, so its answer disagreeing with
    // CIccTagSignature::Validate is visible to consumers even though nothing in the
    // tree calls it today (#2101).
    case (icUInt32Number)icSigMotionPictureFilmScanner:
    case (icUInt32Number)icSigMotionPictureFilmRecorder:
    case (icUInt32Number)icSigDigitalMotionPictureCamera:
    case (icUInt32Number)icSigDigitalCinemaProjector:
      return true;

    default:
#ifdef ICC_SIGNATURE_VERBOSE
      ICC_LOG_WARNING("Invalid Technology signature: 0x%08x", sig);
#endif
      return false;
  }
}

///////////////////////////////////////////////////////////////////////////////
// STRUCT: IccColorSpaceDescription
//
// PURPOSE:
//   Encapsulates metadata for an ICC color space signature.
//
///////////////////////////////////////////////////////////////////////////////
struct IccColorSpaceDescription {
  const char* name;
  bool isKnown;
  char bytes[5]; // 4-byte sig + null
};

///////////////////////////////////////////////////////////////////////////////
// FUNCTION: DescribeColorSpaceSignature
//
// PURPOSE:
//   Converts a signature into metadata: name, known/unknown, raw byte layout.
//   Does NOT log — use DebugColorSpaceMeta() for diagnostic output.
//
///////////////////////////////////////////////////////////////////////////////
inline IccColorSpaceDescription DescribeColorSpaceSignature(icUInt32Number sig)
{
  IccColorSpaceDescription desc;
  desc.name = ColorSpaceSignatureToStr(sig);

  // Validate without triggering logs — inline the check directly
  bool known = false;
  switch (sig) {
    case (icUInt32Number)icSigXYZData:
    case (icUInt32Number)icSigLabData:
    case (icUInt32Number)icSigRgbData:
    case (icUInt32Number)icSigCmykData:
    case (icUInt32Number)icSigGrayData:
    case (icUInt32Number)icSigNamedData:
    case (icUInt32Number)icSigMCH1Data:
    case (icUInt32Number)icSigMCH2Data:
    case (icUInt32Number)icSigMCH3Data:
    case (icUInt32Number)icSigMCH4Data:
    case (icUInt32Number)icSigMCH5Data:
    case (icUInt32Number)icSigMCH6Data:
    case (icUInt32Number)icSigMCH7Data:
    case (icUInt32Number)icSigMCH8Data:
    case (icUInt32Number)icSigMCH9Data:
    case (icUInt32Number)icSigMCHAData:
    case (icUInt32Number)icSigMCHBData:
    case (icUInt32Number)icSigMCHCData:
    case (icUInt32Number)icSigMCHDData:
    case (icUInt32Number)icSigMCHEData:
    case (icUInt32Number)icSigMCHFData:
      known = true;
      break;
    default: {
      icUInt32Number csType = sig & 0xffff0000;
      icUInt32Number nChan  = sig & 0x0000ffff;
      if ((csType == 0x6e630000 || csType == 0x6d630000) && nChan > 0)
        known = true;
      break;
    }
  }
  desc.isKnown = known;

  desc.bytes[0] = static_cast<char>(static_cast<unsigned char>((sig >> 24) & 0xFF));
  desc.bytes[1] = static_cast<char>(static_cast<unsigned char>((sig >> 16) & 0xFF));
  desc.bytes[2] = static_cast<char>(static_cast<unsigned char>((sig >> 8) & 0xFF));
  desc.bytes[3] = static_cast<char>(static_cast<unsigned char>(sig & 0xFF));
  desc.bytes[4] = '\0';
  return desc;
}

///////////////////////////////////////////////////////////////////////////////
// FUNCTION: DebugColorSpaceMeta
//
// PURPOSE:
//   Emits signature diagnostics via ICC_LOG_INFO.
//
///////////////////////////////////////////////////////////////////////////////
inline void DebugColorSpaceMeta(icUInt32Number sig) {
  IccColorSpaceDescription desc = DescribeColorSpaceSignature(sig);
  ICC_LOG_INFO("Signature 0x%08x [%s]: known=%d, bytes='%s'",
               sig, desc.name, desc.isKnown, desc.bytes);
}



///////////////////////////////////////////////////////////////////////////////
// FUNCTION: ICC_TRACE_NAN_ENABLED
//
// PURPOSE:
//   Emits signature diagnostics via ICC_TRACE_NAN.
//
///////////////////////////////////////////////////////////////////////////////

#ifdef ICC_TRACE_NAN_ENABLED
#include <cmath>
#define ICC_TRACE_NAN(val, label) \
  do { \
    if (std::isnan(val)) { \
      union { float f; uint32_t u; } raw; \
      raw.f = static_cast<float>(val); \
      ICC_LOG_WARNING("NaN detected in %s(): input=NaN [bits=0x%08x]", label, raw.u); \
      ICC_LOG_WARNING("NaN Trace: label=%s, file=%s, line=%d", label, __FILE__, __LINE__); \
      TRACE_CALLER(); \
      __builtin_trap(); \
    } \
  } while(0)
#else
#define ICC_TRACE_NAN(val, label) ((void)0)
#endif

#define ICC_LOG_FLOAT_BITS(val, label) \
  do { \
    union { float f; uint32_t u; } raw; \
    raw.f = static_cast<float>(val); \
    ICC_LOG_DEBUG("%s: float=%.8f bits=0x%08x", label, raw.f, raw.u); \
  } while(0)

#define ICC_SANITY_CHECK_SIGNATURE(sig, label) \
  do { \
    if (((sig) & 0xFF000000) == 0x3F000000) { \
      ICC_LOG_WARNING("%s: suspicious signature 0x%08x", \
                      label, (uint32_t)(sig)); \
      assert(false && "Suspicious or uninitialized signature"); \
    } \
  } while(0)



#endif // _ICC_SIGNATURE_UTILS_H
