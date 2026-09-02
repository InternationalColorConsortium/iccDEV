/** @file
    File:       IccProfLibConf.h

    Contains:   Platform Specific Configuration

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

/* Header file guard bands */
#ifndef _ICCCONFIG_h
#define _ICCCONFIG_h

//Define the following to use namespace
//#define USEICCDEVNAMESPACE

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//PC, visual C++
#if defined(_MSC_VER) && !defined(__MWERKS__) && (defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64) || defined(_M_ARM64EC))

  #include <stdint.h>

  #define ICCUINT64 uint64_t
  #define ICCINT64  int64_t
  #define ICUINT64TYPE uint64_t
  #define ICINT64TYPE int64_t

  //Make sure that 32 bit values are set correctly
  #define ICCUINT32 uint32_t
  #define ICCINT32  int32_t
  #define ICUINT32TYPE uint32_t
  #define ICINT32TYPE  int32_t

  #define ICHALFFLOATTYPE uint16_t

  #define USE_WINDOWS_MB_SUPPORT
  #define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
  //#include <windows.h> //For Multibyte Translation Support

  #define ICC_BYTE_ORDER_LITTLE_ENDIAN

  #if defined(ICCPROFLIBDLL_EXPORTS)
    #define ICCPROFLIB_API __declspec(dllexport)
    #define ICCPROFLIB_EXTERN 
  #elif defined(ICCPROFLIBDLL_IMPORTS)
    #define ICCPROFLIB_API __declspec(dllimport)
    #define ICCPROFLIB_EXTERN extern
  #else //just a regular lib
    #define ICCPROFLIB_API
    #define ICCPROFLIB_EXTERN extern
  #endif

  // Exported global *data* needs its own annotation (#2154, from #1888).
  //
  // A shared MSVC build exports through CMake's WINDOWS_EXPORT_ALL_SYMBOLS
  // rather than __declspec, because ICCPROFLIB_API annotation is incomplete
  // (~308 partial uses across 43 headers) -- see Build/Cmake/IccProfLib/
  // CMakeLists.txt and the #764 decision recorded there. That auto-exports
  // functions but NOT data, so a consumer referencing one of the eight
  // exported globals emitted a direct data reference with no __imp_
  // indirection and failed to link (LNK2019/LNK2001) while every function in
  // the same header thunked normally.
  //
  // ICCPROFLIB_DATA_API is deliberately separate from ICCPROFLIB_API so the
  // data symbols can carry real dllexport/dllimport without flipping those 308
  // annotations, which would change how every class and function is exported.
  #if defined(ICCPROFLIBDLL_EXPORTS) || defined(ICCPROFLIBDLL_DATA_EXPORTS)
    #define ICCPROFLIB_DATA_API __declspec(dllexport)
  #elif defined(ICCPROFLIBDLL_IMPORTS) || defined(ICCPROFLIBDLL_DATA_IMPORTS)
    #define ICCPROFLIB_DATA_API __declspec(dllimport)
  #else //static lib, or a consumer of one
    #define ICCPROFLIB_DATA_API
  #endif

  //Since msvc doesn't support cbrtf use pow instead
  #define ICC_CBRTF(v) pow((double)(v), 1.0/3.0)

  #if (_MSC_VER < 1300)
    #define ICC_UNSUPPORTED_TAG_DICT
  #endif

#else // non-PC, perhaps Mac, Linux, or Solaris

  // Using int64_t and uint64_t here will cause more compiler warnings.
  // because long and longlong have the same size, but different compile signatures
  #define ICCUINT64 unsigned long long
  #define ICCINT64  long long
  #define ICUINT64TYPE unsigned long long
  #define ICINT64TYPE long long

  #include <stdint.h>

  //Make sure that 32 bit values are set correctly
  #define ICCUINT32 uint32_t
  #define ICCINT32  int32_t
  #define ICUINT32TYPE uint32_t
  #define ICINT32TYPE  int32_t

  #define ICHALFFLOATTYPE uint16_t

  #if defined(__APPLE__)
    #if  defined(__LITTLE_ENDIAN__)
      #define ICC_BYTE_ORDER_LITTLE_ENDIAN
    #else
      #define ICC_BYTE_ORDER_BIG_ENDIAN
    #endif

  #else // Sun Solaris or Linux
    #if defined(__sun__)
      #define ICC_BYTE_ORDER_BIG_ENDIAN
    #else
      #define ICC_BYTE_ORDER_LITTLE_ENDIAN
    #endif
  #endif

  #if defined(ICCPROFLIBDLL_EXPORTS)
    #define ICCPROFLIB_API __attribute__((visibility("default")))
    #define ICCPROFLIB_EXTERN
  #else
    #define ICCPROFLIB_API
    #define ICCPROFLIB_EXTERN extern
  #endif

  // See the MSVC arm above. There is no import-library model here, so data and
  // functions link the same way and this only needs to track ICCPROFLIB_API.
  #if defined(ICCPROFLIBDLL_EXPORTS) || defined(ICCPROFLIBDLL_DATA_EXPORTS)
    #define ICCPROFLIB_DATA_API __attribute__((visibility("default")))
  #else
    #define ICCPROFLIB_DATA_API
  #endif
  #define stricmp strcasecmp
  #define strnicmp strncasecmp

  //Define ICC_CBRTF as a call to cbrtf (replace with pow if system doesn't support cbrtf)
  #define ICC_CBRTF(v) cbrtf(v)

#endif

// Add comment below if you do not want LAB to XYZ conversions to clip negative XYZ values
// (Warning! Commenting this may result in incorrect round ripping for some Lab Values)
#define REFICCMAX_NOCLIPLABTOXYZ

#ifdef REFICCMAXCMM_EXPORTS
#define MAKE_A_DLL
#endif

#if defined(MAKE_A_DLL)
  #if defined(_MSC_VER)
    #define REFICCMAXEXPORT __declspec(dllexport)
  #elif defined(__GNUC__) || defined(__clang__)
    #define REFICCMAXEXPORT __attribute__((visibility("default")))
  #else
    #define REFICCMAXEXPORT
  #endif
#else
  #if defined(_MSC_VER)
    #define REFICCMAXEXPORT __declspec(dllimport)
  #else
    #define REFICCMAXEXPORT
  #endif
#endif

// Uncomment below if you wish to utilize ZLIB for compressed text tag types
//#define ICC_USE_ZLIB

// Uncomment below if you wish to utilize Eigen library to support matrix solving
//#define ICC_USE_EIGEN_SOLVER

// Auto-detect SSE2 on x86-64 (always available) or x86-32 when compiler signals it.
// Define ICC_DISABLE_SSE2 before including this header to opt out.
#if !defined(ICC_USE_SSE2) && !defined(ICC_DISABLE_SSE2)
  #if defined(_M_X64) || defined(__x86_64__) || defined(__SSE2__)
    #define ICC_USE_SSE2
  #endif
#endif

#ifdef USEICCDEVNAMESPACE
}
#endif

#endif // _ICCCONFIG_h
