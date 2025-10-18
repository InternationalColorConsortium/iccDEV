/** @file
File:       IccSignatureUtils.h

Contains:   Implementation of definitions and macros for ICC signature logging.

Version:    V1

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
//
//
////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef ICC_SIGNATURE_UTILS_H
#define ICC_SIGNATURE_UTILS_H

#include "IccDefs.h"
#include <cstdint>
#include <cstdio>
#include <cmath>


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
// TRACE_CALLER()       – emits anonymous stack trace (max 10 frames)
// TRACE_LOG(msg, ...)  – logs message and trace together
// -----------------------------------------------------------------------------

#ifdef __linux__
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
// INTERNAL RUNTIME CHECK – Soft Contract Enforcement
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
// SAFE VALUE LOGGER – Guarded Memory Access Diagnostic
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
//   The switch-case mapping is derived from the ICC specification for color
//   space signatures. Each case casts the constant to match the icUInt32Number.
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
    default:                              return "Unknown";
  }
}


///////////////////////////////////////////////////////////////////////////////
// FUNCTION: IsValidColorSpaceSignature
//
// PURPOSE:
//   Validates whether the provided ICC color space signature is recognized
//   per the ICC specification. Logs diagnostics for both valid and invalid
//   inputs, including human-readable names.
//
// PARAMETERS:
//   sig - A 32-bit ICC color space signature (icUInt32Number).
//
// RETURNS:
//   true  - If the signature matches a known ICC color space.
//   false - Otherwise; logs a warning with decoded signature value.
//
// DIAGNOSTIC:
//   Includes debug logging of the raw signature and its decoded name.
//   Invalid signatures trigger warnings with source/line context.
//
// EXAMPLE:
//   if (!IsValidColorSpaceSignature(tag->m_ColorSpace)) { ... }
//
// HISTORY:
//   Refactored and instrumented by David Hoyt on 01-MAR-2025.
///////////////////////////////////////////////////////////////////////////////
inline bool IsValidColorSpaceSignature(icUInt32Number sig)
{
  // Log incoming value and its decoded name for debug traceability
  ICC_LOG_DEBUG("IsValidColorSpaceSignature(): input = 0x%08x (%s)", sig, ColorSpaceSignatureToStr(sig));

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
      return true;

    default:
      // Emit structured warning with signature details
      ICC_LOG_WARNING("IccSignatureUtils.h:️ ColorSpace signature: 0x%08x (%s)",
                      sig, ColorSpaceSignatureToStr(sig));
      return false;
  }
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
//   false - If the signature is unrecognized; logs a warning with its hex code.
//
// DIAGNOSTIC:
//   Includes debug logging of the incoming signature value.
//   Adds structured warning output for unrecognized cases.
//
// NOTE:
//   This should be used to verify the `technology` field in `profileDescription`
//   tags or `outputDevice` blocks.
//
// HISTORY:
//   Instrumented and standardized by David Hoyt on 01-MAR-2025.
///////////////////////////////////////////////////////////////////////////////
inline bool IsValidTechnologySignature(icUInt32Number sig)
{
  ICC_LOG_DEBUG("IsValidTechnologySignature(): input = 0x%08x", sig);

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
    case (icUInt32Number)icSigPhotoCD:
    case (icUInt32Number)icSigPhotoImageSetter:
    case (icUInt32Number)icSigGravure:
    case (icUInt32Number)icSigOffsetLithography:
    case (icUInt32Number)icSigSilkscreen:
    case (icUInt32Number)icSigFlexography:
      return true;

    default:
      ICC_LOG_WARNING("IccSignatureUtils.h:️ Invalid Technology signature: 0x%08x", sig);
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
//
///////////////////////////////////////////////////////////////////////////////
inline IccColorSpaceDescription DescribeColorSpaceSignature(icUInt32Number sig)
{
  IccColorSpaceDescription desc;
  desc.name = ColorSpaceSignatureToStr(sig);
  desc.isKnown = IsValidColorSpaceSignature(sig);
  desc.bytes[0] = (sig >> 24) & 0xFF;
  desc.bytes[1] = (sig >> 16) & 0xFF;
  desc.bytes[2] = (sig >> 8) & 0xFF;
  desc.bytes[3] = sig & 0xFF;
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



#endif // ICC_SIGNATURE_UTILS_H
